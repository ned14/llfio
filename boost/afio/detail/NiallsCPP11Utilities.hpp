/* NiallsCPP11Utilities
(C) 2012 Niall Douglas http://www.nedprod.com/
File Created: Nov 2012
*/

#ifndef NIALLSCPP11UTILITIES_H
#define NIALLSCPP11UTILITIES_H

/*! \file NiallsCPP11Utilities.hpp
\brief Declares Niall's useful C++ 11 utilities
*/

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251) // needs to have dll-interface to be used by clients of
#endif

#ifndef DISABLE_SYMBOLMANGLER
#define DISABLE_SYMBOLMANGLER 1 // SymbolMangler needs a ground-up reimplementation. Its current implementation isn't reliable.
#endif

/*! \mainpage

\warning You'll definitely need a fairly compliant C++ 11 compiler for this library to work.
In particular, you \b need variadic templates.

Build using scons (http://www.scons.org/). You only need to build StaticTypeRegistry.cpp
if you use NiallsCPP11Utilities::StaticTypeRegistry (it consists of one line unavoidable
on Windows). You can use --useclang to force use of clang. You can use --usegcc to force
use of gcc on Windows. I have configured scons to have the intelligence to try using g++
if it's not being run from inside a Visual Studio Tools Command Box (i.e. vcvars32.bat
hasn't been run). Finally, --debugbuild generates a debug build ;). There are a few more
build options available too, try 'scons --help' to see them.

Tested on the following compilers:
 - Visual Studio 2012 Nov CTP (the one with variadic template support)
 - clang++ v3.2.
 - g++ v4.6.2.
*/

#include <cassert>

#include <vector>
#include <memory>
#include <type_traits>
#include <functional>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <typeinfo>
#include <string>
#include <list>
#include <streambuf>
#include <ios>
#include <iostream>
#include <cstddef>
#include <stdexcept>

// This avoids including <codecvt> which libstdc++ doesn't provide and therefore breaks GCC and clang
namespace std {
	template <class Elem, unsigned long Maxcode = 0x10ffff>
	class codecvt_utf8_utf16
		: public codecvt<Elem, char, mbstate_t>
	{
		// unspecified
	};
}

#if defined(_MSC_VER) && _MSC_VER<=1700 && !defined(noexcept)
#define noexcept throw()
#endif
#if defined(_MSC_VER) && _MSC_VER<=1700 && !defined(constexpr)
#define constexpr const
#endif
#if defined(__GNUC__) && !defined(GCC_VERSION)
#define GCC_VERSION (__GNUC__ * 10000 \
				   + __GNUC_MINOR__ * 100 \
				   + __GNUC_PATCHLEVEL__)
#endif

//! \def DLLEXPORTMARKUP The markup this compiler uses to export a symbol from a DLL
#ifndef DLLEXPORTMARKUP
#ifdef WIN32
#define DLLEXPORTMARKUP __declspec(dllexport)
#elif defined(__GNUC__)
#define DLLEXPORTMARKUP __attribute__((visibility("default")))
#else
#define DLLEXPORTMARKUP
#endif
#endif

//! \def DLLIMPORTMARKUP The markup this compiler uses to import a symbol from a DLL
#ifndef DLLIMPORTMARKUP
#ifdef WIN32
#define DLLIMPORTMARKUP __declspec(dllimport)
#else
#define DLLIMPORTMARKUP
#endif
#endif

//! \def DLLSELECTANYMARKUP The markup this compiler uses to mark a symbol as being weak
#ifndef DLLWEAKMARKUP
#ifdef WIN32
#define DLLWEAKMARKUP(type, name) extern __declspec(selectany) type name; extern __declspec(selectany) type name##_weak=NULL; __pragma(comment(linker, "/alternatename:_" #name "=_" #name "_weak"))
#elif defined(__GNUC__)
#define DLLWEAKMARKUP(type, name) extern __attribute__((weak)) type declaration
#else
#define DLLWEAKMARKUP(type, name)
#endif
#endif

//! \def TYPEALIGNMENT(bytes) The markup this compiler uses to mark a type as having some given alignment
#ifndef TYPEALIGNMENT
#if __cplusplus>=201103L && GCC_VERSION > 40900
#define TYPEALIGNMENT(bytes) alignas(bytes)
#else
#ifdef _MSC_VER
#define TYPEALIGNMENT(bytes) __declspec(align(bytes))
#elif defined(__GNUC__)
#define TYPEALIGNMENT(bytes) __attribute__((aligned(bytes)))
#else
#define TYPEALIGNMENT(bytes) unknown_type_alignment_markup_for_this_compiler
#endif
#endif
#endif

//! \def PACKEDTYPE(typedecl) The markup this compiler uses to pack a structure as tightly as possible
#ifndef PACKEDTYPE
#ifdef _MSC_VER
#define PACKEDTYPE(typedecl) __pragma(pack(push, 1)) typedecl __pragma(pack(pop))
#elif defined(__GNUC__)
#define PACKEDTYPE(typedecl) typedecl __attribute__((packed))
#else
#define PACKEDTYPE(typedecl) unknown_type_pack_markup_for_this_compiler
#endif
#endif

#ifdef NIALLSCPP11UTILITIES_DLL_EXPORTS
#define NIALLSCPP11UTILITIES_API DLLEXPORTMARKUP
#else
#define NIALLSCPP11UTILITIES_API DLLIMPORTMARKUP
#endif

//! \def DEFINES Defines RETURNS to automatically figure out your return type
#ifndef RETURNS
#define RETURNS(...) -> decltype(__VA_ARGS__) { return (__VA_ARGS__); }
#endif

#ifndef __LP64__
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IA64))
#define __LP64__ 1
#endif
#endif

//! \namespace NiallsCPP11Utilities Where Niall's useful C++ 11 utilities live
namespace NiallsCPP11Utilities {

/*! \brief Defines a byte buffer processing std::streambuf

Use like this:
\code
char foo[5];
membuf mb(foo, sizeof(foo));
std::istream reader(&mb);
\endcode
*/
 

/*! \enum allocator_alignment
\brief Some preset alignment values for convenience
*/
enum class allocator_alignment : size_t
{
    Default = sizeof(void*), //!< The default alignment on this machine.
    SSE    = 16,			//!< The alignment for SSE. Better to use M128 for NEON et al support.
	M128   = 16,			//!< The alignment for a 128 bit vector.
    AVX    = 32,			//!< The alignment for AVX. Better to use M256 for NEON et al support.
	M256   = 32				//!< The alignment for a 256 bit vector.
};


namespace detail {
#ifdef WIN32
	extern "C" void *_aligned_malloc(size_t size, size_t alignment);
	extern "C" void _aligned_free(void *blk);
#else
	extern "C" int posix_memalign(void **memptr, size_t alignment, size_t size);
#endif
    inline void* allocate_aligned_memory(size_t align, size_t size)
	{
#ifdef WIN32
		return _aligned_malloc(size, align);
#else
		void *ret=nullptr;
		if(posix_memalign(&ret, align, size)) return nullptr;
		return ret;
#endif
	}
    inline void deallocate_aligned_memory(void* ptr) noexcept
	{
#ifdef WIN32
		_aligned_free(ptr);
#else
		free(ptr);
#endif
	}
}

/*! \class aligned_allocator
\brief An STL allocator which allocates aligned memory

Stolen from http://stackoverflow.com/questions/12942548/making-stdvector-allocate-aligned-memory
*/
template <typename T, size_t Align=std::alignment_of<T>::value>
class aligned_allocator
{
public:
    typedef T         value_type;
    typedef T*        pointer;
    typedef const T*  const_pointer;
    typedef T& reference;
    typedef const T&  const_reference;
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
	enum { alignment=Align };

    typedef std::true_type propagate_on_container_move_assignment;

    template <class U>
    struct rebind { typedef aligned_allocator<U, Align> other; };

public:
    aligned_allocator() noexcept
    {}

    template <class U>
    aligned_allocator(const aligned_allocator<U, Align>&) noexcept
    {}

    size_type
    max_size() const noexcept
    { return (size_type(~0) - size_type(Align)) / sizeof(T); }

    pointer
    address(reference x) const noexcept
    { return std::addressof(x); }

    const_pointer
    address(const_reference x) const noexcept
    { return std::addressof(x); }

    pointer
    allocate(size_type n, typename aligned_allocator<void, Align>::const_pointer = 0)
    {
        const size_type alignment = static_cast<size_type>( Align );
        void* ptr = detail::allocate_aligned_memory(alignment , n * sizeof(T));
        if (ptr == nullptr) {
            throw std::bad_alloc();
        }

        return reinterpret_cast<pointer>(ptr);
    }

    void
    deallocate(pointer p, size_type) noexcept
    { return detail::deallocate_aligned_memory(p); }

#if !defined(_MSC_VER) || _MSC_VER>1700
    template <class U, class ...Args>
    void
    construct(U* p, Args&&... args)
    { ::new(reinterpret_cast<void*>(p)) U(std::forward<Args>(args)...); }
#else
	void construct( pointer p, const_reference val )
	{
		::new(reinterpret_cast<void*>(p)) T(val);
	}
#endif

    void
    destroy(pointer p)
    { (void) p; p->~T(); }
};

template <size_t Align> class aligned_allocator<void, Align>
{
public:
    typedef void         value_type;
    typedef void *  pointer;
    typedef const void *  const_pointer;
    typedef void reference;
    typedef const void  const_reference;
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
	enum { alignment=Align };
};
template <size_t Align> class aligned_allocator<const void, Align>
{
public:
    typedef const void         value_type;
    typedef const void*  pointer;
    typedef const void*  const_pointer;
    typedef void reference;
    typedef const void  const_reference;
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
	enum { alignment=Align };
};

template <typename T, size_t Align>
class aligned_allocator<const T, Align>
{
public:
    typedef T         value_type;
    typedef const T*  pointer;
    typedef const T*  const_pointer;
    typedef T& reference;
    typedef const T&  const_reference;
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
	enum { alignment=Align };

    typedef std::true_type propagate_on_container_move_assignment;

    template <class U>
    struct rebind { typedef aligned_allocator<U, Align> other; };

public:
    aligned_allocator() noexcept
    {}

    template <class U>
    aligned_allocator(const aligned_allocator<U, Align>&) noexcept
    {}

    size_type
    max_size() const noexcept
    { return (size_type(~0) - size_type(Align)) / sizeof(T); }

    const_pointer
    address(const_reference x) const noexcept
    { return std::addressof(x); }

    pointer
    allocate(size_type n, typename aligned_allocator<void, Align>::const_pointer = 0)
    {
        const size_type alignment = static_cast<size_type>( Align );
        void* ptr = detail::allocate_aligned_memory(alignment , n * sizeof(T));
        if (ptr == nullptr) {
            throw std::bad_alloc();
        }

        return reinterpret_cast<pointer>(ptr);
    }

    void
    deallocate(pointer p, size_type) noexcept
    { return detail::deallocate_aligned_memory(p); }

    template <class U, class ...Args>
    void
    construct(U* p, Args&&... args)
    { ::new(reinterpret_cast<void*>(p)) U(std::forward<Args>(args)...); }

    void
    destroy(pointer p)
    { p->~T(); }
};

template <typename T, size_t TAlign, typename U, size_t UAlign>
inline
bool
operator== (const aligned_allocator<T,TAlign>&, const aligned_allocator<U, UAlign>&) noexcept
{ return TAlign == UAlign; }

template <typename T, size_t TAlign, typename U, size_t UAlign>
inline
bool
operator!= (const aligned_allocator<T,TAlign>&, const aligned_allocator<U, UAlign>&) noexcept
{ return TAlign != UAlign; }


template<size_t padding> class PadSizeToMultipleOfImpl
{
	char __padding[padding];
};
template<> class PadSizeToMultipleOfImpl<0>
{
};
/*! \brief Rounds a type to a given multiple of a size
*/
template<class T, size_t sizemultiple=std::alignment_of<T>::value> struct PadSizeToMultipleOf : public T, private PadSizeToMultipleOfImpl<(sizemultiple-1+sizeof(T)) % sizemultiple>
{
public:
	PadSizeToMultipleOf() { }
	template<class A> PadSizeToMultipleOf(const A &o) : T(o) { }
	template<class A> PadSizeToMultipleOf(A &&o) : T(std::move(o)) { }
};

} // namespace


#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
