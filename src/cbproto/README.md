# cbproto - Protocol Definitions

**Status:** Phase 1 - ✅ **COMPLETE** (2025-11-11)

## Purpose

Pure protocol definitions module containing:
- Packet structures (cbPKT_*)
- Protocol constants (cbNSP1, cbMAXPROCS, etc.)
- Basic types (PROCTIME, etc.)
- Version information
- InstrumentId type for safe 0-based/1-based conversion

## Key Design Principles

1. **No Implementation Logic:** This module contains only data definitions
2. **C Compatibility:** All structures must be C-compatible for public API
3. **Zero Dependencies:** Does not depend on any other CereLink module
4. **Header-Only:** Implemented as header-only library for simplicity

## Current Status

- [x] Directory structure created
- [x] CMake integration added
- [x] InstrumentId type designed and implemented
- [x] Core protocol types extracted from upstream/cbproto/cbproto.h
- [x] Tests written (34 tests, all passing)
- [x] Build system working
- [x] Ground truth compatibility verified

## Usage (Future)

```cpp
#include <cbproto/types.h>
#include <cbproto/packets.h>

// Type-safe instrument ID conversions
cbproto::InstrumentId id = cbproto::InstrumentId::fromOneBased(1);
uint8_t index = id.toIndex();  // 0
uint8_t oneBased = id.toOneBased();  // 1
```

## References

- Design document: `docs/refactor_plan.md` (Phase 1)
- Current protocol: `include/cerelink/cbproto.h`
