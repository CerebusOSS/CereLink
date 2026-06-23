# Multi-Version Central Compatibility

**Author**: Caden Shmookler
**Date**: 2026-06-16


## Brief

This document describes a compatibility layer within CereLink which enables cross-version compatibility with Central's shared memory.  Previous versions of CereLink only supported the latest version of Central.  This compatibility layer consists of an `Adapter` class which hides the implementation differences between protocol versions behind a common interface.

Central has a 'protocol version' and an 'application version'.  As of the writing of this document, the most recent protocol and application versions are 4.2 and 7.8.0 respectively.  Shared memory compatibility is dependent on the protocol version.  Protocols with different major version numbers are completely incompatible whereas protocols with different minor version numbers may be partially compatible.  Different application versions do not break protocol compatibility unless the protocol version has also changed.


## Organization

Each supported protocol version has a `BootstrapAdapter` and `Adapter`.  The `BootstrapAdapter` class fetches the sizes of shared memory structs in order to instantiate an `Adapter` with raw pointers to each struct.  Each adapter has a collection of Central structs which are translated to and from the native CereLink equivalents.  These native equivalents (defined in cbproto and native_types.h) are the common language used by `ShmemSession` to perform operations on the shared memory buffers.

The structure and sizes of types are defined in `src/cbshm/include/cbshm/central_types/*`.  The translation behavior is defined in `src/cbshm/src/central_adapters/*`.  Version headers in `src/cbshm/include/cbshm/central_adapters/*` should be nearly identical.


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

If the added version contains changes to types or constants that are not already in the header, add them by copying directly from Central or cbproto.  Verify your changes by diffing the existing version header with the added version header.

#### 6. Rectify the translators and adapter for the added version

```bash
editor src/cbshm/src/central_adapters/<version>.cpp
```

Verify your changes by diffing the existing version header with the added version header.

#### 7. Add the version to ShmemSession::Impl

```bash
editor src/cbshm/src/shmem_session.cpp
```

Add `#include <cbshm/central_types/<version>.h>` at the top of the file and append the adapter and bootstrap adapter to the corresponding switch statements in `ShmemSession::Impl::open`.  Modify the switch statement at the bottom of `detectCompatProtocol` so corresponding application versions are mapped to the added protocol version.

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

If the added version contains changes to types or constants that are not already in the header, add them by copying directly from Central or cbproto.  Verify your changes by diffing the existing version header with the added version header.

#### 6. Rectify the translators and adapter for the added version

```bash
editor src/cbshm/src/central_adapters/<version>.cpp
```

Verify your changes by diffing the existing version header with the added version header.

#### 7. Add the version to ShmemSession::Impl

```bash
editor src/cbshm/src/shmem_session.cpp
```

Add `#include <cbshm/central_types/<version>.h>` at the top of the file and append the adapter and bootstrap adapter to the corresponding switch statements in `ShmemSession::Impl::open`.  Modify the switch statement at the bottom of `detectCompatProtocol` so corresponding application versions are mapped to the added protocol version.

#### 8. Add the adapter implementation to CMakeLists.txt

```bash
editor src/cbshm/CMakeLists.txt
```

Append `src/central_adapters/<version>.cpp` to the `CBSHMEM_SOURCES` environment variable.


## Remove a protocol version

Follow the instructions for adding a new version in reverse.  To disable support for a specific version instead of outright removing it, replace the adapter construction in `ShmemSession::Impl::open` within `src/cbshm/src/shmem_session.cpp` with an error.
