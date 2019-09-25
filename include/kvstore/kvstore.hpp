/* Standard key-value store for C++
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (24 commits)
File Created: Nov 2018


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

#ifndef KVSTORE_HPP
#define KVSTORE_HPP

#include "../llfio/v2.0/file_handle.hpp"

#include "quickcpplib/memory_resource.hpp"

//! \file kvstore.hpp Provides the abstract interface for a key-value store.

#if defined(LLFIO_UNSTABLE_VERSION) && !defined(LLFIO_DISABLE_ABI_PERMUTATION)
#include "../revision.hpp"
#define KVSTORE_V1 (QUICKCPPLIB_BIND_NAMESPACE_VERSION(kvstore_v1, LLFIO_PREVIOUS_COMMIT_UNIQUE))
#else
#define KVSTORE_V1 (QUICKCPPLIB_BIND_NAMESPACE_VERSION(kvstore_v1))
#endif
/*! \def KVSTORE_V1
\ingroup config
\brief The namespace configuration of this kv store v1. Consists of a sequence
of bracketed tokens later fused by the preprocessor into namespace and C++ module names.
*/
#if DOXYGEN_IS_IN_THE_HOUSE
//! The kv store namespace
namespace kvstore_v1_xxx
{
}
/*! \brief The namespace of this kv store v1 which will be some unknown inline
namespace starting with `v1_` inside the `kvstore` namespace.
\ingroup config
*/
#define KVSTORE_V1_NAMESPACE kvstore_v1_xxx
/*! \brief Expands into the appropriate namespace markup to enter the kv store v1 namespace.
\ingroup config
*/
#define KVSTORE_V1_NAMESPACE_BEGIN                                                                                                                                                                                                                                                                                             \
  namespace kvstore_v1_xxx                                                                                                                                                                                                                                                                                                     \
  {
/*! \brief Expands into the appropriate namespace markup to enter the C++ module
exported kv store v1 namespace.
\ingroup config
*/
#define KVSTORE_V1_NAMESPACE_EXPORT_BEGIN                                                                                                                                                                                                                                                                                      \
  export namespace kvstore_v1_xxx                                                                                                                                                                                                                                                                                              \
  {
/*! \brief Expands into the appropriate namespace markup to exit the kv store v1 namespace.
\ingroup config
*/
#define KVSTORE_V1_NAMESPACE_END }
#elif defined(GENERATING_LLFIO_MODULE_INTERFACE)
#define KVSTORE_V1_NAMESPACE QUICKCPPLIB_BIND_NAMESPACE(KVSTORE_V1)
#define KVSTORE_V1_NAMESPACE_BEGIN QUICKCPPLIB_BIND_NAMESPACE_BEGIN(KVSTORE_V1)
#define KVSTORE_V1_NAMESPACE_EXPORT_BEGIN QUICKCPPLIB_BIND_NAMESPACE_EXPORT_BEGIN(KVSTORE_V1)
#define KVSTORE_V1_NAMESPACE_END QUICKCPPLIB_BIND_NAMESPACE_END(KVSTORE_V1)
#else
#define KVSTORE_V1_NAMESPACE QUICKCPPLIB_BIND_NAMESPACE(KVSTORE_V1)
#define KVSTORE_V1_NAMESPACE_BEGIN QUICKCPPLIB_BIND_NAMESPACE_BEGIN(KVSTORE_V1)
#define KVSTORE_V1_NAMESPACE_EXPORT_BEGIN QUICKCPPLIB_BIND_NAMESPACE_BEGIN(KVSTORE_V1)
#define KVSTORE_V1_NAMESPACE_END QUICKCPPLIB_BIND_NAMESPACE_END(KVSTORE_V1)
#endif

KVSTORE_V1_NAMESPACE_BEGIN

namespace llfio = LLFIO_V2_NAMESPACE;
template <class T> using span = llfio::span<T>;
using llfio::byte;
template <class T> using result = llfio::result<T>;

namespace detail
{
  template <class T> struct extract_span_type
  {
    using type = void;
  };
  template <class T> struct extract_span_type<span<T>>
  {
    using type = T;
  };
}

//! Traits
namespace traits
{
  namespace detail
  {
    // From http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4436.pdf
    namespace impl
    {
      template <typename... Ts> struct make_void
      {
        using type = void;
      };
      template <typename... Ts> using void_t = typename make_void<Ts...>::type;
      template <class...> struct types
      {
        using type = types;
      };
      template <template <class...> class T, class types, class = void> struct test_apply
      {
        using type = void;
      };
      template <template <class...> class T, class... Ts> struct test_apply<T, types<Ts...>, void_t<T<Ts...>>>
      {
        using type = T<Ts...>;
      };
    };
    template <template <class...> class T, class... Ts> using test_apply = impl::test_apply<T, impl::types<Ts...>>;
    template <class T, class... Args> span<byte> _do_attach_object_instance(T &, span<byte> b) { return in_place_attach<T>(b); }
    template <class T, class... Args> span<byte> _do_detach_object_instance(T &, span<byte> b) { return in_place_detach<T>(b); }

    template <class T, class... Args> using get_attach_result = decltype(_do_attach_object_instance(std::declval<T>(), std::declval<Args>()...));
    template <class... Args> using safe_get_attach_result = test_apply<get_attach_result, Args...>;
    template <class T, class... Args> using get_detach_result = decltype(_do_detach_object_instance(std::declval<T>(), std::declval<Args>()...));
    template <class... Args> using safe_get_detach_result = test_apply<get_detach_result, Args...>;
  }

  /*! \brief True if a type is trivially attachable i.e. requires no extra work to attach.

  A type is trivially attachable if it does not contain non-null pointers or references.
  Determining what elements are inside UDTs requires Reflection to be available, so on
  compilers without Reflection, this is true only for fundamental types.
  */
  template <class T> struct is_trivially_attachable
  {
    static constexpr bool value = std::is_fundamental<T>::value && !std::is_void<T>::value;
  };
  template <class T> static constexpr bool is_trivially_attachable_v = is_trivially_attachable<T>::value;
  /*! \brief True if a type is trivially attachable, or has defined an ADL discovered free
  function of the form `span<byte> in_place_attach<T>(span<byte>)`.

  The most common custom object attachment action is to initialise all pointers and references
  with something valid for the running C++ program. When `in_place_attach<T>` is called, the
  lifetime of `T` has not yet begun, so any vptr etc. will be invalid.
  */
  template <class T, class AttachResultType = typename detail::safe_get_attach_result<T, span<byte>>::type> struct is_attachable
  {
    static constexpr bool value = is_trivially_attachable<T>::value || std::is_same<AttachResultType, span<byte>>::value;
  };
  template <class T> static constexpr bool is_attachable_v = is_attachable<T>::value;

  /*! \brief True if a type is trivially detachable i.e. requires no extra work to detach.

  A type is trivially detachable if it does not contain non-null pointers or references, and
  has a trivial destructor.
  Determining what elements are inside UDTs requires Reflection to be available, so on
  compilers without Reflection, this is true only for fundamental types.
  */
  template <class T> struct is_trivially_detachable
  {
    static constexpr bool value = std::is_fundamental<T>::value && !std::is_void<T>::value;
  };
  template <class T> static constexpr bool is_trivially_detachable_v = is_trivially_detachable<T>::value;
  /*! \brief True if a type is trivially detachable, or has defined an ADL discovered free
  function of the form `span<byte> in_place_detach<T>(span<byte>)`.

  The most common custom object detachment action is to reset all pointers and references
  to all bits zero. When `in_place_detach<T>` is called, the lifetime of `T` has ended, so
  any vptr etc. will be invalid.
  */
  template <class T, class DetachResultType = typename detail::safe_get_detach_result<T, span<byte>>::type> struct is_detachable
  {
    static constexpr bool value = is_trivially_detachable<T>::value || std::is_same<DetachResultType, span<byte>>::value;
  };
  template <class T> static constexpr bool is_detachable_v = is_detachable<T>::value;
}

/*! The error codes specific to `basic_key_value_store`. Note the `std::errc` equivalent
codes may also be returned e.g. `std::errc::insufficient_disk_space`.
*/
enum class kvstore_errc
{
  success = 0,
  invalid_uri,                    //!< The URI is not in an understood format.
  unsupported_uri,                //!< The URI specified an unsupported scheme or mechanism.
  unsupported_integrity,          //!< The requested integrity level is not available for this device URI.
  transaction_aborted_collision,  //!< The transaction could not be committed due to dependent key update.
};

/*! \brief Information about an available key value store implementation.
*/
struct basic_key_value_store_info
{
  //! The type of the UTF-8 URI used by this store
  // using uri_type = std::basic_string<char8_t>;
  using uri_type = std::basic_string<char>;
  //! The value extent type used by this store
  using extent_type = llfio::file_handle::extent_type;
  //! The memory extent type used by this store
  using size_type = llfio::file_handle::size_type;
  //! The handle type used by this store
  using handle_type = llfio::file_handle;
  //! The mode used by this store
  using mode = handle_type::mode;
  //! The creation used by this store
  using creation = handle_type::creation;
  //! The kernel caching used by this store
  using caching = handle_type::caching;

  const char *name;            //!< The name of this store implementation
  size_type min_key_size;      //!< The minimum key size, in bytes, supported.
  size_type max_key_size;      //!< The maximum key size, in bytes, supported.
  extent_type min_value_size;  //!< The minimum value size, in bytes, supported.
  extent_type max_value_size;  //!< The maximum value size, in bytes, supported.
  //! Features requested, or provided by, this store.
  QUICKCPPLIB_BITFIELD_BEGIN(features)
  {
    none = 1U << 0U,                //!< Bare key-value store. Very likely to choose a hardware implementation, if one is available.
    shared_memory = 1U << 1U,       //!< Use a single shared memory region for storage. Note that keys and value sizes freeze after URI fetch.
    history = 1U << 2U,             //!< A certain amount of history of previous valid states of the store is kept such that a previously valid state can always be restored after sudden power loss.
    stable_values = 1U << 3U,       //!< Updates do not appear to modify any pinned value.
    stable_keys = 1U << 4U,         //!< Updates do not appear to remove any pinned keys.
    update_deltas = 1U << 5U,       //!< In-place partial updates are recorded as change deltas.
    atomic_snapshots = 1U << 6U,    //!< The ability to pin the value of more than one key in an atomic snapshot.
    atomic_transactions = 1U << 7U  //!< The ability to update many items with dependencies on other items as a single, all-or-nothing, change.
  }
  QUICKCPPLIB_BITFIELD_END(features)
  features features;  //!< The features this store implementation provides
  /*! Examine the specified URI for suitability for this store implementation.
  Zero means that the URI location is suitable, but the format at that location
  is not compatible. Negative means the URI location is not possible.
  Higher positive scores outrank other positive scores.
  */
  int (*score)(const uri_type &uri, handle_type::mode, handle_type::creation creation);
  /*! Construct a store implementation.
  */
  result<basic_key_value_store> (*create)(const basic_key_value_store::uri_type &uri, size_type key_size, features _features, mode _mode, creation _creation, caching _caching);
};

/*! \class basic_key_value_store
\brief A possibly hardware-implemented basic key-value store.

\warning This class has not been implemented nor will be any time soon. It is here for various folk to study the API design.

Reference document https://www.snia.org/sites/default/files/technical_work/PublicReview/KV%20Storage%20API%200.16.pdf
*/
class basic_key_value_store
{
public:
  //! The key type of this store
  using key_type = span<const byte>;
  //! The value type of this store
  using value_type = span<byte>;

  //! The type of the UTF-8 URI used by this store
  using uri_type = basic_key_value_store_info::uri_type;
  //! The device extent type used by this store
  using capacity_type = QUICKCPPLIB_NAMESPACE::integers128::uint128;
  //! The value extent type used by this store
  using extent_type = llfio::file_handle::extent_type;
  //! The memory extent type used by this store
  using size_type = llfio::file_handle::size_type;
  //! The allocator type used by this store
  using allocator_type = QUICKCPPLIB_NAMESPACE::pmr::polymorphic_allocator<byte>;

  //! The handle type used by this store
  using handle_type = llfio::file_handle;
  //! The mode used by this store
  using mode = handle_type::mode;
  //! The creation used by this store
  using creation = handle_type::creation;
  //! The kernel caching used by this store
  using caching = handle_type::caching;
  //! The buffer type used by this store
  using buffer_type = handle_type::buffer_type;
  //! The const buffer type used by this store
  using const_buffer_type = handle_type::const_buffer_type;
  //! The buffers type used by this store
  using buffers_type = handle_type::buffers_type;
  //! The const buffers type used by this store
  using const_buffers_type = handle_type::const_buffers_type;
  //! The i/o request type used by this store
  template <class T> using io_request = handle_type::io_request<T>;
  //! The i/o result type used by this store
  template <class T> using io_result = handle_type::io_result<T>;

  //! Features requested, or provided by, this store.
  using features = struct basic_key_value_store_info::features;

protected:
  uri_type _uri{};
  bool _frozen{false};
  size_type _key_size{0}, _value_size{0}, _key_index_size{0};
  capacity_type _items_quota{0}, _bytes_quota{0};
  allocator_type _allocator{};

  // Cannot be copied
  basic_key_value_store(const basic_key_value_store &) = delete;
  basic_key_value_store &operator=(const basic_key_value_store &) = delete;
  //! Move constructor
  basic_key_value_store(basic_key_value_store &&o) noexcept = default;
  //! Move assignment
  basic_key_value_store &operator=(basic_key_value_store &&o) noexcept = default;

public:
  virtual ~basic_key_value_store() {}

  /*! Fetch the URI for this store. Note that if `features::shared_memory`, this will freeze
  the keys and value sizes in place from henceforth onwards. Only the values will be mutable.
  */
  virtual result<uri_type> uri() noexcept = 0;
  //! Returns the number of bytes in the key for this store.
  size_type key_size() const noexcept { return _key_size; }
  //! Returns the number of bytes in the value for this store. Zero means variably sized.
  size_type value_size() const noexcept { return _value_size; }
  //! True if the keys and values sizes are immutable, and only values may be changed.
  bool frozen() const { return _frozen; }

  //! True if the store is currently empty.
  virtual bool empty() const noexcept = 0;
  //! Returns the maximum number of key-values which could be potentially stored (estimated if using variably sized values).
  virtual result<capacity_type> max_size() const noexcept = 0;
  //! Sets the maximum number of key-values which could be potentially stored.
  virtual result<void> max_size(capacity_type quota) noexcept = 0;
  //! Returns the current number of key-values currently in the store.
  virtual result<capacity_type> size() const noexcept = 0;
  //! Returns the maximum number of bytes which could be potentially stored.
  virtual result<capacity_type> max_bytes_stored() const noexcept = 0;
  //! Sets the maximum number of bytes which could be potentially stored.
  virtual result<void> max_bytes_stored(capacity_type quota) noexcept = 0;
  //! Returns the current number of bytes stored.
  virtual result<capacity_type> bytes_stored() const noexcept = 0;
  //! Returns the maximum value size in bytes supported by this store.
  virtual result<extent_type> max_value_size() const noexcept = 0;

  //! Access the allocator used for this store.
  allocator_type &allocator() noexcept { return _allocator; }
  //! Access the allocator used for this store.
  const allocator_type &allocator() const noexcept { return _allocator; }

  //! Retrieves any MSB key index size, in bytes.
  size_type key_index_size() const noexcept { return _key_index_size; }
  /*! Sets a MSB key index size, in bytes. Some store implementations may only permit this if
  the store is empty. Setting a MSB key index causes filtering and search operations based on the key
  index to be constant rather than linear time.
  */
  virtual result<void> key_index_size(size_type bytes) noexcept = 0;

  //! Clears the store, possibly more quickly than deleting every key. This call may be racy.
  virtual result<void> clear() noexcept = 0;
  //! The state type for performing a filtered match
  using filter_state_type = uint64_t;
  /*! Returns the first or next key in the store matching the given filter. This call is racy, and
  filter processing may take as much time as iterating the entire contents of the store unless
  the mask **exactly** matches any key index, in which case it shall be constant time. Note
  that some hardware devices perform filter matching on-device, which does not change the
  algorithmic complexity, but may benefit from greatly increased on-device bandwidth.

  To begin a match, pass a default initialised `filter_state_type`. An error matching
  `errc::no_such_file_or_directory` will be returned if no more keys match. The default
  initialised mask and bits causes matching of all keys in the store.
  */
  virtual result<void> match(filter_state_type &state, key_type mask = {}, key_type bits = {}) noexcept = 0;

  /*! Returns a handle type which gives access to a key's value. The lifetime of the
  returned handle *may* pin the key's value at the time of retrieval if this store
  has `features::stable_values`.

  This function avoids reading any of the value where possible. It may internally
  use memory maps, and thus return scatter buffers to elsewhere memory, same as
  `llfio::mapped_file_handle`.

  The ability to open a value for write access depends on the lack of `features::stable_values`,
  or presence of `features::update_deltas`. Stable values without update deltas
  will not permit the value to be opened for write access.

  This call is not racy with respect to its value if `features::stable_values`.
  Individual key opens are racy with respect to one another i.e. other threads
  or processes may modify values concurrently, thus causing a sequence of key
  opens to refer to disjoint moments in time. Use `snapshot` if you don't want
  this.

  \note The handle type returned implements `llfio::file_handle`, but may be a
  lightweight synthetic handle with no kernel resources backing it. In particular,
  not all operations may be implemented e.g. byte range locks.
  */
  virtual result<handle_type> open(key_type key, mode _mode = mode::read) noexcept = 0;
  /*! Scatter reads some or all of a key's value into the supplied buffers,
  possibly returning completely different buffers to memory elsewhere similar
  to `llfio::mapped_file_handle`.

  This call *may* be more efficient than `open()`, or it may be implemented entirely
  in terms of `open()`. This call is unavoidably racy, and it may read different
  values for a key each time it is called.
  */
  virtual io_result<buffers_type> read(io_request<buffers_type> reqs, key_type key, llfio::deadline d = llfio::deadline()) noexcept = 0;
  /*! Gather writes some or all of a key's value from the supplied buffers,
  returning buffers written. For stores with `features::stable_values` but without
  `features::update_deltas`, the offset must be zero, and the gather list is assumed
  to reflect the whole of the value to be written.

  This call *may* be more efficient than `open()`, or it may be implemented entirely
  in terms of `open()`. This call is unavoidably racy, and it may write into different
  values for a key each time it is called.
  */
  virtual io_result<const_buffers_type> write(key_type key, io_request<const_buffers_type> reqs, llfio::deadline d = llfio::deadline()) noexcept = 0;


  /*! Returns a snapshot of the key store's values at a single moment in time i.e. all the values
  appear as-if in a single moment of time with no changes appearing to values after the snapshot.
  In this sense, the snapshot is *atomic*. Values snapshotted are pinned to the moment of the
  snapshot.

  Some store implementations do not implement both `features::stable_values` and
  `features::stable_keys`, if so then creating a snapshot must read all the values for all the
  keys during snapshot creation (which can be more efficient than reading each key-value at a
  time). Otherwise values are not read, rather a note is taken of their current state,
  and values are only fetched on demand.

  Some store implementations do not implement `features::stable_keys`, if so a snapshot's
  list of keys may be affected by subsequent changes to the main store. In other words,
  the keys in a snapshot may match those of the main store, only the values for what keys
  are not deleted in the main store remain pinned.

  Note that if a store does not have `features::history`, one may find that retrieving a
  value from a snapshot may no longer be able to retrieve that value as it has since been
  replaced. In this situation you will receive an error matching `errc::no_such_file_or_directory`.

  It is never possible to read a value from a snapshot and get an updated value instead
  of the snapshotted value. This is why not all stores implement `features::snapshot`, as
  a store must implement some mechanism of knowing when a value has been updated since
  it was pinned.

  If a store implementation does not implement `features::atomic_snapshot`, this function returns
  an error code comparing equal to `errc::operation_not_supported`.
  */
  virtual result<basic_key_value_store> snapshot() noexcept = 0;

  class transaction;
  /*! Begin a transaction on this key value store.

  Transactions are a superset of snapshots in that they also allow the modification of
  multiple items in a store in an atomic, all-or-nothing, operation. Reading values
  marks that item as a *dependency* i.e. if that item's value is updated between the
  moment of opening the transaction and committing the transaction, the transaction will abort.
  You can mark values as dependencies without actually reading them using `dependencies()`.

  If a store implementation does not implement `features::atomic_transactions`, this function returns
  an error code comparing equal to `errc::operation_not_supported`.
  */
  virtual result<transaction> begin_transaction() noexcept = 0;
};

class basic_key_value_store::transaction : public basic_key_value_store
{
public:
  //! Indicate that there is a dependency on the snapshotted values of these keys without fetching any values.
  virtual result<void> dependencies(span<key_type> keys) noexcept = 0;
  //! Try to commit this transaction, failing if any dependencies have been updated.
  virtual result<void> commit() noexcept = 0;
};

/*! \brief Create a new key value store, or open or truncate an existing key value store, using the given URI.

Query the system and/or process registry of key value store providers for an implementation capable of using
a store at `uri` with the specified key size and features. Guaranteed built-in providers are:

- `file://` based providers:

    - `directory_simple_legacy`: A very simple FAT-compatible file based store based on turning the key into
    hexadecimal, chopping it into chunks of four (`0x0000 - 0xffff`) such that a hex key `0x01234567890abcdef`'s
    value would be stored in the path `store/01234/5678/90ab/cdef`. Key updates are first written into
    `stage/01234/5678/90ab/cdef`, then once fully written they are atomically renamed into `store`.
    This store implements only `features::stable_values`, and is very widely compatible, including with
    networked drives. Its major downside is potential allocation wastage for many small sized values too
    big to be stored in the inode, but substantially below the allocation granularity.

    - `directory_modern`: A much more complex file based store which implements `features::history`,
    `features::stable_values`, `features::stable_keys`, `features::update_deltas`, `features::atomic_snapshots`
    and `features::atomic_transactions`.

    Inside the file `monotonic` is a 64 bit monotonic count representing time. Updated values are kept at
    `store/01234/5678/90ab/cdef/values/count`, where the monotonic count is atomically incremented every update.
    `store/01234/5678/90ab/cdef/deltas` is where 4Kb update deltas are kept if the value is larger than 64Kb.
    `store/01234/5678/90ab/cdef/latest` is the count of the latest value, its size, and if deltas need to be
    applied.

    - `single_file`: Initially all keys and values are kept in memory. Upon first URI fetch after first creation,
    a single file is created comprising all the keys and values associatively mapped. This single file can be
    then be mapped as shared memory into multiple processes, thus enabling multiple concurrent C++ programs to
    collaborate on a shared store where only values are mutable (and not resizeable). Only one process may pin
    a value at a time concurrently.

URIs may of course specify other sources of key value store than on the file system. Third parties may have
registered system-wide implementations available to all programs. The local process may have registered
additional implementations as well.
*/
LLFIO_HEADERS_ONLY_FUNC_SPEC result<basic_key_value_store> create_kvstore(const basic_key_value_store::uri_type &uri,                                              //
                                                                          basic_key_value_store::size_type key_size,                                               //
                                                                          basic_key_value_store::features _features,                                               //
                                                                          basic_key_value_store::mode _mode = basic_key_value_store::mode::write,                  //
                                                                          basic_key_value_store::creation _creation = basic_key_value_store::creation::if_needed,  //
                                                                          basic_key_value_store::caching _caching = basic_key_value_store::caching::all);
/*! \brief Open an existing key value store. A convenience overload for `create_kvstore()`.
*/
LLFIO_HEADERS_ONLY_FUNC_SPEC result<basic_key_value_store> open_kvstore(const basic_key_value_store::uri_type &uri,                              //
                                                                        basic_key_value_store::mode _mode = basic_key_value_store::mode::write,  //
                                                                        basic_key_value_store::caching _caching = basic_key_value_store::caching::all);

/*! \brief Fill an array with information about all the key value stores available to this process.
*/
LLFIO_HEADERS_ONLY_FUNC_SPEC result<span<basic_key_value_store_info>> enumerate_kvstores(span<basic_key_value_store_info> lst);

#if 0
/*! \brief A possibly hardware-implemented basic key-value store.
\tparam KeyType An integer type used to uniquely identify the mapped value. Ideally keep
these on a DMA boundary (`std::hardware_constructive_interference_size`).
\tparam MappedType A type for which `traits::is_attachable` or `traits::is_detachable`
is true, or else a span of such type to indicate the mapped type may be of variable size.
If `MappedType` is of fixed size, and if
`sizeof(KeyType) + sizeof(MappedType) <= std::hardware_constructive_interference_size`,
a much more efficient implementation may be used. Ideally keep the address of the mapped
value type on a DMA boundary (`std::hardware_constructive_interference_size`).
*/
LLFIO_TEMPLATE(class KeyType, class MappedType = span<byte>)
LLFIO_TREQUIRES(LLFIO_TPRED(std::is_integral<KeyType>::value                                                //
                            && (traits::is_attachable_v<MappedType> || traits::is_detachable_v<MappedType>  //
                                || traits::is_attachable_v<typename detail::extract_span_type<MappedType>::type> || traits::is_detachable_v<typename detail::extract_span_type<MappedType>::type>) ))
class key_value_store : public basic_key_value_store
{
public:
  //! The key type of this store
  using key_type = KeyType;
  //! The value type of this store
  using value_type = MappedType;

  //! Key iterator
  class const_iterator
  {
    basic_key_value_store *_parent;
    key_type _mask, _bits;

  public:
    using value_type = key_type;

    const_iterator();
    value_type operator*() const;
    const_iterator operator++();
  };
  //! Key iterator
  using iterator = const_iterator;

  /*! Returns the "first" key in the store matching the given filter. This call is racy, and
  filter processing may take as much time as iterating the entire contents of the store unless
  the mask **exactly** matches any key index, in which case it shall be constant time. Note
  that some hardware devices perform filter matching on-device, which does not change the
  algorithmic complexity, but may benefit from greatly increased on-device bandwidth.
  */
  virtual result<const_iterator> begin(key_type mask = {}, key_type bits = {}) noexcept = 0;
  //! Returns a null const iterator with which to detect the end of an iteration.
  const_iterator end() noexcept;
};

/*! \brief Returns a reference to this program's address space key value store.

All running C++ programs have an address space key value store which comprises the binary executable
code and memory mapped sections which form that C++ program. The keys are pointer sized, and are the
addresses in memory to which that section is mapped into memory.

This key value store is synthesised from live kernel data, and thus will change over time.
*/
LLFIO_HEADERS_ONLY_FUNC_SPEC const key_value_store<void *, llfio::section_handle> &address_space();

#endif


KVSTORE_V1_NAMESPACE_END

#endif
