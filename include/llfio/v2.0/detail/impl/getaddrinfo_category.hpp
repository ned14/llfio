/* A custom error category for getaddrinfo codes
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

#ifndef LLFIO_GETADDRINFO_CATEGORY_HPP
#define LLFIO_GETADDRINFO_CATEGORY_HPP

#include "../../config.hpp"

#ifdef _WIN32
#include <ws2ipdef.h>
#else
#include <netdb.h>
#endif

#include <system_error>

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

LLFIO_HEADERS_ONLY_FUNC_SPEC const std::error_category &getaddrinfo_category()
{
  static struct final : public std::error_category
  {
    virtual const char * name() const noexcept override { return "getaddrinfo"; }
    virtual std::error_condition default_error_condition(int code) const noexcept override
    {
      switch(code)
      {
      case EAI_ADDRFAMILY:
        return std::errc::address_not_available;
      case EAI_AGAIN:
        return std::errc::resource_unavailable_try_again;
      case EAI_BADFLAGS:
        return std::errc::invalid_argument;
      case EAI_FAIL:
        return std::errc::io_error;
      case EAI_FAMILY:
        return std::errc::address_family_not_supported;
      case EAI_MEMORY:
        return std::errc::not_enough_memory;
      case EAI_NODATA:
        return std::errc::address_not_available;
      case EAI_NONAME:
        return std::errc::invalid_argument;
      case EAI_SERVICE:
        return std::errc::wrong_protocol_type;
      case EAI_SOCKTYPE:
        return std::errc::invalid_argument;
      default:
        return std::errc::resource_unavailable_try_again;
      }
    }
    virtual std::string message(int condition) const override { return ::gai_strerror(condition); }
  } v;
  return v;
}

LLFIO_V2_NAMESPACE_END

#endif
