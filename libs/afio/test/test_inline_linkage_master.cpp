// Boost ASIO needs this
#if !defined(_WIN32_WINNT) && defined(WIN32)
#define _WIN32_WINNT 0x0501
#endif
// Need to include a copy of ASIO
#ifdef BOOST_ASIO_SEPARATE_COMPILATION
#include "../../../boost/asio/impl/src.hpp"
#endif

extern void test_inline_linkage1();
extern void test_inline_linkage2();

int main(void)
{
    test_inline_linkage1();
    test_inline_linkage2();
    return 0;
}