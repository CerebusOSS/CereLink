///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_central_adapters.cpp
/// @author CereLink Development Team
///
/// @brief  Unit tests for the per-version Central shared-memory adapters
///         (central_v7_0 .. central_v7_8).
///
/// These adapters translate between each Central version's binary struct layout
/// and the native cbproto types. They are the heart of the multi-version-compat
/// feature and are fully unit-testable off-Windows: the Adapter constructor takes
/// raw pointers to caller-owned buffers (no shared memory, no Central.exe), and
/// the private fromLegacy/toLegacy overloads are exercised transitively through
/// the public get/set methods.
///
/// Structure:
///   - AdapterRoundTrip  : ONE typed body run against all five versions. Verifies
///                         the universal invariant "what you set is what you get"
///                         (translation round-trips through the version's legacy
///                         layout without loss).
///   - Divergence tests  : targeted, named tests for behavior that genuinely
///                         differs by version/protocol group (bit-packed vs split
///                         CHANINFO/AOUT fields, PROCINFO sortmethod aliasing).
///
/// NOTE (scaffold): the round-trip assertions target fields that are expected to
/// survive on every version. Some index ranges (bank/filter/group/channel) and a
/// few divergence checks are marked TODO where they require inspecting the raw
/// version-specific legacy struct — fill those in against central_types/vX_Y.h.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <cbshm/central_adapters/v7_0.h>
#include <cbshm/central_adapters/v7_5.h>
#include <cbshm/central_adapters/v7_6.h>
#include <cbshm/central_adapters/v7_7.h>
#include <cbshm/central_adapters/v7_8.h>
#include <cbshm/native_types.h>
#include <cbproto/types.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

using namespace cbshm;

namespace {

/// Binds a version's BootstrapAdapter + Adapter together for typed tests.
template <class Boot, class Adpt>
struct VersionTraits {
    using Bootstrap = Boot;
    using Adapter   = Adpt;
};

using V7_0 = VersionTraits<central_v7_0::BootstrapAdapter, central_v7_0::Adapter>;
using V7_5 = VersionTraits<central_v7_5::BootstrapAdapter, central_v7_5::Adapter>;
using V7_6 = VersionTraits<central_v7_6::BootstrapAdapter, central_v7_6::Adapter>;
using V7_7 = VersionTraits<central_v7_7::BootstrapAdapter, central_v7_7::Adapter>;
using V7_8 = VersionTraits<central_v7_8::BootstrapAdapter, central_v7_8::Adapter>;

/// All five versions — for invariants that must hold everywhere.
using AllVersions = ::testing::Types<V7_0, V7_5, V7_6, V7_7, V7_8>;

/// Stub size for the receive/transmit ring buffers. None of the adapter
/// *translation* tests dereference these buffers — the Adapter constructor only
/// stores the pointers (verified in the ctor), and no test here calls the
/// getRec*/getXmt*/enqueue/dequeue paths. Allocating Central's full rings
/// (~805 MB receive + ~290 MB transmit for v7_8) in every SetUp would zero-fill
/// >1 GB per test, blowing up wall time and risking OOM on CI. A small valid
/// allocation is all the pointers need. Ring-buffer behavior belongs in a
/// separate ShmemSession-level test, not here.
static constexpr size_t kStubRingSize = 4096;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Owns the Central buffers and constructs the Adapter over them.
///
/// The config and status buffers are allocated at their real (version-reported)
/// size because the tests read translated data out of them. The receive/transmit
/// rings are stubbed (see kStubRingSize). The spike buffer is stubbed by default
/// and only allocated full-size when a fixture overrides needsFullSpikeBuffer()
/// (i.e. when a test actually reads the spike cache).
///
template <class T>
class AdapterFixture : public ::testing::Test {
protected:
    typename T::Bootstrap boot;
    std::vector<uint8_t> cfg, rec, xmt, xmt_local, status, spike;
    std::unique_ptr<typename T::Adapter> adapter;

    static constexpr uint8_t kInstrument = 0;

    /// Override to true in fixtures whose tests dereference the spike buffer.
    virtual bool needsFullSpikeBuffer() const { return false; }

    void SetUp() override {
        cfg.assign(boot.getConfigBufferSize(), 0);
        status.assign(boot.getStatusBufferSize(), 0);
        rec.assign(kStubRingSize, 0);
        xmt.assign(kStubRingSize, 0);
        xmt_local.assign(kStubRingSize, 0);
        spike.assign(needsFullSpikeBuffer() ? boot.getSpikeBufferSize() : kStubRingSize, 0);

        adapter = std::make_unique<typename T::Adapter>(CentralAdapterArgs{
            kInstrument, cfg.data(), rec.data(), xmt.data(),
            xmt_local.data(), status.data(), spike.data() });
    }
};

} // namespace

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name AdapterRoundTrip — universal set/get invariants (all versions)
///
/// LIMITATION: these are inverse-pair tests. set*() writes via toLegacy, get*()
/// reads via fromLegacy, so they verify the two translations are mutual inverses.
/// That catches dropped fields, truncation, and *asymmetric* mistakes (e.g. a
/// moninst/monchan swap in only one direction). It does NOT catch a *symmetric*
/// bug where both directions agree on a layout that does not match Central's real
/// bytes. Proving the on-wire layout requires seeding a raw version-specific
/// legacy struct and reading it back — see the scaffolded TODOs at the bottom of
/// this file.
/// @{

TYPED_TEST_SUITE(AdapterFixture, AllVersions);

TYPED_TEST(AdapterFixture, MaxProcsIsSane) {
    EXPECT_GE(this->boot.getMaxProcs(), 1u);
}

TYPED_TEST(AdapterFixture, BootstrapSizesArePositive) {
    // Every buffer must have a non-zero size or the adapter would map nothing.
    EXPECT_GT(this->boot.getConfigBufferSize(), 0u);
    EXPECT_GT(this->boot.getReceiveBufferSize(), 0u);
    EXPECT_GT(this->boot.getTransmitBufferSize(), 0u);
    EXPECT_GT(this->boot.getTransmitBufferLocalSize(), 0u);
    EXPECT_GT(this->boot.getStatusBufferSize(), 0u);
    EXPECT_GT(this->boot.getSpikeBufferSize(), 0u);
    EXPECT_GT(this->boot.getReceiveBufferLen(), 0u);
    EXPECT_GT(this->boot.getTransmitBufferLen(), 0u);
    EXPECT_GT(this->boot.getTransmitBufferLocalLen(), 0u);
}

TYPED_TEST(AdapterFixture, ProcInfoRoundTrip) {
    cbPKT_PROCINFO in;
    std::memset(&in, 0, sizeof(in));
    in.proc = 1;
    in.chancount = 256;
    in.bankcount = 8;
    // `reserved` is the field that actually diverges by version: on v7_0 it is
    // aliased to the legacy `sortmethod` field, on v7_5+ it maps straight across.
    // Exercise it explicitly so the round-trip touches the version-specific path
    // rather than only the direct-copy scalars.
    in.reserved = 0xABCDu;

    ASSERT_TRUE(this->adapter->setProcInfo(in).isOk());

    cbPKT_PROCINFO out;
    std::memset(&out, 0, sizeof(out));
    ASSERT_TRUE(this->adapter->getProcInfo(out).isOk());

    EXPECT_EQ(out.proc, in.proc);
    EXPECT_EQ(out.chancount, in.chancount);
    EXPECT_EQ(out.bankcount, in.bankcount);
    EXPECT_EQ(out.reserved, in.reserved);
}

TYPED_TEST(AdapterFixture, BankInfoRoundTrip) {
    const uint32_t bank = 1;  // 1-based
    cbPKT_BANKINFO in;
    std::memset(&in, 0, sizeof(in));
    in.proc = 1;
    in.bank = bank;
    in.chancount = 32;

    ASSERT_TRUE(this->adapter->setBankInfo(bank, in).isOk());

    cbPKT_BANKINFO out;
    std::memset(&out, 0, sizeof(out));
    ASSERT_TRUE(this->adapter->getBankInfo(out, bank).isOk());

    EXPECT_EQ(out.proc, in.proc);
    EXPECT_EQ(out.bank, in.bank);
    EXPECT_EQ(out.chancount, in.chancount);
}

TYPED_TEST(AdapterFixture, FilterInfoRoundTrip) {
    const uint32_t filt = 1;  // 1-based
    cbPKT_FILTINFO in;
    std::memset(&in, 0, sizeof(in));
    in.proc = 1;
    in.filt = filt;
    in.hpfreq = 250000;

    ASSERT_TRUE(this->adapter->setFilterInfo(filt, in).isOk());

    cbPKT_FILTINFO out;
    std::memset(&out, 0, sizeof(out));
    ASSERT_TRUE(this->adapter->getFilterInfo(out, filt).isOk());

    EXPECT_EQ(out.filt, in.filt);
    EXPECT_EQ(out.hpfreq, in.hpfreq);
}

TYPED_TEST(AdapterFixture, ChanInfoRoundTrip) {
    const uint32_t chan_idx = 0;  // 0-based index into the buffer
    cbPKT_CHANINFO in;
    std::memset(&in, 0, sizeof(in));
    in.chan = 1;
    in.proc = 1;
    in.bank = 1;
    std::strncpy(in.label, "elec001", cbLEN_STR_LABEL);

    ASSERT_TRUE(this->adapter->setChanInfo(chan_idx, in).isOk());

    cbPKT_CHANINFO out;
    std::memset(&out, 0, sizeof(out));
    ASSERT_TRUE(this->adapter->getChanInfo(out, chan_idx).isOk());

    EXPECT_EQ(out.chan, in.chan);
    EXPECT_EQ(out.proc, in.proc);
    EXPECT_EQ(out.bank, in.bank);
    EXPECT_STREQ(out.label, in.label);
}

TYPED_TEST(AdapterFixture, SysInfoRoundTrip) {
    cbPKT_SYSINFO in;
    std::memset(&in, 0, sizeof(in));
    in.sysfreq = 30000;

    ASSERT_TRUE(this->adapter->setSysInfo(in).isOk());

    cbPKT_SYSINFO out;
    std::memset(&out, 0, sizeof(out));
    ASSERT_TRUE(this->adapter->getSysInfo(out).isOk());

    EXPECT_EQ(out.sysfreq, in.sysfreq);
}

TYPED_TEST(AdapterFixture, GroupInfoRoundTrip) {
    const uint32_t group = 0;  // 0-based
    cbPKT_GROUPINFO in;
    std::memset(&in, 0, sizeof(in));
    in.proc = 1;
    in.group = 1;

    ASSERT_TRUE(this->adapter->setGroupInfo(group, in).isOk());

    cbPKT_GROUPINFO out;
    std::memset(&out, 0, sizeof(out));
    ASSERT_TRUE(this->adapter->getGroupInfo(out, group).isOk());

    EXPECT_EQ(out.proc, in.proc);
    EXPECT_EQ(out.group, in.group);
}

TYPED_TEST(AdapterFixture, GetConfigBufferReflectsProcInfo) {
    // Write a distinctive value, then verify the full-buffer translation carries
    // it through — not just that the call returns ok.
    cbPKT_PROCINFO in;
    std::memset(&in, 0, sizeof(in));
    in.proc = 7;
    in.chancount = 128;
    ASSERT_TRUE(this->adapter->setProcInfo(in).isOk());

    // Large struct — heap-allocate.
    auto buf = std::make_unique<NativeConfigBuffer>();
    ASSERT_TRUE(this->adapter->getConfigBuffer(*buf).isOk());

    EXPECT_EQ(buf->procinfo.proc, 7u);
    EXPECT_EQ(buf->procinfo.chancount, 128u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Spike cache read (needs a full-size spike buffer)
/// @{

// getSpikeCache dereferences the spike buffer, so this fixture opts into the
// full-size allocation (all other fixtures stub it — see kStubRingSize).
template <class T>
class SpikeAdapterFixture : public AdapterFixture<T> {
protected:
    bool needsFullSpikeBuffer() const override { return true; }
};
TYPED_TEST_SUITE(SpikeAdapterFixture, AllVersions);

TYPED_TEST(SpikeAdapterFixture, GetSpikeCacheReadsEmptyCache) {
    NativeSpikeCache cache;
    std::memset(&cache, 0xFF, sizeof(cache));  // poison, so a real copy is observable

    ASSERT_TRUE(this->adapter->getSpikeCache(cache, 0).isOk());

    // The zeroed spike buffer translates to an empty cache line; confirm the
    // fields were actually populated (not left poisoned).
    EXPECT_EQ(cache.valid, 0u);
    EXPECT_EQ(cache.chid, 0u);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Divergence — bit-packed vs split fields (CHANINFO monsource, AOUT trig)
///
/// v7_0/v7_5 store the moninst/monchan union arm as a single 32-bit monsource
/// whose interpretation is mode-dependent (see the pre-4.1 cbSetDoutOptions):
/// frequency output packs sample counts byte-compatibly with the modern
/// lowsamples/highsamples arm, while every other mode stores a plain channel
/// number (modern monchan, with moninst having no pre-4.1 representation).
/// v7_6+ store the fields separately. A set→get round-trip is symmetric on
/// every version (below), so it does NOT by itself distinguish packed from
/// split storage. To prove the on-wire layout, inspect the raw legacy struct
/// in the cfg buffer.
/// @{

TYPED_TEST(AdapterFixture, ChanInfoMonitorChannelRoundTrips) {
    // Monitoring mode (no cbDOUT_FREQUENCY): the monitored channel must
    // survive every version.  moninst is deliberately 0 — pre-4.1 layouts
    // cannot represent a non-zero monitor instrument.
    const uint32_t chan_idx = 0;
    cbPKT_CHANINFO in;
    std::memset(&in, 0, sizeof(in));
    in.chan = 1;
    in.moninst = 0;
    in.monchan = 5;

    ASSERT_TRUE(this->adapter->setChanInfo(chan_idx, in).isOk());

    cbPKT_CHANINFO out;
    std::memset(&out, 0, sizeof(out));
    ASSERT_TRUE(this->adapter->getChanInfo(out, chan_idx).isOk());

    EXPECT_EQ(out.moninst, in.moninst);
    EXPECT_EQ(out.monchan, in.monchan);
}

TYPED_TEST(AdapterFixture, ChanInfoFrequencyModeSamplesRoundTrip) {
    // Frequency-output mode (cbDOUT_FREQUENCY): the union carries the
    // lowsamples/highsamples pair, which is byte-preserved on packed
    // (v7_0/v7_5) and split (v7_6+) layouts alike.
    const uint32_t chan_idx = 0;
    cbPKT_CHANINFO in;
    std::memset(&in, 0, sizeof(in));
    in.chan = 1;
    in.doutopts = cbDOUT_FREQUENCY;
    in.lowsamples = 0x1234;
    in.highsamples = 0x5678;

    ASSERT_TRUE(this->adapter->setChanInfo(chan_idx, in).isOk());

    cbPKT_CHANINFO out;
    std::memset(&out, 0, sizeof(out));
    ASSERT_TRUE(this->adapter->getChanInfo(out, chan_idx).isOk());

    EXPECT_EQ(out.doutopts, in.doutopts);
    EXPECT_EQ(out.lowsamples, in.lowsamples);
    EXPECT_EQ(out.highsamples, in.highsamples);
}

// TODO(scaffold): raw-layout divergence check. Cast the cfg buffer to the
// version's legacy cbCFGBUFF (from cbshm/central_types/vX_Y.h) and assert:
//   - v7_0/v7_5: cfg->chaninfo[0].monsource == ((moninst << 16) | monchan)
//   - v7_6+    : cfg->chaninfo[0].moninst / .monchan are separate fields
// This is the only way to verify storage format rather than round-trip symmetry.
//
// TEST(AdapterV7_0Divergence, ChanInfoPacksMonSource) { ... }
// TEST(AdapterV7_6Divergence, ChanInfoSplitsMonSource) { ... }

// TODO(scaffold): AOUT_WAVEFORM trig/trigInst packing. There is no public
// getWaveform accessor on the adapter, so this divergence is only reachable by
// inspecting the raw legacy buffer after a config-buffer translation, or via the
// ShmemSession receive/transmit path. Decide the seam before implementing.

// TODO(scaffold): PROCINFO sortmethod aliasing (v7_0 only). v7_0 maps
// cur.reserved <-> leg.sortmethod. Verify by setting cbPKT_PROCINFO.reserved,
// round-tripping, and (for proof of aliasing) inspecting the raw legacy
// procinfo.sortmethod in the cfg buffer.

/// @}
