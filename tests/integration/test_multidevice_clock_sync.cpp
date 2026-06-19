///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_multidevice_clock_sync.cpp
/// @author CereLink Development Team
///
/// @brief  Live-hardware multi-device clock-synchronization integration test
///
/// Exercises the FULL connected topology — a Gemini NSP plus HUB1 and HUB2
/// sharing one PTP device clock — and asserts the device(PTP)->host mapping
/// invariants that the shared-clock bug violates:
///   1. Each device's toLocalTime(getTime()) is close to steady_clock::now()
///      (no raw-PTP-clock leak).
///   2. The NSP specifically does not collapse to the raw device clock
///      (~1.77e18 ns) — the exact reported symptom of the peer-borrow latch.
///   3. All devices map a COMMON PTP instant to host times that agree within
///      1 ms (shared-clock domain).
///
/// The latch is INTERMITTENT and only fires when the NSP is on data-packet
/// fallback at the moment a stale/zero peer offset sits in a HUB segment.  To
/// provoke it this test:
///   * repeats many bring-up/observe/STANDBY iterations (cold starts via
///     STANDBY between iterations);
///   * SAMPLES the invariants repeatedly across the bring-up transition
///     (0.25 s .. 5 s), not just after sync settles — the latch can flash
///     during the fallback/borrow window and then recover;
///   * ROTATES bring-up order each iteration, including NSP-first, so the NSP
///     sometimes borrows before the HUBs have published a good offset.
/// During early samples a device that has not synced yet returns nullopt; that
/// is the SAFE fallback and is NOT a failure.  Only a NON-NULL implausible
/// mapping (the bug signature) is flagged.
///
/// HARDWARE-GATED / OPT-IN.  Bringing up an SdkSession runs a handshake that can
/// HARDRESET a device, which is disruptive on a recording rig, so this test is
/// SKIPPED unless CERELINK_HW_MULTIDEVICE is set to a non-empty value.  It must
/// never run in unattended CI.  Run manually with:
///
///     CERELINK_HW_MULTIDEVICE=1 [CERELINK_HW_MULTIDEVICE_ITERS=50] \
///       ./cbsdk_integration_tests --gtest_filter='integration.MultiDeviceClockSync*'
///
/// The deterministic, always-on counterpart (three ClockSyncs sharing one clock
/// + the first-HUB-wins peer borrow with two HUBs) lives in
/// tests/unit/test_clock_sync_multidevice.cpp and does not need hardware.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <cbproto/cbproto.h>
#include <cbsdk/sdk_session.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace cbsdk;

namespace {

/// True if the operator has explicitly opted in to driving live hardware.
bool hardwareTestEnabled() {
    const char* v = std::getenv("CERELINK_HW_MULTIDEVICE");
    return v != nullptr && v[0] != '\0';
}

/// Number of bring-up/observe/STANDBY iterations.  The shared-clock latch is
/// intermittent, so we repeat to give it a chance to fire.  Override with
/// CERELINK_HW_MULTIDEVICE_ITERS (default 10).
int iterationCount() {
    const char* v = std::getenv("CERELINK_HW_MULTIDEVICE_ITERS");
    if (v != nullptr && v[0] != '\0') {
        const int n = std::atoi(v);
        if (n > 0) return n;
    }
    return 10;
}

struct DeviceUnderTest {
    const char* name;
    DeviceType type;
};

/// Bring-up order for a given iteration.  Rotating the order — including
/// NSP-first — staggers when each NATIVE config segment appears, so the NSP
/// sometimes attempts its peer borrow before the HUBs have published a good
/// offset (maximizing the stale-peer window the latch needs).
std::vector<DeviceUnderTest> topologyForIter(int iter) {
    const DeviceUnderTest hub1{"HUB1", DeviceType::HUB1};
    const DeviceUnderTest hub2{"HUB2", DeviceType::HUB2};
    const DeviceUnderTest nsp {"NSP",  DeviceType::NSP};
    switch (iter % 3) {
        case 1:  return {nsp, hub1, hub2};   // NSP first — stale-peer window
        case 2:  return {hub1, nsp, hub2};   // interleaved
        default: return {hub1, hub2, nsp};   // HUBs first — baseline
    }
}

/// Sample offsets (seconds, from the last bring-up) at which we check the
/// invariants.  Early offsets catch a transient latch during the fallback/
/// borrow transition; the last offset requires every device to be synced.
const std::vector<double>& sampleOffsetsS() {
    // Extended out to 20 s so we can tell a SLOW-converging skew (eventually
    // drops below 1 ms) from a STUCK one (plateaus off-target indefinitely).
    static const std::vector<double> offsets = {
        0.5, 1.0, 2.0, 3.5, 5.0, 8.0, 11.0, 14.0, 17.0, 20.0};
    return offsets;
}

/// Cross-device 1 ms agreement is a steady-state requirement: only enforced at
/// or after this many seconds of settling.  Earlier samples are informational
/// (the no-raw-clock-leak check still applies at every sample).
constexpr double AGREEMENT_SETTLE_S = 8.0;

/// Check the clock-sync invariants once.  A device that has not synced yet
/// returns nullopt — the SAFE fallback, not a failure.  Only a NON-NULL
/// implausible mapping (raw-clock leak) or a >=1 ms cross-device disagreement
/// counts as a violation.  When require_ready is true (final sample) a device
/// still returning nullopt is itself recorded as a failure.
void sampleInvariants(std::vector<SdkSession>& sessions,
                      const std::vector<std::string>& names,
                      int iter, double t_s, bool require_ready,
                      bool require_agreement,
                      int& leak_hits, int& not_ready_hits) {
    SCOPED_TRACE("iter " + std::to_string(iter) + " @ " + std::to_string(t_s) + "s");

    const size_t n = sessions.size();
    std::vector<std::optional<std::chrono::steady_clock::time_point>> nearnow(n);

    // (1)+(2) Per-device: map latest device timestamp to host time near now().
    for (size_t i = 0; i < n; ++i) {
        const uint64_t device_ns = sessions[i].getTime();
        if (device_ns == 0u) {
            if (require_ready) {
                ++not_ready_hits;
                ADD_FAILURE() << names[i] << " [iter " << iter
                    << " @final]: device time still zero";
            }
            continue;
        }
        auto local = sessions[i].toLocalTime(device_ns);
        if (!local.has_value()) {
            if (require_ready) {
                ++not_ready_hits;
                ADD_FAILURE() << names[i] << " [iter " << iter
                    << " @final]: no clock offset yet (toLocalTime nullopt)";
            }
            continue;  // not-yet-synced is the safe fallback on early samples
        }
        nearnow[i] = local;
        const auto now = std::chrono::steady_clock::now();
        const double delta_s = std::chrono::duration_cast<std::chrono::duration<double>>(
            now - *local).count();
        // A raw-PTP-clock leak makes *local ~1.77e9 s from the steady epoch.
        const bool ok = (delta_s > -1.0) && (delta_s < 30.0);
        if (!ok) {
            ++leak_hits;
            std::printf("  [iter %d @%.2fs] %-4s LEAK near-now delta = %.6f s\n",
                        iter, t_s, names[i].c_str(), delta_s);
        }
        EXPECT_GT(delta_s, -1.0) << names[i] << " [iter " << iter << " @" << t_s
            << "s]: mapped time " << -delta_s << "s in the FUTURE (raw-clock leak?)";
        EXPECT_LT(delta_s, 30.0) << names[i] << " [iter " << iter << " @" << t_s
            << "s]: mapped time " << delta_s << "s in the past (raw-clock leak/stale offset?)";
    }

    // (3) Cross-device agreement on a COMMON PTP instant (shared device clock).
    // Use the first non-zero device timestamp as the common instant, and only
    // compare devices that currently have an offset.
    uint64_t common_device_ns = 0;
    for (size_t i = 0; i < n; ++i) {
        const uint64_t t = sessions[i].getTime();
        if (t != 0u) { common_device_ns = t; break; }
    }
    if (common_device_ns == 0u) return;  // nothing synced yet

    std::vector<std::optional<std::chrono::steady_clock::time_point>> shared(n);
    for (size_t i = 0; i < n; ++i) {
        shared[i] = sessions[i].toLocalTime(common_device_ns);
    }
    double max_abs_ms = 0.0;
    int compared = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!shared[i]) continue;
        for (size_t j = i + 1; j < n; ++j) {
            if (!shared[j]) continue;
            ++compared;
            const double disagree_ms = std::chrono::duration_cast<
                std::chrono::duration<double, std::milli>>(
                    *shared[i] - *shared[j]).count();
            if (std::abs(disagree_ms) > max_abs_ms) max_abs_ms = std::abs(disagree_ms);
            // Devices sharing one PTP clock must converge to <1 ms agreement.
            // Early in a cold start the offset is still settling, so only assert
            // the bound once a device has had time to converge; earlier samples
            // are logged for the convergence curve but not asserted.
            if (require_agreement) {
                if (std::abs(disagree_ms) >= 1.0) {
                    ++leak_hits;
                    std::printf("  [iter %d @%.2fs] %-4s vs %-4s DISAGREE = %.6f ms\n",
                                iter, t_s, names[i].c_str(), names[j].c_str(), disagree_ms);
                }
                EXPECT_LT(std::abs(disagree_ms), 1.0)
                    << names[i] << " and " << names[j] << " [iter " << iter << " @"
                    << t_s << "s] map the shared PTP instant " << disagree_ms << " ms apart";
            }
        }
    }
    // Full convergence curve: one line per sample regardless of threshold, so a
    // drop below 1 ms at a later offset is visible as recovery.
    if (compared > 0) {
        std::printf("  [iter %d @%.2fs] maxdisagree = %.4f ms\n",
                    iter, t_s, max_abs_ms);
    }
}

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////////////////////////
// Multi-device clock sync over the live NSP + HUB1 + HUB2 rig
///////////////////////////////////////////////////////////////////////////////////////////////////

// If the shared-clock latch fires, the NSP's mapping collapses to the raw PTP
// clock and/or it disagrees with the HUBs by ~1.77e9 s, and this test FAILS,
// pinpointing the live regression.  On healthy firmware/SDK it passes.
TEST(MultiDeviceClockSyncTest, AllConnectedDevicesMapNearHostAndAgree) {
    if (!hardwareTestEnabled()) {
        GTEST_SKIP() << "Set CERELINK_HW_MULTIDEVICE=1 to run against the live "
                        "NSP + HUB1 + HUB2 rig (handshake may HARDRESET a device).";
    }

    const int iters = iterationCount();
    int leak_hits = 0;        // raw-clock leaks / cross-device disagreements seen
    int not_ready_hits = 0;   // devices still unsynced at the final sample
    int iters_run = 0;        // iterations that brought every device up

    for (int it = 0; it < iters; ++it) {
        const auto order = topologyForIter(it);
        std::printf("=== iteration %d/%d  (order: %s, %s, %s) ===\n",
                    it, iters - 1, order[0].name, order[1].name, order[2].name);
        std::fflush(stdout);

        // Per-iteration bring-up in this iteration's order.  Sessions are scoped
        // to the loop body so each iteration starts from a fresh attach.
        std::vector<SdkSession> sessions;
        std::vector<std::string> names;
        sessions.reserve(order.size());
        bool all_up = true;
        for (const auto& d : order) {
            SdkConfig config;
            config.device_type = d.type;
            auto result = SdkSession::create(config);
            if (result.isError()) {
                ADD_FAILURE() << "[iter " << it << "] failed to bring up "
                    << d.name << ": " << result.error();
                all_up = false;
                break;
            }
            sessions.push_back(std::move(result.value()));
            names.emplace_back(d.name);
        }

        if (all_up) {
            ++iters_run;
            // Sample the invariants repeatedly across the bring-up transition.
            double prev = 0.0;
            for (size_t s = 0; s < sampleOffsetsS().size(); ++s) {
                const double t = sampleOffsetsS()[s];
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(t - prev));
                prev = t;
                const bool require_ready = (s + 1 == sampleOffsetsS().size());
                const bool require_agreement = (t >= AGREEMENT_SETTLE_S);
                sampleInvariants(sessions, names, it, t, require_ready,
                                 require_agreement, leak_hits, not_ready_hits);
            }
        }

        // End the iteration by putting every brought-up device into STANDBY, so
        // the next iteration pays the full handshake/startup latency — the
        // window in which the NSP falls to data fallback and (mis)borrows a peer
        // HUB offset.  Best-effort: a STANDBY failure shouldn't abort the run.
        for (size_t i = 0; i < sessions.size(); ++i) {
            auto r = sessions[i].setSystemRunLevel(cbRUNLEVEL_STANDBY);
            if (r.isError()) {
                std::printf("  [iter %d] %-4s STANDBY failed: %s\n",
                            it, names[i].c_str(), r.error().c_str());
            }
        }
        // sessions/names destroyed here, detaching before the next bring-up.
    }

    std::printf("=== multi-device clock sync: %d/%d iterations brought up all "
                "devices; %d leak/disagreement hit(s); %d not-ready hit(s) ===\n",
                iters_run, iters, leak_hits, not_ready_hits);
    std::fflush(stdout);

    EXPECT_GT(iters_run, 0) << "no iteration managed to bring up all three devices";
}
