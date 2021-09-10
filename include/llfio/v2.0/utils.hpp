/* Misc utilities
(C) 2015-2021 Niall Douglas <http://www.nedproductions.biz/> (8 commits)
File Created: Dec 2015


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License in the accompanying file
Licence.txt or at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


Distributed under the Boost Software License, Version 1.0.
    (See accompanying file Licence.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef LLFIO_UTILS_H
#define LLFIO_UTILS_H

#ifndef LLFIO_CONFIG_HPP
#error You must include the master llfio.hpp, not individual header files directly
#endif
#include "config.hpp"

#include "quickcpplib/algorithm/string.hpp"

//! \file utils.hpp Provides namespace utils

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace utils
{
  /*! \brief Returns the smallest page size of this architecture which is useful for calculating direct i/o multiples.

  \return The page size of this architecture.
  \ingroup utils
  \complexity{Whatever the system API takes (one would hope constant time).}
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC size_t page_size() noexcept;

  /*! \brief Round a value to its next lowest page size multiple
   */
  template <class T> inline T round_down_to_page_size(T i, size_t pagesize) noexcept
  {
    assert(pagesize > 0);
    i = (T)(LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<uintptr_t>(i) & ~(pagesize - 1));  // NOLINT
    return i;
  }
  /*! \brief Round a value to its next highest page size multiple
   */
  template <class T> inline T round_up_to_page_size(T i, size_t pagesize) noexcept
  {
    assert(pagesize > 0);
    i = (T)((LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<uintptr_t>(i) + pagesize - 1) & ~(pagesize - 1));  // NOLINT
    return i;
  }
  /*! \brief Round a pair of a pointer and a size_t to their nearest page size multiples. The pointer will be rounded
  down, the size_t upwards.
  */
  LLFIO_TEMPLATE(class T)
  LLFIO_TREQUIRES(LLFIO_TEXPR(std::declval<T>().data()), LLFIO_TEXPR(std::declval<T>().size()))
  inline T round_to_page_size_larger(T i, size_t pagesize) noexcept
  {
    assert(pagesize > 0);
    const auto base = LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<uintptr_t>(i.data());
    i = {reinterpret_cast<byte *>(base & ~(pagesize - 1)), ((base + i.size() + pagesize - 1) & ~(pagesize - 1)) - (base & ~(pagesize - 1))};
    return i;
  }
  /*! \brief Round a pair of a pointer and a size_t to their nearest page size multiples. The pointer will be rounded
  upwards, the size_t downwards.
  */
  LLFIO_TEMPLATE(class T)
  LLFIO_TREQUIRES(LLFIO_TEXPR(std::declval<T>().data()), LLFIO_TEXPR(std::declval<T>().size()))
  inline T round_to_page_size_smaller(T i, size_t pagesize) noexcept
  {
    assert(pagesize > 0);
    const auto base = LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<uintptr_t>(i.data());
    i = {reinterpret_cast<byte *>((base + pagesize - 1) & ~(pagesize - 1)), ((base + i.size()) & ~(pagesize - 1)) - ((base + pagesize - 1) & ~(pagesize - 1))};
    return i;
  }
  /*! \brief Round a pair of values to their nearest page size multiples. The first will be rounded
  down, the second upwards.
  */
  template <class A, class B> inline std::pair<A, B> round_to_page_size_larger(std::pair<A, B> i, size_t pagesize) noexcept
  {
    assert(pagesize > 0);
    const auto base = LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<uintptr_t>(i.first);
    i = {static_cast<A>(base & ~(pagesize - 1)), static_cast<B>(((base + i.second + pagesize - 1) & ~(pagesize - 1)) - (base & ~(pagesize - 1)))};
    return i;
  }
  /*! \brief Round a pair of values to their nearest page size multiples. The first will be rounded
  upwards, the second downwards.
  */
  template <class A, class B> inline std::pair<A, B> round_to_page_size_smaller(std::pair<A, B> i, size_t pagesize) noexcept
  {
    assert(pagesize > 0);
    const auto base = LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<uintptr_t>(i.first);
    i = {static_cast<A>((base + pagesize - 1) & ~(pagesize - 1)),
         static_cast<B>(((base + i.second) & ~(pagesize - 1)) - ((base + pagesize - 1) & ~(pagesize - 1)))};
    return i;
  }

  /*! \brief Returns the page sizes of this architecture which is useful for calculating direct i/o multiples.

  \param only_actually_available Only return page sizes actually available to the user running this process
  \return The page sizes of this architecture.
  \ingroup utils
  \complexity{First call performs multiple memory allocations, mutex locks and system calls. Subsequent calls
  lock mutexes.}
  \exceptionmodel{Throws any error from the operating system or std::bad_alloc.}
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC const std::vector<size_t> &page_sizes(bool only_actually_available = true);

  /*! \brief Returns a reasonable default size for page_allocator, typically the closest page size from
  page_sizes() to 1Mb.

  \return A value of a TLB large page size close to 1Mb.
  \ingroup utils
  \complexity{Whatever the system API takes (one would hope constant time).}
  \exceptionmodel{Throws any error from the operating system or std::bad_alloc.}
  */
  inline size_t file_buffer_default_size()
  {
    static size_t size;
    if(size == 0u)
    {
      const std::vector<size_t> &sizes = page_sizes(true);
      for(auto &i : sizes)
      {
        if(i >= 1024 * 1024)
        {
          size = i;
          break;
        }
      }
      if(size == 0u)
      {
        size = 1024 * 1024;
      }
    }
    return size;
  }

  /*! \brief Fills the buffer supplied with cryptographically strong randomness. Uses the OS kernel API.

  \param buffer A buffer to fill
  \param bytes How many bytes to fill
  \ingroup utils
  \complexity{Whatever the system API takes.}
  \exceptionmodel{Any error from the operating system.}
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC void random_fill(char *buffer, size_t bytes) noexcept;

  /*! \brief Returns a cryptographically random string capable of being used as a filename. Essentially random_fill() + to_hex_string().

  \param randomlen The number of bytes of randomness to use for the string.
  \return A string representing the randomness at a 2x ratio, so if 32 bytes were requested, this string would be 64 bytes long.
  \ingroup utils
  \complexity{Whatever the system API takes.}
  \exceptionmodel{Any error from the operating system.}
  */
  inline std::string random_string(size_t randomlen)
  {
    size_t outlen = randomlen * 2;
    std::string ret(outlen, 0);
    random_fill(const_cast<char *>(ret.data()), randomlen);
    QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string(const_cast<char *>(ret.data()), outlen, ret.data(), randomlen);
    return ret;
  }

  /*! \brief Tries to flush all modified data to the physical device.
   */
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<void> flush_modified_data() noexcept;

  /*! \brief Tries to flush all modified data to the physical device, and then drop the OS filesystem cache,
  thus making all future reads come from the physical device. Currently only implemented for Microsoft Windows and Linux.

  Note that the OS specific magic called by this routine generally requires elevated privileges for the calling process.
  For obvious reasons, calling this will have a severe negative impact on performance, but it's very useful for
  benchmarking cold cache vs warm cache performance.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<void> drop_filesystem_cache() noexcept;

#ifndef _WIN32
  /*! \brief Returns true if this POSIX is running under Microsoft's Subsystem for Linux.
   */
  LLFIO_HEADERS_ONLY_FUNC_SPEC bool running_under_wsl() noexcept;
#endif

  /*! \brief Memory usage statistics for a process.
   */
  struct process_memory_usage
  {
    //! Fields wanted
    QUICKCPPLIB_BITFIELD_BEGIN(want){
    total_address_space_in_use = 1U << 0U,
    total_address_space_paged_in = 1U << 1U,
    private_committed = 1U << 2U,
    private_paged_in = 1U << 3U,

    private_committed_inaccurate = 1U << 8U,

    system_physical_memory_total = 1U << 16U,
    system_physical_memory_available = 1U << 17U,
    system_commit_charge_maximum = 1U << 18U,
    system_commit_charge_available = 1U << 19U,

    this_process = 0x0000ffff,  //
    this_system = 0xffff0000,   //
    all = 0xffffffff            //
    } QUICKCPPLIB_BITFIELD_END(want)

    //! The total physical memory in this system.
    uint64_t system_physical_memory_total{0};
    //! The physical memory in this system not containing dirty pages i.e. is currently used for file system caching, or unused.
    uint64_t system_physical_memory_available{0};

    //! The maximum amount of memory which can be committed by all processes. This is typically physical RAM plus swap files. Note that swap files can be added
    //! and removed over time.
    uint64_t system_commit_charge_maximum{0};
    //! The amount of commit charge remaining before the maximum. Subtract this from `system_commit_charge_maximum` to determine the amount of commit charge
    //! consumed by all processes in the system.
    uint64_t system_commit_charge_available{0};

    //! The total virtual address space in use.
    size_t total_address_space_in_use{0};
    //! The total memory currently paged into the process. Always `<= total_address_space_in_use`. Also known as "working set", or "resident set size including
    //! shared".
    size_t total_address_space_paged_in{0};

    //! The total anonymous memory committed. Also known as "commit charge".
    size_t private_committed{0};
    //! The total anonymous memory currently paged into the process. Always `<= private_committed`. Also known as "active anonymous pages".
    size_t private_paged_in{0};
  };
  static_assert(std::is_trivially_copyable<process_memory_usage>::value, "process_memory_usage is not trivially copyable!");

  /*! \brief Retrieve the current memory usage statistics for this process.

  Be aware that because Linux provides no summary counter for `private_committed`, we
  have to manually parse through `/proc/pid/smaps` to calculate it. This can start to
  take seconds for a process with a complex virtual memory space. If you are sure that
  you never use `section_handle::flag::nocommit` without `section_handle::flag::none`
  (i.e. you don't nocommit accessible memory), then specifying the flag
  `process_memory_usage::want::private_committed_inaccurate` can yield significant
  performance gains. If you set `process_memory_usage::want::private_committed_inaccurate`,
  we use `/proc/pid/smaps_rollup` and `/proc/pid/maps` to calculate the results. This
  cannot distinguish between regions with the accounted flag enabled or disabled, and
  be aware that glibc's `malloc()` for some inexplicable reason doesn't set the
  accounted flag on regions it commits, so the inaccurate flag will always yield
  higher numbers for private commited on Linux. By default, this fast path is enabled.

  \note `/proc/pid/smaps_rollup` was added in Linux kernel 3.16, so the default specifying
  `process_memory_usage::want::private_committed_inaccurate` will always fail on Linux
  kernels preceding that with an error code comparing equal to `errc::operation_not_supported`.
  As one would assume users would prefer this operation to fail on older kernels rather than
  silently go slowly in complex memory spaces, it is left opt-in to request
  the accurate implementation which works on older Linux kernels. Or, just don't request
  `private_committed` at all, and pretend `private_paged_in` means the same thing.

  \note Mac OS provides no way of reading how much memory a process has committed.
  We therefore supply as `private_committed` the same value as `private_paged_in`.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<process_memory_usage>
  current_process_memory_usage(process_memory_usage::want want = process_memory_usage::want::this_process) noexcept;

  /*! \brief CPU usage statistics for a process.
   */
  struct process_cpu_usage
  {
    //! The amount of nanoseconds all processes ever have spent in user mode.
    uint64_t system_ns_in_user_mode{0};
    //! The amount of nanoseconds all processes ever have spent in kernel mode.
    uint64_t system_ns_in_kernel_mode{0};
    //! The amount of nanoseconds all processes ever have spent in idle mode.
    uint64_t system_ns_in_idle_mode{0};

    //! The amount of nanoseconds this process has spent in user mode.
    uint64_t process_ns_in_user_mode{0};
    //! The amount of nanoseconds this process has spent in kernel mode.
    uint64_t process_ns_in_kernel_mode{0};

    //! Subtracts an earlier result from a later result.
    process_cpu_usage operator-(const process_cpu_usage &o) const noexcept
    {
      return {system_ns_in_user_mode - o.system_ns_in_user_mode, system_ns_in_kernel_mode - o.system_ns_in_kernel_mode,
              system_ns_in_idle_mode - o.system_ns_in_idle_mode, process_ns_in_user_mode - o.process_ns_in_user_mode,
              process_ns_in_kernel_mode - o.process_ns_in_kernel_mode};
    }
    //! Subtracts an earlier result from a later result.
    process_cpu_usage &operator-=(const process_cpu_usage &o) noexcept
    {
      system_ns_in_user_mode -= o.system_ns_in_user_mode;
      system_ns_in_kernel_mode -= o.system_ns_in_kernel_mode;
      system_ns_in_idle_mode -= o.system_ns_in_idle_mode;
      process_ns_in_user_mode -= o.process_ns_in_user_mode;
      process_ns_in_kernel_mode -= o.process_ns_in_kernel_mode;
      return *this;
    }
  };
  static_assert(std::is_trivially_copyable<process_cpu_usage>::value, "process_cpu_usage is not trivially copyable!");

  /*! \brief Retrieve the current CPU usage statistics for this system and this process. These
  are unsigned counters which always increment, and so may eventually wrap.

  The simplest way to use this API is to call it whilst also taking the current monotonic
  clock/CPU TSC and then calculating the delta change over that period of time.

  \note The returned values may not be a snapshot accurate against one another as they
  may get derived from multiple sources. Also, granularity is probably either a lot more
  than one nanosecond on most platforms, but may be CPU TSC based on others (you can test
  it to be sure).

  \note Within some versions of Docker, the per-process counters are not available.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<process_cpu_usage> current_process_cpu_usage() noexcept;

  namespace detail
  {
    struct large_page_allocation
    {
      void *p{nullptr};
      size_t page_size_used{0};
      size_t actual_size{0};
      constexpr large_page_allocation() {}  // NOLINT
      constexpr large_page_allocation(void *_p, size_t pagesize, size_t actual)
          : p(_p)
          , page_size_used(pagesize)
          , actual_size(actual)
      {
      }
    };
    inline large_page_allocation calculate_large_page_allocation(size_t bytes)
    {
      large_page_allocation ret;
      auto pagesizes(page_sizes());
      do
      {
        ret.page_size_used = pagesizes.back();
        pagesizes.pop_back();
      } while(!pagesizes.empty() && ((bytes / ret.page_size_used) == 0u));
      ret.actual_size = (bytes + ret.page_size_used - 1) & ~(ret.page_size_used - 1);
      return ret;
    }
    LLFIO_HEADERS_ONLY_FUNC_SPEC large_page_allocation allocate_large_pages(size_t bytes);
    LLFIO_HEADERS_ONLY_FUNC_SPEC void deallocate_large_pages(void *p, size_t bytes);
  }  // namespace detail

  /*! \class page_allocator
  \brief An STL allocator which allocates large TLB page memory.
  \ingroup utils

  If the operating system is configured to allow it, this type of memory is
  particularly efficient for doing large scale file i/o. This is because the
  kernel must normally convert the scatter gather buffers you pass into
  extended scatter gather buffers as the memory you see as contiguous may not,
  and probably isn't, actually be contiguous in physical memory. Regions
  returned by this allocator \em may be allocated contiguously in physical
  memory and therefore the kernel can pass through your scatter gather buffers
  unmodified.

  A particularly useful combination with this allocator is with the
  `page_sizes()` member function of __llfio_dispatcher__. This will return which
  pages sizes are possible, and which page sizes are enabled for this user. If
  writing a file copy routine for example, using this allocator with the
  largest page size as the copy chunk makes a great deal of sense.

  Be aware that as soon as the allocation exceeds a large page size, most
  systems allocate in multiples of the large page size, so if the large page
  size were 2Mb and you allocate 2Mb + 1 byte, 4Mb is actually consumed.
  */
  template <typename T> class page_allocator
  {
  public:
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    template <class U> struct rebind
    {
      using other = page_allocator<U>;
    };

    constexpr page_allocator() noexcept {}  // NOLINT

    template <class U> page_allocator(const page_allocator<U> & /*unused*/) noexcept {}  // NOLINT

    size_type max_size() const noexcept { return size_type(~0U) / sizeof(T); }

    pointer address(reference x) const noexcept { return std::addressof(x); }

    const_pointer address(const_reference x) const noexcept { return std::addressof(x); }

    pointer allocate(size_type n, const void * /*unused*/ = nullptr)
    {
      if(n > max_size())
      {
        throw std::bad_alloc();
      }
      auto mem(detail::allocate_large_pages(n * sizeof(T)));
      if(mem.p == nullptr)
      {
        throw std::bad_alloc();
      }
      return reinterpret_cast<pointer>(mem.p);
    }

    void deallocate(pointer p, size_type n)
    {
      if(n > max_size())
      {
        throw std::bad_alloc();
      }
      detail::deallocate_large_pages(p, n * sizeof(T));
    }

    template <class U, class... Args> void construct(U *p, Args &&...args) { ::new(reinterpret_cast<void *>(p)) U(std::forward<Args>(args)...); }

    template <class U> void destroy(U *p) { p->~U(); }
  };
  template <> class page_allocator<void>
  {
  public:
    using value_type = void;
    using pointer = void *;
    using const_pointer = const void *;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    template <class U> struct rebind
    {
      using other = page_allocator<U>;
    };
  };
  template <class T, class U> inline bool operator==(const page_allocator<T> & /*unused*/, const page_allocator<U> & /*unused*/) noexcept { return true; }
}  // namespace utils

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/utils.ipp"
#else
#include "detail/impl/posix/utils.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif


#endif
