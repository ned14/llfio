//! \file version.hpp Controls the version of AFIO for cmake, shared library and C++ namespace mangling
#undef BOOST_AFIO_VERSION_MAJOR
#undef BOOST_AFIO_VERSION_MINOR
#undef BOOST_AFIO_VERSION_PATCH
#undef BOOST_AFIO_VERSION_REVISION
#undef BOOST_AFIO_VERSION_GLUE2
#undef BOOST_AFIO_VERSION_GLUE
#undef BOOST_AFIO_HEADERS_VERSION
#undef BOOST_AFIO_NAMESPACE_VERSION

//! \brief Major version for cmake and DLL version stamping \ingroup config
#define BOOST_AFIO_VERSION_MAJOR    2
//! \brief Minor version for cmake and DLL version stamping \ingroup config
#define BOOST_AFIO_VERSION_MINOR    0
//! \brief Patch version for cmake and DLL version stamping \ingroup config
#define BOOST_AFIO_VERSION_PATCH    0
//! \brief Revision version for cmake and DLL version stamping \ingroup config
#define BOOST_AFIO_VERSION_REVISION 0

//! \brief Defined between stable releases of AFIO. It means the inline namespace
//! will be permuted per-commit to ensure ABI uniqueness. \ingroup config
#define BOOST_AFIO_UNSTABLE_VERSION

#define BOOST_AFIO_VERSION_GLUE2(a, b, c) a ## b ## c
#define BOOST_AFIO_VERSION_GLUE(a, b, c)  BOOST_AFIO_VERSION_GLUE2(a, b, c)
#define BOOST_AFIO_NAMESPACE_VERSION   BOOST_AFIO_VERSION_GLUE(BOOST_AFIO_VERSION_MAJOR, _, BOOST_AFIO_VERSION_MINOR)

#if defined(_MSC_VER) && !defined(__clang__)
#define BOOST_AFIO_HEADERS_VERSION     BOOST_AFIO_VERSION_GLUE(BOOST_AFIO_VERSION_MAJOR, ., BOOST_AFIO_VERSION_MINOR)
#else
#define BOOST_AFIO_HEADERS_VERSION     BOOST_AFIO_VERSION_MAJOR.BOOST_AFIO_VERSION_MINOR
#endif
//! \brief The namespace boost::afio::v ## BOOST_AFIO_NAMESPACE_VERSION
#define BOOST_AFIO_NAMESPACE_VERSION BOOST_AFIO_VERSION_GLUE(BOOST_AFIO_VERSION_MAJOR, _, BOOST_AFIO_VERSION_MINOR)
