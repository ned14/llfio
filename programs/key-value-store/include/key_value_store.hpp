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
#if __has_include("quickcpplib/include/algorithm/open_hash_index.hpp")
#include "quickcpplib/include/algorithm/open_hash_index.hpp"
#else
#include "../../../include/afio/v2.0/quickcpplib/include/algorithm/open_hash_index.hpp"
#endif

#include <vector>

namespace key_value_store
{
  namespace afio = AFIO_V2_NAMESPACE;
  template <class T> using optional = afio::optional<T>;
  template <class T> using span = afio::span<T>;
  using afio::undoer;
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
        : std::runtime_error("48 writers have now opened the store, either open read-only or reduce the number of writers")
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
  class index_full : std::runtime_error
  {
  public:
    index_full()
        : std::runtime_error("Failed to insert new key due to too many collisions, use a bigger index!")
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
    using open_hash_index = basic_open_hash_index<atomic_linear_memory_policy<key_type, value_history, 0>, AFIO_V2_NAMESPACE::algorithm::mapped_view>;
    static_assert(sizeof(open_hash_index::value_type) == 128, "open_hash_index::value_type is wrong size");

    struct index
    {
      uint64_t magic;                              // versionmagic, currently "AFIOKV01" for valid, "DEADKV01" for requires repair
      std::atomic<uint64_t> transaction_counter;   // top 16 bits are number of keys changed this transaction, bottom 48 bits are monotonic counter
      uint128 hash;                                // Optional hash of index file written on last close to guard against systems which don't write mmaps properly
      std::atomic<unsigned> writes_occurring[48];  // Incremented just before an update, decremented after, per writer
      std::atomic<bool> all_writes_synced;         // Set if all writers since the first which has opened this store did so with `O_SYNC` on (i.e. safe during fsck to check small file tails only)

      uint64_t contents_hashed : 1;       // If records written are hashed and checked on fetch
      uint64_t key_is_hash_of_value : 1;  // On read, check hash of value equals key
    };

    struct value_tail
    {
      uint128 hash;  // 128 bit hash of contents
      key_type key;
      uint64_t transaction_counter;  // transaction counter when this was updated
      uint64_t length;               // (uint64_t)-1 means key was deleted
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
    afio::file_handle _mysmallfile;  // append only
    afio::file_handle::extent_guard _indexfileguard, _smallfileguard;
    size_t _mysmallfileidx{(size_t) -1};
    struct
    {
      std::vector<afio::file_handle> read;
    } _smallfiles;
    optional<index::open_hash_index> _index;
    index::index *_indexheader{nullptr};
    std::mutex _commitlock;
    bool _use_mmaps_for_commit{false};

    static constexpr afio::file_handle::extent_type _indexinuseoffset = (afio::file_handle::extent_type) -1;
    static constexpr uint64_t _goodmagic = 0x3130564b4f494641;  // "AFIOKV01"
    static constexpr uint64_t _badmagic = 0x3130564b44414544;   // "DEADKV01"

    static size_t _pad_length(size_t length)
    {
      // We append a value_tail record and round up to 64 byte multiple
      return (length + sizeof(index::value_tail) + 63) & ~63;
    }
    void _openfiles(const afio::path_handle &dir, afio::file_handle::mode mode, afio::file_handle::caching caching)
    {
      const afio::file_handle::mode smallfilemode =
#ifdef _WIN32
      afio::file_handle::mode::read
#else
      // Linux won't allow taking an exclusive lock on a read only file
      mode
#endif
      ;
      if(_smallfiles.read.empty())
      {
        // Open the small files, choosing the first unclaimed small file as "mine"
        std::string name;
        _smallfiles.read.reserve(48);
        for(size_t n = 0; n < 48; n++)
        {
          name = std::to_string(n);
          auto fh = afio::file_handle::file(dir, name, smallfilemode, afio::file_handle::creation::open_existing, afio::file_handle::caching::all, afio::file_handle::flag::disable_prefetching);
          if(fh)
          {
          retry:
            bool claimed = false;
            if(mode == afio::file_handle::mode::write && !_mysmallfile.is_valid())
            {
              // Try to claim this small file
              auto smallfileclaimed = fh.value().try_lock(_indexinuseoffset, 1, true);
              if(smallfileclaimed)
              {
                _mysmallfile = afio::file_handle::file(dir, name, afio::file_handle::mode::write, afio::file_handle::creation::open_existing, caching).value();
                _mysmallfile.set_append_only(true).value();
                _smallfileguard = std::move(smallfileclaimed).value();
                _mysmallfileidx = n;
                _smallfiles.read.push_back(std::move(fh).value());
                _smallfileguard.set_handle(&_smallfiles.read.back());
                claimed = true;
              }
            }
            if(!claimed)
            {
              _smallfiles.read.push_back(std::move(fh).value());
            }
            continue;
          }
          else if(mode == afio::file_handle::mode::write && !_mysmallfile.is_valid())
          {
            // Going to need a new smallfile
            fh = afio::file_handle::file(dir, name, smallfilemode, afio::file_handle::creation::only_if_not_exist, caching);
            if(fh)
            {
              goto retry;
            }
            continue;
          }
          break;
        }
        if(mode == afio::file_handle::mode::write && !_mysmallfile.is_valid())
        {
          throw maximum_writers_reached();
        }
        // Set up the index, either r/w or read only with copy on write
        afio::section_handle::flag mapflags = (mode == afio::file_handle::mode::write) ? afio::section_handle::flag::readwrite : (afio::section_handle::flag::read | afio::section_handle::flag::cow);
        afio::section_handle sh = afio::section_handle::section(_indexfile, 0, mapflags).value();
        afio::file_handle::extent_type len = sh.length();
        len -= sizeof(index::index);
        len /= sizeof(index::open_hash_index::value_type);
        size_t offset = sizeof(index::index);
        _index.emplace(sh, len, offset, mapflags);
        _indexheader = reinterpret_cast<index::index *>((char *) _index->container().data() - offset);
        if(_indexheader->writes_occurring[_mysmallfileidx] != 0)
        {
          _indexheader->magic = _badmagic;
          throw corrupted_store();
        }
      }
    }

  public:
    basic_key_value_store(const basic_key_value_store &) = delete;
    basic_key_value_store(basic_key_value_store &&) = delete;
    basic_key_value_store &operator=(const basic_key_value_store &) = delete;
    basic_key_value_store &operator=(basic_key_value_store &&) = delete;

    basic_key_value_store(const afio::path_handle &dir, size_t hashtableentries, bool enable_integrity = false, afio::file_handle::mode mode = afio::file_handle::mode::write, afio::file_handle::caching caching = afio::file_handle::caching::all)
        : _indexfile(afio::file_handle::file(dir, "index", mode, (mode == afio::file_handle::mode::write) ? afio::file_handle::creation::if_needed : afio::file_handle::creation::open_existing, caching, afio::file_handle::flag::disable_prefetching).value())
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
            index::index i;
            memset(&i, 0, sizeof(i));
            i.magic = _goodmagic;
            i.all_writes_synced = _indexfile.are_writes_durable();
            i.contents_hashed = enable_integrity;
            _indexfile.write(0, (char *) &i, sizeof(i)).value();
          }
          else
          {
            /* TODO: Check consistency of index by checking that every item's transaction counter is within 2^47 of head's
            and that no item has a transaction counter later than head's.

            Check that every smallfile's tail points to a complete set of value records which matches the one in the history in the index
            or that that key's latest value exists and is valid.
            */
            //_openfiles(dir, writable);
            if(_indexheader->contents_hashed)
            {
            }
            // Now we've finished the checks, reset writes_occurring and all_writes_synced
            index::index i;
            _indexfile.read(0, (char *) &i, sizeof(i)).value();
            memset(i.writes_occurring, 0, sizeof(i.writes_occurring));
            i.all_writes_synced = _indexfile.are_writes_durable();
            memset(&i.hash, 0, sizeof(i.hash));
            _indexfile.write(0, (char *) &i, sizeof(i)).value();
          }
        }
      }
      // Take a shared lock, blocking if someone is still setting things up
      _indexfileguard = _indexfile.lock(_indexinuseoffset, 1, false).value();
      {
        char buffer[8];
        _indexfile.read(0, buffer, 8).value();
        auto goodmagic = _goodmagic;
        auto badmagic = _badmagic;
        if(!memcmp(buffer, &badmagic, 8))
          throw corrupted_store();
        if(memcmp(buffer, &goodmagic, 8))
          throw unknown_store();
      }
      // Open our smallfiles and map our index for shared usage
      _openfiles(dir, mode, caching);
      if(!_indexfile.are_writes_durable())
      {
        _indexheader->all_writes_synced = false;
      }
    }
    //! \overload
    basic_key_value_store(const afio::path_view &dir, size_t hashtableentries, bool enable_integrity = false, afio::file_handle::mode mode = afio::file_handle::mode::write, afio::file_handle::caching caching = afio::file_handle::caching::all)
        : basic_key_value_store(afio::directory_handle::directory({}, dir, afio::directory_handle::mode::write, afio::directory_handle::creation::if_needed).value(), hashtableentries, enable_integrity, mode, caching)
    {
    }
    //! Opens the store for read only access
    basic_key_value_store(const afio::path_view &dir)
        : basic_key_value_store(afio::path_handle::path(dir).value(), 0, false, afio::file_handle::mode::read)
    {
    }
    ~basic_key_value_store()
    {
      // Release my smallfile
      _smallfileguard.unlock();
      _mysmallfile.close().value();
      // Try to lock the index exclusively
      _indexfileguard.unlock();
      auto indexfileguard = _indexfile.try_lock(_indexinuseoffset, 1, true);
      if(indexfileguard)
      {
        // I am the last user
        if(_indexheader->contents_hashed)
        {
          _indexheader->hash = QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash((char *) _indexheader, _indexfile.length().value());
        }
      }
    }

    /*! \brief Sets whether to use mmaps for writing small objects during commit.

    Normally, this implementation atomic-appends small objects to the smallfile via gather write.
    This is simple, safe, and with reasonable performance most of the time.

    However in certain circumstances e.g. with synchronous i/o enabled, atomic-append forces lots of
    read-modify-write cycles on the storage device. This can become unusably slow if the values you
    write are small, or your OS doesn't implement gather writes for file i/o (e.g. Windows). For
    these circumstances, one can instead use a memory map of the end of the smallfile to append
    the small objects. This can cause the kernel to not flush the map to storage until the map is
    destroyed, but it also avoids the read-modify-write cycle with synchronous i/o.

    Be aware that memory mapping off the end of a file being modified has historically been
    full of quirks, race conditions and bugs in major OS kernels. Everything from data loss,
    data corruption, denial of service and root privilege exploits have been found from the
    unexpected interactions between memory maps and a moving end of file.
    */
    void use_mmaps_for_commit(bool v)
    {
      if(_use_mmaps_for_commit == v)
      {
        return;
      }
      if(v && !_use_mmaps_for_commit)
      {
        _mysmallfile.set_append_only(false).value();
      }
      else
      {
        _mysmallfile.set_append_only(true).value();
      }
      _use_mmaps_for_commit = v;
    }

    //! Retrieve when keys were last updated by setting the second to the latest transaction counter.
    //! Note that counter will be `(uint64_t)-1` for any unknown keys. Never throws exceptions.
    void last_updated(span<std::pair<key_type, uint64_t>> keys) noexcept
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
      friend class transaction;
      //! The key
      key_type key;
      //! The value
      span<const char> value;
      //! When this value was last modified
      uint64_t transaction_counter;

      keyvalue_info(keyvalue_info &&o) noexcept : key(std::move(o.key)), value(std::move(o.value)), transaction_counter(std::move(o.transaction_counter)), _value_buffer(std::move(o._value_buffer)), _value_view(std::move(o._value_view)) { o._value_buffer = nullptr; }
      keyvalue_info &operator=(keyvalue_info &&o) noexcept
      {
        this->~keyvalue_info();
        new(this) keyvalue_info(std::move(o));
        return *this;
      }
      ~keyvalue_info()
      {
        if(_value_buffer != nullptr)
        {
          free(_value_buffer);
        }
      }

      //! True if this info contains a valid value
      explicit operator bool() const noexcept { return transaction_counter != (uint64_t) -1; }

    private:
      keyvalue_info()
          : key(0)
          , transaction_counter((uint64_t) -1)
      {
      }
      keyvalue_info(key_type _key)
          : key(_key)
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
    //! Retrieve the latest value for a key. May throw `corrupted_store`
    keyvalue_info find(key_type key, size_t revision = 0)
    {
      if(_indexheader->magic != _goodmagic)
        throw corrupted_store();
      if(revision >= 4)
        throw std::invalid_argument("valid revision is 0-3");
      auto it = _index->find_shared(key);
      if(it == _index->end())
      {
        // No value as no key
        return keyvalue_info(key);
      }
      else
      {
        // TODO Depending on length, make a mapped_view instead
        const auto &item = it->second.history[revision];
        if(item.transaction_counter == 0)
        {
          // No value on the key at this revision
          return keyvalue_info(key);
        }
        size_t length = item.length, smallfilelength = _pad_length(length);
        char *buffer = (char *) malloc(smallfilelength);
        if(!buffer)
        {
          throw std::bad_alloc();
        }
        if(item.value_identifier >= _smallfiles.read.size())
        {
          // TODO: Open newly created smallfiles
          abort();
        }
        _smallfiles.read[item.value_identifier].read(item.value_offset * 64 - smallfilelength, buffer, smallfilelength).value();
        index::value_tail *vt = reinterpret_cast<index::value_tail *>(buffer + smallfilelength - sizeof(index::value_tail));
        if(_indexheader->contents_hashed || _indexheader->key_is_hash_of_value)
        {
          QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash hasher;
          uint128 tocheck = vt->hash;
          memset(&vt->hash, 0, sizeof(vt->hash));
          uint128 thishash = QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash(buffer, _indexheader->contents_hashed ? smallfilelength : length);
          if(tocheck != thishash)
          {
            _indexheader->magic = _badmagic;
            throw corrupted_store();
          }
        }
        if(vt->key != key)
        {
          _indexheader->magic = _badmagic;
          throw corrupted_store();
        }
        if(vt->length != length)
        {
          _indexheader->magic = _badmagic;
          throw corrupted_store();
        }
        if(vt->transaction_counter != item.transaction_counter)
        {
          _indexheader->magic = _badmagic;
          throw corrupted_store();
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
      bool remove;                               // true if to remove
      _item(basic_key_value_store::keyvalue_info &&_kvi)
          : kvi(std::move(_kvi))
          , remove(false)
      {
      }
    };
    std::vector<_item> _items;

  public:
    //! Start a new transaction
    explicit transaction(basic_key_value_store &parent)
        : _parent(&parent)
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
    //! Set what a value will be updated to on commit. Requires the key to have been previously fetched to establish a base revision.
    void update(key_type key, span<const char> towrite)
    {
      for(auto &i : _items)
      {
        if(i.kvi.key == key)
        {
          i.towrite = towrite;
          i.remove = false;
          return;
        }
      }
      throw bad_update();
    }
    //! Remove a key and its value on commit. Requires the key to have been previously fetched to establish a base revision.
    void remove(key_type key)
    {
      for(auto &i : _items)
      {
        if(i.kvi.key == key)
        {
          i.towrite = {};
          i.remove = true;
          return;
        }
      }
      throw bad_update();
    }
    //! Set what a value will be updated to on commit without establishing a base revision. Concurrent updates to this key may be lost!
    void update_unsafe(key_type key, span<const char> towrite)
    {
      for(auto &i : _items)
      {
        if(i.kvi.key == key)
        {
          i.towrite = towrite;
          i.remove = false;
          return;
        }
      }
      if(_items.size() == 65535)
      {
        throw transaction_limit_reached();
      }
      basic_key_value_store::keyvalue_info kvi(key);
      _items.push_back(std::move(kvi));
      _items.back().towrite = towrite;
    }
    //! Remove a key and its value on commit without establishing a base revision. Concurrent updates to this key may be lost!
    void remove_unsafe(key_type key)
    {
      for(auto &i : _items)
      {
        if(i.kvi.key == key)
        {
          i.towrite = {};
          i.remove = true;
          return;
        }
      }
      if(_items.size() == 65535)
      {
        throw transaction_limit_reached();
      }
      basic_key_value_store::keyvalue_info kvi(key);
      _items.push_back(std::move(kvi));
      _items.back().remove = true;
    }
    //! Commit the transaction, throwing `transaction_aborted` if a key's value was updated since it was fetched for this transaction.
    void commit()
    {
      if(_parent->_indexheader->magic != _parent->_goodmagic)
        throw corrupted_store();

      // Firstly remove any items fetched but not used as a base for an update, and sort the remaining
      // list of keys we are to update into order. This ensures that all writers always lock the keys
      // in the same order, thus preventing deadlock.
      _items.erase(std::remove_if(_items.begin(), _items.end(), [](const auto &item) { return !item.towrite.has_value() && !item.remove; }), _items.end());
      std::sort(_items.begin(), _items.end(), [](const _item &a, const _item &b) { return a.kvi.key < b.kvi.key; });

      // The update list, filled in as we progress
      struct toupdate_type
      {
        const key_type key;
        const uint64_t old_transaction_counter;
        const bool insertion, update, removal;
        index::value_history::item history_item{};
        index::open_hash_index::iterator it{};
        toupdate_type(key_type _key, uint64_t _old_transaction_counter, bool _insertion, bool _update, bool _removal)
            : key(_key)
            , old_transaction_counter(_old_transaction_counter)
            , insertion(_insertion)
            , update(_update)
            , removal(_removal)
        {
        }
      };
      std::vector<toupdate_type> toupdate;
      toupdate.reserve(_items.size());
      // Serialise multiple threads issuing commit using the same store
      std::lock_guard<decltype(_parent->_commitlock)> commitlockguard(_parent->_commitlock);

      // Take out shared locks on all the items in my commit with existing values, early checking if we will abort
      std::vector<index::open_hash_index::const_iterator> shared_locks;
      shared_locks.reserve(_items.size());
      for(const auto &item : _items)
      {
        bool insertion = false, update = false, removal = false;
        if(item.towrite.has_value() || item.remove)
        {
          auto it = _parent->_index->find_shared(item.kvi.key);
          if(it != _parent->_index->end())
          {
            // If item was fetched before update and it has since changed, abort
            if(item.kvi.transaction_counter != (uint64_t) -1 && it->second.history[0].transaction_counter != item.kvi.transaction_counter)
            {
              throw transaction_aborted(item.kvi.key);
            }
            shared_locks.push_back(std::move(it));
            removal = item.remove;
            update = !item.remove;
          }
          else
          {
            insertion = true;
          }
        }
        assert(insertion + update + removal == 1);
        toupdate.emplace_back(item.kvi.key, item.kvi.transaction_counter, insertion, update, removal);
      }
      // Atomically increment the transaction counter to set this latest transaction
      uint64_t this_transaction_counter = 0;
      {
        uint64_t old_transaction_counter;
        union {
          struct
          {
            uint64_t values_updated : 16;
            uint64_t counter : 48;
          };
          uint64_t this_transaction_counter;
        } _;
        do
        {
          _.this_transaction_counter = old_transaction_counter = _parent->_indexheader->transaction_counter.load(std::memory_order_acquire);
          // Increment bottom 48 bits, letting it wrap if necessary
          _.counter++;
          _.values_updated = _items.size();
        } while(!_parent->_indexheader->transaction_counter.compare_exchange_weak(old_transaction_counter, _.this_transaction_counter, std::memory_order_release, std::memory_order_relaxed));
        this_transaction_counter = _.this_transaction_counter;
      }

      bool items_written = false;
      if(_parent->_use_mmaps_for_commit)
      {
        afio::file_handle::extent_type original_length = _parent->_mysmallfile.length().value();
        // How big does this map need to be?
        size_t totalcommitsize = 0;
        for(size_t n = 0; n < _items.size(); n++)
        {
          toupdate_type &thisupdate = toupdate[n];
          const transaction::_item &item = _items[n];
          totalcommitsize += thisupdate.removal ? 64 : _parent->_pad_length(item.towrite->size());
        }
        if(totalcommitsize >= 4096)
        {
          {
#ifdef _WIN32
            afio::file_handle::extent_type mapbegin = original_length & ~65535;
#else
            afio::file_handle::extent_type mapbegin = afio::utils::round_down_to_page_size(original_length);
#endif
            afio::file_handle::extent_type mapend = afio::utils::round_up_to_page_size(original_length + totalcommitsize);
            _parent->_mysmallfile.truncate(mapend).value();
            auto sh = afio::section_handle::section(_parent->_mysmallfile, mapend).value();
            auto mh = afio::map_handle::map(sh, mapend - mapbegin, mapbegin).value();
            char *value = mh.address() + (original_length - mapbegin);
            afio::file_handle::extent_type value_offset = original_length;
            for(size_t n = 0; n < _items.size(); n++)
            {
              toupdate_type &thisupdate = toupdate[n];
              const transaction::_item &item = _items[n];
              size_t totalwrite = 0;
              if(thisupdate.removal)
              {
                totalwrite = 64;
              }
              else
              {
                memcpy(value, item.towrite->data(), item.towrite->size());
                totalwrite = _parent->_pad_length(item.towrite->size());
              }
              index::value_tail *vt = reinterpret_cast<index::value_tail *>(value + totalwrite - sizeof(index::value_tail));
              vt->key = thisupdate.key;
              vt->transaction_counter = this_transaction_counter;
              if(thisupdate.removal)
              {
                vt->length = (uint64_t) -1;  // this key is being deleted
                memset(&thisupdate.history_item, 0, sizeof(thisupdate.history_item));
              }
              else
              {
                vt->length = item.towrite->size();
                index::value_history::item &history_item = thisupdate.history_item;
                history_item.transaction_counter = this_transaction_counter;
                history_item.value_offset = (value_offset + totalwrite) / 64;
                history_item.value_identifier = _parent->_mysmallfileidx;
                history_item.length = vt->length;
              }
              if(_parent->_indexheader->contents_hashed)
              {
                vt->hash = QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash(value, totalwrite);
              }
              value += totalwrite;
              value_offset += totalwrite;
            }
          }
          // Place maximum extent back where it is supposed to be
          _parent->_mysmallfile.truncate(original_length + totalcommitsize).value();
          items_written = true;
        }
      }
      if(!items_written)
      {
        // Gather append write all my items to my smallfile
        afio::file_handle::extent_type value_offset = _parent->_mysmallfile.length().value();
        assert((value_offset % 64) == 0);
        // POSIX guarantees that at least 16 gather buffers can be written in a single shot
        std::vector<afio::file_handle::const_buffer_type> reqs;
        reqs.reserve(16);
        // With tails, that's eight items per syscall
        char tailbuffers[8][128];
        memset(tailbuffers, 0, sizeof(tailbuffers));
        for(size_t n = 0; n < _items.size(); n++)
        {
          char *tailbuffer = tailbuffers[n % 8];
          index::value_tail *vt = reinterpret_cast<index::value_tail *>(tailbuffer + 128 - sizeof(index::value_tail));
          toupdate_type &thisupdate = toupdate[n];
          const transaction::_item &item = _items[n];
          vt->key = thisupdate.key;
          vt->transaction_counter = this_transaction_counter;
          size_t totalwrite = 0;
          if(thisupdate.removal)
          {
            vt->length = (uint64_t) -1;  // this key is being deleted
            totalwrite = 64;
            reqs.push_back({tailbuffer + 64, 64});
            if(_parent->_indexheader->contents_hashed)
            {
              QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash hasher;
              memset(&vt->hash, 0, sizeof(vt->hash));
              hasher.add(reqs.back().data, reqs.back().len);
              vt->hash = hasher.finalise();
            }
            memset(&thisupdate.history_item, 0, sizeof(thisupdate.history_item));
          }
          else
          {
            vt->length = item.towrite->size();
            totalwrite = _parent->_pad_length(item.towrite->size());
            size_t tailbytes = totalwrite - item.towrite->size();
            assert(tailbytes < 128);
            reqs.push_back({item.towrite->data(), item.towrite->size()});
            reqs.push_back({tailbuffer + 128 - tailbytes, tailbytes});
            if(_parent->_indexheader->contents_hashed)
            {
              QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash hasher;
              memset(&vt->hash, 0, sizeof(vt->hash));
              auto rit = reqs.end();
              rit -= 2;
              hasher.add(rit->data, rit->len);
              ++rit;
              hasher.add(rit->data, rit->len);
              vt->hash = hasher.finalise();
            }
            index::value_history::item &history_item = thisupdate.history_item;
            history_item.transaction_counter = this_transaction_counter;
            history_item.value_offset = (value_offset + totalwrite) / 64;
            history_item.value_identifier = _parent->_mysmallfileidx;
            history_item.length = vt->length;
          }
          value_offset += totalwrite;
          if((n % 8) == 7)
          {
            _parent->_mysmallfile.write({reqs, 0}).value();
            reqs.clear();
          }
        }
        if(!reqs.empty())
        {
          _parent->_mysmallfile.write({reqs, 0}).value();
        }
      }

      // Release all the shared locks on the existing items we are about to update
      shared_locks.clear();
      // Bail out if store has become corrupted
      if(_parent->_indexheader->magic != _parent->_goodmagic)
        throw corrupted_store();
      // Remove any newly inserted keys if we abort
      auto removeinserted = undoer([this, &toupdate] {
        for(auto updit = toupdate.rend(); updit != toupdate.rbegin(); ++updit)
        {
          if(updit->insertion && updit->it != _parent->_index->end())
          {
            _parent->_index->erase(std::move(updit->it));
          }
        }
      });
      // Take exclusive locks on all items in this transaction, inserting new keys if necessary
      for(toupdate_type &item : toupdate)
      {
        auto it = _parent->_index->find_exclusive(item.key);
        if(it != _parent->_index->end())
        {
          if(item.insertion || (item.update && it->second.history[0].transaction_counter != item.old_transaction_counter))
          {
            // Item has changed since transaction begun
            throw transaction_aborted(item.key);
          }
        }
        else
        {
          if(item.update || item.removal)
          {
            // Item has changed since transaction begun
            throw transaction_aborted(item.key);
          }
          // Insert a new key with empty history
          index::value_history vh;
          memset(&vh, 0, sizeof(vh));
          it = _parent->_index->insert({item.key, std::move(vh)}).first;
          if(it == _parent->_index->end())
          {
            throw index_full();
          }
        }
        // Store the exclusive lock away for later
        item.it = std::move(it);
      }

      if(_parent->_indexheader->magic != _parent->_goodmagic)
        throw corrupted_store();
      // Finally actually perform the update as quickly as possible to reduce the
      // possibility of a partially issued update which is expensive to repair.
      // This can no longer abort, so dismiss the removeinserter
      removeinserted.dismiss();
      _parent->_indexheader->writes_occurring[_parent->_mysmallfileidx].fetch_add(1);
      for(auto &item : toupdate)
      {
        // Update existing value's latest revision
        index::value_history &value = item.it->second;
        memmove(value.history + 1, value.history, sizeof(value.history) - sizeof(value.history[0]));
        value.history[0] = item.history_item;
        if(item.removal)
        {
          bool alldeleted = true;
          for(const auto &h : value.history)
          {
            if(h.transaction_counter != 0)
            {
              alldeleted = false;
              break;
            }
          }
          if(alldeleted)
          {
            _parent->_index->erase(std::move(item.it));
          }
        }
      }
      _parent->_indexheader->writes_occurring[_parent->_mysmallfileidx].fetch_sub(1);
    }
  };
}

#endif
