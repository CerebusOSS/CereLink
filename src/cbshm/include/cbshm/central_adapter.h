///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   central_adapter.h
/// @author Caden Shmookler
/// @date   2026-05-05
///
/// @brief  The base class of Central-compatible shared memory adapters
///
/// This module provides the interface (base class) of adapters that attach to
/// the shared memory of specific supported versions of Central.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbshm/shmem_session.h>
#include <cbproto/connection.h>
#include <cbutil/result.h>

#ifndef CBSHM_CENTRAL_ADAPTER_H
#define CBSHM_CENTRAL_ADAPTER_H

namespace cbshm {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Base-class adapter for Central-compatible shared memory access
///
/// This module provides the interface for adapters that attach to the shared
/// memory of specific supported versions of Central.
///
class CentralAdapter {
public:
    virtual ~CentralAdapter() = default;

    // TODO: Declare required methods
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Instantiate a shared memory adapter for Central that corresponds to
/// Central's protocol version.
///
cbutil::Result<CentralAdapter> makeCentralAdapter();

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Instantiate a shared memory adapter for Central that corresponds to
/// a specific protocol version.
///
cbutil::Result<CentralAdapter> makeCentralAdapter(cbproto_protocol_version_t protocol_version);

} // namespace cbshm

#endif // CBSHM_CENTRAL_ADAPTER_H
