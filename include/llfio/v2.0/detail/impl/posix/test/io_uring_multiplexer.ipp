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
  */
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
        _to->count = count;
        return _to;
      }
    };
    int _wakecount{0};
    size_t threads{0};
    bool is_polling{false};
    struct _nonseekable_t  // fd is kept in this->_v.fd
    {
      struct submission_t
      {
        uint32_t *head{nullptr}, *tail{nullptr}, ring_mask{0}, ring_entries{0}, flags{0}, *dropped{nullptr}, *array{nullptr};
        span<uint32_t> queue;
        span<_io_uring_sqe> entries;
      } submission;
      struct completion_t
      {
        uint32_t *head{nullptr}, *tail{nullptr}, ring_mask{0}, ring_entries{0}, *overflow{nullptr};
        span<uint32_t> queue;
        span<_io_uring_cqe> entries;
      } completion;
    } _nonseekable;
    struct _seekable_t : _nonseekable_t
    {
      int fd{-1};
    } _seekable;
    struct _registered_fd
    {
      int fd{-1};
      _io_uring_operation_state *first{nullptr}, *last{nullptr};

      constexpr _registered_fd() {}
      explicit _registered_fd(int _fd)
          : fd(_fd)
      {
      }
      bool operator<(const _registered_fd &o) const noexcept { return fd < o.fd; }
    };
    std::vector<_registered_fd> _registered_fds{4};
    bool _have_ioring_register_files_update{true};
    std::vector<registered_buffer_type> _registered_buffers;

    std::vector<_registered_fd> _find_fd(int fd) const { auto ret = std::lower_bound(_registered_fds.begin(), _registered_fds.end(), fd);
      assert(fd == ret->fd);
      return ret;
    }
    void _insert(int fd, _io_uring_operation_state *state)
    {
      assert(state->prev == nullptr);
      assert(state->next == nullptr);
      _registered_fd &mine = *_find_fd(fd);
      assert(mine.first != state);
      assert(mine.last != state);
      if(mine.first == nullptr)
      {
        mine.first = mine.last = state;
      }
      else
      {
        assert(mine.last->next == nullptr);
        state->prev = mine.last;
        mine.last->next = state;
        mine.last = state;
      }
    }
    void _detach(int fd, linux_io_uring_multiplexer *parent)
    {
      _registered_fd &mine = *_find_fd(fd);
      if(prev == nullptr)
      {
        assert(mine.first == this);
        mine.first = next;
      }
      else
      {
        prev->next = next;
      }
      if(next == nullptr)
      {
        assert(mine.last == this);
        mine.last = prev;
      }
      else
      {
        next->prev = prev;
      }
      next = prev = nullptr;
    }

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
    static result<int> init(_nonseekable_t &out, size_t threads, bool is_polling)
    {
      int fd = -1;
      _io_uring_params params;
      memset(&params, 0, sizeof(params));
      if(is_polling)
      {
        // We don't implement IORING_SETUP_IOPOLL, it is for O_DIRECT files only in any case
        params.flags |= _IORING_SETUP_SQPOLL;
        params.sq_thread_idle = 100;  // 100 milliseconds
        if(threads == 1)
        {
          // Pin kernel submission polling thread to same CPU as I am pinned to, if I am pinned
          cpu_set_t affinity;
          CPU_ZERO(&affinity);
          if(-1 != sched_get_affinity(0, sizeof(affinity), &affinity) && CPU_COUNT(&affinity) == 1)
          {
            for(size_t n = 0; n < CPU_SET_SIZE; n++)
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
      fd = _io_uring_setup(getpagesize() / sizeof(_io_uring_sqe), &params);
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
        out.submission.queue = {(uint32_t *) p, params.sq_entries};
        out.head = &out.submission.queue[params.sq_off.head];
        out.tail = &out.submission.queue[params.sq_off.tail];
        out.ring_mask = out.submission.queue[params.sq_off.ring_mask];
        out.ring_entries = out.submission.queue[params.sq_off.ring_entries];
        out.flags = out.submission.queue[params.sq_off.flags];
        out.dropped = &out.submission.queue[params.sq_off.dropped];
        out.array = &out.submission.queue[params.sq_off.array];
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
        out.completion.queue = {(uint32_t *) p, params.cq_entries};
        out.head = &out.completion.queue[params.cq_off.head];
        out.tail = &out.completion.queue[params.cq_off.tail];
        out.ring_mask = out.completion.queue[params.cq_off.ring_mask];
        out.ring_entries = out.completion.queue[params.cq_off.ring_entries];
        out.overflow = out.completion.queue[params.cq_off.overflow];
        out.completion.entries = {(_io_uring_cqe *) &out.completion.queue[params.cq_off.cqes], params.cq_entries};
      }
      this->_v.behaviour |= native_handle_type::disposition::multiplexer;
      return fd;
    }

    // These functions are inherited from handle
    // virtual result<path_type> current_path() const noexcept override
    virtual result<void> close() noexcept override
    {
      auto do_close = [this](auto &s) {
        if(!s.submission.queue.empty())
        {
          if(-1 == ::munmap(s.submission.queue.data(), s.submission.queue.size_bytes()))
          {
            return posix_error();
          }
          s.submission.queue = {};
        }
        if(!s.submission.entries.empty())
        {
          if(-1 == ::munmap(s.submission.entries.data(), s.submission.entries.size_bytes()))
          {
            return posix_error();
          }
          s.submission.entries = {};
        }
        if(!s.completion.queue.empty())
        {
          if(-1 == ::munmap(s.completion.queue.data(), s.completion.queue.size_bytes()))
          {
            return posix_error();
          }
          s.completion.queue = {};
        }
      };
      do_close(_seekable);
      if(-1!=_seekable.fd)
      {
        if(-1 == ::close(_seekable.fd))
        {
          return posix_error();
        }
      }
      do_close(_nonseekable);
      OUTCOME_TRY(_base::close());
      _known_fds.clear();
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

    result<void> _recalculate_registered_fds(int index, int fd) noexcept
    {
      if(_have_ioring_register_files_update)
      {
        int32_t newvalue = fd;
        _io_uring_files_update upd;
        memset(&upd, 0, sizeof(upd));
        upd.offset = index;
        upd.fds = (__aligned_u64) &newvalue;
        if(_io_uring_register(this->_v.fd, _IORING_REGISTER_FILES_UPDATE, &upd, 1) >= 0)  // Linux kernel 5.5 onwards
        {
          return success();
        }
        _have_ioring_register_files_update = false;
      }
#if 0  // Hangs the ring until it empties, which locks this implementation
      // Fall back to the old inefficient API
      std::vector<int32_t> map(_known_fds.back() + 1, -1);
      for(auto fd : _known_fds)
      {
        map[fd] = fd;
      }
      (void) _io_uring_register(this->_v.fd, _IORING_UNREGISTER_FILES, nullptr, 0);
      if(_io_uring_register(this->_v.fd, _IORING_REGISTER_FILES, map.data(), map.size()) < 0)
      {
        return posix_error();
      }
#endif
      return success();
    }
    virtual result<uint8_t> do_io_handle_register(io_handle *h) noexcept override  // linear complexity to total handles registered
    {
      _multiplexer_lock_guard g(this->_lock);
      int toinsert = h->native_handle().fd;
      _registered_fds.push_back(); // capacity expansion
      _registered_fds.pop_back();
      _registered_fds.insert(std::lower_bound(_registered_fds.begin(), _registered_fds.end(), toinsert), toinsert);
      return _recalculate_registered_fds(toinsert, toinsert);
    }
    virtual result<void> do_io_handle_deregister(io_handle *h) noexcept override
    {
      _multiplexer_lock_guard g(this->_lock);
      int toremove = h->native_handle().fd;
      auto it = _find_fd(fd);
      assert(it->first == nullptr);
      if(it->first!=nullptr)
      {
        return errc::operation_in_progress;
      }
      _registered_fds.erase(it);
      return _recalculate_registered_fds(toremove, -1);
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
      OUTCOME_TRY(ret, _base::do_io_handle_allocate_registered_buffer(h, bytes));
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
      _nonseekable_t &queue = state->h->is_seekable() ? _seekable : _nonseekable;
      const uint32_t index = queue.submission.tail & queue.submission.ring_mask;
      _io_uring_sqe *sqe = &queue.submission.entries[index];
      auto submit = [&] { queue.submission.array[index] = index;
        queue.tail++;
        WHAT AM I DOING HERE ? ;
      };
      switch(s)
      {
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
      default:
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
      auto *state = static_cast<_io_uring_operation_state *>(_op);
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

  LLFIO_HEADERS_ONLY_FUNC_SPEC result<io_multiplexer_ptr> multiplexer_linux_io_uring_multiplexer(size_t threads, bool is_polling) noexcept
  {
    try
    {
      if(1 == threads)
      {
        auto ret = std::make_unique<linux_io_uring_multiplexer<false>>();
        OUTCOME_TRY(ret->init(1, is_polling));
        return io_multiplexer_ptr(ret.release());
      }
      auto ret = std::make_unique<linux_io_uring_multiplexer<true>>();
      OUTCOME_TRY(ret->init(threads, is_polling));
      return io_multiplexer_ptr(ret.release());
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
}  // namespace test

LLFIO_V2_NAMESPACE_END
