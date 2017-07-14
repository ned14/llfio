/* Multiplex file i/o
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

#include "../../../io_service.hpp"
#include "import.hpp"

AFIO_V2_NAMESPACE_BEGIN

io_service::io_service()
    : _work_queued(0)
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &_threadh, 0, false, DUPLICATE_SAME_ACCESS))
    throw std::runtime_error("Failed to create creating thread handle");
  _threadid = GetCurrentThreadId();
}

io_service::~io_service()
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(_work_queued)
  {
    fprintf(stderr, "WARNING: ~io_service() sees work still queued, blocking until no work queued\n");
    while(_work_queued)
      std::this_thread::yield();
  }
  CloseHandle(_threadh);
}

result<bool> io_service::run_until(deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(!_work_queued)
    return false;
  if(GetCurrentThreadId() != _threadid)
    return std::errc::operation_not_supported;
  ntsleep(d, true);
  return _work_queued != 0;
}

void io_service::post(detail::function_ptr<void(io_service *)> &&f)
{
  AFIO_LOG_FUNCTION_CALL(this);
  void *data = nullptr;
  {
    post_info pi(this, std::move(f));
    std::lock_guard<decltype(_posts_lock)> g(_posts_lock);
    _posts.push_back(std::move(pi));
    data = (void *) &_posts.back();
  }
  // lambdas can't be __stdcall on winclang, so ...
  struct lambda
  {
    static void __stdcall _(ULONG_PTR data)
    {
      post_info *pi = (post_info *) data;
      pi->f(pi->service);
      pi->service->_post_done(pi);
    }
  };
  PAPCFUNC apcf = lambda::_;
  if(QueueUserAPC(apcf, _threadh, (ULONG_PTR) data))
    _work_enqueued();
  else
  {
    post_info *pi = (post_info *) data;
    pi->service->_post_done(pi);
  }
}

AFIO_V2_NAMESPACE_END
