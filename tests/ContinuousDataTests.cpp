#include <gtest/gtest.h>
#include "../src/cbsdk/ContinuousData.h"
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

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
        const bool success = group->allocate(buffer_size, num_channels, chan_ids);
        ASSERT_TRUE(success);

        delete[] chan_ids;
    }
};

// Test 1: GroupContinuousData constructor initializes all fields to safe defaults
TEST_F(ContinuousDataTest, GroupContinuousDataConstructorInitializesCorrectly) {
    const GroupContinuousData test_group;

    EXPECT_EQ(test_group.getSize(), 0u);
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
    ASSERT_TRUE(test_group.allocate(100, 3, chan_ids));

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
        ASSERT_TRUE(grp.allocate(100, 2, chan_ids));

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
        ASSERT_TRUE(grp.allocate(100, 2, chan_ids));
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
    // Allocate group 5 with 2 channels
    GroupContinuousData& cont_grp5 = cd->groups[5];
    const uint16_t chan_ids5[2] = {1, 2};
    ASSERT_TRUE(cont_grp5.allocate(100, 2, chan_ids5));

    // Allocate group 1 with 3 channels
    GroupContinuousData& cont_grp1 = cd->groups[1];
    const uint16_t chan_ids1[3] = {3, 4, 5};
    ASSERT_TRUE(cont_grp1.allocate(100, 3, chan_ids1));

    // Write data to group 5
    int16_t data5[2] = {100, 0};
    (void)cont_grp5.writeSample(1000, data5, 2);

    // Write data to group 1
    const int16_t data1[3] = {200, 0, 0};
    (void)cont_grp1.writeSample(2000, data1, 3);

    // Verify groups remain independent
    EXPECT_EQ(cont_grp5.getNumChannels(), 2u);
    EXPECT_EQ(cont_grp1.getNumChannels(), 3u);
    EXPECT_EQ(cont_grp5.getTimestamps()[0], 1000u);
    EXPECT_EQ(cont_grp1.getTimestamps()[0], 2000u);
    // Access flat array: [sample 0][channel 0] = index 0 * num_channels + 0 = 0
    EXPECT_EQ(cont_grp5.getChannelData()[0], 100);
    EXPECT_EQ(cont_grp1.getChannelData()[0], 200);
}

// Test 9: Same channel can exist in multiple groups
TEST_F(ContinuousDataTest, SameChannelInMultipleGroups) {
    // Channel 1 in group 5
    GroupContinuousData& grp5 = cd->groups[5];
    const uint16_t chan_ids5[1] = {1};
    ASSERT_TRUE(grp5.allocate(100, 1, chan_ids5));

    // Same channel 1 in group 6
    GroupContinuousData& grp6 = cd->groups[6];
    const uint16_t chan_ids6[1] = {1};
    ASSERT_TRUE(grp6.allocate(100, 1, chan_ids6));

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
    ASSERT_TRUE(grp.allocate(100, 4, chan_ids));

    // Should be able to find all of them
    EXPECT_EQ(cd->findChannelInGroup(5, 1), 0);
    EXPECT_EQ(cd->findChannelInGroup(5, 5), 1);
    EXPECT_EQ(cd->findChannelInGroup(5, 100), 2);
    EXPECT_EQ(cd->findChannelInGroup(5, 250), 3);

    // Should not find channels in between
    EXPECT_EQ(cd->findChannelInGroup(5, 2), -1);
    EXPECT_EQ(cd->findChannelInGroup(5, 10), -1);
}

// Test 16: Parallel writes to same group from multiple threads
TEST_F(ContinuousDataTest, ParallelWritesToSameGroup) {
    constexpr int NUM_THREADS = 4;
    constexpr int WRITES_PER_THREAD = 100;
    constexpr int NUM_CHANNELS = 2;

    cd->default_size = 1000;

    std::vector<std::thread> threads;
    std::atomic<int> successful_writes(0);

    // Channel IDs for this test
    const uint16_t chan_ids[NUM_CHANNELS] = {1, 2};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, &successful_writes, &chan_ids, WRITES_PER_THREAD]() {
            // Hardcode array size for MSVC compatibility (NUM_CHANNELS is 2)
            for (int i = 0; i < WRITES_PER_THREAD; ++i) {
                const PROCTIME timestamp = t * 1000 + i;
                const int16_t data[2] = {
                    static_cast<int16_t>(t * 100 + i),
                    static_cast<int16_t>(t * 200 + i)
                };

                // Write to group 0 (group index 0)
                const bool overflow = cd->writeSampleThreadSafe(0, timestamp, data, 2, chan_ids);
                if (!overflow)
                    successful_writes++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify the group was allocated
    EXPECT_TRUE(cd->groups[0].isAllocated());
    EXPECT_EQ(cd->groups[0].getNumChannels(), NUM_CHANNELS);

    // Total writes should equal NUM_THREADS * WRITES_PER_THREAD
    // Some may have caused overflow, but that's expected behavior
    EXPECT_GT(successful_writes.load(), 0);
}

// Test 17: Parallel writes to different groups
TEST_F(ContinuousDataTest, ParallelWritesToDifferentGroups) {
    constexpr int NUM_THREADS = 4;
    constexpr int WRITES_PER_THREAD = 50;

    cd->default_size = 500;

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, WRITES_PER_THREAD]() {
            const uint32_t group_idx = t % cbMAXGROUPS;  // Use different groups
            const uint16_t chan_ids[2] = {
                static_cast<uint16_t>(t * 10 + 1),
                static_cast<uint16_t>(t * 10 + 2)
            };

            for (int i = 0; i < WRITES_PER_THREAD; ++i) {
                const PROCTIME timestamp = t * 1000 + i;
                const int16_t data[2] = {
                    static_cast<int16_t>(t * 100 + i),
                    static_cast<int16_t>(t * 200 + i)
                };

                (void)cd->writeSampleThreadSafe(group_idx, timestamp, data, 2, chan_ids);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify each used group was allocated
    for (int t = 0; t < NUM_THREADS; ++t) {
        const uint32_t group_idx = t % cbMAXGROUPS;
        EXPECT_TRUE(cd->groups[group_idx].isAllocated());
    }
}

// Test 18: Parallel reads from same group
TEST_F(ContinuousDataTest, ParallelReadsFromSameGroup) {
    constexpr int NUM_CHANNELS = 2;
    constexpr int NUM_SAMPLES = 100;

    cd->default_size = 1000;

    // Pre-populate group 0 with data
    const uint16_t chan_ids[NUM_CHANNELS] = {1, 2};
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        const int16_t data[NUM_CHANNELS] = {
            static_cast<int16_t>(i),
            static_cast<int16_t>(i * 2)
        };
        (void)cd->writeSampleThreadSafe(0, i, data, NUM_CHANNELS, chan_ids);
    }

    // Take a snapshot for reading
    GroupSnapshot snapshot;
    ASSERT_TRUE(cd->snapshotForReading(0, snapshot));
    EXPECT_EQ(snapshot.num_samples, NUM_SAMPLES);

    // Parallel reads (without seeking - just peeking)
    constexpr int NUM_THREADS = 4;
    std::vector<std::thread> threads;
    std::atomic<int> successful_reads(0);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, &successful_reads]() {
            // Use hardcoded size for MSVC compatibility (NUM_CHANNELS is 2)
            int16_t samples[10 * 2];
            PROCTIME timestamps[10];
            uint32_t num_samples = 10;

            // Read without seeking (bSeek = false)
            if (cd->readSamples(0, samples, timestamps, num_samples, false)) {
                successful_reads++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All reads should succeed
    EXPECT_EQ(successful_reads.load(), NUM_THREADS);
}

// Test 19: Concurrent reads and writes
TEST_F(ContinuousDataTest, ConcurrentReadsAndWrites) {
    constexpr int NUM_CHANNELS = 2;
    constexpr int NUM_WRITER_THREADS = 2;
    constexpr int NUM_READER_THREADS = 2;
    constexpr int OPERATIONS_PER_THREAD = 50;

    cd->default_size = 1000;

    std::vector<std::thread> threads;
    std::atomic<bool> stop_flag(false);

    // Writer threads
    for (int t = 0; t < NUM_WRITER_THREADS; ++t) {
        threads.emplace_back([this, t, &stop_flag, OPERATIONS_PER_THREAD]() {
            // Hardcode array sizes for MSVC compatibility (NUM_CHANNELS is 2)
            const uint16_t chan_ids[2] = {1, 2};
            for (int i = 0; i < OPERATIONS_PER_THREAD && !stop_flag; ++i) {
                const PROCTIME timestamp = t * 1000 + i;
                const int16_t data[2] = {
                    static_cast<int16_t>(t * 100 + i),
                    static_cast<int16_t>(t * 200 + i)
                };

                (void)cd->writeSampleThreadSafe(0, timestamp, data, 2, chan_ids);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // Reader threads
    for (int t = 0; t < NUM_READER_THREADS; ++t) {
        threads.emplace_back([this, &stop_flag, OPERATIONS_PER_THREAD]() {
            // Use hardcoded size for MSVC compatibility (NUM_CHANNELS is 2)
            for (int i = 0; i < OPERATIONS_PER_THREAD && !stop_flag; ++i) {
                int16_t samples[10 * 2];
                PROCTIME timestamps[10];
                uint32_t num_samples = 10;

                // Try to read (may get 0 samples if buffer is empty, which is ok)
                (void)cd->readSamples(0, samples, timestamps, num_samples, false);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Should complete without deadlock or crash
    EXPECT_TRUE(cd->groups[0].isAllocated());
}

// Test 20: Snapshot consistency under concurrent writes
TEST_F(ContinuousDataTest, SnapshotConsistencyUnderWrites) {
    constexpr int NUM_CHANNELS = 2;
    constexpr int NUM_WRITER_THREADS = 2;
    constexpr int NUM_SNAPSHOT_THREADS = 2;
    constexpr int OPERATIONS_PER_THREAD = 30;

    cd->default_size = 500;

    std::vector<std::thread> threads;
    std::atomic<int> successful_snapshots(0);

    // Writer threads
    for (int t = 0; t < NUM_WRITER_THREADS; ++t) {
        threads.emplace_back([this, t, OPERATIONS_PER_THREAD]() {
            // Hardcode array sizes for MSVC compatibility (NUM_CHANNELS is 2)
            const uint16_t chan_ids[2] = {1, 2};
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                const PROCTIME timestamp = t * 1000 + i;
                const int16_t data[2] = {
                    static_cast<int16_t>(t * 100 + i),
                    static_cast<int16_t>(t * 200 + i)
                };

                (void)cd->writeSampleThreadSafe(0, timestamp, data, 2, chan_ids);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // Snapshot threads
    for (int t = 0; t < NUM_SNAPSHOT_THREADS; ++t) {
        threads.emplace_back([this, &successful_snapshots, OPERATIONS_PER_THREAD]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                GroupSnapshot snapshot;
                if (cd->snapshotForReading(0, snapshot)) {
                    // Snapshot should be internally consistent
                    if (snapshot.is_allocated) {
                        EXPECT_LE(snapshot.num_samples, snapshot.buffer_size);
                    }
                    successful_snapshots++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All snapshots should succeed
    EXPECT_EQ(successful_snapshots.load(), NUM_SNAPSHOT_THREADS * OPERATIONS_PER_THREAD);
}

// Test 21: Read with seek vs without seek
TEST_F(ContinuousDataTest, ReadWithSeekVsWithoutSeek) {
    constexpr int NUM_CHANNELS = 2;
    constexpr int NUM_SAMPLES = 20;

    cd->default_size = 100;

    // Write some data
    const uint16_t chan_ids[NUM_CHANNELS] = {1, 2};
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        const int16_t data[NUM_CHANNELS] = {
            static_cast<int16_t>(i),
            static_cast<int16_t>(i * 2)
        };
        (void)cd->writeSampleThreadSafe(0, i, data, NUM_CHANNELS, chan_ids);
    }

    // Read without seeking (peek)
    int16_t samples1[10 * NUM_CHANNELS];
    PROCTIME timestamps1[10];
    uint32_t num_samples1 = 10;
    ASSERT_TRUE(cd->readSamples(0, samples1, timestamps1, num_samples1, false));
    EXPECT_EQ(num_samples1, 10u);

    // Read again without seeking - should get same data
    int16_t samples2[10 * NUM_CHANNELS];
    PROCTIME timestamps2[10];
    uint32_t num_samples2 = 10;
    ASSERT_TRUE(cd->readSamples(0, samples2, timestamps2, num_samples2, false));
    EXPECT_EQ(num_samples2, 10u);

    // Verify we got the same data
    EXPECT_EQ(memcmp(samples1, samples2, 10 * NUM_CHANNELS * sizeof(int16_t)), 0);
    EXPECT_EQ(memcmp(timestamps1, timestamps2, 10 * sizeof(PROCTIME)), 0);

    // Now read with seeking - should advance pointer
    int16_t samples3[10 * NUM_CHANNELS];
    PROCTIME timestamps3[10];
    uint32_t num_samples3 = 10;
    ASSERT_TRUE(cd->readSamples(0, samples3, timestamps3, num_samples3, true));
    EXPECT_EQ(num_samples3, 10u);

    // Next read should get different data (next 10 samples)
    int16_t samples4[10 * NUM_CHANNELS];
    PROCTIME timestamps4[10];
    uint32_t num_samples4 = 10;
    ASSERT_TRUE(cd->readSamples(0, samples4, timestamps4, num_samples4, false));
    EXPECT_EQ(num_samples4, 10u);

    // These should be different from samples3
    EXPECT_NE(timestamps3[0], timestamps4[0]);
}
