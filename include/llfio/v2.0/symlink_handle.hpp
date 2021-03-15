/* A handle to a symbolic link
(C) 2018-2021 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Jul 2018


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

#ifndef LLFIO_SYMLINK_HANDLE_H
#define LLFIO_SYMLINK_HANDLE_H

#include "handle.hpp"
#include "path_view.hpp"

//! \file symlink_handle.hpp Provides a handle to a symbolic link.

#ifndef LLFIO_SYMLINK_HANDLE_IS_FAKED
#if defined(_WIN32) || defined(__linux__)
#define LLFIO_SYMLINK_HANDLE_IS_FAKED 0
#else
#define LLFIO_SYMLINK_HANDLE_IS_FAKED 1
#endif
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

extern "C"
{
  struct stat;
}

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

class symlink_handle;

namespace detail
{
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<void> stat_from_symlink(struct stat &s, const handle &h) noexcept;
}

/*! \class symlink_handle
\brief A handle to an inode which redirects to a different path.

Microsoft Windows and Linux provide the ability to open the contents of a symbolic link directly,
for those platforms this handle works exactly like any ordinary handle. For other POSIX platforms
without proprietary extensions, it is not possible to get a valid file descriptor to the contents
of a symlink, and in this situation the native handle returned will be `-1` and the preprocessor
macro `LLFIO_SYMLINK_HANDLE_IS_FAKED` will be non-zero.

If `LLFIO_SYMLINK_HANDLE_IS_FAKED` is on, the handle is race free up to the containing directory
only. If a third party relocates the symbolic link into a different directory, and race free
checking is enabled, this class will simply refuse to work with `errc::no_such_file_or_directory`
as it no longer has any way of finding the symbolic link. You should take care that this does not
become a denial of service attack.

On Microsoft Windows, there are many kinds of symbolic link: this implementation supports
directory junctions, and NTFS symbolic links. Reads of any others will return an error
code comparing equal to `errc::protocol_not_supported`. One should note that modifying symbolic
links was not historically permitted by users with ordinary permissions on Microsoft Windows,
however recent versions of Windows 10 do support symbolic links for ordinary users. All versions
of Windows support directory symbolic links (junctions), these work for all users in any configuration.

(Note that to create a directory junction on Windows, first create a directory, open that
empty directory for modification using symlink_handle, then write using `symlink_type::win_junction`)
*/
class LLFIO_DECL symlink_handle : public handle, public fs_handle
{
#if LLFIO_SYMLINK_HANDLE_IS_FAKED
  // Need to retain a handle to our base and our leafname
  path_handle _dirh;
  handle::path_type _leafname;
#endif
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC const handle &_get_handle() const noexcept final { return *this; }

#ifndef _WIN32
  friend result<void> detail::stat_from_symlink(struct stat &s, const handle &h) noexcept;
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> _create_symlink(const path_handle &dirh, const handle::path_type &filename, path_view target, deadline d,
                                                               bool atomic_replace, bool exists_is_ok) noexcept;
#endif

public:
  using path_type = handle::path_type;
  using extent_type = handle::extent_type;
  using size_type = handle::size_type;
  using mode = handle::mode;
  using creation = handle::creation;
  using caching = handle::caching;
  using flag = handle::flag;
  using dev_t = fs_handle::dev_t;
  using ino_t = fs_handle::ino_t;
  using path_view_type = fs_handle::path_view_type;

  //! The type of symbolic link this is
  enum class symlink_type
  {
    none,      //!<! No link
    symbolic,  //!< Standard symbolic link

    win_wsl,      //!< WSL symbolic link (Windows only, not actually implemented currently)
    win_junction  //!< NTFS directory junction (Windows only, directories and volumes only)
  };

  //! The buffer type used by this handle, which is a `path_view`
  using buffer_type = path_view;
  //! The const buffer type used by this handle, which is a `path_view`
  using const_buffer_type = path_view;
  /*! The buffers type used by this handle for reads, which is a single item sequence of `path_view`.

  \warning Unless you supply your own kernel buffer, you need to keep this around as long as you
  use the path view, as the path is a view of the original buffer filled by
  the kernel and the existence of this keeps that original buffer around.
  */
  struct buffers_type
  {
    //! Type of the pointer to the buffer.
    using pointer = path_view *;
    //! Type of the iterator to the buffer.
    using iterator = path_view *;
    //! Type of the iterator to the buffer.
    using const_iterator = const path_view *;
    //! Type of the length of the buffers.
    using size_type = size_t;

    //! Default constructor
    constexpr buffers_type() {}  // NOLINT

    /*! Constructor
     */
    constexpr buffers_type(path_view link, symlink_type type = symlink_type::symbolic)
        : _link(link)
        , _type(type)
    {
    }
    ~buffers_type() = default;
    //! Move constructor
    buffers_type(buffers_type &&o) noexcept
        : _link(o._link)
        , _type(o._type)
        , _kernel_buffer(std::move(o._kernel_buffer))
        , _kernel_buffer_size(o._kernel_buffer_size)
    {
      o._link = {};
      o._type = symlink_type::none;
      o._kernel_buffer_size = 0;
    }
    //! No copy construction
    buffers_type(const buffers_type &) = delete;
    //! Move assignment
    buffers_type &operator=(buffers_type &&o) noexcept
    {
      if(this == &o)
      {
        return *this;
      }
      this->~buffers_type();
      new(this) buffers_type(std::move(o));
      return *this;
    }
    //! No copy assignment
    buffers_type &operator=(const buffers_type &) = delete;

    //! Returns an iterator to the beginning of the buffers
    constexpr iterator begin() noexcept { return &_link; }
    //! Returns an iterator to the beginning of the buffers
    constexpr const_iterator begin() const noexcept { return &_link; }
    //! Returns an iterator to the beginning of the buffers
    constexpr const_iterator cbegin() const noexcept { return &_link; }
    //! Returns an iterator to after the end of the buffers
    constexpr iterator end() noexcept { return &_link + 1; }
    //! Returns an iterator to after the end of the buffers
    constexpr const_iterator end() const noexcept { return &_link + 1; }
    //! Returns an iterator to after the end of the buffers
    constexpr const_iterator cend() const noexcept { return &_link + 1; }

    //! The path referenced by the symbolic link
    path_view path() const noexcept { return _link; }
    //! The type of the symbolic link
    symlink_type type() const noexcept { return _type; }

  private:
    friend class symlink_handle;
    path_view _link;
    symlink_type _type{symlink_type::none};
    std::unique_ptr<char[]> _kernel_buffer;
    size_t _kernel_buffer_size{0};
  };
  /*! The constant buffers type used by this handle for writes, which is a single item sequence of `path_view`.
   */
  struct const_buffers_type
  {
    //! Type of the pointer to the buffer.
    using pointer = const path_view *;
    //! Type of the iterator to the buffer.
    using iterator = const path_view *;
    //! Type of the iterator to the buffer.
    using const_iterator = const path_view *;
    //! Type of the length of the buffers.
    using size_type = size_t;

    //! Constructor
    constexpr const_buffers_type(path_view link, symlink_type type = symlink_type::symbolic)
        : _link(link)
        , _type(type)
    {
    }
    ~const_buffers_type() = default;
    //! Move constructor
    const_buffers_type(const_buffers_type &&o) noexcept
        : _link(o._link)
        , _type(o._type)
    {
      o._link = {};
      o._type = symlink_type::none;
    }
    //! No copy construction
    const_buffers_type(const buffers_type &) = delete;
    //! Move assignment
    const_buffers_type &operator=(const_buffers_type &&o) noexcept
    {
      if(this == &o)
      {
        return *this;
      }
      this->~const_buffers_type();
      new(this) const_buffers_type(std::move(o));
      return *this;
    }
    //! No copy assignment
    const_buffers_type &operator=(const const_buffers_type &) = delete;

    //! Returns an iterator to the beginning of the buffers
    constexpr iterator begin() noexcept { return &_link; }
    //! Returns an iterator to the beginning of the buffers
    constexpr const_iterator begin() const noexcept { return &_link; }
    //! Returns an iterator to the beginning of the buffers
    constexpr const_iterator cbegin() const noexcept { return &_link; }
    //! Returns an iterator to after the end of the buffers
    constexpr iterator end() noexcept { return &_link + 1; }
    //! Returns an iterator to after the end of the buffers
    constexpr const_iterator end() const noexcept { return &_link + 1; }
    //! Returns an iterator to after the end of the buffers
    constexpr const_iterator cend() const noexcept { return &_link + 1; }

    //! The path referenced by the symbolic link
    path_view path() const noexcept { return _link; }
    //! The type of the symbolic link
    symlink_type type() const noexcept { return _type; }

  private:
    friend class symlink_handle;
    path_view _link;
    symlink_type _type{symlink_type::none};
  };
  //! The i/o request type used by this handle.
  template <class T, bool = true> struct io_request;
  //! Specialisation for reading symlinks
  template <bool ____> struct io_request<buffers_type, ____>  // workaround lack of nested specialisation support on older compilers
  {
    span<char> kernelbuffer{};

    constexpr io_request() {}  // NOLINT
    //! Construct a request to read a link with optionally specified kernel buffer
    constexpr io_request(span<char> _kernelbuffer)
        : kernelbuffer(_kernelbuffer)
    {
    }
    //! Convenience constructor constructing from anything a `span<char>` can construct from
    LLFIO_TEMPLATE(class... Args)
    LLFIO_TREQUIRES(LLFIO_TPRED(std::is_constructible<span<char>, Args...>::value))
    constexpr io_request(Args &&... args) noexcept
        : io_request(span<char>(static_cast<Args &&>(args)...))
    {
    }
  };
  //! Specialisation for writing symlinks
  template <bool ____> struct io_request<const_buffers_type, ____>  // workaround lack of nested specialisation support on older compilers
  {
    const_buffers_type buffers;
    span<char> kernelbuffer;
    //! Construct a request to write a link with optionally specified kernel buffer
    io_request(const_buffers_type _buffers, span<char> _kernelbuffer = span<char>())
        : buffers(std::move(_buffers))
        , kernelbuffer(_kernelbuffer)
    {
    }
    //! Convenience constructor constructing from anything a `path_view` can construct from
    LLFIO_TEMPLATE(class... Args)
    LLFIO_TREQUIRES(LLFIO_TPRED(std::is_constructible<path_view, Args...>::value))
    constexpr io_request(Args &&... args) noexcept
        : buffers(path_view(static_cast<Args &&>(args)...))
    {
    }
    //! Convenience constructor constructing a specific type of link from anything a `path_view` can construct from
    LLFIO_TEMPLATE(class... Args)
    LLFIO_TREQUIRES(LLFIO_TPRED(std::is_constructible<path_view, Args...>::value))
    constexpr io_request(symlink_type type, Args &&... args) noexcept
        : buffers(path_view(static_cast<Args &&>(args)...), type)
    {
    }
  };

//! Default constructor
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
  constexpr
#endif
  symlink_handle()
  {
  }  // NOLINT
//! Construct a handle from a supplied native handle
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
  constexpr
#endif
  explicit symlink_handle(native_handle_type h, dev_t devid, ino_t inode, flag flags = flag::none)
      : handle(std::move(h), caching::all, flags)
      , fs_handle(devid, inode)
  {
  }
//! Explicit conversion from handle permitted
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
  constexpr
#endif
  explicit symlink_handle(handle &&o) noexcept
      : handle(std::move(o))
  {
  }
  //! Move construction permitted
  symlink_handle(symlink_handle &&) = default;
  //! No copy construction (use `clone()`)
  symlink_handle(const symlink_handle &) = delete;
  //! Move assignment permitted
  symlink_handle &operator=(symlink_handle &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    this->~symlink_handle();
    new(this) symlink_handle(std::move(o));
    return *this;
  }
  //! No copy assignment
  symlink_handle &operator=(const symlink_handle &) = delete;
  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(symlink_handle &o) noexcept
  {
    symlink_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~symlink_handle() override
  {
    if(_v)
    {
      (void) symlink_handle::close();
    }
  }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(_flags & flag::unlink_on_first_close)
    {
      auto ret = unlink();
      if(!ret)
      {
        // File may have already been deleted, if so ignore
        if(ret.error() != errc::no_such_file_or_directory)
        {
          return std::move(ret).error();
        }
      }
    }
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
#ifndef NDEBUG
    if(_v)
    {
      // Tell handle::close() that we have correctly executed
      _v.behaviour |= native_handle_type::disposition::_child_close_executed;
    }
#endif
    return handle::close();
#else
    _dirh = {};
    _leafname = path_type();
    _v = {};
    return success();
#endif
  }

  /*! Reopen this handle (copy constructor is disabled to avoid accidental copying),
  optionally race free reopening the handle with different access or caching.

  Microsoft Windows provides a syscall for cloning an existing handle but with new
  access. On POSIX, we must loop calling `current_path()`,
  trying to open the path returned and making sure it is the same inode.

  \errors Any of the values POSIX dup() or DuplicateHandle() can return.
  \mallocs On POSIX if changing the mode, we must loop calling `current_path()` and
  trying to open the path returned. Thus many allocations may occur.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<symlink_handle> reopen(mode mode_ = mode::unchanged, deadline d = std::chrono::seconds(30)) const noexcept;
  LLFIO_DEADLINE_TRY_FOR_UNTIL(reopen)

#if LLFIO_SYMLINK_HANDLE_IS_FAKED
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<path_type> current_path() const noexcept override;
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC
  result<void> relink(const path_handle &base, path_view_type path, bool atomic_replace = true, deadline d = std::chrono::seconds(30)) noexcept override;
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC
  result<void> unlink(deadline d = std::chrono::seconds(30)) noexcept override;
#endif

  /*! Create a symlink handle opening access to a symbolic link.

  For obvious reasons, one cannot append to a symbolic link, nor create with truncate.
  In this situation a failure comparing equal to `errc::function_not_supported` shall
  be returned.

  \errors Any of the values POSIX open() or CreateFile() can return.
  \mallocs None, unless `LLFIO_SYMLINK_HANDLE_IS_FAKED` is on, in which case one.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<symlink_handle> symlink(const path_handle &base, path_view_type path, mode _mode = mode::read,
                                                                        creation _creation = creation::open_existing, flag flags = flag::none) noexcept;
  /*! Create a symlink handle creating a uniquely named symlink on a path.
  The symlink is opened exclusively with `creation::only_if_not_exist` so it
  will never collide with nor overwrite any existing symlink.

  \errors Any of the values POSIX open() or CreateFile() can return,
  or failure to allocate memory.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<symlink_handle> uniquely_named_symlink(const path_handle &dirpath, mode _mode = mode::write, flag flags = flag::none) noexcept
  {
    try
    {
      for(;;)
      {
        auto randomname = utils::random_string(32);
        randomname.append(".random");
        result<symlink_handle> ret = symlink(dirpath, randomname, _mode, creation::only_if_not_exist, flags);
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

  /*! Read the contents of the symbolic link.

  If supplying your own `kernelbuffer`, be aware that the length of the contents of the symbolic
  link may change at any time. You should therefore retry reading the symbolic link, expanding
  your `kernelbuffer` each time, until a successful read occurs.

  \return Returns the buffers filled, with its path adjusted to the bytes filled.
  \param req A buffer to fill with the contents of the symbolic link.
  \errors Any of the errors which `readlinkat()` or `DeviceIoControl()` might return, or failure
  to allocate memory if the user did not supply a kernel buffer to use, or the user supplied buffer
  was too small.
  \mallocs If the `kernelbuffer` parameter is set in the request, no memory allocations.
  If unset, at least one memory allocation, possibly more is performed.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<buffers_type> read(io_request<buffers_type> req = {}) noexcept;

  /*! Write the contents of the symbolic link.

  \param req A buffer with which to replace the contents of the symbolic link.
  \param d An optional deadline by which the i/o must complete, else it is cancelled. Ignored
  on Windows.
  \errors Any of the errors which `symlinkat()` or `DeviceIoControl()` might return.
  \mallocs On Windows, if the `kernelbuffer` parameter is set on entry, no memory allocations.
  If unset, then at least one memory allocation, possibly more is performed. On POSIX,
  at least one memory allocation.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<const_buffers_type> write(io_request<const_buffers_type> req, deadline d = deadline()) noexcept;
};

//! \brief Constructor for `symlink_handle`
template <> struct construct<symlink_handle>
{
  const path_handle &base;
  symlink_handle::path_view_type _path;
  symlink_handle::mode _mode{symlink_handle::mode::read};
  symlink_handle::creation _creation{symlink_handle::creation::open_existing};
  result<symlink_handle> operator()() const noexcept { return symlink_handle::symlink(base, _path, _mode, _creation); }
};


LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/symlink_handle.ipp"
#else
#include "detail/impl/posix/symlink_handle.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
