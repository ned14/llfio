/* A TLS socket source based on Windows SChannel
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

#include "../../../../byte_socket_source.hpp"

LLFIO_V2_NAMESPACE_BEGIN

static byte_socket_source_implementation_information windows_schannel_socket_source_registration_info;
static struct windows_schannel_socket_source_registration_t
{
  struct _byte_socket_source final : byte_socket_source
  {
    constexpr _byte_socket_source()
        : byte_socket_source(windows_schannel_socket_source_registration_info)
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

  windows_schannel_socket_source_registration_t()
  {
    auto &info = windows_schannel_socket_source_registration_info;
    info.name = "windows-schannel";
    info.postfix = "nonmultiplexable";
    info.features = byte_socket_source_implementation_features::kernel_sockets | byte_socket_source_implementation_features::system_implementation;
    info.instantiate = _instantiate;
    info.instantiate_with = _instantiate_with;
    byte_socket_source_registry::register_source(info).value();
  }
  ~windows_schannel_socket_source_registration_t()
  {
    byte_socket_source_registry::unregister_source(windows_schannel_socket_source_registration_info);
  }
} windows_schannel_socket_source_registration;

LLFIO_V2_NAMESPACE_END
