/* A profile of an OS and filing system
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (5 commits)
File Created: Jan 2016


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

#include "../../../handle.hpp"
#include "../../../storage_profile.hpp"

#include <sys/ioctl.h>
#include <sys/utsname.h>  // for uname()
#include <unistd.h>
#if defined(__linux__)
#include <linux/fs.h>
#else
#include <sys/disk.h>
#include <sys/sysctl.h>
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace storage_profile
{
  namespace system
  {
    // OS name, version
    outcome<void> os(storage_profile &sp, file_handle & /*unused*/) noexcept
    {
      static std::string os_name, os_ver;
      if(!os_name.empty())
      {
        sp.os_name.value = os_name;
        sp.os_ver.value = os_ver;
      }
      else
      {
        try
        {
          struct utsname name
          {
          };
          memset(&name, 0, sizeof(name));
          if(uname(&name) < 0)
          {
            return posix_error();
          }
          sp.os_name.value = os_name = name.sysname;
          sp.os_ver.value = os_ver = name.release;
        }
        catch(...)
        {
          return std::current_exception();
        }
      }
      return success();
    }

    // CPU name, architecture, physical cores
    outcome<void> cpu(storage_profile &sp, file_handle & /*unused*/) noexcept
    {
      static std::string cpu_name, cpu_architecture;
      static unsigned cpu_physical_cores;
      if(!cpu_name.empty())
      {
        sp.cpu_name.value = cpu_name;
        sp.cpu_architecture.value = cpu_architecture;
        sp.cpu_physical_cores.value = cpu_physical_cores;
      }
      else
      {
        try
        {
          struct utsname name
          {
          };
          memset(&name, 0, sizeof(name));
          if(uname(&name) < 0)
          {
            return posix_error();
          }
          sp.cpu_name.value = sp.cpu_architecture.value = name.machine;
          sp.cpu_physical_cores.value = 0;
#if defined(__linux__)
          {
            int ih = ::open("/proc/cpuinfo", O_RDONLY | O_CLOEXEC);
            if(ih >= 0)
            {
              char cpuinfo[8192];
              cpuinfo[ ::read(ih, cpuinfo, sizeof(cpuinfo) - 1)] = 0;
              ::close(ih);
              /* If siblings > cpu cores hyperthread is enabled:
              siblings   : 8
              cpu cores  : 4
              */
              const char *siblings = strstr(cpuinfo, "siblings");
              const char *cpucores = strstr(cpuinfo, "cpu cores");
              if((siblings != nullptr) && (cpucores != nullptr))
              {
                for(siblings = strchr(siblings, ':'); ' ' == *siblings; siblings++)
                {
                  ;
                }
                for(cpucores = strchr(cpucores, ':'); ' ' == *cpucores; cpucores++)
                {
                  ;
                }
                int s = atoi(siblings), c = atoi(cpucores);  // NOLINT
                if((s != 0) && (c != 0))
                {
                  sp.cpu_physical_cores.value = sysconf(_SC_NPROCESSORS_ONLN) * c / s;
                }
              }
            }
          }
#else
          // Currently only available on OS X
          {
            int physicalCores = 0;
            size_t len = sizeof(physicalCores);
            if(sysctlbyname("hw.physicalcpu", &physicalCores, &len, NULL, 0) >= 0)
              sp.cpu_physical_cores.value = physicalCores;
          }
          if(!sp.cpu_physical_cores.value)
          {
            char topology[8192];
            size_t len = sizeof(topology) - 1;
            if(sysctlbyname("kern.sched.topology_spec", topology, &len, NULL, 0) >= 0)
            {
              topology[len] = 0;
              sp.cpu_physical_cores.value = sysconf(_SC_NPROCESSORS_ONLN);
              if(strstr(topology, "HTT"))
                sp.cpu_physical_cores.value /= 2;
            }
          }
#endif
          // Doesn't account for any hyperthreading
          if(sp.cpu_physical_cores.value == 0u)
          {
            sp.cpu_physical_cores.value = sysconf(_SC_NPROCESSORS_ONLN);
          }
#if defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
          // We can do a much better CPU name on x86/x64
          sp.cpu_name.value.clear();
          auto x86cpuid = [](int *cpuInfo, int func) { __asm__ __volatile__("cpuid\n\t" : "=a"(cpuInfo[0]), "=b"(cpuInfo[1]), "=c"(cpuInfo[2]), "=d"(cpuInfo[3]) : "0"(func)); };  // NOLINT
          {
            char buffer[62];
            memset(buffer, 32, 62);
            int nBuff[4];
            x86cpuid(nBuff, 0);
            memcpy(buffer + 0, nBuff + 1, 4);
            *reinterpret_cast<int *>(buffer + 4) = nBuff[3];
            *reinterpret_cast<int *>(buffer + 8) = nBuff[2];

            // Do we have a brand string?
            x86cpuid(nBuff, 0x80000000);
            if(static_cast<unsigned>(nBuff[0]) >= 0x80000004)
            {
              x86cpuid(reinterpret_cast<int *>(&buffer[14]), 0x80000002);
              x86cpuid(reinterpret_cast<int *>(&buffer[30]), 0x80000003);
              x86cpuid(reinterpret_cast<int *>(&buffer[46]), 0x80000004);
            }
            else
            {
              memcpy(&buffer[14], "unbranded", 10);
            }

            // Trim string
            for(size_t n = 0; n < 62; n++)
            {
              if((n == 0u) || buffer[n] != 32 || buffer[n - 1] != 32)
              {
                if(buffer[n] != 0)
                {
                  sp.cpu_name.value.push_back(buffer[n]);
                }
              }
            }
          }
#endif
          cpu_name = sp.cpu_name.value;
          cpu_architecture = sp.cpu_architecture.value;
          cpu_physical_cores = sp.cpu_physical_cores.value;
        }
        catch(...)
        {
          return std::current_exception();
        }
      }
      return success();
    }
    namespace posix
    {
      outcome<void> _mem(storage_profile &sp, file_handle & /*unused*/) noexcept
      {
#if defined(_SC_PHYS_PAGES)
        size_t physpages = sysconf(_SC_PHYS_PAGES), pagesize = sysconf(_SC_PAGESIZE);
        sp.mem_quantity.value = static_cast<unsigned long long>(physpages) * pagesize;
#if defined(_SC_AVPHYS_PAGES)
        size_t freepages = sysconf(_SC_AVPHYS_PAGES);
        sp.mem_in_use.value = static_cast<float>(physpages - freepages) / physpages;
#elif defined(HW_USERMEM)
        unsigned long long freemem = 0;
        size_t len = sizeof(freemem);
        int mib[2] = {CTL_HW, HW_USERMEM};
        if(sysctl(mib, 2, &freemem, &len, nullptr, 0) >= 0)
        {
          size_t freepages = (size_t)(freemem / pagesize);
          sp.mem_in_use.value = (float) (physpages - freepages) / physpages;
        }
#else
#error Do not know how to get free physical RAM on this platform
#endif
#endif
        return success();
      }
    }  // namespace posix
  }    // namespace system
  namespace storage
  {
    namespace posix
    {
      // Controller type, max transfer, max buffers. Device name, size
      outcome<void> _device(storage_profile &sp, file_handle & /*unused*/, const std::string &_mntfromname, const std::string &fstypename) noexcept
      {
        (void) fstypename;
        try
        {
          std::string mntfromname(_mntfromname);
          // Firstly open a handle to the device
          if(strncmp(mntfromname.data(), "/dev", 4) == 0)
          {
            if(std::isdigit(mntfromname.back()) != 0)
            {
              mntfromname.resize(mntfromname.size() - 1);
            }
          }
          else
          {
// If the mount point doesn't begin with /dev we can't use that here, so return ENOSYS
#ifdef __FreeBSD__
            // If on ZFS and there is exactly one physical disk in the system, use that
            if(fstypename == "zfs")
            {
              char buffer[4096];
              size_t len = sizeof(buffer);
              if(sysctlbyname("kern.disks", buffer, &len, NULL, 0) >= 0)
              {
                mntfromname.clear();
                // Might be a string like "ada0 cd0 ..."
                const char *s, *e = buffer;
                for(; e < buffer + len; e++)
                {
                  for(s = e; e < buffer + len && *e != ' '; e++)
                    ;
                  if(s[0] == 'c' && s[1] == 'd')
                    continue;
                  if(s[0] == 'f' && s[1] == 'd')
                    continue;
                  if(s[0] == 'm' && s[1] == 'c' && s[2] == 'd')
                    continue;
                  if(s[0] == 's' && s[1] == 'c' && s[2] == 'd')
                    continue;
                  // Is there more than one physical disk device?
                  if(!mntfromname.empty())
                    return errc::function_not_supported;
                  mntfromname = "/dev/" + std::string(s, e - s);
                }
              }
              else
                return errc::function_not_supported;
            }
            else
#endif
              return errc::function_not_supported;
          }
          OUTCOME_TRY(auto &&deviceh, file_handle::file({}, mntfromname, handle::mode::none, handle::creation::open_existing, handle::caching::only_metadata));

// TODO(ned): See https://github.com/baruch/diskscan/blob/master/arch/arch-linux.c
//          sp.controller_type.value = "SCSI";
//          sp.controller_max_transfer.value = sad->MaximumTransferLength;
//          sp.controller_max_buffers.value = sad->MaximumPhysicalPages;
//          sp.device_name.value.resize(sp.device_name.value.size() - 1);

#ifdef DIOCGMEDIASIZE
          // BSDs
          ioctl(deviceh.native_handle().fd, DIOCGMEDIASIZE, &sp.device_size.value);
#endif
#ifdef BLKGETSIZE64
          // Linux
          ioctl(deviceh.native_handle().fd, BLKGETSIZE64, &sp.device_size.value);
#endif
#ifdef DKIOCGETBLOCKCOUNT
          // OS X
          ioctl(deviceh.native_handle().fd, DKIOCGETBLOCKCOUNT, &sp.device_size.value);
#endif
        }
        catch(...)
        {
          return std::current_exception();
        }
        return success();
      }
    }  // namespace posix
  }    // namespace storage
}  // namespace storage_profile

LLFIO_V2_NAMESPACE_END
