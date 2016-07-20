#undef BOOST_AFIO_VERSION_MAJOR
#undef BOOST_AFIO_VERSION_MINOR
#undef BOOST_AFIO_VERSION_PATCH
#undef BOOST_AFIO_VERSION_REVISION
#undef BOOST_AFIO_VERSION_GLUE2
#undef BOOST_AFIO_VERSION_GLUE
#undef BOOST_AFIO_HEADERS_VERSION
#undef BOOST_AFIO_NAMESPACE_VERSION

#define BOOST_AFIO_VERSION_MAJOR    2  // Major version for cmake and DLL version stamping
#define BOOST_AFIO_VERSION_MINOR    0  // Minor version for cmake and DLL version stamping
#define BOOST_AFIO_VERSION_PATCH    0  // Patch version for cmake and DLL version stamping
#define BOOST_AFIO_VERSION_REVISION 0  // Revision version for cmake and DLL version stamping

// Defined between stable releases of AFIO
#define BOOST_AFIO_UNSTABLE_VERSION

#define BOOST_AFIO_VERSION_GLUE2(a, b, c) a ## b ## c
#define BOOST_AFIO_VERSION_GLUE(a, b, c)  BOOST_AFIO_VERSION_GLUE2(a, b, c)
// The namespace boost::afio::v ## BOOST_AFIO_NAMESPACE_VERSION
#define BOOST_AFIO_NAMESPACE_VERSION   BOOST_AFIO_VERSION_GLUE(BOOST_AFIO_VERSION_MAJOR, _, BOOST_AFIO_VERSION_MINOR)

#if defined(_MSC_VER) && !defined(__clang)
// The path for the headers v ## BOOST_AFIO_HEADERS_VERSION
#define BOOST_AFIO_HEADERS_VERSION     BOOST_AFIO_VERSION_GLUE(BOOST_AFIO_VERSION_MAJOR, ., BOOST_AFIO_VERSION_MINOR)
#else
// The path for the headers v ## BOOST_AFIO_HEADERS_VERSION
#define BOOST_AFIO_HEADERS_VERSION     BOOST_AFIO_VERSION_MAJOR.BOOST_AFIO_VERSION_MINOR
#endif
