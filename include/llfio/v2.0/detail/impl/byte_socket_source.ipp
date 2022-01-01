/* A source of byte-orientated socket handles
(C) 2021-2021 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Dec 2021


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

#include "../../byte_socket_source.hpp"

#include <map>
#include <mutex>
#include <sstream>

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  struct byte_socket_source_registry_impl_t
  {
    std::mutex lock;
    std::map<std::string, byte_socket_source_implementation_information> registry;
  };
  LLFIO_HEADERS_ONLY_FUNC_SPEC byte_socket_source_registry_impl_t &byte_socket_source_registry_impl()
  {
    static byte_socket_source_registry_impl_t v;
    return v;
  }
}  // namespace detail

size_t byte_socket_source_registry::size() noexcept
{
  auto &impl = detail::byte_socket_source_registry_impl();
  std::lock_guard<std::mutex> g(impl.lock);
  return impl.registry.size();
}

span<byte_socket_source_implementation_information> byte_socket_source_registry::sources(span<byte_socket_source_implementation_information> tofill,
                                                                                         byte_socket_source_implementation_features set,
                                                                                         byte_socket_source_implementation_features mask) noexcept
{
  if(tofill.empty())
  {
    return tofill;
  }
  auto &impl = detail::byte_socket_source_registry_impl();
  std::lock_guard<std::mutex> g(impl.lock);
  auto it = tofill.begin();
  for(auto &i : impl.registry)
  {
    if((i.second.features & mask) == set)
    {
      *it++ = i.second;
      if(it == tofill.end())
      {
        return tofill;
      }
    }
  }
  return {tofill.data(), (size_t)(it - tofill.begin())};
}

result<void> byte_socket_source_registry::register_source(byte_socket_source_implementation_information i) noexcept
{
  std::stringstream ss;
  ss << i;
  auto &impl = detail::byte_socket_source_registry_impl();
  std::lock_guard<std::mutex> g(impl.lock);
  auto it = impl.registry.find(ss.str());
  if(it != impl.registry.end())
  {
    return errc::file_exists;
  }
  impl.registry.emplace(std::move(ss).str(), i);
  return success();
}

void byte_socket_source_registry::unregister_source(byte_socket_source_implementation_information i) noexcept
{
  std::stringstream ss;
  ss << i;
  auto &impl = detail::byte_socket_source_registry_impl();
  std::lock_guard<std::mutex> g(impl.lock);
  impl.registry.erase(ss.str());
}


/***********************************************************************************************************/


static byte_socket_source_implementation_information default_nonmultiplexing_kernel_socket_source_registration_info;
static struct default_nonmultiplexing_kernel_socket_source_registration_t
{
  struct _byte_socket_source final : byte_socket_source
  {
    constexpr _byte_socket_source()
        : byte_socket_source(default_nonmultiplexing_kernel_socket_source_registration_info)
    {
    }

    virtual result<byte_socket_source_handle_ptr> byte_socket(ip::family family, byte_socket_handle::mode _mode, byte_socket_handle::caching _caching,
                                                              byte_socket_handle::flag flags) noexcept override
    {
      OUTCOME_TRY(auto &&sock, byte_socket_handle::byte_socket(family, _mode, _caching, flags));
      auto *p = new(std::nothrow) byte_socket_handle(std::move(sock));
      if(p == nullptr)
      {
        return errc::not_enough_memory;
      }
      return byte_socket_source_handle_ptr(p);
    }

    virtual result<listening_socket_source_handle_ptr> listening_socket(ip::family family, byte_socket_handle::mode _mode, byte_socket_handle::caching _caching,
                                                                        byte_socket_handle::flag flags) noexcept override
    {
      OUTCOME_TRY(auto &&sock, listening_socket_handle::listening_socket(family, _mode, _caching, flags));
      auto *p = new(std::nothrow) listening_socket_handle(std::move(sock));
      if(p == nullptr)
      {
        return errc::not_enough_memory;
      }
      return listening_socket_source_handle_ptr(p);
    }
  };

  static result<byte_socket_source_ptr> _instantiate() noexcept
  {
    auto *p = new(std::nothrow) _byte_socket_source;
    if(p == nullptr)
    {
      return errc::not_enough_memory;
    }
    return byte_socket_source_ptr(p);
  }
  static result<byte_socket_source_ptr> _instantiate_with(byte_io_multiplexer * /*unused*/) noexcept { return errc::function_not_supported; }

  default_nonmultiplexing_kernel_socket_source_registration_t()
  {
    auto &info = default_nonmultiplexing_kernel_socket_source_registration_info;
    info.name = "kernelsocket";
    info.postfix = "nonmultiplexable";
    info.features = byte_socket_source_implementation_features::kernel_sockets | byte_socket_source_implementation_features::system_implementation;
    info.instantiate = _instantiate;
    info.instantiate_with = _instantiate_with;
    byte_socket_source_registry::register_source(info).value();
  }
  ~default_nonmultiplexing_kernel_socket_source_registration_t()
  {
    byte_socket_source_registry::unregister_source(default_nonmultiplexing_kernel_socket_source_registration_info);
  }
} default_nonmultiplexing_kernel_socket_source_registration;

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#if defined(_WIN32)
#include "windows/byte_socket_sources/schannel.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif
