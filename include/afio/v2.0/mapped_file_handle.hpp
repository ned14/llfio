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

#ifndef AFIO_MAPPED_FILE_HANDLE_H
#define AFIO_MAPPED_FILE_HANDLE_H

AFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class mapped_file_handle
\brief A memory mapped regular file or device

All the major OSs on all the major 64 bit CPU architectures now offer at least 127 Tb of address
spaces to user mode processes. This makes feasible mapping multi-Tb files directly into
memory, and thus avoiding the syscall overhead involved when reading and writing. This
becames **especially** important with nextgen storage devices capable of Direct Access
Storage (DAX) like Optane from 2018 onwards, performance via syscalls will always be
but a fraction of speaking directly to the storage device via directly mapped memory.

As an example of the gains, on Microsoft Windows to read or write 1Kb using the standard
syscalls takes about fifteen times longer than the exact same i/o via mapped memory. On Linux,
OS X or FreeBSD the gain is considerably lower, a 1Kb i/o might only be 50% slower via syscalls
than memory maps. However for lots of say 64 byte i/o, the gain of memory maps over
syscalls is unsurpassable.

This class combines a `file_handle` with a `section_handle` and a `map_handle` to
implement a fully memory mapped `file_handle`. The whole file is always mapped entirely
into memory, including any appends to the file, and i/o is performed directly with the map.
Reads always return the original mapped data, and do not fill any buffers passed in.
For obvious reasons the utility of this class on 32-bit systems is limited,
but can be useful when used with smaller files.

Note that zero lengthed files cannot be memory mapped. On first write will the map be
created, until then `address()` will return a null pointer. Similarly, calling `truncate(0)`
will destroy the maps which can be useful to know as Microsoft Windows will not permit
shrinking of a file with open maps in any process on it, thus every process must call
`truncate(0)` and sink the error about the shrink failing until when the final process to
attempt the truncation succeeds.

For better performance when handling files which are growing, there is a concept of
"address space reservation" via `reserve()` and `capacity()`. The implementation asks the
kernel to set out a contiguous region of pages matching that reservation, and to map the
file into the beginning of the reservation. The remainder of the pages are inaccessible
and will generate a segfault.

`length()` reports the length of the mapped file, NOT the underlying file. For better
performance, and to avoid kernel bugs, we do not automatically track the length of the
underlying file, so reads and writes always terminate at the mapped file length and do
not auto-extend the file. If you wish to extend the file, you must call `truncate(bytes)`
which is of course racy with respect to other things extending the file. When you
know that another process has extended the file and you wish to map the newly appended
data, you can call `truncate()` with no parameters. This will read the current length
of the underlying file, and map the new data into your process up until the reservation
is full. It is then up to you to detect that the reservation has been exhausted, and to
reserve a new reservation which will change the value returned by `address()`. This
entirely manual system is a bit tedious and cumbersome to use, but as mapping files
is an expensive operation given TLB shootdown, the only place where we change mappings
silently is on the first write to an empty mapped file.

\warning You must be cautious when the file is being extended by third parties which are
not using this `mapped_file_handle` to write the new data. With unified page cache kernels,
mixing mapped and normal i/o is generally safe except at the end of a file where race
conditions and outright kernel bugs tend to abound. To avoid these, **make sure you truncate to new
length before appending data**, or else solely and exclusively use a dedicated handle
configured to atomic append only to do the appends.
*/
class AFIO_DECL mapped_file_handle : public file_handle
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
  section_handle _sh;
  map_handle _mh;

public:
  //! Default constructor
  mapped_file_handle() = default;
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC ~mapped_file_handle();

  //! Construct a handle from a supplied native handle
  constexpr mapped_file_handle(native_handle_type h, dev_t devid, ino_t inode, caching caching = caching::none, flag flags = flag::none)
      : file_handle(std::move(h), devid, inode, std::move(caching), std::move(flags))
  {
  }
  //! Implicit move construction of mapped_file_handle permitted
  mapped_file_handle(mapped_file_handle &&o) noexcept = default;
  //! Explicit conversion from file_handle permitted
  explicit constexpr mapped_file_handle(file_handle &&o) noexcept : file_handle(std::move(o)) {}
  //! Move assignment of mapped_file_handle permitted
  mapped_file_handle &operator=(mapped_file_handle &&o) noexcept
  {
    this->~mapped_file_handle();
    new(this) mapped_file_handle(std::move(o));
    return *this;
  }
  //! Swap with another instance
  AFIO_MAKE_FREE_FUNCTION
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
  later when `truncate()` or `write()` is called.

  \errors Any of the values which the constructors for `file_handle`, `section_handle` and `map_handle` can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<mapped_file_handle> mapped_file(size_type reservation, const path_handle &base, path_view_type _path, mode _mode = mode::read, creation _creation = creation::open_existing, caching _caching = caching::all, flag flags = flag::none) noexcept;
  //! \overload
  AFIO_MAKE_FREE_FUNCTION
  static inline result<mapped_file_handle> mapped_file(const path_handle &base, path_view_type _path, mode _mode = mode::read, creation _creation = creation::open_existing, caching _caching = caching::all, flag flags = flag::none) noexcept { return mapped_file(0, base, _path, _mode, _creation, _caching, flags); }

  /*! Create an mapped file handle creating a randomly named file on a path.
  The file is opened exclusively with `creation::only_if_not_exist` so it
  will never collide with nor overwrite any existing file. Note also
  that caching defaults to temporary which hints to the OS to only
  flush changes to physical storage as lately as possible.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static inline result<mapped_file_handle> mapped_random_file(size_type reservation, const path_handle &dirpath, mode _mode = mode::write, caching _caching = caching::temporary, flag flags = flag::none) noexcept
  {
    try
    {
      for(;;)
      {
        auto randomname = utils::random_string(32);
        randomname.append(".random");
        result<mapped_file_handle> ret = mapped_file(reservation, dirpath, randomname, _mode, creation::only_if_not_exist, _caching, flags);
        if(ret || (!ret && ret.error() != std::errc::file_exists))
          return ret;
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
  `mapped_random_file(temporary_files_directory())` and the creation
  parameter is ignored.

  \note If the temporary file you are creating is not going to have its
  path sent to another process for usage, this is the WRONG function
  to use. Use `temp_inode()` instead, it is far more secure.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static inline result<mapped_file_handle> mapped_temp_file(size_type reservation, path_view_type name = path_view_type(), mode _mode = mode::write, creation _creation = creation::if_needed, caching _caching = caching::temporary, flag flags = flag::unlink_on_close) noexcept
  {
    OUTCOME_TRY(tempdirh, path_handle::path(temporary_files_directory()));
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
  AFIO_MAKE_FREE_FUNCTION
  static AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<mapped_file_handle> mapped_temp_inode(path_view_type dirpath = temporary_files_directory(), mode _mode = mode::write, flag flags = flag::none) noexcept
  {
    // Open it overlapped, otherwise no difference.
    OUTCOME_TRY(v, file_handle::temp_inode(std::move(dirpath), std::move(_mode), flags));
    mapped_file_handle ret(std::move(v));
    return std::move(ret);
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
  char *address() const noexcept { return _mh.address(); }

  //! The length of the underlying file
  result<extent_type> underlying_file_length() const noexcept { return file_handle::length(); }

  AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override;
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC native_handle_type release() noexcept override;
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> barrier(io_request<const_buffers_type> reqs = io_request<const_buffers_type>(), bool wait_for_device = false, bool and_metadata = false, deadline d = deadline()) noexcept override
  {
    return _mh.barrier(std::move(reqs), wait_for_device, and_metadata, std::move(d));
  }
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<file_handle> clone() const noexcept override;
  //! Return the current maximum permitted extent of the file.
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> length() const noexcept override { return _mh.length(); }

  /*! \brief Resize the current maximum permitted extent of the mapped file to the given extent, avoiding any
  new allocation of physical storage where supported.

  Note that on extents based filing systems
  this will succeed even if there is insufficient free space on the storage medium. Only when
  pages are written to will the lack of sufficient free space be realised, resulting in an
  operating system specific exception.

  \note On Microsoft Windows you cannot shrink a file below any section handle's extent in any
  process in the system. We do, of course, shrink the internally held section handle correctly
  before truncating the underlying file. However you will need to coordinate with any other
  processes to shrink their section handles first. This is partly why `section()` is exposed.

  \return The bytes actually truncated to.
  \param newsize The bytes to truncate the file to. Zero causes the maps to be closed before
  truncation.
  */
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> truncate(extent_type newsize) noexcept override;
  /*! \brief Resize the mapping to match that of the underlying file, returning the size of the underlying file.

  If the internal section and map handle are invalid, they are restored unless the underlying file is zero length.
  */
  AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<extent_type> truncate() noexcept;

  AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> zero(extent_type offset, extent_type bytes, deadline /*unused*/ = deadline()) noexcept override
  {
    OUTCOME_TRYV(_mh.zero_memory({_mh.address() + offset, bytes}));
    return bytes;
  }

  /*! \brief Read data from the mapped file.

  \note Because this implementation never copies memory, you can pass in buffers with a null address.

  \return The buffers read, which will never be the buffers input because they will point into the mapped view.
  The size of each scatter-gather buffer is updated with the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors None, though the various signals and structured exception throws common to using memory maps may occur.
  \mallocs None.
  */
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept override { return _mh.read(std::move(reqs), std::move(d)); }
  /*! \brief Write data to the mapped file. Note this will never extend past the current length of the mapped file.

  \return The buffers written, which will never be the buffers input because they will point at where the data was copied into the mapped view.
  The size of each scatter-gather buffer is updated with the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors None, though the various signals and structured exception throws common to using memory maps may occur.
  \mallocs None.
  */
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept override { return _mh.write(std::move(reqs), std::move(d)); }
};

AFIO_V2_NAMESPACE_END

#if AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define AFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/mapped_file_handle.ipp"
#else
#include "detail/impl/posix/mapped_file_handle.ipp"
#endif
#undef AFIO_INCLUDED_BY_HEADER
#endif

#endif
