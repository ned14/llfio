/* A filing system handle
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

#include "../../../fs_handle.hpp"
#include "../../../stat.hpp"
#include "../../../utils.hpp"
#include "import.hpp"

AFIO_V2_NAMESPACE_BEGIN

result<void> fs_handle::_fetch_inode() noexcept
{
  stat_t s;
  OUTCOME_TRYV(s.fill(_get_handle(), stat_t::want::dev | stat_t::want::ino));
  _devid = s.st_dev;
  _inode = s.st_ino;
  return success();
}

result<void> fs_handle::relink(const path_handle &base, path_view_type path, deadline /*unused*/) noexcept
{
  using flag = handle::flag;
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  auto &h = _get_handle();

  path_view::c_str zpath(path);
  UNICODE_STRING _path;
  _path.Buffer = const_cast<wchar_t *>(zpath.buffer);
  _path.MaximumLength = (_path.Length = (USHORT)(zpath.length * sizeof(wchar_t))) + sizeof(wchar_t);
  if(zpath.length >= 4 && _path.Buffer[0] == '\\' && _path.Buffer[1] == '\\' && _path.Buffer[2] == '.' && _path.Buffer[3] == '\\')
  {
    _path.Buffer += 3;
    _path.Length -= 3 * sizeof(wchar_t);
    _path.MaximumLength -= 3 * sizeof(wchar_t);
  }

  IO_STATUS_BLOCK isb = make_iostatus();
  alignas(8) char buffer[sizeof(FILE_RENAME_INFORMATION) + 65536];
  FILE_RENAME_INFORMATION *fni = (FILE_RENAME_INFORMATION *) buffer;
  fni->ReplaceIfExists = true;
  fni->RootDirectory = base.is_valid() ? base.native_handle().h : nullptr;
  fni->FileNameLength = _path.Length;
  memcpy(fni->FileName, _path.Buffer, fni->FileNameLength);
  NtSetInformationFile(h.native_handle().h, &isb, fni, sizeof(FILE_RENAME_INFORMATION) + fni->FileNameLength, FileRenameInformation);
  if(h.flags() & flag::overlapped)
    ntwait(h.native_handle().h, isb, deadline());
  if(STATUS_SUCCESS != isb.Status)
    return {(int) isb.Status, ntkernel_category()};
  return success();
}

result<void> fs_handle::unlink(deadline /*unused*/) noexcept
{
  using flag = handle::flag;
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  auto &h = _get_handle();
  if((h.is_regular() || h.is_symlink()) && !(h.flags() & flag::win_disable_unlink_emulation))
  {
    // Rename it to something random to emulate immediate unlinking
    auto randomname = utils::random_string(32);
    randomname.append(".deleted");
    OUTCOME_TRY(cpath, h.current_path());
    cpath.remove_filename();
    OUTCOME_TRY(dirh, path_handle::path(cpath));
    OUTCOME_TRYV(relink(dirh, randomname));
  }
  // No point marking it for deletion if it's already been so
  if(!(h.flags() & flag::unlink_on_close))
  {
    // Hide the item in Explorer and the command line
    {
      IO_STATUS_BLOCK isb = make_iostatus();
      FILE_BASIC_INFORMATION fbi;
      memset(&fbi, 0, sizeof(fbi));
      fbi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
      NtSetInformationFile(h.native_handle().h, &isb, &fbi, sizeof(fbi), FileBasicInformation);
      if(h.flags() & flag::overlapped)
        ntwait(h.native_handle().h, isb, deadline());
    }
    // Mark the item as delete on close
    IO_STATUS_BLOCK isb = make_iostatus();
    FILE_DISPOSITION_INFORMATION fdi;
    memset(&fdi, 0, sizeof(fdi));
    fdi._DeleteFile = true;
    NtSetInformationFile(h.native_handle().h, &isb, &fdi, sizeof(fdi), FileDispositionInformation);
    if(h.flags() & flag::overlapped)
      ntwait(h.native_handle().h, isb, deadline());
    if(STATUS_SUCCESS != isb.Status)
      return {(int) isb.Status, ntkernel_category()};
  }
  return success();
}

AFIO_V2_NAMESPACE_END
