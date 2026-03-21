# cbproto - Protocol Definitions

Static library containing Cerebus wire protocol definitions.

## Contents

- Packet structures (`cbPKT_*`) and header formats for protocol versions 3.11, 4.0, 4.1, 4.2 (current)
- Protocol constants (`cbNSP1`, `cbMAXPROCS`, `cbMAXCHANS`, etc.)
- Basic types (`PROCTIME`, etc.)
- `InstrumentId` type for safe 0-based/1-based conversion
- `PacketTranslator` for bidirectional packet translation between protocol versions (used by both cbdev and cbshm)

## Design Principles

- **C Compatibility:** All structures are C-compatible for the public API
- **Minimal Dependencies:** Does not depend on any other CereLink module

## Usage

```cpp
#include <cbproto/types.h>

// Type-safe instrument ID conversions
cbproto::InstrumentId id = cbproto::InstrumentId::fromOneBased(1);
uint8_t index = id.toIndex();  // 0

// Packet translation (3.11 → current)
cbproto::PacketTranslator::translateHeader_311_to_current(src, dst);
cbproto::PacketTranslator::translatePayload_311_to_current(dst);
```
