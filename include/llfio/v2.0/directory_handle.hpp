/* A handle to a directory
(C) 2017-2021 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Aug 2017


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

#ifndef LLFIO_DIRECTORY_HANDLE_H
#define LLFIO_DIRECTORY_HANDLE_H

#include "path_discovery.hpp"
#include "stat.hpp"

#include <memory>  // for unique_ptr

//! \file directory_handle.hpp Provides a handle to a directory.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

struct directory_entry
{
  //! The leafname of the directory entry
  path_view leafname;
  //! The metadata retrieved for the directory entry
  stat_t stat;
};
#ifndef NDEBUG
// Is trivial in all ways, except default constructibility
static_assert(std::is_trivially_copyable<directory_entry>::value, "directory_entry is not trivially copyable!");
static_assert(std::is_trivially_assignable<directory_entry, directory_entry>::value, "directory_entry is not trivially assignable!");
static_assert(std::is_trivially_destructible<directory_entry>::value, "directory_entry is not trivially destructible!");
static_assert(std::is_trivially_copy_constructible<directory_entry>::value, "directory_entry is not trivially copy constructible!");
static_assert(std::is_trivially_move_constructible<directory_entry>::value, "directory_entry is not trivially move constructible!");
static_assert(std::is_trivially_copy_assignable<directory_entry>::value, "directory_entry is not trivially copy assignable!");
static_assert(std::is_trivially_move_assignable<directory_entry>::value, "directory_entry is not trivially move assignable!");
static_assert(std::is_standard_layout<directory_entry>::value, "directory_entry is not a standard layout type!");
#endif

/*! \class directory_handle
\brief A handle to a directory which can be enumerated.

\note For good performance, make sure you reuse `buffers_type` across calls to `read()`, including
across different instances of `directory_handle`. This is because a kernel buffer is allocated
within the first use of a `buffers_type` in a `read()`, so reusing `buffers_type` will save on
an allocation-free cycle per directory enumeration.
*/
class LLFIO_DECL directory_handle : public path_handle, public fs_handle
{
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC const handle &_get_handle() const noexcept final { return *this; }

  mutable std::atomic<unsigned> _lock{0};  // used to serialise read()

public:
  using path_type = path_handle::path_type;
  using extent_type = path_handle::extent_type;
  using size_type = path_handle::size_type;
  using mode = path_handle::mode;
  using creation = path_handle::creation;
  using caching = path_handle::caching;
  using flag = path_handle::flag;
  using dev_t = fs_handle::dev_t;
  using ino_t = fs_handle::ino_t;
  using path_view_type = fs_handle::path_view_type;

  //! The buffer type used by this handle, which is a `directory_entry`
  using buffer_type = directory_entry;
  //! The const buffer type used by this handle, which is a `directory_entry`
  using const_buffer_type = directory_entry;
  /*! The buffers type used by this handle, which is a contiguous sequence of `directory_entry`.

  \warning Unless you supply your own kernel buffer, you need to keep this around as long as you
  use any of the directory entries, as their leafnames are views of the original buffer filled by
  the kernel and the existence of this keeps that original buffer around.
  */
  struct buffers_type : public span<buffer_type>
  {
    using _base = span<buffer_type>;
    /*! The list of stat metadata retrieved. Sometimes, due to kernel API design,
    enumerating a directory retrieves more than the metadata requested in the read
    request. This indidicates what stat metadata is in the buffers filled.
    */
    stat_t::want metadata() const noexcept { return _metadata; }
    //! Whether the directory was entirely read or not into any buffers supplied.
    bool done() const noexcept { return _done; }

    using _base::_base;
    buffers_type() = default;
    //! Implicit construction from a span
    /* constexpr */ buffers_type(span<buffer_type> v)  // NOLINT TODO FIXME Make this constexpr when span becomes constexpr. SAME for move constructor below
    : _base(v)
    {
    }
    //! Construct from a span, using a kernel buffer from a preceding `buffers_type`.
    buffers_type(span<buffer_type> v, buffers_type &&o) noexcept
        : _base(std::move(v))
        , _kernel_buffer(std::move(o._kernel_buffer))
        , _kernel_buffer_size(o._kernel_buffer_size)
        , _metadata(o._metadata)
        , _done(o._done)
    {
      static_cast<_base &>(o) = {};
      o._kernel_buffer_size = 0;
    }
    ~buffers_type() = default;
    //! Move constructor
    /* constexpr */ buffers_type(buffers_type &&o) noexcept : _base(std::move(o)), _kernel_buffer(std::move(o._kernel_buffer)), _kernel_buffer_size(o._kernel_buffer_size), _metadata(o._metadata), _done(o._done)
    {
      static_cast<_base &>(o) = {};
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
      std::unique_ptr<char[]> kernel_buffer = std::move(_kernel_buffer);
      size_t kernel_buffer_size = _kernel_buffer_size;
      this->~buffers_type();
      new(this) buffers_type(std::move(o));
      if(kernel_buffer_size > _kernel_buffer_size)
      {
        _kernel_buffer.reset();
        _kernel_buffer = std::move(kernel_buffer);
        _kernel_buffer_size = kernel_buffer_size;
      }
      return *this;
    }
    //! No copy assignment
    buffers_type &operator=(const buffers_type &) = delete;

  private:
    friend class directory_handle;
    std::unique_ptr<char[]> _kernel_buffer;
    size_t _kernel_buffer_size{0};
    void _resize(size_t l) { *static_cast<span<buffer_type> *>(this) = this->subspan(0, l); }
    stat_t::want _metadata{stat_t::want::none};
    bool _done{false};
  };
  //! How to do deleted file elimination on Windows
  enum class filter
  {
    none,        //!< Do no filtering at all
    fastdeleted  //!< For Windows without POSIX delete semantics, filter out LLFIO deleted files based on their filename (fast and fairly reliable)
  };
  //! The i/o request type used by this handle.
  template <class /* unused */> struct io_request
  {
    buffers_type buffers{};
    path_view_type glob{};
    filter filtering{filter::fastdeleted};
    span<char> kernelbuffer{};

    /*! Construct a request to enumerate a directory with optionally specified kernel buffer.

    \param _buffers The buffers to fill with enumerated directory entries.
    \param _glob An optional shell glob by which to filter the items filled. Done kernel side on Windows, user side on POSIX.
    \param _filtering Whether to filter out fake-deleted files on Windows or not.
    \param _kernelbuffer A buffer to use for the kernel to fill. If left defaulted, a kernel buffer
    is allocated internally and returned in the buffers returned which needs to not be destructed until one
    is no longer using any items within (leafnames are views onto the original kernel data).
    */
    /*constexpr*/ io_request(buffers_type _buffers, path_view_type _glob = {}, filter _filtering = filter::fastdeleted, span<char> _kernelbuffer = {})
        : buffers(std::move(_buffers))
        , glob(_glob)
        , filtering(_filtering)
        , kernelbuffer(_kernelbuffer)
    {
    }
  };


public:
  //! Default constructor
  constexpr directory_handle() {}  // NOLINT
  //! Construct a directory_handle from a supplied native path_handle
  explicit constexpr directory_handle(native_handle_type h, dev_t devid, ino_t inode, caching caching, flag flags)
      : path_handle(std::move(h), caching, flags)
      , fs_handle(devid, inode)
  {
  }
  //! Construct a directory_handle from a supplied native path_handle
  explicit constexpr directory_handle(native_handle_type h, caching caching, flag flags)
      : path_handle(std::move(h), caching, flags)
  {
  }
  //! Implicit move construction of directory_handle permitted
  constexpr directory_handle(directory_handle &&o) noexcept : path_handle(std::move(o)), fs_handle(std::move(o)) {}
  //! No copy construction (use `clone()`)
  directory_handle(const directory_handle &) = delete;
  //! Explicit conversion from handle permitted
  explicit constexpr directory_handle(handle &&o, dev_t devid, ino_t inode) noexcept : path_handle(std::move(o)), fs_handle(devid, inode) {}
  //! Move assignment of directory_handle permitted
  directory_handle &operator=(directory_handle &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    this->~directory_handle();
    new(this) directory_handle(std::move(o));
    return *this;
  }
  //! No copy assignment
  directory_handle &operator=(const directory_handle &) = delete;
  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(directory_handle &o) noexcept
  {
    directory_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  /*! Create a handle opening access to a directory on path.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<directory_handle> directory(const path_handle &base, path_view_type path, mode _mode = mode::read, creation _creation = creation::open_existing, caching _caching = caching::all, flag flags = flag::none) noexcept;
  /*! Create a directory handle creating a uniquely named file on a path.
  The file is opened exclusively with `creation::only_if_not_exist` so it
  will never collide with nor overwrite any existing entry.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<directory_handle> uniquely_named_directory(const path_handle &dirpath, mode _mode = mode::write, caching _caching = caching::temporary, flag flags = flag::none) noexcept
  {
    try
    {
      for(;;)
      {
        auto randomname = utils::random_string(32);
        result<directory_handle> ret = directory(dirpath, randomname, _mode, creation::only_if_not_exist, _caching, flags);
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
  /*! Create a directory handle creating the named directory on some path which
  the OS declares to be suitable for temporary files.
  Note also that an empty name is equivalent to calling
  `uniquely_named_file(path_discovery::storage_backed_temporary_files_directory())` and the creation
  parameter is ignored.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<directory_handle> temp_directory(path_view_type name = path_view_type(), mode _mode = mode::write, creation _creation = creation::if_needed, caching _caching = caching::all, flag flags = flag::none) noexcept
  {
    auto &tempdirh = path_discovery::storage_backed_temporary_files_directory();
    return name.empty() ? uniquely_named_directory(tempdirh, _mode, _caching, flags) : directory(tempdirh, name, _mode, _creation, _caching, flags);
  }

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~directory_handle() override
  {
    if(_v)
    {
      (void) directory_handle::close();
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
    return path_handle::close();
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
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<directory_handle> reopen(mode mode_ = mode::unchanged, caching caching_ = caching::unchanged, deadline d = std::chrono::seconds(30)) const noexcept;
  LLFIO_DEADLINE_TRY_FOR_UNTIL(reopen)

  /*! Return a copy of this directory handle, but as a path handle.

  \errors Any of the values POSIX dup() or DuplicateHandle() can return.
  \mallocs On POSIX, we must loop calling `current_path()` and
  trying to open the path returned. Thus many allocations may occur.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<path_handle> clone_to_path_handle() const noexcept;

#ifdef _WIN32
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC
  result<void> relink(const path_handle &base, path_view_type newpath, bool atomic_replace = true, deadline d = std::chrono::seconds(30)) noexcept override;
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC
  result<void> unlink(deadline d = std::chrono::seconds(30)) noexcept override;
#endif

  /*! Fill the buffers type with as many directory entries as will fit into any optionally supplied buffer.
  This operation returns a **snapshot**, without races, of the directory contents at the moment of the
  call.

  \return Returns the buffers filled, what metadata was filled in and whether the entire directory
  was read or not. You should *always* examine `.metadata()` for the metadata you are about to use,
  fetching it with `stat_t::fill()` if not yet present.
  \param req A buffer fill (directory enumeration) request.
  \errors todo
  \mallocs If the `kernelbuffer` parameter is set in the request, no memory allocations.
  If unset, at least one memory allocation, possibly more is performed. MAKE SURE you reuse the
  `buffers_type` across calls once you are no longer using the buffers filled (simply restamp
  its span range, the internal kernel buffer will then get reused).
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<buffers_type> read(io_request<buffers_type> req, deadline d = std::chrono::seconds(30)) const noexcept;
};
inline std::ostream &operator<<(std::ostream &s, const directory_handle::filter &v)
{
  static constexpr const char *values[] = {"none", "fastdeleted"};
  if(static_cast<size_t>(v) >= sizeof(values) / sizeof(values[0]) || (values[static_cast<size_t>(v)] == nullptr))
  {
    return s << "llfio::directory_handle::filter::<unknown>";
  }
  return s << "llfio::directory_handle::filter::" << values[static_cast<size_t>(v)];
}
inline std::ostream &operator<<(std::ostream &s, const directory_handle::buffers_type & /*unused*/)
{
  return s << "llfio::directory_handle::buffers_type";
}

//! \brief Constructor for `directory_handle`
template <> struct construct<directory_handle>
{
  const path_handle &base;
  directory_handle::path_view_type _path;
  directory_handle::mode _mode = directory_handle::mode::read;
  directory_handle::creation _creation = directory_handle::creation::open_existing;
  directory_handle::caching _caching = directory_handle::caching::all;
  directory_handle::flag flags = directory_handle::flag::none;
  result<directory_handle> operator()() const noexcept { return directory_handle::directory(base, _path, _mode, _creation, _caching, flags); }
};

// BEGIN make_free_functions.py
//! Swap with another instance
inline void swap(directory_handle &self, directory_handle &o) noexcept
{
  return self.swap(std::forward<decltype(o)>(o));
}
/*! Create a handle opening access to a directory on path.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<directory_handle> directory(const path_handle &base, directory_handle::path_view_type path, directory_handle::mode _mode = directory_handle::mode::read, directory_handle::creation _creation = directory_handle::creation::open_existing, directory_handle::caching _caching = directory_handle::caching::all,
                                          directory_handle::flag flags = directory_handle::flag::none) noexcept
{
  return directory_handle::directory(std::forward<decltype(base)>(base), std::forward<decltype(path)>(path), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_creation)>(_creation), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! Create a directory handle creating a randomly named file on a path.
The file is opened exclusively with `creation::only_if_not_exist` so it
will never collide with nor overwrite any existing entry.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<directory_handle> uniquely_named_directory(const path_handle &dirpath, directory_handle::mode _mode = directory_handle::mode::write, directory_handle::caching _caching = directory_handle::caching::temporary, directory_handle::flag flags = directory_handle::flag::none) noexcept
{
  return directory_handle::uniquely_named_directory(std::forward<decltype(dirpath)>(dirpath), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! Create a directory handle creating the named directory on some path which
the OS declares to be suitable for temporary files.
Note also that an empty name is equivalent to calling
`uniquely_named_file(path_discovery::storage_backed_temporary_files_directory())` and the creation
parameter is ignored.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<directory_handle> temp_directory(directory_handle::path_view_type name = directory_handle::path_view_type(), directory_handle::mode _mode = directory_handle::mode::write, directory_handle::creation _creation = directory_handle::creation::if_needed,
                                               directory_handle::caching _caching = directory_handle::caching::all, directory_handle::flag flags = directory_handle::flag::none) noexcept
{
  return directory_handle::temp_directory(std::forward<decltype(name)>(name), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_creation)>(_creation), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/directory_handle.ipp"
#else
#include "detail/impl/posix/directory_handle.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
