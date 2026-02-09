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
    NativeConfigBuffer cfg = {};

    // Verify key fields are accessible and zero-initialized
    EXPECT_EQ(cfg.version, 0u);
    EXPECT_EQ(cfg.sysflags, 0u);
    EXPECT_EQ(cfg.instrument_status, 0u);
    EXPECT_EQ(cfg.procinfo.proc, 0u);
    EXPECT_EQ(cfg.adaptinfo.chan, 0u);
    EXPECT_EQ(cfg.refelecinfo.chan, 0u);
}

TEST(NativeTypesTest, TransmitBufferLayout) {
    NativeTransmitBuffer xmt = {};

    EXPECT_EQ(xmt.transmitted, 0u);
    EXPECT_EQ(xmt.headindex, 0u);
    EXPECT_EQ(xmt.tailindex, 0u);
    EXPECT_EQ(xmt.last_valid_index, 0u);
    EXPECT_EQ(xmt.bufferlen, 0u);

    // Buffer should be large enough for the configured number of slots
    EXPECT_EQ(sizeof(xmt.buffer) / sizeof(uint32_t), NATIVE_cbXMT_GLOBAL_BUFFLEN);
}

TEST(NativeTypesTest, LocalTransmitBufferLayout) {
    NativeTransmitBufferLocal xmt = {};

    EXPECT_EQ(xmt.transmitted, 0u);
    EXPECT_EQ(xmt.headindex, 0u);
    EXPECT_EQ(xmt.tailindex, 0u);
    EXPECT_EQ(sizeof(xmt.buffer) / sizeof(uint32_t), NATIVE_cbXMT_LOCAL_BUFFLEN);
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
