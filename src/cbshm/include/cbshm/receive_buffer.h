///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   receive_buffer.h
/// @author CereLink Development Team
/// @date   2025-11-15
///
/// @brief  Receive buffer structure for incoming packets
///
/// Central-compatible receive buffer that can be shared between cbdev and cbshm.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBPROTO_RECEIVE_BUFFER_H
#define CBPROTO_RECEIVE_BUFFER_H

#include <cbproto/cbproto.h>
#include <cstdint>

// Ensure tight packing for shared memory structures
#pragma pack(push, 1)

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Receive Buffer Constants
/// @{

/// Maximum number of frontend channels (used for buffer size calculation)
#ifndef cbNUM_FE_CHANS
#define cbNUM_FE_CHANS 256
#endif

/// Receive buffer length (matches Central's calculation)
/// Formula: cbNUM_FE_CHANS * 65536 * 4 - 1
/// This provides enough space for a ring buffer of packets from all FE channels
constexpr uint32_t cbRECBUFFLEN = cbNUM_FE_CHANS * 65536 * 4 - 1;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Receive buffer for incoming packets
///
/// Ring buffer that stores raw packet data received from the device.
/// Matches Central's cbRECBUFF structure exactly.
///
/// The buffer stores packets as uint32_t words, and the headindex tracks where
/// new data should be written. Packets are modified in-place (instrument ID, proc, bank)
/// before being stored.
///
struct cbReceiveBuffer {
    uint32_t received;                  ///< Number of packets received
    PROCTIME lasttime;                  ///< Last timestamp seen
    uint32_t headwrap;                  ///< Head wrap counter (for detecting buffer wraps)
    uint32_t headindex;                 ///< Current head index (write position)
    uint32_t buffer[cbRECBUFFLEN];      ///< Ring buffer for packet data
};

#pragma pack(pop)

#endif // CBPROTO_RECEIVE_BUFFER_H
