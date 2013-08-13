/* std_filesystem.hpp
Gets a std::filesystem implementation from somewhere
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifndef STD_FILESYSTEM_FROM_SOMEWHERE
#define STD_FILESYSTEM_FROM_SOMEWHERE
#if __cplusplus > 201103L
#include <filesystem>
#elif defined(HAVE_TR2_FILESYSTEM)
#include <tr2/filesystem>
namespace std { namespace filesystem { using namespace tr2::filesystem; } }
#else
#include "boost/filesystem/convenience.hpp"
#include "boost/filesystem/fstream.hpp"
namespace std {
	namespace filesystem { using namespace boost::filesystem; using boost::filesystem::path; }
	// Some MSVC headers define std::hash as a class
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4099) // warning C4099: 'std::hash' : type name first seen using 'class' now seen using 'struct'
#endif
	template<class T> struct hash;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
	template<> struct hash<boost::filesystem::path>
	{
	public:
		size_t operator()(const boost::filesystem::path& p) const
		{
			return boost::filesystem::hash_value(p);
		}
	};
}
#endif
#endif
