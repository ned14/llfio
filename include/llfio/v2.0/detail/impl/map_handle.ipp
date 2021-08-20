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
    std::mutex lock;
    size_t trie_count{0};
    map_handle_cache_item_t *trie_children[8 * sizeof(size_t)];
    bool trie_nobbledir{0};
    size_t bytes_in_cache{0};
  };
  static const size_t page_size_shift = [] { return QUICKCPPLIB_NAMESPACE::algorithm::bitwise_trie::detail::bitscanr(utils::page_size()); }();
  class map_handle_cache_t : protected QUICKCPPLIB_NAMESPACE::algorithm::bitwise_trie::bitwise_trie<map_handle_cache_base_t, map_handle_cache_item_t>
  {
    using _base = QUICKCPPLIB_NAMESPACE::algorithm::bitwise_trie::bitwise_trie<map_handle_cache_base_t, map_handle_cache_item_t>;
    using _lock_guard = std::unique_lock<std::mutex>;

  public:
    std::atomic<unsigned> do_not_store_failed_count{0};

    using _base::size;
    void *get(size_t bytes, size_t page_size)
    {
      const auto _bytes = bytes >> page_size_shift;
      _lock_guard g(lock);
      auto it = _base::find_equal_or_larger(_bytes, 7 /* 99% close to the key after page size shift */);
      for(; it != _base::end() && page_size != it->page_size && _bytes == it->trie_key; ++it)
      {
      }
      if(it == _base::end() || page_size != it->page_size || _bytes != it->trie_key)
      {
        return nullptr;
      }
      auto *p = *it;
      _base::erase(it);
      _base::bytes_in_cache -= bytes;
      assert(bytes == p->trie_key << page_size_shift);
      g.unlock();
      void *ret = p->addr;
      delete p;
      return ret;
    }
    void add(size_t bytes, size_t page_size, void *addr)
    {
      const auto _bytes = bytes >> page_size_shift;
      auto *p = new map_handle_cache_item_t(_bytes, page_size, addr);
      _lock_guard g(lock);
      _base::insert(p);
      _base::bytes_in_cache += bytes;
    }
    map_handle::cache_statistics trim_cache(std::chrono::steady_clock::time_point older_than)
    {
      _lock_guard g(lock);
      map_handle::cache_statistics ret;
      if(older_than != std::chrono::steady_clock::time_point())
      {
        for(auto it = _base::begin(); it != _base::end();)
        {
          if(it->when_added < older_than)
          {
            auto *p = *it;
            it = _base::erase(it);
            const auto _bytes = p->trie_key << page_size_shift;
            _base::bytes_in_cache -= _bytes;
            ret.bytes_just_trimmed += _bytes;
            ret.items_just_trimmed++;
            delete p;
          }
          else
          {
            ++it;
          }
        }
      }
      ret.items_in_cache = _base::size();
      ret.bytes_in_cache = _base::bytes_in_cache;
      return ret;
    }
  };
  extern inline QUICKCPPLIB_SYMBOL_EXPORT map_handle_cache_t &map_handle_cache()
  {
    static map_handle_cache_t v;
    return v;
  }
}  // namespace detail

result<map_handle> map_handle::_recycled_map(size_type bytes, section_handle::flag _flag) noexcept
{
  if(bytes == 0u)
  {
    return errc::argument_out_of_domain;
  }
  result<map_handle> ret(map_handle(nullptr, _flag));
  native_handle_type &nativeh = ret.value()._v;
  OUTCOME_TRY(auto &&pagesize, detail::pagesize_from_flags(ret.value()._flag));
  bytes = utils::round_up_to_page_size(bytes, pagesize);
  LLFIO_LOG_FUNCTION_CALL(&ret);
  void *addr = detail::map_handle_cache().get(bytes, pagesize);
  if(addr == nullptr)
  {
    return _new_map(bytes, _flag);
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
    auto &c = detail::map_handle_cache();
#ifdef _WIN32
    if(!win32_release_nonfile_allocations(_addr, _length, MEM_DECOMMIT))
    {
      return false;
    }
#else
    if(memory_accounting() == memory_accounting_kind::commit_charge)
    {
      if(!do_mmap(_v, _addr, MAP_FIXED, nullptr, _pagesize, _length, 0, section_handle::flag::none | section_handle::flag::nocommit))
      {
        return false;
      }
    }
    else
    {
#ifdef __linux__
      if(c.do_not_store_failed_count.load(std::memory_order_relaxed) < 10)
      {
        auto r = do_not_store({_addr, _length});
        if(!r)
        {
          c.do_not_store_failed_count.fetch_add(1, std::memory_order_relaxed);
        }
      }
#endif
    }
#endif
    c.add(_length, _pagesize, _addr);
    return true;  // cached
  }
  catch(...)
  {
    return false;
  }
}

map_handle::cache_statistics map_handle::trim_cache(std::chrono::steady_clock::time_point older_than) noexcept
{
  return detail::map_handle_cache().trim_cache(older_than);
}


LLFIO_V2_NAMESPACE_END
