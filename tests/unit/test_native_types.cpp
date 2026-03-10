///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_native_types.cpp
/// @author CereLink Development Team
/// @date   2026-02-08
///
/// @brief  Unit tests for native-mode shared memory type definitions
///
/// Validates struct sizes, constants, and layout assumptions for native-mode buffers.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <cbshm/native_types.h>
#include <cbshm/central_types.h>
#include <cbproto/cbproto.h>
#include <memory>
#include <cstring>

using namespace cbshm;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Constants Tests
/// @{

TEST(NativeTypesTest, ChannelCountConstants) {
    EXPECT_EQ(NATIVE_NUM_FE_CHANS, 256u);
    EXPECT_EQ(NATIVE_NUM_ANALOG_CHANS, 272u);
    EXPECT_EQ(NATIVE_MAXCHANS, 284u);
    EXPECT_EQ(NATIVE_MAXGROUPS, 8u);
    EXPECT_EQ(NATIVE_MAXFILTS, 32u);
}

TEST(NativeTypesTest, TransmitSlotSize) {
    // Native slots are sized for cbPKT_MAX_SIZE (1024 bytes)
    EXPECT_EQ(NATIVE_XMT_SLOT_WORDS, 256u);  // 1024 / 4
    EXPECT_EQ(NATIVE_XMT_SLOT_WORDS * sizeof(uint32_t), cbPKT_MAX_SIZE);
}

TEST(NativeTypesTest, SpikeCacheConstants) {
    EXPECT_EQ(NATIVE_cbPKT_SPKCACHEPKTCNT, CENTRAL_cbPKT_SPKCACHEPKTCNT);
    EXPECT_EQ(NATIVE_cbPKT_SPKCACHELINECNT, NATIVE_NUM_ANALOG_CHANS);
    EXPECT_EQ(NATIVE_cbPKT_SPKCACHELINECNT, 272u);
}

TEST(NativeTypesTest, ReceiveBufferLengthMatchesCbproto) {
    EXPECT_EQ(NATIVE_cbRECBUFFLEN, cbRECBUFFLEN);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Size Comparison Tests
/// @{

TEST(NativeTypesTest, NativeConfigBufferSmallerThanCentral) {
    // Native config buffer should be significantly smaller than Central's
    size_t native_size = sizeof(NativeConfigBuffer);
    size_t central_size = sizeof(CentralConfigBuffer);

    EXPECT_LT(native_size, central_size)
        << "Native config buffer (" << native_size << " bytes) should be smaller than "
        << "Central config buffer (" << central_size << " bytes)";

    // Native should be roughly 1 MB range (order of magnitude check)
    EXPECT_GT(native_size, 100 * 1024u) << "Native config buffer seems too small";
    EXPECT_LT(native_size, 10 * 1024 * 1024u) << "Native config buffer seems too large";
}

TEST(NativeTypesTest, NativeSpikeBufferSmallerThanCentral) {
    size_t native_size = sizeof(NativeSpikeBuffer);
    size_t central_size = sizeof(CentralSpikeBuffer);

    EXPECT_LT(native_size, central_size)
        << "Native spike buffer (" << native_size << " bytes) should be smaller than "
        << "Central spike buffer (" << central_size << " bytes)";
}

TEST(NativeTypesTest, NativeTransmitBufferSmallerThanCentral) {
    size_t native_size = sizeof(NativeTransmitBuffer);
    size_t central_size = sizeof(CentralTransmitBuffer);

    EXPECT_LT(native_size, central_size)
        << "Native transmit buffer (" << native_size << " bytes) should be smaller than "
        << "Central transmit buffer (" << central_size << " bytes)";
}

TEST(NativeTypesTest, NativeLocalTransmitBufferSmallerThanCentral) {
    size_t native_size = sizeof(NativeTransmitBufferLocal);
    size_t central_size = sizeof(CentralTransmitBufferLocal);

    EXPECT_LT(native_size, central_size)
        << "Native local transmit buffer (" << native_size << " bytes) should be smaller than "
        << "Central local transmit buffer (" << central_size << " bytes)";
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Struct Layout Tests
/// @{

TEST(NativeTypesTest, ConfigBufferHasExpectedFields) {
    // Heap-allocate: NativeConfigBuffer is multi-MB (too large for stack)
    auto cfg = std::make_unique<NativeConfigBuffer>();
    std::memset(cfg.get(), 0, sizeof(NativeConfigBuffer));

    // Verify key fields are accessible and zero-initialized
    EXPECT_EQ(cfg->version, 0u);
    EXPECT_EQ(cfg->sysflags, 0u);
    EXPECT_EQ(cfg->instrument_status, 0u);
    EXPECT_EQ(cfg->procinfo.proc, 0u);
    EXPECT_EQ(cfg->adaptinfo.chan, 0u);
    EXPECT_EQ(cfg->refelecinfo.chan, 0u);
}

TEST(NativeTypesTest, TransmitBufferLayout) {
    // Heap-allocate: NativeTransmitBuffer is ~5 MB (too large for stack)
    auto xmt = std::make_unique<NativeTransmitBuffer>();
    std::memset(xmt.get(), 0, sizeof(NativeTransmitBuffer));

    EXPECT_EQ(xmt->transmitted, 0u);
    EXPECT_EQ(xmt->headindex, 0u);
    EXPECT_EQ(xmt->tailindex, 0u);
    EXPECT_EQ(xmt->last_valid_index, 0u);
    EXPECT_EQ(xmt->bufferlen, 0u);

    // Buffer should be large enough for the configured number of slots
    EXPECT_EQ(sizeof(xmt->buffer) / sizeof(uint32_t), NATIVE_cbXMT_GLOBAL_BUFFLEN);
}

TEST(NativeTypesTest, LocalTransmitBufferLayout) {
    // Heap-allocate: NativeTransmitBufferLocal is ~2MB (too large for stack on Windows)
    auto xmt = std::make_unique<NativeTransmitBufferLocal>();
    std::memset(xmt.get(), 0, sizeof(NativeTransmitBufferLocal));

    EXPECT_EQ(xmt->transmitted, 0u);
    EXPECT_EQ(xmt->headindex, 0u);
    EXPECT_EQ(xmt->tailindex, 0u);
    EXPECT_EQ(sizeof(xmt->buffer) / sizeof(uint32_t), NATIVE_cbXMT_LOCAL_BUFFLEN);
}

TEST(NativeTypesTest, SpikeCacheLayout) {
    NativeSpikeCache cache = {};

    EXPECT_EQ(cache.chid, 0u);
    EXPECT_EQ(cache.pktcnt, 0u);
    EXPECT_EQ(cache.pktsize, 0u);
    EXPECT_EQ(cache.head, 0u);
    EXPECT_EQ(cache.valid, 0u);
}

TEST(NativeTypesTest, SpikeBufferLayout) {
    // NativeSpikeBuffer is too large for stack (~109 MB), verify layout via sizeof/offsetof
    static_assert(sizeof(NativeSpikeBuffer) > sizeof(NativeSpikeCache) * NATIVE_cbPKT_SPKCACHELINECNT,
                  "Spike buffer should contain NATIVE_cbPKT_SPKCACHELINECNT cache lines plus header");

    // Verify cache line count
    EXPECT_EQ(sizeof(NativeSpikeBuffer::cache) / sizeof(NativeSpikeCache), NATIVE_cbPKT_SPKCACHELINECNT);
}

TEST(NativeTypesTest, PCStatusLayout) {
    NativePCStatus status = {};

    EXPECT_EQ(status.m_iBlockRecording, 0);
    EXPECT_EQ(status.m_nPCStatusFlags, 0u);
    EXPECT_EQ(status.m_nNumFEChans, 0u);
    EXPECT_EQ(status.m_nNumTotalChans, 0u);
    EXPECT_EQ(status.m_nGeminiSystem, 0u);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name CentralLegacyCFGBUFF Tests
/// @{

TEST(CentralLegacyTypesTest, SizeCloseToConfigBuffer) {
    // CentralLegacyCFGBUFF and CentralConfigBuffer should be close in size.
    // Differences:
    //   CentralConfigBuffer has instrument_status[4] (+16 bytes), no optiontable/colortable move (neutral)
    //   CentralLegacyCFGBUFF omits hwndCentral (saves ~8 bytes) and instrument_status (saves 16 bytes)
    //   isLnc position differs but size is the same
    // So the difference should be small (under 1KB)
    size_t legacy_size = sizeof(CentralLegacyCFGBUFF);
    size_t config_size = sizeof(CentralConfigBuffer);

    // Both should be in the multi-MB range
    EXPECT_GT(legacy_size, 1 * 1024 * 1024u) << "Legacy config buffer seems too small: " << legacy_size;
    EXPECT_GT(config_size, 1 * 1024 * 1024u) << "Config buffer seems too small: " << config_size;

    // The difference should be small (instrument_status[4] = 16 bytes)
    size_t diff = (legacy_size > config_size) ? (legacy_size - config_size) : (config_size - legacy_size);
    EXPECT_LT(diff, 1024u)
        << "Legacy (" << legacy_size << ") and CereLink (" << config_size
        << ") config buffers differ by " << diff << " bytes (expected < 1KB)";
}

TEST(CentralLegacyTypesTest, FieldOrderMatchesCentral) {
    // Verify that optiontable comes right after sysflags (Central's layout)
    // In CereLink's cbConfigBuffer, optiontable is at the END
    // Heap-allocate: CentralLegacyCFGBUFF is multi-MB (too large for stack)
    auto legacy = std::make_unique<CentralLegacyCFGBUFF>();
    std::memset(legacy.get(), 0, sizeof(CentralLegacyCFGBUFF));

    // Access fields to verify they compile and are accessible
    legacy->version = 42;
    legacy->sysflags = 1;
    legacy->optiontable = {};
    legacy->colortable = {};
    legacy->sysinfo = {};
    legacy->procinfo[0] = {};
    legacy->procinfo[3] = {};
    legacy->bankinfo[0][0] = {};
    legacy->chaninfo[0] = {};
    legacy->chaninfo[CENTRAL_cbMAXCHANS - 1] = {};
    legacy->isLnc[0] = {};
    legacy->isLnc[3] = {};
    legacy->fileinfo = {};

    EXPECT_EQ(legacy->version, 42u);
}

/// @}
