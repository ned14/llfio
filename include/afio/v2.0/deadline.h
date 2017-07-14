/* Specifies a time deadline
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (4 commits)
File Created: Dec 2015


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

#ifndef AFIO_DEADLINE_H
#define AFIO_DEADLINE_H

#include <stdbool.h>
#include <time.h>

//! \file deadline.h Provides struct deadline

#ifdef __cplusplus
#ifndef AFIO_CONFIG_HPP
#error You must include the master afio.hpp, not individual header files directly
#endif
#include "config.hpp"
#include <stdexcept>
AFIO_V2_NAMESPACE_EXPORT_BEGIN
#define AFIO_DEADLINE_NAME deadline
#else
#define AFIO_DEADLINE_NAME boost_afio_deadline
#endif

/*! \struct deadline
\brief A time deadline in either relative-to-now or absolute (system clock) terms
*/
struct AFIO_DEADLINE_NAME
{
  bool steady;  //!< True if deadline does not change with system clock changes
  union {
    struct timespec utc;       //!< System time from timespec_get(&ts, TIME_UTC)
    unsigned long long nsecs;  //!< Nanosecond ticks from start of operation
  };
#ifdef __cplusplus
  deadline() noexcept { memset(this, 0, sizeof(*this)); }
  //! True if deadline is valid
  explicit operator bool() const noexcept { return steady || utc.tv_sec != 0; }
  //! Construct a deadline from a system clock time point
  deadline(std::chrono::system_clock::time_point tp)
      : steady(false)
  {
    std::chrono::seconds secs(std::chrono::system_clock::to_time_t(tp));
    utc.tv_sec = secs.count();
    std::chrono::system_clock::time_point _tp(std::chrono::system_clock::from_time_t(utc.tv_sec));
    utc.tv_nsec = (long) std::chrono::duration_cast<std::chrono::nanoseconds>(tp - _tp).count();
  }
  //! Construct a deadline from a duration from now
  template <class Rep, class Period>
  deadline(std::chrono::duration<Rep, Period> d)
      : steady(true)
  {
    std::chrono::nanoseconds _nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(d);
    // Negative durations are zero duration
    if(_nsecs.count() > 0)
      nsecs = _nsecs.count();
    else
      nsecs = 0;
  }
  //! Returns a system_clock::time_point for this deadline
  std::chrono::system_clock::time_point to_time_point() const
  {
    if(steady)
      throw std::invalid_argument("Not a UTC deadline!");
    std::chrono::system_clock::time_point tp(std::chrono::system_clock::from_time_t(utc.tv_sec));
    tp += std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds(utc.tv_nsec));
    return tp;
  }
#endif
};

#undef AFIO_DEADLINE_NAME
#ifdef __cplusplus
AFIO_V2_NAMESPACE_END
#endif

#endif
