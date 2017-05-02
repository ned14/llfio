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

#include "../../../spinlock.hpp"

#include <sys/mman.h>

BOOST_AFIO_V2_NAMESPACE_BEGIN

namespace utils
{
  size_t page_size() noexcept
  {
    static size_t ret;
    if(!ret)
      ret = getpagesize();
    return ret;
  }
  std::vector<size_t> page_sizes(bool only_actually_available)
  {
    static boost_lite::configurable_spinlock::spinlock<bool> lock;
    static std::vector<size_t> pagesizes, pagesizes_available;
    stl11::lock_guard<decltype(lock)> g(lock);
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
      int ih = ::open("/proc/meminfo", O_RDONLY);
      if(-1 != ih)
      {
        char buffer[4096], *hugepagesize, *hugepages;
        buffer[ ::read(ih, buffer, sizeof(buffer) - 1)] = 0;
        ::close(ih);
        hugepagesize = strstr(buffer, "Hugepagesize:");
        hugepages = strstr(buffer, "HugePages_Total:");
        if(hugepagesize && hugepages)
        {
          unsigned _hugepages = 0, _hugepagesize = 0;
          while(*++hugepagesize != ' ')
            ;
          while(*++hugepages != ' ')
            ;
          while(*++hugepagesize == ' ')
            ;
          while(*++hugepages == ' ')
            ;
          sscanf(hugepagesize, "%u", &_hugepagesize);
          sscanf(hugepages, "%u", &_hugepages);
          if(_hugepagesize)
          {
            pagesizes.push_back(((size_t) _hugepagesize) * 1024);
            if(_hugepages)
              pagesizes_available.push_back(((size_t) _hugepagesize) * 1024);
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

  void random_fill(char *buffer, size_t bytes)
  {
    static boost_lite::configurable_spinlock::spinlock<bool> lock;
    static int randomfd = -1;
    if(-1 == randomfd)
    {
      stl11::lock_guard<decltype(lock)> g(lock);
      randomfd = ::open("/dev/urandom", O_RDONLY);
    }
    if(-1 == randomfd || ::read(randomfd, buffer, bytes) < (ssize_t) bytes)
    {
      BOOST_AFIO_LOG_FATAL(0, "afio: Kernel crypto function failed");
      std::terminate();
    }
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
      if(!(ret.p = mmap(nullptr, ret.actual_size, PROT_WRITE, flags, -1, 0)))
      {
        if(ENOMEM == errno)
          if((ret.p = mmap(nullptr, ret.actual_size, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0)))
            return ret;
      }
#ifndef NDEBUG
      else if(ret.page_size_used > 65536)
        printf("afio: Large page allocation successful\n");
#endif
      return ret;
    }
    void deallocate_large_pages(void *p, size_t bytes)
    {
      if(munmap(p, bytes) < 0)
      {
        BOOST_AFIO_LOG_FATAL(p, "afio: Freeing large pages failed");
        std::terminate();
      }
    }
  }
}

BOOST_AFIO_V2_NAMESPACE_END
