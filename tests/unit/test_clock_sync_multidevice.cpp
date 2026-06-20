///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_clock_sync_multidevice.cpp
/// @author CereLink Development Team
///
/// @brief  Cross-device clock-synchronization consistency tests
///
/// Pins down the Gemini NSP/HUB shared-clock bug at the smallest seams that
/// expose it: (B6) two ClockSync instances that share one device clock and one
/// host clock must map a common device timestamp to agreeing host times; and
/// (B7) the peer-borrow leak, driven through the production ShmemSession clock
/// exchange (setClockSync / getClockOffsetNs) and ClockSync::setExternalOffset
/// (which DeviceSession::setExternalClockOffset forwards to verbatim).
///
/// Each test is labelled GREEN (guards existing correct behavior) or RED
/// (encodes the INTENDED invariant; EXPECTED TO FAIL on current code).
///
/// --------------------------------------------------------------------------
/// (C) Multi-device INTEGRATION feasibility — explicit conclusion
/// --------------------------------------------------------------------------
/// A real multi-device integration test is NOT achievable with the existing
/// nPlay harness, but IS achievable on a live rig with real Gemini devices on a
/// shared PTP domain (the maintainer's current bench has an NSP + HUB1 + HUB2).
/// Why nPlay alone is insufficient:
///   * NPlayFixture (tests/integration/nplay_fixture.h) launches a SINGLE
///     nPlayServer process on the loopback legacy-NSP ports.  There is no
///     second simulated device, and no HUB-typed device that publishes a
///     "hub config" NATIVE shmem segment for an NSP to borrow from.
///   * nPlayServer presents as a legacy/nPlay device (sample-count timestamps,
///     not the Gemini nanosecond PTP clock), so it cannot reproduce the
///     wall-clock-epoch offset (~1.77e18 ns) that the bug hinges on, nor the
///     DATA_FALLBACK_UNCERT==700us gate that triggers the peer borrow.
///   * Standing up two SdkSessions against one nPlayServer would have them
///     fight over the same loopback ports and shmem segment names.
/// The live-hardware integration test for the NSP + HUB1 + HUB2 topology lives
/// separately in tests/integration/test_multidevice_clock_sync.cpp.  It is
/// hardware-gated (opt-in via an env var) because bringing up an SdkSession runs
/// a handshake that can HARDRESET the device — disruptive on a recording rig —
/// so it must never run in unattended CI.  The deterministic, always-on
/// multi-device coverage (NSP + HUB1 + HUB2, three ClockSyncs sharing one clock,
/// plus the first-HUB-wins peer borrow with two HUBs) is carried by (B6)/(B7)
/// below, which exercise the real cross-device seams (ClockSync + the production
/// ShmemSession exchange).
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "cbdev/clock_sync.h"
#include <cbshm/shmem_session.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>
#ifdef _WIN32
#  include <process.h>
#  define getpid _getpid
#else
#  include <unistd.h>
#endif

using namespace cbdev;
using cbshm::ShmemSession;
using cbshm::Mode;
using cbshm::ShmemLayout;
using SteadyTP = ClockSync::time_point;

namespace {

SteadyTP tp_from_ns(int64_t ns) {
    return SteadyTP(std::chrono::nanoseconds(ns));
}
int64_t tp_to_ns(SteadyTP tp) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        tp.time_since_epoch()).count();
}

// Same realistic magnitudes as test_clock_sync.cpp: host steady_clock ~5000 s
// uptime, device(PTP) ~1.77e18 ns wall clock, true offset ~1.77e18.
constexpr int64_t HOST_NOW_NS = 5'000'000'000'000LL;
constexpr int64_t TRUE_OFFSET_NS = 1'770'000'000'000'000'000LL;
constexpr uint64_t DEVICE_NOW_NS =
    static_cast<uint64_t>(HOST_NOW_NS + TRUE_OFFSET_NS);

// Feed a ClockSync with tight, symmetric probes establishing offset==TRUE_OFFSET.
// This is the HUB1 path (probes reliable).
void feedReliableProbes(ClockSync& sync) {
    for (int k = 0; k < 4; ++k) {
        const int64_t mid = HOST_NOW_NS + k * 1'000'000LL;
        sync.addProbeSample(tp_from_ns(mid - 1000),
                            static_cast<uint64_t>(mid + TRUE_OFFSET_NS),
                            tp_from_ns(mid + 1000));
    }
}

// Feed a ClockSync with data-packet samples (no probes) establishing the
// data-floor estimate ≈ TRUE_OFFSET + 400us.  This is the NSP fallback path.
void feedDataFallback(ClockSync& sync, int count = 8) {
    for (int k = 0; k < count; ++k) {
        const int64_t recv_ns = HOST_NOW_NS + k * 1'000'000LL;
        const uint64_t device_ns =
            static_cast<uint64_t>(recv_ns + TRUE_OFFSET_NS - 300'000);
        sync.addDataPacketSample(device_ns, tp_from_ns(recv_ns));
    }
}

// Replicates the SdkSession peer-borrow conditional (src/cbsdk/src/sdk_session.cpp
// ~1023-1029): read the peer's published offset from shmem and inject it via the
// device's external-clock-offset setter.  The real borrow lives inside a private
// datagram-complete lambda on SdkSession::Impl, so it cannot be called directly;
// this helper drives the identical public sequence (ShmemSession::getClockOffsetNs
// + ClockSync::setExternalOffset, which DeviceSession::setExternalClockOffset
// forwards to verbatim).  Gap noted: the uncertainty-gate / once-only latching in
// the SDK lambda is covered separately by the ClockSync latch test (A4).
void borrowPeerOffset(const ShmemSession& peer, ClockSync& nsp_sync) {
    auto peer_offset = peer.getClockOffsetNs();
    auto peer_uncert = peer.getClockUncertaintyNs();
    if (peer_offset) {
        nsp_sync.setExternalOffset(peer_offset, peer_uncert);
    } else {
        nsp_sync.setExternalOffset(std::nullopt);
    }
}

// Models the SDK's multi-HUB peer-borrow (src/cbsdk/src/sdk_session.cpp
// ~1015-1029): the NSP iterates the HUBs in priority order {HUB1, HUB2, HUB3},
// borrows from the FIRST segment that opens, and STOPS (peer_clock_attempted
// latches after a single attempt).  It therefore never consults a later HUB,
// even if the first HUB's published offset is stale.  Gap noted: production
// resolves segments via getNativeSegmentName(HUB*, "config") (static in
// sdk_session.cpp); here the priority list is passed explicitly.
void borrowFromFirstOpenHub(const std::vector<const ShmemSession*>& hubs_in_priority,
                            ClockSync& nsp_sync) {
    const ShmemSession* chosen = nullptr;
    for (const auto* h : hubs_in_priority) {
        if (h && h->isOpen()) {  // first that "opens" wins, then break
            chosen = h;
            break;
        }
    }
    if (!chosen) {
        nsp_sync.setExternalOffset(std::nullopt);
        return;
    }
    auto peer_offset = chosen->getClockOffsetNs();
    auto peer_uncert = chosen->getClockUncertaintyNs();
    if (peer_offset) {
        nsp_sync.setExternalOffset(peer_offset, peer_uncert);
    } else {
        nsp_sync.setExternalOffset(std::nullopt);
    }
}

// Helper to create a NATIVE STANDALONE shmem session with unique segment names.
cbshm::Result<ShmemSession> makeNativeStandalone(const std::string& name) {
    return ShmemSession::create(
        name + "_cfg", name + "_rec", name + "_xmt", name + "_xmt_local",
        name + "_status", name + "_spk", name + "_signal",
        Mode::STANDALONE, ShmemLayout::NATIVE);
}
// Helper to attach a NATIVE CLIENT session to the same segment names.
cbshm::Result<ShmemSession> makeNativeClient(const std::string& name) {
    return ShmemSession::create(
        name + "_cfg", name + "_rec", name + "_xmt", name + "_xmt_local",
        name + "_status", name + "_spk", name + "_signal",
        Mode::CLIENT, ShmemLayout::NATIVE);
}

// Keep names short: macOS POSIX shared-memory names are limited to ~31 chars
// total, and create() appends suffixes like "_xmt_local".
int g_seg_counter = 0;
std::string uniqueSegName() {
    return "csmd" + std::to_string(g_seg_counter++);
}

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////////////////////////
// (B6) Cross-device consistency for two devices sharing one clock
///////////////////////////////////////////////////////////////////////////////////////////////////

// (B6) GREEN — A HUB (probe path) and an NSP (data-fallback path) that share one
// device clock and one host clock, fed equivalent evidence, must map a COMMON
// device timestamp to host times that agree within tolerance and neither may
// leak the raw device clock.
TEST(ClockSyncMultiDeviceTest, SharedClockTwoDevicesAgree) {
    ClockSync hub;   // probe path (reliable)
    ClockSync nsp;   // data-packet fallback path

    feedReliableProbes(hub);
    feedDataFallback(nsp);

    auto hub_local = hub.toLocalTime(DEVICE_NOW_NS);
    auto nsp_local = nsp.toLocalTime(DEVICE_NOW_NS);
    ASSERT_TRUE(hub_local.has_value());
    ASSERT_TRUE(nsp_local.has_value());

    const int64_t hub_ns = tp_to_ns(*hub_local);
    const int64_t nsp_ns = tp_to_ns(*nsp_local);

    // Both land near the true host instant and agree with each other within the
    // one-way-delay/uncertainty budget (well under 2 ms).
    EXPECT_NEAR(static_cast<double>(hub_ns), static_cast<double>(HOST_NOW_NS), 2'000'000.0);
    EXPECT_NEAR(static_cast<double>(nsp_ns), static_cast<double>(HOST_NOW_NS), 2'000'000.0);
    EXPECT_LT(std::llabs(hub_ns - nsp_ns), 2'000'000LL)
        << "shared-clock devices disagree: hub=" << hub_ns << " nsp=" << nsp_ns;

    // Neither leaks the raw device clock.
    EXPECT_GT(std::llabs(static_cast<long long>(DEVICE_NOW_NS) - hub_ns), 1'000'000'000LL);
    EXPECT_GT(std::llabs(static_cast<long long>(DEVICE_NOW_NS) - nsp_ns), 1'000'000'000LL);
}

// (B6-3dev) GREEN — Full connected topology: a Gemini NSP, HUB1 and HUB2 all
// share one PTP device clock and one host clock.  HUB1/HUB2 use the probe path;
// the NSP uses the data-packet fallback.  All three must map a COMMON device
// timestamp to host times that agree within tolerance and none may leak the raw
// device clock.  (Mirrors the user's live rig: NSP + HUB1 + HUB2.)
TEST(ClockSyncMultiDeviceTest, SharedClockThreeDevicesAgree) {
    ClockSync nsp;    // data-packet fallback path
    ClockSync hub1;   // probe path (reliable)
    ClockSync hub2;   // probe path (reliable)

    feedDataFallback(nsp);
    feedReliableProbes(hub1);
    feedReliableProbes(hub2);

    struct Named { const char* name; ClockSync* cs; };
    const Named devs[] = {{"NSP", &nsp}, {"HUB1", &hub1}, {"HUB2", &hub2}};

    int64_t mapped[3];
    for (int i = 0; i < 3; ++i) {
        auto local = devs[i].cs->toLocalTime(DEVICE_NOW_NS);
        ASSERT_TRUE(local.has_value()) << devs[i].name << " produced no mapping";
        mapped[i] = tp_to_ns(*local);
        // None leaks the raw device clock.
        EXPECT_GT(std::llabs(static_cast<long long>(DEVICE_NOW_NS) - mapped[i]),
                  1'000'000'000LL)
            << devs[i].name << " leaked the raw device clock";
        // Each lands near the true host instant.
        EXPECT_NEAR(static_cast<double>(mapped[i]),
                    static_cast<double>(HOST_NOW_NS), 2'000'000.0)
            << devs[i].name << " off from host time";
    }
    // All pairs agree within the one-way-delay/uncertainty budget.
    EXPECT_LT(std::llabs(mapped[0] - mapped[1]), 2'000'000LL) << "NSP vs HUB1";
    EXPECT_LT(std::llabs(mapped[0] - mapped[2]), 2'000'000LL) << "NSP vs HUB2";
    EXPECT_LT(std::llabs(mapped[1] - mapped[2]), 2'000'000LL) << "HUB1 vs HUB2";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// (B7) Peer-borrow leak through the production ShmemSession exchange
///////////////////////////////////////////////////////////////////////////////////////////////////

// (B7a) GREEN — When the HUB publishes a GOOD offset, the NSP borrowing it via
// the real shmem exchange agrees with the HUB and does not leak the raw clock.
// Confirms the borrow mechanism itself is sound when the input is sane.
TEST(ClockSyncMultiDeviceTest, PeerBorrowGoodOffsetAgrees) {
    const std::string name = uniqueSegName();

    auto hub_shmem_r = makeNativeStandalone(name);
    ASSERT_TRUE(hub_shmem_r.isOk()) << hub_shmem_r.error();
    auto& hub_shmem = hub_shmem_r.value();

    // HUB derives a good offset from probes and publishes it to shmem.
    ClockSync hub;
    feedReliableProbes(hub);
    ASSERT_TRUE(hub.getOffsetNs().has_value());
    hub_shmem.setClockSync(*hub.getOffsetNs(), hub.getUncertaintyNs().value_or(0));

    // NSP attaches as CLIENT and borrows the peer offset.
    auto nsp_client_r = makeNativeClient(name);
    ASSERT_TRUE(nsp_client_r.isOk()) << nsp_client_r.error();
    auto& nsp_client = nsp_client_r.value();

    ClockSync nsp;
    feedDataFallback(nsp);          // NSP also has its own (good) evidence
    borrowPeerOffset(nsp_client, nsp);

    auto hub_local = hub.toLocalTime(DEVICE_NOW_NS);
    auto nsp_local = nsp.toLocalTime(DEVICE_NOW_NS);
    ASSERT_TRUE(hub_local.has_value());
    ASSERT_TRUE(nsp_local.has_value());
    const int64_t hub_ns = tp_to_ns(*hub_local);
    const int64_t nsp_ns = tp_to_ns(*nsp_local);

    EXPECT_LT(std::llabs(hub_ns - nsp_ns), 2'000'000LL)
        << "after borrowing a good peer offset, NSP disagrees with HUB";
    EXPECT_GT(std::llabs(static_cast<long long>(DEVICE_NOW_NS) - nsp_ns), 1'000'000'000LL)
        << "NSP leaked the raw device clock after a good borrow";
}

// (B7b) RED — Reproduces the peer-borrow leak deterministically.  The HUB's
// published offset is intermittently stale/implausible (here 0, modelling a
// segment whose clock_offset_ns has not yet caught up to the PTP epoch).  The
// NSP has its OWN good data-packet evidence (offset ≈ TRUE_OFFSET) but borrows
// the bad peer offset and adopts it with unconditional priority — collapsing
// toLocalTime() to the raw device/PTP clock (~1.77e18 ns).
//
// INTENDED invariant: the borrowed peer offset must be sanity-checked against
// the NSP's own evidence and rejected when implausible, so the NSP keeps
// mapping near the true host time and agrees with the HUB's correct mapping.
//
// Root cause: ClockSync::recomputeEstimate / setExternalOffset
// (src/cbdev/src/clock_sync.cpp:191-198, 112-125) give the external offset
// unconditional priority with no sanity bound.  EXPECTED FAIL.
TEST(ClockSyncMultiDeviceTest, PeerBorrowStaleOffsetLeaksRawClock) {
    const std::string name = uniqueSegName();

    auto hub_shmem_r = makeNativeStandalone(name);
    ASSERT_TRUE(hub_shmem_r.isOk()) << hub_shmem_r.error();
    auto& hub_shmem = hub_shmem_r.value();

    // What the HUB's correct mapping looks like (for the agreement check).
    ClockSync hub_reference;
    feedReliableProbes(hub_reference);
    ASSERT_TRUE(hub_reference.toLocalTime(DEVICE_NOW_NS).has_value());
    const int64_t hub_ref_ns = tp_to_ns(*hub_reference.toLocalTime(DEVICE_NOW_NS));

    // HUB publishes a STALE/implausible offset (0) but marks it valid.
    hub_shmem.setClockSync(0, 700'000);

    // NSP has good internal data evidence ≈ TRUE_OFFSET.
    ClockSync nsp;
    feedDataFallback(nsp);
    ASSERT_TRUE(nsp.toLocalTime(DEVICE_NOW_NS).has_value());

    // NSP borrows the bad peer offset through the real shmem read.
    auto nsp_client_r = makeNativeClient(name);
    ASSERT_TRUE(nsp_client_r.isOk()) << nsp_client_r.error();
    borrowPeerOffset(nsp_client_r.value(), nsp);

    auto nsp_local = nsp.toLocalTime(DEVICE_NOW_NS);
    ASSERT_TRUE(nsp_local.has_value());
    const int64_t nsp_ns = tp_to_ns(*nsp_local);

    // INTENDED: NSP must not collapse to the raw device clock...
    EXPECT_GT(std::llabs(static_cast<long long>(DEVICE_NOW_NS) - nsp_ns), 1'000'000'000LL)
        << "NSP leaked the raw device/PTP clock after borrowing a stale peer "
           "offset; toLocalTime=" << nsp_ns << " device_ns=" << DEVICE_NOW_NS;
    // ...and must still agree with the HUB's correct mapping.
    EXPECT_LT(std::llabs(nsp_ns - hub_ref_ns), 2'000'000LL)
        << "NSP disagrees with HUB after a stale borrow: nsp=" << nsp_ns
        << " hub=" << hub_ref_ns;
}

// (B7c) GREEN — Two HUBs present (HUB1 + HUB2), both publishing good offsets.
// The NSP borrows from the first-priority HUB and agrees with both.  Confirms
// the multi-HUB borrow is sound when the chosen HUB is sane.
TEST(ClockSyncMultiDeviceTest, PeerBorrowMultiHubGoodAgrees) {
    const std::string run = uniqueSegName();

    auto hub1_r = makeNativeStandalone(run + "a");
    auto hub2_r = makeNativeStandalone(run + "b");
    ASSERT_TRUE(hub1_r.isOk()) << hub1_r.error();
    ASSERT_TRUE(hub2_r.isOk()) << hub2_r.error();

    // Both HUBs derive good offsets and publish them.
    ClockSync hub1_cs, hub2_cs;
    feedReliableProbes(hub1_cs);
    feedReliableProbes(hub2_cs);
    hub1_r.value().setClockSync(*hub1_cs.getOffsetNs(),
                               hub1_cs.getUncertaintyNs().value_or(0));
    hub2_r.value().setClockSync(*hub2_cs.getOffsetNs(),
                               hub2_cs.getUncertaintyNs().value_or(0));

    // NSP attaches CLIENT readers to both HUB segments (priority HUB1, HUB2).
    auto c1 = makeNativeClient(run + "a");
    auto c2 = makeNativeClient(run + "b");
    ASSERT_TRUE(c1.isOk()) << c1.error();
    ASSERT_TRUE(c2.isOk()) << c2.error();

    ClockSync nsp;
    feedDataFallback(nsp);
    borrowFromFirstOpenHub({&c1.value(), &c2.value()}, nsp);

    auto nsp_local = nsp.toLocalTime(DEVICE_NOW_NS);
    auto hub_local = hub1_cs.toLocalTime(DEVICE_NOW_NS);
    ASSERT_TRUE(nsp_local.has_value());
    ASSERT_TRUE(hub_local.has_value());
    const int64_t nsp_ns = tp_to_ns(*nsp_local);

    EXPECT_LT(std::llabs(nsp_ns - tp_to_ns(*hub_local)), 2'000'000LL)
        << "NSP disagrees with chosen HUB after a good multi-HUB borrow";
    EXPECT_GT(std::llabs(static_cast<long long>(DEVICE_NOW_NS) - nsp_ns),
              1'000'000'000LL)
        << "NSP leaked the raw device clock after a good multi-HUB borrow";
}

// (B7d) RED — Two HUBs present.  The first-priority HUB (HUB1) is momentarily
// stale (publishes 0), while HUB2 publishes a perfectly good offset.  Because
// the SDK borrows from the FIRST HUB that opens and never falls back to HUB2,
// the NSP latches HUB1's bad offset and leaks the raw device clock — even though
// a sane HUB2 offset was available the whole time.
//
// INTENDED invariant: the NSP must not adopt an implausible peer offset (it is
// inconsistent with its own data evidence by ~1.77e18 ns), and with a sane HUB2
// present it should end up agreeing with the good mapping rather than collapsing
// to the raw PTP clock.
//
// Root causes: no sanity bound on the borrowed offset
// (src/cbdev/src/clock_sync.cpp:191-198, 112-125) and the first-HUB-wins /
// borrow-once policy (src/cbsdk/src/sdk_session.cpp:1015-1029).  EXPECTED FAIL.
TEST(ClockSyncMultiDeviceTest, PeerBorrowFirstHubStaleSecondHubGood) {
    const std::string run = uniqueSegName();

    auto hub1_r = makeNativeStandalone(run + "a");  // priority 1, stale
    auto hub2_r = makeNativeStandalone(run + "b");  // priority 2, good
    ASSERT_TRUE(hub1_r.isOk()) << hub1_r.error();
    ASSERT_TRUE(hub2_r.isOk()) << hub2_r.error();

    // The good mapping that SHOULD be achievable (HUB2's correct offset).
    ClockSync hub2_cs;
    feedReliableProbes(hub2_cs);
    const int64_t good_ref_ns = tp_to_ns(*hub2_cs.toLocalTime(DEVICE_NOW_NS));

    // HUB1 stale (0), HUB2 good.
    hub1_r.value().setClockSync(0, 700'000);
    hub2_r.value().setClockSync(*hub2_cs.getOffsetNs(),
                               hub2_cs.getUncertaintyNs().value_or(0));

    auto c1 = makeNativeClient(run + "a");
    auto c2 = makeNativeClient(run + "b");
    ASSERT_TRUE(c1.isOk()) << c1.error();
    ASSERT_TRUE(c2.isOk()) << c2.error();

    ClockSync nsp;
    feedDataFallback(nsp);                       // NSP's own evidence ≈ TRUE_OFFSET
    borrowFromFirstOpenHub({&c1.value(), &c2.value()}, nsp);  // grabs HUB1 (stale)

    auto nsp_local = nsp.toLocalTime(DEVICE_NOW_NS);
    ASSERT_TRUE(nsp_local.has_value());
    const int64_t nsp_ns = tp_to_ns(*nsp_local);

    // INTENDED: NSP must not collapse to the raw device clock despite HUB1 stale.
    EXPECT_GT(std::llabs(static_cast<long long>(DEVICE_NOW_NS) - nsp_ns),
              1'000'000'000LL)
        << "NSP leaked the raw device clock after borrowing stale HUB1 "
           "(good HUB2 was available); toLocalTime=" << nsp_ns
        << " device_ns=" << DEVICE_NOW_NS;
    // INTENDED: with a sane HUB2 present, the NSP ends up near the good mapping.
    EXPECT_LT(std::llabs(nsp_ns - good_ref_ns), 2'000'000LL)
        << "NSP did not converge on the available good (HUB2) mapping: nsp="
        << nsp_ns << " good=" << good_ref_ns;
}
