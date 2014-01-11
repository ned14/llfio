#include "afio_pch.hpp"

int main(void)
{
#if 0
    //[current_exception_hack
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
    // VS2010 and Mingw32 segfaults if you ever do a try { throw; } catch(...) without an exception ever having been thrown :(
    try
    {
        throw boost::afio::detail::vs2010_lack_of_decent_current_exception_support_hack_t();
    }
    catch(...)
    {
        boost::afio::detail::vs2010_lack_of_decent_current_exception_support_hack()=boost::current_exception();
#endif
      
    ... do your AFIO using code ...
      
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
    }
#endif
    //]
#endif
    return 0;
}
