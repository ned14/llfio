/* Misc utilities
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (6 commits)
File Created: Jan 2015


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

#include "quickcpplib/spinlock.hpp"

#include <mutex>  // for lock_guard

#include <sys/mman.h>

#ifdef __linux__
#include <unistd.h>  // for preadv
#endif
#ifdef __APPLE__
#include <mach/task.h>
#include <mach/task_info.h>
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace utils
{
  size_t page_size() noexcept
  {
    static size_t ret;
    if(ret == 0u)
    {
      ret = getpagesize();
    }
    return ret;
  }
  const std::vector<size_t> &page_sizes(bool only_actually_available)
  {
    static QUICKCPPLIB_NAMESPACE::configurable_spinlock::spinlock<bool> lock;
    std::lock_guard<decltype(lock)> g(lock);
    static std::vector<size_t> pagesizes, pagesizes_available;
    if(pagesizes.empty())
    {
#if defined(__FreeBSD__)
      pagesizes.resize(32);
      int out;
      if(-1 == (out = getpagesizes(pagesizes.data(), 32)))
      {
        pagesizes.clear();
        pagesizes.push_back(getpagesize());
        pagesizes_available.push_back(getpagesize());
      }
      else
      {
        pagesizes.resize(out);
        pagesizes_available = pagesizes;
      }
#elif defined(__APPLE__)
      // I can't find how to determine what the super page size is on OS X programmatically
      // It appears to be hard coded into mach/vm_statistics.h which says that it's always 2Mb
      // Therefore, we hard code 2Mb
      pagesizes.push_back(getpagesize());
      pagesizes_available.push_back(getpagesize());
      pagesizes.push_back(2 * 1024 * 1024);
      pagesizes_available.push_back(2 * 1024 * 1024);
#elif defined(__linux__)
      pagesizes.push_back(getpagesize());
      pagesizes_available.push_back(getpagesize());
      int ih = ::open("/proc/meminfo", O_RDONLY | O_CLOEXEC);
      if(-1 != ih)
      {
        char buffer[4096], *hugepagesize, *hugepages;
        buffer[::read(ih, buffer, sizeof(buffer) - 1)] = 0;
        ::close(ih);
        hugepagesize = strstr(buffer, "Hugepagesize:");
        hugepages = strstr(buffer, "HugePages_Total:");
        if((hugepagesize != nullptr) && (hugepages != nullptr))
        {
          unsigned _hugepages = 0, _hugepagesize = 0;
          while(*++hugepagesize != ' ')
          {
            ;
          }
          while(*++hugepages != ' ')
          {
            ;
          }
          while(*++hugepagesize == ' ')
          {
            ;
          }
          while(*++hugepages == ' ')
          {
            ;
          }
          sscanf(hugepagesize, "%u", &_hugepagesize);  // NOLINT
          sscanf(hugepages, "%u", &_hugepages);        // NOLINT
          if(_hugepagesize != 0u)
          {
            pagesizes.push_back((static_cast<size_t>(_hugepagesize)) * 1024);
            if(_hugepages != 0u)
            {
              pagesizes_available.push_back((static_cast<size_t>(_hugepagesize)) * 1024);
            }
          }
        }
      }
#else
#warning page_sizes() does not know this platform, so assuming getpagesize() is the best available
      pagesizes.push_back(getpagesize());
      pagesizes_available.push_back(getpagesize());
#endif
    }
    return only_actually_available ? pagesizes_available : pagesizes;
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  void random_fill(char *buffer, size_t bytes) noexcept
  {
    static QUICKCPPLIB_NAMESPACE::configurable_spinlock::spinlock<bool> lock;
    static std::atomic<int> randomfd(-1);
    int fd = randomfd;
    if(-1 == fd)
    {
      std::lock_guard<decltype(lock)> g(lock);
      randomfd = fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    }
    if(-1 == fd || ::read(fd, buffer, bytes) < static_cast<ssize_t>(bytes))
    {
      LLFIO_LOG_FATAL(0, "llfio: Kernel crypto function failed");
      std::terminate();
    }
  }

  result<void> flush_modified_data() noexcept
  {
    ::sync();
    return success();
  }

  result<void> drop_filesystem_cache() noexcept
  {
    (void) flush_modified_data();
#ifdef __linux__
    int h = ::open("/proc/sys/vm/drop_caches", O_WRONLY | O_CLOEXEC);
    if(h == -1)
    {
      return posix_error();
    }
    auto unh = undoer([&h] { ::close(h); });
    char v = '3';  // drop everything
    if(-1 == ::write(h, &v, 1))
    {
      return posix_error();
    }
    return success();
#endif
    return errc::not_supported;
  }

  bool running_under_wsl() noexcept
  {
#ifdef __linux__
    static int cached;
    if(cached != 0)
    {
      return cached == 2;
    }
    int h = ::open("/proc/version", O_RDONLY | O_CLOEXEC);
    if(h == -1)
    {
      cached = 1;
      return false;
    }
    auto unh = undoer([&h] { ::close(h); });
    char buffer[257];
    ssize_t bytes = ::read(h, buffer, 256);
    if(bytes == -1)
    {
      cached = 1;
      return false;
    }
    buffer[bytes] = 0;
    if(strstr(buffer, "-Microsoft (Microsoft@Microsoft.com)") != nullptr)
    {
      cached = 2;
      return true;
    }
    cached = 1;
#endif
    return false;
  }

  result<process_memory_usage> current_process_memory_usage() noexcept
  {
#ifdef __linux__
    try
    {
      /* /proc/[pid]/status:

      total_address_space_in_use = VmSize
      total_address_space_paged_in = VmRSS
      private_committed = ???  MISSING
      private_paged_in = RssAnon

      /proc/[pid]/smaps:

      total_address_space_in_use = Sum of Size
      total_address_space_paged_in = Sum of Rss
      private_committed = Sum of Size for all entries with VmFlags containing ac, and inode = 0?
      private_paged_in = (Sum of Anonymous - Sum of LazyFree) for all entries with VmFlags containing ac, and inode = 0?
      */
      std::vector<char> buffer(65536);
      for(;;)
      {
        int ih = ::open("/proc/self/smaps", O_RDONLY);
        if(ih == -1)
        {
          return posix_error();
        }
        size_t totalbytesread = 0;
        for(;;)
        {
          auto bytesread = ::read(ih, buffer.data() + totalbytesread, buffer.size() - totalbytesread);
          if(bytesread < 0)
          {
            ::close(ih);
            return posix_error();
          }
          if(bytesread == 0)
          {
            break;
          }
          totalbytesread += bytesread;
        }
        ::close(ih);
        if(totalbytesread < buffer.size())
        {
          buffer.resize(totalbytesread);
          break;
        }
        buffer.resize(buffer.size() * 2);
      }
      const string_view totalview(buffer.data(), buffer.size());
      //std::cerr << totalview << std::endl;
      std::vector<string_view> anon_entries, non_anon_entries;
      anon_entries.reserve(32);
      non_anon_entries.reserve(32);
      auto sizeidx = totalview.find("\nSize:");
      while(sizeidx < totalview.size())
      {
        auto itemtopidx = totalview.rfind("\n", sizeidx - 1);
        if(string_view::npos == itemtopidx)
        {
          itemtopidx = 0;
        }
        // hexaddr-hexaddr flags offset dev:id inode [path]
        size_t begin, end, offset, inode = 1;
        char f1, f2, f3, f4, f5, f6, f7, f8;
        sscanf(totalview.data() + itemtopidx, "%zx-%zx %c%c%c%c %zx %c%c:%c%c %zu", &begin, &end, &f1, &f2, &f3, &f4, &offset, &f5, &f6, &f7, &f8, &inode);
        sizeidx = totalview.find("\nSize:", sizeidx + 1);
        if(string_view::npos == sizeidx)
        {
          sizeidx = totalview.size();
        }
        auto itemendidx = totalview.rfind("\n", sizeidx - 1);
        if(string_view::npos == itemendidx)
        {
          abort();
        }
        const string_view item(totalview.substr(itemtopidx + 1, itemendidx - itemtopidx - 1));
        auto vmflagsidx = item.rfind("\n");
        if(string_view::npos == itemendidx)
        {
          abort();
        }
        if(0 != memcmp(item.data() + vmflagsidx, "\nVmFlags:", 9))
        {
          return errc::illegal_byte_sequence;
        }
        // Is there " ac" after vmflagsidx?
        if(string_view::npos != item.find(" ac", vmflagsidx) && inode == 0)
        {
          //std::cerr << "Adding anon entry at offset " << itemtopidx << std::endl;
          anon_entries.push_back(item);
        }
        else
        {
          //std::cerr << "Adding non-anon entry at offset " << itemtopidx << std::endl;
          non_anon_entries.push_back(item);
        }
      }
      auto parse = [](string_view item, string_view what) ->result<uint64_t> { auto idx = item.find(what);
        if(string_view::npos == idx)
        {
          return (uint64_t) -1;
        }
        idx += what.size();
        for(; item[idx] == ' '; idx++)
          ;
        auto eidx = idx;
        for(; item[eidx] != '\n'; eidx++)
          ;
        string_view unit(item.substr(eidx - 2, 2));
        uint64_t value = atoll(item.data() + idx);
        if(unit == "kB")
        {
          value *= 1024ULL;
        }
        else if(unit == "mB")
        {
          value *= 1024ULL * 1024;
        }
        else if(unit == "gB")
        {
          value *= 1024ULL * 1024 * 1024;
        }
        else if(unit == "tB")
        {
          value *= 1024ULL * 1024 * 1024 * 1024;
        }
        else if(unit == "pB")
        {
          value *= 1024ULL * 1024 * 1024 * 1024 * 1024;
        }
        else if(unit == "eB")
        {
          value *= 1024ULL * 1024 * 1024 * 1024 * 1024 * 1024;
        }
        else if(unit == "zB")
        {
          value *= 1024ULL * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
        }
        else if(unit == "yB")
        {
          value *= 1024ULL * 1024 * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
        }
        else
        {
          return errc::illegal_byte_sequence;
        }
        return value;
      };
      process_memory_usage ret;
      //std::cerr << "Anon entries:";
      for(auto &i : anon_entries)
      {
        OUTCOME_TRY(size, parse(i, "\nSize:"));
        OUTCOME_TRY(rss, parse(i, "\nRss:"));
        OUTCOME_TRY(anonymous, parse(i, "\nAnonymous:"));
        OUTCOME_TRY(lazyfree, parse(i, "\nLazyFree:"));
        if(size != (uint64_t) -1 && rss != (uint64_t) -1 && anonymous != (uint64_t) -1)
        {
          ret.total_address_space_in_use += size;
          ret.total_address_space_paged_in += rss;
          ret.private_committed += size;
          ret.private_paged_in += anonymous;
          if(lazyfree != (uint64_t) -1)
          {
            ret.total_address_space_paged_in -= lazyfree;
            ret.private_paged_in -= lazyfree;
          }
        }
        //std::cerr << i << "\nSize = " << size << " Rss = " << rss << std::endl;
      }
      //std::cerr << "\n\nNon-anon entries:";
      for(auto &i : non_anon_entries)
      {
        OUTCOME_TRY(size, parse(i, "\nSize:"));
        OUTCOME_TRY(rss, parse(i, "\nRss:"));
        OUTCOME_TRY(lazyfree, parse(i, "\nLazyFree:"));
        if(size != (uint64_t) -1 && rss != (uint64_t) -1)
        {
          ret.total_address_space_in_use += size;
          ret.total_address_space_paged_in += rss;
          if(lazyfree != (uint64_t) -1)
          {
            ret.total_address_space_paged_in -= lazyfree;
          }
        }
        //std::cerr << i << "\nSize = " << size << " Rss = " << rss << std::endl;
      }
      return ret;
    }
    catch(...)
    {
      return error_from_exception();
    }
#elif defined(__APPLE__)
  kern_return_t error;
  mach_msg_type_number_t outCount;
  task_vm_info_data_t vmInfo;

  outCount = TASK_VM_INFO_COUNT;
  error = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vmInfo, &outCount);
  if (error != KERN_SUCCESS) {
    return errc::invalid_argument;
  }
  process_memory_usage ret;
  ret.total_address_space_in_use = vmInfo.virtual_size;
  ret.total_address_space_paged_in = vmInfo.resident_size;
  ret.private_committed = vmInfo.internal + vmInfo.compressed;
  ret.private_paged_in = vmInfo.phys_footprint;
  return ret;
#else
#error Unknown platform
#endif
  }

  namespace detail
  {
    large_page_allocation allocate_large_pages(size_t bytes)
    {
      large_page_allocation ret(calculate_large_page_allocation(bytes));
      int flags = MAP_SHARED | MAP_ANON;
      if(ret.page_size_used > 65536)
      {
#ifdef MAP_HUGETLB
        flags |= MAP_HUGETLB;
#endif
#ifdef MAP_ALIGNED_SUPER
        flags |= MAP_ALIGNED_SUPER;
#endif
#ifdef VM_FLAGS_SUPERPAGE_SIZE_ANY
        flags |= VM_FLAGS_SUPERPAGE_SIZE_ANY;
#endif
      }
      if((ret.p = mmap(nullptr, ret.actual_size, PROT_WRITE, flags, -1, 0)) == nullptr)
      {
        if(ENOMEM == errno)
        {
          if((ret.p = mmap(nullptr, ret.actual_size, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0)) != nullptr)
          {
            return ret;
          }
        }
      }
#ifndef NDEBUG
      else if(ret.page_size_used > 65536)
      {
        printf("llfio: Large page allocation successful\n");
      }
#endif
      return ret;
    }
    void deallocate_large_pages(void *p, size_t bytes)
    {
      if(munmap(p, bytes) < 0)
      {
        LLFIO_LOG_FATAL(p, "llfio: Freeing large pages failed");
        std::terminate();
      }
    }
  }  // namespace detail
}  // namespace utils

LLFIO_V2_NAMESPACE_END
