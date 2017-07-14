//! \file version.hpp Controls the version of AFIO for cmake, shared library and C++ namespace mangling
#undef AFIO_VERSION_MAJOR
#undef AFIO_VERSION_MINOR
#undef AFIO_VERSION_PATCH
#undef AFIO_VERSION_REVISION
#undef AFIO_VERSION_GLUE2
#undef AFIO_VERSION_GLUE
#undef AFIO_HEADERS_VERSION
#undef AFIO_NAMESPACE_VERSION

//! \brief Major version for cmake and DLL version stamping \ingroup config
#define AFIO_VERSION_MAJOR    2
//! \brief Minor version for cmake and DLL version stamping \ingroup config
#define AFIO_VERSION_MINOR    0
//! \brief Patch version for cmake and DLL version stamping \ingroup config
#define AFIO_VERSION_PATCH    0
//! \brief Revision version for cmake and DLL version stamping \ingroup config
#define AFIO_VERSION_REVISION 0

//! \brief Defined between stable releases of AFIO. It means the inline namespace
//! will be permuted per-commit to ensure ABI uniqueness. \ingroup config
#define AFIO_UNSTABLE_VERSION

#define AFIO_VERSION_GLUE2(a, b, c) a ## b ## c
#define AFIO_VERSION_GLUE(a, b, c)  AFIO_VERSION_GLUE2(a, b, c)
#define AFIO_NAMESPACE_VERSION   AFIO_VERSION_GLUE(AFIO_VERSION_MAJOR, _, AFIO_VERSION_MINOR)

#if defined(_MSC_VER) && !defined(__clang__)
#define AFIO_HEADERS_VERSION     AFIO_VERSION_GLUE(AFIO_VERSION_MAJOR, ., AFIO_VERSION_MINOR)
#else
#define AFIO_HEADERS_VERSION     AFIO_VERSION_MAJOR.AFIO_VERSION_MINOR
#endif
//! \brief The namespace AFIO_V2_NAMESPACE::v ## AFIO_NAMESPACE_VERSION
#define AFIO_NAMESPACE_VERSION AFIO_VERSION_GLUE(AFIO_VERSION_MAJOR, _, AFIO_VERSION_MINOR)
