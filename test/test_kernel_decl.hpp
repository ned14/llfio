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

#ifndef LLFIO_TEST_KERNEL_DECL_HPP
#define LLFIO_TEST_KERNEL_DECL_HPP

#ifdef BOOST_KERNELTEST_PLEASE_INLINE_TEST_KERNELS
// We have been included as part of an inline test suite
#define LLFIO_TEST_KERNEL_DECL inline
#else
// We are standalone
#include "../include/llfio/llfio.hpp"
#define LLFIO_TEST_KERNEL_DECL extern inline QUICKCPPLIB_SYMBOL_EXPORT
#endif

#if LLFIO_EXPERIMENTAL_STATUS_CODE
#define KERNELTEST_EXPERIMENTAL_STATUS_CODE 1
#include "outcome/include/outcome/experimental/status-code/include/iostream_support.hpp"
#endif
#include "kerneltest/include/kerneltest.hpp"

#if 0
// Tell KernelTest's outcome how to grok LLFIO's result
OUTCOME_V2_NAMESPACE_BEGIN
namespace convert
{
  // Provide custom ValueOrError conversion from llfio::result<U> into kerneltest::result<T>
  template <class T, class U> struct value_or_error<KERNELTEST_V1_NAMESPACE::result<T>, LLFIO_V2_NAMESPACE::result<U>>
  {
    static constexpr bool enable_result_inputs = true;
    static constexpr bool enable_outcome_inputs = true;

    template <class X,                                                                                        //
              typename = std::enable_if_t<std::is_same<LLFIO_V2_NAMESPACE::result<U>, std::decay_t<X>>::value  //
                                          && std::is_constructible<T, U>::value>>                             //
    constexpr KERNELTEST_V1_NAMESPACE::result<T>
    operator()(X &&src)
    {
      // Forward exactly
      return src.has_value() ?                                                 //
             KERNELTEST_V1_NAMESPACE::result<T>{std::forward<X>(src).value()}  //
             :
             KERNELTEST_V1_NAMESPACE::result<T>{make_error_code(std::forward<X>(src).error())};
    }
  };
  // Provide custom ValueOrError conversion from llfio::result<U> into kerneltest::outcome<T>
  template <class T, class U> struct value_or_error<KERNELTEST_V1_NAMESPACE::outcome<T>, LLFIO_V2_NAMESPACE::result<U>>
  {
    static constexpr bool enable_result_inputs = true;
    static constexpr bool enable_outcome_inputs = true;

    template <class X,                                                                                        //
              typename = std::enable_if_t<std::is_same<LLFIO_V2_NAMESPACE::result<U>, std::decay_t<X>>::value  //
                                          && std::is_constructible<T, U>::value>>                             //
    constexpr KERNELTEST_V1_NAMESPACE::outcome<T>
    operator()(X &&src)
    {
      // Forward exactly
      return src.has_value() ?                                                  //
             KERNELTEST_V1_NAMESPACE::outcome<T>{std::forward<X>(src).value()}  //
             :
             KERNELTEST_V1_NAMESPACE::outcome<T>{make_error_code(std::forward<X>(src).error())};
    }
  };
}
OUTCOME_V2_NAMESPACE_END
static_assert(std::is_constructible<KERNELTEST_V1_NAMESPACE::result<int>, LLFIO_V2_NAMESPACE::result<int>>::value, "kerneltest::result<int> is not constructible from llfio::result<int>!");
static_assert(std::is_constructible<KERNELTEST_V1_NAMESPACE::outcome<int>, LLFIO_V2_NAMESPACE::result<int>>::value, "kerneltest::outcome<int> is not constructible from llfio::result<int>!");
#endif

#endif  // namespace
