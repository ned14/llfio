/* A handle to a directory
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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
*/
class LLFIO_DECL directory_handle : public path_handle, public fs_handle
{
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC const handle &_get_handle() const noexcept final { return *this; }

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
  /*! The buffers type used by this handle, which is a contiguous sequence of `directory_entry`.

  \warning Unless you supply your own kernel buffer, you need to keep this around as long as you
  use any of the directory entries, as their leafnames are views of the original buffer filled by
  the kernel and the existence of this keeps that original buffer around.
  */
  struct buffers_type : public span<buffer_type>
  {
    using span<buffer_type>::span;
    //! Implicit construction from a span
    /* constexpr */ buffers_type(span<buffer_type> v)  // NOLINT TODO FIXME Make this constexpr when span becomes constexpr. SAME for move constructor below
    : span<buffer_type>(v)
    {
    }
    ~buffers_type() = default;
    //! Move constructor
    /* constexpr */ buffers_type(buffers_type &&o) noexcept : span<buffer_type>(std::move(o)), _kernel_buffer(std::move(o._kernel_buffer)), _kernel_buffer_size(o._kernel_buffer_size)
    {
      static_cast<span<buffer_type> &>(o) = {};
      o._kernel_buffer_size = 0;
    }
    //! No copy construction
    buffers_type(const buffers_type &) = delete;
    //! Move assignment
    buffers_type &operator=(buffers_type &&o) noexcept
    {
      this->~buffers_type();
      new(this) buffers_type(std::move(o));
      return *this;
    }
    //! No copy assignment
    buffers_type &operator=(const buffers_type &) = delete;

  private:
    friend class directory_handle;
    std::unique_ptr<char[]> _kernel_buffer;
    size_t _kernel_buffer_size{0};
    void _resize(size_t l) { *static_cast<span<buffer_type> *>(this) = this->subspan(0, l); }
  };

  //! How to do deleted file elimination on Windows
  enum class filter
  {
    none,        //!< Do no filtering at all
    fastdeleted  //!< Filter out AFIO deleted files based on their filename (fast and fairly reliable)
  };

public:
  //! Default constructor
  constexpr directory_handle() {}  // NOLINT
  //! Construct a directory_handle from a supplied native path_handle
  explicit constexpr directory_handle(native_handle_type h, dev_t devid, ino_t inode, caching caching = caching::all, flag flags = flag::none)
      : path_handle(std::move(h), caching, flags)
      , fs_handle(devid, inode)
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
  /*! Create a directory handle creating a randomly named file on a path.
  The file is opened exclusively with `creation::only_if_not_exist` so it
  will never collide with nor overwrite any existing entry.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<directory_handle> random_directory(const path_handle &dirpath, mode _mode = mode::write, caching _caching = caching::temporary, flag flags = flag::none) noexcept
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
  `random_file(path_discovery::storage_backed_temporary_files_directory())` and the creation
  parameter is ignored.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<directory_handle> temp_directory(path_view_type name = path_view_type(), mode _mode = mode::write, creation _creation = creation::if_needed, caching _caching = caching::all, flag flags = flag::none) noexcept
  {
    auto &tempdirh = path_discovery::storage_backed_temporary_files_directory();
    return name.empty() ? random_directory(tempdirh, _mode, _caching, flags) : directory(tempdirh, name, _mode, _creation, _caching, flags);
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
          return ret.error();
        }
      }
    }
    return path_handle::close();
  }

  /*! Clone this handle (copy constructor is disabled to avoid accidental copying),
  optionally race free reopening the handle with different access or caching.

  Microsoft Windows provides a syscall for cloning an existing handle but with new
  access. On POSIX, we must loop calling `current_path()`,
  trying to open the path returned and making sure it is the same inode.

  \errors Any of the values POSIX dup() or DuplicateHandle() can return.
  \mallocs On POSIX if changing the mode, we must loop calling `current_path()` and
  trying to open the path returned. Thus many allocations may occur.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<directory_handle> clone(mode mode_ = mode::unchanged, caching caching_ = caching::unchanged, deadline d = std::chrono::seconds(30)) const noexcept;

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

  //! Completion information for `enumerate()`
  struct enumerate_info
  {
    //! The buffers filled.
    buffers_type filled;
    //! The list of stat metadata retrieved by `enumerate()` this call per `buffer_type`.
    stat_t::want metadata;
    //! Whether the directory was entirely read or not.
    bool done;
  };
  /*! Fill the buffers type with as many directory entries as will fit.

  \return Returns the buffers filled, what metadata was filled in and whether the entire directory
  was read into `tofill`.
  \param tofill The buffers to fill, returned to you on exit.
  \param glob An optional shell glob by which to filter the items filled. Done kernel side on Windows, user side on POSIX.
  \param filtering Whether to filter out fake-deleted files on Windows or not.
  \param kernelbuffer A buffer to use for the kernel to fill. If left defaulted, a kernel buffer
  is allocated internally and stored into `tofill` which needs to not be destructed until one
  is no longer using any items within (leafnames are views onto the original kernel data).
  \errors todo
  \mallocs If the `kernelbuffer` parameter is set on entry, no memory allocations.
  If unset, at least one memory allocation, possibly more is performed.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<enumerate_info> enumerate(buffers_type &&tofill, path_view_type glob = path_view_type(), filter filtering = filter::fastdeleted, span<char> kernelbuffer = span<char>()) const noexcept;
};
inline std::ostream &operator<<(std::ostream &s, const directory_handle::filter &v)
{
  static constexpr const char *values[] = {"none", "fastdeleted"};
  if(static_cast<size_t>(v) >= sizeof(values) / sizeof(values[0]) || (values[static_cast<size_t>(v)] == nullptr))
  {
    return s << "afio::directory_handle::filter::<unknown>";
  }
  return s << "afio::directory_handle::filter::" << values[static_cast<size_t>(v)];
}
inline std::ostream &operator<<(std::ostream &s, const directory_handle::enumerate_info & /*unused*/)
{
  return s << "afio::directory_handle::enumerate_info";
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
inline result<directory_handle> directory(const path_handle &base, directory_handle::path_view_type path, directory_handle::mode _mode = directory_handle::mode::read, directory_handle::creation _creation = directory_handle::creation::open_existing, directory_handle::caching _caching = directory_handle::caching::all, directory_handle::flag flags = directory_handle::flag::none) noexcept
{
  return directory_handle::directory(std::forward<decltype(base)>(base), std::forward<decltype(path)>(path), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_creation)>(_creation), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! Create a directory handle creating a randomly named file on a path.
The file is opened exclusively with `creation::only_if_not_exist` so it
will never collide with nor overwrite any existing entry.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<directory_handle> random_directory(const path_handle &dirpath, directory_handle::mode _mode = directory_handle::mode::write, directory_handle::caching _caching = directory_handle::caching::temporary, directory_handle::flag flags = directory_handle::flag::none) noexcept
{
  return directory_handle::random_directory(std::forward<decltype(dirpath)>(dirpath), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! Create a directory handle creating the named directory on some path which
the OS declares to be suitable for temporary files.
Note also that an empty name is equivalent to calling
`random_file(path_discovery::storage_backed_temporary_files_directory())` and the creation
parameter is ignored.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<directory_handle> temp_directory(directory_handle::path_view_type name = directory_handle::path_view_type(), directory_handle::mode _mode = directory_handle::mode::write, directory_handle::creation _creation = directory_handle::creation::if_needed, directory_handle::caching _caching = directory_handle::caching::all, directory_handle::flag flags = directory_handle::flag::none) noexcept
{
  return directory_handle::temp_directory(std::forward<decltype(name)>(name), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_creation)>(_creation), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! Fill the buffers type with as many directory entries as will fit.

\return Returns the buffers filled, what metadata was filled in and whether the entire directory
was read into `tofill`.
\param self The object whose member function to call.
\param tofill The buffers to fill, returned to you on exit.
\param glob An optional shell glob by which to filter the items filled. Done kernel side on Windows, user side on POSIX.
\param filtering Whether to filter out fake-deleted files on Windows or not.
\param kernelbuffer A buffer to use for the kernel to fill. If left defaulted, a kernel buffer
is allocated internally and stored into `tofill` which needs to not be destructed until one
is no longer using any items within (leafnames are views onto the original kernel data).
\errors todo
\mallocs If the `kernelbuffer` parameter is set on entry, no memory allocations.
If unset, at least one memory allocation, possibly more is performed.
*/
inline result<directory_handle::enumerate_info> enumerate(const directory_handle &self, directory_handle::buffers_type &&tofill, directory_handle::path_view_type glob = directory_handle::path_view_type(), directory_handle::filter filtering = directory_handle::filter::fastdeleted, span<char> kernelbuffer = span<char>()) noexcept
{
  return self.enumerate(std::forward<decltype(tofill)>(tofill), std::forward<decltype(glob)>(glob), std::forward<decltype(filtering)>(filtering), std::forward<decltype(kernelbuffer)>(kernelbuffer));
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
