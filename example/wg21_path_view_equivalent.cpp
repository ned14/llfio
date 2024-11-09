/* Demonstration that path_view overloads work as expected for WG21
(C) 2024 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Nov 2024


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

#include "../include/llfio.hpp"
#include "llfio/v2.0/path_view.hpp"
#include "quickcpplib/string_view.hpp"

#if __cplusplus >= 202000L || _HAS_CXX20
#include <filesystem>
#include <iostream>

namespace llfio = LLFIO_V2_NAMESPACE;
namespace fs = llfio::filesystem;

namespace std_filesystem
{
  using llfio::path_view;
  template <class T> using basic_string_view = llfio::basic_string_view<T>;
  template <class T> using span = llfio::span<T>;

  // Exposition type path-view-like, except made concrete here
  struct path_view_like : public path_view
  {
    path_view_like(path_view_component p,
                   format fmt = path_view::auto_format) noexcept
        : path_view(p, fmt)
    {
    }

    template <class CharT>
    constexpr path_view_like(const CharT *b, size_type l, enum termination zt,
                             format fmt = path_view::auto_format) noexcept
        : path_view(b, l, zt, fmt)
    {
    }
    constexpr path_view_like(const byte *b, size_type l,
                             enum termination zt) noexcept
        : path_view(b, l, zt)
    {
    }

    template <class CharT>
    constexpr path_view_like(basic_string_view<CharT> b, enum termination zt,
                             format fmt = path_view::auto_format) noexcept
        : path_view(b, zt, fmt)
    {
    }
    constexpr path_view_like(span<const byte> b, enum termination zt) noexcept
        : path_view(b, zt)
    {
    }

    template <class It, class End>
    constexpr path_view_like(It b, End e, enum termination zt,
                             format fmt = path_view::auto_format) noexcept
        : path_view(b, e, zt, fmt)
    {
    }
    template <class It, class End>
    constexpr path_view_like(It b, End e, enum termination zt) noexcept
        : path_view(b, e, zt)
    {
    }
  };

  // Emulate future std::path
  class path : public fs::path
  {
    using _base = fs::path;

  public:
    using _base::_base;

    explicit path(path_view_like pv)
        : _base(pv.path())
    {
    }
  };

  bool equivalent(const path &p1, const path &p2);

  bool equivalent(path_view_like p1, path_view_like p2);

  bool equivalent(path_view_like p1, const path &p2)
  {
    return equivalent(p1, path_view(p2));
  }

  bool equivalent(const path &p1, path_view_like p2)
  {
    return equivalent(path_view(p1), p2);
  }
}  // namespace std_filesystem

static size_t path_overload, path_view_overload;

/************************ implementation ***************************/

bool std_filesystem::equivalent(const path &p1, const path &p2)
{
  std::cout << "Calls path overload" << std::endl;
  path_overload++;
  return fs::equivalent(p1, p2);
}

bool std_filesystem::equivalent(path_view_like p1, path_view_like p2)
{
  std::cout << "Calls path_view overload" << std::endl;
  path_view_overload++;

  llfio::file_handle h1 =
  llfio::file_handle::file(
  {},                                  // base for lookup
  p1,                                  // path fragment
  llfio::file_handle::mode::attr_read  // open for attributes read
  )
  .value();  // throw exception if it fails

  llfio::file_handle h2 =
  llfio::file_handle::file(
  {},                                  // base for lookup
  p2,                                  // path fragment
  llfio::file_handle::mode::attr_read  // open for attributes read
  )
  .value();  // throw exception if it fails

  // If unique ids are identical, they are the same inode
  return h1.unique_id() == h2.unique_id();
}

/************************ test ***************************/

int main()
{
  auto temp_file = llfio::file_handle::temp_file().value();
  std_filesystem::path temp_file_path(temp_file.current_path().value());
  auto temp_file_path_view = llfio::path_view(temp_file_path);
  std::basic_string_view<fs::path::value_type> temp_file_string_view(
  temp_file_path.native());

  std::cout << "equivalent(path, path): ";
  if(!std_filesystem::equivalent(temp_file_path, temp_file_path))
  {
    abort();
  }
  if(path_overload != 1 || path_view_overload != 0)
  {
    abort();
  }
  std::cout << "equivalent(path_view, path_view): ";
  if(!std_filesystem::equivalent(temp_file_path_view, temp_file_path_view))
  {
    abort();
  }
  if(path_overload != 1 || path_view_overload != 1)
  {
    abort();
  }
  std::cout << "equivalent(path, path_view): ";
  if(!std_filesystem::equivalent(temp_file_path, temp_file_path_view))
  {
    abort();
  }
  if(path_overload != 1 || path_view_overload != 2)
  {
    abort();
  }
  std::cout << "equivalent(c_str, path): ";
  if(!std_filesystem::equivalent(temp_file_path.c_str(), temp_file_path))
  {
    abort();
  }
  if(path_overload != 2 || path_view_overload != 2)
  {
    abort();
  }
  std::cout << "equivalent(c_str, path_view): ";
  if(!std_filesystem::equivalent(temp_file_path.c_str(), temp_file_path_view))
  {
    abort();
  }
  if(path_overload != 2 || path_view_overload != 3)
  {
    abort();
  }
  std::cout << "equivalent(c_str, c_str): ";
  if(!std_filesystem::equivalent(temp_file_path.c_str(),
                                 temp_file_path.c_str()))
  {
    abort();
  }
  if(path_overload != 3 || path_view_overload != 3)
  {
    abort();
  }
  std::cout << "equivalent(string_view, path): ";
  if(!std_filesystem::equivalent(temp_file_string_view, temp_file_path))
  {
    abort();
  }
  if(path_overload != 4 || path_view_overload != 3)
  {
    abort();
  }
  std::cout << "equivalent({string_view, termination}, path): ";
  if(!std_filesystem::equivalent(
     {temp_file_string_view, std_filesystem::path_view::zero_terminated},
     temp_file_path))
  {
    abort();
  }
  if(path_overload != 4 || path_view_overload != 4)
  {
    abort();
  }
  std::cout << "equivalent({c_str, length, termination}, path): ";
  if(!std_filesystem::equivalent({temp_file_string_view.data(),
                                  temp_file_string_view.size(),
                                  std_filesystem::path_view::zero_terminated},
                                 temp_file_path))
  {
    abort();
  }
  if(path_overload != 4 || path_view_overload != 5)
  {
    abort();
  }
  return 0;
}
#else
int main()
{
  return 0;
}
#endif
