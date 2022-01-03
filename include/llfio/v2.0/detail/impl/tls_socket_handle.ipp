/* A source of byte-orientated socket handles
(C) 2021-2022 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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

#include "../../tls_socket_handle.hpp"

#include <map>
#include <mutex>
#include <sstream>

#ifndef LLFIO_DISABLE_OPENSSL
#if __has_include(<openssl/ssl.h>)
#define LLFIO_DISABLE_OPENSSL 0
#else
#define LLFIO_DISABLE_OPENSSL 1
#endif
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  struct tls_socket_source_registry_impl_t
  {
    std::mutex lock;
    std::map<std::string, tls_socket_source_implementation_information> registry;
  };
  LLFIO_HEADERS_ONLY_FUNC_SPEC tls_socket_source_registry_impl_t &tls_socket_source_registry_impl()
  {
    static tls_socket_source_registry_impl_t v;
    return v;
  }
}  // namespace detail

size_t tls_socket_source_registry::size() noexcept
{
  auto &impl = detail::tls_socket_source_registry_impl();
  std::lock_guard<std::mutex> g(impl.lock);
  return impl.registry.size();
}

span<tls_socket_source_implementation_information> tls_socket_source_registry::sources(span<tls_socket_source_implementation_information> tofill,
                                                                                         tls_socket_source_implementation_features set,
                                                                                         tls_socket_source_implementation_features mask) noexcept
{
  if(tofill.empty())
  {
    return tofill;
  }
  auto &impl = detail::tls_socket_source_registry_impl();
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

result<void> tls_socket_source_registry::register_source(tls_socket_source_implementation_information i) noexcept
{
  std::stringstream ss;
  ss << i;
  auto &impl = detail::tls_socket_source_registry_impl();
  std::lock_guard<std::mutex> g(impl.lock);
  auto it = impl.registry.find(ss.str());
  if(it != impl.registry.end())
  {
    return errc::file_exists;
  }
  impl.registry.emplace(std::move(ss).str(), i);
  return success();
}

void tls_socket_source_registry::unregister_source(tls_socket_source_implementation_information i) noexcept
{
  std::stringstream ss;
  ss << i;
  auto &impl = detail::tls_socket_source_registry_impl();
  std::lock_guard<std::mutex> g(impl.lock);
  impl.registry.erase(ss.str());
}

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#if !LLFIO_DISABLE_OPENSSL
#include "tls_socket_sources/openssl.ipp"
#endif
#if defined(_WIN32)
//#include "windows/tls_socket_sources/schannel.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif
