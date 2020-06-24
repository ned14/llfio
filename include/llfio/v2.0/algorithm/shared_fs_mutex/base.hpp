/* Protect a shared filing system resource from concurrent modification
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
File Created: March 2016


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

#ifndef LLFIO_SHARED_FS_MUTEX_BASE_HPP
#define LLFIO_SHARED_FS_MUTEX_BASE_HPP

#include "../../handle.hpp"

#include "quickcpplib/algorithm/hash.hpp"


//! \file base.hpp Provides algorithm::shared_fs_mutex::shared_fs_mutex

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  //! Algorithms for protecting a shared filing system resource from racy modification
  namespace shared_fs_mutex
  {
    //! Unsigned 64 bit integer
    using uint64 = unsigned long long;
    //! Unsigned 128 bit integer
    using uint128 = QUICKCPPLIB_NAMESPACE::integers128::uint128;

    /*! \class shared_fs_mutex
    \brief Abstract base class for an object which protects shared filing system resources

    The implementations of this abstract base class have various pros and cons with varying
    time and space complexities. See their documentation for details. All share the concept
    of "entity_type" as being a unique 63 bit identifier of a lockable entity. Various
    conversion functions are provided below for converting strings, buffers etc. into an
    entity_type.
    */
    class shared_fs_mutex
    {
    public:
      //! The type of an entity id
      struct entity_type
      {
        //! The type backing the value
        using value_type = handle::extent_type;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)  // nameless union used
#endif
        union {
          value_type _init;
          struct
          {
            //! The value of the entity type which can range between 0 and (2^63)-1
            value_type value : 63;
            //! True if entity should be locked for exclusive access
            value_type exclusive : 1;
          };
        };
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        //! Default constructor
        constexpr entity_type() noexcept
            : _init(0)
        {
        }  // NOLINT
//! Constructor
#if !defined(__GNUC__) || defined(__clang__) || __GNUC__ >= 7
        constexpr
#endif
        entity_type(value_type _value, bool _exclusive) noexcept
            : _init(0)
        {
          value = _value;
          exclusive = _exclusive;  // NOLINT
        }
      };
#if __cplusplus < 201700 && !_HAS_CXX17
      static_assert(std::is_literal_type<entity_type>::value, "entity_type is not a literal type");
#endif
      static_assert(sizeof(entity_type) == sizeof(entity_type::value_type), "entity_type is bit equal to its underlying type");
      //! The type of a sequence of entities
      using entities_type = span<entity_type>;

    protected:
      constexpr shared_fs_mutex() {}  // NOLINT
      shared_fs_mutex(const shared_fs_mutex &) = default;
      shared_fs_mutex(shared_fs_mutex &&) = default;
      shared_fs_mutex &operator=(const shared_fs_mutex &) = default;
      shared_fs_mutex &operator=(shared_fs_mutex &&) = default;

    public:
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~shared_fs_mutex() = default;

      //! Generates an entity id from a sequence of bytes
      entity_type entity_from_buffer(const char *buffer, size_t bytes, bool exclusive = true) noexcept
      {
        uint128 hash = QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash(buffer, bytes);
        return {hash.as_longlongs[0] ^ hash.as_longlongs[1], exclusive};
      }
      //! Generates an entity id from a string
      template <typename T> entity_type entity_from_string(const std::basic_string<T> &str, bool exclusive = true) noexcept
      {
        uint128 hash = QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash(str);
        return {hash.as_longlongs[0] ^ hash.as_longlongs[1], exclusive};
      }
      //! Generates a cryptographically random entity id.
      entity_type random_entity(bool exclusive = true) noexcept
      {
        entity_type::value_type v;
        utils::random_fill(reinterpret_cast<char *>(&v), sizeof(v));
        return {v, exclusive};
      }
      //! Fills a sequence of entity ids with cryptographic randomness. Much faster than calling random_entity() individually.
      void fill_random_entities(span<entity_type> seq, bool exclusive = true) noexcept
      {
        utils::random_fill(reinterpret_cast<char *>(seq.data()), seq.size() * sizeof(entity_type));
        for(auto &i : seq)
        {
          i.exclusive = exclusive;  // NOLINT
        }
      }

      //! RAII holder for a lock on a sequence of entities
      class entities_guard
      {
        entity_type _entity;

      public:
        shared_fs_mutex *parent{nullptr};
        entities_type entities;
        unsigned long long hint{0};
        entities_guard() = default;
        entities_guard(shared_fs_mutex *_parent, entities_type _entities)
            : parent(_parent)
            , entities(_entities)
            , hint(0)
        {
        }
        entities_guard(shared_fs_mutex *_parent, entity_type entity)
            : _entity(entity)
            , parent(_parent)
            , entities(&_entity, 1)
            , hint(0)
        {
        }
        entities_guard(const entities_guard &) = delete;
        entities_guard &operator=(const entities_guard &) = delete;
        entities_guard(entities_guard &&o) noexcept
            : _entity(o._entity)
            , parent(o.parent)
            , entities(o.entities)
            , hint(o.hint)
        {
          if(entities.data() == &o._entity)
          {
            entities = entities_type(&_entity, 1);
          }
          o.release();
        }
        entities_guard &operator=(entities_guard &&o) noexcept
        {
          if(this == &o)
          {
            return *this;
          }
          this->~entities_guard();
          new(this) entities_guard(std::move(o));
          return *this;
        }
        ~entities_guard()
        {
          if(parent != nullptr)
          {
            unlock();
          }
        }
        //! True if extent guard is valid
        explicit operator bool() const noexcept { return parent != nullptr; }
        //! True if extent guard is invalid
        bool operator!() const noexcept { return parent == nullptr; }
        //! Unlocks the locked entities immediately
        void unlock() noexcept
        {
          if(parent != nullptr)
          {
            parent->unlock(entities, hint);
            release();
          }
        }
        //! Detach this RAII unlocker from the locked state
        void release() noexcept
        {
          parent = nullptr;
          entities = entities_type();
        }
      };

      virtual result<void> _lock(entities_guard &out, deadline d, bool spin_not_sleep) noexcept = 0;

      //! Lock all of a sequence of entities for exclusive or shared access
      result<entities_guard> lock(entities_type entities, deadline d = deadline(), bool spin_not_sleep = false) noexcept
      {
        entities_guard ret(this, entities);
        OUTCOME_TRYV(_lock(ret, d, spin_not_sleep));
        return {std::move(ret)};
      }
      //! Lock a single entity for exclusive or shared access
      result<entities_guard> lock(entity_type entity, deadline d = deadline(), bool spin_not_sleep = false) noexcept
      {
        entities_guard ret(this, entity);
        OUTCOME_TRYV(_lock(ret, d, spin_not_sleep));
        return {std::move(ret)};
      }
      //! Try to lock all of a sequence of entities for exclusive or shared access
      result<entities_guard> try_lock(entities_type entities) noexcept { return lock(entities, deadline(std::chrono::seconds(0))); }
      //! Try to lock a single entity for exclusive or shared access
      result<entities_guard> try_lock(entity_type entity) noexcept { return lock(entity, deadline(std::chrono::seconds(0))); }
      //! Unlock a previously locked sequence of entities
      virtual void unlock(entities_type entities, unsigned long long hint = 0) noexcept = 0;
    };

  }  // namespace shared_fs_mutex
}  // namespace algorithm

LLFIO_V2_NAMESPACE_END


#endif
