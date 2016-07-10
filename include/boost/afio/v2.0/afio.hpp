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

#include "async_file_handle.hpp"
#include "statfs.hpp"
#include "storage_profile.hpp"

#include "algorithm/shared_fs_mutex/atomic_append.hpp"
#include "algorithm/shared_fs_mutex/byte_ranges.hpp"

#include "detail/child_process.hpp"

#endif
