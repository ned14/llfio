/* Allows the test kernel to be compiled standalone
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (6 commits)
Created: May 2016


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

#ifndef BOOST_AFIO_TEST_KERNEL_DECL_HPP
#define BOOST_AFIO_TEST_KERNEL_DECL_HPP

#ifdef BOOST_KERNELTEST_PLEASE_INLINE_TEST_KERNELS
// We have been included as part of an inline test suite
#define BOOST_AFIO_TEST_KERNEL_DECL inline
#else
// We are standalone
#include "../include/boost/afio/afio.hpp"
#define BOOST_AFIO_TEST_KERNEL_DECL extern BOOSTLITE_SYMBOL_EXPORT
#endif

#endif  // namespace
