//! \file v2.0/afio.hpp The master *versioned* AFIO include file. All version specific AFIO consuming libraries should include this header only.

#undef AFIO_VERSION_MAJOR
#undef AFIO_VERSION_MINOR
#undef AFIO_VERSION_PATCH
// Remove any previously defined versioning
#undef AFIO_VERSION_REVISION
#undef AFIO_VERSION_GLUE2
#undef AFIO_VERSION_GLUE
#undef AFIO_HEADERS_VERSION
#undef AFIO_NAMESPACE_VERSION
#undef AFIO_MODULE_NAME

#define AFIO_VERSION_GLUE2(a, b, c) a##b##c
#define AFIO_VERSION_GLUE(a, b, c) AFIO_VERSION_GLUE2(a, b, c)

// Hard coded as this is a specific version
#define AFIO_VERSION_MAJOR 2
#define AFIO_VERSION_MINOR 0
#define AFIO_VERSION_PATCH 0
#define AFIO_VERSION_REVISION 0
//! \brief The namespace AFIO_V2_NAMESPACE::v ## AFIO_NAMESPACE_VERSION
#define AFIO_NAMESPACE_VERSION AFIO_VERSION_GLUE(AFIO_VERSION_MAJOR, _, AFIO_VERSION_MINOR)

#if defined(__cpp_modules) || defined(DOXYGEN_SHOULD_SKIP_THIS)
#if defined(_MSC_VER) && !defined(__clang__)
//! \brief The AFIO C++ module name
#define AFIO_MODULE_NAME AFIO_VERSION_GLUE(afio_v, AFIO_NAMESPACE_VERSION, )
#else
//! \brief The AFIO C++ module name
#define AFIO_MODULE_NAME AFIO_VERSION_GLUE(afio_v, AFIO_NAMESPACE_VERSION, )
#endif
#endif

// If C++ Modules are on and we are not compiling the library,
// we are either generating the interface or importing
#if defined(__cpp_modules)
#if defined(GENERATING_AFIO_MODULE_INTERFACE)
// We are generating this module's interface
#define QUICKCPPLIB_HEADERS_ONLY 0
#define AFIO_HEADERS_ONLY 0
#define AFIO_INCLUDE_ALL
#elif defined(AFIO_SOURCE)
// We are implementing this module
#define AFIO_INCLUDE_ALL
#else
// We are importing this module
import AFIO_MODULE_NAME;
#undef AFIO_INCLUDE_ALL
#endif
#else
// C++ Modules not on, therefore include as usual
#define AFIO_INCLUDE_ALL
#endif

#ifdef AFIO_INCLUDE_ALL

#include "config.hpp"

#include "async_file_handle.hpp"
#include "directory_handle.hpp"
#include "map_handle.hpp"
#include "statfs.hpp"
#include "storage_profile.hpp"

#include "algorithm/mapped_view.hpp"
#include "algorithm/section_allocator.hpp"
#include "algorithm/shared_fs_mutex/atomic_append.hpp"
#include "algorithm/shared_fs_mutex/byte_ranges.hpp"
#include "algorithm/shared_fs_mutex/lock_files.hpp"
#include "algorithm/shared_fs_mutex/memory_map.hpp"
#include "algorithm/shared_fs_mutex/safe_byte_ranges.hpp"

#endif
