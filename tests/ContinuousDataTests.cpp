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

    // Helper: Allocate a group manually for testing
    void AllocateGroup(uint32_t buffer_size, uint32_t num_channels) {
        group->size = buffer_size;
        group->num_channels = num_channels;
        group->capacity = num_channels + 4;  // Add headroom like production code
        group->sample_rate = 30000;

        group->timestamps = new PROCTIME[buffer_size];
        group->channel_data = new int16_t*[group->capacity];
        for (uint32_t i = 0; i < group->capacity; ++i) {
            group->channel_data[i] = new int16_t[buffer_size];
        }
        group->channel_ids = new uint16_t[group->capacity];

        // Initialize channel IDs (1-based)
        for (uint32_t i = 0; i < num_channels; ++i) {
            group->channel_ids[i] = static_cast<uint16_t>(i + 1);
        }
    }
};

// Test 1: GroupContinuousData constructor initializes all fields to safe defaults
TEST_F(ContinuousDataTest, GroupContinuousDataConstructorInitializesCorrectly) {
    GroupContinuousData test_group;

    EXPECT_EQ(test_group.size, 0u);
    EXPECT_EQ(test_group.sample_rate, 0u);
    EXPECT_EQ(test_group.write_index, 0u);
    EXPECT_EQ(test_group.write_start_index, 0u);
    EXPECT_EQ(test_group.read_end_index, 0u);
    EXPECT_EQ(test_group.num_channels, 0u);
    EXPECT_EQ(test_group.capacity, 0u);
    EXPECT_EQ(test_group.channel_ids, nullptr);
    EXPECT_EQ(test_group.timestamps, nullptr);
    EXPECT_EQ(test_group.channel_data, nullptr);
}

// Test 2: GroupContinuousData reset() clears data but preserves structure
TEST_F(ContinuousDataTest, GroupResetClearsDataButPreservesStructure) {
    AllocateGroup(100, 4);

    // Write some data
    group->write_index = 50;
    group->write_start_index = 10;
    group->timestamps[0] = 12345;
    group->channel_data[0][0] = 999;

    // Reset should clear indices and data
    group->reset();

    EXPECT_EQ(group->write_index, 0u);
    EXPECT_EQ(group->write_start_index, 0u);
    EXPECT_EQ(group->read_end_index, 0u);
    EXPECT_EQ(group->timestamps[0], 0u);
    EXPECT_EQ(group->channel_data[0][0], 0);

    // But allocation should remain
    EXPECT_NE(group->channel_data, nullptr);
    EXPECT_NE(group->timestamps, nullptr);
    EXPECT_EQ(group->num_channels, 4u);
}

// Test 3: GroupContinuousData cleanup() deallocates all memory
TEST_F(ContinuousDataTest, GroupCleanupDeallocatesMemory) {
    AllocateGroup(100, 4);

    EXPECT_NE(group->channel_data, nullptr);
    EXPECT_NE(group->timestamps, nullptr);
    EXPECT_NE(group->channel_ids, nullptr);

    group->cleanup();

    EXPECT_EQ(group->channel_data, nullptr);
    EXPECT_EQ(group->timestamps, nullptr);
    EXPECT_EQ(group->channel_ids, nullptr);
    EXPECT_EQ(group->num_channels, 0u);
    EXPECT_EQ(group->capacity, 0u);
}

// Test 4: ContinuousData findChannelInGroup finds correct channel
TEST_F(ContinuousDataTest, FindChannelInGroupFindsCorrectChannel) {
    auto& test_group = cd->groups[5];  // Use group 5 (30kHz)
    test_group.size = 100;
    test_group.num_channels = 3;
    test_group.capacity = 7;
    test_group.channel_ids = new uint16_t[7];

    // Set up channel IDs: 1, 10, 25
    test_group.channel_ids[0] = 1;
    test_group.channel_ids[1] = 10;
    test_group.channel_ids[2] = 25;

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
        auto& grp = cd->groups[g];
        grp.size = 100;
        grp.num_channels = 2;
        grp.capacity = 6;
        grp.timestamps = new PROCTIME[100];
        grp.channel_data = new int16_t*[6];
        for (uint32_t i = 0; i < 6; ++i) {
            grp.channel_data[i] = new int16_t[100];
        }

        grp.write_index = 50;
        grp.timestamps[0] = 12345;
        grp.channel_data[0][0] = 999;
    }

    cd->reset();

    // Verify both groups were reset
    for (uint32_t g = 0; g < 2; ++g) {
        EXPECT_EQ(cd->groups[g].write_index, 0u);
        EXPECT_EQ(cd->groups[g].timestamps[0], 0u);
        EXPECT_EQ(cd->groups[g].channel_data[0][0], 0);
    }
}

// Test 6: ContinuousData cleanup() cleans up all groups
TEST_F(ContinuousDataTest, ContinuousDataCleanupCleansAllGroups) {
    // Allocate two groups
    for (uint32_t g = 0; g < 2; ++g) {
        auto& grp = cd->groups[g];
        grp.size = 100;
        grp.num_channels = 2;
        grp.capacity = 6;
        grp.timestamps = new PROCTIME[100];
        grp.channel_data = new int16_t*[6];
        for (uint32_t i = 0; i < 6; ++i) {
            grp.channel_data[i] = new int16_t[100];
        }
        grp.channel_ids = new uint16_t[6];
    }

    cd->cleanup();

    // Verify both groups were cleaned up
    for (uint32_t g = 0; g < 2; ++g) {
        EXPECT_EQ(cd->groups[g].channel_data, nullptr);
        EXPECT_EQ(cd->groups[g].timestamps, nullptr);
        EXPECT_EQ(cd->groups[g].channel_ids, nullptr);
        EXPECT_EQ(cd->groups[g].num_channels, 0u);
    }
}

// Test 7: Ring buffer wraparound works correctly
TEST_F(ContinuousDataTest, RingBufferWraparoundWorks) {
    AllocateGroup(10, 2);  // Small buffer for easy wraparound testing

    // Fill buffer to capacity
    for (uint32_t i = 0; i < 10; ++i) {
        group->timestamps[i] = 1000 + i;
        group->channel_data[0][i] = static_cast<int16_t>(100 + i);
        group->channel_data[1][i] = static_cast<int16_t>(200 + i);
        group->write_index = (group->write_index + 1) % group->size;
    }

    EXPECT_EQ(group->write_index, 0u);  // Should have wrapped to 0

    // Write one more sample - should overwrite oldest
    group->timestamps[group->write_index] = 1010;
    group->channel_data[0][group->write_index] = 110;
    group->channel_data[1][group->write_index] = 210;
    group->write_index = (group->write_index + 1) % group->size;

    EXPECT_EQ(group->write_index, 1u);
    EXPECT_EQ(group->timestamps[0], 1010u);
    EXPECT_EQ(group->channel_data[0][0], 110);
}

// Test 8: Multiple groups can coexist independently
TEST_F(ContinuousDataTest, MultipleGroupsCoexistIndependently) {
    // Allocate group 5 (30kHz) with 2 channels
    auto& grp5 = cd->groups[5];
    grp5.size = 100;
    grp5.sample_rate = 30000;
    grp5.num_channels = 2;
    grp5.capacity = 6;
    grp5.timestamps = new PROCTIME[100];
    grp5.channel_data = new int16_t*[6];
    for (uint32_t i = 0; i < 6; ++i) {
        grp5.channel_data[i] = new int16_t[100];
    }
    grp5.channel_ids = new uint16_t[6];
    grp5.channel_ids[0] = 1;
    grp5.channel_ids[1] = 2;

    // Allocate group 1 (500Hz) with 3 channels
    auto& grp1 = cd->groups[1];
    grp1.size = 100;
    grp1.sample_rate = 500;
    grp1.num_channels = 3;
    grp1.capacity = 7;
    grp1.timestamps = new PROCTIME[100];
    grp1.channel_data = new int16_t*[7];
    for (uint32_t i = 0; i < 7; ++i) {
        grp1.channel_data[i] = new int16_t[100];
    }
    grp1.channel_ids = new uint16_t[7];
    grp1.channel_ids[0] = 3;
    grp1.channel_ids[1] = 4;
    grp1.channel_ids[2] = 5;

    // Write data to group 5
    grp5.timestamps[0] = 1000;
    grp5.channel_data[0][0] = 100;
    grp5.write_index = 1;

    // Write data to group 1
    grp1.timestamps[0] = 2000;
    grp1.channel_data[0][0] = 200;
    grp1.write_index = 1;

    // Verify groups remain independent
    EXPECT_EQ(grp5.sample_rate, 30000u);
    EXPECT_EQ(grp1.sample_rate, 500u);
    EXPECT_EQ(grp5.num_channels, 2u);
    EXPECT_EQ(grp1.num_channels, 3u);
    EXPECT_EQ(grp5.timestamps[0], 1000u);
    EXPECT_EQ(grp1.timestamps[0], 2000u);
    EXPECT_EQ(grp5.channel_data[0][0], 100);
    EXPECT_EQ(grp1.channel_data[0][0], 200);
}

// Test 9: Same channel can exist in multiple groups
TEST_F(ContinuousDataTest, SameChannelInMultipleGroups) {
    // Channel 1 in group 5
    auto& grp5 = cd->groups[5];
    grp5.size = 100;
    grp5.num_channels = 1;
    grp5.capacity = 5;
    grp5.channel_ids = new uint16_t[5];
    grp5.channel_ids[0] = 1;
    grp5.timestamps = new PROCTIME[100];
    grp5.channel_data = new int16_t*[5];
    for (uint32_t i = 0; i < 5; ++i) {
        grp5.channel_data[i] = new int16_t[100];
    }

    // Same channel 1 in group 6
    auto& grp6 = cd->groups[6];
    grp6.size = 100;
    grp6.num_channels = 1;
    grp6.capacity = 5;
    grp6.channel_ids = new uint16_t[5];
    grp6.channel_ids[0] = 1;  // Same channel ID
    grp6.timestamps = new PROCTIME[100];
    grp6.channel_data = new int16_t*[5];
    for (uint32_t i = 0; i < 5; ++i) {
        grp6.channel_data[i] = new int16_t[100];
    }

    // Write different data to each
    grp5.timestamps[0] = 1000;
    grp5.channel_data[0][0] = 500;
    grp6.timestamps[0] = 2000;
    grp6.channel_data[0][0] = 600;

    // Verify they can be found independently
    EXPECT_EQ(cd->findChannelInGroup(5, 1), 0);
    EXPECT_EQ(cd->findChannelInGroup(6, 1), 0);

    // Verify they maintain separate data
    EXPECT_EQ(grp5.channel_data[0][0], 500);
    EXPECT_EQ(grp6.channel_data[0][0], 600);
}

// Test 10: Large channel counts work correctly
TEST_F(ContinuousDataTest, LargeChannelCountsWork) {
    const uint32_t num_channels = 128;  // Realistic large channel count

    AllocateGroup(1000, num_channels);

    EXPECT_EQ(group->num_channels, num_channels);
    EXPECT_GE(group->capacity, num_channels);

    // Verify all channels are accessible
    for (uint32_t i = 0; i < num_channels; ++i) {
        EXPECT_NE(group->channel_data[i], nullptr);
        group->channel_data[i][0] = static_cast<int16_t>(i);
    }

    // Verify data was written correctly
    for (uint32_t i = 0; i < num_channels; ++i) {
        EXPECT_EQ(group->channel_data[i][0], static_cast<int16_t>(i));
    }
}

// Test 11: Empty group (no allocation) is safe to clean up
TEST_F(ContinuousDataTest, EmptyGroupSafeToCleanup) {
    GroupContinuousData empty_group;

    // Should not crash
    EXPECT_NO_THROW(empty_group.cleanup());
    EXPECT_NO_THROW(empty_group.reset());

    // Should remain empty
    EXPECT_EQ(empty_group.channel_data, nullptr);
    EXPECT_EQ(empty_group.timestamps, nullptr);
}

// Test 12: Memory allocation matches capacity, not just num_channels
TEST_F(ContinuousDataTest, MemoryAllocationUsesCapacity) {
    const uint32_t num_channels = 10;
    const uint32_t capacity = 14;  // More than num_channels

    group->size = 100;
    group->num_channels = num_channels;
    group->capacity = capacity;
    group->timestamps = new PROCTIME[100];
    group->channel_data = new int16_t*[capacity];
    for (uint32_t i = 0; i < capacity; ++i) {
        group->channel_data[i] = new int16_t[100];
    }
    group->channel_ids = new uint16_t[capacity];

    // Should be able to access all capacity slots
    for (uint32_t i = 0; i < capacity; ++i) {
        EXPECT_NE(group->channel_data[i], nullptr);
        group->channel_data[i][0] = static_cast<int16_t>(i);
    }

    // Verify we can expand num_channels up to capacity without reallocation
    group->num_channels = capacity;
    for (uint32_t i = 0; i < capacity; ++i) {
        EXPECT_EQ(group->channel_data[i][0], static_cast<int16_t>(i));
    }
}

// Test 13: Read/write indices work independently
TEST_F(ContinuousDataTest, ReadWriteIndicesWorkIndependently) {
    AllocateGroup(100, 2);

    // Simulate writing
    group->write_index = 50;
    group->write_start_index = 0;

    // Simulate reading up to index 30
    group->read_end_index = 30;

    // All three should be independent
    EXPECT_EQ(group->write_index, 50u);
    EXPECT_EQ(group->write_start_index, 0u);
    EXPECT_EQ(group->read_end_index, 30u);

    // Simulate more writing causing wraparound
    group->write_index = 5;  // Wrapped around
    group->write_start_index = 6;  // Oldest data now at 6

    EXPECT_EQ(group->write_index, 5u);
    EXPECT_EQ(group->write_start_index, 6u);
    EXPECT_EQ(group->read_end_index, 30u);  // Should be unchanged
}

// Test 14: Verify typical 30kHz group memory usage is reasonable
TEST_F(ContinuousDataTest, TypicalMemoryUsageIsReasonable) {
    const uint32_t buffer_size = 102400;  // cbSdk_CONTINUOUS_DATA_SAMPLES
    const uint32_t num_channels = 32;     // Typical channel count

    AllocateGroup(buffer_size, num_channels);

    // Calculate memory usage
    size_t timestamp_mem = buffer_size * sizeof(PROCTIME);
    size_t channel_data_mem = group->capacity * buffer_size * sizeof(int16_t);
    size_t channel_ids_mem = group->capacity * sizeof(uint16_t);
    size_t total_mem = timestamp_mem + channel_data_mem + channel_ids_mem;

    // For 32 channels with 102400 samples:
    // Timestamps: 102400 * 4 = 409KB
    // Data: 36 * 102400 * 2 = 7.2MB (capacity = 32 + 4 = 36)
    // IDs: 36 * 2 = 72 bytes
    // Total: ~7.6MB per group

    // This is much better than old system: 256 * 102400 * 2 = 52MB per group!
    EXPECT_LT(total_mem, 10 * 1024 * 1024);  // Less than 10MB

    // Old system would have used ~52MB for all channels
    size_t old_mem = cbNUM_ANALOG_CHANS * buffer_size * sizeof(int16_t);
    EXPECT_LT(total_mem, old_mem / 5);  // New system uses less than 20% of old
}

// Test 15: Channel IDs can be non-contiguous
TEST_F(ContinuousDataTest, NonContiguousChannelIDsWork) {
    auto& grp = cd->groups[5];
    grp.size = 100;
    grp.num_channels = 4;
    grp.capacity = 8;
    grp.channel_ids = new uint16_t[8];

    // Use non-contiguous channel IDs
    grp.channel_ids[0] = 1;
    grp.channel_ids[1] = 5;
    grp.channel_ids[2] = 100;
    grp.channel_ids[3] = 250;

    // Should be able to find all of them
    EXPECT_EQ(cd->findChannelInGroup(5, 1), 0);
    EXPECT_EQ(cd->findChannelInGroup(5, 5), 1);
    EXPECT_EQ(cd->findChannelInGroup(5, 100), 2);
    EXPECT_EQ(cd->findChannelInGroup(5, 250), 3);

    // Should not find channels in between
    EXPECT_EQ(cd->findChannelInGroup(5, 2), -1);
    EXPECT_EQ(cd->findChannelInGroup(5, 10), -1);
}
