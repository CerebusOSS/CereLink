///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   types.h
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Core protocol types and constants
///
/// This file contains the fundamental types and constants that define the Cerebus protocol.
/// These MUST match Blackrock's cbproto.h exactly to ensure compatibility with Central.
///
/// DO NOT MODIFY unless updating from upstream protocol changes.
///
/// Reference: cbproto.h (Protocol Version 4.2)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBPROTO_TYPES_H
#define CBPROTO_TYPES_H

#include <stdint.h>

// Ensure tight packing for network protocol structures
#pragma pack(push, 1)

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Protocol Version
/// @{

#define cbVERSION_MAJOR  4
#define cbVERSION_MINOR  2

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Time Type
/// @{

/// @brief Processor time type
/// Protocol 4.0+ uses 64-bit timestamps
/// Protocol 3.x uses 32-bit timestamps (compile with CBPROTO_311)
#ifdef CBPROTO_311
typedef uint32_t PROCTIME;
#else
typedef uint64_t PROCTIME;
#endif

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Maximum Entity Ranges
///
/// Ground truth from upstream/cbproto/cbproto.h lines 237-262
/// These define the maximum number of instruments, channels, etc.
/// @{

#define cbNSP1          1   ///< First instrument ID (1-based)
#define cbNSP2          2   ///< Second instrument ID (1-based)
#define cbNSP3          3   ///< Third instrument ID (1-based)
#define cbNSP4          4   ///< Fourth instrument ID (1-based)

#define cbMAXOPEN       4   ///< Maximum number of open cbhwlib's (instruments)
#define cbMAXPROCS      1   ///< Number of processors per NSP

#define cbNUM_FE_CHANS  256 ///< Front-end channels per NSP

#define cbMAXGROUPS     8   ///< Number of sample rate groups
#define cbMAXFILTS      32  ///< Maximum number of filters

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Channel Counts
///
/// Ground truth from upstream/cbproto/cbproto.h lines 252-262
/// Note: Some channel counts depend on cbMAXPROCS
/// @{

#define cbNUM_ANAIN_CHANS       (16 * cbMAXPROCS)   ///< Analog input channels
#define cbNUM_ANALOG_CHANS      (cbNUM_FE_CHANS + cbNUM_ANAIN_CHANS)  ///< Total analog inputs
#define cbNUM_ANAOUT_CHANS      (4 * cbMAXPROCS)    ///< Analog output channels
#define cbNUM_AUDOUT_CHANS      (2 * cbMAXPROCS)    ///< Audio output channels
#define cbNUM_ANALOGOUT_CHANS   (cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS)  ///< Total analog outputs
#define cbNUM_DIGIN_CHANS       (1 * cbMAXPROCS)    ///< Digital input channels
#define cbNUM_SERIAL_CHANS      (1 * cbMAXPROCS)    ///< Serial input channels
#define cbNUM_DIGOUT_CHANS      (4 * cbMAXPROCS)    ///< Digital output channels

/// @brief Total number of channels
#define cbMAXCHANS  (cbNUM_ANALOG_CHANS + cbNUM_ANALOGOUT_CHANS + \
                     cbNUM_DIGIN_CHANS + cbNUM_SERIAL_CHANS + cbNUM_DIGOUT_CHANS)

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Network Configuration
///
/// Ground truth from upstream/cbproto/cbproto.h lines 193-196
/// @{

#define cbNET_UDP_ADDR_INST     "192.168.137.1"     ///< Cerebus default address
#define cbNET_UDP_ADDR_CNT      "192.168.137.128"   ///< Gemini NSP default control address
#define cbNET_UDP_ADDR_BCAST    "192.168.137.255"   ///< NSP default broadcast address
#define cbNET_UDP_PORT_BCAST    51002               ///< Neuroflow Data Port
#define cbNET_UDP_PORT_CNT      51001               ///< Neuroflow Control Port

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Packet Header
///
/// Ground truth from upstream/cbproto/cbproto.h lines 376-383
/// Every packet contains this header (must be a multiple of uint32_t)
/// @{

/// @brief Cerebus packet header data structure
typedef struct {
    PROCTIME time;        ///< System clock timestamp
    uint16_t chid;        ///< Channel identifier
    uint16_t type;        ///< Packet type
    uint16_t dlen;        ///< Length of data field in 32-bit chunks
    uint8_t  instrument;  ///< Instrument number (0-based in packet, despite cbNSP1=1!)
    uint8_t  reserved;    ///< Reserved for future use
} cbPKT_HEADER;

#define cbPKT_MAX_SIZE      1024                    ///< Maximum packet size in bytes
#define cbPKT_HEADER_SIZE   sizeof(cbPKT_HEADER)    ///< Packet header size in bytes
#define cbPKT_HEADER_32SIZE (cbPKT_HEADER_SIZE / 4) ///< Packet header size in uint32_t's

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Generic Packet
///
/// Base packet structure for all packet types
/// @{

/// @brief Generic packet structure
typedef struct {
    cbPKT_HEADER cbpkt_header;  ///< Packet header
    union {
        uint8_t  data_u8[cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE];   ///< Data as uint8_t array
        uint16_t data_u16[(cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE) / 2];  ///< Data as uint16_t array
        uint32_t data_u32[(cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE) / 4];  ///< Data as uint32_t array
    };
} cbPKT_GENERIC;

/// @}

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif // CBPROTO_TYPES_H
