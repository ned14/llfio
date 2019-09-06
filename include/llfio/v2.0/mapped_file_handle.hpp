/* An mapped handle to a file
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
File Created: Sept 2017


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

#include "map_handle.hpp"

//! \file mapped_file_handle.hpp Provides mapped_file_handle

#ifndef LLFIO_MAPPED_FILE_HANDLE_H
#define LLFIO_MAPPED_FILE_HANDLE_H

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class mapped_file_handle
\brief A memory mapped regular file or device

<table>
<tr><th></th><th>Cost of opening</th><th>Cost of i/o</th><th>Concurrency and Atomicity</th><th>Other remarks</th></tr>
<tr><td>`file_handle`</td><td>Least</td><td>Syscall</td><td>POSIX guarantees (usually)</td><td>Least gotcha</td></tr>
<tr><td>`async_file_handle`</td><td>More</td><td>Most (syscall + malloc/free + reactor)</td><td>POSIX guarantees (usually)</td><td>Makes no sense to use with cached i/o as it's a very expensive way to call `memcpy()`</td></tr>
<tr><td>`mapped_file_handle`</td><td>Most</td><td>Least</td><td>None</td><td>Cannot be used with uncached i/o</td></tr>
</table>

All the major OSs on all the major 64 bit CPU architectures now offer at least 127 Tb of address
spaces to user mode processes. This makes feasible mapping multi-Tb files directly into
memory, and thus avoiding the syscall overhead involved when reading and writing. This
becames **especially** important with next-gen storage devices capable of Direct Access
Storage (DAX) like Optane from 2018 onwards, performance via syscalls will always be
but a fraction of speaking directly to the storage device via directly mapped memory.

As an example of the gains, on Microsoft Windows to read or write 1Kb using the standard
syscalls takes about fifteen times longer than the exact same i/o via mapped memory. On Linux,
OS X or FreeBSD the gain is considerably lower, a 1Kb i/o might only be 50% slower via syscalls
than memory maps. However for lots of say 64 byte i/o, the gain of memory maps over
syscalls is unsurpassable.

This class combines a `file_handle` with a `section_handle` and a `map_handle` to
implement a fully memory mapped `file_handle`. The whole file is always mapped entirely
into memory, and `read()` and `write()` i/o is performed directly with the map.
Reads always return the original mapped data, and do not fill any buffers passed in.
For obvious reasons the utility of this class on 32-bit systems is limited,
but can be useful when used with smaller files.

Note that zero length files cannot be memory mapped, and writes past the maximum
extent do NOT auto-extend the size of the file, rather the data written beyond the maximum valid
extent has undefined kernel-specific behaviour, which includes segfaulting. You must therefore always
`truncate(newsize)` to resize the file and its maps before you can read or write to it,
and be VERY careful to not read or write beyond the maximum extent of the file.

Therefore, when a file is created or is otherwise of zero length, `address()` will return
a null pointer. Similarly, calling `truncate(0)` will close the map and section handles,
they will be recreated on next truncation to a non-zero size.

For better performance when handling files which are growing, there is a concept of
"address space reservation" via `reserve()` and `capacity()`, which on some kernels
is automatically and efficiently expanded into when the underlying file grows.
The implementation asks the
kernel to set out a contiguous region of pages matching that reservation, and to map the
file into the beginning of the reservation. The remainder of the pages may be inaccessible
and may generate a segfault, or they may automatically reflect any growth in the underlying
file. This is why `read()` and `write()` only know about the reservation size, and will read
and write memory up to that reservation size, without checking if the memory involved exists
or not yet. You are guaranteed that `address()` will not return a new
value unless you truncate from a bigger length to a smaller length, or you call `reserve()`
with a new reservation or `truncate()` with a value bigger than the reservation.

`maximum_extent()` reports the last truncated length of the mapped file (possibly by any process in
the system) up to the reservation limit, NOT the maximum extent of the underlying file.
When you know that another process has extended the file and you wish to map the newly appended
data, you can call `update_map()` which guarantees that the mapping your
process sees is up to date, rather than relying on any kernel-specific automatic mapping.
Whether automatic or enforced by `update_map()`, the reservation limit will not be exceeded
nor will `address()` suddenly return something different.

It is thus up to you to detect that the reservation has been exhausted, and to
reserve a new reservation which will change the value returned by `address()`. This
entirely manual system is a bit tedious and cumbersome to use, but as mapping files
is an expensive operation given TLB shootdown, we leave it up to the end user to decide
when to expend the cost of mapping.

\warning You must be cautious when the file is being extended by third parties which are
not using this `mapped_file_handle` to write the new data. With unified page cache kernels,
mixing mapped and normal i/o is generally safe except at the end of a file where race
conditions and outright kernel bugs tend to abound. To avoid these, solely and exclusively
use a dedicated handle configured to atomic append only to do the appends.
*/
class LLFIO_DECL mapped_file_handle : public file_handle
{
public:
  using dev_t = file_handle::dev_t;
  using ino_t = file_handle::ino_t;
  using path_view_type = file_handle::path_view_type;
  using path_type = io_handle::path_type;
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
  size_type _reservation{0};
  section_handle _sh;  // Tracks the file (i.e. *this) somewhat lazily
  map_handle _mh;      // The current map with valid extent

public:
  //! Default constructor
  constexpr mapped_file_handle() {}  // NOLINT

  //! Implicit move construction of mapped_file_handle permitted
  mapped_file_handle(mapped_file_handle &&o) noexcept : file_handle(std::move(o)), _reservation(o._reservation), _sh(std::move(o._sh)), _mh(std::move(o._mh))
  {
    _sh.set_backing(this);
    _mh.set_section(&_sh);
  }
  //! No copy construction (use `clone()`)
  mapped_file_handle(const mapped_file_handle &) = delete;
  //! Explicit conversion from file_handle permitted
  explicit constexpr mapped_file_handle(file_handle &&o) noexcept : file_handle(std::move(o)) {}
  //! Explicit conversion from file_handle permitted, this overload also attempts to map the file
  explicit mapped_file_handle(file_handle &&o, size_type reservation) noexcept : file_handle(std::move(o))
  {
    auto out = reserve(reservation);
    if(!out)
    {
      _reservation = reservation;
      // sink the error
    }
  }
  //! Move assignment of mapped_file_handle permitted
  mapped_file_handle &operator=(mapped_file_handle &&o) noexcept
  {
    this->~mapped_file_handle();
    new(this) mapped_file_handle(std::move(o));
    return *this;
  }
  //! No copy assignment
  mapped_file_handle &operator=(const mapped_file_handle &) = delete;
  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(mapped_file_handle &o) noexcept
  {
    mapped_file_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  /*! Create a memory mapped file handle opening access to a file on path.
  \param reservation The number of bytes to reserve for later expansion when mapping. Zero means reserve only the current file length.
  \param base Handle to a base location on the filing system. Pass `{}` to indicate that path will be absolute.
  \param _path The path relative to base to open.
  \param _mode How to open the file.
  \param _creation How to create the file.
  \param _caching How to ask the kernel to cache the file.
  \param flags Any additional custom behaviours.

  Note that if the file is currently zero sized, no mapping occurs now, but
  later when `truncate()` or `update_map()` is called.

  \errors Any of the values which the constructors for `file_handle`, `section_handle` and `map_handle` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<mapped_file_handle> mapped_file(size_type reservation, const path_handle &base, path_view_type _path, mode _mode = mode::read, creation _creation = creation::open_existing, caching _caching = caching::all, flag flags = flag::none) noexcept
  {
    if(_mode == mode::append)
    {
      return errc::invalid_argument;
    }
    OUTCOME_TRY(fh, file_handle::file(base, _path, _mode, _creation, _caching, flags));
    switch(_creation)
    {
    default:
    {
      // Attempt mapping now (may silently fail if file is empty)
      mapped_file_handle mfh(std::move(fh), reservation);
      return {std::move(mfh)};
    }
    case creation::only_if_not_exist:
    case creation::truncate:
    {
      // Don't attempt mapping now as file will be empty
      mapped_file_handle mfh(std::move(fh));
      mfh._reservation = reservation;
      return {std::move(mfh)};
    }
    }
  }
  //! \overload
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<mapped_file_handle> mapped_file(const path_handle &base, path_view_type _path, mode _mode = mode::read, creation _creation = creation::open_existing, caching _caching = caching::all, flag flags = flag::none) noexcept { return mapped_file(0, base, _path, _mode, _creation, _caching, flags); }

  /*! Create an mapped file handle creating a randomly named file on a path.
  The file is opened exclusively with `creation::only_if_not_exist` so it
  will never collide with nor overwrite any existing file. Note also
  that caching defaults to temporary which hints to the OS to only
  flush changes to physical storage as lately as possible.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<mapped_file_handle> mapped_random_file(size_type reservation, const path_handle &dirpath, mode _mode = mode::write, caching _caching = caching::temporary, flag flags = flag::none) noexcept
  {
    try
    {
      for(;;)
      {
        auto randomname = utils::random_string(32);
        randomname.append(".random");
        result<mapped_file_handle> ret = mapped_file(reservation, dirpath, randomname, _mode, creation::only_if_not_exist, _caching, flags);
        if(ret || (!ret && ret.error() != errc::file_exists))
        {
          return ret;
        }
      }
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
  /*! Create a mapped file handle creating the named file on some path which
  the OS declares to be suitable for temporary files. Most OSs are
  very lazy about flushing changes made to these temporary files.
  Note the default flags are to have the newly created file deleted
  on first handle close.
  Note also that an empty name is equivalent to calling
  `mapped_random_file(path_discovery::storage_backed_temporary_files_directory())` and the creation
  parameter is ignored.

  \note If the temporary file you are creating is not going to have its
  path sent to another process for usage, this is the WRONG function
  to use. Use `temp_inode()` instead, it is far more secure.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<mapped_file_handle> mapped_temp_file(size_type reservation, path_view_type name = path_view_type(), mode _mode = mode::write, creation _creation = creation::if_needed, caching _caching = caching::temporary, flag flags = flag::unlink_on_first_close) noexcept
  {
    auto &tempdirh = path_discovery::storage_backed_temporary_files_directory();
    return name.empty() ? mapped_random_file(reservation, tempdirh, _mode, _caching, flags) : mapped_file(reservation, tempdirh, name, _mode, _creation, _caching, flags);
  }
  /*! \em Securely create a mapped file handle creating a temporary anonymous inode in
  the filesystem referred to by \em dirpath. The inode created has
  no name nor accessible path on the filing system and ceases to
  exist as soon as the last handle is closed, making it ideal for use as
  a temporary file where other processes do not need to have access
  to its contents via some path on the filing system (a classic use case
  is for backing shared memory maps).

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<mapped_file_handle> mapped_temp_inode(const path_handle &dir = path_discovery::storage_backed_temporary_files_directory(), mode _mode = mode::write, flag flags = flag::none) noexcept
  {
    OUTCOME_TRY(v, file_handle::temp_inode(dir, _mode, flags));
    mapped_file_handle ret(std::move(v));
    return {std::move(ret)};
  }

  //! The memory section this handle is using
  const section_handle &section() const noexcept { return _sh; }
  //! The memory section this handle is using
  section_handle &section() noexcept { return _sh; }

  //! The map this handle is using
  const map_handle &map() const noexcept { return _mh; }
  //! The map this handle is using
  map_handle &map() noexcept { return _mh; }

  //! The address in memory where this mapped file resides
  byte *address() const noexcept { return _mh.address(); }

  //! The page size used by the map, in bytes.
  size_type page_size() const noexcept { return _mh.page_size(); }

  //! True if the map is of non-volatile RAM
  bool is_nvram() const noexcept { return _mh.is_nvram(); }

  //! The maximum extent of the underlying file
  result<extent_type> underlying_file_maximum_extent() const noexcept { return file_handle::maximum_extent(); }

  //! The address space (to be) reserved for future expansion of this file.
  size_type capacity() const noexcept { return _reservation; }

  /*! \brief Reserve a new amount of address space for mapping future expansion of this file.
  \param reservation The number of bytes of virtual address space to reserve. Zero means reserve
  the current length of the underlying file.

  Note that this is an expensive call, and `address()` may return a different value afterwards.
  This call will fail if the underlying file has zero length.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<size_type> reserve(size_type reservation = 0) noexcept;

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~mapped_file_handle() override
  {
    if(_v)
    {
      (void) mapped_file_handle::close();
    }
  }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override;
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC native_handle_type release() noexcept override;
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> barrier(io_request<const_buffers_type> reqs = io_request<const_buffers_type>(), bool wait_for_device = false, bool and_metadata = false, deadline d = deadline()) noexcept override { return _mh.barrier(reqs, wait_for_device, and_metadata, d); }
  result<mapped_file_handle> clone(size_type reservation, mode mode_ = mode::unchanged, caching caching_ = caching::unchanged, deadline d = std::chrono::seconds(30)) const noexcept
  {
    OUTCOME_TRY(fh, file_handle::clone(mode_, caching_, d));
    return mapped_file_handle(std::move(fh), reservation);
  }
  //! Return the current maximum permitted extent of the file.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> maximum_extent() const noexcept override { return _mh.length(); }

  /*! \brief Resize the current maximum permitted extent of the mapped file to the given extent, avoiding any
  new allocation of physical storage where supported, and mapping or unmapping any new pages
  up to the reservation to reflect the new maximum extent. If the new size exceeds the reservation,
  `reserve()` will be called to increase the reservation.

  Note that on extents based filing systems
  this will succeed even if there is insufficient free space on the storage medium. Only when
  pages are written to will the lack of sufficient free space be realised, resulting in an
  operating system specific exception.

  \note On Microsoft Windows you cannot shrink a file with a section handle open on it in any
  process in the system. We therefore *always* destroy the internal map and section before
  truncating, and then recreate the map and section afterwards if the new size is not zero.
  `address()` therefore may change.
  You will need to ensure all other users of the same file close their section and
  map handles before any process can shrink the underlying file.

  \return The bytes actually truncated to.
  \param newsize The bytes to truncate the file to. Zero causes the maps to be closed before
  truncation.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> truncate(extent_type newsize) noexcept override;

  /*! \brief Efficiently update the mapping to match that of the underlying file,
  returning the size of the underlying file.

  This call is often considerably less heavyweight than `truncate(newsize)`, and should be used where possible.

  If the internal section and map handle are invalid, they are restored unless the underlying file is zero length.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<extent_type> update_map() noexcept;

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> zero(extent_type offset, extent_type bytes, deadline /*unused*/ = deadline()) noexcept override
  {
    OUTCOME_TRYV(_mh.zero_memory({_mh.address() + offset, bytes}));
    return bytes;
  }

  using file_handle::read;
  using file_handle::write;

  /*! \brief Read data from the mapped file.

  \note Because this implementation never copies memory, you can pass in buffers with a null address. As this
  function never reads any memory, no attempt to trap signal raises can be made, this falls onto the user of
  this function. See `QUICKCPPLIB_NAMESPACE::signal_guard` for a helper function.

  \return The buffers read, which will never be the buffers input, because they will point into the mapped view.
  The size of each scatter-gather buffer is updated with the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors None, though the various signals and structured exception throws common to using memory maps may occur.
  \mallocs None.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept override { return _mh.read(reqs, d); }
  /*! \brief Write data to the mapped file.

  \note This call traps signals and structured exception throws using `QUICKCPPLIB_NAMESPACE::signal_guard`.
  Instantiating a `QUICKCPPLIB_NAMESPACE::signal_guard_install` somewhere much higher up in the call stack
  will improve performance enormously. The signal guard may cost less than 100 CPU cycles depending on how
  you configure it. If you don't want the guard, you can write memory directly using `address()`.

  \return The buffers written, which will never be the buffers input because they will point at where the data was copied into the mapped view.
  The size of each scatter-gather buffer is updated with the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors If during the attempt to write the buffers to the map a `SIGBUS` or `EXCEPTION_IN_PAGE_ERROR` is raised,
  an error code comparing equal to `errc::no_space_on_device` will be returned. This may not always be the cause
  of the raised signal, but it is by far the most likely.
  \mallocs None if a `QUICKCPPLIB_NAMESPACE::signal_guard_install` is already instanced.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept override { return _mh.write(reqs, d); }
};

//! \brief Constructor for `mapped_file_handle`
template <> struct construct<mapped_file_handle>
{
  mapped_file_handle::size_type reservation;
  const path_handle &base;
  mapped_file_handle::path_view_type _path;
  mapped_file_handle::mode _mode = mapped_file_handle::mode::read;
  mapped_file_handle::creation _creation = mapped_file_handle::creation::open_existing;
  mapped_file_handle::caching _caching = mapped_file_handle::caching::all;
  mapped_file_handle::flag flags = mapped_file_handle::flag::none;
  result<mapped_file_handle> operator()() const noexcept { return mapped_file_handle::mapped_file(reservation, base, _path, _mode, _creation, _caching, flags); }
};

LLFIO_V2_NAMESPACE_END

// Do not actually attach/detach, as it causes a page fault storm in the current emulation
QUICKCPPLIB_NAMESPACE_BEGIN

namespace in_place_attach_detach
{
  namespace traits
  {
    template <> struct disable_attached_for<LLFIO_V2_NAMESPACE::mapped_file_handle> : public std::true_type
    {
    };
  }
}
QUICKCPPLIB_NAMESPACE_END

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

//! \brief Declare `mapped_file_handle` as a suitable source for P1631 `attached<T>`.
template <class T> constexpr inline span<T> in_place_attach(mapped_file_handle &mfh) noexcept
{
  return span<T>{reinterpret_cast<T *>(mfh.address()), mfh.map().length()/sizeof(T)};
}

// BEGIN make_free_functions.py
//! Swap with another instance
inline void swap(mapped_file_handle &self, mapped_file_handle &o) noexcept
{
  return self.swap(std::forward<decltype(o)>(o));
}
/*! Create a memory mapped file handle opening access to a file on path.
\param reservation The number of bytes to reserve for later expansion when mapping. Zero means reserve only the current file length.
\param base Handle to a base location on the filing system. Pass `{}` to indicate that path will be absolute.
\param _path The path relative to base to open.
\param _mode How to open the file.
\param _creation How to create the file.
\param _caching How to ask the kernel to cache the file.
\param flags Any additional custom behaviours.

Note that if the file is currently zero sized, no mapping occurs now, but
later when `truncate()` or `update_map()` is called.

\errors Any of the values which the constructors for `file_handle`, `section_handle` and `map_handle` can return.
*/
inline result<mapped_file_handle> mapped_file(mapped_file_handle::size_type reservation, const path_handle &base, mapped_file_handle::path_view_type _path, mapped_file_handle::mode _mode = mapped_file_handle::mode::read, mapped_file_handle::creation _creation = mapped_file_handle::creation::open_existing,
                                              mapped_file_handle::caching _caching = mapped_file_handle::caching::all, mapped_file_handle::flag flags = mapped_file_handle::flag::none) noexcept
{
  return mapped_file_handle::mapped_file(std::forward<decltype(reservation)>(reservation), std::forward<decltype(base)>(base), std::forward<decltype(_path)>(_path), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_creation)>(_creation), std::forward<decltype(_caching)>(_caching),
                                         std::forward<decltype(flags)>(flags));
}
//! \overload
inline result<mapped_file_handle> mapped_file(const path_handle &base, mapped_file_handle::path_view_type _path, mapped_file_handle::mode _mode = mapped_file_handle::mode::read, mapped_file_handle::creation _creation = mapped_file_handle::creation::open_existing,
                                              mapped_file_handle::caching _caching = mapped_file_handle::caching::all, mapped_file_handle::flag flags = mapped_file_handle::flag::none) noexcept
{
  return mapped_file_handle::mapped_file(std::forward<decltype(base)>(base), std::forward<decltype(_path)>(_path), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_creation)>(_creation), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! Create an mapped file handle creating a randomly named file on a path.
The file is opened exclusively with `creation::only_if_not_exist` so it
will never collide with nor overwrite any existing file. Note also
that caching defaults to temporary which hints to the OS to only
flush changes to physical storage as lately as possible.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<mapped_file_handle> mapped_random_file(mapped_file_handle::size_type reservation, const path_handle &dirpath, mapped_file_handle::mode _mode = mapped_file_handle::mode::write, mapped_file_handle::caching _caching = mapped_file_handle::caching::temporary,
                                                     mapped_file_handle::flag flags = mapped_file_handle::flag::none) noexcept
{
  return mapped_file_handle::mapped_random_file(std::forward<decltype(reservation)>(reservation), std::forward<decltype(dirpath)>(dirpath), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! Create a mapped file handle creating the named file on some path which
the OS declares to be suitable for temporary files. Most OSs are
very lazy about flushing changes made to these temporary files.
Note the default flags are to have the newly created file deleted
on first handle close.
Note also that an empty name is equivalent to calling
`mapped_random_file(path_discovery::storage_backed_temporary_files_directory())` and the creation
parameter is ignored.

\note If the temporary file you are creating is not going to have its
path sent to another process for usage, this is the WRONG function
to use. Use `temp_inode()` instead, it is far more secure.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<mapped_file_handle> mapped_temp_file(mapped_file_handle::size_type reservation, mapped_file_handle::path_view_type name = mapped_file_handle::path_view_type(), mapped_file_handle::mode _mode = mapped_file_handle::mode::write,
                                                   mapped_file_handle::creation _creation = mapped_file_handle::creation::if_needed, mapped_file_handle::caching _caching = mapped_file_handle::caching::temporary, mapped_file_handle::flag flags = mapped_file_handle::flag::unlink_on_first_close) noexcept
{
  return mapped_file_handle::mapped_temp_file(std::forward<decltype(reservation)>(reservation), std::forward<decltype(name)>(name), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_creation)>(_creation), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! \em Securely create a mapped file handle creating a temporary anonymous inode in
the filesystem referred to by \em dirpath. The inode created has
no name nor accessible path on the filing system and ceases to
exist as soon as the last handle is closed, making it ideal for use as
a temporary file where other processes do not need to have access
to its contents via some path on the filing system (a classic use case
is for backing shared memory maps).

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<mapped_file_handle> mapped_temp_inode(const path_handle &dir = path_discovery::storage_backed_temporary_files_directory(), mapped_file_handle::mode _mode = mapped_file_handle::mode::write, mapped_file_handle::flag flags = mapped_file_handle::flag::none) noexcept
{
  return mapped_file_handle::mapped_temp_inode(std::forward<decltype(dir)>(dir), std::forward<decltype(_mode)>(_mode), std::forward<decltype(flags)>(flags));
}
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/mapped_file_handle.ipp"
#else
#include "detail/impl/posix/mapped_file_handle.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#endif
