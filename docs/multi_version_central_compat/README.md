# Multi-Version Central Compatibility

**Author**: Caden Shmookler
**Date**: 2026-06-16


## Brief

This document describes a compatibility layer within CereLink which enables cross-version compatibility with Central's shared memory.  Previous versions of CereLink only supported the latest version of Central.  This compatibility layer consists of an `Adapter` class which hides the implementation differences between Central versions behind a common interface.  Adapters are selected per Central application version (see the `CentralVersion` enum in [Organization](#organization)).

Central has a 'protocol version' and an 'application version'.  As of the writing of this document, the most recent protocol and application versions are 4.2 and 7.8.0 respectively.  Shared memory compatibility is dependent on the protocol version.  Protocols with different major version numbers are completely incompatible whereas protocols with different minor version numbers may be partially compatible.  Different application versions do not break protocol compatibility unless the protocol version has also changed.


## Organization

Each supported application version of Central has a `BootstrapAdapter` and `Adapter`.  The `BootstrapAdapter` class fetches the sizes of shared memory structs in order to instantiate an `Adapter` with raw pointers to each struct.  Each adapter has a collection of Central structs which are translated to and from the native CereLink equivalents.  These native equivalents (defined in cbproto and native_types.h) are the common language used by `ShmemSession` to perform operations on the shared memory buffers.

Adapters are named after the Central *application* version they support (e.g. `v7_0`, `v7_5`), not the protocol version.  The `CentralVersion` enum in `src/cbshm/include/cbshm/central_version.h` enumerates every supported application version: `V7_0`, `V7_5`, `V7_6`, `V7_7`, and `CURRENT` (the newest supported version, currently 7.8).

The structure and sizes of types are defined in `src/cbshm/include/cbshm/central_types/<version>.h`.  The adapter and bootstrap adapter classes are declared in `src/cbshm/include/cbshm/central_adapters/<version>.h`, and their translation behavior is defined in `src/cbshm/src/central_adapters/<version>.cpp`.  Each version lives in its own namespace (e.g. `central_v7_0`).  The current version's types and adapters live in `central_v7_8`, which `src/cbshm/include/cbshm/central_current.h` aliases as `central`.  Version headers in `src/cbshm/include/cbshm/central_adapters/*` should be nearly identical.

At runtime, `detectCentralVersion` (declared in `src/cbshm/include/cbshm/central_version.h`, defined in `src/cbshm/src/central_version.cpp`) inspects the application version of the running `Central.exe` and returns the matching `CentralVersion`.  `getProtocolVersion` converts a `CentralVersion` to its protocol version for the receive/transmit buffer logic.  `ShmemSession::Impl::open` uses the detected `CentralVersion` to select the appropriate `BootstrapAdapter` and `Adapter`.


## Limitations

This compatibility layer is limited to Central's configuration, status, and spike buffers.  The receive and transmit buffers are handled by brittle logic spread throughout cbdev, cbproto, and cbshm.  Replacing this brittle logic with another adapter class encapsulating all version-specific code would dramatically simplify the process of adding protocol versions.


## Add a Version

This section provides instructions for adding support for a version of Central's shared memory.

By default, new application versions of Central are assumed to use the latest protocol version.  If Central's application version increments but it's protocol version remains the same, no action is required to enable compatibility.

If a version of Central has a newer protocol version than what's currently supported, follow the [newer version](#add-a-newer-protocol-version) instructions below.

If a version of Central has an older protocol version than what's currently supported, follow the [older version](#add-an-older-protocol-version) instructions below.


### Add a newer protocol version

#### 1. Add the version to cbproto_protocol_version

```bash
editor src/cbproto/include/cbproto/connection.h
```

`CBPROTO_PROTOCOL_CURRENT` now codes for the added version.  Add a new value for the replaced version (e.g. `CBPROTO_PROTOCOL_420`).

#### 2. Implement receive/transmit buffer operations

Multiple files throughout cbdev, cbproto, and cbshm contain version-specific logic (pertaining to the receive and transmit buffers) that must be changed to include a case for the added version.

> Note: This is a complex operation involving large portions of CereLink (depending on what was changed in the new protocol).  See the [limitations](#limitations) section for an idea on how this process could be simplified in the future.

#### 3. Duplicate version headers and implementation from an existing version

Replace `<existing_version>` with the name of an existing version and `<version>` with the name of the version that's being added.  Duplicate an existing version that's similar in structure and behavior to the version that's being added.

```bash
cp src/cbshm/include/cbshm/central_types/<existing_version>.h src/cbshm/include/cbshm/central_types/<version>.h
cp src/cbshm/include/cbshm/central_adapters/<existing_version>.h src/cbshm/include/cbshm/central_adapters/<version>.h
cp src/cbshm/src/central_adapters/<existing_version>.cpp src/cbshm/src/central_adapters/<version>.cpp
```

#### 4. Name the added version

Change all references to the existing version to the added version.

```bash
editor src/cbshm/include/cbshm/central_adapters/<version>.h
editor src/cbshm/include/cbshm/central_types/<version>.h
editor src/cbshm/src/central_adapters/<version>.cpp
```

#### 5. Rectify the types for the added version

```bash
editor src/cbshm/include/cbshm/central_types/<version>.h
```

If the added version contains changes to types or constants that are not already in the header, add them by copying directly from Central or cbproto.  Update the hardcoded sizes within the static_assert expressions to match ground truth in Central and cbproto.  Verify your changes by diffing the existing version header with the added version header.

#### 6. Rectify the translators and adapter for the added version

```bash
editor src/cbshm/src/central_adapters/<version>.cpp
```

Verify your changes by diffing the existing version header with the added version header.

#### 7. Register the version

First, add a value for the version to the `CentralVersion` enum.  Because the added version is newer than any currently supported version, it becomes the new `CURRENT`; add an explicit value for the version that `CURRENT` previously coded for (e.g. `V7_8`).

```bash
editor src/cbshm/include/cbshm/central_version.h
```

Repoint the `central` alias at the added version's namespace.

```bash
editor src/cbshm/include/cbshm/central_current.h
```

Map the application version to the added `CentralVersion` value in `detectCentralVersion`, and map each `CentralVersion` value to its protocol version in `getProtocolVersion`.  The added version maps to `CBPROTO_PROTOCOL_CURRENT`; the previously-current version now maps to its own (frozen) protocol version.

```bash
editor src/cbshm/src/central_version.cpp
```

Finally, register the adapter with `ShmemSession`.  Add `#include <cbshm/central_types/<version>.h>` and `#include <cbshm/central_adapters/<version>.h>` near the top of the file and append cases for the new `CentralVersion` values to both switch statements in `ShmemSession::Impl::open` (one selects the `BootstrapAdapter`, the other selects the `Adapter`).  The `CURRENT` case uses the `central::` alias, so the added version is reached through it; add an explicit case for the previously-current version.

```bash
editor src/cbshm/src/shmem_session.cpp
```

#### 8. Add the adapter implementation to CMakeLists.txt

```bash
editor src/cbshm/CMakeLists.txt
```

Append `src/central_adapters/<version>.cpp` to the `CBSHMEM_SOURCES` environment variable.

#### 9. Update types in cbproto to match the added version

The types in cbproto must match the most recent protocol version, `CBPROTO_PROTOCOL_CURRENT`.  Changes to these types may cause downstream side effects elsewhere in CereLink, including but not limited to the `PacketTranslator` and `DeviceSession` classes.

> Note: The cbproto types are used for direct communication with instruments instead of with Central.  If there are differences in the protocol between these targets then they must be reflected in the code.

> Note: This is a complex operation involving large portions of CereLink (depending on what was changed in the new protocol).  See the [limitations](#limitations) section for an idea on how this process could be simplified in the future.

#### 10. Rectify the translators and adapters for all older versions

All translators and adapters use the cbproto types, so these methods must be fixed to translate to the added version instead.

#### 11. Add the version to the adapter unit tests

```bash
editor tests/unit/test_central_adapters.cpp
```

Add `#include <cbshm/central_adapters/<version>.h>` near the top of the file, define a `VersionTraits` alias for the version (e.g. `using V7_9 = VersionTraits<central_v7_9::BootstrapAdapter, central_v7_9::Adapter>;`), and add that alias to the `AllVersions` type list so the round-trip invariants run against it.  If the added version's protocol diverges from the others (e.g. the NSP-status and Gemini fields introduced in protocol 4.0+), cover that behavior with dedicated tests.  The test file has no entry in `tests/unit/CMakeLists.txt` to update — it already compiles every version through the `AllVersions` type list.


### Add an older protocol version

#### 1. Add the version to cbproto_protocol_version

```bash
editor src/cbproto/include/cbproto/connection.h
```

Add a new value for the added version (e.g. `CBPROTO_PROTOCOL_309`).

#### 2. Implement receive/transmit buffer operations

Multiple files throughout cbdev, cbproto, and cbshm contain version-specific logic (pertaining to the receive and transmit buffers) that must be changed to include a case for the added version.

> Note: This is a complex operation involving large portions of CereLink (depending on what was changed in the new protocol).  See the [limitations](#limitations) section for an idea on how this process could be simplified in the future.

#### 3. Duplicate version headers and implementation from an existing version

Replace `<existing_version>` with the name of an existing version and `<version>` with the name of the version that's being added.  Duplicate an existing version that's similar in structure and behavior to the version that's being added.

```bash
cp src/cbshm/include/cbshm/central_types/<existing_version>.h src/cbshm/include/cbshm/central_types/<version>.h
cp src/cbshm/include/cbshm/central_adapters/<existing_version>.h src/cbshm/include/cbshm/central_adapters/<version>.h
cp src/cbshm/src/central_adapters/<existing_version>.cpp src/cbshm/src/central_adapters/<version>.cpp
```

#### 4. Name the added version

Change all references to the existing version to the added version.

```bash
editor src/cbshm/include/cbshm/central_adapters/<version>.h
editor src/cbshm/include/cbshm/central_types/<version>.h
editor src/cbshm/src/central_adapters/<version>.cpp
```

#### 5. Rectify the types for the added version

```bash
editor src/cbshm/include/cbshm/central_types/<version>.h
```

If the added version contains changes to types or constants that are not already in the header, add them by copying directly from Central or cbproto.  Update the hardcoded sizes within the static_assert expressions to match ground truth in Central and cbproto.  Verify your changes by diffing the existing version header with the added version header.

#### 6. Rectify the translators and adapter for the added version

```bash
editor src/cbshm/src/central_adapters/<version>.cpp
```

Verify your changes by diffing the existing version header with the added version header.

#### 7. Register the version

First, add a value for the version to the `CentralVersion` enum.

```bash
editor src/cbshm/include/cbshm/central_version.h
```

Map the application version to the added `CentralVersion` value in `detectCentralVersion`, and map the added `CentralVersion` value to its protocol version in `getProtocolVersion`.

```bash
editor src/cbshm/src/central_version.cpp
```

Finally, register the adapter with `ShmemSession`.  Add `#include <cbshm/central_types/<version>.h>` and `#include <cbshm/central_adapters/<version>.h>` near the top of the file and append a case for the added `CentralVersion` value to both switch statements in `ShmemSession::Impl::open` (one selects the `BootstrapAdapter`, the other selects the `Adapter`).

```bash
editor src/cbshm/src/shmem_session.cpp
```

#### 8. Add the adapter implementation to CMakeLists.txt

```bash
editor src/cbshm/CMakeLists.txt
```

Append `src/central_adapters/<version>.cpp` to the `CBSHMEM_SOURCES` environment variable.

#### 9. Add the version to the adapter unit tests

```bash
editor tests/unit/test_central_adapters.cpp
```

Add `#include <cbshm/central_adapters/<version>.h>` near the top of the file, define a `VersionTraits` alias for the version (e.g. `using V7_1 = VersionTraits<central_v7_1::BootstrapAdapter, central_v7_1::Adapter>;`), and add that alias to the `AllVersions` type list so the round-trip invariants run against it.  If the added version's protocol diverges from the others (e.g. the NSP-status and Gemini fields introduced in protocol 4.0+), cover that behavior with dedicated tests.  The test file has no entry in `tests/unit/CMakeLists.txt` to update — it already compiles every version through the `AllVersions` type list.


## Remove a protocol version

Follow the instructions for adding a new version in reverse.  To disable support for a specific version instead of outright removing it, replace the adapter construction in `ShmemSession::Impl::open` within `src/cbshm/src/shmem_session.cpp` with an error.
