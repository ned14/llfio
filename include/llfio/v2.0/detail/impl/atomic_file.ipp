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

#include "../../atomic_file.hpp"

LLFIO_V2_NAMESPACE_BEGIN

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<atomic_file> atomic_file::open(const path_handle &base, path_view_type path,
                                                                      options opts) noexcept
{
  LLFIO_EXCEPTION_TRY
  {
    if(path.empty())
    {
      return errc::invalid_argument;
    }
    path_view_type parent = path.parent_path();
    path_view_type leaf = path.filename();
    if(leaf.empty())
    {
      return errc::invalid_argument;
    }
    atomic_file ret;
    /* `opts.fallback` says "falling back is *permitted*", not "this transaction *is* falling back".
    `_fallback` records which of the two it actually turned out to be, so it must stay at its `false`
    default until the temporary file really could not be created and the destination's own inode has
    been opened directly. Copying the caller's preference here would make an ordinary, successful
    transaction claim to be in fallback mode: `is_fallback()` would lie, `cancel()` would refuse as
    unimplementable, the permission preservation would be skipped, and - worst of all -
    `_write_target()` would aim at the unopened destination instead of the temporary file which is
    the entire point of this class. See \ref atomic_file_fallback.
    */
    // We own only the leafname, not the whole target path. The containing directory is held as an
    // open handle, which both anchors the destination against third party path changes and lets us
    // flush the directory entry at commit time.
    ret._leafname = leaf.path();
    {
      auto dirh = parent.empty() ? directory_handle::directory(base, ".", mode::read) :
                                   directory_handle::directory(base, parent, mode::read);
      if(!dirh)
      {
        return std::move(dirh).error();
      }
      ret._dirh = std::move(dirh).value();
    }
    // Prefer a uniquely named temporary file in the same directory as the destination, which is
    // what makes the eventual replacement atomic and crash-safe.
    auto temp = file_handle::uniquely_named_file(ret._dirh, mode::write, opts._caching);
    if(temp)
    {
      // A file we created ourselves, and the only thing the destructor is ever allowed to unlink.
      ret._tempfile = std::move(temp).value();
      return ret;
    }
    auto failed = std::move(temp).error();
    /* Only one class of failure to create the temporary file means "this directory will not let us
    add a new name, but the destination's own inode may still be writable": `EACCES`. Falling back
    for anything else would be misleading at best, and at worst destructive - the fallback truncates
    the destination as it starts, so a resource-exhaustion failure would destroy the old contents on
    its way to failing with the very same error. Everything else is therefore reported unchanged, as
    it would be with the fallback switched off. This is the same line `QSaveFile` draws: its POSIX
    fallback is guarded by `errno == EACCES`. See the fallback documentation in the class comment.
    */
    if(!opts.fallback || failed != errc::permission_denied)
    {
      return failed;
    }
    // The caller has opted in to the non-atomic fallback *and* the temporary file could not be
    // created for the one error class which means this is worth trying, so this is where the
    // transaction actually enters fallback mode - the only place `_fallback` ever becomes true.
    // Note that the real destination is stored in `_destination`, never in `_tempfile`, so if any of
    // the steps below fail and this partially constructed object is destroyed, the destructor has no
    // handle to the destination and cannot delete it.
    ret._fallback = true;
    auto direct = file_handle::file(ret._dirh, ret._leafname, mode::write, creation::if_needed, opts._caching);
    if(!direct)
    {
      return std::move(direct).error();
    }
    ret._destination = std::move(direct).value();
    // Represent the new content faithfully by starting from an empty destination. This truncates
    // the destination there and then, so from this point the destination has been modified and
    // cannot be restored.
    auto trunc = ret._destination.truncate(0);
    if(!trunc)
    {
      return std::move(trunc).error();
    }
    ret._destination_modified = true;
    return ret;
  }
  LLFIO_EXCEPTION_CATCH_ALL
  {
    return error_from_exception();
  }
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<atomic_file::size_type> atomic_file::write(const_buffers_type buffers,
                                                                                  deadline d) noexcept
{
  LLFIO_EXCEPTION_TRY
  {
    if(_state != state::open)
    {
      return errc::operation_not_permitted;
    }
    if(_write_failed)
    {
      // Remember the first write failure and never write anything further.
      return std::move(_write_error);
    }
    if(buffers.empty())
    {
      return size_type(0);
    }
    byte_io_handle::io_request<const_buffers_type> req(buffers, _offset);
    auto ret = _write_target().write(req, d);
    if(!ret)
    {
      _write_failed = true;
      _write_error = std::move(ret).error();
      return std::move(_write_error);
    }
    auto written = ret.bytes_transferred();
    _offset += written;
    return written;
  }
  LLFIO_EXCEPTION_CATCH_ALL
  {
    return error_from_exception();
  }
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<atomic_file::size_type> atomic_file::write(const_buffer_type buffer,
                                                                                  deadline d) noexcept
{
  const_buffer_type buf[1] = {buffer};
  return write(const_buffers_type(buf, 1), d);
}

/*! Applies the existing destination's permission bits to the temporary file, so that the replacement
keeps the mode of the file it replaces.

This is best effort and never fails the commit: a filing system which cannot report or apply
permissions (and an absent destination) must not stop a caller saving their data.
*/
void atomic_file::_preserve_destination_permissions(deadline d) noexcept
{
  /* The rename in `commit()` discards the destination's inode, and with it the destination's
  permission bits, leaving whichever ones the temporary file happened to be created with. Read the
  old mode first and stamp it onto the file which is about to take its place. This is what
  `QSaveFile` does, and it is what stops an atomic save from silently changing the mode of the file
  being replaced - dropping the executable bit off a script, or widening a 0600 file to the umask
  default.

  The mode is read *without opening the destination*: asking `_dirh` to fill a single buffer entry
  for a leafname which contains no glob is a single `fstatat()` of that entry relative to the
  directory handle. So it needs no permission on the destination itself - a file the caller cannot
  even read still has a mode worth preserving - and it cannot block, as opening the destination
  would if the destination were a FIFO with no writer. Being relative to `_dirh` also means no
  component of the destination path is re-resolved, so a third party cannot substitute a different
  entry for the one whose mode we read. `stat_t::stamp()` then applies the mode through the
  temporary file's handle. Neither half goes anywhere near a path, and neither can fail the commit.
  */
  directory_entry entry{};
  directory_handle::buffers_type buffers(span<directory_entry>(&entry, 1));
  auto filled = _dirh.read(directory_handle::io_request<directory_handle::buffers_type>(std::move(buffers), _leafname), d);
  if(!filled)
  {
    // Most commonly `errc::no_such_file_or_directory`: there is no existing destination whose
    // permissions could be preserved, so the temporary file keeps the mode it was created with.
    return;
  }
  if((filled.value().metadata() & stat_t::want::perms) == 0)
  {
    // `read()` reports which metadata it filled in, and on Windows (where permission bits do not
    // exist at all) `perms` is never among it. Stamping here would apply whatever happened to be in
    // `st_perms`, so leave the temporary file's own mode alone instead.
    return;
  }
  if(filled.value().empty() || entry.stat.st_type != filesystem::file_type::regular)
  {
    /* Only a regular file has a mode which it makes sense to carry across to its replacement. A
    symbolic link's mode is 0777 and says nothing about the file which replaces it, a FIFO's or
    device's is not the mode of the new file either, and a directory cannot be replaced at all.
    */
    return;
  }
  // Applying the mode can fail on a filing system which does not support permissions at all. The
  // caller asked for their contents to be saved, not for this, so let the commit proceed without.
  (void) entry.stat.stamp(_tempfile, stat_t::want::perms);
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> atomic_file::commit(deadline d) noexcept
{
  LLFIO_EXCEPTION_TRY
  {
    if(_state == state::committed)
    {
      return errc::operation_not_permitted;
    }
    if(_state == state::cancelled || _state == state::failed)
    {
      return errc::operation_canceled;
    }
    if(_write_failed)
    {
      _state = state::failed;
      return std::move(_write_error);
    }
    if(!_fallback)
    {
      // Fallback mode writes the destination's own inode in place, so there is nothing to preserve
      // there. Do this before the barrier below, so that the mode change it makes is covered by
      // that barrier and is therefore durable by the time the rename is published.
      _preserve_destination_permissions(d);
    }
    // Exactly one durability barrier, at commit time, for all the data written so far plus the
    // file metadata needed to retrieve it. Note that a synchronous barrier cannot honour a
    // deadline (LLFIO returns `errc::not_supported` for that), so we pass an empty deadline here.
    // This is a barrier, not a destructive operation, so it is safe on the real destination.
    auto bar = _write_target().barrier(barrier_kind::wait_all, deadline());
    if(!bar)
    {
      _state = state::failed;
      return std::move(bar).error();
    }
    if(_fallback)
    {
      // We have been writing the destination directly all along; there is nothing to rename.
      _state = state::committed;
      return success();
    }
    // Atomically and silently replace whatever is at the destination. Fallback mode returned above,
    // so `_tempfile` is definitely the temporary file here.
    auto rel = _tempfile.relink(_dirh, _leafname, true, d);
    if(!rel)
    {
      _state = state::failed;
      return std::move(rel).error();
    }
    // The relocation succeeded, so the destination now names the new content. Record that *before*
    // anything else can fail, so that a caller which sees a failure from the directory flush (or a
    // deadline which expired once the rename had been issued) can still tell that the destination
    // was modified. See replacement_performed().
    _destination_modified = true;
    _state = state::committed;
    auto dirbar = _dirh.barrier();
    if(!dirbar)
    {
      return std::move(dirbar).error();
    }
    return success();
  }
  LLFIO_EXCEPTION_CATCH_ALL
  {
    return error_from_exception();
  }
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> atomic_file::cancel() noexcept
{
  if(_state == state::cancelled)
  {
    return success();
  }
  if(_state == state::committed || _state == state::failed)
  {
    return errc::operation_not_permitted;
  }
  if(_fallback)
  {
    // We have been writing the destination directly. Those writes cannot be undone, so we refuse
    // to pretend that cancellation succeeded, and leave the state unchanged.
    return errc::not_supported;
  }
  _state = state::cancelled;
  return success();
}

LLFIO_V2_NAMESPACE_END