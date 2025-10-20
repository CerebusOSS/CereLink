#include <gtest/gtest.h>
#include "../src/cbsdk/EventData.h"
#include <thread>
#include <vector>
#include <atomic>

// This test file covers the refactored EventData class that manages
// a flat time-series ring buffer for spike and digital input event data.

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
    EXPECT_EQ(test_ed.getTimestamps(), nullptr);
    EXPECT_EQ(test_ed.getChannels(), nullptr);
    EXPECT_EQ(test_ed.getUnits(), nullptr);
    EXPECT_EQ(test_ed.getWriteIndex(), 0u);
    EXPECT_EQ(test_ed.getWriteStartIndex(), 0u);
}

// Test 2: allocate() allocates memory correctly
TEST_F(EventDataTest, AllocateAllocatesMemoryCorrectly) {
    const uint32_t buffer_size = 100;

    ASSERT_TRUE(ed->allocate(buffer_size));

    EXPECT_EQ(ed->getSize(), buffer_size);
    EXPECT_TRUE(ed->isAllocated());
    EXPECT_NE(ed->getTimestamps(), nullptr);
    EXPECT_NE(ed->getChannels(), nullptr);
    EXPECT_NE(ed->getUnits(), nullptr);
    EXPECT_EQ(ed->getWriteIndex(), 0u);
    EXPECT_EQ(ed->getWriteStartIndex(), 0u);
}

// Test 3: allocate() with same size reuses allocation
TEST_F(EventDataTest, AllocateWithSameSizeReusesAllocation) {
    const uint32_t buffer_size = 100;

    ASSERT_TRUE(ed->allocate(buffer_size));
    const PROCTIME* pOrigTimestamps = ed->getTimestamps();

    // Write some data
    ed->writeEvent(1, 12345, 1);
    EXPECT_EQ(ed->getWriteIndex(), 1u);

    // Allocate again with same size - should reset but keep allocation
    ASSERT_TRUE(ed->allocate(buffer_size));

    EXPECT_EQ(ed->getSize(), buffer_size);
    EXPECT_EQ(ed->getWriteIndex(), 0u);  // Reset
    EXPECT_TRUE(ed->isAllocated());
    const PROCTIME* pNewTimestamps = ed->getTimestamps();
    EXPECT_EQ(pOrigTimestamps, pNewTimestamps);  // Same allocation
}

// Test 4: allocate() with different size reallocates
TEST_F(EventDataTest, AllocateWithDifferentSizeReallocates) {
    ASSERT_TRUE(ed->allocate(100));
    EXPECT_EQ(ed->getSize(), 100u);
    const PROCTIME* pOrigTimestamps = ed->getTimestamps();

    ASSERT_TRUE(ed->allocate(200));
    EXPECT_EQ(ed->getSize(), 200u);
    EXPECT_TRUE(ed->isAllocated());
    const PROCTIME* pNewTimestamps = ed->getTimestamps();
    EXPECT_NE(pOrigTimestamps, pNewTimestamps);  // New allocation
}

// Test 5: writeEvent() stores data correctly
TEST_F(EventDataTest, WriteEventStoresDataCorrectly) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channel = 5;
    const PROCTIME timestamp = 123456;
    const uint16_t unit = 2;

    const bool overflow = ed->writeEvent(channel, timestamp, unit);

    EXPECT_FALSE(overflow);
    EXPECT_EQ(ed->getWriteIndex(), 1u);
    EXPECT_EQ(ed->getTimestamps()[0], timestamp);
    EXPECT_EQ(ed->getChannels()[0], channel);
    EXPECT_EQ(ed->getUnits()[0], unit);
}

// Test 6: writeEvent() advances write index
TEST_F(EventDataTest, WriteEventAdvancesWriteIndex) {
    ASSERT_TRUE(ed->allocate(100));

    bool overflow = false;
    for (uint32_t i = 0; i < 5; ++i) {
        overflow = ed->writeEvent(10, 1000 + i, 1);
        EXPECT_FALSE(overflow);
        EXPECT_EQ(ed->getWriteIndex(), i + 1);
    }
}

// Test 7: writeEvent() detects buffer overflow
TEST_F(EventDataTest, WriteEventDetectsOverflow) {
    const uint32_t buffer_size = 10;
    ASSERT_TRUE(ed->allocate(buffer_size));

    // Fill buffer to capacity (buffer_size writes should all succeed)
    bool overflow = false;
    for (uint32_t i = 0; i < buffer_size; ++i) {
        overflow = ed->writeEvent(1, i, 0);
        EXPECT_FALSE(overflow);  // No overflow yet - we have capacity for buffer_size events
    }

    // The next write (buffer_size + 1) should detect overflow
    overflow = ed->writeEvent(1, 999, 0);
    EXPECT_TRUE(overflow);
    EXPECT_EQ(ed->getWriteIndex(), 0u);  // Wrapped around
    EXPECT_EQ(ed->getWriteStartIndex(), 1u);  // Advanced

    // Subsequent writes should also overflow
    overflow = ed->writeEvent(1, 1000, 0);
    EXPECT_TRUE(overflow);

    // Write start index should have advanced again
    EXPECT_EQ(ed->getWriteIndex(), 1u);
    EXPECT_EQ(ed->getWriteStartIndex(), 2u);
}

// Test 8: writeEvent() wraps around correctly
TEST_F(EventDataTest, WriteEventWrapsAroundCorrectly) {
    const uint32_t buffer_size = 5;
    ASSERT_TRUE(ed->allocate(buffer_size));

    // Fill buffer and wrap around (8 writes total)
    for (uint32_t i = 0; i < buffer_size + 3; ++i) {
        ed->writeEvent(3, 1000 + i, static_cast<uint16_t>(i % 6));
    }

    // After 8 writes into a buffer of capacity 5 (internal size 6):
    // - write_index should be at 2 (next write position)
    // - write_start_index should be at 3 (oldest valid data)
    EXPECT_EQ(ed->getWriteIndex(), 2u);
    EXPECT_EQ(ed->getWriteStartIndex(), 3u);

    // Buffer contains data from writes 3-7 (the last 5 writes)
    // At indices: [6, 7, 2, 3, 4, 5] -> timestamps [1006, 1007, 1002, 1003, 1004, 1005]
    EXPECT_EQ(ed->getTimestamps()[0], 1006u);  // From write i=6
    EXPECT_EQ(ed->getTimestamps()[1], 1007u);  // From write i=7
    EXPECT_EQ(ed->getTimestamps()[2], 1002u);  // From write i=2
    EXPECT_EQ(ed->getTimestamps()[3], 1003u);  // From write i=3
    EXPECT_EQ(ed->getTimestamps()[4], 1004u);  // From write i=4
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
    const PROCTIME* pOrigTimestamps = ed->getTimestamps();

    // Write some data from multiple channels
    ed->writeEvent(1, 1000, 1);
    ed->writeEvent(5, 2000, 2);
    ed->writeEvent(10, 3000, 3);

    EXPECT_NE(ed->getWriteIndex(), 0u);

    ed->reset();

    // Indices should be cleared
    EXPECT_EQ(ed->getWriteIndex(), 0u);
    EXPECT_EQ(ed->getWriteStartIndex(), 0u);

    // Data should be zeroed
    EXPECT_EQ(ed->getTimestamps()[0], 0u);
    EXPECT_EQ(ed->getChannels()[0], 0u);
    EXPECT_EQ(ed->getUnits()[0], 0u);

    // Allocation should remain
    EXPECT_TRUE(ed->isAllocated());
    EXPECT_EQ(ed->getSize(), 100u);
    const PROCTIME* pNewTimestamps = ed->getTimestamps();
    EXPECT_EQ(pOrigTimestamps, pNewTimestamps);  // Same allocation
}

// Test 11: cleanup() deallocates all memory
TEST_F(EventDataTest, CleanupDeallocatesMemory) {
    ASSERT_TRUE(ed->allocate(100));

    EXPECT_TRUE(ed->isAllocated());

    ed->cleanup();

    EXPECT_FALSE(ed->isAllocated());
    EXPECT_EQ(ed->getSize(), 0u);
    EXPECT_EQ(ed->getTimestamps(), nullptr);
    EXPECT_EQ(ed->getChannels(), nullptr);
    EXPECT_EQ(ed->getUnits(), nullptr);
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

    // Write some events from different channels
    for (uint32_t i = 0; i < 5; ++i) {
        ed->writeEvent(7 + (i % 3), 1000 + i, 0);
    }

    const PROCTIME* timestamps = ed->getTimestamps();
    ASSERT_NE(timestamps, nullptr);

    for (uint32_t i = 0; i < 5; ++i) {
        EXPECT_EQ(timestamps[i], 1000u + i);
    }
}

// Test 14: getChannels() returns correct pointer
TEST_F(EventDataTest, GetChannelsReturnsCorrectPointer) {
    ASSERT_TRUE(ed->allocate(100));

    const uint16_t channels[] = {1, 5, 10, 15, 20};

    // Write events from different channels
    for (uint32_t i = 0; i < 5; ++i) {
        ed->writeEvent(channels[i], 1000, 0);
    }

    const uint16_t* channel_array = ed->getChannels();
    ASSERT_NE(channel_array, nullptr);

    for (uint32_t i = 0; i < 5; ++i) {
        EXPECT_EQ(channel_array[i], channels[i]);
    }
}

// Test 15: getUnits() returns correct pointer
TEST_F(EventDataTest, GetUnitsReturnsCorrectPointer) {
    ASSERT_TRUE(ed->allocate(100));

    // Write events with different units
    for (uint16_t i = 0; i < 5; ++i) {
        ed->writeEvent(15, 1000, i);
    }

    const uint16_t* units = ed->getUnits();
    ASSERT_NE(units, nullptr);

    for (uint16_t i = 0; i < 5; ++i) {
        EXPECT_EQ(units[i], i);
    }
}

// Test 16: setWriteIndex() and setWriteStartIndex() work correctly
TEST_F(EventDataTest, SettersWorkCorrectly) {
    ASSERT_TRUE(ed->allocate(100));

    ed->setWriteIndex(50);
    ed->setWriteStartIndex(10);

    EXPECT_EQ(ed->getWriteIndex(), 50u);
    EXPECT_EQ(ed->getWriteStartIndex(), 10u);
}

// Test 16b: setWriteIndex() and setWriteStartIndex() reject out-of-bounds values
// Note: Only test defensive behavior in release builds - debug builds will assert
#ifdef NDEBUG
TEST_F(EventDataTest, SettersRejectOutOfBounds) {
    ASSERT_TRUE(ed->allocate(100));

    // Set valid initial values
    ed->setWriteIndex(50);
    ed->setWriteStartIndex(10);

    // Try to set out-of-bounds values (should be silently ignored in release)
    ed->setWriteIndex(200);  // Beyond internal size of 101
    ed->setWriteStartIndex(200);

    // Values should remain unchanged
    EXPECT_EQ(ed->getWriteIndex(), 50u);
    EXPECT_EQ(ed->getWriteStartIndex(), 10u);
}
#endif  // NDEBUG

// Test 17: Events from multiple channels are stored in time order
TEST_F(EventDataTest, EventsFromMultipleChannelsStoredInTimeOrder) {
    ASSERT_TRUE(ed->allocate(100));

    // Write to different channels in specific order
    ed->writeEvent(1, 1000, 1);
    ed->writeEvent(5, 1001, 2);
    ed->writeEvent(1, 1002, 1);
    ed->writeEvent(10, 1003, 3);
    ed->writeEvent(5, 1004, 2);

    // Verify time-ordered storage
    const PROCTIME* timestamps = ed->getTimestamps();
    const uint16_t* channels = ed->getChannels();
    const uint16_t* units = ed->getUnits();

    EXPECT_EQ(timestamps[0], 1000u);
    EXPECT_EQ(channels[0], 1u);
    EXPECT_EQ(units[0], 1u);

    EXPECT_EQ(timestamps[1], 1001u);
    EXPECT_EQ(channels[1], 5u);
    EXPECT_EQ(units[1], 2u);

    EXPECT_EQ(timestamps[2], 1002u);
    EXPECT_EQ(channels[2], 1u);
    EXPECT_EQ(units[2], 1u);

    EXPECT_EQ(timestamps[3], 1003u);
    EXPECT_EQ(channels[3], 10u);
    EXPECT_EQ(units[3], 3u);

    EXPECT_EQ(timestamps[4], 1004u);
    EXPECT_EQ(channels[4], 5u);
    EXPECT_EQ(units[4], 2u);
}

// Test 18: All valid channels can write events
TEST_F(EventDataTest, AllChannelsCanWriteEvents) {
    ASSERT_TRUE(ed->allocate(cbMAXCHANS + 100));

    // Write to all channels
    for (uint16_t ch = 1; ch <= cbMAXCHANS; ++ch) {
        bool overflow = ed->writeEvent(ch, ch * 100, ch % 6);
        EXPECT_FALSE(overflow);
    }

    // Verify all channels wrote events
    EXPECT_EQ(ed->getWriteIndex(), cbMAXCHANS);

    const uint16_t* channels = ed->getChannels();
    for (uint16_t ch = 1; ch <= cbMAXCHANS; ++ch) {
        EXPECT_EQ(channels[ch - 1], ch);
    }
}

// Test 19: Parallel writes to same channel
TEST_F(EventDataTest, ParallelWritesToSameChannel) {
    constexpr int NUM_THREADS = 4;
    constexpr int WRITES_PER_THREAD = 100;
    constexpr uint16_t CHANNEL = 1;

    ASSERT_TRUE(ed->allocate(1000));

    std::vector<std::thread> threads;
    std::atomic<int> successful_writes(0);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, &successful_writes, WRITES_PER_THREAD, CHANNEL]() {
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

// Test 20: Parallel writes to different channels
TEST_F(EventDataTest, ParallelWritesToDifferentChannels) {
    constexpr int NUM_THREADS = 4;
    constexpr int WRITES_PER_THREAD = 50;

    ASSERT_TRUE(ed->allocate(500));

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, WRITES_PER_THREAD]() {
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

    // Verify total number of writes
    EXPECT_EQ(ed->getWriteIndex(), NUM_THREADS * WRITES_PER_THREAD);
}

// Test 21: Concurrent reads and writes with mutex
TEST_F(EventDataTest, ConcurrentReadsAndWritesWithMutex) {
    constexpr int NUM_WRITER_THREADS = 2;
    constexpr int NUM_READER_THREADS = 2;
    constexpr int OPERATIONS_PER_THREAD = 50;

    ASSERT_TRUE(ed->allocate(500));

    std::vector<std::thread> threads;
    std::atomic<int> read_successes(0);

    // Writer threads
    for (int t = 0; t < NUM_WRITER_THREADS; ++t) {
        threads.emplace_back([this, t, OPERATIONS_PER_THREAD]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                std::lock_guard<std::mutex> lock(ed->m_mutex);
                ed->writeEvent(1, t * 1000 + i, t % 6);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // Reader threads
    for (int t = 0; t < NUM_READER_THREADS; ++t) {
        threads.emplace_back([this, &read_successes, OPERATIONS_PER_THREAD]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                std::lock_guard<std::mutex> lock(ed->m_mutex);
                const PROCTIME* timestamps = ed->getTimestamps();
                const uint16_t* channels = ed->getChannels();
                const uint32_t write_idx = ed->getWriteIndex();

                if (timestamps && channels && write_idx > 0) {
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

// Test 22: Unit classification values
TEST_F(EventDataTest, UnitClassificationValues) {
    ASSERT_TRUE(ed->allocate(100));

    // Test different unit values (0-5 for sorted units, 255 for noise)
    const uint16_t units[] = {0, 1, 2, 3, 4, 5, 255};

    for (uint16_t unit : units) {
        ed->writeEvent(1, 1000, unit);
    }

    const uint16_t* unit_array = ed->getUnits();
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); ++i) {
        EXPECT_EQ(unit_array[i], units[i]);
    }
}

// Test 23: Large buffer allocation
TEST_F(EventDataTest, LargeBufferAllocation) {
    const uint32_t large_size = 100000;  // 100k total events

    ASSERT_TRUE(ed->allocate(large_size));
    EXPECT_EQ(ed->getSize(), large_size);

    // Write to a few channels to verify allocation works
    for (uint16_t ch = 1; ch <= 10; ++ch) {
        ed->writeEvent(ch, 1000, 1);
    }
    EXPECT_EQ(ed->getWriteIndex(), 10u);
}

// Test 24: Memory usage is reasonable
TEST_F(EventDataTest, MemoryUsageIsReasonable) {
    const uint32_t buffer_size = 10000;  // Typical event buffer size

    ASSERT_TRUE(ed->allocate(buffer_size));

    // Calculate memory usage for flat layout
    const size_t timestamp_mem = buffer_size * sizeof(PROCTIME);
    const size_t channel_mem = buffer_size * sizeof(uint16_t);
    const size_t unit_mem = buffer_size * sizeof(uint16_t);
    const size_t total_mem = timestamp_mem + channel_mem + unit_mem;

    // For 10000 events (assuming 64-bit PROCTIME):
    // Timestamps: 10000 * 8 = 80KB
    // Channels: 10000 * 2 = 20KB
    // Units: 10000 * 2 = 20KB
    // Total: ~120KB (much less than old per-channel layout!)

    // Verify it's less than 1MB total (very reasonable)
    EXPECT_LT(total_mem, 1 * 1024 * 1024);
}

// Test 25: getNumEvents() returns correct count
TEST_F(EventDataTest, GetNumEventsReturnsCorrectCount) {
    ASSERT_TRUE(ed->allocate(100));

    // Initially no events
    EXPECT_EQ(ed->getNumEvents(), 0u);

    // Write 10 events from various channels
    for (uint32_t i = 0; i < 10; ++i) {
        ed->writeEvent((i % 5) + 1, 1000 + i, 1);
    }

    EXPECT_EQ(ed->getNumEvents(), 10u);

    // Write 5 more
    for (uint32_t i = 0; i < 5; ++i) {
        ed->writeEvent(2, 2000 + i, 2);
    }

    EXPECT_EQ(ed->getNumEvents(), 15u);
}

// Test 26: getNumEvents() handles ring buffer wraparound
TEST_F(EventDataTest, GetNumEventsHandlesWraparound) {
    const uint32_t buffer_size = 10;
    ASSERT_TRUE(ed->allocate(buffer_size));

    // Fill buffer (buffer_size writes, no overflow yet)
    for (uint32_t i = 0; i < buffer_size; ++i) {
        ed->writeEvent(3, i, 0);
    }

    // After buffer_size writes, we have buffer_size events available
    EXPECT_EQ(ed->getNumEvents(), buffer_size);

    // Write more (will cause overflow)
    for (uint32_t i = 0; i < 5; ++i) {
        ed->writeEvent(3, 100 + i, 0);
    }

    // Should still report buffer_size events (oldest were overwritten)
    EXPECT_EQ(ed->getNumEvents(), buffer_size);
}

// Test 27: readEvents() reads flat arrays correctly
TEST_F(EventDataTest, ReadEventsReadsCorrectly) {
    ASSERT_TRUE(ed->allocate(100));

    // Write events from multiple channels
    ed->writeEvent(1, 1000, 0);
    ed->writeEvent(5, 1001, 1);
    ed->writeEvent(1, 1002, 0);
    ed->writeEvent(10, 1003, 2);
    ed->writeEvent(5, 1004, 1);

    // Read all events
    PROCTIME timestamps[10] = {};
    uint16_t channels[10] = {};
    uint16_t units[10] = {};

    const uint32_t num_read = ed->readEvents(timestamps, channels, units, 10, false);

    EXPECT_EQ(num_read, 5u);

    // Verify data
    EXPECT_EQ(timestamps[0], 1000u);
    EXPECT_EQ(channels[0], 1u);
    EXPECT_EQ(units[0], 0u);

    EXPECT_EQ(timestamps[1], 1001u);
    EXPECT_EQ(channels[1], 5u);
    EXPECT_EQ(units[1], 1u);

    EXPECT_EQ(timestamps[2], 1002u);
    EXPECT_EQ(channels[2], 1u);
    EXPECT_EQ(units[2], 0u);

    EXPECT_EQ(timestamps[3], 1003u);
    EXPECT_EQ(channels[3], 10u);
    EXPECT_EQ(units[3], 2u);

    EXPECT_EQ(timestamps[4], 1004u);
    EXPECT_EQ(channels[4], 5u);
    EXPECT_EQ(units[4], 1u);
}

// Test 28: readEvents() with partial read
TEST_F(EventDataTest, ReadEventsPartialRead) {
    ASSERT_TRUE(ed->allocate(100));

    // Write 10 events
    for (uint32_t i = 0; i < 10; ++i) {
        ed->writeEvent(1, 1000 + i, 0);
    }

    // Read only 5
    PROCTIME timestamps[5] = {};
    uint16_t channels[5] = {};
    uint16_t units[5] = {};

    const uint32_t num_read = ed->readEvents(timestamps, channels, units, 5, false);

    EXPECT_EQ(num_read, 5u);

    for (uint32_t i = 0; i < 5; ++i) {
        EXPECT_EQ(timestamps[i], 1000u + i);
    }
}

// Test 29: readEvents() with bSeek updates read position
TEST_F(EventDataTest, ReadEventsWithSeekUpdatesPosition) {
    ASSERT_TRUE(ed->allocate(100));

    // Write 10 events
    for (uint32_t i = 0; i < 10; ++i) {
        ed->writeEvent(1, 1000 + i, 0);
    }

    EXPECT_EQ(ed->getWriteStartIndex(), 0u);

    // Read 5 events with bSeek=true
    PROCTIME timestamps[10] = {};
    uint16_t channels[10] = {};
    uint16_t units[10] = {};

    uint32_t num_read = ed->readEvents(timestamps, channels, units, 5, true);

    EXPECT_EQ(num_read, 5u);
    EXPECT_EQ(ed->getWriteStartIndex(), 5u);  // Should have advanced

    // Read remaining 5
    num_read = ed->readEvents(timestamps, channels, units, 10, true);

    EXPECT_EQ(num_read, 5u);  // Should read remaining 5
    EXPECT_EQ(ed->getWriteStartIndex(), 10u);
}

// Test 30: readEvents() without bSeek doesn't update position
TEST_F(EventDataTest, ReadEventsWithoutSeekDoesntUpdatePosition) {
    ASSERT_TRUE(ed->allocate(100));

    // Write 5 events
    for (uint32_t i = 0; i < 5; ++i) {
        ed->writeEvent(1, 1000 + i, 0);
    }

    EXPECT_EQ(ed->getWriteStartIndex(), 0u);

    // Read with bSeek=false
    PROCTIME timestamps[10] = {};
    uint16_t channels[10] = {};
    uint16_t units[10] = {};

    uint32_t num_read = ed->readEvents(timestamps, channels, units, 10, false);

    EXPECT_EQ(num_read, 5u);
    EXPECT_EQ(ed->getWriteStartIndex(), 0u);  // Should not have changed

    // Read again - should get same data
    num_read = ed->readEvents(timestamps, channels, units, 10, false);

    EXPECT_EQ(num_read, 5u);  // Same data again
    EXPECT_EQ(timestamps[0], 1000u);  // Same timestamps
}

// Test 31: readEvents() handles ring buffer wraparound
TEST_F(EventDataTest, ReadEventsHandlesWraparound) {
    const uint32_t buffer_size = 5;
    ASSERT_TRUE(ed->allocate(buffer_size));

    // Fill buffer and wrap around (8 writes total)
    for (uint32_t i = 0; i < 8; ++i) {
        ed->writeEvent(1, 1000 + i, 0);
    }

    // Read all available events (should be buffer_size = 5)
    PROCTIME timestamps[10] = {};
    uint16_t channels[10] = {};
    uint16_t units[10] = {};

    const uint32_t num_read = ed->readEvents(timestamps, channels, units, 10, false);

    EXPECT_EQ(num_read, 5u);

    // Should read events 3-7 (the last 5 writes)
    EXPECT_EQ(timestamps[0], 1003u);
    EXPECT_EQ(timestamps[1], 1004u);
    EXPECT_EQ(timestamps[2], 1005u);
    EXPECT_EQ(timestamps[3], 1006u);
    EXPECT_EQ(timestamps[4], 1007u);
}

// Test 32: readEvents() with null pointers for unwanted data
TEST_F(EventDataTest, ReadEventsWithNullPointers) {
    ASSERT_TRUE(ed->allocate(100));

    ed->writeEvent(1, 1000, 0);
    ed->writeEvent(5, 1001, 1);
    ed->writeEvent(10, 1002, 2);

    // Read only timestamps
    PROCTIME timestamps[10] = {};
    uint32_t num_read = ed->readEvents(timestamps, nullptr, nullptr, 10, false);

    EXPECT_EQ(num_read, 3u);
    EXPECT_EQ(timestamps[0], 1000u);
    EXPECT_EQ(timestamps[1], 1001u);
    EXPECT_EQ(timestamps[2], 1002u);
}

// Test 33: Digital/serial data stored in unit field
TEST_F(EventDataTest, DigitalSerialDataInUnitField) {
    ASSERT_TRUE(ed->allocate(100));

    // Write digital events (data in unit field)
    ed->writeEvent(10, 1000, 0x0001);
    ed->writeEvent(10, 1001, 0x0002);
    ed->writeEvent(10, 1002, 0x0004);

    PROCTIME timestamps[10] = {};
    uint16_t channels[10] = {};
    uint16_t units[10] = {};

    const uint32_t num_read = ed->readEvents(timestamps, channels, units, 10, false);

    EXPECT_EQ(num_read, 3u);

    // Check digital data in unit field
    EXPECT_EQ(units[0], 0x0001);
    EXPECT_EQ(units[1], 0x0002);
    EXPECT_EQ(units[2], 0x0004);

    // Check timestamps
    EXPECT_EQ(timestamps[0], 1000u);
    EXPECT_EQ(timestamps[1], 1001u);
    EXPECT_EQ(timestamps[2], 1002u);
}
