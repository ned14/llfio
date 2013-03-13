/* std_filesystem.hpp
Gets a std::filesystem implementation from somewhere
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#if __cplusplus > 201103L
#include <filesystem>
#elif defined(HAVE_TR2_FILESYSTEM)
#include <tr2/filesystem>
namespace std { namespace filesystem { using namespace tr2::filesystem; } }
#else
#include "boost/filesystem/convenience.hpp"
namespace std { namespace filesystem { using namespace boost::filesystem; using boost::filesystem::path; } }
#endif
