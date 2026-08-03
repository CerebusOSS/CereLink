///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   current.h
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Default version selection for Central-compatible shared memory
/// structure and adapter definitions
///
/// Central-compatible shared memory structure definitions have been moved to
/// version-specific files in central_types/. Adapters are declared in
/// central_adapters/.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_CENTRAL_ADAPTERS_CURRENT_H
#define CBSHM_CENTRAL_ADAPTERS_CURRENT_H

#include <cbshm/central_types/v7_8.h>
#include <cbshm/central_adapters/v7_8.h>

namespace cbshm {

namespace central = cbshm::central_v7_8;

} // namespace cbshm

#endif // CBSHMEM_CENTRAL_ADAPTERS_CURRENT_H
