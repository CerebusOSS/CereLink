///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_sdk_data_reception.cpp
/// @brief  Integration tests for C++ SdkSession data reception
///
/// Tests callback-based data paths: group callbacks, batch callbacks, event
/// callbacks, and multi-rate reception against a live nPlayServer instance.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "nplay_fixture.h"

#include <cbproto/cbproto.h>
#include <cbsdk/sdk_session.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

using namespace cbsdk;

static constexpr size_t N_CHANS = 4;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Helper
///////////////////////////////////////////////////////////////////////////////////////////////////

static Result<SdkSession> createNPlaySession() {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;
    return SdkSession::create(config);
}

static void setupChannels(SdkSession& session, SampleRate rate = SampleRate::SR_30kHz) {
    session.setChannelSampleGroup(N_CHANS, ChannelType::FRONTEND, rate, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Group Callback (per-sample)
///////////////////////////////////////////////////////////////////////////////////////////////////

class GroupCallbackTest : public NPlayFixture {};

TEST_F(GroupCallbackTest, ReceivesSamples) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();
    setupChannels(session);

    std::atomic<int> count{0};
    session.registerGroupCallback(SampleRate::SR_30kHz,
        [&count](const cbPKT_GROUP& pkt) {
            count.fetch_add(1, std::memory_order_relaxed);
        });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.stop();

    EXPECT_GT(count.load(), 1000) << "Expected >1000 samples at 30kHz in 1s";
}

TEST_F(GroupCallbackTest, ReceivesSamplesAt1kHz) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();
    setupChannels(session, SampleRate::SR_1kHz);

    std::atomic<int> count{0};
    session.registerGroupCallback(SampleRate::SR_1kHz,
        [&count](const cbPKT_GROUP& pkt) {
            count.fetch_add(1, std::memory_order_relaxed);
        });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.stop();

    EXPECT_GT(count.load(), 500) << "Expected >500 samples at 1kHz in 1s";
    EXPECT_LT(count.load(), 2000) << "Sanity: too many samples for 1kHz";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Batch Group Callback
///////////////////////////////////////////////////////////////////////////////////////////////////

class BatchCallbackTest : public NPlayFixture {};

TEST_F(BatchCallbackTest, ReceivesBatches) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();
    setupChannels(session);

    std::atomic<int> batch_count{0};
    std::atomic<int> total_samples{0};
    size_t first_n_channels = 0;

    session.registerGroupBatchCallback(SampleRate::SR_30kHz,
        [&](const int16_t* samples, size_t n_samples, size_t n_channels,
            const uint64_t* timestamps) {
            batch_count.fetch_add(1, std::memory_order_relaxed);
            total_samples.fetch_add(static_cast<int>(n_samples), std::memory_order_relaxed);
            if (first_n_channels == 0) first_n_channels = n_channels;
        });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.stop();

    EXPECT_GT(batch_count.load(), 0) << "No batch callbacks received";
    EXPECT_GT(total_samples.load(), 1000) << "Too few total samples";
    EXPECT_EQ(first_n_channels, N_CHANS);
}

TEST_F(BatchCallbackTest, TimestampsIncrease) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();
    setupChannels(session);

    std::vector<uint64_t> all_timestamps;
    std::mutex mtx;

    session.registerGroupBatchCallback(SampleRate::SR_30kHz,
        [&](const int16_t* samples, size_t n_samples, size_t n_channels,
            const uint64_t* timestamps) {
            std::lock_guard<std::mutex> lock(mtx);
            for (size_t i = 0; i < n_samples; ++i)
                all_timestamps.push_back(timestamps[i]);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    session.stop();

    ASSERT_GT(all_timestamps.size(), 100u);

    // Check monotonicity
    for (size_t i = 1; i < std::min(all_timestamps.size(), size_t(1000)); ++i) {
        EXPECT_GT(all_timestamps[i], all_timestamps[i - 1])
            << "Timestamp not increasing at index " << i;
    }
}

TEST_F(BatchCallbackTest, BatchesContainMultipleSamples) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();
    setupChannels(session);

    std::vector<size_t> batch_sizes;
    std::mutex mtx;

    session.registerGroupBatchCallback(SampleRate::SR_30kHz,
        [&](const int16_t* samples, size_t n_samples, size_t n_channels,
            const uint64_t* timestamps) {
            std::lock_guard<std::mutex> lock(mtx);
            batch_sizes.push_back(n_samples);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    session.stop();

    ASSERT_GT(batch_sizes.size(), 0u);

    double sum = 0;
    for (auto s : batch_sizes) sum += s;
    double avg = sum / batch_sizes.size();
    EXPECT_GT(avg, 1.0) << "Average batch size " << avg << " — expected batching";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Event Callback
///////////////////////////////////////////////////////////////////////////////////////////////////

class EventCallbackTest : public NPlayFixture {};

TEST_F(EventCallbackTest, RegisterEventCallback) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::atomic<int> count{0};
    session.registerEventCallback(ChannelType::FRONTEND,
        [&count](const cbPKT_GENERIC& pkt) {
            count.fetch_add(1, std::memory_order_relaxed);
        });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.stop();

    // The test file may or may not contain spike events — just verify no crash.
}

TEST_F(EventCallbackTest, RegisterAllEvents) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::atomic<int> count{0};
    session.registerEventCallback(ChannelType::ANY,
        [&count](const cbPKT_GENERIC& pkt) {
            count.fetch_add(1, std::memory_order_relaxed);
        });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.stop();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Callback (catch-all)
///////////////////////////////////////////////////////////////////////////////////////////////////

class PacketCallbackTest : public NPlayFixture {};

TEST_F(PacketCallbackTest, ReceivesAllPackets) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::atomic<int> count{0};
    session.registerPacketCallback([&count](const cbPKT_GENERIC& pkt) {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.stop();

    EXPECT_GT(count.load(), 0) << "No packets received via catch-all callback";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Multi-rate Reception
///////////////////////////////////////////////////////////////////////////////////////////////////

class MultiRateTest : public NPlayFixture {};

TEST_F(MultiRateTest, TwoRatesSimultaneously) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // Configure first 2 channels at 30kHz
    session.setChannelSampleGroup(2, ChannelType::FRONTEND, SampleRate::SR_30kHz, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Get channel IDs and individually configure channels 3-4 at 1kHz
    // by reading their chaninfo, modifying smpgroup, and sending it back.
    auto ids_result = session.getMatchingChannelIds(cbMAXCHANS, ChannelType::FRONTEND);
    ASSERT_TRUE(ids_result.isOk());
    auto& ids = ids_result.value();
    ASSERT_GE(ids.size(), 4u);

    for (size_t i = 2; i < 4; ++i) {
        const auto* info = session.getChanInfo(ids[i]);
        ASSERT_NE(info, nullptr) << "getChanInfo returned null for channel " << ids[i];
        cbPKT_CHANINFO modified = *info;
        modified.smpgroup = static_cast<uint32_t>(SampleRate::SR_1kHz);
        // Must set the packet type to SET (not REP) for the device to accept it
        modified.cbpkt_header.type = cbPKTTYPE_CHANSET;
        auto set_result = session.setChannelConfig(modified);
        EXPECT_TRUE(set_result.isOk()) << "setChannelConfig failed for ch " << ids[i]
            << ": " << set_result.error();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::atomic<int> count_30k{0};
    std::atomic<int> count_1k{0};

    session.registerGroupCallback(SampleRate::SR_30kHz,
        [&count_30k](const cbPKT_GROUP& pkt) {
            count_30k.fetch_add(1, std::memory_order_relaxed);
        });
    session.registerGroupCallback(SampleRate::SR_1kHz,
        [&count_1k](const cbPKT_GROUP& pkt) {
            count_1k.fetch_add(1, std::memory_order_relaxed);
        });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.stop();

    EXPECT_GT(count_30k.load(), 0) << "No 30kHz samples received";
    EXPECT_GT(count_1k.load(), 0) << "No 1kHz samples received";

    double ratio = static_cast<double>(count_30k.load()) / std::max(count_1k.load(), 1);
    EXPECT_GT(ratio, 10.0) << "Rate ratio " << ratio << " — expected ~30x";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Error Callback
///////////////////////////////////////////////////////////////////////////////////////////////////

class ErrorCallbackTest : public NPlayFixture {};

TEST_F(ErrorCallbackTest, NoErrorsDuringNormalOperation) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();
    setupChannels(session);

    std::atomic<int> error_count{0};
    session.setErrorCallback([&error_count](const std::string& msg) {
        error_count.fetch_add(1, std::memory_order_relaxed);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    session.stop();

    EXPECT_EQ(error_count.load(), 0);
}
