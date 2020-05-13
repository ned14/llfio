/* Multiplex file i/o
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (9 commits)
File Created: May 2019


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

#include "../io_handle.ipp"

#ifndef __linux__
#error This implementation file is for Linux only
#endif

#include <linux/fs.h>
#include <linux/types.h>

LLFIO_V2_NAMESPACE_BEGIN

namespace test
{
  template <bool is_threadsafe> class linux_io_uring_multiplexer final : public io_multiplexer_impl<is_threadsafe>
  {
    using _base = io_multiplexer_impl<is_threadsafe>;
    using _multiplexer_lock_guard = typename _base::_lock_guard;

    using path_type = typename _base::path_type;
    using extent_type = typename _base::extent_type;
    using size_type = typename _base::size_type;
    using mode = typename _base::mode;
    using creation = typename _base::creation;
    using caching = typename _base::caching;
    using flag = typename _base::flag;
    using barrier_kind = typename _base::barrier_kind;
    using const_buffers_type = typename _base::const_buffers_type;
    using buffers_type = typename _base::buffers_type;
    using registered_buffer_type = typename _base::registered_buffer_type;
    template <class T> using io_request = typename _base::template io_request<T>;
    template <class T> using io_result = typename _base::template io_result<T>;
    using io_operation_state = typename _base::io_operation_state;
    using io_operation_state_visitor = typename _base::io_operation_state_visitor;
    using check_for_any_completed_io_statistics = typename _base::check_for_any_completed_io_statistics;

    // The io_uring kernel submission structure
    struct _io_uring_sqe
    {
      __u8 opcode;  /* type of operation for this sqe */
      __u8 flags;   /* IOSQE_ flags */
      __u16 ioprio; /* ioprio for the request */
      __s32 fd;     /* file descriptor to do IO on */
      union {
        __u64 off; /* offset into file */
        __u64 addr2;
      };
      union {
        __u64 addr; /* pointer to buffer or iovecs */
        __u64 splice_off_in;
      };
      __u32 len; /* buffer size or number of iovecs */
      union {
        __kernel_rwf_t rw_flags;
        __u32 fsync_flags;
        __u16 poll_events;
        __u32 sync_range_flags;
        __u32 msg_flags;
        __u32 timeout_flags;
        __u32 accept_flags;
        __u32 cancel_flags;
        __u32 open_flags;
        __u32 statx_flags;
        __u32 fadvise_advice;
        __u32 splice_flags;
      };
      __u64 user_data; /* data to be passed back at completion time */
      union {
        struct
        {
          /* pack this to avoid bogus arm OABI complaints */
          union {
            /* index into fixed buffers, if used */
            __u16 buf_index;
            /* for grouped buffer selection */
            __u16 buf_group;
          } __attribute__((packed));
          /* personality to use, if used */
          __u16 personality;
          __s32 splice_fd_in;
        };
        __u64 __pad2[3];
      };
    };

    // sqe->flags
    /* use fixed fileset */
    static constexpr __u32 _IOSQE_FIXED_FILE = (1U << 0);
    /* issue after inflight IO */
    static constexpr __u32 _IOSQE_IO_DRAIN = (1U << 1);
    /* links next sqe */
    static constexpr __u32 _IOSQE_IO_LINK = (1U << 2);
    /* like LINK, but stronger */
    static constexpr __u32 _IOSQE_IO_HARDLINK = (1U << 3);
    /* always go async */
    static constexpr __u32 _IOSQE_ASYNC = (1U << 4);
    /* select buffer from sqe->buf_group */
    static constexpr __u32 _IOSQE_BUFFER_SELECT = (1U << 5);

    // io_uring_setup() flags
    static constexpr __u32 _IORING_SETUP_IOPOLL = (1U << 0);    /* io_context is polled */
    static constexpr __u32 _IORING_SETUP_SQPOLL = (1U << 1);    /* SQ poll thread */
    static constexpr __u32 _IORING_SETUP_SQ_AFF = (1U << 2);    /* sq_thread_cpu is valid */
    static constexpr __u32 _IORING_SETUP_CQSIZE = (1U << 3);    /* app defines CQ size */
    static constexpr __u32 _IORING_SETUP_CLAMP = (1U << 4);     /* clamp SQ/CQ ring sizes */
    static constexpr __u32 _IORING_SETUP_ATTACH_WQ = (1U << 5); /* attach to existing wq */

    // sqe->opcode
    enum
    {
      _IORING_OP_NOP,
      _IORING_OP_READV,
      _IORING_OP_WRITEV,
      _IORING_OP_FSYNC,
      _IORING_OP_READ_FIXED,
      _IORING_OP_WRITE_FIXED,
      _IORING_OP_POLL_ADD,
      _IORING_OP_POLL_REMOVE,
      _IORING_OP_SYNC_FILE_RANGE,
      _IORING_OP_SENDMSG,
      _IORING_OP_RECVMSG,
      _IORING_OP_TIMEOUT,
      _IORING_OP_TIMEOUT_REMOVE,
      _IORING_OP_ACCEPT,
      _IORING_OP_ASYNC_CANCEL,
      _IORING_OP_LINK_TIMEOUT,
      _IORING_OP_CONNECT,
      _IORING_OP_FALLOCATE,
      _IORING_OP_OPENAT,
      _IORING_OP_CLOSE,
      _IORING_OP_FILES_UPDATE,
      _IORING_OP_STATX,
      _IORING_OP_READ,
      _IORING_OP_WRITE,
      _IORING_OP_FADVISE,
      _IORING_OP_MADVISE,
      _IORING_OP_SEND,
      _IORING_OP_RECV,
      _IORING_OP_OPENAT2,
      _IORING_OP_EPOLL_CTL,
      _IORING_OP_SPLICE,
      _IORING_OP_PROVIDE_BUFFERS,
      _IORING_OP_REMOVE_BUFFERS,

      /* this goes last, obviously */
      _IORING_OP_LAST,
    };

    // sqe->fsync_flags
    static constexpr __u32 _IORING_FSYNC_DATASYNC = (1U << 0);

    // sqe->timeout_flags
    static constexpr __u32 _IORING_TIMEOUT_ABS = (1U << 0);

    /*
     * sqe->splice_flags
     * extends splice(2) flags
     */
    static constexpr __u32 _SPLICE_F_FD_IN_FIXED = (1U << 31); /* the last bit of __u32 */


    // The io_uring kernel completion structure
    struct _io_uring_cqe
    {
      __u64 user_data; /* sqe->data submission passed back */
      __s32 res;       /* result code for this event */
      __u32 flags;
    };

    // cqe->flags
    // IORING_CQE_F_BUFFER	If set, the upper 16 bits are the buffer ID
    static constexpr __u32 _IORING_CQE_F_BUFFER = (1U << 0);

    static constexpr __u32 _IORING_CQE_BUFFER_SHIFT = 16;

    // Magic offsets for the application to mmap the data it needs
    static constexpr size_t _IORING_OFF_SQ_RING = (size_t) 0;
    static constexpr size_t _IORING_OFF_CQ_RING = (size_t) 0x8000000;
    static constexpr size_t _IORING_OFF_SQES = (size_t) 0x10000000;

    // Filled with the offset for mmap(2)
    struct _io_sqring_offsets
    {
      __u32 head;
      __u32 tail;
      __u32 ring_mask;
      __u32 ring_entries;
      __u32 flags;
      __u32 dropped;
      __u32 array;
      __u32 resv1;
      __u64 resv2;
    };

    // sq_ring->flags
    static constexpr __u32 _IORING_SQ_NEED_WAKEUP = (1U << 0); /* needs io_uring_enter wakeup */

    struct _io_cqring_offsets
    {
      __u32 head;
      __u32 tail;
      __u32 ring_mask;
      __u32 ring_entries;
      __u32 overflow;
      __u32 cqes;
      __u64 resv[2];
    };

    // io_uring_enter(2) flags
    static constexpr __u32 _IORING_ENTER_GETEVENTS = (1U << 0);
    static constexpr __u32 _IORING_ENTER_SQ_WAKEUP = (1U << 1);

    // Passed in for io_uring_setup(2). Copied back with updated info on success
    struct _io_uring_params
    {
      __u32 sq_entries;
      __u32 cq_entries;
      __u32 flags;
      __u32 sq_thread_cpu;
      __u32 sq_thread_idle;
      __u32 features;
      __u32 wq_fd;
      __u32 resv[3];
      struct _io_sqring_offsets sq_off;
      struct _io_cqring_offsets cq_off;
    };

    // io_uring_params->features flags
    static constexpr __u32 _IORING_FEAT_SINGLE_MMAP = (1U << 0);
    static constexpr __u32 _IORING_FEAT_NODROP = (1U << 1);
    static constexpr __u32 _IORING_FEAT_SUBMIT_STABLE = (1U << 2);
    static constexpr __u32 _IORING_FEAT_RW_CUR_POS = (1U << 3);
    static constexpr __u32 _IORING_FEAT_CUR_PERSONALITY = (1U << 4);
    static constexpr __u32 _IORING_FEAT_FAST_POLL = (1U << 5);

    // io_uring_register(2) opcodes and arguments
    enum
    {
      _IORING_REGISTER_BUFFERS,
      _IORING_UNREGISTER_BUFFERS,
      _IORING_REGISTER_FILES,
      _IORING_UNREGISTER_FILES,
      _IORING_REGISTER_EVENTFD,
      _IORING_UNREGISTER_EVENTFD,
      _IORING_REGISTER_FILES_UPDATE,
      _IORING_REGISTER_EVENTFD_ASYNC,
      _IORING_REGISTER_PROBE,
      _IORING_REGISTER_PERSONALITY,
      _IORING_UNREGISTER_PERSONALITY
    };

    struct _io_uring_files_update
    {
      __u32 offset;
      __u32 resv;
      __aligned_u64 /* __s32 * */ fds;
    };

    static constexpr __u32 _IO_URING_OP_SUPPORTED = (1U << 0);

    struct _io_uring_probe_op
    {
      __u8 op;
      __u8 resv;
      __u16 flags; /* IO_URING_OP_* flags */
      __u32 resv2;
    };

    struct _io_uring_probe
    {
      __u8 last_op; /* last opcode supported */
      __u8 ops_len; /* length of ops[] array below */
      __u16 resv;
      __u32 resv2[3];
      struct _io_uring_probe_op ops[0];
    };

    template<class T> static T _io_uring_smp_load_acquire(const T &_v) noexcept{
      auto *v = (const std::atomic<T> *) &_v;
      return v->load(std::memory_order_acquire);
    }
    template<class T> static void _io_uring_smp_store_release(T &_v, T x) noexcept{
      auto *v = (std::atomic<T> *) &_v;
      v->store(x, std::memory_order_release);
    }



    /* We need to choose a finalised i/o operation state implementation.
    i/o multiplexer provides an unsynchronised and a synchronised implementation
    from which we can subclass. We can, of course, add our own i/o operation
    state. Here we track time.
    */
    struct _null_operation_state final : public std::conditional_t<is_threadsafe, typename _base::_synchronised_io_operation_state, typename _base::_unsynchronised_io_operation_state>
    {
      using _impl = std::conditional_t<is_threadsafe, typename _base::_synchronised_io_operation_state, typename _base::_unsynchronised_io_operation_state>;

      size_t count{2};  // when reaches 1, completes. When reaches 0, finishes.
      _null_operation_state *prev{nullptr}, *next{nullptr};

      _null_operation_state() = default;
      // Construct implicitly from the base implementation, see relocate_to()
      explicit _null_operation_state(_impl &&o) noexcept
          : _impl(std::move(o))
      {
      }
      using _impl::_impl;

      // You will need to reimplement this to relocate any custom state defined
      // here, and to restamp to vptr with this finalised dynamic type. It is
      // important to do this, as final-based optimisations compare the vptr
      // to the finalised vptr and do non-indirect dispatch if they match.
      virtual io_operation_state *relocate_to(byte *to_) noexcept override
      {
        auto *to = _impl::relocate_to(to_);
        // restamp the vptr with my own
        auto _to = new(to) _null_operation_state(std::move(*static_cast<_impl *>(to)));
        _to->count = count;
        return _to;
      }

      void detach(linux_io_uring_multiplexer *parent)
      {
        if(prev == nullptr)
        {
          assert(parent->_first == this);
          parent->_first = next;
        }
        else
        {
          prev->next = next;
        }
        if(next == nullptr)
        {
          assert(parent->_last == this);
          parent->_last = prev;
        }
        else
        {
          next->prev = prev;
        }
        next = prev = nullptr;
      }
    };
    _null_operation_state *_first{nullptr}, *_last{nullptr};
    int _wakecount{0};

    void _insert(_null_operation_state *state)
    {
      assert(state->prev == nullptr);
      assert(state->next == nullptr);
      assert(_first != state);
      assert(_last != state);
      if(_first == nullptr)
      {
        _first = _last = state;
      }
      else
      {
        assert(_last->next == nullptr);
        state->prev = _last;
        _last->next = state;
        _last = state;
      }
    }

    // Per-thread statistics
    struct _thread_statistics_t
    {
    };
    std::vector<_thread_statistics_t> _thread_statistics;
    // Whether to simulate finishing immediately after completion
    bool _disable_immediate_completions{false};

  public:
    constexpr linux_io_uring_multiplexer() {}
    linux_io_uring_multiplexer(const linux_io_uring_multiplexer &) = delete;
    linux_io_uring_multiplexer(linux_io_uring_multiplexer &&) = delete;
    linux_io_uring_multiplexer &operator=(const linux_io_uring_multiplexer &) = delete;
    linux_io_uring_multiplexer &operator=(linux_io_uring_multiplexer &&) = delete;
    virtual ~linux_io_uring_multiplexer()
    {
      if(this->_v)
      {
        (void) linux_io_uring_multiplexer::close();
      }
    }
    result<void> init(size_t threads, bool disable_immediate_completions)
    {
      _thread_statistics.resize(threads);
      _disable_immediate_completions = disable_immediate_completions;
      // In a real multiplexer, you need to create the system's i/o multiplexer
      // and store it into this->_v. We shall store something other than -1
      // to make this handle appear open.
      this->_v._init = -2;  // otherwise appears closed
      this->_v.behaviour |= native_handle_type::disposition::multiplexer;
      return success();
    }

    // These functions are inherited from handle
    virtual result<path_type> current_path() const noexcept override
    {
      // handle::current_path() does the right thing for handles registered with the kernel
      // but we'll need to return something sensible here
      return success();  // empty path, means it has been deleted
    }
    virtual result<void> close() noexcept override
    {
      // handle::close() would close the handle with the kernel, which might suit your multiplexer implementation.
      // However we need to not do that here.
      this->_v._init = -1;  // make it appear closed
      return success();
    }
    // virtual native_handle_type release() noexcept override { return _base::release(); }

    // This registers an i/o handle with the system i/o multiplexer
    virtual result<uint8_t> do_io_handle_register(io_handle * /*unused*/) noexcept override
    {
      _multiplexer_lock_guard g(this->_lock);
      return success();
    }
    // This deregisters an i/o handle from the system i/o multiplexer
    virtual result<void> do_io_handle_deregister(io_handle * /*unused*/) noexcept override
    {
      _multiplexer_lock_guard g(this->_lock);
      return success();
    }

    // This returns the maximum *atomic* number of scatter-gather i/o that this i/o multiplexer can do
    // This is the value returned by io_handle::max_buffers()
    virtual size_t do_io_handle_max_buffers(const io_handle * /*unused*/) const noexcept override { return 1; }

    // This allocates a registered i/o buffer with the system i/o multiplexer
    // The default implementation calls mmap()/VirtualAlloc(), creates a deallocating
    // wrapper calling munmap()/VirtualFree(), and returns an aliasing shared_ptr for
    // those memory pages.
    //
    // A RDMA-capable i/o multiplexer would allocate the memory in a region mapped from the
    // RDMA device into the CPU. The program would read and write that shared memory. Upon
    // i/o, the buffer would be locked/unmapped for RDMA, thus implementing true zero whole
    // system memory copy i/o
    // virtual result<registered_buffer_type> do_io_handle_allocate_registered_buffer(io_handle *h, size_t &bytes) noexcept override {}

    // Code other side of the formal ABI boundary allocate the storage for i/o operation
    // states. They need to know how many bytes to allocate, and what alignment they must
    // have.
    //
    // Note there is a trick available here: if you say you require 1024 bytes aligned to
    // 1024 byte alignment, then if your system i/o multiplexer returns you a pointer to
    // some unknown offset within the i/o state, you can round down to the nearest 1024
    // byte boundary to get the address of the i/o operation state.
    virtual std::pair<size_t, size_t> io_state_requirements() noexcept override { return {sizeof(_null_operation_state), alignof(_null_operation_state)}; }

    // These are straight i/o state construction functions, one each for read, write and barrier
    virtual io_operation_state *construct(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<buffers_type> reqs) noexcept override
    {
      assert(storage.size() >= sizeof(_null_operation_state));
      assert(((uintptr_t) storage.data() % alignof(_null_operation_state)) == 0);
      if(storage.size() < sizeof(_null_operation_state) || ((uintptr_t) storage.data() % alignof(_null_operation_state)) != 0)
      {
        return nullptr;
      }
      return new(storage.data()) _null_operation_state(_h, _visitor, std::move(b), d, std::move(reqs));
    }
    virtual io_operation_state *construct(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<const_buffers_type> reqs) noexcept override
    {
      assert(storage.size() >= sizeof(_null_operation_state));
      assert(((uintptr_t) storage.data() % alignof(_null_operation_state)) == 0);
      if(storage.size() < sizeof(_null_operation_state) || ((uintptr_t) storage.data() % alignof(_null_operation_state)) != 0)
      {
        return nullptr;
      }
      return new(storage.data()) _null_operation_state(_h, _visitor, std::move(b), d, std::move(reqs));
    }
    virtual io_operation_state *construct(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<const_buffers_type> reqs, barrier_kind kind) noexcept override
    {
      assert(storage.size() >= sizeof(_null_operation_state));
      assert(((uintptr_t) storage.data() % alignof(_null_operation_state)) == 0);
      if(storage.size() < sizeof(_null_operation_state) || ((uintptr_t) storage.data() % alignof(_null_operation_state)) != 0)
      {
        return nullptr;
      }
      return new(storage.data()) _null_operation_state(_h, _visitor, std::move(b), d, std::move(reqs), kind);
    }

    // This initiates an i/o whose state was previously constructed
    virtual io_operation_state_type init_io_operation(io_operation_state *_op) noexcept override
    {
      auto *state = static_cast<_null_operation_state *>(_op);
      auto s = state->current_state();  // read the current state, holding the state's lock
      switch(s)
      {
      case io_operation_state_type::unknown:
        abort();
      case io_operation_state_type::read_initialised:
      {
        io_handle::io_result<io_handle::buffers_type> ret(state->payload.noncompleted.params.read.reqs.buffers);
        /* Try to eagerly complete the i/o now, if so ... */
        if(false /* completed immediately */)
        {
          state->read_completed(std::move(ret).value());
          if(!_disable_immediate_completions /* state is no longer in use by anyone else */)
          {
            state->read_finished();
            return io_operation_state_type::read_finished;
          }
          return io_operation_state_type::read_completed;
        }
        /* Otherwise the i/o has been initiated and will complete at some later point */
        state->read_initiated();
        _multiplexer_lock_guard g(this->_lock);
        _insert(state);
        return io_operation_state_type::read_initiated;
      }
      case io_operation_state_type::write_initialised:
      {
        io_handle::io_result<io_handle::const_buffers_type> ret(state->payload.noncompleted.params.write.reqs.buffers);
        if(false /* completed immediately */)
        {
          state->write_completed(std::move(ret).value());
          if(!_disable_immediate_completions /* state is no longer in use by anyone else */)
          {
            state->write_or_barrier_finished();
            return io_operation_state_type::write_or_barrier_finished;
          }
          return io_operation_state_type::write_or_barrier_completed;
        }
        state->write_initiated();
        _multiplexer_lock_guard g(this->_lock);
        _insert(state);
        return io_operation_state_type::write_initiated;
      }
      case io_operation_state_type::barrier_initialised:
      {
        io_handle::io_result<io_handle::const_buffers_type> ret(state->payload.noncompleted.params.write.reqs.buffers);
        if(false /* completed immediately */)
        {
          state->barrier_completed(std::move(ret).value());
          if(!_disable_immediate_completions /* state is no longer in use by anyone else */)
          {
            state->write_or_barrier_finished();
            return io_operation_state_type::write_or_barrier_finished;
          }
          return io_operation_state_type::write_or_barrier_completed;
        }
        state->write_initiated();
        _multiplexer_lock_guard g(this->_lock);
        _insert(state);
        return io_operation_state_type::barrier_initiated;
      }
      case io_operation_state_type::read_initiated:
      case io_operation_state_type::read_completed:
      case io_operation_state_type::read_finished:
      case io_operation_state_type::write_initiated:
      case io_operation_state_type::barrier_initiated:
      case io_operation_state_type::write_or_barrier_completed:
      case io_operation_state_type::write_or_barrier_finished:
        assert(false);
        break;
      }
      return s;
    }

    // If you can combine `construct()` with `init_io_operation()` into a more efficient implementation,
    // you should override these
    // virtual io_operation_state *construct_and_init_io_operation(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<buffers_type> reqs) noexcept override
    // virtual io_operation_state *construct_and_init_io_operation(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<const_buffers_type> reqs) noexcept override
    // virtual io_operation_state *construct_and_init_io_operation(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<const_buffers_type> reqs, barrier_kind kind) noexcept override

    // On some implementations, `init_io_operation()` just enqueues request packets, and
    // a separate operation is required to submit the enqueued list.
    virtual result<void> flush_inited_io_operations() noexcept override { return success(); }

    // This must check an individual i/o state, if it has completed or finished you must invoke its
    // visitor
    virtual io_operation_state_type check_io_operation(io_operation_state *_op) noexcept override
    {
      auto *state = static_cast<_null_operation_state *>(_op);
      typename io_operation_state::lock_guard g(state);
      if(state->count > 0)
      {
        --state->count;
      }
      if(state->count < 2)
      {
        switch(state->state)
        {
        case io_operation_state_type::unknown:
          abort();
        case io_operation_state_type::read_initialised:
        case io_operation_state_type::write_initialised:
        case io_operation_state_type::barrier_initialised:
          assert(false);
          break;
        case io_operation_state_type::read_initiated:
        {
          io_handle::io_result<io_handle::buffers_type> ret(state->payload.noncompleted.params.read.reqs.buffers);
          state->_read_completed(g, std::move(ret).value());
          if(_disable_immediate_completions)
          {
            return io_operation_state_type::read_completed;
          }
          state->_read_finished(g);
          _multiplexer_lock_guard g2(this->_lock);
          state->detach(this);
          state->count = 0;
          return io_operation_state_type::read_finished;
        }
        case io_operation_state_type::read_completed:
        {
          state->_read_finished(g);
          _multiplexer_lock_guard g2(this->_lock);
          state->detach(this);
          state->count = 0;
          return io_operation_state_type::read_finished;
        }
        case io_operation_state_type::write_initiated:
        {
          io_handle::io_result<io_handle::const_buffers_type> ret(state->payload.noncompleted.params.write.reqs.buffers);
          state->_write_completed(g, std::move(ret).value());
          if(_disable_immediate_completions)
          {
            return io_operation_state_type::write_or_barrier_completed;
          }
          state->_write_or_barrier_finished(g);
          _multiplexer_lock_guard g2(this->_lock);
          state->detach(this);
          state->count = 0;
          return io_operation_state_type::write_or_barrier_finished;
        }
        case io_operation_state_type::barrier_initiated:
        {
          io_handle::io_result<io_handle::const_buffers_type> ret(state->payload.noncompleted.params.barrier.reqs.buffers);
          state->_barrier_completed(g, std::move(ret).value());
          if(_disable_immediate_completions)
          {
            return io_operation_state_type::write_or_barrier_completed;
          }
          state->_write_or_barrier_finished(g);
          _multiplexer_lock_guard g2(this->_lock);
          state->detach(this);
          state->count = 0;
          return io_operation_state_type::write_or_barrier_finished;
        }
        case io_operation_state_type::write_or_barrier_completed:
        {
          state->_write_or_barrier_finished(g);
          _multiplexer_lock_guard g2(this->_lock);
          state->detach(this);
          state->count = 0;
          return io_operation_state_type::write_or_barrier_finished;
        }
        case io_operation_state_type::read_finished:
        case io_operation_state_type::write_or_barrier_finished:
          assert(false);
          break;
        }
      }
      return state->state;
    }

    // This attempts to cancel the in-progress i/o within the given deadline.
    virtual result<io_operation_state_type> cancel_io_operation(io_operation_state *_op, deadline d = {}) noexcept override
    {
      (void) d;
      auto *state = static_cast<_null_operation_state *>(_op);
      typename io_operation_state::lock_guard g(state);
      switch(state->state)
      {
      case io_operation_state_type::unknown:
        abort();
      case io_operation_state_type::read_initialised:
      case io_operation_state_type::write_initialised:
      case io_operation_state_type::barrier_initialised:
        assert(false);
        break;
      case io_operation_state_type::read_initiated:
      {
        io_handle::io_result<io_handle::buffers_type> ret(errc::operation_canceled);
        state->_read_completed(g, std::move(ret).value());
        state->count = 1;
        return io_operation_state_type::read_completed;
      }
      case io_operation_state_type::write_initiated:
      {
        io_handle::io_result<io_handle::const_buffers_type> ret(errc::operation_canceled);
        state->_write_completed(g, std::move(ret).value());
        state->count = 1;
        return io_operation_state_type::write_or_barrier_completed;
      }
      case io_operation_state_type::barrier_initiated:
      {
        io_handle::io_result<io_handle::const_buffers_type> ret(errc::operation_canceled);
        state->_barrier_completed(g, std::move(ret).value());
        state->count = 1;
        return io_operation_state_type::write_or_barrier_completed;
      }
      case io_operation_state_type::read_completed:
      {
        state->_read_finished(g);
        _multiplexer_lock_guard g2(this->_lock);
        state->detach(this);
        state->count = 0;
        return io_operation_state_type::read_finished;
      }
      case io_operation_state_type::write_or_barrier_completed:
      {
        state->_write_or_barrier_finished(g);
        _multiplexer_lock_guard g2(this->_lock);
        state->detach(this);
        state->count = 0;
        return io_operation_state_type::write_or_barrier_finished;
      }
      case io_operation_state_type::read_finished:
      case io_operation_state_type::write_or_barrier_finished:
        assert(false);
        break;
      }
      return state->state;
    }

    // This must check all i/o initiated or completed on this i/o multiplexer
    // and invoke state transition from initiated to completed/finished, or from
    // completed to finished, for no more than max_completions i/o states.
    virtual result<check_for_any_completed_io_statistics> check_for_any_completed_io(deadline d = std::chrono::seconds(0), size_t max_completions = (size_t) -1) noexcept override
    {
      LLFIO_DEADLINE_TO_SLEEP_INIT(d);
      check_for_any_completed_io_statistics ret;
      while(max_completions > 0)
      {
        _multiplexer_lock_guard g(this->_lock);
        // If another kernel thread woke me, exit the loop
        if(_wakecount > 0)
        {
          --_wakecount;
          break;
        }
        auto *c = _first;
        if(c != nullptr)
        {
          // Move from front to back
          c->detach(this);
          assert(c != _first);
          _insert(c);
        }
        g.unlock();
        if(c == nullptr)
        {
          std::this_thread::yield();
        }
        else
        {
          // Check the i/o for completion
          auto s = check_io_operation(c);
          if(is_completed(s))
          {
            ++ret.initiated_ios_completed;
          }
          else if(is_finished(s))
          {
            ++ret.initiated_ios_finished;
          }
        }
        // If the timeout has been exceeded, exit the loop
        if(d.nsecs == 0 || !([&]() -> result<void> {
             LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
             return success();
           })())
        {
          break;
        }
        --max_completions;
      }
      return ret;
    }

    // This can be used from any kernel thread to cause a check_for_any_completed_io()
    // running in another kernel thread to return early
    virtual result<void> wake_check_for_any_completed_io() noexcept override
    {
      _multiplexer_lock_guard g(this->_lock);
      ++_wakecount;
      return success();
    }
  };

  LLFIO_HEADERS_ONLY_FUNC_SPEC result<io_multiplexer_ptr> multiplexer_null(size_t threads, bool disable_immediate_completions) noexcept
  {
    try
    {
      if(1 == threads)
      {
        auto ret = std::make_unique<linux_io_uring_multiplexer<false>>();
        OUTCOME_TRY(ret->init(1, disable_immediate_completions));
        return io_multiplexer_ptr(ret.release());
      }
      auto ret = std::make_unique<linux_io_uring_multiplexer<true>>();
      OUTCOME_TRY(ret->init(threads, disable_immediate_completions));
      return io_multiplexer_ptr(ret.release());
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
}  // namespace test

LLFIO_V2_NAMESPACE_END
