/* 
 * File:   Aligned_Allocator.hpp
 * Author: atlas
 *
 * Created on July 5, 2013, 6:52 PM
 */

#ifndef ALIGNED_ALLOCATOR_HPP
#define	ALIGNED_ALLOCATOR_HPP

//#include <cassert>

//#include <vector>
#include <memory>
#include <type_traits>
//#include <functional>
//#include <algorithm>
//nclude <map>
//#include <unordered_map>
#include <typeinfo>
//#include <string>

//#include <list>
//#include <streambuf>
//#include <ios>
//#include <iostream>
//#include <cstddef>
//#include <stdexcept>


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





namespace boost{
    namespace afio{
        namespace detail {
            
enum class allocator_alignment : size_t
{
    Default = sizeof(void*), //!< The default alignment on this machine.
    SSE    = 16,			//!< The alignment for SSE. Better to use M128 for NEON et al support.
	M128   = 16,			//!< The alignment for a 128 bit vector.
    AVX    = 32,			//!< The alignment for AVX. Better to use M256 for NEON et al support.
	M256   = 32				//!< The alignment for a 256 bit vector.
};      
            
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


        }//namespace detail
    }//namespace afio
}//namespace boost
#endif	/* ALIGNED_ALLOCATOR_HPP */
