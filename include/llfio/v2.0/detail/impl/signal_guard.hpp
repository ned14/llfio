/* Guard against signal raises
(C) 2026 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
File Created: March 2026


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

#ifndef LLFIO_SIGNAL_GUARD_H
#define LLFIO_SIGNAL_GUARD_H

#include "../../config.hpp"

#ifdef LLFIO_DISABLE_SIGNAL_GUARD
#include <signal.h>

// MSVC may be missing necessary signal support
typedef uint32_t sigset_t;
static inline void sigemptyset(sigset_t *ss)
{
  *ss = 0;
}
static inline void sigfillset(sigset_t *ss)
{
  *ss = UINT32_MAX;
}
static inline void sigaddset(sigset_t *ss, const int signo)
{
  *ss |= (1u << (signo - 1));
}
static inline void sigdelset(sigset_t *ss, const int signo)
{
  *ss &= ~(1u << (signo - 1));
}
static inline bool sigismember(const sigset_t *ss, const int signo)
{
  return (*ss & (1u << (signo - 1))) != 0;
}

// MSVC appears to follow the Linux signal numbering
#ifndef SIGBUS
#define SIGBUS (7)
#endif
#ifndef SIGKILL
#define SIGKILL (9)
#endif
#ifndef SIGSTOP
#define SIGSTOP (19)
#endif

LLFIO_V2_NAMESPACE_BEGIN

template <class F, class H, class C, class... Args>
inline auto signal_guard(const sigset_t *, F &&f, H &&, C &&, Args &&...args)
{
  return f(static_cast<Args &&>(args)...);
}
template <class F, class H> inline auto signal_guard(const sigset_t *, F &&f, H &&)
{
  return f();
}

class signal_guard_installation_holder
{
public:
  explicit signal_guard_installation_holder(const sigset_t *) {}
  signal_guard_installation_holder(const signal_guard_installation_holder &) = delete;
  signal_guard_installation_holder(signal_guard_installation_holder &&) = delete;
  signal_guard_installation_holder &operator=(const signal_guard_installation_holder &) = delete;
  signal_guard_installation_holder &operator=(signal_guard_installation_holder &&) = delete;
};


LLFIO_V2_NAMESPACE_END
#else
// Header only build so it'll inline
#define WG14_SIGNALS_ENABLE_HEADER_ONLY 1
// Give it a custom prefix to prevent symbol clashes
#define LLFIO_SIGNAL_GUARD_WG14_SIGNALS_PREFIX3(x, y, z) x##_##y##_##z
#define LLFIO_SIGNAL_GUARD_WG14_SIGNALS_PREFIX2(x, y, z) LLFIO_SIGNAL_GUARD_WG14_SIGNALS_PREFIX3(x, y, z)
#if defined(LLFIO_UNSTABLE_VERSION) && !defined(LLFIO_DISABLE_ABI_PERMUTATION)
#define WG14_SIGNALS_PREFIX(x) LLFIO_SIGNAL_GUARD_WG14_SIGNALS_PREFIX2(llfio_v2, LLFIO_PREVIOUS_COMMIT_UNIQUE, x)
#else
#define WG14_SIGNALS_PREFIX(x) LLFIO_SIGNAL_GUARD_WG14_SIGNALS_PREFIX2(llfio_v2, , x)
#endif
#include "../../../wg14_signals/include/wg14_signals/thrd_signal_handle.h"

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  struct signal_guard_indirect
  {
    virtual ~signal_guard_indirect() {}
    virtual void do_guarded() noexcept = 0;
    virtual enum WG14_SIGNALS_PREFIX(sig_decision)
    do_decider(struct WG14_SIGNALS_PREFIX(stdc_siginfo) * rsi) noexcept = 0;
    virtual void do_recover(const struct WG14_SIGNALS_PREFIX(stdc_siginfo) * rsi) noexcept = 0;

    static union WG14_SIGNALS_PREFIX(stdc_siginfo_value) c_guarded(union WG14_SIGNALS_PREFIX(stdc_siginfo_value) value)
    {
      ((signal_guard_indirect *) value.ptr_value)->do_guarded();
      return value;
    }
    static enum WG14_SIGNALS_PREFIX(sig_decision) c_decider(struct WG14_SIGNALS_PREFIX(stdc_siginfo) * rsi)
    {
      return ((signal_guard_indirect *) rsi->value.ptr_value)->do_decider(rsi);
    }
    static union WG14_SIGNALS_PREFIX(stdc_siginfo_value) c_recover(const struct WG14_SIGNALS_PREFIX(stdc_siginfo) * rsi)
    {
      ((signal_guard_indirect *) rsi->value.ptr_value)->do_recover(rsi);
      return rsi->value;
    }
  };
}  // namespace detail

template <class F, class H, class C, class... Args>
inline auto signal_guard(const sigset_t *guarded, F &&f, H &&h, C &&c, Args &&...args)
{
  auto bound = [&]() { return f(static_cast<Args &&>(args)...); };
  using type = decltype(bound);
  using rettype = decltype(bound());
  struct signal_guard_indirect_impl final : detail::signal_guard_indirect
  {
    optional<rettype> ret;

    type f;
    H recover;
    C decider;

    signal_guard_indirect_impl(type &&f, H &&h, C &&c)
        : f(static_cast<type &&>(f))
        , recover(static_cast<H &&>(h))
        , decider(static_cast<C &&>(c))
    {
    }
    virtual void do_guarded() noexcept override { ret.emplace(f()); }
    virtual enum WG14_SIGNALS_PREFIX(sig_decision)
    do_decider(struct WG14_SIGNALS_PREFIX(stdc_siginfo) * rsi) noexcept override
    {
      return decider(rsi);
    }
    virtual void do_recover(const struct WG14_SIGNALS_PREFIX(stdc_siginfo) * rsi) noexcept override
    {
      ret.emplace(recover(rsi));
    }
  } trampoline(static_cast<type &&>(bound), static_cast<H &&>(h), static_cast<C &&>(c));
  union WG14_SIGNALS_PREFIX(stdc_siginfo_value) value;
  value.ptr_value = (void *) &trampoline;
  union WG14_SIGNALS_PREFIX(stdc_siginfo_value) ret = WG14_SIGNALS_PREFIX(sigguarded)(
  guarded, detail::signal_guard_indirect::c_guarded, detail::signal_guard_indirect::c_recover,
  detail::signal_guard_indirect::c_decider, value);
  if(ret.int_value == WG14_SIGNALS_PREFIX(SIGGUARDED_FAILURE_VALUE).int_value)
  {
    // The per-thread state required by sigguarded() could not be set up, so
    // the guarded function was never called. Degrade to running it unguarded.
    return f(static_cast<Args &&>(args)...);
  }
  return trampoline.ret.value();
}
template <class F, class H> inline auto signal_guard(const sigset_t *guarded, F &&f, H &&h)
{
  return signal_guard(guarded, static_cast<F &&>(f), static_cast<H &&>(h),
                      [](const auto *) { return WG14_SIGNALS_PREFIX(sig_decision_call_recovery); });
}

class signal_guard_installation_holder
{
  void *_h;

public:
  explicit signal_guard_installation_holder(const sigset_t *set)
      : _h(WG14_SIGNALS_PREFIX(siginstall(set)))
  {
  }
  ~signal_guard_installation_holder() { WG14_SIGNALS_PREFIX(siguninstall(_h)); }
  signal_guard_installation_holder(const signal_guard_installation_holder &) = delete;
  signal_guard_installation_holder(signal_guard_installation_holder &&) = delete;
  signal_guard_installation_holder &operator=(const signal_guard_installation_holder &) = delete;
  signal_guard_installation_holder &operator=(signal_guard_installation_holder &&) = delete;
};

LLFIO_V2_NAMESPACE_END
#endif

#endif
