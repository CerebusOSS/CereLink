#include <gtest/gtest.h>
#include "../src/cbsdk/ContinuousData.h"
#include <cstring>

// This test file covers the refactored ContinuousData structure that uses
// per-group allocation instead of per-channel allocation.

// Test fixture for ContinuousData tests
class ContinuousDataTest : public ::testing::Test {
protected:
    GroupContinuousData* group;
    ContinuousData* cd;

    void SetUp() override {
        group = new GroupContinuousData();
        cd = new ContinuousData();
        cd->default_size = 1000;  // Use smaller buffer for tests
    }

    void TearDown() override {
        group->cleanup();
        delete group;
        cd->cleanup();
        delete cd;
    }

    // Helper: Allocate a group using the allocate() method
    void AllocateGroup(const uint32_t buffer_size, const uint32_t num_channels) {
        // Create channel IDs array (1-based)
        auto* chan_ids = new uint16_t[num_channels];
        for (uint32_t i = 0; i < num_channels; ++i) {
            chan_ids[i] = static_cast<uint16_t>(i + 1);
        }

        // Use the allocate() method
        const bool success = group->allocate(buffer_size, num_channels, chan_ids, 30000);
        ASSERT_TRUE(success);

        delete[] chan_ids;
    }
};

// Test 1: GroupContinuousData constructor initializes all fields to safe defaults
TEST_F(ContinuousDataTest, GroupContinuousDataConstructorInitializesCorrectly) {
    const GroupContinuousData test_group;

    EXPECT_EQ(test_group.getSize(), 0u);
    EXPECT_EQ(test_group.getSampleRate(), 0u);
    EXPECT_EQ(test_group.getWriteIndex(), 0u);
    EXPECT_EQ(test_group.getWriteStartIndex(), 0u);
    EXPECT_EQ(test_group.getReadEndIndex(), 0u);
    EXPECT_EQ(test_group.getNumChannels(), 0u);
    EXPECT_EQ(test_group.getChannelIds(), nullptr);
    EXPECT_EQ(test_group.getTimestamps(), nullptr);
    EXPECT_EQ(test_group.getChannelData(), nullptr);
}

// Test 2: GroupContinuousData reset() clears data but preserves structure
TEST_F(ContinuousDataTest, GroupResetClearsDataButPreservesStructure) {
    AllocateGroup(100, 4);

    // Write some data using setters and const_cast for array access
    group->setWriteIndex(50);
    group->setWriteStartIndex(10);
    const_cast<PROCTIME*>(group->getTimestamps())[0] = 12345;
    // Access flat array: [sample 0][channel 0] = index 0 * num_channels + 0 = 0
    const_cast<int16_t*>(group->getChannelData())[0] = 999;

    // Reset should clear indices and data
    group->reset();

    EXPECT_EQ(group->getWriteIndex(), 0u);
    EXPECT_EQ(group->getWriteStartIndex(), 0u);
    EXPECT_EQ(group->getReadEndIndex(), 0u);
    EXPECT_EQ(group->getTimestamps()[0], 0u);
    // Access flat array: [sample 0][channel 0] = index 0
    EXPECT_EQ(group->getChannelData()[0], 0);

    // But allocation should remain
    EXPECT_NE(group->getChannelData(), nullptr);
    EXPECT_NE(group->getTimestamps(), nullptr);
    EXPECT_EQ(group->getNumChannels(), 4u);
}

// Test 3: GroupContinuousData cleanup() deallocates all memory
TEST_F(ContinuousDataTest, GroupCleanupDeallocatesMemory) {
    AllocateGroup(100, 4);

    EXPECT_NE(group->getChannelData(), nullptr);
    EXPECT_NE(group->getTimestamps(), nullptr);
    EXPECT_NE(group->getChannelIds(), nullptr);

    group->cleanup();

    EXPECT_EQ(group->getChannelData(), nullptr);
    EXPECT_EQ(group->getTimestamps(), nullptr);
    EXPECT_EQ(group->getChannelIds(), nullptr);
    EXPECT_EQ(group->getNumChannels(), 0u);
}

// Test 4: ContinuousData findChannelInGroup finds correct channel
TEST_F(ContinuousDataTest, FindChannelInGroupFindsCorrectChannel) {
    GroupContinuousData& test_group = cd->groups[5];

    // Set up channel IDs: 1, 10, 25
    uint16_t chan_ids[3] = {1, 10, 25};
    ASSERT_TRUE(test_group.allocate(100, 3, chan_ids, 30000));

    // Test finding existing channels
    EXPECT_EQ(cd->findChannelInGroup(5, 1), 0);
    EXPECT_EQ(cd->findChannelInGroup(5, 10), 1);
    EXPECT_EQ(cd->findChannelInGroup(5, 25), 2);

    // Test not finding non-existent channel
    EXPECT_EQ(cd->findChannelInGroup(5, 99), -1);

    // Test invalid group index
    EXPECT_EQ(cd->findChannelInGroup(999, 1), -1);
}

// Test 5: ContinuousData reset() resets all groups
TEST_F(ContinuousDataTest, ContinuousDataResetResetsAllGroups) {
    // Allocate and populate two groups
    for (uint32_t g = 0; g < 2; ++g) {
        GroupContinuousData& grp = cd->groups[g];
        uint16_t chan_ids[2] = {1, 2};
        ASSERT_TRUE(grp.allocate(100, 2, chan_ids, 30000));

        grp.setWriteIndex(50);
        const_cast<PROCTIME*>(grp.getTimestamps())[0] = 12345;
        // Access flat array: [sample 0][channel 0] = index 0
        const_cast<int16_t*>(grp.getChannelData())[0] = 999;
    }

    cd->reset();

    // Verify both groups were reset
    for (uint32_t g = 0; g < 2; ++g) {
        EXPECT_EQ(cd->groups[g].getWriteIndex(), 0u);
        EXPECT_EQ(cd->groups[g].getTimestamps()[0], 0u);
        // Access flat array: [sample 0][channel 0] = index 0
        EXPECT_EQ(cd->groups[g].getChannelData()[0], 0);
    }
}

// Test 6: ContinuousData cleanup() cleans up all groups
TEST_F(ContinuousDataTest, ContinuousDataCleanupCleansAllGroups) {
    // Allocate two groups
    for (uint32_t g = 0; g < 2; ++g) {
        GroupContinuousData& grp = cd->groups[g];
        uint16_t chan_ids[2] = {1, 2};
        ASSERT_TRUE(grp.allocate(100, 2, chan_ids, 30000));
    }

    cd->cleanup();

    // Verify both groups were cleaned up
    for (uint32_t g = 0; g < 2; ++g) {
        EXPECT_EQ(cd->groups[g].getChannelData(), nullptr);
        EXPECT_EQ(cd->groups[g].getTimestamps(), nullptr);
        EXPECT_EQ(cd->groups[g].getChannelIds(), nullptr);
        EXPECT_EQ(cd->groups[g].getNumChannels(), 0u);
    }
}

// Test 7: Ring buffer wraparound works correctly
TEST_F(ContinuousDataTest, RingBufferWraparoundWorks) {
    AllocateGroup(10, 2);  // Small buffer for easy wraparound testing

    // Fill buffer to capacity using writeSample()
    for (uint32_t i = 0; i < 10; ++i) {
        const int16_t data[2] = {static_cast<int16_t>(100 + i), static_cast<int16_t>(200 + i)};
        (void)group->writeSample(1000 + i, data, 2);  // Ignore overflow - just filling buffer
    }

    EXPECT_EQ(group->getWriteIndex(), 0u);  // Should have wrapped to 0

    // Write one more sample - should overwrite oldest
    const int16_t data[2] = {110, 210};
    const bool overflow = group->writeSample(1010, data, 2);
    EXPECT_TRUE(overflow);  // Buffer was full, so this should overflow

    EXPECT_EQ(group->getWriteIndex(), 1u);
    EXPECT_EQ(group->getTimestamps()[0], 1010u);
    // Access flat array: [sample 0][channel 0] = index 0 * 2 + 0 = 0
    EXPECT_EQ(group->getChannelData()[0], 110);
}

// Test 8: Multiple groups can coexist independently
TEST_F(ContinuousDataTest, MultipleGroupsCoexistIndependently) {
    // Allocate group 5 (30kHz) with 2 channels
    GroupContinuousData& grp5 = cd->groups[5];
    const uint16_t chan_ids5[2] = {1, 2};
    ASSERT_TRUE(grp5.allocate(100, 2, chan_ids5, 30000));

    // Allocate group 1 (500Hz) with 3 channels
    GroupContinuousData& grp1 = cd->groups[1];
    const uint16_t chan_ids1[3] = {3, 4, 5};
    ASSERT_TRUE(grp1.allocate(100, 3, chan_ids1, 500));

    // Write data to group 5
    int16_t data5[2] = {100, 0};
    (void)grp5.writeSample(1000, data5, 2);

    // Write data to group 1
    const int16_t data1[3] = {200, 0, 0};
    (void)grp1.writeSample(2000, data1, 3);

    // Verify groups remain independent
    EXPECT_EQ(grp5.getSampleRate(), 30000u);
    EXPECT_EQ(grp1.getSampleRate(), 500u);
    EXPECT_EQ(grp5.getNumChannels(), 2u);
    EXPECT_EQ(grp1.getNumChannels(), 3u);
    EXPECT_EQ(grp5.getTimestamps()[0], 1000u);
    EXPECT_EQ(grp1.getTimestamps()[0], 2000u);
    // Access flat array: [sample 0][channel 0] = index 0 * num_channels + 0 = 0
    EXPECT_EQ(grp5.getChannelData()[0], 100);
    EXPECT_EQ(grp1.getChannelData()[0], 200);
}

// Test 9: Same channel can exist in multiple groups
TEST_F(ContinuousDataTest, SameChannelInMultipleGroups) {
    // Channel 1 in group 5
    GroupContinuousData& grp5 = cd->groups[5];
    const uint16_t chan_ids5[1] = {1};
    ASSERT_TRUE(grp5.allocate(100, 1, chan_ids5, 30000));

    // Same channel 1 in group 6
    GroupContinuousData& grp6 = cd->groups[6];
    const uint16_t chan_ids6[1] = {1};
    ASSERT_TRUE(grp6.allocate(100, 1, chan_ids6, 60000));

    // Write different data to each
    const int16_t data5[1] = {500};
    (void)grp5.writeSample(1000, data5, 1);
    const int16_t data6[1] = {600};
    (void)grp6.writeSample(2000, data6, 1);

    // Verify they can be found independently
    EXPECT_EQ(cd->findChannelInGroup(5, 1), 0);
    EXPECT_EQ(cd->findChannelInGroup(6, 1), 0);

    // Verify they maintain separate data
    // Access flat array: [sample 0][channel 0] = index 0 * 1 + 0 = 0
    EXPECT_EQ(grp5.getChannelData()[0], 500);
    EXPECT_EQ(grp6.getChannelData()[0], 600);
}

// Test 10: Large channel counts work correctly
TEST_F(ContinuousDataTest, LargeChannelCountsWork) {
    const uint32_t num_channels = 128;  // Realistic large channel count

    AllocateGroup(1000, num_channels);

    EXPECT_EQ(group->getNumChannels(), num_channels);

    // Verify all channels are accessible - write to [sample 0][channel i]
    // Flat array layout: data[sample_idx * num_channels + channel_idx]
    auto channel_data = const_cast<int16_t*>(group->getChannelData());
    for (uint32_t i = 0; i < num_channels; ++i) {
        // For sample 0, channel i: index = 0 * num_channels + i = i
        channel_data[i] = static_cast<int16_t>(i);
    }

    // Verify data was written correctly
    for (uint32_t i = 0; i < num_channels; ++i) {
        // For sample 0, channel i: index = i
        EXPECT_EQ(group->getChannelData()[i], static_cast<int16_t>(i));
    }
}

// Test 11: Empty group (no allocation) is safe to clean up
TEST_F(ContinuousDataTest, EmptyGroupSafeToCleanup) {
    GroupContinuousData empty_group;

    // Should not crash
    EXPECT_NO_THROW(empty_group.cleanup());
    EXPECT_NO_THROW(empty_group.reset());

    // Should remain empty
    EXPECT_EQ(empty_group.getChannelData(), nullptr);
    EXPECT_EQ(empty_group.getTimestamps(), nullptr);
}

// Test 12: Memory allocation works correctly for specified channel count
TEST_F(ContinuousDataTest, MemoryAllocationWorks) {
    const uint32_t num_channels = 10;
    const uint32_t buffer_size = 100;

    // Allocate with 10 channels
    AllocateGroup(buffer_size, num_channels);

    EXPECT_EQ(group->getNumChannels(), num_channels);
    EXPECT_EQ(group->getSize(), buffer_size);

    // Should be able to access all channel slots for sample 0
    // Flat array layout: data[sample_idx * num_channels + channel_idx]
    auto channel_data = const_cast<int16_t*>(group->getChannelData());
    for (uint32_t i = 0; i < num_channels; ++i) {
        // For sample 0, channel i: index = 0 * num_channels + i = i
        channel_data[i] = static_cast<int16_t>(i);
    }

    // Verify data was written correctly
    for (uint32_t i = 0; i < num_channels; ++i) {
        EXPECT_EQ(group->getChannelData()[i], static_cast<int16_t>(i));
    }
}

// Test 13: Read/write indices work independently
TEST_F(ContinuousDataTest, ReadWriteIndicesWorkIndependently) {
    AllocateGroup(100, 2);

    // Simulate writing
    group->setWriteIndex(50);
    group->setWriteStartIndex(0);

    // Simulate reading up to index 30
    group->setReadEndIndex(30);

    // All three should be independent
    EXPECT_EQ(group->getWriteIndex(), 50u);
    EXPECT_EQ(group->getWriteStartIndex(), 0u);
    EXPECT_EQ(group->getReadEndIndex(), 30u);

    // Simulate more writing causing wraparound
    group->setWriteIndex(5);  // Wrapped around
    group->setWriteStartIndex(6);  // Oldest data now at 6

    EXPECT_EQ(group->getWriteIndex(), 5u);
    EXPECT_EQ(group->getWriteStartIndex(), 6u);
    EXPECT_EQ(group->getReadEndIndex(), 30u);  // Should be unchanged
}

// Test 14: Verify typical 30kHz group memory usage is reasonable
TEST_F(ContinuousDataTest, TypicalMemoryUsageIsReasonable) {
    const uint32_t buffer_size = 102400;  // cbSdk_CONTINUOUS_DATA_SAMPLES
    const uint32_t num_channels = 32;     // Typical channel count

    AllocateGroup(buffer_size, num_channels);

    // Calculate memory usage
    const size_t timestamp_mem = buffer_size * sizeof(PROCTIME);
    const size_t channel_data_mem = num_channels * buffer_size * sizeof(int16_t);
    const size_t channel_ids_mem = num_channels * sizeof(uint16_t);
    const size_t total_mem = timestamp_mem + channel_data_mem + channel_ids_mem;

    // For 32 channels with 102400 samples:
    // Timestamps: 102400 * 4 = 409KB (or 8 bytes for 64-bit PROCTIME)
    // Data: 32 * 102400 * 2 = 6.4MB
    // IDs: 32 * 2 = 64 bytes
    // Total: ~6.8MB per group

    // This is much better than old system: 256 * 102400 * 2 = 52MB per group!
    EXPECT_LT(total_mem, 10 * 1024 * 1024);  // Less than 10MB

    // Old system would have used ~52MB for all channels
    size_t old_mem = cbNUM_ANALOG_CHANS * buffer_size * sizeof(int16_t);
    EXPECT_LT(total_mem, old_mem / 5);  // New system uses less than 20% of old
}

// Test 15: Channel IDs can be non-contiguous
TEST_F(ContinuousDataTest, NonContiguousChannelIDsWork) {
    GroupContinuousData& grp = cd->groups[5];

    // Use non-contiguous channel IDs
    const uint16_t chan_ids[4] = {1, 5, 100, 250};
    ASSERT_TRUE(grp.allocate(100, 4, chan_ids, 30000));

    // Should be able to find all of them
    EXPECT_EQ(cd->findChannelInGroup(5, 1), 0);
    EXPECT_EQ(cd->findChannelInGroup(5, 5), 1);
    EXPECT_EQ(cd->findChannelInGroup(5, 100), 2);
    EXPECT_EQ(cd->findChannelInGroup(5, 250), 3);

    // Should not find channels in between
    EXPECT_EQ(cd->findChannelInGroup(5, 2), -1);
    EXPECT_EQ(cd->findChannelInGroup(5, 10), -1);
}
