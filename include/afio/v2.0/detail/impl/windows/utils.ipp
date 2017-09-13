/* Misc utilities
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (7 commits)
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

#include "../../../utils.hpp"

#include "../../../quickcpplib/include/spinlock.hpp"
#include "import.hpp"

AFIO_V2_NAMESPACE_BEGIN

namespace utils
{
  // Stupid MSVC ...
  namespace detail
  {
    using namespace AFIO_V2_NAMESPACE::detail;
  }
  size_t page_size() noexcept
  {
    static size_t ret;
    if(!ret)
    {
      SYSTEM_INFO si;
      memset(&si, 0, sizeof(si));
      GetSystemInfo(&si);
      ret = si.dwPageSize;
    }
    return ret;
  }
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6387)  // MSVC sanitiser warns that GetModuleHandleA() might fail (hah!)
#endif
  std::vector<size_t> page_sizes(bool only_actually_available)
  {
    static QUICKCPPLIB_NAMESPACE::configurable_spinlock::spinlock<bool> lock;
    static std::vector<size_t> pagesizes, pagesizes_available;
    std::lock_guard<decltype(lock)> g(lock);
    if(pagesizes.empty())
    {
      typedef size_t(WINAPI * GetLargePageMinimum_t)(void);
      SYSTEM_INFO si;
      memset(&si, 0, sizeof(si));
      GetSystemInfo(&si);
      pagesizes.push_back(si.dwPageSize);
      pagesizes_available.push_back(si.dwPageSize);
      GetLargePageMinimum_t GetLargePageMinimum_ = (GetLargePageMinimum_t) GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetLargePageMinimum");
      if(GetLargePageMinimum_)
      {
        windows_nt_kernel::init();
        using namespace windows_nt_kernel;
        pagesizes.push_back(GetLargePageMinimum_());
        /* Attempt to enable SeLockMemoryPrivilege */
        HANDLE token;
        if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
        {
          auto untoken = undoer([&token] { CloseHandle(token); });
          TOKEN_PRIVILEGES privs;
          memset(&privs, 0, sizeof(privs));
          privs.PrivilegeCount = 1;
          if(LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &privs.Privileges[0].Luid))
          {
            privs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if(AdjustTokenPrivileges(token, FALSE, &privs, 0, NULL, NULL) && GetLastError() == S_OK)
              pagesizes_available.push_back(GetLargePageMinimum_());
          }
        }
      }
    }
    return only_actually_available ? pagesizes_available : pagesizes;
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  void random_fill(char *buffer, size_t bytes) noexcept
  {
    windows_nt_kernel::init();
    using namespace windows_nt_kernel;
    if(!RtlGenRandom(buffer, (ULONG) bytes))
    {
      AFIO_LOG_FATAL(0, "afio: Kernel crypto function failed");
      std::terminate();
    }
  }

  result<void> flush_modified_data() noexcept
  {
    windows_nt_kernel::init();
    using namespace windows_nt_kernel;
    static bool prived;
    if(!prived)
    {
      HANDLE processToken;
      if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &processToken) == FALSE)
      {
        return {GetLastError(), std::system_category()};
      }
      auto unprocessToken = undoer([&processToken] { CloseHandle(processToken); });
      {
        LUID luid;
        if(!LookupPrivilegeValue(NULL, L"SeProfileSingleProcessPrivilege", &luid))
        {
          return {GetLastError(), std::system_category()};
        }
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if(!AdjustTokenPrivileges(processToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES) NULL, (PDWORD) NULL))
        {
          return {GetLastError(), std::system_category()};
        }
        if(GetLastError() == ERROR_NOT_ALL_ASSIGNED)
        {
          return {GetLastError(), std::system_category()};
        }
      }
      prived = true;
    }
    typedef enum _SYSTEM_MEMORY_LIST_COMMAND { MemoryCaptureAccessedBits, MemoryCaptureAndResetAccessedBits, MemoryEmptyWorkingSets, MemoryFlushModifiedList, MemoryPurgeStandbyList, MemoryPurgeLowPriorityStandbyList, MemoryCommandMax } SYSTEM_MEMORY_LIST_COMMAND;

    // Write all modified pages to storage
    SYSTEM_MEMORY_LIST_COMMAND command = MemoryPurgeStandbyList;
    NTSTATUS ntstat = NtSetSystemInformation(80 /*SystemMemoryListInformation*/, &command, sizeof(SYSTEM_MEMORY_LIST_COMMAND));
    if(ntstat != STATUS_SUCCESS)
    {
      return {(int) ntstat, ntkernel_category()};
    }
    return success();
  }

  result<void> drop_filesystem_cache() noexcept
  {
    windows_nt_kernel::init();
    using namespace windows_nt_kernel;
    static bool prived;
    if(!prived)
    {
      HANDLE processToken;
      if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &processToken) == FALSE)
      {
        return {GetLastError(), std::system_category()};
      }
      auto unprocessToken = undoer([&processToken] { CloseHandle(processToken); });
      {
        LUID luid;
        if(!LookupPrivilegeValue(NULL, L"SeIncreaseQuotaPrivilege", &luid))
        {
          return {GetLastError(), std::system_category()};
        }
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if(!AdjustTokenPrivileges(processToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES) NULL, (PDWORD) NULL))
        {
          return {GetLastError(), std::system_category()};
        }
        if(GetLastError() == ERROR_NOT_ALL_ASSIGNED)
        {
          return {GetLastError(), std::system_category()};
        }
      }
      prived = true;
    }
    // Flush modified data so dropping the cache drops everything
    OUTCOME_TRYV(flush_modified_data());
    // Drop filesystem cache
    if(!SetSystemFileCacheSize((SIZE_T) -1, (SIZE_T) -1, 0))
    {
      return {GetLastError(), std::system_category()};
    }
    return success();
  }

  namespace detail
  {
    large_page_allocation allocate_large_pages(size_t bytes)
    {
      large_page_allocation ret(calculate_large_page_allocation(bytes));
      DWORD type = MEM_COMMIT | MEM_RESERVE;
      if(ret.page_size_used > 65536)
        type |= MEM_LARGE_PAGES;
      ret.p = VirtualAlloc(nullptr, ret.actual_size, type, PAGE_READWRITE);
      if(!ret.p)
      {
        if(ERROR_NOT_ENOUGH_MEMORY == GetLastError())
          ret.p = VirtualAlloc(nullptr, ret.actual_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      }
#ifndef NDEBUG
      else if(ret.page_size_used > 65536)
        printf("afio: Large page allocation successful\n");
#endif
      return ret;
    }
    void deallocate_large_pages(void *p, size_t bytes)
    {
      (void) bytes;
      if(!VirtualFree(p, 0, MEM_RELEASE))
      {
        AFIO_LOG_FATAL(p, "afio: Freeing large pages failed");
        std::terminate();
      }
    }
  }
}

AFIO_V2_NAMESPACE_END
