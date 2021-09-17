/* A handle to a source of mapped memory
(C) 2021 Niall Douglas <http://www.nedproductions.biz/> (10 commits)
File Created: Aug 2021


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

#include "../../map_handle.hpp"
#include "../../utils.hpp"

#include <chrono>
#include <mutex>

#include <quickcpplib/algorithm/bitwise_trie.hpp>

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  struct map_handle_cache_item_t
  {
    map_handle_cache_item_t *trie_parent{nullptr};
    map_handle_cache_item_t *trie_child[2];
    map_handle_cache_item_t *trie_sibling[2];
    size_t trie_key{0};  // allocation size shifted right by map_handle_cache_page_size_shift
    size_t page_size{0};
    void *addr{nullptr};
    std::chrono::steady_clock::time_point when_added{std::chrono::steady_clock::now()};

    map_handle_cache_item_t(size_t key, size_t _page_size, void *_addr)
        : trie_key(key)
        , page_size(_page_size)
        , addr(_addr)
    {
    }
  };
  struct map_handle_cache_base_t
  {
    mutable std::mutex lock;
    size_t trie_count{0};
    map_handle_cache_item_t *trie_children[8 * sizeof(size_t)];
    bool trie_nobbledir{0}, disabled{false};
    size_t bytes_in_cache{0}, hits{0}, misses{0};
  };
  inline size_t page_size_shift()
  {
    static size_t v = [] { return QUICKCPPLIB_NAMESPACE::algorithm::bitwise_trie::detail::bitscanr(utils::page_size()); }();
    return v;
  }
  class map_handle_cache_t : protected QUICKCPPLIB_NAMESPACE::algorithm::bitwise_trie::bitwise_trie<map_handle_cache_base_t, map_handle_cache_item_t>
  {
    using _base = QUICKCPPLIB_NAMESPACE::algorithm::bitwise_trie::bitwise_trie<map_handle_cache_base_t, map_handle_cache_item_t>;
    using _lock_guard = std::unique_lock<std::mutex>;

  public:
#ifdef __linux__
    std::atomic<unsigned> do_not_store_failed_count{0};
#endif

    ~map_handle_cache_t() { trim_cache(std::chrono::steady_clock::now(), (size_t) -1); }

    bool is_disabled() const noexcept
    {
      _lock_guard g(lock);
      return _base::disabled;
    }

    using _base::size;
    void *get(size_t bytes, size_t page_size)
    {
      const auto _bytes = bytes >> page_size_shift();
      _lock_guard g(lock);
      // TODO: Consider finding slightly bigger, and returning a length shorter than reservation?
      auto it = _base::find(_bytes);
      for(; it != _base::end() && page_size != it->page_size && _bytes == it->trie_key; ++it)
      {
      }
      if(it == _base::end() || page_size != it->page_size || _bytes != it->trie_key)
      {
        misses++;
        return nullptr;
      }
      hits++;
      auto *p = *it;
      _base::erase(it);
      _base::bytes_in_cache -= bytes;
      assert(bytes == p->trie_key << page_size_shift());
      // std::cout << "map_handle::get(" << p->addr << ", " << bytes << "). Index item was " << p << std::endl;
      g.unlock();
      void *ret = p->addr;
      delete p;
      return ret;
    }
    bool add(size_t bytes, size_t page_size, void *addr)
    {
      const auto _bytes = bytes >> page_size_shift();
      if(_bytes == 0)
      {
        return false;
      }
      auto *p = new map_handle_cache_item_t(_bytes, page_size, addr);
      _lock_guard g(lock);
      // std::cout << "map_handle::add(" << addr << ", " << bytes << "). Index item was " << p << ". Trie key is " << _bytes << std::endl;
      _base::insert(p);
      _base::bytes_in_cache += bytes;
      return true;
    }
    map_handle::cache_statistics trim_cache(std::chrono::steady_clock::time_point older_than, size_t max_items)
    {
      _lock_guard g(lock);
      map_handle::cache_statistics ret;

      if(older_than != std::chrono::steady_clock::time_point() && max_items > 0)
      {
        // Prefer bigger items to trim than smaller ones
        for(auto it = --_base::end(); it != _base::end() && max_items > 0;)
        {
          if(it->when_added <= older_than)
          {
            auto *p = *it;
            _base::erase(it--);
            const auto _bytes = p->trie_key << page_size_shift();
            // std::cout << "map_handle::trim_cache(" << p->addr << ", " << _bytes << "). Index item was " << p << ". Trie key is " << p->trie_key << std::endl;
#ifdef _WIN32
            if(!win32_release_nonfile_allocations((byte *) p->addr, _bytes, MEM_RELEASE))
#else
            if(-1 == ::munmap(p->addr, _bytes))
#endif
            {
              // fprintf(stderr, "munmap failed with %s. addr was %p bytes was %zu. page_size_shift was %zu\n", strerror(errno), p->addr, _bytes,
              // page_size_shift);
              LLFIO_LOG_FATAL(nullptr,
                              "FATAL: map_handle cache failed to trim a map! If on Linux, you may have exceeded the "
                              "64k VMA process limit, set the LLFIO_DEBUG_LINUX_MUNMAP macro at the top of posix/map_handle.ipp to cause dumping of VMAs to "
                              "/tmp/llfio_unmap_debug_smaps.txt, and combine with strace to figure it out.");
              abort();
            }
            _base::bytes_in_cache -= _bytes;
            ret.bytes_just_trimmed += _bytes;
            ret.items_just_trimmed++;
            max_items--;
            delete p;
          }
          else
          {
            --it;
          }
        }
      }
      ret.items_in_cache = _base::size();
      ret.bytes_in_cache = _base::bytes_in_cache;
      ret.hits = _base::hits;
      ret.misses = _base::misses;
      return ret;
    }
    bool set_cache_disabled(bool v)
    {
      _lock_guard g(lock);
      bool ret = _base::disabled;
      _base::disabled = v;
      return ret;
    }
  };
  extern inline QUICKCPPLIB_SYMBOL_VISIBLE map_handle_cache_t *map_handle_cache()
  {
    static struct _
    {
      map_handle_cache_t *v;
      _()
          : v(new map_handle_cache_t)
      {
      }
      ~_()
      {
        if(v != nullptr)
        {
          // std::cout << "map_handle_cache static deinit begin" << std::endl;
          auto *r = v;
          *(volatile map_handle_cache_t **) &v = nullptr;
          delete r;
          // std::cout << "map_handle_cache static deinit complete" << std::endl;
        }
      }
    } v;
    return v.v;
  }
}  // namespace detail

result<map_handle> map_handle::_recycled_map(size_type bytes, section_handle::flag _flag) noexcept
{
  if(bytes == 0u)
  {
    return errc::argument_out_of_domain;
  }
  auto *c = detail::map_handle_cache();
  if(c == nullptr || c->is_disabled() || bytes >= (1ULL << 30U) /*1Gb*/)
  {
    return _new_map(bytes, false, _flag);
  }
  result<map_handle> ret(map_handle(nullptr, _flag));
  native_handle_type &nativeh = ret.value()._v;
  OUTCOME_TRY(auto &&pagesize, detail::pagesize_from_flags(ret.value()._flag));
  bytes = utils::round_up_to_page_size(bytes, pagesize);
  LLFIO_LOG_FUNCTION_CALL(&ret);
  void *addr = c->get(bytes, pagesize);
  if(addr == nullptr)
  {
    return _new_map(bytes, false, _flag);
  }
#ifdef _WIN32
  {
    DWORD allocation = MEM_COMMIT, prot;
    size_t commitsize;
    OUTCOME_TRY(win32_map_flags(nativeh, allocation, prot, commitsize, true, _flag));
    if(VirtualAlloc(addr, bytes, allocation, prot) == nullptr)
    {
      return win32_error();
    }
    // Windows has no way of getting the kernel to prefault maps on creation, so ...
    if(_flag & section_handle::flag::prefault)
    {
      using namespace windows_nt_kernel;
      // Start an asynchronous prefetch, so it might fault the whole lot in at once
      buffer_type b{static_cast<byte *>(addr), bytes};
      (void) prefetch(span<buffer_type>(&b, 1));
      volatile auto *a = static_cast<volatile char *>(addr);
      for(size_t n = 0; n < bytes; n += pagesize)
      {
        a[n];
      }
    }
  }
#else
  int flags = MAP_FIXED;
#ifdef MAP_POPULATE
  if(_flag & section_handle::flag::prefault)
  {
    flags |= MAP_POPULATE;
  }
#endif
#ifdef MAP_PREFAULT_READ
  if(_flag & section_handle::flag::prefault)
    flags |= MAP_PREFAULT_READ;
#endif
  OUTCOME_TRY(do_mmap(nativeh, addr, flags, nullptr, pagesize, bytes, 0, _flag));
#endif
  ret.value()._addr = static_cast<byte *>(addr);
  ret.value()._reservation = bytes;
  ret.value()._length = bytes;
  ret.value()._pagesize = pagesize;
  nativeh._init = -2;  // otherwise appears closed
  nativeh.behaviour |= native_handle_type::disposition::allocation;
  return ret;
}

bool map_handle::_recycle_map() noexcept
{
  try
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    auto *c = detail::map_handle_cache();
    if(c == nullptr || c->is_disabled() || _reservation >= (1ULL << 30U) /*1Gb*/)
    {
      return false;
    }
#ifdef _WIN32
    if(!win32_release_nonfile_allocations(_addr, _length, MEM_DECOMMIT))
    {
      return false;
    }
#else
#ifdef __linux__
    if(memory_accounting() == memory_accounting_kind::commit_charge)
#endif
    {
      if(!do_mmap(_v, _addr, MAP_FIXED, nullptr, _pagesize, _length, 0, section_handle::flag::none | section_handle::flag::nocommit))
      {
        return false;
      }
    }
#ifdef __linux__
    else
    {
      if(c->do_not_store_failed_count.load(std::memory_order_relaxed) < 10)
      {
        auto r = do_not_store({_addr, _length});
        if(!r || r.assume_value().size() == 0)
        {
          c->do_not_store_failed_count.fetch_add(1, std::memory_order_relaxed);
        }
      }
    }
#endif
#endif
    return c->add(_reservation, _pagesize, _addr);
  }
  catch(...)
  {
    return false;
  }
}

map_handle::cache_statistics map_handle::trim_cache(std::chrono::steady_clock::time_point older_than, size_t max_items) noexcept
{
  auto *c = detail::map_handle_cache();
  return (c != nullptr) ? c->trim_cache(older_than, max_items) : cache_statistics{};
}

bool map_handle::set_cache_disabled(bool disabled) noexcept
{
  auto *c = detail::map_handle_cache();
  return (c != nullptr) ? c->set_cache_disabled(disabled) : true;
}

LLFIO_V2_NAMESPACE_END
