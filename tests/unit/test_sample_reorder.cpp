///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_sample_reorder.cpp
/// @brief  Unit tests for cbdev::SampleReorderBuffer (#213 datagram reordering)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "sample_reorder.h"

#include <chrono>
#include <cstring>
#include <map>
#include <vector>

using cbdev::SampleReorderBuffer;
using namespace std::chrono_literals;

namespace {

constexpr PROCTIME kPeriod = 33333;       // group 6 / 5: ~30 kHz, in ns
constexpr PROCTIME kPeriod1k = 30 * kPeriod;  // group 2: 1 kHz
constexpr PROCTIME kT0 = 1'000'000'000;

struct Datagram {
    std::vector<uint8_t> bytes;

    Datagram& packet(uint16_t chid, uint16_t type, PROCTIME time, uint16_t dlen = 4) {
        cbPKT_HEADER h{};
        h.time = time;
        h.chid = chid;
        h.type = type;
        h.dlen = dlen;
        const size_t off = bytes.size();
        bytes.resize(off + cbPKT_HEADER_SIZE + size_t(dlen) * 4, 0);
        std::memcpy(bytes.data() + off, &h, sizeof(h));
        return *this;
    }
    /// Samples [first, first + n) of a 30 kHz group.
    Datagram& samples(int first, int n, uint16_t group = 6) {
        for (int i = 0; i < n; ++i)
            packet(0, group, kT0 + PROCTIME(first + i) * kPeriod);
        return *this;
    }
    Datagram& spike(uint16_t chan, PROCTIME time) { return packet(chan, 0, time, 2); }
    Datagram& monitor(PROCTIME time) {
        return packet(cbPKTCHAN_CONFIGURATION, cbPKTTYPE_SYSPROTOCOLMONITOR, time, 2);
    }
};

Datagram samples(int first, int n, uint16_t group = 6) { return Datagram{}.samples(first, n, group); }

/// Feeds datagrams; records delivered packets as (chid, type, time) per stream.
struct Harness {
    SampleReorderBuffer buf{3ms};
    std::map<uint16_t, std::vector<PROCTIME>> group;  // sample group -> delivered times
    std::vector<std::pair<uint16_t, uint16_t>> order;  // (chid, type) in delivery order
    SampleReorderBuffer::Clock::time_point now{};

    void record(const uint8_t* data, size_t len) {
        size_t off = 0;
        while (off + cbPKT_HEADER_SIZE <= len) {
            cbPKT_HEADER h;
            std::memcpy(&h, data + off, sizeof(h));
            if (h.chid == 0)
                group[h.type].push_back(h.time);
            order.emplace_back(h.chid, h.type);
            off += cbPKT_HEADER_SIZE + size_t(h.dlen) * 4;
        }
    }
    auto emitter() { return [this](const uint8_t* d, size_t n) { record(d, n); }; }
    void push(const Datagram& dg) { buf.push(dg.bytes.data(), dg.bytes.size(), now, emitter()); }
    void tick(std::chrono::microseconds dt) {
        now += dt;
        buf.releaseIfDue(now, emitter());
    }
    bool held() const { return buf.deadline().has_value(); }
    /// Group @p g delivered exactly @p n consecutive samples, in order.
    bool contiguous(uint16_t g, size_t n, PROCTIME period = kPeriod) const {
        auto it = group.find(g);
        if (it == group.end() || it->second.size() != n)
            return false;
        for (size_t i = 1; i < n; ++i)
            if (it->second[i] != it->second[i - 1] + period)
                return false;
        return true;
    }
};

}  // namespace

TEST(SampleReorder, InOrderTrafficIsNeverHeld) {
    Harness h;
    for (int ms = 0; ms < 10; ++ms) {
        h.push(samples(ms * 30, 30));
        EXPECT_FALSE(h.held());
    }
    // An in-order split (27, monitor, 3) is not held either.
    h.push(samples(300, 27));
    h.push(Datagram{}.monitor(kT0));
    h.push(samples(327, 3));
    EXPECT_FALSE(h.held());
    EXPECT_TRUE(h.contiguous(6, 330));
    EXPECT_EQ(h.buf.reorderedCount(), 0u);
}

/// The bench case: 27 + monitor + 3 on the wire; the host delivers the 3 first.
TEST(SampleReorder, RestoresSplitDeliveredTailFirst) {
    Harness h;
    h.push(samples(0, 30));
    h.push(samples(30, 30));
    h.push(samples(87, 3));   // tail of the split, after a gap: held
    EXPECT_TRUE(h.held());
    h.push(samples(60, 27));  // the late head: delivered first, then the tail
    EXPECT_FALSE(h.held());
    h.push(samples(90, 30));
    EXPECT_TRUE(h.contiguous(6, 120));
    EXPECT_EQ(h.buf.reorderedCount(), 1u);
}

/// Sample-free datagrams that arrive while the tail is held stay behind it.
TEST(SampleReorder, SampleFreeDatagramsQueueBehindHeldTail) {
    Harness h;
    h.push(samples(0, 30));
    h.push(samples(30, 30));
    h.push(samples(87, 3));                   // held
    h.push(Datagram{}.monitor(kT0 + 61 * kPeriod));  // queued, not passed through
    EXPECT_EQ(h.order.size(), 60u);
    h.push(samples(60, 27));                  // head: head, tail, then the monitor
    ASSERT_EQ(h.order.size(), 91u);
    EXPECT_EQ(h.order.back().first, cbPKTCHAN_CONFIGURATION);
    EXPECT_TRUE(h.contiguous(6, 90));
}

/// Genuine loss is normally followed by a full datagram: not held, no latency.
TEST(SampleReorder, FullDatagramAfterRealGapIsNotHeld) {
    Harness h;
    h.push(samples(0, 30));
    h.push(samples(30, 30));
    h.push(samples(90, 30));  // 60..89 lost
    EXPECT_FALSE(h.held());
    ASSERT_EQ(h.group[6].size(), 90u);
    EXPECT_EQ(h.group[6][59] + 31 * kPeriod, h.group[6][60]);  // the gap stays a gap
}

TEST(SampleReorder, SmallDatagramAfterRealGapReleasedByNextSamples) {
    Harness h;
    h.push(samples(0, 30));
    h.push(samples(30, 30));
    h.push(samples(87, 3));    // held, but its head was really lost
    h.push(samples(90, 30));   // samples that are not the head: release, then this
    EXPECT_FALSE(h.held());
    EXPECT_EQ(h.group[6].size(), 93u);
    EXPECT_EQ(h.buf.reorderedCount(), 0u);
}

TEST(SampleReorder, SmallDatagramAfterRealGapReleasedWhenHoldExpires) {
    Harness h;
    h.push(samples(0, 30));
    h.push(samples(30, 30));
    h.push(samples(87, 3));  // held
    h.tick(2ms);
    EXPECT_EQ(h.group[6].size(), 60u);
    h.tick(2ms);             // past the 3 ms hold
    EXPECT_EQ(h.group[6].size(), 63u);
    EXPECT_FALSE(h.held());
}

/// Group 6 alongside group 2 (1 kHz).  The tail datagram starts with a group-2
/// packet, so detection must look at every group it carries, not just the first.
TEST(SampleReorder, DetectsGapInEitherOfTwoGroups) {
    Harness h;
    for (int ms = 0; ms < 3; ++ms)
        h.push(Datagram{}.packet(0, 2, kT0 + PROCTIME(ms) * kPeriod1k).samples(ms * 30, 30));
    EXPECT_FALSE(h.held());  // group 2's 1 ms steps are not gaps for group 2
    // The tail of a split of ms 3 arrives first: a group-2 sample, then group-6 87..89.
    h.push(Datagram{}.packet(0, 2, kT0 + 3 * kPeriod1k).samples(117, 3));
    EXPECT_TRUE(h.held());
    h.push(samples(90, 27));
    EXPECT_FALSE(h.held());
    EXPECT_TRUE(h.contiguous(6, 120));
    EXPECT_TRUE(h.contiguous(2, 4, kPeriod1k));
}

/// Group 6 is published ahead of groups 1-4 for the same instant; cross-group
/// skew must not look like a gap or a reversal.
TEST(SampleReorder, CrossGroupSkewIsIgnored) {
    Harness h;
    for (int ms = 0; ms < 5; ++ms) {
        Datagram dg;
        dg.samples(ms * 30, 30);
        if (ms > 0)
            dg.packet(0, 2, kT0 + PROCTIME(ms - 1) * kPeriod1k);  // group 2 lags by 1 ms
        h.push(dg);
        EXPECT_FALSE(h.held());
    }
}

/// Spike packets are interleaved with newer samples; they never trigger a hold.
TEST(SampleReorder, SpikesDoNotAffectDetection) {
    Harness h;
    for (int ms = 0; ms < 5; ++ms) {
        Datagram dg;
        dg.samples(ms * 30, 15).spike(7, kT0 + PROCTIME(ms * 30 - 40) * kPeriod).samples(ms * 30 + 15, 15);
        h.push(dg);
        EXPECT_FALSE(h.held());
    }
    h.push(Datagram{}.spike(3, kT0));  // a spike-only datagram, out of time order
    EXPECT_FALSE(h.held());
    EXPECT_TRUE(h.contiguous(6, 150));
}

TEST(SampleReorder, NoHoldBeforeThePeriodIsKnown) {
    Harness h;
    h.push(samples(10, 1));
    h.push(samples(50, 1));
    EXPECT_FALSE(h.held());
}

TEST(SampleReorder, TimestampResetIsNotAGap) {
    Harness h;
    h.push(samples(1000, 30));
    h.push(samples(0, 30));  // device restarted: time went back
    h.push(samples(30, 3));
    EXPECT_FALSE(h.held());
    EXPECT_EQ(h.group[6].size(), 63u);
}

TEST(SampleReorder, ZeroHoldDisablesReordering) {
    SampleReorderBuffer buf{0us};
    std::vector<PROCTIME> got;
    auto rec = [&](const uint8_t* d, size_t) {
        cbPKT_HEADER hdr;
        std::memcpy(&hdr, d, sizeof(hdr));
        got.push_back(hdr.time);
    };
    for (auto& dg : {samples(0, 30), samples(30, 30), samples(87, 3), samples(60, 27)})
        buf.push(dg.bytes.data(), dg.bytes.size(), {}, rec);
    ASSERT_EQ(got.size(), 4u);
    EXPECT_GT(got[2], got[3]);  // left exactly as received
}

TEST(SampleReorder, HeldCopyLengthExcludesPadding) {
    SampleReorderBuffer buf{3ms, 4096};
    std::vector<size_t> seen;
    auto rec = [&](const uint8_t*, size_t n) { seen.push_back(n); };
    for (auto& dg : {samples(0, 30), samples(30, 30)})
        buf.push(dg.bytes.data(), dg.bytes.size(), {}, rec);
    auto tail = samples(87, 3);
    buf.push(tail.bytes.data(), tail.bytes.size(), {}, rec);
    buf.releaseAll(rec);
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[2], tail.bytes.size());
}
