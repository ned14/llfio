/* Multiplex i/o
(C) 2019 Niall Douglas <http://www.nedproductions.biz/> (9 commits)
File Created: Nov 2019


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

#ifndef LLFIO_MULTIPLEX_H
#define LLFIO_MULTIPLEX_H

#include "handle.hpp"

#include "outcome/coroutine_support.hpp"

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \brief A C++ Receiver of an i/o read for an i/o type of `IoHandleType`.
 */
template <class IoHandleType> class read_receiver
{
public:
  //! The i/o handle type this read receiver is for
  using io_handle_type = IoHandleType;
  //! The buffers type this receiver receives
  using buffers_type = typename io_handle_type::buffers_type;
  //! The successful read result type this receiver receives
  using value_type = typename io_handle_type::io_result<buffers_type>;
  //! The failure result type this receiver receives
  using error_type = typename value_type::error_type;

  //! Used by a Sender to set the result of an i/o read
  void set_value(value_type result);
  //! Used by a Sender to set the failure of an i/o read
  void set_error(error_type errinfo);
  //! Used by a Sender to set that an i/o read was cancelled
  void set_done();
};

/*! \brief A C++ Receiver of an i/o write for an i/o type of `IoHandleType`.
 */
template <class IoHandleType> class write_receiver
{
public:
  //! The i/o handle type this read receiver is for
  using io_handle_type = IoHandleType;
  //! The buffers type this receiver receives
  using buffers_type = typename io_handle_type::const_buffers_type;
  //! The successful write result type this receiver receives
  using value_type = typename io_handle_type::io_result<buffers_type>;
  //! The failure result type this receiver receives
  using error_type = typename value_type::error_type;

  //! Used by a Sender to set the result of an i/o write
  void set_value(value_type result);
  //! Used by a Sender to set the failure of an i/o write
  void set_error(error_type errinfo);
  //! Used by a Sender to set that an i/o write was cancelled
  void set_done();
};

/*! \brief Some implementation of a C++ Executor
 */
class executor
{
public:
  virtual ~executor() {}

  virtual sender submit(receiver &&) = 0;
};

/*! \brief An awaitable handle which attempts to execute the i/o
immediately. If the i/o can complete immediately, no coroutine
suspension occurs. Only if the i/o would take a while is coroutine
suspension performed.
 */
template <class IoHandleType> class awaitable_handle : public handle
{
public:
  template<class T> using eager_awaitable = OUTCOME_V2_NAMESPACE::awaitables::eager<T>;
  LLFIO_MAKE_FREE_FUNCTION
  eager_awaitable<io_result<buffers_type>> read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept;

  LLFIO_DEADLINE_TRY_FOR_UNTIL(read)

  LLFIO_MAKE_FREE_FUNCTION
  eager_awaitable<io_result<const_buffers_type>> write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept;

  LLFIO_DEADLINE_TRY_FOR_UNTIL(write)

  LLFIO_MAKE_FREE_FUNCTION
  eager_awaitable<io_result<const_buffers_type>> barrier(io_request<const_buffers_type> reqs = io_request<const_buffers_type>(), barrier_kind kind = barrier_kind::nowait_data_only, deadline d = deadline()) noexcept;

  LLFIO_DEADLINE_TRY_FOR_UNTIL(barrier)
};

LLFIO_V2_NAMESPACE_END

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
