/* Atomically replace a file's contents
(C) 2026 d3fault <d3fault@d3fault.net> (1 commit)
File Created: Oct 2026


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

#ifndef LLFIO_ATOMIC_FILE_HPP
#define LLFIO_ATOMIC_FILE_HPP

#include "directory_handle.hpp"
#include "file_handle.hpp"

//! \file atomic_file.hpp Provides a transaction for atomically replacing the contents of a file.

LLFIO_V2_NAMESPACE_BEGIN

/*! \class atomic_file
\brief A write-only transaction which publishes its contents to a destination path atomically and durably.

This is a small, self-contained "safe save" abstraction. You write new content to it sequentially,
and only when you `commit()` does the destination path get atomically and durably replaced. If you
never commit, the destination path is left completely untouched.

A typical use looks like this:

```cpp
// Replace /path/to/file.txt with "new contents", atomically.
LLFIO_NAMESPACE::atomic_file f = LLFIO_NAMESPACE::atomic_file::open(
    LLFIO_NAMESPACE::path_handle(), "/path/to/file.txt").value();
f.write({LLFIO_NAMESPACE::byte_io_handle::const_buffer_type(
    reinterpret_cast<const LLFIO_NAMESPACE::byte *>("new contents"), 12)}).value();
f.commit().value();
// If we get here, /path/to/file.txt now contains "new contents", and that
// fact survives a power loss.
```

\section atomic_file_guarantees What is guaranteed

1. The destination path is never observed in a partially written state. A concurrent reader sees
either the old content or the new content, never a mixture. This holds because the new content is
written to a temporary file in the *same directory* as the destination, and the destination is
published by an atomic rename (`fs_handle::relink()` with `atomic_replace = true`).
2. If you never call `commit()`, the destination path is left exactly as it was. The temporary file
is removed when the `atomic_file` is destroyed.
3. After a successful `commit()` returns, the new content and the directory entry naming it have
been flushed to storage, so a subsequent power loss will not revert the destination to its old
contents (subject to the storage stack actually honouring flushes, see caveats below).
4. Writes are buffered by the OS as normal. There is **no** per-write barrier or per-write
`fsync()`. Durability is established exactly once, at commit time.

\section atomic_file_caveats Caveats and what is *not* guaranteed

- **Metadata durability on a directory which cannot be opened for reading** is not supported. To
flush the directory entry we open the containing directory with `directory_handle::directory()`,
which requires read permission on that directory. Directories which may be written but not read
(a rare configuration) will cause `open()` to fail.
- **Permissions of an existing destination are preserved.** If the destination is a regular file when
`commit()` is reached, its permission bits are read and applied to the temporary file before the
replacement, so the new contents keep the mode of the file they replace. Without this an atomic save
would silently change the mode of the destination - dropping the executable bit off a script, or
widening a 0600 file to the umask default - which is both surprising and a security problem. This is
what `QSaveFile` does. The mode is read without opening the destination, so it does not matter
whether the caller can read the destination's contents. If the destination does not exist, is not a
regular file (a symbolic link, say, whose mode is 0777 and describes the link rather than the file
which replaces it), or lives on a filing system which does not report or apply permissions, the
temporary file keeps the mode it was created with, subject to the process umask. This step is best
effort and never fails the commit. Ownership, ACLs and extended attributes are **not** copied, only
the permission bits; if you need those preserved, do so yourself.
- **The fallback mode is not atomic.** See `options::fallback` and \ref atomic_file_fallback
below. When it is active, guarantee (1) and guarantee (2) do not hold.
- **Storage stacks which lie about flushes** (some cheap USB sticks, some virtualised block layers)
will break guarantee (3) no matter what this class does.
- On Windows, directory metadata durability relies upon the OS and filesystem accepting a flush of
a directory handle (see `directory_handle::barrier()`). If the platform returns an error for that
step, `commit()` reports it (see below), and `replacement_performed()` will tell you whether the
destination had already been replaced.

\section atomic_file_commit_failure When commit() fails partway through

`commit()` performs, in order: (i) if the destination is a regular file, its permission bits are
applied to the temporary file (see the permissions caveat above), (ii) a single `barrier_kind::wait_all`
barrier of
the temporary file, (iii) an atomic replacement of the destination with the temporary file, and (iv)
a `directory_handle::barrier()` of the containing directory, which flushes its metadata.

If step (ii) or (iii) fails, the destination has *not* been replaced, the temporary file is
discarded, and `commit()` returns the failure. Step (i) cannot fail the commit; it is best effort.

If step (iv) fails, the destination *has* already been replaced with the new content (this is
unavoidable: the rename is a single atomic operation and there is no way to undo it), but the
durability of that replacement is not confirmed. In that case `commit()` returns the failure from
step (iv), and `replacement_performed()` returns true so you can tell the two situations apart.

\section atomic_file_fallback Which failures activate the non-atomic fallback

`options::fallback` defaults to `false`, so by default a temporary file which cannot be created is
simply an error. When it is explicitly enabled, exactly **one** class of failure to create that
temporary file diverts the transaction into writing the destination in place:

- **`errc::permission_denied` (`EACCES`) - this one falls back.** It means the containing directory
  itself refuses to let this process add a new name to it. That says nothing about the destination's
  *own inode*, which is a separate permission and frequently may be written - it is precisely the
  "save a document which lives in a directory I am not allowed to write" case that the fallback
  exists for. It is also the only case `QSaveFile` falls back for: its POSIX fallback is guarded by
  `errno == EACCES` (`qsavefile.cpp`). Note that `QSaveFile` also requires the destination to exist
  and to be user-writable before it even attempts the temporary file, so its `EACCES` arm is only
  ever reached in that same situation.

**Every other failure to create the temporary file is returned to the caller unchanged, and no
fallback happens.** In particular:

- `errc::no_space_on_device`, `errc::file_too_large` (`ENOSPC`, `EDQUOT`, `EFBIG`, and whatever
  else a full or quota-exhausted filing system reports): resource exhaustion. Falling back here
  would be worse than useless: the fallback truncates the destination as it starts, so a save which
  would have failed cleanly, leaving the old contents intact, would instead destroy those contents
  on its way to failing with the same error. Direct writing is also unlikely to succeed for the
  same reason the temporary file could not be allocated.
- `errc::too_many_files_open`, `errc::too_many_files_open_in_system`, `errc::not_enough_memory`
  (`EMFILE`, `ENFILE`, `ENOMEM`): the same kind of exhaustion. The direct open needs a descriptor
  and memory too, so the fallback would simply fail one step later, having already thrown the
  destination away.
- `errc::read_only_file_system` (`EROFS`): the destination's inode is not writable either, so the
  fallback cannot help; reporting the original error is the more honest answer.
- `errc::no_such_file_or_directory`, `errc::not_a_directory` (`ENOENT`, `ENOTDIR`): the directory
  which `open()` resolved has gone or changed underneath us. Quietly writing *some* destination in
  place is not an improvement, and appearing to succeed would hide a real third-party race.
- `errc::operation_not_permitted` (`EPERM`): what a protected directory (`fs.protected_regular=2`,
  sticky directories) returns when it deliberately denies the creation of a new name. Reading that
  as "then write the existing file in place instead" would subvert the protection and turn an
  explicit refusal into an in-place overwrite. `QSaveFile` does not fall back for it either.
- `errc::input_output_error` and the other storage failures: a failing storage stack must not be
  silently downgraded from "the atomic save failed" to "the old contents have been destroyed".

Not covered, deliberately: `QSaveFile` *also* falls back when the destination exists and is not a
regular file (a device, a FIFO, an alternate data stream on Windows), because it cannot rename onto
it. `atomic_file` does not, because that is not a failure to create a temporary file at all, and
quietly reinterpreting "replace this file" as "write into this device" would change the meaning of
the transaction behind the caller's back. Such a transaction still fails safely at `commit()` with
the destination untouched.

Two invariants follow, and they are the reason the fallback is opt-in:

1. **The fallback never happens unless `options::fallback` was set.** No heuristic decides on the
   caller's behalf that a weaker guarantee is acceptable.
2. **The fallback can never be entered from an ordinary `write()` or `commit()` failure.** It is
   decided once, in `open()`, before a single byte of content has been written, and is consulted
   nowhere else. A write or commit failure always remains a hard failure of the transaction. The
   only way content can reach a non-atomically written destination is if `open()` itself had already
   selected the fallback, because the temporary file could not be created for `EACCES` alone.
*/
class LLFIO_DECL atomic_file
{
public:
  //! Path type of the underlying handles.
  using path_type = file_handle::path_type;
  //! The type of an offset within the file.
  using extent_type = file_handle::extent_type;
  //! The type of a size within the file.
  using size_type = file_handle::size_type;
  //! The mode with which a file is opened.
  using mode = file_handle::mode;
  //! The creation mode with which a file is opened.
  using creation = file_handle::creation;
  //! The caching with which a file is opened.
  using caching = file_handle::caching;
  //! The flags with which a file is opened.
  using flag = file_handle::flag;
  //! The path view type used by this handle.
  using path_view_type = file_handle::path_view_type;
  //! The scatter buffer type used by this handle.
  using const_buffer_type = byte_io_handle::const_buffer_type;
  //! The gather buffers type used by this handle.
  using const_buffers_type = byte_io_handle::const_buffers_type;
  //! The kinds of write reordering barrier which can be performed.
  using barrier_kind = byte_io_handle::barrier_kind;
  //! The error type returned by this class.
  using error_type = result<void>::error_type;

  /*! Options for opening an `atomic_file`.
   */
  struct options
  {
    /*! If true, and if a temporary file cannot be created in the destination's directory
    *specifically* because that directory refuses to let this process create a new name in it
    (`errc::permission_denied`, i.e. `EACCES`), then fall back to opening and writing the
    destination file directly, in place.

    This is **not atomic**: a concurrent reader may observe a partially written destination, and if
    the `atomic_file` is destroyed without `commit()` the partially written destination remains.
    It exists to mirror the behaviour of similar abstractions elsewhere, and defaults to `false`
    precisely because it silently gives up the guarantees which make this class worth using.

    Only that one class of failure diverts into the fallback. Any other reason the temporary file
    could not be created is reported to the caller unchanged, because falling back would either
    fail anyway or destroy the destination on the way to failing. See
    \ref atomic_file_fallback for the exact list and the reasoning.

    Note carefully that this option means "falling back is **permitted**", never "this transaction
    **is** in fallback mode". Setting it does not by itself change anything: an opted-in transaction
    which *can* create its temporary file is a completely ordinary atomic one - writes go to the
    temporary file, `commit()` relinks it, `is_fallback()` returns false. Whether the fallback was
    actually taken is transaction state, reported by `is_fallback()`.
    */
    bool fallback;
    /*! The caching to use for the file. Defaults to `caching::all`, i.e. normal cached writes
    with the OS flushing at its leisure. Durability is established by a single barrier at commit
    time, so a stronger per-write caching mode is not needed and only costs performance.
    */
    caching _caching;

    //! Default constructor.
    constexpr options() noexcept
        : fallback(false)
        , _caching(caching::all)
    {
    }
    //! Construct from an explicit fallback and caching.
    constexpr options(bool _fallback, caching __caching = caching::all) noexcept
        : fallback(_fallback)
        , _caching(__caching)
    {
    }
  };

  //! The state of the transaction.
  enum class state : uint8_t
  {
    open,       //!< The transaction is accepting writes. The destination has not been touched.
    committed,  //!< The destination has been published (or, in fallback mode, finalised).
    cancelled,  //!< `cancel()` was called. The destination will not be published.
    failed      //!< A failure occurred. The destination will not be published.
  };

  //! Default constructor. Produces an object which owns nothing and is in the `open` state.
  atomic_file() = default;

  ~atomic_file()
  {
    // Only `_tempfile` may ever be deleted here, and `_tempfile` is *only* ever assigned a file this
    // object created itself with `uniquely_named_file()`. In fallback mode the real destination is
    // held in `_destination` instead, which nothing here ever unlinks. The two handles therefore
    // cannot be confused, so no failure path - including one part way through fallback setup - can
    // cause this destructor to delete the caller's destination.
    if(_tempfile.is_valid() && !_destination_modified)
    {
      (void) _tempfile.unlink();
    }
  }

  //! Move construction. The moved-from object is left owning nothing.
  atomic_file(atomic_file &&o) noexcept = default;
  //! Move assignment. Any temporary file currently held is discarded first.
  atomic_file &operator=(atomic_file &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    if(_tempfile.is_valid() && !_destination_modified)
    {
      (void) _tempfile.unlink();
    }
    _tempfile = std::move(o._tempfile);
    _destination = std::move(o._destination);
    _dirh = std::move(o._dirh);
    _leafname = std::move(o._leafname);
    _offset = o._offset;
    _write_error = std::move(o._write_error);
    _write_failed = o._write_failed;
    _destination_modified = o._destination_modified;
    _fallback = o._fallback;
    _state = o._state;
    // The moved-from object's `_tempfile` is now empty, so its destructor cannot discard ours; but
    // mark it neutral anyway so it also will not report the destination as publishable.
    o._state = state::committed;
    o._fallback = true;
    o._destination_modified = true;
    return *this;
  }
  //! No copy construction.
  atomic_file(const atomic_file &) = delete;
  //! No copy assignment.
  atomic_file &operator=(const atomic_file &) = delete;

  /*! Create an `atomic_file` transaction which will eventually publish to `path`.

  \param base The base handle to interpret `path` relative to. May be a default constructed
  (invalid) handle, in which case `path` is interpreted relative to the current working directory,
  or an absolute path.
  \param path The destination path. It may be a leafname (in which case it is interpreted relative
  to `base`), or it may contain directories (in which case those directories are opened relative to
  `base`). The final component is the destination file.
  \param opts The options to use. See `options`.
  \returns A transaction. Nothing has been written to the destination yet.

  \errors Any of the errors which `directory_handle::directory()`, `file_handle::uniquely_named_file()`
  or `file_handle::file()` can return. If the temporary file cannot be created and
  `options::fallback` is set, only `errc::permission_denied` is diverted into the non-atomic
  fallback; every other error is returned unchanged. See \ref atomic_file_fallback.

  \mallocs The default synchronous `handle::barrier()` implementation uses thread local buffers, and
  may allocate.
  */
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<atomic_file> open(const path_handle &base, path_view_type path,
                                                                  options opts = {}) noexcept;

  /*! Write some bytes to the end of the transaction's file. Multiple sequential writes are
  supported; the write offset is tracked internally and starts at zero.

  \return The number of bytes written.
  \param buffers The gather buffers to write.
  \param d The deadline by which the write must complete.

  \errors Returns `errc::operation_not_permitted` if the transaction is no longer accepting writes
  (i.e. it has been committed or cancelled). If an earlier write failed, that same failure is
  returned immediately, and no further bytes are written.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<size_type> write(const_buffers_type buffers, deadline d = deadline()) noexcept;

  //! \overload
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<size_type> write(const_buffer_type buffer, deadline d = deadline()) noexcept;

  /*! Publish the transaction's contents to the destination path, atomically and durably.

  On success, the destination path now names the new content, that content is on stable storage,
  and the directory entry naming it has been flushed. If the destination existed, the new file has
  its permission bits; see the permissions caveat in the class documentation.

  If a write previously failed, this discards the temporary file and returns that earlier failure.
  If the transaction was cancelled, this returns `errc::operation_canceled` and does not publish.

  \param d The deadline by which to complete the replacement. Defaults to no deadline (wait
  indefinitely), matching the rest of LLFIO; pass an explicit deadline to bound it. The durability
  barrier of the temporary file and the directory flush are synchronous and ignore this deadline.
  \returns Success, or the failure of the step which failed. See the class documentation for what
  has happened to the destination when this returns failure.

  \errors `errc::operation_not_permitted` if already committed; `errc::operation_canceled` if
  cancelled; otherwise whatever the barrier, `relink()` or directory flush returned.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> commit(deadline d = deadline()) noexcept;

  /*! Cancel the transaction, so that `commit()` will not publish the destination.

  After this returns success, the destination path has not been modified and cannot be published by
  this object.

  \returns Success in temporary-file mode. In fallback mode the destination is being written
  directly and the already-performed writes cannot be undone, so this returns `errc::not_supported`
  rather than pretending that cancellation succeeded. In that case the state is left unchanged.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> cancel() noexcept;

  //! The current state of the transaction.
  state current_state() const noexcept { return _state; }
  //! Whether this transaction is writing the destination directly (see `options::fallback`).
  /*! This is the transaction *state*, and is deliberately not the same as `options::fallback`, which
  only says that falling back would have been permitted. It is false for every transaction which
  managed to create its temporary file, including every transaction opened with `options(true)` in a
  directory which allows new names; only a transaction which actually took the fallback reports true.
  */
  bool is_fallback() const noexcept { return _fallback; }
  /*! Whether this transaction has modified the destination path.

  This is true once the atomic replacement has happened, and also, in the non-atomic fallback mode,
  from the moment the destination was opened for direct writing (fallback truncates the destination
  immediately). It is deliberately *not* the same as `current_state() == state::committed`: if
  `commit()` returns failure *after* the replacement has already happened — for example the final
  directory flush fails, or a caller-supplied deadline expired once the rename had been issued —
  the destination has still been modified, and this returns true so the caller can tell that case
  apart from a failure which did not touch the destination. See the class documentation.
  */
  bool replacement_performed() const noexcept { return _destination_modified; }

private:
  /*! The disposable temporary file which this transaction creates and owns. It is *only* ever
  assigned a file this object created with `uniquely_named_file()`; it is never the caller's
  destination, which lives in `_destination` instead. This is what makes it impossible for the
  destructor to delete the destination: the handle it is allowed to unlink cannot be one.
  */
  file_handle _tempfile;
  /*! In non-atomic fallback mode, the caller's real destination, opened for direct writing. This
  object never owns it and must never delete it, so nothing here ever unlinks it.
  */
  file_handle _destination;
  directory_handle _dirh;             //!< The directory containing the destination.
  filesystem::path _leafname;         //!< The leafname of the destination within `_dirh`.
  extent_type _offset{0};             //!< The offset at which the next write will occur.
  error_type _write_error{};          //!< The first write failure, if any.
  bool _write_failed{false};          //!< Whether `_write_error` is meaningful.
  bool _destination_modified{false};  //!< Whether the destination path has been modified by this transaction.
  bool _fallback{false};              //!< Whether we are writing the destination directly.
  state _state{state::open};          //!< The state of the transaction.

  //! The handle the transaction is currently writing to. In fallback mode this is the caller's real
  //! destination, which this object does not own; never use it for anything destructive.
  file_handle &_write_target() noexcept { return _fallback ? _destination : _tempfile; }

  /*! Applies the existing destination's permission bits to `_tempfile`, so that the replacement
  keeps the mode of the file it replaces. Best effort: it is a no-op when the destination does not
  exist, is not a regular file, or its permissions cannot be read or applied, and it never reports a
  failure. See the permissions caveat in the class documentation.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC void _preserve_destination_permissions(deadline d) noexcept;
};

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "detail/impl/atomic_file.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#endif