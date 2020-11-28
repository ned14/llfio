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
#include <sched.h>
#include <sys/syscall.h>

LLFIO_V2_NAMESPACE_BEGIN

namespace test
{
  /* io_uring is a bit of an interesting design, so we've ended up with a rather
  unusual i/o multiplexer design, which wasn't anticipated when we began this.

  POSIX provides strong read/write concurrency guarantees which are valuable, and
  more importantly, lots of file i/o code hard-assumes (often unintentionally) that
  there is an implicit sequencing of all i/o issued against each inode. This arose,
  historically speaking, because each inode has a read-write mutex, and i/o upon
  that inode therefore was serialised by that mutex in the kernel.

  Just to be clear, POSIX's guarantees are weaker than this - i/o upon non-overlapping
  regions can parallelise embarrassingly. However, if concurrent i/o upon the same
  inode overlaps a region, each i/o must complete as an atomic operation with
  respect to other i/o operations. And given how fast i/o can be, spending CPU on
  figuring out if regions overlap is usually more expensive than just using a per-inode
  read-write mutex.

  Conformance to POSIX read/write concurrency guarantees is excellent on Windows and
  all POSIX, except for Linux ext4 without O_DIRECT. However, io_uring doesn't
  expose any of this for file i/o - i/o submitted is immediately initiated, and no
  ordering is implemented at all. i.e. it's on you, the io_uring user, to not submit
  i/o the concurrency of which would be problematic. This even extends to IORING_OP_FSYNC,
  which will complete without reordering constraints to any i/o initiated beforehand
  or afterwards.

  io_uring *does* provide completion ordering *per-queue* via the IOSQE_IO_DRAIN and
  the IOSQE_IO_LINK flags. The former allows reordering of all i/o before the drain,
  but all that i/o must complete before the IOSQE_IO_DRAIN flagged submission can
  begin (this equals fence semantics). The latter imposes sequentially consistent
  ordering in that each item in the chain must complete before the next item,
  however individual chains can be reordered against one another.

  If one wishes to implement POSIX read/write concurrency guarantees,
  then one needs to enforce an ordering per-inode, which because io_uring only offers
  ordering at a per-queue level, implies that there must be either a queue per inode,
  or all file i/o must be sequentially orderered to all other file i/o i.e. you use
  a fully sequentially ordered queue for file i/o, and a separate freely reordered
  queue for non-file i/o.

  What we've thus done for this test i/o multiplexer is this:

  - If the handle type is seekable, each write submitted sets IOSQE_IO_DRAIN for that
  submissed entry. This forces all reads preceding that submission to complete
  beforehand, and requires the write to complete before subsequent operations can
  begin.

  - If the handle type is not seekable, all initiated i/o enters a queue per handle.
  As each i/o completes, the next i/o from the queue is submitted.

  - Two io_uring instances are used, one for seekable i/o, the other for non-seekable
  i/o. This prevents writes to seekable handles blocking until non-seekable i/o completes.

  Todo list:

  - Timeouts implementation

  - Registered i/o buffers

  */
  template <bool is_threadsafe> class linux_io_uring_multiplexer final : public io_multiplexer_impl<is_threadsafe>
  {
    friend LLFIO_HEADERS_ONLY_FUNC_SPEC result<io_multiplexer_ptr> multiplexer_linux_io_uring(size_t threads, bool is_polling) noexcept;

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
      uint8_t opcode;  /* type of operation for this sqe */
      uint8_t flags;   /* IOSQE_ flags */
      uint16_t ioprio; /* ioprio for the request */
      int32_t fd;      /* file descriptor to do IO on */
      union {
        uint64_t off; /* offset into file */
        uint64_t addr2;
      };
      union {
        uint64_t addr; /* pointer to buffer or iovecs */
        uint64_t splice_off_in;
      };
      uint32_t len; /* buffer size or number of iovecs */
      union {
        __kernel_rwf_t rw_flags;
        uint32_t fsync_flags;
        uint16_t poll_events;
        uint32_t sync_range_flags;
        uint32_t msg_flags;
        uint32_t timeout_flags;
        uint32_t accept_flags;
        uint32_t cancel_flags;
        uint32_t open_flags;
        uint32_t statx_flags;
        uint32_t fadvise_advice;
        uint32_t splice_flags;
      };
      uint64_t user_data; /* data to be passed back at completion time */
      union {
        struct
        {
          /* pack this to avoid bogus arm OABI complaints */
          union {
            /* index into fixed buffers, if used */
            uint16_t buf_index;
            /* for grouped buffer selection */
            uint16_t buf_group;
          } __attribute__((packed));
          /* personality to use, if used */
          uint16_t personality;
          int32_t splice_fd_in;
        };
        uint64_t __pad2[3];
      };
    };

    // sqe->flags
    /* use fixed fileset */
    static constexpr uint32_t _IOSQE_FIXED_FILE = (1U << 0);
    /* issue after inflight IO */
    static constexpr uint32_t _IOSQE_IO_DRAIN = (1U << 1);
    /* links next sqe */
    static constexpr uint32_t _IOSQE_IO_LINK = (1U << 2);
    /* like LINK, but stronger */
    static constexpr uint32_t _IOSQE_IO_HARDLINK = (1U << 3);
    /* always go async */
    static constexpr uint32_t _IOSQE_ASYNC = (1U << 4);
    /* select buffer from sqe->buf_group */
    static constexpr uint32_t _IOSQE_BUFFER_SELECT = (1U << 5);

    // io_uring_setup() flags
    static constexpr uint32_t _IORING_SETUP_IOPOLL = (1U << 0);    /* io_context is polled */
    static constexpr uint32_t _IORING_SETUP_SQPOLL = (1U << 1);    /* SQ poll thread */
    static constexpr uint32_t _IORING_SETUP_SQ_AFF = (1U << 2);    /* sq_thread_cpu is valid */
    static constexpr uint32_t _IORING_SETUP_CQSIZE = (1U << 3);    /* app defines CQ size */
    static constexpr uint32_t _IORING_SETUP_CLAMP = (1U << 4);     /* clamp SQ/CQ ring sizes */
    static constexpr uint32_t _IORING_SETUP_ATTACH_WQ = (1U << 5); /* attach to existing wq */

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
    static constexpr uint32_t _IORING_FSYNC_DATASYNC = (1U << 0);

    // sqe->timeout_flags
    static constexpr uint32_t _IORING_TIMEOUT_ABS = (1U << 0);

    /*
     * sqe->splice_flags
     * extends splice(2) flags
     */
    static constexpr uint32_t _SPLICE_F_FD_IN_FIXED = (1U << 31); /* the last bit of uint32_t */


    // The io_uring kernel completion structure
    struct _io_uring_cqe
    {
      uint64_t user_data; /* sqe->data submission passed back */
      int32_t res;        /* result code for this event */
      uint32_t flags;
    };

    // cqe->flags
    // IORING_CQE_F_BUFFER	If set, the upper 16 bits are the buffer ID
    static constexpr uint32_t _IORING_CQE_F_BUFFER = (1U << 0);

    static constexpr uint32_t _IORING_CQE_BUFFER_SHIFT = 16;

    // Magic offsets for the application to mmap the data it needs
    static constexpr size_t _IORING_OFF_SQ_RING = (size_t) 0;
    static constexpr size_t _IORING_OFF_CQ_RING = (size_t) 0x8000000;
    static constexpr size_t _IORING_OFF_SQES = (size_t) 0x10000000;

    // Filled with the offset for mmap(2)
    struct _io_sqring_offsets
    {
      uint32_t head;
      uint32_t tail;
      uint32_t ring_mask;
      uint32_t ring_entries;
      uint32_t flags;
      uint32_t dropped;
      uint32_t array;
      uint32_t resv1;
      uint64_t resv2;
    };

    // sq_ring->flags
    static constexpr uint32_t _IORING_SQ_NEED_WAKEUP = (1U << 0); /* needs io_uring_enter wakeup */

    struct _io_cqring_offsets
    {
      uint32_t head;
      uint32_t tail;
      uint32_t ring_mask;
      uint32_t ring_entries;
      uint32_t overflow;
      uint32_t cqes;
      uint64_t resv[2];
    };

    // io_uring_enter(2) flags
    static constexpr uint32_t _IORING_ENTER_GETEVENTS = (1U << 0);
    static constexpr uint32_t _IORING_ENTER_SQ_WAKEUP = (1U << 1);

    // Passed in for io_uring_setup(2). Copied back with updated info on success
    struct _io_uring_params
    {
      uint32_t sq_entries;
      uint32_t cq_entries;
      uint32_t flags;
      uint32_t sq_thread_cpu;
      uint32_t sq_thread_idle;
      uint32_t features;
      uint32_t wq_fd;
      uint32_t resv[3];
      struct _io_sqring_offsets sq_off;
      struct _io_cqring_offsets cq_off;
    };

    // io_uring_params->features flags
    static constexpr uint32_t _IORING_FEAT_SINGLE_MMAP = (1U << 0);
    static constexpr uint32_t _IORING_FEAT_NODROP = (1U << 1);
    static constexpr uint32_t _IORING_FEAT_SUBMIT_STABLE = (1U << 2);
    static constexpr uint32_t _IORING_FEAT_RW_CUR_POS = (1U << 3);
    static constexpr uint32_t _IORING_FEAT_CUR_PERSONALITY = (1U << 4);
    static constexpr uint32_t _IORING_FEAT_FAST_POLL = (1U << 5);

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
      uint32_t offset;
      uint32_t resv;
      __aligned_u64 /* int32_t * */ fds;
    };

    static constexpr uint32_t _IO_URING_OP_SUPPORTED = (1U << 0);

    struct _io_uring_probe_op
    {
      uint8_t op;
      uint8_t resv;
      uint16_t flags; /* IO_URING_OP_* flags */
      uint32_t resv2;
    };

    struct _io_uring_probe
    {
      uint8_t last_op; /* last opcode supported */
      uint8_t ops_len; /* length of ops[] array below */
      uint16_t resv;
      uint32_t resv2[3];
      struct _io_uring_probe_op ops[0];
    };

    template <class T> static T _io_uring_smp_load_acquire(const T &_v) noexcept
    {
      auto *v = (const std::atomic<T> *) &_v;
      return v->load(std::memory_order_acquire);
    }
    template <class T> static void _io_uring_smp_store_release(T &_v, T x) noexcept
    {
      auto *v = (std::atomic<T> *) &_v;
      v->store(x, std::memory_order_release);
    }
    static int _io_uring_setup(unsigned entries, struct _io_uring_params *p)
    {
#ifdef __alpha__
      return syscall(535 /*__NR_io_uring_setup*/, entries, p);
#else
      return syscall(425 /*__NR_io_uring_setup*/, entries, p);
#endif
    }
    static int _io_uring_register(int fd, unsigned opcode, void *arg, unsigned nr_args)
    {
#ifdef __alpha__
      return syscall(537 /*__NR_io_uring_register*/, fd, opcode, arg, nr_args);
#else
      return syscall(427 /*__NR_io_uring_register*/, fd, opcode, arg, nr_args);
#endif
    }
    static int _io_uring_enter(int fd, unsigned to_submit, unsigned min_complete, unsigned flags, sigset_t *sig)
    {
#ifdef __alpha__
      return syscall(536 /*__NR_io_uring_enter*/, fd, to_submit, min_complete, flags, sig, _NSIG / 8);
#else
      return syscall(426 /*__NR_io_uring_enter*/, fd, to_submit, min_complete, flags, sig, _NSIG / 8);
#endif
    }
    static int _io_uring_enter(int fd, unsigned to_submit, unsigned min_complete, unsigned flags)
    {
#ifdef __alpha__
      return syscall(536 /*__NR_io_uring_enter*/, fd, to_submit, min_complete, flags, nullptr, 0);
#else
      return syscall(426 /*__NR_io_uring_enter*/, fd, to_submit, min_complete, flags, nullptr, 0);
#endif
    }


    struct _io_uring_operation_state final : public std::conditional_t<is_threadsafe, typename _base::_synchronised_io_operation_state, typename _base::_unsynchronised_io_operation_state>
    {
      using _impl = std::conditional_t<is_threadsafe, typename _base::_synchronised_io_operation_state, typename _base::_unsynchronised_io_operation_state>;

      _io_uring_operation_state *prev{nullptr}, *next{nullptr};
      // These are cached here from the handle for performance
      int fd{-1};
      bool is_seekable{false};
      bool submitted_to_iouring{false};

      _io_uring_operation_state() = default;
      // Construct implicitly from the base implementation, see relocate_to()
      explicit _io_uring_operation_state(_impl &&o) noexcept
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
        auto _to = new(to) _io_uring_operation_state(std::move(*static_cast<_impl *>(to)));
        _to->fd = fd;
        _to->is_seekable = is_seekable;
        _to->submitted_to_iouring = submitted_to_iouring;
        return _to;
      }
    };

    const bool _is_polling{false};
    bool _have_ioring_register_files_update{true};  // track if this Linux kernel implements IORING_REGISTER_FILES_UPDATE
    int _seekable_iouring_fd{-1};
    struct _submission_completion_t
    {
      struct submission_t
      {
        std::atomic<uint32_t> *head{nullptr}, *tail{nullptr}, *flags{nullptr}, *dropped{nullptr};
        uint32_t ring_mask{0}, ring_entries{0}; 
        span<uint32_t> region;  // refers to the mmapped region, used to munmap on close
        span<_io_uring_sqe> entries;
        span<uint32_t> array;
      } submission;
      struct completion_t
      {
        std::atomic<uint32_t> *head{nullptr}, *tail{nullptr}, *overflow{nullptr};
        uint32_t ring_mask{0}, ring_entries{0};
        span<uint32_t> region;  // refers to the mmapped region, used to munmap on close
        span<_io_uring_cqe> entries;
      } completion;
    } _nonseekable, _seekable;
    struct _registered_fd
    {
      const int fd{-1};
      struct queue_t
      {
        _io_uring_operation_state *first{nullptr}, *last{nullptr};
      };
      // contains initiated i/o not yet submitted to io_uring. state->submitted_to_iouring will be false.
      queue_t enqueued_io;
      // contains initiated i/o submitted to io_uring. state->submitted_to_iouring will be true.
      struct
      {
        // For seekable devices, there can be multiple, concurrent, reads.
        // For non-seekable devices, there is only one read submitted per file descriptor at a time.
        queue_t reads;
        // Only a single write/barrier, per file descriptor is submitted at a time.
        _io_uring_operation_state *write_or_barrier{nullptr};
      } inprogress;

      explicit _registered_fd(io_handle &h)
          : fd(h.native_handle().fd)
      {
      }
      bool operator<(const _registered_fd &o) const noexcept { return fd < o.fd; }
    };
    /* If this were a real, and not test, implementation this would be a hash table
    and there would be separate tables for registered fds with i/o pending and
    needing submitted. This would avoid a linear scan of the whole table per i/o pump.
    */
    std::vector<_registered_fd> _registered_fds;  // ordered by fd so can be binary searched
    std::vector<registered_buffer_type> _registered_buffers;

    typename std::vector<_registered_fd>::iterator _find_fd(int fd) const
    {
      auto ret = std::lower_bound(_registered_fds.begin(), _registered_fds.end(), fd);
      assert(fd == ret->fd);
      return ret;
    }
    static void _enqueue_to(typename _registered_fd::queue_t &queue, _io_uring_operation_state *state)
    {
      assert(state->prev == nullptr);
      assert(state->next == nullptr);
      assert(queue.first != state);
      assert(queue.last != state);
      if(queue.first == nullptr)
      {
        queue.first = queue.last = state;
      }
      else
      {
        assert(queue.last->next == nullptr);
        state->prev = queue.last;
        queue.last->next = state;
        queue.last = state;
      }
    }
    static _io_uring_operation_state *_dequeue_from(typename _registered_fd::queue_t &queue, _io_uring_operation_state *state)
    {
      _io_uring_operation_state *ret = queue.first;
      if(state->prev == nullptr)
      {
        assert(queue.first == this);
        queue.first = state->next;
      }
      else
      {
        state->prev->next = state->next;
      }
      if(state->next == nullptr)
      {
        assert(queue.last == this);
        queue.last = state->prev;
      }
      else
      {
        state->next->prev = state->prev;
      }
      state->next = state->prev = nullptr;
      return ret;
    }
    void _pump(_multiplexer_lock_guard &g)
    {
      // Drain completions first
      auto drain_completions = [&](_submission_completion_t &inst) { 
        for(;;)
        {
          const uint32_t head = inst.completion.head->load(std::memory_order_acquire);
          if(head == inst.completion.tail->load(std::memory_order_relaxed))
          {
            break;
          }
          const _io_uring_cqe *cqe = &inst.completion.entries[head & inst.completion.ring_mask];
          if(cqe->user_data == nullptr)
          {
            // A wakeup
            continue;
          }
          auto *state = (_io_uring_operation_state *) (uintptr_t) cqe->user_data;
          assert(state->submitted_to_iouring);
          assert(is_initiated(state->state));
          auto it = _find_fd(state->fd);
          assert(it != _registered_fds.end());
          switch(state->state)
          {
          default:
            abort();
          case io_operation_state_type::read_initiated:
          {
            _dequeue_from(it->inprogress.reads, state);
            auto &reqs = state->payload.noncompleted.params.read.reqs;
            io_handle::io_result<io_handle::buffers_type> ret(reqs.buffers);
            if(cqe->res < 0)
            {
              ret = posix_error(-cqe->res);
            }
            else
            {
              size_t bytesread = cqe->res;
              for(size_t i = 0; i < reqs.buffers.size(); i++)
              {
                auto &buffer = reqs.buffers[i];
                if(buffer.size() <= static_cast<size_t>(bytesread))
                {
                  bytesread -= buffer.size();
                }
                else
                {
                  buffer = {buffer.data(), (size_type) bytesread};
                  reqs.buffers = {reqs.buffers.data(), i + 1};
                  break;
                }
              }
            }
            g.unlock();
            state->read_completed(std::move(ret));
            g.lock();
            break;
          }
          case io_operation_state_type::write_initiated:
          {
            assert(it->inprogress.write_or_barrier == state);
            it->inprogress.write_or_barrier = nullptr;
            auto &reqs = state->payload.noncompleted.params.write.reqs;
            io_handle::io_result<io_handle::const_buffers_type> ret(reqs.buffers);
            if(cqe->res < 0)
            {
              ret = posix_error(-cqe->res);
            }
            else
            {
              size_t bytesread = cqe->res;
              for(size_t i = 0; i < reqs.buffers.size(); i++)
              {
                auto &buffer = reqs.buffers[i];
                if(buffer.size() <= static_cast<size_t>(bytesread))
                {
                  bytesread -= buffer.size();
                }
                else
                {
                  buffer = {buffer.data(), (size_type) bytesread};
                  reqs.buffers = {reqs.buffers.data(), i + 1};
                  break;
                }
              }
            }
            g.unlock();
            state->write_completed(std::move(ret));
            g.lock();
            break;
          }
          case io_operation_state_type::barrier_initiated:
          {
            assert(it->inprogress.write_or_barrier == state);
            it->inprogress.write_or_barrier = nullptr;
            auto &reqs = state->payload.noncompleted.params.barrier.reqs;
            io_handle::io_result<io_handle::const_buffers_type> ret(reqs.buffers);
            if(cqe->res < 0)
            {
              ret = posix_error(-cqe->res);
            }
            else
            {
              size_t bytesread = cqe->res;
              for(size_t i = 0; i < reqs.buffers.size(); i++)
              {
                auto &buffer = reqs.buffers[i];
                if(buffer.size() <= static_cast<size_t>(bytesread))
                {
                  bytesread -= buffer.size();
                }
                else
                {
                  buffer = {buffer.data(), (size_type) bytesread};
                  reqs.buffers = {reqs.buffers.data(), i + 1};
                  break;
                }
              }
            }
            g.unlock();
            state->barrier_completed(std::move(ret));
            g.lock();
            break;
          }
          }
          inst.completion.head->fetch_add(1, std::memory_order_release);
        }
      };
      drain_completions(_nonseekable);
      if(-1 != _seekable_iouring_fd)
      {
        drain_completions(_seekable);
      }

      auto enqueue_submissions = [&](_submission_completion_t &inst) {
        for(auto &rfd : _registered_fds)
        {
          if(rfd.inprogress.reads.first == nullptr && rfd.inprogress.write_or_barrier == nullptr)
          {
            bool enqueue_more = true;
            while(rfd.enqueued_io.first != nullptr && enqueue_more)
            {
              // This registered fd has no i/o in progress but does have i/o enqueued
              auto *state = _dequeue_from(rfd.enqueued_io);
              assert(!state->submitted_to_iouring);
              assert(is_initiated(state->state));

              const uint32_t tail = inst.submission.tail->load(std::memory_order_acquire);
              if(tail == inst.submission.head->load(std::memory_order_relaxed))
              {
                break;
              }
              const uint32_t sqeidx = tail & inst.submission.ring_mask;
              _io_uring_sqe *sqe = &inst.submission.entries[sqeidx];
              auto s = state->current_state();
              bool enqueue_more = false;
              memset(sqe, 0, sizeof(_io_uring_sqe));
              sqe->fd = state->fd;
              sqe->user_data = state;
              switch(s)
              {
              default:
                abort();
              case io_operation_state_type::read_initialised:
              {
                sqe->opcode = _IORING_OP_READV;  // TODO: _IORING_OP_READ_FIXED into registered i/o buffers
                sqe->off = state->noncompleted.params.read.reqs.offset;
                sqe->addr = state->noncompleted.params.read.reqs.buffers.data();
                sqe->len = state->noncompleted.params.read.reqs.buffers.size();
                _enqueue_to(rfd.inprogress.reads, state);
                // If this is a read upon a seekable handle, keep enqueuing
                enqueue_more = state->is_seekable;
                break;
              }
              case io_operation_state_type::write_initialised:
              {
                sqe->opcode = _IORING_OP_WRITEV;  // TODO: _IORING_OP_WRITE_FIXED into registered i/o buffers
                if(state->is_seekable)
                {
                  sqe->flags = _IOSQE_IO_DRAIN;  // Drain all preceding reads before doing the write, and don't start anything new until this completes
                }
                sqe->off = state->noncompleted.params.write.reqs.offset;
                sqe->addr = state->noncompleted.params.write.reqs.buffers.data();
                sqe->len = state->noncompleted.params.write.reqs.buffers.size();
                rfd.inprogress.write_or_barrier = state;
                break;
              }
              case io_operation_state_type::barrier_initialised:
              {
                // Drain all preceding writes before doing the barrier, and don't start anything new until this completes
                sqe->flags = _IOSQE_IO_DRAIN;  
                if(state->noncompleted.params.barrier.kind <= barrier_kind::wait_data_only)
                {
                  // Linux has a lovely dedicated syscall giving us exactly what we need here
                  sqe->opcode = _IORING_OP_SYNC_FILE_RANGE;
                  sqe->off = state->noncompleted.params.write.reqs.offset;
                  // empty buffers means bytes = 0 which means sync entire file
                  for(const auto &req : state->noncompleted.params.write.reqs.buffers)
                  {
                    sqe->len += req.size();
                  }
                  sqe->sync_range_flags = SYNC_FILE_RANGE_WRITE;  // start writing all dirty pages in range now
                  if(state->noncompleted.params.barrier.kind == barrier_kind::wait_data_only)
                  {
                    sqe->sync_range_flags |= SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WAIT_AFTER;  // block until they're on storage
                  }
                }
                else
                {
                  sqe->opcode = _IORING_OP_FSYNC;
                }
                rfd.inprogress.write_or_barrier = state;
                break;
              }
              }
              inst.submission.array[sqeidx] = sqeidx;
              inst.submission.tail->fetch_add(1, std::memory_order_release);
              state->submitted_to_iouring = true;
            }
          }
        }
      };
      enqueue_submissions(_nonseekable);
      if(-1 != _seekable_iouring_fd)
      {
        enqueue_submissions(_seekable);
      }
    }

  public:
    constexpr explicit linux_io_uring_multiplexer(bool is_polling)
        : _is_polling(is_polling)
    {
      _registered_fds.reserve(4);
    }
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
    result<void> init(bool is_seekable, _submission_completion_t &out)
    {
      int fd = -1;
      _io_uring_params params;
      memset(&params, 0, sizeof(params));
      if(_is_polling)
      {
        // We don't implement IORING_SETUP_IOPOLL, it is for O_DIRECT files only in any case
        params.flags |= _IORING_SETUP_SQPOLL;
        params.sq_thread_idle = 100;  // 100 milliseconds
        if(!is_threadsafe)
        {
          // Pin kernel submission polling thread to same CPU as I am pinned to, if I am pinned
          cpu_set_t affinity;
          CPU_ZERO(&affinity);
          if(-1 != ::sched_getaffinity(0, sizeof(affinity), &affinity) && CPU_COUNT(&affinity) == 1)
          {
            for(size_t n = 0; n < CPU_SETSIZE; n++)
            {
              if(CPU_ISSET(n, &affinity))
              {
                params.flags |= _IORING_SETUP_SQ_AFF;
                params.sq_thread_cpu = n;
                break;
              }
            }
          }
        }
      }
      // 64 items is 4Kb of sqe entries. Given the binary searched registered fd table, more than
      // this would not be useful.
      fd = _io_uring_setup(64, &params);
      if(fd < 0)
      {
        return posix_error();
      }
      {
        auto *p = ::mmap(nullptr, params.sq_off.array + params.sq_entries * sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, _IORING_OFF_SQ_RING);
        if(p == nullptr)
        {
          return posix_error();
        }
        out.submission.region = {(uint32_t *) p, (params.sq_off.array + params.sq_entries * sizeof(uint32_t)) / sizeof(uint32_t)};
        out.head = &out.submission.region[params.sq_off.head / sizeof(uint32_t)];
        out.tail = &out.submission.region[params.sq_off.tail / sizeof(uint32_t)];
        out.ring_mask = out.submission.region[params.sq_off.ring_mask / sizeof(uint32_t)];
        out.ring_entries = out.submission.region[params.sq_off.ring_entries / sizeof(uint32_t)];
        out.flags = out.submission.region[params.sq_off.flags / sizeof(uint32_t)];
        out.dropped = &out.submission.region[params.sq_off.dropped / sizeof(uint32_t)];
        out.array = {
        &out.submission.region[params.sq_off.array / sizeof(uint32_t)], params.sq_entries};
      }
      {
        auto *p = ::mmap(nullptr, params.sq_entries * sizeof(_io_uring_sqe), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, _IORING_OFF_SQES);
        if(p == nullptr)
        {
          return posix_error();
        }
        out.submission.entries = {(_io_uring_sqe *) p, params.sq_entries};
      }
      {
        auto *p = ::mmap(nullptr, params.cq_off.cqes + params.cq_entries * sizeof(_io_uring_cqe), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, _IORING_OFF_CQ_RING);
        if(p == nullptr)
        {
          return posix_error();
        }
        out.completion.region = {(uint32_t *) p, (params.cq_off.cqes + params.cq_entries * sizeof(_io_uring_cqe)) / sizeof(uint32_t)};
        out.head = &out.completion.region[params.cq_off.head / sizeof(uint32_t)];
        out.tail = &out.completion.region[params.cq_off.tail / sizeof(uint32_t)];
        out.ring_mask = out.completion.region[params.cq_off.ring_mask / sizeof(uint32_t)];
        out.ring_entries = out.completion.region[params.cq_off.ring_entries / sizeof(uint32_t)];
        out.overflow = out.completion.region[params.cq_off.overflow / sizeof(uint32_t)];
        out.completion.entries = {(_io_uring_cqe *) &out.completion.region[params.cq_off.cqes / sizeof(uint32_t)], params.cq_entries};
      }
      if(is_seekable)
      {
        _seekable_iouring_fd = fd;
      }
      else
      {
        this->_v.fd = fd;
        this->_v.behaviour |= native_handle_type::disposition::multiplexer;
      }
      return success();
    }

    // These functions are inherited from handle
    // virtual result<path_type> current_path() const noexcept override
    virtual result<void> close() noexcept override
    {
      auto do_close = [this](auto &s) {
        if(!s.submission.region.empty())
        {
          if(-1 == ::munmap(s.submission.region.data(), s.submission.region.size_bytes()))
          {
            return posix_error();
          }
          s.submission.region = {};
        }
        if(!s.submission.entries.empty())
        {
          if(-1 == ::munmap(s.submission.entries.data(), s.submission.entries.size_bytes()))
          {
            return posix_error();
          }
          s.submission.entries = {};
        }
        if(!s.completion.region.empty())
        {
          if(-1 == ::munmap(s.completion.region.data(), s.completion.region.size_bytes()))
          {
            return posix_error();
          }
          s.completion.region = {};
        }
      };
      if(-1 != _seekable_iouring_fd)
      {
        do_close(_seekable);
        if(-1 == ::close(_seekable_iouring_fd))
        {
          return posix_error();
        }
        _seekable_iouring_fd = -1;
      }
      do_close(_nonseekable);
      OUTCOME_TRY(_base::close());
      _registered_fds.clear();
      _registered_buffers.clear();
      return success();
    }
    virtual native_handle_type release() noexcept override
    {
      native_handle_type ret = this->_v;
      this->_v = {};
      (void) close();
      _base::release();
      return ret;
    }

    result<void> _change_fd_registration(bool is_seekable, int fd, bool enable) noexcept
    {
      if(_have_ioring_register_files_update)
      {
        int32_t newvalue = enable ? fd : -1;
        _io_uring_files_update upd;
        memset(&upd, 0, sizeof(upd));
        upd.offset = fd;
        upd.fds = (__aligned_u64) &newvalue;
        if(_io_uring_register(is_seekable ? _seekable_iouring_fd : this->_v.fd, _IORING_REGISTER_FILES_UPDATE, &upd, 1) >= 0)  // Linux kernel 5.5 onwards
        {
          return success();
        }
        // Failed, so disable ever calling this again
        _have_ioring_register_files_update = false;
      }
      return success();
    }
    virtual result<uint8_t> do_io_handle_register(io_handle *h) noexcept override  // linear complexity to total handles registered
    {
      _multiplexer_lock_guard g(this->_lock);
      if(h->is_seekable() && -1 == _seekable_iouring_fd)
      {
        // Create the seekable io_uring ring
        OUTCOME_TRY(init(true, _seekable));
      }
      _registered_fd toinsert(h);
      _registered_fds.insert(std::lower_bound(_registered_fds.begin(), _registered_fds.end(), toinsert), toinsert);
      return _change_fd_registration(h->is_seekable(), toinsert.fd, true);
    }
    virtual result<void> do_io_handle_deregister(io_handle *h) noexcept override
    {
      _multiplexer_lock_guard g(this->_lock);
      int fd = h->native_handle().fd;
      auto it = _find_fd(fd);
      assert(it->inprogress.reads.first == nullptr);
      assert(it->inprogress.write_or_barrier == nullptr);
      if(it->inprogress.reads.first != nullptr || it->inprogress.write_or_barrier != nullptr)
      {
        // Can't deregister a handle with i/o in progress
        return errc::operation_in_progress;
      }
      _registered_fds.erase(it);
      return _change_fd_registration(h->is_seekable(), fd, false);
    }

    virtual size_t do_io_handle_max_buffers(const io_handle * /*unused*/) const noexcept override { return IOV_MAX; }

    virtual result<registered_buffer_type> do_io_handle_allocate_registered_buffer(io_handle *h, size_t &bytes) noexcept override
    {
      _multiplexer_lock_guard g(this->_lock);
      // Try to reuse any previously registered buffers no longer in use, as
      // registered buffer registration is one-way in io_uring
      for(auto &b : _registered_buffers)
      {
        if(b.use_count() == 1)
        {
          return b;
        }
      }
      // The default implementation uses mmap, so this is done for us
      OUTCOME_TRY(auto &&ret, _base::do_io_handle_allocate_registered_buffer(h, bytes));
      // Register this buffer with io_uring
      struct iovec upd;
      upd.iov_base = ret->data();
      upd.iov_len = ret->size();
      if(_io_uring_register(this->_v.fd, _IORING_REGISTER_BUFFERS, &upd, 1) < 0)
      {
        return posix_error();
      }
      _registered_buffers.push_back(ret);
      return result<registered_buffer_type>(std::move(ret));
    }

    // io_uring has very minimal i/o state requirements
    virtual std::pair<size_t, size_t> io_state_requirements() noexcept override { return {sizeof(_io_uring_operation_state), alignof(_io_uring_operation_state)}; }

    virtual io_operation_state *construct(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<buffers_type> reqs) noexcept override
    {
      assert(storage.size() >= sizeof(_io_uring_operation_state));
      assert(((uintptr_t) storage.data() % alignof(_io_uring_operation_state)) == 0);
      if(storage.size() < sizeof(_io_uring_operation_state) || ((uintptr_t) storage.data() % alignof(_io_uring_operation_state)) != 0)
      {
        return nullptr;
      }
      return new(storage.data()) _io_uring_operation_state(_h, _visitor, std::move(b), d, std::move(reqs));
    }
    virtual io_operation_state *construct(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<const_buffers_type> reqs) noexcept override
    {
      assert(storage.size() >= sizeof(_io_uring_operation_state));
      assert(((uintptr_t) storage.data() % alignof(_io_uring_operation_state)) == 0);
      if(storage.size() < sizeof(_io_uring_operation_state) || ((uintptr_t) storage.data() % alignof(_io_uring_operation_state)) != 0)
      {
        return nullptr;
      }
      return new(storage.data()) _io_uring_operation_state(_h, _visitor, std::move(b), d, std::move(reqs));
    }
    virtual io_operation_state *construct(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<const_buffers_type> reqs, barrier_kind kind) noexcept override
    {
      assert(storage.size() >= sizeof(_io_uring_operation_state));
      assert(((uintptr_t) storage.data() % alignof(_io_uring_operation_state)) == 0);
      if(storage.size() < sizeof(_io_uring_operation_state) || ((uintptr_t) storage.data() % alignof(_io_uring_operation_state)) != 0)
      {
        return nullptr;
      }
      return new(storage.data()) _io_uring_operation_state(_h, _visitor, std::move(b), d, std::move(reqs), kind);
    }

    virtual io_operation_state_type init_io_operation(io_operation_state *_op) noexcept override
    {
      auto *state = static_cast<_io_uring_operation_state *>(_op);
      auto s = state->current_state();  // read the current state, holding the state's lock
      if(!is_initialised(s))
      {
        assert(false);
        return s;
      }
      switch(s)
      {
      case io_operation_state_type::read_initialised:
      {
        state->read_initiated();
        break;
      }
      case io_operation_state_type::write_initialised:
      {
        state->write_initiated();
        break;
      }
      case io_operation_state_type::barrier_initialised:
      {
        state->barrier_initiated();
        break;
      }
      }
      state->fd = state->h->native_handle().fd;
      state->is_seekable = state->h->is_seekable();
      assert(state->submitted_to_iouring == false);
      _multiplexer_lock_guard g(this->_lock);
      auto it = _find_fd(state->fd);
      assert(it != _registered_fds.end());
      _enqueue_to(it->enqueued_io, state);
      return state->state;
    }

    // virtual io_operation_state *construct_and_init_io_operation(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<buffers_type> reqs) noexcept override
    // virtual io_operation_state *construct_and_init_io_operation(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<const_buffers_type> reqs) noexcept override
    // virtual io_operation_state *construct_and_init_io_operation(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<const_buffers_type> reqs, barrier_kind kind) noexcept override

    virtual result<void> flush_inited_io_operations() noexcept override
    {
      _multiplexer_lock_guard g(this->_lock);
      TODO mark all submitted i / os as initiated;
      return success();
    }

    virtual io_operation_state_type check_io_operation(io_operation_state *_op) noexcept override
    {
      auto *state = static_cast<_io_uring_operation_state *>(_op);
      typename io_operation_state::lock_guard g(state);
      _pump(g);
      return state->state;
    }

    virtual result<io_operation_state_type> cancel_io_operation(io_operation_state *_op, deadline d = {}) noexcept override
    {
      auto *state = static_cast<_io_uring_operation_state *>(_op);
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
        TODO if there is a timeout, submit a timeout before this;
        auto ret = io_uring_enter(_v.fd, 0, 1, _IORING_ENTER_GETEVENTS);
        if(ret< 0)
        {
          return posix_error();
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
      // Post a null SQE, it'll break out any waits
      const uint32_t tail = inst.submission.tail->load(std::memory_order_acquire);
      if(tail == inst.submission.head->load(std::memory_order_relaxed))
      {
        return errc::resource_unavailable_try_again;  // SQE ring is full
      }
      const uint32_t sqeidx = tail & inst.submission.ring_mask;
      _io_uring_sqe *sqe = &inst.submission.entries[sqeidx];
      memset(sqe, 0, sizeof(_io_uring_sqe));
      sqe->opcode = _IORING_OP_NOP;
      inst.submission.array[sqeidx] = sqeidx;
      inst.submission.tail->fetch_add(1, std::memory_order_release);
      return success();
    }
  };

  LLFIO_HEADERS_ONLY_FUNC_SPEC result<io_multiplexer_ptr> multiplexer_linux_io_uring(size_t threads, bool is_polling) noexcept
  {
    try
    {
      if(1 == threads)
      {
        // Make non locking edition
        auto ret = std::make_unique<linux_io_uring_multiplexer<false>>(is_polling);
        OUTCOME_TRY(ret->init(false, ret->_nonseekable));
        return io_multiplexer_ptr(ret.release());
      }
      auto ret = std::make_unique<linux_io_uring_multiplexer<true>>(is_polling);
      OUTCOME_TRY(ret->init(false, ret->_nonseekable));
      return io_multiplexer_ptr(ret.release());
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
}  // namespace test

LLFIO_V2_NAMESPACE_END
