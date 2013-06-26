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
#include <type_traits>
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
struct membuf : public std::streambuf
{
	char *s;
	size_t n;
    membuf(char *_s, size_t _n) : s(_s), n(_n)
    {
        setg(s, s, s + n);
    }
protected:
	virtual pos_type seekoff( off_type off, std::ios_base::seekdir dir,
                          std::ios_base::openmode which = std::ios_base::in | std::ios_base::out )
	{
		(void) which;
		off_type offset=(std::ios::beg==dir) ? off : (std::ios::end==dir) ? n-off : (gptr()-s)+off;
		setg(s, s+offset, s+n);
		return gptr()-s;
	}
};

namespace Impl {
	template<typename T, bool iscomparable> struct is_nullptr { bool operator()(T c) const noexcept { return !c; } };
	template<typename T> struct is_nullptr<T, false> { bool operator()(T) const noexcept { return false; } };
}
//! Compile-time safe detector of if \em v is nullptr (can cope with non-pointer convertibles)
#if defined(__GNUC__) && GCC_VERSION<40900
template<typename T> bool is_nullptr(T v) noexcept { return Impl::is_nullptr<T, std::is_constructible<bool, T>::value>()(std::forward<T>(v)); }
#else
template<typename T> bool is_nullptr(T v) noexcept { return Impl::is_nullptr<T, std::is_trivially_constructible<bool, T>::value>()(std::forward<T>(v)); }
#endif

namespace Impl {
	template<bool isTemplated, typename T> struct has_regular_call_operator
	{
	  template<typename C> // detect regular operator()
	  static char test(decltype(&C::operator()));

	  template<typename C> // worst match
	  static char (&test(...))[2];

	  static constexpr bool value = (sizeof( test<T>(0)  ) == 1);
	};

	template<typename T> struct has_regular_call_operator<true,T>
	{
	  static constexpr bool value = true;
	};
}

template<typename T> struct has_call_operator
{
  template<typename F, typename A> // detect 1-arg operator()
  static char test(int, decltype( (*(F*)0)( (*(A*)0) ) ) = 0);

  template<typename F, typename A, typename B> // detect 2-arg operator()
  static char test(int, decltype( (*(F*)0)( (*(A*)0), (*(B*)0) ) ) = 0);

  template<typename F, typename A, typename B, typename C> // detect 3-arg operator()
  static char test(int, decltype( (*(F*)0)( (*(A*)0), (*(B*)0), (*(C*)0) ) ) = 0);

  template<typename F, typename ...Args> // worst match
  static char (&test(...))[2];

  static constexpr bool OneArg = (sizeof( test<T, int>(0)  ) == 1);
  static constexpr bool TwoArg = (sizeof( test<T, int, int>(0)  ) == 1);
  static constexpr bool ThreeArg = (sizeof( test<T, int, int, int>(0)  ) == 1);

  static constexpr bool HasTemplatedOperator = OneArg || TwoArg || ThreeArg;
  static constexpr bool value = Impl::has_regular_call_operator<HasTemplatedOperator, T>::value;
};

namespace Impl
{
   template <size_t offset, typename F, typename Tuple, bool Done, int Total, int... N>
    struct call_using_tuple
    {
        static void call(F f, Tuple && t)
        {
            Impl::call_using_tuple<offset, F, Tuple, Total == 1 + sizeof...(N), Total, N..., sizeof...(N)>::call(std::forward<F>(f), std::forward<Tuple>(t));
        }
    };

    template <size_t offset, typename F, typename Tuple, int Total, int... N>
    struct call_using_tuple<offset, F, Tuple, true, Total, N...>
    {
        static void call(F f, Tuple && t)
        {
            f(std::get<offset + N>(std::forward<Tuple>(t))...);
        }
    };
}
/*! \brief Calls some callable unpacking a supplied std::tuple<> as args

Derived from http://stackoverflow.com/questions/10766112/c11-i-can-go-from-multiple-args-to-tuple-but-can-i-go-from-tuple-to-multiple
*/
template <typename F, typename Tuple> void call_using_tuple(F f, Tuple &&t)
{
    typedef typename std::decay<Tuple>::type ttype;
    Impl::call_using_tuple<0, F, Tuple, 0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value>::call(std::forward<F>(f), std::forward<Tuple>(t));
}
template <size_t offset, typename F, typename Tuple, typename... Args> void call_using_tuple(F f, Tuple &&t)
{
    typedef typename std::decay<Tuple>::type ttype;
    Impl::call_using_tuple<offset, F, Tuple, 0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value-offset>::call(std::forward<F>(f), std::forward<Tuple>(t));
}
/*
template<typename callable> class UndoerImpl
{
	callable undoer;
	bool _dismissed;

#if !defined(_MSC_VER) || _MSC_VER>1700
	UndoerImpl() = delete;
	UndoerImpl(const UndoerImpl &) = delete;
	UndoerImpl &operator=(const UndoerImpl &) = delete;
#else
	UndoerImpl();
	UndoerImpl(const UndoerImpl &);
	UndoerImpl &operator=(const UndoerImpl &);
#endif
	explicit UndoerImpl(callable &&c) : undoer(std::move(c)), _dismissed(false) { }
	void int_trigger() { if(!_dismissed && !is_nullptr(undoer)) { undoer(); _dismissed=true; } }
public:
	UndoerImpl(UndoerImpl &&o) : undoer(std::move(o.undoer)), _dismissed(o._dismissed) { o._dismissed=true; }
	UndoerImpl &operator=(UndoerImpl &&o) { int_trigger(); undoer=std::move(o.undoer); _dismissed=o._dismissed; o._dismissed=true; return *this; }
	template<typename _callable> friend UndoerImpl<_callable> Undoer(_callable c);
	~UndoerImpl() { int_trigger(); }
	//! Returns if the Undoer is dismissed
	bool dismissed() const { return _dismissed; }
	//! Dismisses the Undoer
	void dismiss(bool d=true) { _dismissed=d; }
	//! Undismisses the Undoer
	void undismiss(bool d=true) { _dismissed=!d; }
};


/*! \brief Alexandrescu style rollbacks, a la C++ 11.

Example of usage:
\code
auto resetpos=Undoer([&s]() { s.seekg(0, std::ios::beg); });
...
resetpos.dismiss();
\endcode

template<typename callable> inline UndoerImpl<callable> Undoer(callable c)
{
	//static_assert(!std::is_function<callable>::value && !std::is_member_function_pointer<callable>::value && !std::is_member_object_pointer<callable>::value && !has_call_operator<callable>::value, "Undoer applied to a type not providing a call operator");
	auto foo=UndoerImpl<callable>(std::move(c));
	return foo;
}*/


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

template<class T> struct TextDumpImpl
{
	const T *inst;
	TextDumpImpl(const T &_inst) : inst(&_inst) { }
};
/*! \brief A textual dumper of types

Use this to signify that you wish to dump a textual representation of the given object instance
to a std::ostream, like this:
\code
s << TextDump(obj) << std::endl;
\endcode

To add text dumpers for your particular type, overload operator<< as follows:
\code
template<class _registry, class _type, class _containertype> inline std::ostream &operator<<(std::ostream &s, const TextDumpImpl<StaticTypeRegistry<_registry, _type, _containertype>> &v)
{
	for(const auto &i : *v.inst)
		s << "* " << i << std::endl;
	return s;
}
\endcode

An alternative is to provide a textDump() method for your object like this:
\code
struct Foo
{
	std::ostream &textDump(std::ostream &s) const;
};
\endcode
*/
template<class T> inline TextDumpImpl<T> TextDump(const T &_inst) { return TextDumpImpl<T>(_inst); }
//! Default stream overloader for TextDump
template<class T> inline std::ostream &operator<<(std::ostream &s, const TextDumpImpl<T> &v) { return v.inst->textDump(s); }


namespace Impl {
	typedef std::unordered_map<size_t, std::map<std::string, void *>> ErasedTypeRegistryMapType;
	extern NIALLSCPP11UTILITIES_API std::shared_ptr<ErasedTypeRegistryMapType> get_static_type_registry_storage();

	template<class _registry, class _type, class _containertype> struct StaticTypeRegistryStorage
	{
		typedef _registry registry;
		typedef _type type;
		typedef _containertype containertype;
		static containertype **registryStorage()
		{
			static containertype **_registryStorage; // Keep a local cache
			if(!_registryStorage)
			{
				const std::type_info &typeinfo=typeid(containertype);
                // Holds a shared pointer until static deinit
                static std::shared_ptr<ErasedTypeRegistryMapType> static_type_registry_storage(get_static_type_registry_storage());
				auto &typemap=(*static_type_registry_storage)[typeinfo.hash_code()];
				auto &containerstorage=typemap[typeinfo.name()];
				if(!containerstorage)
				{
					auto container=new containertype();
					containerstorage=static_cast<void *>(container);
					//assert(typemap[typeinfo.name()]==static_cast<void *>(container));
				}
				_registryStorage=(containertype **) &containerstorage;
			}
			return _registryStorage;
		}
		static void RegisterData(const type &c)
		{
			(*registryStorage())->push_back(c);
		}
		static void RegisterData(type &&c)
		{
			(*registryStorage())->push_back(std::move(c));
		}
		static void UnregisterData(const type &c)
		{
			auto _r=registryStorage();
			auto r=*_r;
			// Quick optimisation for tail pop to avoid a search
			if(*r->rbegin()==c)
				r->erase(--r->end());
			else
				r->erase(std::remove(r->begin(), r->end(), c), r->end());
			if(r->empty())
			{
				delete r;
				*_r=nullptr;
			}
		}
	};
}
/*! \brief An iterable, statically stored registry of items associated with a type

Only one of these ever exists in the process, so you can always iterate like this:
\code
typedef StaticTypeRegistry<Foo, std::unique_ptr<Foo>(*)()> MakeablesRegistry;
for(auto n : MakeablesRegistry())
   ...
\endcode

To use this you must compile StaticTypeRegistry.cpp.

\sa NiallsCPP11Utilities::RegisterData(), NiallsCPP11Utilities::AutoDataRegistration()
*/
template<class _registry, class _type, class _containertype=std::vector<_type>> struct StaticTypeRegistry
{
private:
	_containertype &__me() { auto r=Impl::StaticTypeRegistryStorage<_registry, _type, _containertype>::registryStorage(); return **r; }
	const _containertype &__me() const { auto r=Impl::StaticTypeRegistryStorage<_registry, _type, _containertype>::registryStorage(); return **r; }
public:
	operator _containertype &() { return __me(); }
	operator const _containertype &() const { return __me(); }
	typename _containertype::iterator begin() { return __me().begin(); }
	typename _containertype::const_iterator begin() const { return __me().begin(); }
	typename _containertype::const_iterator cbegin() const { return __me().cbegin(); }
	typename _containertype::iterator end() { return __me().end(); }
	typename _containertype::const_iterator end() const { return __me().end(); }
	typename _containertype::const_iterator cend() const { return __me().cend(); }
	typename _containertype::iterator rbegin() { return __me().rbegin(); }
	typename _containertype::const_iterator rbegin() const { return __me().rbegin(); }
	typename _containertype::iterator rend() { return __me().rend(); }
	typename _containertype::const_iterator rend() const { return __me().rend(); }
	typename _containertype::size_type size() const { return __me().size(); }
	typename _containertype::size_type max_size() const { return __me().max_size(); }
	bool empty() const { return __me().empty(); }
};
template<class _registry, class _type, class _containertype> inline std::ostream &operator<<(std::ostream &s, const TextDumpImpl<StaticTypeRegistry<_registry, _type, _containertype>> &v)
{
	for(const auto &i : *v.inst)
		s << "* " << i << std::endl;
	return s;
}

namespace Impl {
	template<class typeregistry> struct RegisterDataImpl;
	template<class _registry, class _type, class _containertype> struct RegisterDataImpl<StaticTypeRegistry<_registry, _type, _containertype>>
	{
		typedef _registry registry;
		typedef _type type;
		typedef _containertype containertype;
		static void Do(_type &&v)
		{
			Impl::StaticTypeRegistryStorage<_registry, _type, _containertype>::RegisterData(std::forward<_type>(v));
		}
	};
	template<class typeregistry> struct UnregisterDataImpl;
	template<class _registry, class _type, class _containertype> struct UnregisterDataImpl<StaticTypeRegistry<_registry, _type, _containertype>>
	{
		typedef _registry registry;
		typedef _type type;
		typedef _containertype containertype;
		static void Do(const _type &v)
		{
			Impl::StaticTypeRegistryStorage<_registry, _type, _containertype>::UnregisterData(v);
		}
	};
}
//! Registers a piece of data with the specified type registry
template<class typeregistry> inline void RegisterData(const typename Impl::RegisterDataImpl<typeregistry>::type &v)
{
	Impl::RegisterDataImpl<typeregistry>::Do(v);
}
//! Registers a piece of data with the specified type registry
template<class typeregistry> inline void RegisterData(typename Impl::RegisterDataImpl<typeregistry>::type &&v)
{
	Impl::RegisterDataImpl<typeregistry>::Do(std::forward<typename Impl::RegisterDataImpl<typeregistry>::type>(v));
}
//! Unregisters a piece of data with the specified type registry
template<class typeregistry> inline void UnregisterData(const typename Impl::UnregisterDataImpl<typeregistry>::type &v)
{
	Impl::UnregisterDataImpl<typeregistry>::Do(v);
}

namespace Impl {
	template<class _typeregistry> struct DataRegistration;
	template<class _registry, class _type, class _containertype> struct DataRegistration<StaticTypeRegistry<_registry, _type, _containertype>>
	{
		typedef StaticTypeRegistry<_registry, _type, _containertype> _typeregistry;
		DataRegistration(_type &&_c) : c(std::move(_c)) { RegisterData<_typeregistry>(std::forward<_type>(c)); }
		~DataRegistration() { UnregisterData<_typeregistry>(std::forward<_type>(c)); }
	private:
		_type c;
	};
}
/*! \brief Auto registers a data item with a type registry. Typically used at static init/deinit time.

Per DLL object:
\code
typedef StaticTypeRegistry<Foo, std::unique_ptr<Foo>(*)()> MakeablesRegistry;
static auto reg=AutoDataRegistration<MakeablesRegistry>(&Goo::Make);
\endcode
This registers the Goo::Make callable with the registry MakeablesRegistry during DLL load. It also unregisters during DLL unload.

You now have a registry of static Make() methods associated with type MakeablesRegistry. To iterate:
\code
for(auto n : StaticTypeRegistry<MakeablesRegistry>())
   ...
\endcode
*/
template<class _typeregistry, class _type> inline Impl::DataRegistration<_typeregistry> AutoDataRegistration(_type &&c)
{
	return Impl::DataRegistration<_typeregistry>(std::move(c));
}

/*! \brief Information about mapped files in the process

This is not a fast call, on any system. On Linux and FreeBSD this call returns
a perfect snapshot - on Windows and Mac OS X, there is a slight possibility that
data returned is incomplete or contains spurious data as the set of mapped files
may change mid-traversal.

To use this you must compile MappedFileInfo.cpp and ErrorHandling.cpp.

\sa NiallsCPP11Utilities::FromCodePoint()
*/
struct NIALLSCPP11UTILITIES_API MappedFileInfo
{
	std::string path;				//!< Full path to the binary.
	size_t startaddr, endaddr;		//!< Start and end addresses of where it's mapped to
	size_t offset;					//!< From which offset in the file
	size_t length;					//!< Length of mapped section (basically \c endaddr-startaddr)
	bool read, write, execute, copyonwrite;	//!< Reflecting if the section is readable, writeable, executable and/or copy-on-write

	bool operator<(const MappedFileInfo &o) const noexcept { return startaddr<o.startaddr; }
	bool operator==(const MappedFileInfo &o) const noexcept { return startaddr==o.startaddr && endaddr==o.endaddr && read==o.read && write==o.write && execute==o.execute && copyonwrite==o.copyonwrite && path==o.path; }
	//! Returns a snapshot of mapped sections in the process
	static std::map<size_t, MappedFileInfo> mappedFiles();
	//! Returns a text dump of this item
	std::ostream &textDump(std::ostream &s) const
	{
		s << std::hex << startaddr << "-" << endaddr << " ";
		s << (read ? 'R' : 'r') << (write ? 'W' : 'w') << (execute ? 'X' : 'x') << (copyonwrite ? 'C' : 'c');
		s << " +" << offset << " : " << path << std::endl;
		return s;
	}
};
//! Text dumps a std::map<size_t, MappedFileInfo>
inline std::ostream &operator<<(std::ostream &s, const TextDumpImpl<std::map<size_t, MappedFileInfo>> &v)
{
	for(const auto &i : *v.inst)
		s << "   " << TextDump(i.second);
	return s;
}
//! \brief Finds the MappedFileInfo containing code point \em codepoint, if any
template<class R, class... Pars> inline std::map<size_t, MappedFileInfo>::const_iterator FromCodePoint(const std::map<size_t, MappedFileInfo> &list, R(*codepoint)(Pars...))
{
	size_t addr=(size_t)(void *)codepoint;
	auto it=list.lower_bound(addr);
	if(it->first>addr) --it;
	if(it->second.startaddr<=addr && it->second.endaddr>addr)
		return it;
	return list.cend();
}

#if !DISABLE_SYMBOLMANGLER
//! The type of a symbol type
enum class SymbolTypeType
{
	Constant,
	Void,
	Bool,
	Char,
	SignedChar,
	UnsignedChar,
	ShortInt,
	UnsignedShortInt,
	Int,
	UnsignedInt,
	LongInt,
	UnsignedLongInt,
	LongLong,
	UnsignedLongLong,
	Wchar_t,
	Float,
	Double,
	LongDouble,
	Vect64,
	Vect128f,
	Vect128d,
	Vect128i,
	Vect256f,
	Vect256d,
	Vect256i,
	Varargs,

	// The following have names
	Namespace,
	Union,
	Struct,
	Class,
	Enum,
	EnumMember,

	Function,
	StaticMemberFunction,
	StaticMemberFunctionProtected,
	StaticMemberFunctionPrivate,
	MemberFunction,
	MemberFunctionProtected,
	MemberFunctionPrivate,
	VirtualMemberFunction,
	VirtualMemberFunctionProtected,
	VirtualMemberFunctionPrivate,

	VTable,
	Unknown
};
//! The qualifiers of a symbol type
enum class SymbolTypeQualifier
{
	None,
	Const,
	Pointer,
	ConstPointer,
	VolatilePointer,
	ConstVolatilePointer,
	PointerConst,
	PointerVolatile,
	PointerConstVolatile,
	ConstPointerConst,
	PointerRestrict,
	LValueRef,
	RValueRef,
	ConstLValueRef,
	VolatileLValueRef,
	ConstVolatileLValueRef,

	Array,
	ConstArray,

	Unknown
};
//! The storage class of a symbol type
enum class SymbolTypeStorage
{
	None,
	Const,
	Volatile,
	ConstVolatile,
	Static,
	StaticConst,
	StaticVolatile,
	StaticConstVolatile,

	Unknown
};
//! C++ operator type
enum class SymbolTypeOperator
{
	None,
	Constructor,
	Destructor,
	Index,
	Call,
	Indirect,
	PreIncrement,
	PostIncrement,
	PreDecrement,
	PostDecrement,
	New,
	NewArray,
	Delete,
	DeleteArray,
	Dereference,
	AddressOf,
	Positive,
	Negative,
	BooleanNot,
	BitwiseNot,
	DereferencePointer,
	Multiply,
	Divide,
	Modulus,
	Add,
	Subtract,
	ShiftLeft,
	ShiftRight,
	LessThan,
	GreaterThan,
	LessThanEqual,
	GreaterThanEqual,
	Equal,
	NotEqual,
	BitwiseAnd,
	BitwiseOr,
	BitwiseXOR,
	And,
	Or,
	Assign,
	MultiplyAssign,
	DivideAssign,
	ModulusAssign,
	AddAssign,
	SubtractAssign,
	ShiftLeftAssign,
	ShiftRightAssign,
	BitwiseAndAssign,
	BitwiseOrAssign,
	BitwiseXORAssign,
	Comma,
	Cast
};
/*! \brief A type potentially containing other types

To use this you must compile SymbolMangler.cpp which depends on Boost.MPL and Boost.Spirit.

\sa NiallsCPP11Utilities::Demangle(), NiallsCPP11Utilities::Mangle()
*/
struct NIALLSCPP11UTILITIES_API SymbolType
{
	SymbolTypeStorage storage;					//!< The storage class of the variable, or the type returned by a function
	const SymbolType *returns;					//!< The type returned, if a type is a function type
	SymbolTypeQualifier qualifier;				//!< The qualifier of the type (const, volatile, pointer etc)
	int indirectioncount;						//!< The number of indirections (e.g. void ** is 2)
	SymbolTypeType type;						//!< The type of the type (int, struct, namespace etc)
	std::list<const SymbolType *> dependents;	//!< The dependent types of the type (namespaces, member functions)
	std::string name;							//!< The name of the type (union/struct/class/enum/functions)
	std::list<const SymbolType *> templ_params;	//!< The template parameters of the type
	std::list<const SymbolType *> func_params;	//!< The parameters of the function type
	SymbolType() : storage(SymbolTypeStorage::Unknown), returns(nullptr), qualifier(SymbolTypeQualifier::Unknown), indirectioncount(0), type(SymbolTypeType::Unknown) { }
	bool operator==(const SymbolType &o) const
	{
		return storage==o.storage && returns==o.returns && qualifier==o.qualifier && indirectioncount==o.indirectioncount && type==o.type
			&& dependents==o.dependents && name==o.name && templ_params==o.templ_params && func_params==o.func_params;
	}
	//! Constructor for a variable
	SymbolType(SymbolTypeStorage _storage, SymbolTypeType _type, const std::string &_name) : storage(_storage), returns(nullptr), qualifier(SymbolTypeQualifier::Unknown), indirectioncount(qualifier>=SymbolTypeQualifier::Pointer), type(_type), name(_name) { }
	//! Constructor for a type
	SymbolType(SymbolTypeQualifier _qualifiers, SymbolTypeType _type, const std::string &_name=std::string()) : storage(SymbolTypeStorage::Unknown), returns(nullptr), qualifier(_qualifiers), indirectioncount(qualifier>=SymbolTypeQualifier::Pointer), type(_type), name(_name) { }
	std::string prettyText(bool withTypeType=true) const;
};
} // namespace

namespace std { template<> struct hash<const NiallsCPP11Utilities::SymbolType> {
	size_t operator()(const NiallsCPP11Utilities::SymbolType &v) const {
		size_t ret=hash<int>()(static_cast<int>(v.qualifier)) ^ hash<int>()(static_cast<int>(v.type)) ^ hash<decltype(v.name)>()(v.name);
		if(v.returns) ret^=hash<size_t>()((size_t) v.returns);
		for(auto p : v.dependents)
			ret^=hash<size_t>()((size_t) p);
		for(auto p : v.templ_params)
			ret^=hash<size_t>()((size_t) p);
		for(auto p : v.func_params)
			ret^=hash<size_t>()((size_t) p);
		return ret;
	}
}; }

namespace NiallsCPP11Utilities {

//! A dictionary of known symbol types. Used to store types across mangles/demangles.
typedef std::unordered_map<std::string, const SymbolType> SymbolTypeDict;

namespace Private { struct SymbolDemangle; }
/*! \brief Holds state for a symbol demangle session

To use this you must compile SymbolMangler.cpp which depends on Boost.MPL and Boost.Spirit.
*/
class NIALLSCPP11UTILITIES_API SymbolDemangle
{
	Private::SymbolDemangle *p;
public:
	//! Constructs using an internal typedict
	SymbolDemangle();
	//! Constructs using an external typedict. Faster.
	SymbolDemangle(SymbolTypeDict &typedict);
	~SymbolDemangle();

	//! Returns the type dictionary used by this demangler sessions
	SymbolTypeDict &typedict() const;

	//! Sets the type dictionary used by this demangler sessions. Implies calling reset().
	void setTypedict(SymbolTypeDict &typedict);

	//! Resets the lists of demangled symbols
	void reset();

	//! Returns the raw set of mangled symbols and their demangled ASTs
	const std::unordered_map<std::string, SymbolType> &parsedSymbols() const;

	//! Returns the raw set of mangled symbols we failed to parse, their partially demangled ASTs and an error message
	const std::unordered_map<std::string, std::pair<SymbolType, std::string>> &failedParsedSymbols() const;

	//! Adds a demangle to the internal store, returning the item and true if successfully parsed
	std::pair<const SymbolType *, bool> demangle(const std::string &mangled);

	//! Returns a namespace/class/struct to mangled symbol map
	const std::unordered_multimap<std::string, std::string> &namespaces() const;
};

//! \brief Convenience overload which demangles a single mangled symbol, throwing an exception if it failed. Use the class if you're demangling more than one symbol.
inline std::string Demangle(const std::string &mangled, SymbolDemangle &demangler)
{
	auto ret=demangler.demangle(mangled);
	if(!ret.second)
		throw std::runtime_error("Mangled symbol '"+mangled+"' is malformed. Error was '"+demangler.failedParsedSymbols().at(mangled).second+"'");
	return ret.first->prettyText();
}
//! \brief Convenience overload which demangles a single mangled symbol, throwing an exception if it failed. Use the class if you're demangling more than one symbol.
inline std::string Demangle(const std::string &mangled)
{
	SymbolDemangle demangler;
	return Demangle(mangled, demangler);
}
//! \brief Convenience overload which demangles a single mangled symbol, returning any error message if it failed. Use the class if you're demangling more than one symbol.
inline std::pair<std::string, std::string> Demangle(const std::string &mangled, std::nothrow_t, SymbolDemangle &demangler)
{
	auto ret=demangler.demangle(mangled);
	if(!ret.second)
	{
		const auto &failed=demangler.failedParsedSymbols().at(mangled);
		return std::make_pair(failed.first.prettyText(), failed.second);
	}
	else
		return std::make_pair(ret.first->prettyText(), std::string());
}
//! \brief Convenience overload which demangles a single mangled symbol, returning any error message if it failed. Use the class if you're demangling more than one symbol.
inline std::pair<std::string, std::string> Demangle(const std::string &mangled, std::nothrow_t nt)
{
	SymbolDemangle demangler;
	return Demangle(mangled, nt, demangler);
}

#endif // DISABLE_SYMBOLMANGLER

} // namespace


#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
