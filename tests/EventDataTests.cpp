#include <gtest/gtest.h>
#include "../src/cbsdk/EventData.h"
#include <thread>
#include <vector>
#include <atomic>

// This test file covers the refactored EventData class that manages
// per-channel ring buffers for spike and digital input event data.

// Test fixture for EventData tests
class EventDataTest : public ::testing::Test {
protected:
    EventData* ed;

    void SetUp() override {
        ed = new EventData();
    }

    void TearDown() override {
        ed->cleanup();
        delete ed;
    }
};

// Test 1: EventData constructor initializes all fields to safe defaults
TEST_F(EventDataTest, ConstructorInitializesCorrectly) {
    EventData test_ed;

    EXPECT_EQ(test_ed.getSize(), 0u);
    EXPECT_FALSE(test_ed.isAllocated());
    EXPECT_EQ(test_ed.getWaveformData(), nullptr);

    // Check that all channel pointers are null
    for (uint16_t ch = 1; ch <= cbMAXCHANS; ++ch) {
        EXPECT_EQ(test_ed.getTimestamps(ch), nullptr);
        EXPECT_EQ(test_ed.getUnits(ch), nullptr);
        EXPECT_EQ(test_ed.getWriteIndex(ch), 0u);
        EXPECT_EQ(test_ed.getWriteStartIndex(ch), 0u);
    }
}

// Test 2: allocate() allocates memory correctly
TEST_F(EventDataTest, AllocateAllocatesMemoryCorrectly) {
    const uint32_t buffer_size = 100;

    ASSERT_TRUE(ed->allocate(buffer_size));

    EXPECT_EQ(ed->getSize(), buffer_size);
    EXPECT_TRUE(ed->isAllocated());

    // Check that all channels have allocated arrays
    for (uint16_t ch = 1; ch <= cbMAXCHANS; ++ch) {
        EXPECT_NE(ed->getTimestamps(ch), nullptr);
        EXPECT_NE(ed->getUnits(ch), nullptr);
        EXPECT_EQ(ed->getWriteIndex(ch), 0u);
        EXPECT_EQ(ed->getWriteStartIndex(ch), 0u);
    }
}

// Test 3: allocate() with same size reuses allocation
TEST_F(EventDataTest, AllocateWithSameSizeReusesAllocation) {
    const uint32_t buffer_size = 100;

    ASSERT_TRUE(ed->allocate(buffer_size));

    // Write some data
    ed->writeEvent(1, 12345, 1);
    EXPECT_EQ(ed->getWriteIndex(1), 1u);

    // Allocate again with same size - should reset but keep allocation
    ASSERT_TRUE(ed->allocate(buffer_size));

    EXPECT_EQ(ed->getSize(), buffer_size);
    EXPECT_EQ(ed->getWriteIndex(1), 0u);  // Reset
    EXPECT_TRUE(ed->isAllocated());
}

// Test 4: allocate() with different size reallocates
TEST_F(EventDataTest, AllocateWithDifferentSizeReallocates) {
    ASSERT_TRUE(ed->allocate(100));
    EXPECT_EQ(ed->getSize(), 100u);

    ASSERT_TRUE(ed->allocate(200));
    EXPECT_EQ(ed->getSize(), 200u);
    EXPECT_TRUE(ed->isAllocated());
}

// Test 5: writeEvent() stores data correctly
TEST_F(EventDataTest, WriteEventStoresDataCorrectly) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 5;
    const PROCTIME timestamp = 123456;
    const uint16_t unit = 2;

    bool overflow = ed->writeEvent(channel, timestamp, unit);

    EXPECT_FALSE(overflow);
    EXPECT_EQ(ed->getWriteIndex(channel), 1u);
    EXPECT_EQ(ed->getTimestamps(channel)[0], timestamp);
    EXPECT_EQ(ed->getUnits(channel)[0], unit);
}

// Test 6: writeEvent() advances write index
TEST_F(EventDataTest, WriteEventAdvancesWriteIndex) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 10;

    for (uint32_t i = 0; i < 5; ++i) {
        bool overflow = ed->writeEvent(channel, 1000 + i, 1);
        EXPECT_FALSE(overflow);
        EXPECT_EQ(ed->getWriteIndex(channel), i + 1);
    }
}

// Test 7: writeEvent() detects buffer overflow
TEST_F(EventDataTest, WriteEventDetectsOverflow) {
    const uint32_t buffer_size = 10;
    ASSERT_TRUE(ed->allocate(buffer_size));

    const uint16_t channel = 1;

    // Fill buffer to capacity - the last write (10th) will detect overflow
    bool overflow = false;
    for (uint32_t i = 0; i < buffer_size; ++i) {
        overflow = ed->writeEvent(channel, i, 0);
        if (i < buffer_size - 1) {
            EXPECT_FALSE(overflow);
        }
    }

    // The 10th write should have detected overflow
    EXPECT_TRUE(overflow);
    EXPECT_EQ(ed->getWriteIndex(channel), 0u);  // Wrapped around
    EXPECT_EQ(ed->getWriteStartIndex(channel), 1u);  // Advanced

    // Next write should also overflow
    overflow = ed->writeEvent(channel, 999, 0);
    EXPECT_TRUE(overflow);

    // Write start index should have advanced again
    EXPECT_EQ(ed->getWriteIndex(channel), 1u);
    EXPECT_EQ(ed->getWriteStartIndex(channel), 2u);
}

// Test 8: writeEvent() wraps around correctly
TEST_F(EventDataTest, WriteEventWrapsAroundCorrectly) {
    const uint32_t buffer_size = 5;
    ASSERT_TRUE(ed->allocate(buffer_size));

    const uint16_t channel = 3;

    // Fill buffer and wrap around (8 writes total)
    for (uint32_t i = 0; i < buffer_size + 3; ++i) {
        ed->writeEvent(channel, 1000 + i, static_cast<uint16_t>(i % 6));
    }

    // After 8 writes into a buffer of 5:
    // - write_index should be at 3 (next write position)
    // - write_start_index should be at 4 (oldest valid data)
    EXPECT_EQ(ed->getWriteIndex(channel), 3u);
    EXPECT_EQ(ed->getWriteStartIndex(channel), 4u);

    // Buffer contains data from writes 3-7 (the last 5 writes)
    // At indices: [5, 6, 7, 3, 4] -> timestamps [1005, 1006, 1007, 1003, 1004]
    EXPECT_EQ(ed->getTimestamps(channel)[0], 1005u);  // From write i=5
    EXPECT_EQ(ed->getTimestamps(channel)[1], 1006u);  // From write i=6
    EXPECT_EQ(ed->getTimestamps(channel)[2], 1007u);  // From write i=7
    EXPECT_EQ(ed->getTimestamps(channel)[3], 1003u);  // From write i=3
    EXPECT_EQ(ed->getTimestamps(channel)[4], 1004u);  // From write i=4
}

// Test 9: writeEvent() with invalid channel returns false
TEST_F(EventDataTest, WriteEventWithInvalidChannelReturnsFalse) {
    ASSERT_TRUE(ed->allocate(100));

    // Channel 0 is invalid
    bool overflow = ed->writeEvent(0, 1000, 1);
    EXPECT_FALSE(overflow);

    // Channel > cbMAXCHANS is invalid
    overflow = ed->writeEvent(cbMAXCHANS + 1, 1000, 1);
    EXPECT_FALSE(overflow);
}

// Test 10: reset() clears data but preserves allocation
TEST_F(EventDataTest, ResetClearsDataButPreservesAllocation) {
    ASSERT_TRUE(ed->allocate(100));

    // Write some data to multiple channels
    ed->writeEvent(1, 1000, 1);
    ed->writeEvent(5, 2000, 2);
    ed->writeEvent(10, 3000, 3);

    EXPECT_NE(ed->getWriteIndex(1), 0u);
    EXPECT_NE(ed->getWriteIndex(5), 0u);
    EXPECT_NE(ed->getWriteIndex(10), 0u);

    ed->reset();

    // Indices should be cleared
    EXPECT_EQ(ed->getWriteIndex(1), 0u);
    EXPECT_EQ(ed->getWriteIndex(5), 0u);
    EXPECT_EQ(ed->getWriteIndex(10), 0u);
    EXPECT_EQ(ed->getWriteStartIndex(1), 0u);
    EXPECT_EQ(ed->getWriteStartIndex(5), 0u);
    EXPECT_EQ(ed->getWriteStartIndex(10), 0u);

    // Data should be zeroed
    EXPECT_EQ(ed->getTimestamps(1)[0], 0u);
    EXPECT_EQ(ed->getTimestamps(5)[0], 0u);
    EXPECT_EQ(ed->getTimestamps(10)[0], 0u);

    // Allocation should remain
    EXPECT_TRUE(ed->isAllocated());
    EXPECT_EQ(ed->getSize(), 100u);
}

// Test 11: cleanup() deallocates all memory
TEST_F(EventDataTest, CleanupDeallocatesMemory) {
    ASSERT_TRUE(ed->allocate(100));

    EXPECT_TRUE(ed->isAllocated());

    ed->cleanup();

    EXPECT_FALSE(ed->isAllocated());
    EXPECT_EQ(ed->getSize(), 0u);

    // Check that all channel pointers are null
    for (uint16_t ch = 1; ch <= cbMAXCHANS; ++ch) {
        EXPECT_EQ(ed->getTimestamps(ch), nullptr);
        EXPECT_EQ(ed->getUnits(ch), nullptr);
    }
}

// Test 12: cleanup() is safe on unallocated EventData
TEST_F(EventDataTest, CleanupSafeOnUnallocated) {
    EventData empty_ed;

    EXPECT_NO_THROW(empty_ed.cleanup());
    EXPECT_NO_THROW(empty_ed.reset());

    EXPECT_FALSE(empty_ed.isAllocated());
}

// Test 13: getTimestamps() returns correct pointer
TEST_F(EventDataTest, GetTimestampsReturnsCorrectPointer) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 7;

    // Write some events
    for (uint32_t i = 0; i < 5; ++i) {
        ed->writeEvent(channel, 1000 + i, 0);
    }

    const PROCTIME* timestamps = ed->getTimestamps(channel);
    ASSERT_NE(timestamps, nullptr);

    for (uint32_t i = 0; i < 5; ++i) {
        EXPECT_EQ(timestamps[i], 1000u + i);
    }
}

// Test 14: getTimestamps() returns nullptr for invalid channel
TEST_F(EventDataTest, GetTimestampsReturnsNullForInvalidChannel) {
    ASSERT_TRUE(ed->allocate(100));

    EXPECT_EQ(ed->getTimestamps(0), nullptr);
    EXPECT_EQ(ed->getTimestamps(cbMAXCHANS + 1), nullptr);
}

// Test 15: getUnits() returns correct pointer
TEST_F(EventDataTest, GetUnitsReturnsCorrectPointer) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 15;

    // Write events with different units
    for (uint16_t i = 0; i < 5; ++i) {
        ed->writeEvent(channel, 1000, i);
    }

    const uint16_t* units = ed->getUnits(channel);
    ASSERT_NE(units, nullptr);

    for (uint16_t i = 0; i < 5; ++i) {
        EXPECT_EQ(units[i], i);
    }
}

// Test 16: getUnits() returns nullptr for invalid channel
TEST_F(EventDataTest, GetUnitsReturnsNullForInvalidChannel) {
    ASSERT_TRUE(ed->allocate(100));

    EXPECT_EQ(ed->getUnits(0), nullptr);
    EXPECT_EQ(ed->getUnits(cbMAXCHANS + 1), nullptr);
}

// Test 17: setWriteIndex() and setWriteStartIndex() work correctly
TEST_F(EventDataTest, SettersWorkCorrectly) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 20;

    ed->setWriteIndex(channel, 50);
    ed->setWriteStartIndex(channel, 10);

    EXPECT_EQ(ed->getWriteIndex(channel), 50u);
    EXPECT_EQ(ed->getWriteStartIndex(channel), 10u);
}

// Test 18: setters ignore invalid channels
TEST_F(EventDataTest, SettersIgnoreInvalidChannels) {
    ASSERT_TRUE(ed->allocate(100));

    // Should not crash
    EXPECT_NO_THROW(ed->setWriteIndex(0, 10));
    EXPECT_NO_THROW(ed->setWriteIndex(cbMAXCHANS + 1, 10));
    EXPECT_NO_THROW(ed->setWriteStartIndex(0, 10));
    EXPECT_NO_THROW(ed->setWriteStartIndex(cbMAXCHANS + 1, 10));
}

// Test 19: Multiple channels maintain independent state
TEST_F(EventDataTest, MultipleChannelsMaintainIndependentState) {
    ASSERT_TRUE(ed->allocate(100));

    // Write to different channels
    ed->writeEvent(1, 1000, 1);
    ed->writeEvent(1, 1001, 1);

    ed->writeEvent(5, 2000, 2);

    ed->writeEvent(10, 3000, 3);
    ed->writeEvent(10, 3001, 3);
    ed->writeEvent(10, 3002, 3);

    // Verify independent indices
    EXPECT_EQ(ed->getWriteIndex(1), 2u);
    EXPECT_EQ(ed->getWriteIndex(5), 1u);
    EXPECT_EQ(ed->getWriteIndex(10), 3u);

    // Verify independent data
    EXPECT_EQ(ed->getTimestamps(1)[0], 1000u);
    EXPECT_EQ(ed->getTimestamps(5)[0], 2000u);
    EXPECT_EQ(ed->getTimestamps(10)[0], 3000u);

    EXPECT_EQ(ed->getUnits(1)[0], 1u);
    EXPECT_EQ(ed->getUnits(5)[0], 2u);
    EXPECT_EQ(ed->getUnits(10)[0], 3u);
}

// Test 20: All cbMAXCHANS channels are accessible
TEST_F(EventDataTest, AllChannelsAccessible) {
    ASSERT_TRUE(ed->allocate(10));

    // Write to all channels
    for (uint16_t ch = 1; ch <= cbMAXCHANS; ++ch) {
        bool overflow = ed->writeEvent(ch, ch * 100, ch % 6);
        EXPECT_FALSE(overflow);
    }

    // Verify all channels
    for (uint16_t ch = 1; ch <= cbMAXCHANS; ++ch) {
        EXPECT_EQ(ed->getWriteIndex(ch), 1u);
        EXPECT_EQ(ed->getTimestamps(ch)[0], ch * 100u);
        EXPECT_EQ(ed->getUnits(ch)[0], ch % 6);
    }
}

// Test 21: Parallel writes to same channel
TEST_F(EventDataTest, ParallelWritesToSameChannel) {
    constexpr int NUM_THREADS = 4;
    constexpr int WRITES_PER_THREAD = 100;
    constexpr uint16_t CHANNEL = 1;

    ASSERT_TRUE(ed->allocate(1000));

    std::vector<std::thread> threads;
    std::atomic<int> successful_writes(0);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, &successful_writes]() {
            for (int i = 0; i < WRITES_PER_THREAD; ++i) {
                std::lock_guard<std::mutex> lock(ed->m_mutex);
                const PROCTIME timestamp = t * 1000 + i;
                const uint16_t unit = t % 6;

                ed->writeEvent(CHANNEL, timestamp, unit);
                successful_writes++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Total writes should equal NUM_THREADS * WRITES_PER_THREAD
    EXPECT_EQ(successful_writes.load(), NUM_THREADS * WRITES_PER_THREAD);
}

// Test 22: Parallel writes to different channels
TEST_F(EventDataTest, ParallelWritesToDifferentChannels) {
    constexpr int NUM_THREADS = 4;
    constexpr int WRITES_PER_THREAD = 50;

    ASSERT_TRUE(ed->allocate(500));

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            const uint16_t channel = t + 1;  // Each thread writes to different channel

            for (int i = 0; i < WRITES_PER_THREAD; ++i) {
                std::lock_guard<std::mutex> lock(ed->m_mutex);
                const PROCTIME timestamp = t * 1000 + i;
                const uint16_t unit = t % 6;

                ed->writeEvent(channel, timestamp, unit);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify each channel has the expected number of writes
    for (int t = 0; t < NUM_THREADS; ++t) {
        const uint16_t channel = t + 1;
        EXPECT_EQ(ed->getWriteIndex(channel), WRITES_PER_THREAD);
    }
}

// Test 23: Concurrent reads and writes with mutex
TEST_F(EventDataTest, ConcurrentReadsAndWritesWithMutex) {
    constexpr int NUM_WRITER_THREADS = 2;
    constexpr int NUM_READER_THREADS = 2;
    constexpr int OPERATIONS_PER_THREAD = 50;
    constexpr uint16_t CHANNEL = 1;

    ASSERT_TRUE(ed->allocate(500));

    std::vector<std::thread> threads;
    std::atomic<int> read_successes(0);

    // Writer threads
    for (int t = 0; t < NUM_WRITER_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                std::lock_guard<std::mutex> lock(ed->m_mutex);
                ed->writeEvent(CHANNEL, t * 1000 + i, t % 6);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // Reader threads
    for (int t = 0; t < NUM_READER_THREADS; ++t) {
        threads.emplace_back([this, &read_successes]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                std::lock_guard<std::mutex> lock(ed->m_mutex);
                const PROCTIME* timestamps = ed->getTimestamps(CHANNEL);
                const uint16_t* units = ed->getUnits(CHANNEL);
                const uint32_t write_idx = ed->getWriteIndex(CHANNEL);

                if (timestamps && units && write_idx > 0) {
                    read_successes++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Should complete without deadlock or crash
    EXPECT_GT(read_successes.load(), 0);
}

// Test 24: Unit classification values
TEST_F(EventDataTest, UnitClassificationValues) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 1;

    // Test different unit values (0-5 for sorted units, 255 for noise)
    const uint16_t units[] = {0, 1, 2, 3, 4, 5, 255};

    for (uint16_t unit : units) {
        ed->writeEvent(channel, 1000, unit);
    }

    const uint16_t* unit_array = ed->getUnits(channel);
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); ++i) {
        EXPECT_EQ(unit_array[i], units[i]);
    }
}

// Test 25: Large buffer allocation
TEST_F(EventDataTest, LargeBufferAllocation) {
    const uint32_t large_size = 100000;  // 100k events per channel

    ASSERT_TRUE(ed->allocate(large_size));
    EXPECT_EQ(ed->getSize(), large_size);

    // Write to a few channels to verify allocation works
    for (uint16_t ch = 1; ch <= 10; ++ch) {
        ed->writeEvent(ch, 1000, 1);
        EXPECT_EQ(ed->getWriteIndex(ch), 1u);
    }
}

// Test 26: Memory usage is reasonable
TEST_F(EventDataTest, MemoryUsageIsReasonable) {
    const uint32_t buffer_size = 10000;  // Typical event buffer size

    ASSERT_TRUE(ed->allocate(buffer_size));

    // Calculate memory usage per channel
    const size_t timestamp_mem = buffer_size * sizeof(PROCTIME);
    const size_t unit_mem = buffer_size * sizeof(uint16_t);
    const size_t per_channel_mem = timestamp_mem + unit_mem;

    // Total for all channels
    const size_t total_mem = per_channel_mem * cbMAXCHANS;

    // For 10000 events per channel with cbMAXCHANS channels:
    // Timestamps: 10000 * 8 bytes * cbMAXCHANS (assuming 64-bit PROCTIME)
    // Units: 10000 * 2 bytes * cbMAXCHANS
    // With cbMAXCHANS=512: ~51MB total

    // Verify it's less than 100MB total (reasonable for event buffering)
    EXPECT_LT(total_mem, 100 * 1024 * 1024);
}

// Test 27: getAvailableSamples() returns correct count
TEST_F(EventDataTest, GetAvailableSamplesReturnsCorrectCount) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 5;

    // Initially no samples
    EXPECT_EQ(ed->getAvailableSamples(channel), 0u);

    // Write 10 events
    for (uint32_t i = 0; i < 10; ++i) {
        ed->writeEvent(channel, 1000 + i, 1);
    }

    EXPECT_EQ(ed->getAvailableSamples(channel), 10u);

    // Write 5 more
    for (uint32_t i = 0; i < 5; ++i) {
        ed->writeEvent(channel, 2000 + i, 2);
    }

    EXPECT_EQ(ed->getAvailableSamples(channel), 15u);
}

// Test 28: getAvailableSamples() handles ring buffer wraparound
TEST_F(EventDataTest, GetAvailableSamplesHandlesWraparound) {
    const uint32_t buffer_size = 10;
    ASSERT_TRUE(ed->allocate(buffer_size));

    const uint16_t channel = 3;

    // Fill buffer (the 10th write will detect overflow since next_write_index wraps to write_start_index)
    for (uint32_t i = 0; i < buffer_size; ++i) {
        ed->writeEvent(channel, i, 0);
    }

    // After buffer_size writes, we have buffer_size-1 samples available
    // (the ring buffer needs one empty slot to distinguish full from empty)
    EXPECT_EQ(ed->getAvailableSamples(channel), buffer_size - 1);

    // Write more (will continue to overflow)
    for (uint32_t i = 0; i < 5; ++i) {
        ed->writeEvent(channel, 100 + i, 0);
    }

    // Should still report buffer_size-1 samples (oldest were overwritten)
    EXPECT_EQ(ed->getAvailableSamples(channel), buffer_size - 1);
}

// Test 29: countSamplesPerUnit() counts correctly
TEST_F(EventDataTest, CountSamplesPerUnitCountsCorrectly) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 7;

    // Write events with different units
    ed->writeEvent(channel, 1000, 0);  // unit 0
    ed->writeEvent(channel, 1001, 0);  // unit 0
    ed->writeEvent(channel, 1002, 1);  // unit 1
    ed->writeEvent(channel, 1003, 1);  // unit 1
    ed->writeEvent(channel, 1004, 1);  // unit 1
    ed->writeEvent(channel, 1005, 2);  // unit 2
    ed->writeEvent(channel, 1006, 3);  // unit 3

    uint32_t counts[cbMAXUNITS + 1] = {};
    ed->countSamplesPerUnit(channel, counts, false);

    EXPECT_EQ(counts[0], 2u);
    EXPECT_EQ(counts[1], 3u);
    EXPECT_EQ(counts[2], 1u);
    EXPECT_EQ(counts[3], 1u);
    EXPECT_EQ(counts[4], 0u);
}

// Test 30: countSamplesPerUnit() handles digital/serial channels
TEST_F(EventDataTest, CountSamplesPerUnitHandlesDigitalSerial) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 10;

    // Write events with different "units" (which are actually data values)
    ed->writeEvent(channel, 1000, 0x0001);
    ed->writeEvent(channel, 1001, 0x0002);
    ed->writeEvent(channel, 1002, 0x0004);
    ed->writeEvent(channel, 1003, 0x0008);

    uint32_t counts[cbMAXUNITS + 1] = {};
    ed->countSamplesPerUnit(channel, counts, true);  // is_digital_or_serial = true

    // All should be counted as unit 0
    EXPECT_EQ(counts[0], 4u);
    for (uint32_t i = 1; i <= cbMAXUNITS; ++i) {
        EXPECT_EQ(counts[i], 0u);
    }
}

// Test 31: readChannelEvents() reads and separates by unit
TEST_F(EventDataTest, ReadChannelEventsReadsAndSeparatesByUnit) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 5;

    // Write mixed units
    ed->writeEvent(channel, 1000, 0);
    ed->writeEvent(channel, 1001, 1);
    ed->writeEvent(channel, 1002, 0);
    ed->writeEvent(channel, 1003, 1);
    ed->writeEvent(channel, 1004, 2);

    // Prepare output arrays
    PROCTIME timestamps_unit0[10] = {};
    PROCTIME timestamps_unit1[10] = {};
    PROCTIME timestamps_unit2[10] = {};
    PROCTIME* timestamps[cbMAXUNITS + 1] = {};
    timestamps[0] = timestamps_unit0;
    timestamps[1] = timestamps_unit1;
    timestamps[2] = timestamps_unit2;

    uint32_t max_samples[cbMAXUNITS + 1] = {};
    std::fill_n(max_samples, cbMAXUNITS + 1, 10u);

    uint32_t actual_samples[cbMAXUNITS + 1] = {};
    uint32_t final_read_index = 0;

    ed->readChannelEvents(channel, max_samples, timestamps, nullptr,
                         actual_samples, false, false, final_read_index);

    // Check counts
    EXPECT_EQ(actual_samples[0], 2u);
    EXPECT_EQ(actual_samples[1], 2u);
    EXPECT_EQ(actual_samples[2], 1u);

    // Check timestamps
    EXPECT_EQ(timestamps_unit0[0], 1000u);
    EXPECT_EQ(timestamps_unit0[1], 1002u);
    EXPECT_EQ(timestamps_unit1[0], 1001u);
    EXPECT_EQ(timestamps_unit1[1], 1003u);
    EXPECT_EQ(timestamps_unit2[0], 1004u);
}

// Test 32: readChannelEvents() handles digital data
TEST_F(EventDataTest, ReadChannelEventsHandlesDigitalData) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 8;

    // Write digital events (data in unit field)
    ed->writeEvent(channel, 1000, 0x0001);
    ed->writeEvent(channel, 1001, 0x0002);
    ed->writeEvent(channel, 1002, 0x0004);

    // Prepare output arrays
    PROCTIME timestamps_unit0[10] = {};
    PROCTIME* timestamps[cbMAXUNITS + 1] = {};
    timestamps[0] = timestamps_unit0;

    uint16_t digital_data[10] = {};

    uint32_t max_samples[cbMAXUNITS + 1] = {};
    std::fill_n(max_samples, cbMAXUNITS + 1, 10u);

    uint32_t actual_samples[cbMAXUNITS + 1] = {};
    uint32_t final_read_index = 0;

    ed->readChannelEvents(channel, max_samples, timestamps, digital_data,
                         actual_samples, true, false, final_read_index);

    // All should be unit 0
    EXPECT_EQ(actual_samples[0], 3u);

    // Check digital data
    EXPECT_EQ(digital_data[0], 0x0001);
    EXPECT_EQ(digital_data[1], 0x0002);
    EXPECT_EQ(digital_data[2], 0x0004);

    // Check timestamps
    EXPECT_EQ(timestamps_unit0[0], 1000u);
    EXPECT_EQ(timestamps_unit0[1], 1001u);
    EXPECT_EQ(timestamps_unit0[2], 1002u);
}

// Test 33: readChannelEvents() with bSeek updates read position
TEST_F(EventDataTest, ReadChannelEventsWithSeekUpdatesPosition) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 6;

    // Write events
    for (uint32_t i = 0; i < 10; ++i) {
        ed->writeEvent(channel, 1000 + i, 0);
    }

    EXPECT_EQ(ed->getWriteStartIndex(channel), 0u);

    // Read 5 events with bSeek=true
    PROCTIME timestamps_unit0[10] = {};
    PROCTIME* timestamps[cbMAXUNITS + 1] = {};
    timestamps[0] = timestamps_unit0;

    uint32_t max_samples[cbMAXUNITS + 1] = {};
    max_samples[0] = 5;  // Read only 5

    uint32_t actual_samples[cbMAXUNITS + 1] = {};
    uint32_t final_read_index = 0;

    ed->readChannelEvents(channel, max_samples, timestamps, nullptr,
                         actual_samples, false, true, final_read_index);

    EXPECT_EQ(actual_samples[0], 5u);
    EXPECT_EQ(ed->getWriteStartIndex(channel), 5u);  // Should have advanced

    // Read remaining 5
    max_samples[0] = 10;
    actual_samples[0] = 0;
    ed->readChannelEvents(channel, max_samples, timestamps, nullptr,
                         actual_samples, false, true, final_read_index);

    EXPECT_EQ(actual_samples[0], 5u);  // Should read remaining 5
    EXPECT_EQ(ed->getWriteStartIndex(channel), 10u);
}

// Test 34: readChannelEvents() without bSeek doesn't update position
TEST_F(EventDataTest, ReadChannelEventsWithoutSeekDoesntUpdatePosition) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 9;

    // Write events
    for (uint32_t i = 0; i < 5; ++i) {
        ed->writeEvent(channel, 1000 + i, 0);
    }

    EXPECT_EQ(ed->getWriteStartIndex(channel), 0u);

    // Read with bSeek=false
    PROCTIME timestamps_unit0[10] = {};
    PROCTIME* timestamps[cbMAXUNITS + 1] = {};
    timestamps[0] = timestamps_unit0;

    uint32_t max_samples[cbMAXUNITS + 1] = {};
    std::fill_n(max_samples, cbMAXUNITS + 1, 10u);

    uint32_t actual_samples[cbMAXUNITS + 1] = {};
    uint32_t final_read_index = 0;

    ed->readChannelEvents(channel, max_samples, timestamps, nullptr,
                         actual_samples, false, false, final_read_index);

    EXPECT_EQ(actual_samples[0], 5u);
    EXPECT_EQ(ed->getWriteStartIndex(channel), 0u);  // Should not have changed

    // Read again - should get same data
    actual_samples[0] = 0;
    ed->readChannelEvents(channel, max_samples, timestamps, nullptr,
                         actual_samples, false, false, final_read_index);

    EXPECT_EQ(actual_samples[0], 5u);  // Same data again
    EXPECT_EQ(timestamps_unit0[0], 1000u);  // Same timestamps
}
