/* An mapped handle to a file
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
File Created: Sept 2017


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

#include "../../../mapped_file_handle.hpp"
#include "import.hpp"

AFIO_V2_NAMESPACE_BEGIN

mapped_file_handle::~mapped_file_handle()
{
}

result<mapped_file_handle::size_type> mapped_file_handle::reserve(size_type reservation) noexcept
{
}

result<void> mapped_file_handle::close() noexcept override
{
}
native_handle_type mapped_file_handle::release() noexcept override
{
}
result<file_handle> mapped_file_handle::clone() const noexcept override
{
}

result<mapped_file_handle::extent_type> mapped_file_handle::truncate(extent_type newsize) noexcept override
{
}

result<mapped_file_handle::extent_type> mapped_file_handle::truncate() noexcept
{
}

AFIO_V2_NAMESPACE_END
