//! \file v2.0/afio.hpp The master *versioned* AFIO include file. All version specific AFIO consuming libraries should include this header only.

#undef BOOST_AFIO_VERSION_MAJOR
#undef BOOST_AFIO_VERSION_MINOR
#undef BOOST_AFIO_VERSION_PATCH
// Remove any previously defined versioning
#undef BOOST_AFIO_VERSION_REVISION
#undef BOOST_AFIO_VERSION_GLUE2
#undef BOOST_AFIO_VERSION_GLUE
#undef BOOST_AFIO_HEADERS_VERSION
#undef BOOST_AFIO_NAMESPACE_VERSION
#undef BOOST_AFIO_MODULE_NAME

#define BOOST_AFIO_VERSION_GLUE2(a, b, c) a##b##c
#define BOOST_AFIO_VERSION_GLUE(a, b, c) BOOST_AFIO_VERSION_GLUE2(a, b, c)

// Hard coded as this is a specific version
#define BOOST_AFIO_VERSION_MAJOR 2
#define BOOST_AFIO_VERSION_MINOR 0
#define BOOST_AFIO_VERSION_PATCH 0
#define BOOST_AFIO_VERSION_REVISION 0
//! \brief The namespace boost::afio::v ## BOOST_AFIO_NAMESPACE_VERSION
#define BOOST_AFIO_NAMESPACE_VERSION BOOST_AFIO_VERSION_GLUE(BOOST_AFIO_VERSION_MAJOR, _, BOOST_AFIO_VERSION_MINOR)

#if defined(__cpp_modules) || defined(DOXYGEN_SHOULD_SKIP_THIS)
#if defined(_MSC_VER) && !defined(__clang__)
//! \brief The AFIO C++ module name
#define BOOST_AFIO_MODULE_NAME BOOST_AFIO_VERSION_GLUE(afio_v, BOOST_AFIO_NAMESPACE_VERSION, )
#else
//! \brief The AFIO C++ module name
#define BOOST_AFIO_MODULE_NAME BOOST_AFIO_VERSION_GLUE(afio_v, BOOST_AFIO_NAMESPACE_VERSION, )
#endif
#endif

// If C++ Modules are on and we are not compiling the library,
// we are either generating the interface or importing
#if defined(__cpp_modules)
#if defined(GENERATING_CXX_MODULE_INTERFACE)
// We are generating this module's interface
#define BOOSTLITE_HEADERS_ONLY 0
#define BOOST_AFIO_HEADERS_ONLY 0
#define BOOST_AFIO_INCLUDE_ALL
#elif defined(BOOST_AFIO_SOURCE)
// We are implementing this module
#define BOOST_AFIO_INCLUDE_ALL
#else
// We are importing this module
import BOOST_AFIO_MODULE_NAME;
#undef BOOST_AFIO_INCLUDE_ALL
#endif
#else
// C++ Modules not on, therefore include as usual
#define BOOST_AFIO_INCLUDE_ALL
#endif

#ifdef BOOST_AFIO_INCLUDE_ALL

#include "config.hpp"

#include "async_file_handle.hpp"
#include "map_handle.hpp"
#include "statfs.hpp"
#include "storage_profile.hpp"

#include "algorithm/shared_fs_mutex/atomic_append.hpp"
#include "algorithm/shared_fs_mutex/byte_ranges.hpp"

#include "detail/child_process.hpp"

#endif
