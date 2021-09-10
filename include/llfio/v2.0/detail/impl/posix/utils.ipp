/* Misc utilities
(C) 2016-2021 Niall Douglas <http://www.nedproductions.biz/> (6 commits)
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

#include <mutex>  // for lock_guard

#include <sys/mman.h>

// #include <iostream>

#ifdef __linux__
#include <unistd.h>  // for preadv
#endif
#ifdef __APPLE__
#include <mach/mach_host.h>
#include <mach/mach_time.h>
#include <mach/processor_info.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include <mach/vm_map.h>
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
    static spinlock lock;
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
    static spinlock lock;
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
    auto unh = make_scope_exit([&h]() noexcept { ::close(h); });
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
    auto unh = make_scope_exit([&h]() noexcept { ::close(h); });
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

  result<process_memory_usage> current_process_memory_usage(process_memory_usage::want want) noexcept
  {
#ifdef __linux__
    try
    {
      auto fill_buffer = [](std::vector<char> &buffer, const char *path) -> result<void> {
        for(;;)
        {
          int ih = ::open(path, O_RDONLY);
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
        return success();
      };
      auto parse = [](string_view item, string_view what) -> result<uint64_t> {
        auto idx = item.find(what);
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
      /* /proc/[pid]/status:

      total_address_space_in_use = VmSize
      total_address_space_paged_in = VmRSS
      private_committed = ???  MISSING
      private_paged_in = RssAnon

      /proc/[pid]/smaps:

      total_address_space_in_use = Sum of Size
      total_address_space_paged_in = Sum of Rss
      private_committed = (Sum of Anonymous - Sum of LazyFree) for all entries with VmFlags containing ac, and inode = 0?
      private_paged_in = Sum of Rss for all entries with VmFlags containing ac, and inode = 0?

      /proc/[pid]/maps:

      hexstart-hexend rw-p offset devid:devid inode                      pathname

      total_address_space_in_use = Sum of regions
      total_address_space_paged_in = ???  MISSING
      private_committed = Sum of Size for all entries with rw-p, and inode = 0?
      private_paged_in = ??? MISSING

      /proc/[pid]/statm:

      %zu %zu %zu ...  total_address_space_in_use total_address_space_paged_in file_shared_pages_paged_in

      (values are in pages)

      /proc/[pid]/smaps_rollup (Linux 3.16 onwards only):

      total_address_space_in_use = ??? MISSING
      total_address_space_paged_in = ??? MISSING
      private_committed = Anonymous - LazyFree (but, can't distinguish reservations!)
      private_paged_in = ??? MISSING

      */
      process_memory_usage ret;
      if(!!(want & process_memory_usage::want::this_process))
      {
        if(!(want & process_memory_usage::want::private_committed) || (want & process_memory_usage::want::private_committed_inaccurate))
        {
          if((want & process_memory_usage::want::total_address_space_in_use) || (want & process_memory_usage::want::total_address_space_paged_in) ||
             (want & process_memory_usage::want::private_paged_in))
          {
            std::vector<char> buffer(256);
            OUTCOME_TRY(fill_buffer(buffer, "/proc/self/statm"));
            if(buffer.size() > 1)
            {
              size_t file_and_shared_pages_paged_in = 0;
              sscanf(buffer.data(), "%zu %zu %zu", &ret.total_address_space_in_use, &ret.total_address_space_paged_in, &file_and_shared_pages_paged_in);
              ret.private_paged_in = ret.total_address_space_paged_in - file_and_shared_pages_paged_in;
              ret.total_address_space_in_use *= page_size();
              ret.total_address_space_paged_in *= page_size();
              ret.private_paged_in *= page_size();
              // std::cout << string_view(buffer.data(), buffer.size()) << std::endl;
            }
          }
          if(want & process_memory_usage::want::private_committed)
          {
            std::vector<char> smaps_rollup(256), maps(65536);
            auto r = fill_buffer(smaps_rollup, "/proc/self/smaps_rollup");
            if(!r)
            {
              if(r.error() == errc::no_such_file_or_directory)
              {
                // Linux kernel is too old
                return errc::operation_not_supported;
              }
              return std::move(r).error();
            }
            OUTCOME_TRY(fill_buffer(maps, "/proc/self/maps"));
            uint64_t lazyfree = 0;
            {
              string_view i(smaps_rollup.data(), smaps_rollup.size());
              OUTCOME_TRY(lazyfree, parse(i, "\nLazyFree:"));
            }
            string_view i(maps.data(), maps.size());
            size_t anonymous = 0;
            for(size_t idx = 0;;)
            {
              idx = i.find("\n", idx);
              if(idx == i.npos)
              {
                break;
              }
              idx++;
              size_t start = 0, end = 0, inode = 1;
              char read = 0, write = 0, executable = 0, private_ = 0;
              sscanf(i.data() + idx, "%zx-%zx %c%c%c%c %*u %*u:%*u %zd", &start, &end, &read, &write, &executable, &private_, &inode);
              if(inode == 0 && read == 'r' && write == 'w' && executable == '-' && private_ == 'p')
              {
                anonymous += end - start;
                // std::cout << (end - start) << " " << i.substr(idx, 40) << std::endl;
              }
            }
            if(lazyfree != (uint64_t) -1)
            {
              anonymous -= (size_t) lazyfree;
            }
            ret.private_committed = anonymous;
          }
        }
        else
        {
          std::vector<char> buffer(1024 * 1024);
          OUTCOME_TRY(fill_buffer(buffer, "/proc/self/smaps"));
          const string_view totalview(buffer.data(), buffer.size());
          // std::cerr << totalview << std::endl;
          std::vector<string_view> anon_entries, non_anon_entries;
          anon_entries.reserve(32);
          non_anon_entries.reserve(32);
          auto find_item = [&](size_t idx) -> string_view {
            auto x = totalview.rfind("\nSize:", idx);
            if(x == string_view::npos)
            {
              return {};
            }
            x = totalview.rfind("\n", x - 1);
            if(x == string_view::npos)
            {
              x = 0;
            }
            else
            {
              x++;
            }
            return totalview.substr(x, idx - x);
          };
          for(string_view item = find_item(totalview.size()); item != string_view(); item = find_item(item.data() - totalview.data()))
          {
            // std::cout << "***" << item << "***";
            // hexaddr-hexaddr flags offset dev:id inode [path]
            size_t inode = 1;
            sscanf(item.data(), "%*x-%*x %*c%*c%*c%*c %*x %*c%*c:%*c%*c %zu", &inode);
            auto vmflagsidx = item.rfind("\nVmFlags:");
            if(vmflagsidx == string_view::npos)
            {
              return errc::illegal_byte_sequence;
            }
            // Is there " ac" after vmflagsidx?
            if(string_view::npos != item.find(" ac", vmflagsidx) && inode == 0)
            {
              // std::cerr << "Adding anon entry at offset " << itemtopidx << std::endl;
              anon_entries.push_back(item);
            }
            else
            {
              // std::cerr << "Adding non-anon entry at offset " << itemtopidx << std::endl;
              non_anon_entries.push_back(item);
            }
          }
          // std::cerr << "Anon entries:";
          for(auto &i : anon_entries)
          {
            OUTCOME_TRY(auto &&size, parse(i, "\nSize:"));            // amount committed
            OUTCOME_TRY(auto &&rss, parse(i, "\nRss:"));              // amount paged in
            OUTCOME_TRY(auto &&anonymous, parse(i, "\nAnonymous:"));  // amount actually dirtied
            OUTCOME_TRY(auto &&lazyfree, parse(i, "\nLazyFree:"));    // amount "decommitted" on Linux to avoid a VMA split
            if(size != (uint64_t) -1 && rss != (uint64_t) -1 && anonymous != (uint64_t) -1)
            {
              ret.total_address_space_in_use += size;
              ret.total_address_space_paged_in += rss;
              ret.private_committed += size;
              if(lazyfree != (uint64_t) -1)
              {
                ret.total_address_space_paged_in -= lazyfree;
                ret.private_committed -= lazyfree;
              }
              ret.private_paged_in += rss;
            }
            // std::cerr << i << "\nSize = " << size << " Rss = " << rss << std::endl;
          }
          // std::cerr << "\n\nNon-anon entries:";
          for(auto &i : non_anon_entries)
          {
            OUTCOME_TRY(auto &&size, parse(i, "\nSize:"));
            OUTCOME_TRY(auto &&rss, parse(i, "\nRss:"));
            OUTCOME_TRY(auto &&lazyfree, parse(i, "\nLazyFree:"));
            if(size != (uint64_t) -1 && rss != (uint64_t) -1)
            {
              ret.total_address_space_in_use += size;
              ret.total_address_space_paged_in += rss;
              if(lazyfree != (uint64_t) -1)
              {
                ret.total_address_space_in_use -= lazyfree;
              }
            }
            // std::cerr << i << "\nSize = " << size << " Rss = " << rss << std::endl;
          }
        }
      }
      if(!!(want & process_memory_usage::want::this_system))
      {
        std::vector<char> buffer(1024);
        OUTCOME_TRY(fill_buffer(buffer, "/proc/meminfo"));
        if(buffer.size() > 1)
        {
          string_view i(buffer.data(), buffer.size());
          OUTCOME_TRY(ret.system_physical_memory_total, parse(i, "MemTotal:"));
          OUTCOME_TRY(ret.system_physical_memory_available, parse(i, "\nMemAvailable:"));
          if((uint64_t) -1 == ret.system_physical_memory_available)
          {
            // MemAvailable is >= Linux 3.14, so let's approximate what it would be
            OUTCOME_TRY(auto &&memfree, parse(i, "\nMemFree:"));
            OUTCOME_TRY(auto &&cached, parse(i, "\nCached:"));
            OUTCOME_TRY(auto &&swapcached, parse(i, "\nSwapCached:"));
            ret.system_physical_memory_available = memfree + cached + swapcached;
          }
          OUTCOME_TRY(ret.system_commit_charge_maximum, parse(i, "\nCommitLimit:"));
          OUTCOME_TRY(ret.system_commit_charge_available, parse(i, "\nCommitted_AS:"));
          OUTCOME_TRY(auto &&lazyfree, parse(i, "\nLazyFree:"));
          if(lazyfree == (uint64_t) -1)
          {
            lazyfree = 0;
          }
          ret.system_commit_charge_available = ret.system_commit_charge_maximum - ret.system_commit_charge_available + lazyfree;
        }
      }
      return ret;
    }
    catch(...)
    {
      return error_from_exception();
    }
#elif defined(__APPLE__)
    (void) want;
    kern_return_t error;
    mach_msg_type_number_t outCount;
    process_memory_usage ret;
    if(!!(want & process_memory_usage::want::this_process))
    {
      task_vm_info_data_t vmInfo;
      // task_kernelmemory_info_data_t kmInfo;

      outCount = TASK_VM_INFO_COUNT;
      error = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t) &vmInfo, &outCount);
      if(error != KERN_SUCCESS)
      {
        return errc::invalid_argument;
      }
      // outCount = TASK_KERNELMEMORY_INFO_COUNT;
      // error = task_info(mach_task_self(), TASK_KERNELMEMORY_INFO, (task_info_t)&kmInfo, &outCount);
      // if (error != KERN_SUCCESS) {
      //  return errc::invalid_argument;
      //}
      // std::cout << vmInfo.virtual_size << "\n" << vmInfo.region_count << "\n" << vmInfo.resident_size << "\n" << vmInfo.device << "\n" << vmInfo.internal <<
      // "\n" << vmInfo.external << "\n" << vmInfo.reusable << "\n" << vmInfo.purgeable_volatile_pmap<< "\n" << vmInfo.purgeable_volatile_resident << "\n" <<
      // vmInfo.purgeable_volatile_virtual << "\n" << vmInfo.compressed << "\n" << vmInfo.phys_footprint << std::endl; std::cout << "\n" << kmInfo.total_palloc
      // <<
      // "\n" << kmInfo.total_pfree << "\n" << kmInfo.total_salloc << "\n" << kmInfo.total_sfree << std::endl;
      ret.total_address_space_in_use = vmInfo.virtual_size;
      ret.total_address_space_paged_in = vmInfo.resident_size;
      ret.private_committed = vmInfo.internal + vmInfo.compressed;
      ret.private_paged_in = vmInfo.phys_footprint;
    }
    if(!!(want & process_memory_usage::want::this_system))
    {
      vm_statistics_data_t vmStat;
      outCount = HOST_VM_INFO_COUNT;
      error = host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t) &vmStat, &outCount);
      if(error != KERN_SUCCESS)
      {
        return errc::invalid_argument;
      }
      ret.system_physical_memory_total = ((uint64_t) vmStat.free_count + vmStat.active_count + vmStat.inactive_count + vmStat.wire_count) * page_size();
      ret.system_physical_memory_available = ((uint64_t) vmStat.free_count + vmStat.inactive_count) * page_size();
      // Not sure how to retrieve these on Mac OS
      // ret.system_commit_charge_maximum = (uint64_t) pi.CommitLimit * page_size();
      // ret.system_commit_charge_available = (uint64_t) pi.CommitTotal * page_size();
    }
    return ret;
#else
#error Unknown platform
#endif
  }

  result<process_cpu_usage> current_process_cpu_usage() noexcept
  {
    process_cpu_usage ret;
#ifdef __linux__
    try
    {
      /* Need to multiply all below by 1000000000ULL / sysconf(_SC_CLK_TCK)

      /proc/[pid]/stat:

      %*d %*s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu

      The last two are process user time and process kernel time.

      /proc/stat:

      cpu <user> <user-nice> <kernel> <idle>
      */
      std::vector<char> buffer1(65536), buffer2(65536);
      auto fill_buffer = [](std::vector<char> &buffer, const char *path) -> result<void> {
        for(;;)
        {
          int ih = ::open(path, O_RDONLY);
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
        return success();
      };
      static const uint64_t ts_multiplier = 1000000000ULL / sysconf(_SC_CLK_TCK);
      OUTCOME_TRY(fill_buffer(buffer1, "/proc/self/stat"));
      OUTCOME_TRY(fill_buffer(buffer2, "/proc/stat"));
      if(sscanf(buffer1.data(), "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ret.process_ns_in_user_mode, &ret.process_ns_in_kernel_mode) <
         2)
      {
        return errc::protocol_error;
      }
      uint64_t user_nice;
      if(sscanf(buffer2.data(), "cpu %lu %lu %lu %lu", &ret.system_ns_in_user_mode, &user_nice, &ret.system_ns_in_kernel_mode, &ret.system_ns_in_idle_mode) < 4)
      {
        return errc::protocol_error;
      }
      ret.system_ns_in_user_mode += user_nice;
      ret.process_ns_in_user_mode *= ts_multiplier;
      ret.process_ns_in_kernel_mode *= ts_multiplier;
      ret.system_ns_in_user_mode *= ts_multiplier;
      ret.system_ns_in_kernel_mode *= ts_multiplier;
      ret.system_ns_in_idle_mode *= ts_multiplier;
      return ret;
    }
    catch(...)
    {
      return error_from_exception();
    }
#elif defined(__APPLE__)
    kern_return_t error;
    mach_msg_type_number_t outCount;
    task_basic_info_64 processInfo1;
    task_thread_times_info processInfo2;

    outCount = TASK_BASIC_INFO_64_COUNT;
    error = task_info(mach_task_self(), TASK_BASIC_INFO_64, (task_info_t) &processInfo1, &outCount);
    if(error != KERN_SUCCESS)
    {
      return errc::invalid_argument;
    }
    outCount = TASK_THREAD_TIMES_INFO_COUNT;
    error = task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t) &processInfo2, &outCount);
    if(error != KERN_SUCCESS)
    {
      return errc::invalid_argument;
    }
    ret.process_ns_in_user_mode = (processInfo1.user_time.seconds + processInfo2.user_time.seconds) * 1000000000ULL +
                                  (processInfo1.user_time.microseconds + processInfo2.user_time.microseconds) * 1000ULL;
    ret.process_ns_in_kernel_mode = (processInfo1.system_time.seconds + processInfo2.system_time.seconds) * 1000000000ULL +
                                    (processInfo1.system_time.microseconds + processInfo2.system_time.microseconds) * 1000ULL;

    natural_t numCPU = 0;
    processor_info_array_t cpuInfo;
    mach_msg_type_number_t numCpuInfo;
    error = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numCPU, &cpuInfo, &numCpuInfo);
    if(error != KERN_SUCCESS)
    {
      return errc::invalid_argument;
    }
    for(natural_t n = 0; n < numCPU; n++)
    {
      ret.system_ns_in_user_mode += cpuInfo[CPU_STATE_MAX * n + CPU_STATE_USER] + cpuInfo[CPU_STATE_MAX * n + CPU_STATE_NICE];
      ret.system_ns_in_kernel_mode += cpuInfo[CPU_STATE_MAX * n + CPU_STATE_SYSTEM];
      ret.system_ns_in_idle_mode += cpuInfo[CPU_STATE_MAX * n + CPU_STATE_IDLE];
    }
    vm_deallocate(mach_task_self(), (vm_address_t) cpuInfo, sizeof(integer_t) * numCpuInfo);
    static const double ts_multiplier = /*[] {
      mach_timebase_info_data_t timebase;
      mach_timebase_info(&timebase);
      return (double) timebase.numer / timebase.denom;
    }();*/
    10000000.0;                         // no idea why, but apparently this is the multiplier according to Mac CI runners
    ret.system_ns_in_user_mode = (uint64_t)(ts_multiplier * ret.system_ns_in_user_mode);
    ret.system_ns_in_kernel_mode = (uint64_t)(ts_multiplier * ret.system_ns_in_kernel_mode);
    ret.system_ns_in_idle_mode = (uint64_t)(ts_multiplier * ret.system_ns_in_idle_mode);
    return ret;
#else
#error Unknown platform
#endif
    return ret;
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
