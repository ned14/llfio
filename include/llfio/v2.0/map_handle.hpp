/* A handle to a source of mapped memory
(C) 2016-2018 Niall Douglas <http://www.nedproductions.biz/> (14 commits)
File Created: August 2016


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

#ifndef LLFIO_MAP_HANDLE_H
#define LLFIO_MAP_HANDLE_H

#include "file_handle.hpp"

//! \file map_handle.hpp Provides `map_handle`

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class section_handle
\brief A handle to a source of mapped memory.

There are two configurations of section handle, one where the user supplies the file backing for the
section, and the other where an internal file descriptor to an unnamed inode in a tmpfs or ramfs based
temporary directory is kept and managed. The latter is merely a convenience for creating an anonymous
source of memory which can be resized whilst preserving contents: see `algorithm::trivial_vector<T>`.

On Windows the native handle of this handle is that of the NT kernel section object. On POSIX it is
a cloned file descriptor of the backing storage if there is backing storage, else it will be the
aforementioned file descriptor to an unnamed inode.
*/
class LLFIO_DECL section_handle : public handle
{
public:
  using extent_type = handle::extent_type;
  using size_type = handle::size_type;

  //! The behaviour of the memory section
  QUICKCPPLIB_BITFIELD_BEGIN(flag){none = 0U,           //!< No flags
                                   read = 1U << 0U,     //!< Memory views can be read
                                   write = 1U << 1U,    //!< Memory views can be written
                                   cow = 1U << 2U,      //!< Memory views can be copy on written
                                   execute = 1U << 3U,  //!< Memory views can execute code

                                   nocommit = 1U << 8U,     //!< Don't allocate space for this memory in the system immediately
                                   prefault = 1U << 9U,     //!< Prefault, as if by reading every page, any views of memory upon creation.
                                   executable = 1U << 10U,  //!< The backing storage is in fact an executable program binary.
                                   singleton = 1U << 11U,   //!< A single instance of this section is to be shared by all processes using the same backing file.

                                   barrier_on_close = 1U << 16U,   //!< Maps of this section, if writable, issue a `barrier()` when destructed blocking until data (not metadata) reaches physical storage.
                                   nvram = 1U << 17U,              //!< This section is of non-volatile RAM.
                                   write_via_syscall = 1U << 18U,  //!< For file backed maps, `map_handle::write()` is implemented as a `write()` syscall to the file descriptor. This causes the map to be mapped read-only.

                                   page_sizes_1 = 1U << 24U,  //!< Use `utils::page_sizes()[1]` sized pages, or fail.
                                   page_sizes_2 = 2U << 24U,  //!< Use `utils::page_sizes()[2]` sized pages, or fail.
                                   page_sizes_3 = 3U << 24U,  //!< Use `utils::page_sizes()[3]` sized pages, or fail.

                                   // NOTE: IF UPDATING THIS UPDATE THE std::ostream PRINTER BELOW!!!

                                   readwrite = (read | write)};
  QUICKCPPLIB_BITFIELD_END(flag);

protected:
  file_handle *_backing{nullptr};
  file_handle _anonymous;
  flag _flag{flag::none};

public:
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~section_handle() override;
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override;
  //! Default constructor
  constexpr section_handle() {}  // NOLINT
  //! Construct a section handle using the given flags
  constexpr explicit section_handle(flag __flag)
      : _flag(__flag)
  {
  }
  //! Construct a section handle using the given native handle type for the section and the given i/o handle for the backing storage
  explicit section_handle(native_handle_type sectionh, file_handle *backing, file_handle anonymous, flag __flag)
      : handle(sectionh, handle::caching::all)
      , _backing(backing)
      , _anonymous(std::move(anonymous))
      , _flag(__flag)
  {
  }
  //! Implicit move construction of section_handle permitted
  constexpr section_handle(section_handle &&o) noexcept
      : handle(std::move(o))
      , _backing(o._backing)
      , _anonymous(std::move(o._anonymous))
      , _flag(o._flag)
  {
    o._backing = nullptr;
    o._flag = flag::none;
  }
  //! No copy construction (use `clone()`)
  section_handle(const section_handle &) = delete;
  //! Move assignment of section_handle permitted
  section_handle &operator=(section_handle &&o) noexcept
  {
    this->~section_handle();
    new(this) section_handle(std::move(o));
    return *this;
  }
  //! No copy assignment
  section_handle &operator=(const section_handle &) = delete;
  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(section_handle &o) noexcept
  {
    section_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  /*! \brief Create a memory section backed by a file.
  \param backing The handle to use as backing storage.
  \param maximum_size The initial size of this section, which cannot be larger than any backing file. Zero means to use `backing.maximum_extent()`.
  \param _flag How to create the section.

  \errors Any of the values POSIX dup(), open() or NtCreateSection() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<section_handle> section(file_handle &backing, extent_type maximum_size, flag _flag) noexcept;
  /*! \brief Create a memory section backed by a file.
  \param backing The handle to use as backing storage.
  \param bytes The initial size of this section, which cannot be larger than any backing file. Zero means to use `backing.maximum_extent()`.

  This convenience overload create a writable section if the backing file is writable, otherwise a read-only section.

  \errors Any of the values POSIX dup(), open() or NtCreateSection() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static result<section_handle> section(file_handle &backing, extent_type bytes = 0) noexcept { return section(backing, bytes, backing.is_writable() ? (flag::readwrite) : (flag::read)); }
  /*! \brief Create a memory section backed by an anonymous, managed file.
  \param bytes The initial size of this section. Cannot be zero.
  \param dirh Where to create the anonymous, managed file.
  \param _flag How to create the section.

  \errors Any of the values POSIX dup(), open() or NtCreateSection() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<section_handle> section(extent_type bytes, const path_handle &dirh = path_discovery::storage_backed_temporary_files_directory(), flag _flag = flag::read | flag::write) noexcept;

  //! Returns the memory section's flags
  flag section_flags() const noexcept { return _flag; }
  //! True if the section reflects non-volatile RAM
  bool is_nvram() const noexcept { return !!(_flag & flag::nvram); }
  //! Returns the borrowed handle backing this section, if any
  file_handle *backing() const noexcept { return _backing; }
  //! Sets the borrowed handle backing this section, if any
  void set_backing(file_handle *fh) noexcept { _backing = fh; }
  //! Returns the borrowed native handle backing this section
  native_handle_type backing_native_handle() const noexcept { return _backing != nullptr ? _backing->native_handle() : native_handle_type(); }
  //! Return the current length of the memory section.
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<extent_type> length() const noexcept;

  /*! Resize the current maximum permitted extent of the memory section to the given extent.
  \param newsize The new size of the memory section, which cannot be zero. Specify zero to use `backing.maximum_extent()`.
  This cannot exceed the size of any backing file used if that file is not writable.

  \errors Any of the values `NtExtendSection()` or `ftruncate()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<extent_type> truncate(extent_type newsize = 0) noexcept;
};
inline std::ostream &operator<<(std::ostream &s, const section_handle::flag &v)
{
  std::string temp;
  if(!!(v & section_handle::flag::read))
  {
    temp.append("read|");
  }
  if(!!(v & section_handle::flag::write))
  {
    temp.append("write|");
  }
  if(!!(v & section_handle::flag::cow))
  {
    temp.append("cow|");
  }
  if(!!(v & section_handle::flag::execute))
  {
    temp.append("execute|");
  }
  if(!!(v & section_handle::flag::nocommit))
  {
    temp.append("nocommit|");
  }
  if(!!(v & section_handle::flag::prefault))
  {
    temp.append("prefault|");
  }
  if(!!(v & section_handle::flag::executable))
  {
    temp.append("executable|");
  }
  if(!!(v & section_handle::flag::singleton))
  {
    temp.append("singleton|");
  }
  if(!!(v & section_handle::flag::barrier_on_close))
  {
    temp.append("barrier_on_close|");
  }
  if(!!(v & section_handle::flag::nvram))
  {
    temp.append("nvram|");
  }
  if(!!(v & section_handle::flag::write_via_syscall))
  {
    temp.append("write_via_syscall|");
  }
  if((v & section_handle::flag::page_sizes_3) == section_handle::flag::page_sizes_3)
  {
    temp.append("page_sizes_3|");
  }
  else if((v & section_handle::flag::page_sizes_2) == section_handle::flag::page_sizes_2)
  {
    temp.append("page_sizes_2|");
  }
  else if((v & section_handle::flag::page_sizes_1) == section_handle::flag::page_sizes_1)
  {
    temp.append("page_sizes_1|");
  }
  if(!temp.empty())
  {
    temp.resize(temp.size() - 1);
    if(std::count(temp.cbegin(), temp.cend(), '|') > 0)
    {
      temp = "(" + temp + ")";
    }
  }
  else
  {
    temp = "none";
  }
  return s << "llfio::section_handle::flag::" << temp;
}

//! \brief Constructor for `section_handle`
template <> struct construct<section_handle>
{
  file_handle &backing;
  section_handle::extent_type maximum_size = 0;
  section_handle::flag _flag = section_handle::flag::read | section_handle::flag::write;
  result<section_handle> operator()() const noexcept { return section_handle::section(backing, maximum_size, _flag); }
};

class mapped_file_handle;

/*! Lightweight inlined barrier which causes the CPU to write out all buffered writes and dirty cache lines
in the request to main memory.
\return The cache lines actually barriered. This may be empty. This function does not return an error.
\param req The range of cache lines to write barrier.
\param evict Whether to also evict the cache lines from CPU caches, useful if they will not be used again.

Upon return, one knows that memory in the returned buffer has been barriered
(it may be empty if there is no support for this operation in LLFIO, or if the current CPU does not
support this operation). You may find the `is_nvram()` observer of particular use here.
*/
inline io_handle::const_buffer_type nvram_barrier(io_handle::const_buffer_type req, bool evict = false) noexcept
{
  auto *tp = (io_handle::const_buffer_type::pointer)(((uintptr_t) req.data()) & ~63);
  io_handle::const_buffer_type ret{tp, (size_t)(req.data() + 63 + req.size() - tp) & ~63};
  if(memory_flush_none == mem_flush_stores(ret.data(), ret.size(), evict ? memory_flush_evict : memory_flush_retain))
  {
    ret = {tp, 0};
  }
  return ret;
}


/*! \class map_handle
\brief A handle to a memory mapped region of memory, either backed by the system page file or by a section.

An important concept to realise with mapped regions is that they can far exceed the size of their backing
storage. This allows one to reserve address space for a file which may grow in the future. This is how
`mapped_file_handle` is implemented to provide very fast memory mapped file i/o of a potentially growing
file.

The size you specify when creating the map handle is the address space reservation. The map's `length()`
will return the last known **valid** length of the mapped data i.e. the backing storage's length at the
time of construction. This length is used by `read()` and `write()` to prevent reading and writing off
the end of the mapped region. You can update this length to the backing storage's length using `update_map()`
up to the reservation limit.

You can attempt to modify the address space reservation after creation using `truncate()`. If successful,
this will be more efficient than tearing down the map and creating a new larger map.

The native handle returned by this map handle is always that of the backing storage, but closing this handle
does not close that of the backing storage, nor does releasing this handle release that of the backing storage.
Locking byte ranges of this handle is therefore equal to locking byte ranges in the original backing storage,
which can be very useful.

On Microsoft Windows, when mapping file content, you should try to always create the first map of that
file using a writable handle. See `mapped_file_handle` for more detail on this.

## Commit charge:

All virtual memory systems account for memory allocated, even if never used. This is known as "commit charge".
All virtual memory systems will not permit more pages to be committed than there is storage for them
between RAM and the swap files (except for Linux, where most distributions configure "over commit" in
the Linux kernel). This ensures that if the system gives you a committed memory page, you are hard
guaranteed that writing into it will not fail. Note that memory mapped files have storage backed by
their file contents, so except for pages written into and not yet flushed to storage, memory mapped files
usually do not contribute more than a few pages each to commit charge.

The system commit limit can be easily exceeded if programs commit a lot of memory that they never use.
To avoid this, for large allocations you should *reserve* pages which you don't expect to use immediately,
and *later* explicitly commit and decommit them. You can request pages not accounted against the system
commit charge using `flag::nocommit`. For portability, you should **always** combine `flag::nocommit`
with `flag::none`, indeed only Linux permits the allocation of usable pages which are not charged
against commit. All other platforms enforce that reserved pages must be unusable, and only pages
which are committed are usable.

Separate to whether a page is committed or not is whether it actually consumes resources or not.
Pages never written into are not stored by virtual memory systems, and much code when considering the
memory consumption of a process only considers the portion of the total commit charge which contains
modified pages. This makes sense, given the prevalence of code which commits memory it never uses,
however it also leads to anti-social outcomes such as Linux distributions enabling pathological
workarounds such as over commit and specialised OOM killers.

## Barriers:

`map_handle`, because it implements `io_handle`, implements `barrier()` in a very conservative way
to account for OS differences i.e. it calls `msync()`, and then the `barrier()` implementation for the backing file
(probably `fsync()` or equivalent on most platforms, which synchronises the entire file).

This is vast overkill if you are using non-volatile RAM, so a special *inlined* `nvram_barrier()` implementation
taking a single buffer and no other arguments is also provided as a free function. This calls the appropriate architecture-specific
instructions to cause the CPU to write all preceding writes out of the write buffers and CPU caches to main
memory, so for Intel CPUs this would be `CLWB <each cache line>; SFENCE;`. As this is inlined, it ought to
produce optimal code. If your CPU does not support the requisite instructions (or LLFIO has not added support),
and empty buffer will be returned to indicate that nothing was barriered, same as the normal `barrier()`
function.

## Large page support:

Large, huge, massive and super page support is available via the `section_handle::flag::page_sizes_N` flags.
Use these in combination with `utils::page_size()` to request allocations or maps which use different page sizes.

### Windows:

Firstly, explicit large page support is only available to processes and logged in users who have been assigned the
`SeLockMemoryPrivilege`. A default Windows installation assigns that privilege to nothing, so explicit
action will need to be taken to assign that privilege per Windows installation.

Secondly, as of Windows 10 1803, there is the large page size or the normal page size. There isn't (useful)
support for pages of any other size, as there is on other systems.

For allocating memory, large page allocation can randomly fail depending on what the system is feeling
like, same as on all the other operating systems. It is not permitted to reserve address space using large pages.

For mapping files, large page maps do not work as of Windows 10 1803 (curiously, ReactOS *does*
implement this). There is a big exception to this, which is for DAX formatted NTFS volumes with a formatted
cluster size of the large page size, where if
you map in large page sized multiples, the Windows kernel uses large pages (and one need not hold
`SeLockMemoryPrivilege` either). Therefore, if you specify `section_handle::flag::nvram` with a
`section_handle::flag::page_sizes_N`, LLFIO does not ask for large pages which would fail, it merely
rounds all requests up to the nearest large page multiple.

### Linux:

As usual on Linux, large page (often called huge page on Linux) support comes in many forms.

Explicit support is via `MAP_HUGETLB` to `mmap()`, and whether an explicit request succeeds or not is up to how
many huge pages were configured into the running system via boot-time kernel parameters, and how many
huge pages are in use already. For most recent kernels on most distributions, explicit memory allocation
requests using large pages generally works without issue. As of Linux kernel 4.18, mapping files using
large pages only works on `tmpfs`, this corresponds to `path_discovery::memory_backed_temporary_files_directory()`
sourced anonymous section handles. Work is proceeding well for the major Linux filing systems to become
able to map files using large pages soon, and in theory LLFIO based should "just work" on such a newer kernel.

Note that many distributions enable transparent huge pages, whereby if you request allocations of large page multiples
at large page offsets, the kernel uses large pages, without you needing to specify any `section_handle::flag::page_sizes_N`.

### FreeBSD:

FreeBSD has no support for failing if large pages cannot be used for a specific `mmap()`. The best you can do is to ask for
large pages, and you may or may not get them depending on available system resources, filing system in use,
etc. LLFIO does not check returned maps to discover if large
pages were actually used, that is on end user code to check if it really needs to know.

### MacOS:

MacOS only supports large pages for memory allocations, not for mapping files. It fails if large pages could
not be used when a large page allocation was requested.

\sa `mapped_file_handle`, `algorithm::mapped_span`
*/
class LLFIO_DECL map_handle : public lockable_io_handle
{
  friend class mapped_file_handle;

public:
  using extent_type = io_handle::extent_type;
  using size_type = io_handle::size_type;
  using mode = io_handle::mode;
  using creation = io_handle::creation;
  using caching = io_handle::caching;
  using flag = io_handle::flag;
  using buffer_type = io_handle::buffer_type;
  using const_buffer_type = io_handle::const_buffer_type;
  using buffers_type = io_handle::buffers_type;
  using const_buffers_type = io_handle::const_buffers_type;
  template <class T> using io_request = io_handle::io_request<T>;
  template <class T> using io_result = io_handle::io_result<T>;

protected:
  section_handle *_section{nullptr};
  byte *_addr{nullptr};
  extent_type _offset{0};
  size_type _reservation{0}, _length{0}, _pagesize{0};
  section_handle::flag _flag{section_handle::flag::none};

  explicit map_handle(section_handle *section, section_handle::flag flags)
      : _section(section)
      , _flag(flags)
  {
    if(section != nullptr)
    {
      // Apart from read/write/cow/execute, the section's flags override the map's flags
      _flag |= (section->section_flags() & ~(section_handle::flag::read | section_handle::flag::write | section_handle::flag::cow | section_handle::flag::execute));
    }
  }

public:
  //! Default constructor
  constexpr map_handle() {}  // NOLINT
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~map_handle() override;
  //! Construct an instance managing pages at `addr`, `length`, `pagesize` and `flags`
  explicit map_handle(byte *addr, size_type length, size_type pagesize, section_handle::flag flags, section_handle *section = nullptr, extent_type offset = 0) noexcept
      : _section(section)
      , _addr(addr)
      , _offset(offset)
      , _reservation(length)
      , _length(length)
      , _pagesize(pagesize)
      , _flag(flags)
  {
    _v._init = -2;  // otherwise appears closed
    _v.behaviour |= native_handle_type::disposition::allocation;
  }
  //! Implicit move construction of map_handle permitted
  constexpr map_handle(map_handle &&o) noexcept
      : lockable_io_handle(std::move(o))
      , _section(o._section)
      , _addr(o._addr)
      , _offset(o._offset)
      , _reservation(o._reservation)
      , _length(o._length)
      , _pagesize(o._pagesize)
      , _flag(o._flag)
  {
    o._section = nullptr;
    o._addr = nullptr;
    o._offset = 0;
    o._reservation = 0;
    o._length = 0;
    o._pagesize = 0;
    o._flag = section_handle::flag::none;
  }
  //! No copy construction (use `clone()`)
  map_handle(const map_handle &) = delete;
  //! Move assignment of map_handle permitted
  map_handle &operator=(map_handle &&o) noexcept
  {
    this->~map_handle();
    new(this) map_handle(std::move(o));
    return *this;
  }
  //! No copy assignment
  map_handle &operator=(const map_handle &) = delete;
  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(map_handle &o) noexcept
  {
    map_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  //! Unmap the mapped view.
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override;
  //! Releases the mapped view, but does NOT release the native handle.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC native_handle_type release() noexcept override;

protected:
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC size_t _do_max_buffers() const noexcept override { return 0; }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> _do_barrier(io_request<const_buffers_type> reqs = io_request<const_buffers_type>(), barrier_kind kind = barrier_kind::nowait_data_only, deadline d = deadline()) noexcept override;
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> _do_read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept override;
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> _do_write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept override;

public:
  /*! Map unused memory into view, creating new memory if insufficient unused memory is available
  (i.e. add the returned memory to the process' commit charge, unless `flag::nocommit`
  was specified). Note that the memory mapped by this call may contain non-zero bits (recycled memory)
  unless `zeroed` is true.

  \param bytes How many bytes to map. Typically will be rounded up to a multiple of the page size
  (see `page_size()`).
  \param zeroed Set to true if only all bits zeroed memory is wanted.
  \param _flag The permissions with which to map the view.

  \note On Microsoft Windows this constructor uses the faster `VirtualAlloc()` which creates less
  versatile page backed memory. If you want anonymous memory allocated from a paging file backed
  section instead, create a page file backed section and then a mapped view from that using
  the other constructor. This makes available all those very useful VM tricks Windows can do with
  section mapped memory which `VirtualAlloc()` memory cannot do.

  \errors Any of the values POSIX `mmap()` or `VirtualAlloc()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<map_handle> map(size_type bytes, bool zeroed = false, section_handle::flag _flag = section_handle::flag::readwrite) noexcept;

  /*! Reserve address space within which individual pages can later be committed. Reserved address
  space is NOT added to the process' commit charge.

  \param bytes How many bytes to reserve. Rounded up to nearest 64Kb on Windows.

  \note On Microsoft Windows this constructor uses the faster `VirtualAlloc()` which creates less
  versatile page backed memory. If you want anonymous memory allocated from a paging file backed
  section instead, create a page file backed section and then a mapped view from that using
  the other constructor. This makes available all those very useful VM tricks Windows can do with
  section mapped memory which `VirtualAlloc()` memory cannot do.

  \errors Any of the values POSIX `mmap()` or `VirtualAlloc()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<map_handle> reserve(size_type bytes) noexcept { return map(bytes, false, section_handle::flag::none | section_handle::flag::nocommit); }

  /*! Create a memory mapped view of a backing storage, optionally reserving additional address
  space for later growth.

  \param section A memory section handle specifying the backing storage to use.
  \param bytes How many bytes to reserve (0 = the size of the section). Rounded up to nearest
  64Kb on Windows.
  \param offset The offset into the backing storage to map from. Typically needs to be at least
  a multiple of the page size (see `page_size()`), on Windows it needs to be a multiple of the
  kernel memory allocation granularity (typically 64Kb).
  \param _flag The permissions with which to map the view which are constrained by the
  permissions of the memory section. `flag::none` can be useful for reserving virtual address
  space without committing system resources, use `commit()` to later change availability of memory.
  Note that apart from read/write/cow/execute, the section's flags override the map's flags.

  \errors Any of the values POSIX `mmap()` or `NtMapViewOfSection()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<map_handle> map(section_handle &section, size_type bytes = 0, extent_type offset = 0, section_handle::flag _flag = section_handle::flag::readwrite) noexcept;

  //! The memory section this handle is using
  section_handle *section() const noexcept { return _section; }
  //! Sets the memory section this handle is using
  void set_section(section_handle *s) noexcept { _section = s; }

  //! The address in memory where this mapped view resides
  byte *address() const noexcept { return _addr; }

  //! The offset of the memory map.
  extent_type offset() const noexcept { return _offset; }

  //! The reservation size of the memory map.
  size_type capacity() const noexcept { return _reservation; }

  //! The size of the memory map. This is the accessible size, NOT the reservation size.
  LLFIO_MAKE_FREE_FUNCTION
  size_type length() const noexcept { return _length; }

  //! The memory map as a span of bytes.
  span<byte> as_span() noexcept { return {_addr, _length}; }
  //! \overload
  span<const byte> as_span() const noexcept { return {_addr, _length}; }

  //! The page size used by the map, in bytes.
  size_type page_size() const noexcept { return _pagesize; }

  //! True if the map is of non-volatile RAM
  bool is_nvram() const noexcept { return !!(_flag & section_handle::flag::nvram); }

  //! Update the size of the memory map to that of any backing section, up to the reservation limit.
  result<size_type> update_map() noexcept
  {
    if(_section == nullptr)
    {
      return _reservation;
    }
    OUTCOME_TRY(length, _section->length());  // length of the backing file
    length -= _offset;
    if(length > _reservation)
    {
      length = _reservation;
    }
    _length = static_cast<size_type>(length);
    return _length;
  }

  /*! Resize the reservation of the memory map without changing the address (unless the map
  was zero sized, in which case a new address will be chosen).

  If shrinking, address space is released on POSIX, and on Windows if the new size is zero.
  If the new size is zero, the address is set to null to prevent surprises.
  Windows does not support modifying existing mapped regions, so if the new size is not
  zero, the call will probably fail. Windows should let you truncate a previous extension
  however, if it is exact.

  If expanding, an attempt is made to map in new reservation immediately after the current address
  reservation, thus extending the reservation. If anything else is mapped in after
  the current reservation, the function fails.

  \note On all supported platforms apart from OS X, proprietary flags exist to avoid
  performing a map if a map extension cannot be immediately placed after the current map. On OS X,
  we hint where we'd like the new map to go, but if something is already there OS X will
  place the map elsewhere. In this situation, we delete the new map and return failure,
  which is inefficient, but there is nothing else we can do.

  \return The bytes actually reserved.
  \param newsize The bytes to truncate the map reservation to. Rounded up to the nearest page size (POSIX) or 64Kb on Windows.
  \param permit_relocation Permit the address to change (some OSs provide a syscall for resizing
  a memory map).
  \errors Any of the values POSIX `mremap()`, `mmap(addr)` or `VirtualAlloc(addr)` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<size_type> truncate(size_type newsize, bool permit_relocation = false) noexcept;

  /*! Ask the system to commit the system resources to make the memory represented
  by the buffer available with the given permissions. addr and length should be page
  aligned (see `page_size()`), if not the returned buffer is the region actually committed.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<buffer_type> commit(buffer_type region, section_handle::flag flag = section_handle::flag::readwrite) noexcept;

  /*! Ask the system to make the memory represented by the buffer unavailable and
  to decommit the system resources representing them. addr and length should be
  page aligned (see `page_size()`), if not the returned buffer is the region actually
  decommitted.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<buffer_type> decommit(buffer_type region) noexcept;

  /*! Zero the memory represented by the buffer. Differs from zero() because it acts on
  mapped memory, not on allocated file extents.

  On Linux only, any full 4Kb pages will be deallocated from the
  system entirely, including the extents for them in any backing storage. On
  newer Linux kernels the kernel can additionally swap whole 4Kb pages for
  freshly zeroed ones making this a very efficient way of zeroing large ranges of memory.
  Note that commit charge is not affected by this operation, as writes into
  the zeroed pages are guaranteed to succeed.

  On Windows and Mac OS, this call currently only has an effect for non-backed memory due to lacking kernel support.

  \errors Any of the errors returnable by madvise() or DiscardVirtualMemory or the zero() function.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> zero_memory(buffer_type region) noexcept;

  /*! Ask the system to unset the dirty flag for the memory represented by the
  buffer. This will prevent any changes not yet sent to the backing storage from
  being sent in the future, also if the system kicks out this page and reloads it
  you may see some edition of the underlying storage instead of what was here.
  `addr` and `length` should be page aligned (see`page_size()`), if not the returned
  buffer is the region actually undirtied.

  Note that commit charge is not affected by this operation, as writes into
  the undirtied pages are guaranteed to succeed.

  \warning This function destroys the contents of unwritten pages in the region
  in a totally unpredictable fashion. Only use it if you don't care how much of
  the region reaches physical storage or not. Note that the region is not necessarily
  zeroed, and may be randomly zeroed.

  \note Microsoft Windows does not support unsetting the dirty flag on file backed maps,
  so on Windows this call does nothing.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<buffer_type> do_not_store(buffer_type region) noexcept;

  //! Ask the system to begin to asynchronously prefetch the span of memory regions given, returning the regions actually prefetched. Note that on Windows 7 or earlier the system call to implement this was not available, and so you will see an empty span returned.
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<span<buffer_type>> prefetch(span<buffer_type> regions) noexcept;
  //! \overload
  static result<buffer_type> prefetch(buffer_type region) noexcept
  {
    OUTCOME_TRY(ret, prefetch(span<buffer_type>(&region, 1)));
    return *ret.data();
  }

  /*! \brief Read data from the mapped view.

  \note Because this implementation never copies memory, you can pass in buffers with a null address. As this
  function never reads any memory, no attempt to trap signal raises can be made, this falls onto the user of
  this function. See `QUICKCPPLIB_NAMESPACE::signal_guard` for a helper function.

  \return The buffers read, which will never be the buffers input, because they will point into the mapped view.
  The size of each scatter-gather buffer returned is updated with the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors None, though the various signals and structured exception throws common to using memory maps may occur.
  \mallocs None.
  */
  using io_handle::read;

  /*! \brief Write data to the mapped view.

  If this map is backed by a file, and `section_handle::flag::write_via_syscall` is set, this function is
  actually implemented using the equivalent to the kernel's `write()` syscall. This will be very significantly
  slower for small writes than writing into the map directly, and will corrupt file content on kernels without
  a unified kernel page cache, so it should not be normally enabled. However, this technique is known to work
  around various kernel bugs, quirks and race conditions found in modern OS kernels when memory mapped i/o
  is performed at scale (files of many tens of Gb each).

 \note This call traps signals and structured exception throws using `QUICKCPPLIB_NAMESPACE::signal_guard`.
  Instantiating a `QUICKCPPLIB_NAMESPACE::signal_guard_install` somewhere much higher up in the call stack
  will improve performance enormously. The signal guard may cost less than 100 CPU cycles depending on how
  you configure it. If you don't want the guard, you can write memory directly using `address()`.

  \return The buffers written, which will never be the buffers input because they will point at where the data was copied into the mapped view.
  The size of each scatter-gather buffer returned is updated with the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors If during the attempt to write the buffers to the map a `SIGBUS` or `EXCEPTION_IN_PAGE_ERROR` is raised,
  an error code comparing equal to `errc::no_space_on_device` will be returned. This may not always be the cause
  of the raised signal, but it is by far the most likely.
  \mallocs None if a `QUICKCPPLIB_NAMESPACE::signal_guard_install` is already instanced.
  */
  using io_handle::write;
};

//! \brief Constructor for `map_handle`
template <> struct construct<map_handle>
{
  section_handle &section;
  map_handle::size_type bytes = 0;
  map_handle::extent_type offset = 0;
  section_handle::flag _flag = section_handle::flag::readwrite;
  result<map_handle> operator()() const noexcept { return map_handle::map(section, bytes, offset, _flag); }
};

LLFIO_V2_NAMESPACE_END

// Do not actually attach/detach, as it causes a page fault storm in the current emulation
QUICKCPPLIB_NAMESPACE_BEGIN

namespace in_place_attach_detach
{
  namespace traits
  {
    template <> struct disable_attached_for<LLFIO_V2_NAMESPACE::map_handle> : public std::true_type
    {
    };
  }  // namespace traits
}  // namespace in_place_attach_detach
QUICKCPPLIB_NAMESPACE_END

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

//! \brief Declare `map_handle` as a suitable source for P1631 `attached<T>`.
template <class T> constexpr inline span<T> in_place_attach(map_handle &mh) noexcept
{
  return span<T>{reinterpret_cast<T *>(mh.address()), mh.length() / sizeof(T)};
}

namespace detail
{
  inline result<io_handle::registered_buffer_type> map_handle_allocate_registered_buffer(size_t &bytes) noexcept
  {
    try
    {
      auto make_shared = [](map_handle h) {
        struct registered_buffer_type_indirect
        {
          map_handle h;
          io_handle::buffer_type buffer;
          registered_buffer_type_indirect(map_handle _h)
              : h(std::move(_h))
              , buffer(h.as_span())
          {
          }
        };
        auto ptr = std::make_shared<registered_buffer_type_indirect>(std::move(h));
        return io_handle::registered_buffer_type(ptr, &ptr->buffer);
      };
      const auto &page_sizes = utils::page_sizes(true);
      size_t idx = 0;
      for(size_t n = 0; n < page_sizes.size(); ++n)
      {
        if(page_sizes[n] > bytes)
        {
          break;
        }
        if((bytes & (page_sizes[n] - 1)) == 0)
        {
          idx = n;
        }
      }
      section_handle::flag flags = section_handle::flag::readwrite;
      if(idx > 0)
      {
        switch(idx)
        {
        case 1:
          flags |= section_handle::flag::page_sizes_1;
          break;
        case 2:
          flags |= section_handle::flag::page_sizes_2;
          break;
        case 3:
          flags |= section_handle::flag::page_sizes_3;
          break;
        default:
          break;
        }
        auto r = map_handle::map(bytes, false, flags);
        if(r)
        {
          bytes = (bytes + page_sizes[idx] - 1) & ~(page_sizes[idx] - 1);
          return make_shared(std::move(r).value());
        }
      }
      auto r = map_handle::map(bytes, false);
      if(r)
      {
        bytes = (bytes + page_sizes[0] - 1) & ~(page_sizes[0] - 1);
        return make_shared(std::move(r).value());
      }
      return errc::not_enough_memory;
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
}  // namespace detail

// Implement io_handle::_do_allocate_registered_buffer()
LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<io_handle::registered_buffer_type> io_handle::_do_allocate_registered_buffer(size_t &bytes) noexcept
{
  return detail::map_handle_allocate_registered_buffer(bytes);
}

// BEGIN make_free_functions.py
//! Swap with another instance
inline void swap(section_handle &self, section_handle &o) noexcept
{
  return self.swap(std::forward<decltype(o)>(o));
}
/*! \brief Create a memory section backed by a file.
\param backing The handle to use as backing storage.
\param maximum_size The initial size of this section, which cannot be larger than any backing file. Zero means to use `backing.maximum_extent()`.
\param _flag How to create the section.

\errors Any of the values POSIX dup(), open() or NtCreateSection() can return.
*/
inline result<section_handle> section(file_handle &backing, section_handle::extent_type maximum_size, section_handle::flag _flag) noexcept
{
  return section_handle::section(std::forward<decltype(backing)>(backing), std::forward<decltype(maximum_size)>(maximum_size), std::forward<decltype(_flag)>(_flag));
}
/*! \brief Create a memory section backed by a file.
\param backing The handle to use as backing storage.
\param bytes The initial size of this section, which cannot be larger than any backing file. Zero means to use `backing.maximum_extent()`.

This convenience overload create a writable section if the backing file is writable, otherwise a read-only section.

\errors Any of the values POSIX dup(), open() or NtCreateSection() can return.
*/
inline result<section_handle> section(file_handle &backing, section_handle::extent_type bytes = 0) noexcept
{
  return section_handle::section(std::forward<decltype(backing)>(backing), std::forward<decltype(bytes)>(bytes));
}
/*! \brief Create a memory section backed by an anonymous, managed file.
\param bytes The initial size of this section. Cannot be zero.
\param dirh Where to create the anonymous, managed file.
\param _flag How to create the section.

\errors Any of the values POSIX dup(), open() or NtCreateSection() can return.
*/
inline result<section_handle> section(section_handle::extent_type bytes, const path_handle &dirh = path_discovery::storage_backed_temporary_files_directory(), section_handle::flag _flag = section_handle::flag::read | section_handle::flag::write) noexcept
{
  return section_handle::section(std::forward<decltype(bytes)>(bytes), std::forward<decltype(dirh)>(dirh), std::forward<decltype(_flag)>(_flag));
}
//! Return the current maximum permitted extent of the memory section.
inline result<section_handle::extent_type> length(const section_handle &self) noexcept
{
  return self.length();
}
/*! Resize the current maximum permitted extent of the memory section to the given extent.
\param self The object whose member function to call.
\param newsize The new size of the memory section, which cannot be zero. Specify zero to use `backing.maximum_extent()`.
This cannot exceed the size of any backing file used if that file is not writable.

\errors Any of the values `NtExtendSection()` or `ftruncate()` can return.
*/
inline result<section_handle::extent_type> truncate(section_handle &self, section_handle::extent_type newsize = 0) noexcept
{
  return self.truncate(std::forward<decltype(newsize)>(newsize));
}
//! Swap with another instance
inline void swap(map_handle &self, map_handle &o) noexcept
{
  return self.swap(std::forward<decltype(o)>(o));
}
//! Unmap the mapped view.
inline result<void> close(map_handle &self) noexcept
{
  return self.close();
}
/*! Create new memory and map it into view.
\param bytes How many bytes to create and map. Typically will be rounded up to a multiple of the page size (see `page_size()`) on POSIX, 64Kb on Windows.
\param zeroed Set to true if only all bits zeroed memory is wanted.
\param _flag The permissions with which to map the view. `flag::none` can be useful for reserving virtual address space without committing system resources, use commit() to later change availability of memory.

\note On Microsoft Windows this constructor uses the faster VirtualAlloc() which creates less versatile page backed memory. If you want anonymous memory
allocated from a paging file backed section instead, create a page file backed section and then a mapped view from that using
the other constructor. This makes available all those very useful VM tricks Windows can do with section mapped memory which
VirtualAlloc() memory cannot do.

\errors Any of the values POSIX mmap() or VirtualAlloc() can return.
*/
inline result<map_handle> map(map_handle::size_type bytes, bool zeroed = false, section_handle::flag _flag = section_handle::flag::readwrite) noexcept
{
  return map_handle::map(std::forward<decltype(bytes)>(bytes), zeroed, std::forward<decltype(_flag)>(_flag));
}
/*! Create a memory mapped view of a backing storage, optionally reserving additional address space for later growth.
\param section A memory section handle specifying the backing storage to use.
\param bytes How many bytes to reserve (0 = the size of the section). Rounded up to nearest 64Kb on Windows.
\param offset The offset into the backing storage to map from. Typically needs to be at least a multiple of the page size (see `page_size()`), on Windows it needs to be a multiple of the kernel memory allocation granularity (typically 64Kb).
\param _flag The permissions with which to map the view which are constrained by the permissions of the memory section. `flag::none` can be useful for reserving virtual address space without committing system resources, use commit() to later change availability of memory.

\errors Any of the values POSIX mmap() or NtMapViewOfSection() can return.
*/
inline result<map_handle> map(section_handle &section, map_handle::size_type bytes = 0, map_handle::extent_type offset = 0, section_handle::flag _flag = section_handle::flag::readwrite) noexcept
{
  return map_handle::map(std::forward<decltype(section)>(section), std::forward<decltype(bytes)>(bytes), std::forward<decltype(offset)>(offset), std::forward<decltype(_flag)>(_flag));
}
//! The size of the memory map. This is the accessible size, NOT the reservation size.
inline map_handle::size_type length(const map_handle &self) noexcept
{
  return self.length();
}
/*! Resize the reservation of the memory map without changing the address (unless the map
was zero sized, in which case a new address will be chosen).

If shrinking, address space is released on POSIX, and on Windows if the new size is zero.
If the new size is zero, the address is set to null to prevent surprises.
Windows does not support modifying existing mapped regions, so if the new size is not
zero, the call will probably fail. Windows should let you truncate a previous extension
however, if it is exact.

If expanding, an attempt is made to map in new reservation immediately after the current address
reservation, thus extending the reservation. If anything else is mapped in after
the current reservation, the function fails.

\note On all supported platforms apart from OS X, proprietary flags exist to avoid
performing a map if a map extension cannot be immediately placed after the current map. On OS X,
we hint where we'd like the new map to go, but if something is already there OS X will
place the map elsewhere. In this situation, we delete the new map and return failure,
which is inefficient, but there is nothing else we can do.

\return The bytes actually reserved.
\param self The object whose member function to call.
\param newsize The bytes to truncate the map reservation to. Rounded up to the nearest page size (POSIX) or 64Kb on Windows.
\param permit_relocation Permit the address to change (some OSs provide a syscall for resizing
a memory map).
\errors Any of the values POSIX `mremap()`, `mmap(addr)` or `VirtualAlloc(addr)` can return.
*/
inline result<map_handle::size_type> truncate(map_handle &self, map_handle::size_type newsize, bool permit_relocation = false) noexcept
{
  return self.truncate(std::forward<decltype(newsize)>(newsize), std::forward<decltype(permit_relocation)>(permit_relocation));
}
/*! \brief Read data from the mapped view.

\note Because this implementation never copies memory, you can pass in buffers with a null address.

\return The buffers read, which will never be the buffers input because they will point into the mapped view.
The size of each scatter-gather buffer is updated with the number of bytes of that buffer transferred.
\param self The object whose member function to call.
\param reqs A scatter-gather and offset request.
\param d Ignored.
\errors None, though the various signals and structured exception throws common to using memory maps may occur.
\mallocs None.
*/
inline map_handle::io_result<map_handle::buffers_type> read(map_handle &self, map_handle::io_request<map_handle::buffers_type> reqs, deadline d = deadline()) noexcept
{
  return self.read(std::forward<decltype(reqs)>(reqs), std::forward<decltype(d)>(d));
}
/*! \brief Write data to the mapped view.

\return The buffers written, which will never be the buffers input because they will point at where the data was copied into the mapped view.
The size of each scatter-gather buffer is updated with the number of bytes of that buffer transferred.
\param self The object whose member function to call.
\param reqs A scatter-gather and offset request.
\param d Ignored.
\errors None, though the various signals and structured exception throws common to using memory maps may occur.
\mallocs None.
*/
inline map_handle::io_result<map_handle::const_buffers_type> write(map_handle &self, map_handle::io_request<map_handle::const_buffers_type> reqs, deadline d = deadline()) noexcept
{
  return self.write(std::forward<decltype(reqs)>(reqs), std::forward<decltype(d)>(d));
}
// END make_free_functions.py

namespace detail
{
  inline result<size_t> pagesize_from_flags(section_handle::flag _flag) noexcept
  {
    try
    {
      const auto &pagesizes = utils::page_sizes();
      if((_flag & section_handle::flag::page_sizes_3) == section_handle::flag::page_sizes_3)
      {
        if(pagesizes.size() < 4)
        {
          return errc::invalid_argument;
        }
        return pagesizes[3];
      }
      else if((_flag & section_handle::flag::page_sizes_2) == section_handle::flag::page_sizes_2)
      {
        if(pagesizes.size() < 3)
        {
          return errc::invalid_argument;
        }
        return pagesizes[2];
      }
      else if((_flag & section_handle::flag::page_sizes_1) == section_handle::flag::page_sizes_1)
      {
        if(pagesizes.size() < 2)
        {
          return errc::invalid_argument;
        }
        return pagesizes[1];
      }
      return pagesizes[0];
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
}  // namespace detail

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/map_handle.ipp"
#else
#include "detail/impl/posix/map_handle.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
