///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   cbproto.h
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Main header for cbproto module
///
/// This is the primary include file for the protocol definitions module.
/// Include this file to access all protocol types, constants, and utilities.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBPROTO_CBPROTO_H
#define CBPROTO_CBPROTO_H

// Core protocol types and constants (C-compatible)
#include "types.h"
#include "config_buffer.h"

// C++-only utilities
#ifdef __cplusplus
#include "instrument_id.h"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @mainpage CereLink Protocol Definitions
///
/// @section intro Introduction
///
/// This module provides protocol definitions for the Cerebus protocol specification.
/// It serves as the foundation for the CereLink architecture.
///
/// @section key_types Key Types
///
/// - cbPKT_HEADER: Every packet contains this header
/// - cbPKT_GENERIC: Generic packet structure
/// - cbproto::InstrumentId: Type-safe instrument ID (C++ only)
///
/// @section ground_truth Ground Truth
///
/// All structures and constants in this module match Blackrock's cbproto.h exactly
/// to ensure compatibility with Central. DO NOT modify without updating from upstream.
///
/// @section usage Usage
///
/// C code:
/// @code
///     #include <cbproto/cbproto.h>
///
///     cbPKT_HEADER pkt;
///     pkt.instrument = 0;  // 0-based in packet!
/// @endcode
///
/// C++ code:
/// @code
///     #include <cbproto/cbproto.h>
///
///     // Type-safe instrument ID conversions
///     cbproto::InstrumentId id = cbproto::InstrumentId::fromPacketField(pkt.cbpkt_header.instrument);
///     uint8_t idx = id.toIndex();          // For array access
///     uint8_t oneBased = id.toOneBased();  // For API calls (cbNSP1, etc.)
/// @endcode
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#endif // CBPROTO_CBPROTO_H
