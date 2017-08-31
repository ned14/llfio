/* Prototype key-value store
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (3 commits)
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

#ifndef KEY_VALUE_STORE_HPP
#define KEY_VALUE_STORE_HPP

#include "../../../include/afio/afio.hpp"
#include "quickcpplib/include/algorithm/open_hash_index.hpp"
#include "quickcpplib/include/uint128.hpp"

#include <vector>

namespace key_value_store
{
  namespace afio = AFIO_V2_NAMESPACE;
  template <class T> using optional = afio::optional<T>;
  template <class T> using span = afio::span<T>;
  using uint128 = QUICKCPPLIB_NAMESPACE::integers128::uint128;
  using key_type = uint128;

  class corrupted_store : std::runtime_error
  {
  public:
    corrupted_store()
        : std::runtime_error("The store is corrupted, please back it up and then run rescue on it")
    {
    }
  };
  class unknown_store : std::runtime_error
  {
  public:
    unknown_store()
        : std::runtime_error("The store has an unknown version, please use a newer version of this library")
    {
    }
  };
  class maximum_writers_reached : std::runtime_error
  {
  public:
    maximum_writers_reached()
        : std::runtime_error("64 writers have now opened the store, either open read-only or reduce the number of writers")
    {
    }
  };
  class transaction_limit_reached : std::runtime_error
  {
  public:
    transaction_limit_reached()
        : std::runtime_error("You may only update 65,535 items per transaction")
    {
    }
  };
  class bad_update : std::runtime_error
  {
  public:
    bad_update()
        : std::runtime_error("You cannot update a key in a transaction without fetching it first")
    {
    }
  };
  class transaction_aborted : std::runtime_error
  {
    key_type _key;

  public:
    transaction_aborted(key_type key)
        : std::runtime_error("A key was modified since it was fetched by this transaction")
        , _key(key)
    {
    }
    //! The key which caused the transaction to abort
    key_type key() const { return _key; }
  };

  namespace index
  {
    using namespace QUICKCPPLIB_NAMESPACE::algorithm::open_hash_index;
    struct value_history
    {
      // Most recent six versions of this value
      struct item
      {
        uint64_t transaction_counter;   // transaction counter when this was updated
        uint64_t value_offset : 58;     // Shifted left 6 as tail of blob record (value_tail) will always be on 64 byte boundary
        uint64_t value_identifier : 6;  // 0-47 is smallfile identifier, 48-63 is reserved for future usage
        uint64_t length;                // Length in bytes
      } history[4];
    };
    static_assert(sizeof(value_history) == 96, "value_history is wrong size");
    /* atomic_linear_memory_policy layout: Total 128 bytes
       - atomic<uint32_t> lock    4 bytes
       - atomic<uint32_t> inuse   4 bytes
       - padding for uint128      8 bytes
       - uint128 key             16 bytes
       - value_history          104 bytes
    */
    using open_hash_index = basic_open_hash_index<atomic_linear_memory_policy<key_type, value_history, 1>, AFIO_V2_NAMESPACE::algorithm::mapped_view>;
    static_assert(sizeof(open_hash_index::value_type) == 128, "open_hash_index::value_type is wrong size");

    struct index
    {
      uint64_t magic;                             // versionmagic, currently "AFIOKV01" for valid, "DEADKV01" for requires repair
      std::atomic<uint64_t> transaction_counter;  // top 16 bits are number of keys changed this transaction, bottom 48 bits are monotonic counter
      std::atomic<bool> write_interrupted;        // Set just before an update, cleared after
      std::atomic<bool> all_writes_synced;        // Set if all writers since the first which has opened this store did so with `O_SYNC` on (i.e. safe during fsck to check small file tails only)
    };

    struct value_tail
    {
      uint128 hash;  // 128 bit hash of contents
      key_type key;
      uint64_t transaction_counter;  // transaction counter when this was updated
      uint64_t length;
    };
    static_assert(sizeof(value_tail) == 48, "value_tail is wrong size");
  }

  class transaction;

  /*! A transactional key-value store.
  */
  class basic_key_value_store
  {
    friend class transaction;
    afio::file_handle _indexfile;
    afio::file_handle::extent_guard _indexfileguard, _smallfileguard;
    afio::file_handle _mysmallfile;  // append only
    size_t _mysmallfileidx{(size_t) -1};
    struct
    {
      std::vector<afio::file_handle> read;
    } _smallfiles;
    optional<index::open_hash_index> _index;
    index::index *_indexheader{nullptr};

    static constexpr afio::file_handle::extent_type _indexinuseoffset = (afio::file_handle::extent_type) -1;
    static constexpr const char *goodmagic = "AFIOKV01";
    static constexpr const char *badmagic = "DEADKV01";

    static size_t _pad_length(size_t length)
    {
      // We append a value_tail record and round up to 64 byte multiple
      return (length + sizeof(index::value_tail) + 63) & ~63;
    }
    void _openfiles(const afio::path_handle &dir, afio::file_handle::mode mode, afio::file_handle::caching caching)
    {
      if(_smallfiles.read.empty())
      {
        // Open the small files, choosing the first unclaimed small file as "mine"
        std::string name;
        for(size_t n = 0; n < 64; n++)
        {
          name = std::to_string(n);
          auto fh = afio::file_handle::file(dir, name, afio::file_handle::mode::read);
          if(fh)
          {
          retry:
            if(mode == afio::file_handle::mode::write && !_mysmallfile.is_valid())
            {
              // Try to claim this small file
              auto smallfileclaimed = fh.value().try_lock(_indexinuseoffset, 1, true);
              if(smallfileclaimed)
              {
                _mysmallfile = afio::file_handle::file(dir, name, afio::file_handle::mode::append, afio::file_handle::creation::open_existing, caching).value();
                _smallfileguard = std::move(smallfileclaimed).value();
                _mysmallfileidx = n;
              }
            }
            _smallfiles.read.push_back(std::move(fh).value());
          }
          else if(mode == afio::file_handle::mode::write && !_mysmallfile.is_valid())
          {
            // Going to need a new smallfile
            fh = afio::file_handle::file(dir, name, afio::file_handle::mode::read, afio::file_handle::creation::only_if_not_exist, caching);
            if(fh)
            {
              goto retry;
            }
          }
          else
          {
            break;
          }
        }
        if(mode == afio::file_handle::mode::write && !_mysmallfile.is_valid())
        {
          throw maximum_writers_reached();
        }
        // Set up the index
        afio::section_handle sh = afio::section_handle::section(_indexfile).value();
        afio::file_handle::extent_type len = sh.length();
        len -= sizeof(index::index);
        len /= sizeof(index::open_hash_index::value_type);
        size_t offset = sizeof(index::index);
        _index.emplace(sh, len, offset);
        _indexheader = reinterpret_cast<index::index *>(_index->container().data() - offset);
      }
    }

  public:
    basic_key_value_store(const afio::path_handle &dir, size_t hashtableentries, afio::file_handle::mode mode = afio::file_handle::mode::write, afio::file_handle::caching caching = afio::file_handle::caching::temporary)
        : _indexfile(afio::file_handle::file(dir, "index", mode, (mode == afio::file_handle::mode::write) ? afio::file_handle::creation::if_needed : afio::file_handle::creation::open_existing, caching).value())
    {
      if(mode == afio::file_handle::mode::write)
      {
        // Try an exclusive lock on inuse byte of the index file
        auto indexinuse = _indexfile.try_lock(_indexinuseoffset, 1, true);
        if(indexinuse.has_value())
        {
          // I am the first entrant into this data store
          if(_indexfile.length().value() == 0)
          {
            afio::file_handle::extent_type size = sizeof(index::index) + (hashtableentries) * sizeof(index::open_hash_index::value_type);
            size = afio::utils::round_up_to_page_size(size);
            _indexfile.truncate(size).value();
            _indexfile.write(0, goodmagic, 8).value();
          }
          else
          {
            /* TODO: Check consistency of index by checking that every item's transaction counter is within 2^47 of head's
            and that no item has a transaction counter later than head's.

            Check that every smallfile's tail points to a value record which matches one in the history in the index
            or that that key's latest value exists and is valid.
            */
            //_openfiles(dir, writable);
          }
          // Reset write_interrupted and all_writes_synced
          index::index i;
          _indexfile.read(0, (char *) &i, sizeof(i)).value();
          i.write_interrupted = false;
          i.all_writes_synced = _indexfile.are_writes_durable();
          _indexfile.write(0, (char *) &i, sizeof(i)).value();
        }
      }
      // Take a shared lock, blocking if someone is still setting things up
      _indexfileguard = _indexfile.lock(_indexinuseoffset, 1, false).value();
      {
        char buffer[8];
        _indexfile.read(0, buffer, 8).value();
        if(!strncmp(buffer, badmagic, 8))
          throw corrupted_store();
        if(strncmp(buffer, goodmagic, 8))
          throw unknown_store();
      }
      // Open our smallfiles and map our index for shared usage
      _openfiles(dir, mode, caching);
      if(mode == afio::file_handle::mode::write)
      {
        if(!_indexfile.are_writes_durable())
        {
          _indexheader->all_writes_synced = false;
        }
      }
    }
    //! \overload
    basic_key_value_store(const afio::path_view &dir, size_t hashtableentries, afio::file_handle::mode mode = afio::file_handle::mode::write, afio::file_handle::caching caching = afio::file_handle::caching::temporary)
        : basic_key_value_store(afio::path_handle::path(dir).value(), hashtableentries, mode, caching)
    {
    }
    //! Opens the store for read only access
    basic_key_value_store(const afio::path_view &dir)
        : basic_key_value_store(afio::path_handle::path(dir).value(), 0, afio::file_handle::mode::read)
    {
    }

    //! Retrieve when keys were last updated by setting the second to the latest transaction counter.
    //! Note that counter will be `(uint64_t)-1` for any unknown keys
    void last_updated(span<std::pair<key_type, uint64_t>> keys)
    {
      for(auto &key : keys)
      {
        auto it = _index->find_shared(key.first);
        if(it == _index->end())
        {
          key.second = (uint64_t) -1;
        }
        else
        {
          key.second = it->second.history[0].transaction_counter;
        }
      }
    }
    //! Information about a key value
    struct keyvalue_info
    {
      friend class basic_key_value_store;
      //! The key
      key_type key;
      //! The value
      span<const char> value;
      //! When this value was last modified
      uint64_t transaction_counter;

      keyvalue_info(keyvalue_info &&) = default;
      keyvalue_info &operator=(keyvalue_info &&) = default;
      ~keyvalue_info()
      {
        if(_value_buffer != nullptr)
        {
          free(_value_buffer);
        }
      }

    private:
      keyvalue_info()
          : key(0)
          , transaction_counter((uint64_t) -1)
      {
      }
      keyvalue_info(key_type _key, span<char> buffer, uint64_t tc)
          : key(_key)
          , value(buffer)
          , transaction_counter(tc)
          , _value_buffer(buffer.data())
      {
      }
      char *_value_buffer{nullptr};
      afio::algorithm::mapped_view<const char> _value_view;
    };
    //! Retrieve the latest value for a key
    keyvalue_info find(key_type key, size_t revision = 0)
    {
      auto it = _index->find_shared(key);
      if(it == _index->end())
      {
        return {};
      }
      else
      {
        // TODO Depending on length, make a mapped_view instead
        const auto &item = it->second.history[revision];
        size_t length = item.length, smallfilelength = _pad_length(length);
        char *buffer = (char *) malloc(length);
        if(!buffer)
        {
          throw std::bad_alloc();
        }
        if(item.value_identifier >= _smallfiles.read.size())
        {
          // FIXME: Open newly created smallfiles
          abort();
        }
        _smallfiles.read[item.value_identifier].read(item.value_offset * 64 - smallfilelength, buffer, smallfilelength).value();
        index::value_tail *vt = reinterpret_cast<index::value_tail *>(buffer + smallfilelength - sizeof(index::value_tail));
        // TODO: Check hash equals contents if hashing enabled
        if(vt->key != key)
        {
          abort();
        }
        if(vt->length != length)
        {
          abort();
        }
        if(vt->transaction_counter != item.transaction_counter)
        {
          abort();
        }
        return keyvalue_info(key, span<char>(buffer, length), item.transaction_counter);
      }
    }
  };

  /*! A transaction object.
  */
  class transaction
  {
    friend class basic_key_value_store;
    basic_key_value_store *_parent;
    struct _item
    {
      basic_key_value_store::keyvalue_info kvi;  // the item's value when fetched
      afio::optional<span<const char>> towrite;  // the value to be written on commit
      _item(basic_key_value_store::keyvalue_info &&_kvi)
          : kvi(std::move(_kvi))
      {
      }
    };
    std::vector<_item> _items;

  public:
    //! Start a new transaction
    explicit transaction(basic_key_value_store *parent)
        : _parent(parent)
    {
    }
    transaction(const transaction &) = delete;
    transaction &operator=(const transaction &) = delete;

    //! Fetch a value
    span<const char> fetch(key_type key)
    {
      // Did I fetch it already
      for(auto &i : _items)
      {
        if(i.kvi.key == key)
        {
          return i.kvi.value;
        }
      }
      // Fetch it now so
      if(_items.size() == 65535)
      {
        throw transaction_limit_reached();
      }
      _items.push_back(_parent->find(key));
      return _items.back().kvi.value;
    }
    //! Set what a value will be updated to on commit
    void update(key_type key, span<const char> towrite)
    {
      for(auto &i : _items)
      {
        if(i.kvi.key == key)
        {
          i.towrite = towrite;
          return;
        }
      }
      throw bad_update();
    }
    //! Commit the transaction, throwing `transaction_aborted` if a key's value was updated since it was fetched for this transaction.
    void commit()
    {
      // Firstly sort the list of keys we are to update into order. This ensures that all
      // writers always lock the keys in the same order, thus preventing deadlock.
      std::sort(_items.begin(), _items.end(), [](const _item &a, const _item &b) { return a.kvi.key < b.kvi.key; });

      // Take out shared locks on all the items in my commit, early checking if we will abort
      std::vector<index::open_hash_index::const_iterator> its;
      its.reserve(_items.size());
      for(const auto &item : _items)
      {
        if(item.towrite.has_value())
        {
          its.push_back(_parent->_index->find_shared(item.kvi.key));
          if(its.back() != _parent->_index->end())
          {
            if(its.back()->second.history[0].transaction_counter != item.kvi.transaction_counter)
            {
              throw transaction_aborted(item.kvi.key);
            }
          }
        }
      }
      // Atomically increment the transaction counter to set this latest transaction
      uint64_t old_transaction_counter;
      union {
        struct
        {
          uint64_t values_updated : 16;
          uint64_t counter : 48;
        };
        uint64_t this_transaction_counter;
      };
      do
      {
        this_transaction_counter = old_transaction_counter = _parent->_indexheader->transaction_counter.load(std::memory_order_acquire);
        // Increment bottom 48 bits, letting it wrap if necessary
        counter++;
        values_updated = _items.size();
      } while(!_parent->_indexheader->transaction_counter.compare_exchange_weak(old_transaction_counter, this_transaction_counter, std::memory_order_release, std::memory_order_relaxed));

      // Gather append write all my items to my smallfile
      char buffer[128];
      memset(buffer, 0, sizeof(buffer));
      index::value_tail *vt = reinterpret_cast<index::value_tail *>(buffer + sizeof(buffer) - sizeof(index::value_tail));
      afio::file_handle::extent_type value_offset = _parent->_mysmallfile.length().value();
      using toupdate_type = std::tuple<key_type, uint64_t, index::value_history::item, index::open_hash_index::iterator>;
      std::vector<toupdate_type> toupdate;
      toupdate.reserve(its.size());
      for(const auto &item : _items)
      {
        if(item.towrite.has_value())
        {
          vt->key = item.kvi.key;
          vt->length = item.towrite->size();
          // TODO: Hash contents
          size_t totalwrite = _parent->_pad_length(vt->length);
          size_t tailbytes = totalwrite - vt->length;
          assert(tailbytes < sizeof(buffer));
          afio::file_handle::const_buffer_type reqs[] = {{item.towrite->data(), item.towrite->size()}, {buffer + sizeof(buffer) - tailbytes, tailbytes}};
          _parent->_mysmallfile.write({reqs, 0}).value();
          index::value_history::item history_item;
          history_item.transaction_counter = this_transaction_counter;
          history_item.value_offset = value_offset / 64;
          history_item.value_identifier = _parent->_mysmallfileidx;
          history_item.length = vt->length;
          toupdate.emplace_back(vt->key, item.kvi.transaction_counter, history_item, index::open_hash_index::iterator{});
          value_offset += totalwrite;
        }
      }

      // Release all the shared locks, and take exclusive locks
      its.clear();
      for(auto &item : toupdate)
      {
        auto it = _parent->_index->find_exclusive(std::get<0>(item));
        if(it != _parent->_index->end())
        {
          if(it->second.history[0].transaction_counter != std::get<1>(item))
          {
            throw transaction_aborted(std::get<0>(item));
          }
        }
        std::get<3>(item) = std::move(it);
      }

      // Update all the values we are updating this transaction
      for(auto &item : toupdate)
      {
        auto &indexitem = std::get<3>(item)->second;
        memmove(indexitem.history + 1, indexitem.history, sizeof(indexitem.history) / sizeof(indexitem.history[0]) - 1);
        indexitem.history[0] = std::get<2>(item);
      }
    }
  };
}

#endif
