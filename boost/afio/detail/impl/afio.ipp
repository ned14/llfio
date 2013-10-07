/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

//#define BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH 1

#ifndef BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH
#define BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH 8
#endif
//#define USE_POSIX_ON_WIN32 // Useful for testing

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996) // This function or variable may be unsafe
#endif

// This always compiles in input validation for this file only (the header file
// disables at the point of instance validation in release builds)
#ifndef BOOST_AFIO_NEVER_VALIDATE_INPUTS
#define BOOST_AFIO_VALIDATE_INPUTS 1
#endif

// Define this to have every allocated op take a backtrace from where it was allocated
//#ifndef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
//#ifndef NDEBUG
//#define BOOST_AFIO_OP_STACKBACKTRACEDEPTH 8
//#endif
//#endif

// Define this to have every part of AFIO print, in extremely terse text, what it is doing and why.
#if defined(DEBUG) && 0 //|| 1
#define BOOST_AFIO_DEBUG_PRINTING 1
#ifdef WIN32
#define BOOST_AFIO_DEBUG_PRINT(...) \
    { \
    char buffer[16384]; \
    sprintf(buffer, __VA_ARGS__); \
    OutputDebugStringA(buffer); \
    }
#else
#define BOOST_AFIO_DEBUG_PRINT(...) \
    { \
    fprintf(stderr, __VA_ARGS__); \
    }
#endif
#else
#define BOOST_AFIO_DEBUG_PRINT(...)
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
#include <dlfcn.h>
// Set to 1 to use libunwind instead of glibc's stack backtracer
#if 0
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#else
#ifdef __linux__
#include <execinfo.h>
#else
#undef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
#endif
#endif
#endif

#include "../../afio.hpp"
#include "../Aligned_Allocator.hpp"
#include "../MemoryTransactions.hpp"
#include "../valgrind/memcheck.h"
#include "../valgrind/helgrind.h"
#include "ErrorHandling.ipp"

#include <fcntl.h>
#ifdef WIN32
// We also compile the posix compat layer for catching silly compile errors for POSIX
#include <io.h>
#include <direct.h>
#define BOOST_AFIO_POSIX_MKDIR(path, mode) _wmkdir(path)
#define BOOST_AFIO_POSIX_RMDIR _wrmdir
#define BOOST_AFIO_POSIX_STAT_STRUCT struct __stat64
#define BOOST_AFIO_POSIX_STAT _wstat64
#define BOOST_AFIO_POSIX_LSTAT _wstat64
#define BOOST_AFIO_POSIX_OPEN _wopen
#define BOOST_AFIO_POSIX_CLOSE _close
#define BOOST_AFIO_POSIX_UNLINK _wunlink
#define BOOST_AFIO_POSIX_FSYNC _commit
#define BOOST_AFIO_POSIX_FTRUNCATE winftruncate
#define BOOST_AFIO_POSIX_MMAP(addr, size, prot, flags, fd, offset) (-1)
#define BOOST_AFIO_POSIX_MUNMAP(addr, size) (-1)
#include "nt_kernel_stuff.hpp"
#else
#include <dirent.h>     /* Defines DT_* constants */
#include <sys/syscall.h>
#include <fnmatch.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <limits.h>
#define BOOST_AFIO_POSIX_MKDIR mkdir
#define BOOST_AFIO_POSIX_RMDIR ::rmdir
#define BOOST_AFIO_POSIX_STAT_STRUCT struct stat 
#define BOOST_AFIO_POSIX_STAT stat
#define BOOST_AFIO_POSIX_LSTAT ::lstat
#define BOOST_AFIO_POSIX_OPEN open
#define BOOST_AFIO_POSIX_SYMLINK ::symlink
#define BOOST_AFIO_POSIX_CLOSE ::close
#define BOOST_AFIO_POSIX_UNLINK unlink
#define BOOST_AFIO_POSIX_FSYNC fsync
#define BOOST_AFIO_POSIX_FTRUNCATE ftruncate
#define BOOST_AFIO_POSIX_MMAP mmap
#define BOOST_AFIO_POSIX_MUNMAP munmap

static inline boost::afio::chrono::system_clock::time_point to_timepoint(struct timespec ts)
{
    // Need to have this self-adapt to the STL being used
    static BOOST_CONSTEXPR_OR_CONST unsigned long long STL_TICKS_PER_SEC=(unsigned long long) boost::afio::chrono::system_clock::period::den/boost::afio::chrono::system_clock::period::num;
    // We make the big assumption that the STL's system_clock is based on the time_t epoch 1st Jan 1970.
    boost::afio::chrono::system_clock::duration duration(ts.tv_sec*STL_TICKS_PER_SEC+ts.tv_nsec/(1000000000ULL/STL_TICKS_PER_SEC));
    return boost::afio::chrono::system_clock::time_point(duration);
}
static inline void fill_stat_t(boost::afio::stat_t &stat, BOOST_AFIO_POSIX_STAT_STRUCT s, boost::afio::metadata_flags wanted)
{
    using namespace boost::afio;
    if(!!(wanted&metadata_flags::dev)) { stat.st_dev=s.st_dev; }
    if(!!(wanted&metadata_flags::ino)) { stat.st_ino=s.st_ino; }
    if(!!(wanted&metadata_flags::type)) { stat.st_type=s.st_mode; }
    if(!!(wanted&metadata_flags::mode)) { stat.st_mode=s.st_mode; }
    if(!!(wanted&metadata_flags::nlink)) { stat.st_nlink=s.st_nlink; }
    if(!!(wanted&metadata_flags::uid)) { stat.st_uid=s.st_uid; }
    if(!!(wanted&metadata_flags::gid)) { stat.st_gid=s.st_gid; }
    if(!!(wanted&metadata_flags::rdev)) { stat.st_rdev=s.st_rdev; }
    if(!!(wanted&metadata_flags::atim)) { stat.st_atim=to_timepoint(s.st_atim); }
    if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=to_timepoint(s.st_mtim); }
    if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=to_timepoint(s.st_ctim); }
    if(!!(wanted&metadata_flags::size)) { stat.st_size=s.st_size; }
    if(!!(wanted&metadata_flags::allocated)) { stat.st_allocated=s.st_blocks*s.st_blksize; }
    if(!!(wanted&metadata_flags::blocks)) { stat.st_blocks=s.st_blocks; }
    if(!!(wanted&metadata_flags::blksize)) { stat.st_blksize=s.st_blksize; }
#ifdef HAVE_STAT_FLAGS
    if(!!(wanted&metadata_flags::flags)) { stat.st_flags=s.st_flags; }
#endif
#ifdef HAVE_STAT_GEN
    if(!!(wanted&metadata_flags::gen)) { stat.st_gen=s.st_gen; }
#endif
#ifdef HAVE_BIRTHTIMESPEC
    if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=to_timepoint(s.st_birthtim); }
#endif
}
#endif

// libstdc++ doesn't come with std::lock_guard
#define BOOST_AFIO_LOCK_GUARD boost::lock_guard

#ifdef WIN32
#ifndef IOV_MAX
#define IOV_MAX 1024
#endif
struct iovec {
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
};
typedef ptrdiff_t ssize_t;
static boost::detail::spinlock preadwritelock;
inline ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, boost::afio::off_t offset)
{
    boost::afio::off_t at=offset;
    ssize_t transferred;
    BOOST_AFIO_LOCK_GUARD<boost::detail::spinlock> lockh(preadwritelock);
    if(-1==_lseeki64(fd, offset, SEEK_SET)) return -1;
    for(; iovcnt; iov++, iovcnt--, at+=(boost::afio::off_t) transferred)
        if(-1==(transferred=_read(fd, iov->iov_base, (unsigned) iov->iov_len))) return -1;
    return (ssize_t)(at-offset);
}
inline ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, boost::afio::off_t offset)
{
    boost::afio::off_t at=offset;
    ssize_t transferred;
    BOOST_AFIO_LOCK_GUARD<boost::detail::spinlock> lockh(preadwritelock);
    if(-1==_lseeki64(fd, offset, SEEK_SET)) return -1;
    for(; iovcnt; iov++, iovcnt--, at+=(boost::afio::off_t) transferred)
        if(-1==(transferred=_write(fd, iov->iov_base, (unsigned) iov->iov_len))) return -1;
    return (ssize_t)(at-offset);
}
#endif


namespace boost { namespace afio {

#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
    namespace detail { BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC boost::exception_ptr &vs2010_lack_of_decent_current_exception_support_hack() { static boost::exception_ptr v; return v; } }
#endif

size_t async_file_io_dispatcher_base::page_size() BOOST_NOEXCEPT_OR_NOTHROW
{
    static size_t pagesize;
    if(!pagesize)
    {
#ifdef WIN32
        SYSTEM_INFO si={ 0 };
        GetSystemInfo(&si);
        pagesize=si.dwPageSize;
#else
        pagesize=getpagesize();
#endif
    }
    return pagesize;
}

std::shared_ptr<std_thread_pool> process_threadpool()
{
    // This is basically how many file i/o operations can occur at once
    // Obviously the kernel also has a limit
    static std::weak_ptr<std_thread_pool> shared;
    static boost::detail::spinlock lock;
    std::shared_ptr<std_thread_pool> ret(shared.lock());
    if(!ret)
    {
        BOOST_AFIO_LOCK_GUARD<boost::detail::spinlock> lockh(lock);
        ret=shared.lock();
        if(!ret)
        {
            shared=ret=std::make_shared<std_thread_pool>(BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH);
        }
    }
    return ret;
}


namespace detail {
	BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void print_fatal_exception_message_to_stderr(const char *msg)
	{
		std::cerr << "FATAL EXCEPTION: " << msg << std::endl;
	}

    struct async_io_handle_posix : public async_io_handle
    {
        int fd;
        bool has_been_added, SyncOnClose, has_ever_been_fsynced;
        void *mapaddr; size_t mapsize;

        async_io_handle_posix(async_file_io_dispatcher_base *_parent, std::shared_ptr<async_io_handle> _dirh, const std::filesystem::path &path, file_flags flags, bool _SyncOnClose, int _fd) : async_io_handle(_parent, std::move(_dirh), path, flags), fd(_fd), has_been_added(false), SyncOnClose(_SyncOnClose), has_ever_been_fsynced(false), mapaddr(nullptr), mapsize(0)
        {
            if(fd!=-999)
                BOOST_AFIO_ERRHOSFN(fd, path);
        }
        void int_close()
        {
            BOOST_AFIO_DEBUG_PRINT("D %p\n", this);
            if(has_been_added)
            {
                parent()->int_del_io_handle((void *) (size_t) fd);
                has_been_added=false;
            }
            if(mapaddr)
            {
                BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_MUNMAP(mapaddr, mapsize), path());
                mapaddr=nullptr;
            }
            if(fd>=0)
            {
                if(SyncOnClose && write_count_since_fsync())
                    BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_FSYNC(fd), path());
                BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_CLOSE(fd), path());
                fd=-1;
            }
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void close()
        {
            int_close();
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void *native_handle() const { return (void *)(size_t)fd; }

        // You can't use shared_from_this() in a constructor so ...
        void do_add_io_handle_to_parent()
        {
            if(fd!=-999)
            {
                parent()->int_add_io_handle((void *) (size_t) fd, shared_from_this());
                has_been_added=true;
            }
        }
        ~async_io_handle_posix()
        {
            int_close();
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC directory_entry direntry(metadata_flags wanted) const
        {
            stat_t stat(nullptr);
            BOOST_AFIO_POSIX_STAT_STRUCT s={0};
            BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTAT(path().c_str(), &s), path());
            fill_stat_t(stat, s, wanted);
            return directory_entry(path().leaf(), stat, wanted);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::filesystem::path target() const
        {
#ifdef WIN32
            return std::filesystem::path();
#else
            if(!opened_as_symlink())
                return std::filesystem::path();
            char buffer[PATH_MAX+1];
            ssize_t len;
            if((len = readlink(path().c_str(), buffer, sizeof(buffer)-1)) == -1)
                BOOST_AFIO_ERRGOS(-1);
            return std::filesystem::path::string_type(buffer, len);
#endif
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void *try_mapfile()
        {
#ifndef WIN32
            if(!mapaddr)
            {
                if(!(flags() & file_flags::Write) && !(flags() & file_flags::Append))
                {
                    BOOST_AFIO_POSIX_STAT_STRUCT s={0};
                    if(-1!=fstat(fd, &s))
                    {
                        if((mapaddr=BOOST_AFIO_POSIX_MMAP(nullptr, s.st_size, PROT_READ, MAP_SHARED, fd, 0)))
                            mapsize=s.st_size;
                    }
                }
            }
#endif
            return mapaddr;
        }
    };

#ifdef DOXYGEN_NO_CLASS_ENUMS
    enum file_flags
#elif defined(BOOST_NO_CXX11_SCOPED_ENUMS)
    BOOST_SCOPED_ENUM_DECLARE_BEGIN(OpType)
#else
    enum class OpType 
#endif
    {
        Unknown,
        UserCompletion,
        dir,
        rmdir,
        file,
        rmfile,
        symlink,
        rmsymlink,
        sync,
        close,
        read,
        write,
        truncate,
        barrier,
        enumerate,

        Last
    }
#ifdef BOOST_NO_CXX11_SCOPED_ENUMS
    BOOST_SCOPED_ENUM_DECLARE_END(OpType)
#else
    ;
#endif
    static const char *optypes[]={
        "unknown",
        "UserCompletion",
        "dir",
        "rmdir",
        "file",
        "rmfile",
        "symlink",
        "rmsymlink",
        "sync",
        "close",
        "read",
        "write",
        "truncate",
        "barrier",
        "enumerate"
    };
    static_assert(static_cast<size_t>(OpType::Last)==sizeof(optypes)/sizeof(*optypes), "You forgot to fix up the strings matching OpType");
    struct async_file_io_dispatcher_op
    {
        OpType optype;
        async_op_flags flags;
        std::shared_ptr<shared_future<std::shared_ptr<async_io_handle>>> h;
        std::unique_ptr<promise<std::shared_ptr<async_io_handle>>> detached_promise;
        typedef std::pair<size_t, std::function<std::shared_ptr<async_io_handle> (std::shared_ptr<async_io_handle>, exception_ptr *)>> completion_t;
        std::vector<completion_t> completions;
#ifndef NDEBUG
        completion_t boundf; // Useful for debugging
#endif
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
        std::vector<void *> stack;
        void fillStack()
        {
#ifdef UNW_LOCAL_ONLY
            // libunwind seems a bit more accurate than glibc's backtrace()
            unw_context_t uc;
            unw_cursor_t cursor;
            unw_getcontext(&uc);
            stack.reserve(BOOST_AFIO_OP_STACKBACKTRACEDEPTH);
            unw_init_local(&cursor, &uc);
            size_t n=0;
            while(unw_step(&cursor)>0 && n++<BOOST_AFIO_OP_STACKBACKTRACEDEPTH)
            {
                unw_word_t ip;
                unw_get_reg(&cursor, UNW_REG_IP, &ip);
                stack.push_back((void *) ip);
            }
#else
            stack.reserve(BOOST_AFIO_OP_STACKBACKTRACEDEPTH);
            stack.resize(BOOST_AFIO_OP_STACKBACKTRACEDEPTH);
            stack.resize(backtrace(&stack.front(), stack.size()));
#endif
        }
#else
        void fillStack() { }
#endif
        async_file_io_dispatcher_op(OpType _optype, async_op_flags _flags, std::shared_ptr<shared_future<std::shared_ptr<async_io_handle>>> _h, completion_t _boundf)
            : optype(_optype), flags(_flags), h(_h)
#ifndef NDEBUG
            , boundf(_boundf)
#endif
        { fillStack(); }
        async_file_io_dispatcher_op(async_file_io_dispatcher_op &&o) BOOST_NOEXCEPT_OR_NOTHROW : optype(o.optype), flags(std::move(o.flags)), h(std::move(o.h)),
            detached_promise(std::move(o.detached_promise)), completions(std::move(o.completions))
#ifndef NDEBUG
            , boundf(std::move(o.boundf))
#endif
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
            , stack(std::move(o.stack))
#endif
            {
            }
    private:
        async_file_io_dispatcher_op(const async_file_io_dispatcher_op &o)
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
            = delete
#endif
            ;
    };
    struct async_file_io_dispatcher_base_p
    {
        std::shared_ptr<thread_source> pool;
        file_flags flagsforce, flagsmask;

        typedef spinlock<size_t> fdslock_t;
        typedef spinlock<size_t,
            spins_to_transact<50>::policy,
            spins_to_loop<150>::policy,
            spins_to_yield<500>::policy,
            spins_to_sleep::policy
        > opslock_t;
        typedef recursive_mutex dircachelock_t;
        fdslock_t fdslock; std::unordered_map<void *, std::weak_ptr<async_io_handle>> fds;
        opslock_t opslock; atomic<size_t> monotoniccount; std::unordered_map<size_t, std::shared_ptr<async_file_io_dispatcher_op>> ops;
        dircachelock_t dircachelock; std::unordered_map<std::filesystem::path, std::weak_ptr<async_io_handle>, std::filesystem_hash> dirhcache;

        async_file_io_dispatcher_base_p(std::shared_ptr<thread_source> _pool, file_flags _flagsforce, file_flags _flagsmask) : pool(_pool),
            flagsforce(_flagsforce), flagsmask(_flagsmask), monotoniccount(0)
        {
#if !defined(BOOST_MSVC)|| BOOST_MSVC >= 1700 // MSVC 2010 doesn't support reserve
            ops.reserve(10000);
#else
            ops.rehash((size_t) std::ceil(10000 / ops.max_load_factor()));
#endif
        }
        ~async_file_io_dispatcher_base_p()
        {
        }

        // Returns a handle to a directory from the cache, or creates a new directory handle.
        template<class F> std::shared_ptr<async_io_handle> get_handle_to_dir(F *parent, size_t id, exception_ptr *e, async_path_op_req req, typename async_file_io_dispatcher_base::completion_returntype (F::*dofile)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_path_op_req))
        {
            std::shared_ptr<async_io_handle> dirh;
            BOOST_AFIO_LOCK_GUARD<dircachelock_t> dircachelockh(dircachelock);
            do
            {
                std::unordered_map<std::filesystem::path, std::weak_ptr<async_io_handle>, std::filesystem_hash>::iterator it=dirhcache.find(req.path);
                if(dirhcache.end()==it || it->second.expired())
                {
                    if(dirhcache.end()!=it) dirhcache.erase(it);
                    auto result=(parent->*dofile)(id, /* known null */dirh, e, req);
                    if(!result.first) abort();
                    dirh=std::move(result.second);
                    if(dirh)
                    {
                        auto _it=dirhcache.insert(std::make_pair(req.path, std::weak_ptr<async_io_handle>(dirh)));
                        return dirh;
                    }
                    else
                        abort();
                }
                else
                    dirh=std::shared_ptr<async_io_handle>(it->second);
            } while(!dirh);
            return dirh;
        }
        // Returns a handle to a containing directory from the cache, or creates a new directory handle.
        template<class F> std::shared_ptr<async_io_handle> get_handle_to_containing_dir(F *parent, size_t id, exception_ptr *e, async_path_op_req req, typename async_file_io_dispatcher_base::completion_returntype (F::*dofile)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_path_op_req))
        {
            req.path=req.path.parent_path();
            req.flags=req.flags&~file_flags::FastDirectoryEnumeration;
            return get_handle_to_dir(parent, id, e, req, dofile);
        }
    };
    class async_file_io_dispatcher_compat;
    class async_file_io_dispatcher_windows;
    class async_file_io_dispatcher_linux;
    class async_file_io_dispatcher_qnx;
    struct immediate_async_ops
    {
        typedef std::shared_ptr<async_io_handle> rettype;
        typedef rettype retfuncttype();
        std::vector<packaged_task<retfuncttype>> toexecute;

        immediate_async_ops() { }
        // Returns a promise which is fulfilled when this is destructed
        future<rettype> enqueue(std::function<retfuncttype> f)
        {
            packaged_task<retfuncttype> t(std::move(f));
            toexecute.push_back(std::move(t));
            return toexecute.back().get_future();
        }
        ~immediate_async_ops()
        {
            BOOST_FOREACH(auto &i, toexecute)
            {
                i();
            }
        }
    private:
        immediate_async_ops(const immediate_async_ops &);
        immediate_async_ops &operator=(const immediate_async_ops &);
        immediate_async_ops(immediate_async_ops &&);
        immediate_async_ops &operator=(immediate_async_ops &&);
    };
}

metadata_flags directory_entry::metadata_supported() BOOST_NOEXCEPT_OR_NOTHROW
{
    metadata_flags ret;
#ifdef WIN32
    ret=metadata_flags::None
        //| metadata_flags::dev
        | metadata_flags::ino        // FILE_INTERNAL_INFORMATION, enumerated
        | metadata_flags::type       // FILE_BASIC_INFORMATION, enumerated
        //| metadata_flags::mode
        | metadata_flags::nlink      // FILE_STANDARD_INFORMATION
        //| metadata_flags::uid
        //| metadata_flags::gid
        //| metadata_flags::rdev
        | metadata_flags::atim       // FILE_BASIC_INFORMATION, enumerated
        | metadata_flags::mtim       // FILE_BASIC_INFORMATION, enumerated
        | metadata_flags::ctim       // FILE_BASIC_INFORMATION, enumerated
        | metadata_flags::size       // FILE_STANDARD_INFORMATION, enumerated
        | metadata_flags::allocated  // FILE_STANDARD_INFORMATION, enumerated
        | metadata_flags::blocks
        | metadata_flags::blksize    // FILE_ALIGNMENT_INFORMATION
        //| metadata_flags::flags
        //| metadata_flags::gen
        | metadata_flags::birthtim   // FILE_BASIC_INFORMATION, enumerated
        ;
#elif defined(__linux__)
    ret=metadata_flags::None
        | metadata_flags::dev
        | metadata_flags::ino
        | metadata_flags::type
        | metadata_flags::mode
        | metadata_flags::nlink
        | metadata_flags::uid
        | metadata_flags::gid
        | metadata_flags::rdev
        | metadata_flags::atim
        | metadata_flags::mtim
        | metadata_flags::ctim
        | metadata_flags::size
        | metadata_flags::allocated
        | metadata_flags::blocks
        | metadata_flags::blksize
    // Sadly these must wait until someone fixes the Linux stat() call e.g. the xstat() proposal.
        //| metadata_flags::flags
        //| metadata_flags::gen
        //| metadata_flags::birthtim
    // According to http://computer-forensics.sans.org/blog/2011/03/14/digital-forensics-understanding-ext4-part-2-timestamps
    // ext4 keeps birth time at offset 144 to 151 in the inode. If we ever got round to it, birthtime could be hacked.
        ;
#else
    // Kinda assumes FreeBSD or OS X really ...
    ret=metadata_flags::None
        | metadata_flags::dev
        | metadata_flags::ino
        | metadata_flags::type
        | metadata_flags::mode
        | metadata_flags::nlink
        | metadata_flags::uid
        | metadata_flags::gid
        | metadata_flags::rdev
        | metadata_flags::atim
        | metadata_flags::mtim
        | metadata_flags::ctim
        | metadata_flags::size
        | metadata_flags::allocated
        | metadata_flags::blocks
        | metadata_flags::blksize
#define HAVE_STAT_FLAGS
        | metadata_flags::flags
#define HAVE_STAT_GEN
        | metadata_flags::gen
#define HAVE_BIRTHTIMESPEC
        | metadata_flags::birthtim
        ;
#endif
    return ret;
}

metadata_flags directory_entry::metadata_fastpath() BOOST_NOEXCEPT_OR_NOTHROW
{
    metadata_flags ret;
#ifdef WIN32
    ret=metadata_flags::None
        //| metadata_flags::dev
        | metadata_flags::ino        // FILE_INTERNAL_INFORMATION, enumerated
        | metadata_flags::type       // FILE_BASIC_INFORMATION, enumerated
        //| metadata_flags::mode
        //| metadata_flags::nlink      // FILE_STANDARD_INFORMATION
        //| metadata_flags::uid
        //| metadata_flags::gid
        //| metadata_flags::rdev
        | metadata_flags::atim       // FILE_BASIC_INFORMATION, enumerated
        | metadata_flags::mtim       // FILE_BASIC_INFORMATION, enumerated
        | metadata_flags::ctim       // FILE_BASIC_INFORMATION, enumerated
        | metadata_flags::size       // FILE_STANDARD_INFORMATION, enumerated
        | metadata_flags::allocated  // FILE_STANDARD_INFORMATION, enumerated
        //| metadata_flags::blocks
        //| metadata_flags::blksize    // FILE_ALIGNMENT_INFORMATION
        //| metadata_flags::flags
        //| metadata_flags::gen
        | metadata_flags::birthtim   // FILE_BASIC_INFORMATION, enumerated
        ;
#elif defined(__linux__)
    ret=metadata_flags::None
        | metadata_flags::dev
        | metadata_flags::ino
        | metadata_flags::type
        | metadata_flags::mode
        | metadata_flags::nlink
        | metadata_flags::uid
        | metadata_flags::gid
        | metadata_flags::rdev
        | metadata_flags::atim
        | metadata_flags::mtim
        | metadata_flags::ctim
        | metadata_flags::size
        | metadata_flags::allocated
        | metadata_flags::blocks
        | metadata_flags::blksize
    // Sadly these must wait until someone fixes the Linux stat() call e.g. the xstat() proposal.
        //| metadata_flags::flags
        //| metadata_flags::gen
        //| metadata_flags::birthtim
    // According to http://computer-forensics.sans.org/blog/2011/03/14/digital-forensics-understanding-ext4-part-2-timestamps
    // ext4 keeps birth time at offset 144 to 151 in the inode. If we ever got round to it, birthtime could be hacked.
        ;
#else
    // Kinda assumes FreeBSD or OS X really ...
    ret=metadata_flags::None
        | metadata_flags::dev
        | metadata_flags::ino
        | metadata_flags::type
        | metadata_flags::mode
        | metadata_flags::nlink
        | metadata_flags::uid
        | metadata_flags::gid
        | metadata_flags::rdev
        | metadata_flags::atim
        | metadata_flags::mtim
        | metadata_flags::ctim
        | metadata_flags::size
        | metadata_flags::allocated
        | metadata_flags::blocks
        | metadata_flags::blksize
#define HAVE_STAT_FLAGS
        | metadata_flags::flags
#define HAVE_STAT_GEN
        | metadata_flags::gen
#define HAVE_BIRTHTIMESPEC
        | metadata_flags::birthtim
        ;
#endif
    return ret;
}

size_t directory_entry::compatibility_maximum() BOOST_NOEXCEPT_OR_NOTHROW
{
#ifdef WIN32
    // Let's choose 100k entries. Why not!
    return 100000;
#else
    return 100000;
    // This is what glibc uses, a 32Kb buffer.
    //return 32768/sizeof(dirent);
#endif
}

async_file_io_dispatcher_base::async_file_io_dispatcher_base(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask) : p(new detail::async_file_io_dispatcher_base_p(threadpool, flagsforce, flagsmask))
{
}

async_file_io_dispatcher_base::~async_file_io_dispatcher_base()
{
#ifndef BOOST_AFIO_COMPILING_FOR_GCOV
    std::unordered_map<shared_future<std::shared_ptr<async_io_handle>> *, std::pair<size_t, future_status>> reallyoutstanding;
    for(;;)
    {
        std::vector<std::pair<size_t, std::shared_ptr<shared_future<std::shared_ptr<async_io_handle>>>>> outstanding;
        BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock)
        {
            if(!p->ops.empty())
            {
                outstanding.reserve(p->ops.size());
                BOOST_FOREACH(auto &op, p->ops)
                {
                    if(op.second->h->valid())
                    {
                        auto it=reallyoutstanding.find(op.second->h.get());
                        if(reallyoutstanding.end()!=it)
                        {
                            if(it->second.first>=5)
                            {
                                static const char *statuses[]={ "ready", "timeout", "deferred", "unknown" };
                                std::cerr << "WARNING: ~async_file_dispatcher_base() detects stuck async_io_op in total of " << p->ops.size() << " extant ops\n"
                                    "   id=" << op.first << " type=" << detail::optypes[static_cast<size_t>(op.second->optype)] << " flags=0x" << std::hex << static_cast<size_t>(op.second->flags) << std::dec << " status=" << statuses[(((int) it->second.second)>=0 && ((int) it->second.second)<=2) ? (int) it->second.second : 3] << " handle_usecount=" << op.second->h.use_count() << " failcount=" << it->second.first << " Completions:";
                                BOOST_FOREACH(auto &c, op.second->completions)
                                {
                                    std::cerr << " id=" << c.first;
                                }
                                std::cerr << std::endl;
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
                                std::cerr << "  Allocation backtrace:" << std::endl;
                                size_t n=0;
                                BOOST_FOREACH(void *addr, op.second.stack)
                                {
                                    Dl_info info;
                                    std::cerr << "    " << ++n << ". 0x" << std::hex << addr << std::dec << ": ";
                                    if(dladdr(addr, &info))
                                    {
                                        // This is hacky ...
                                        if(info.dli_fname)
                                        {
                                            char buffer2[4096];
                                            sprintf(buffer2, "/usr/bin/addr2line -C -f -i -e %s %lx", info.dli_fname, (long)((size_t) addr - (size_t) info.dli_fbase));
                                            FILE *ih=popen(buffer2, "r");
                                            auto unih=boost::afio::detail::Undoer([&ih]{ if(ih) pclose(ih); });
                                            if(ih)
                                            {
                                                size_t length=fread(buffer2, 1, sizeof(buffer2), ih);
                                                buffer2[length]=0;
                                                std::string buffer(buffer2, length-1);
                                                boost::replace_all(buffer, "\n", "\n       ");
                                                std::cerr << buffer << " (+0x" << std::hex << ((size_t) addr - (size_t) info.dli_saddr) << ")" << std::dec;
                                            }
                                            else std::cerr << info.dli_fname << ":0 (+0x" << std::hex << ((size_t) addr - (size_t) info.dli_fbase) << ")" << std::dec;
                                        }
                                        else std::cerr << "unknown:0";
                                    }
                                    else
                                        std::cerr << "completely unknown";
                                    std::cerr << std::endl;
                                }
#endif
                            }
                        }
                        outstanding.push_back(std::make_pair(op.first, op.second->h));
                    }
                }
            }
        }
        BOOST_END_MEMORY_TRANSACTION(p->opslock)
        if(outstanding.empty()) break;
        size_t mincount=(size_t)-1;
        BOOST_FOREACH(auto &op, outstanding)
        {
            future_status status=op.second->wait_for(chrono::duration<int, ratio<1, 10>>(1).toBoost());
            switch(status)
            {
            case future_status::ready:
                reallyoutstanding.erase(op.second.get());
                break;
            case future_status::deferred:
                // Probably pending on others, but log
            case future_status::timeout:
                auto it=reallyoutstanding.find(op.second.get());
                if(reallyoutstanding.end()==it)
                    it=reallyoutstanding.insert(std::make_pair(op.second.get(), std::make_pair(0, status))).first;
                it->second.first++;
                if(it->second.first<mincount) mincount=it->second.first;
                break;
            }
        }
        if(mincount>=10 && mincount!=(size_t)-1) // i.e. nothing is changing
        {
            std::cerr << "WARNING: ~async_file_dispatcher_base() sees no change in " << reallyoutstanding.size() << " stuck async_io_ops, so exiting destructor wait" << std::endl;
            break;
        }
    }
#endif
    delete p;
}

void async_file_io_dispatcher_base::int_add_io_handle(void *key, std::shared_ptr<async_io_handle> h)
{
    BOOST_BEGIN_MEMORY_TRANSACTION(p->fdslock)
    {
        p->fds.insert(std::make_pair(key, std::weak_ptr<async_io_handle>(h)));
    }
    BOOST_END_MEMORY_TRANSACTION(p->fdslock)
}

void async_file_io_dispatcher_base::int_del_io_handle(void *key)
{
    BOOST_BEGIN_MEMORY_TRANSACTION(p->fdslock)
    {
        p->fds.erase(key);
    }
    BOOST_END_MEMORY_TRANSACTION(p->fdslock)
}

std::shared_ptr<thread_source> async_file_io_dispatcher_base::threadsource() const
{
    return p->pool;
}

file_flags async_file_io_dispatcher_base::fileflags(file_flags flags) const
{
    file_flags ret=(flags&~p->flagsmask)|p->flagsforce;
    if(!!(ret&file_flags::EnforceDependencyWriteOrder))
    {
        // The logic (for now) is this:
        // If the data is sequentially accessed, we won't be seeking much
        // so turn on AlwaysSync.
        // If not sequentially accessed and we might therefore be seeking,
        // turn on SyncOnClose.
        if(!!(ret&file_flags::WillBeSequentiallyAccessed))
            ret=ret|file_flags::AlwaysSync;
        else
            ret=ret|file_flags::SyncOnClose;
    }
    return ret;
}

size_t async_file_io_dispatcher_base::wait_queue_depth() const
{
    BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock)
    {
        return p->ops.size();
    }
    BOOST_END_MEMORY_TRANSACTION(p->opslock)
}

size_t async_file_io_dispatcher_base::fd_count() const
{
    size_t ret;
    BOOST_BEGIN_MEMORY_TRANSACTION(p->fdslock)
    {
        ret=p->fds.size();
    }
    BOOST_END_MEMORY_TRANSACTION(p->fdslock)
    return ret;
}

// Non op lock holding variant
async_io_op async_file_io_dispatcher_base::int_op_from_scheduled_id(size_t id) const
{
    std::unordered_map<size_t, std::shared_ptr<detail::async_file_io_dispatcher_op>>::iterator it=p->ops.find(id);
    if(p->ops.end()==it)
    {
        BOOST_AFIO_THROW(std::runtime_error("Failed to find this operation in list of currently executing operations"));
    }
    return async_io_op(const_cast<async_file_io_dispatcher_base *>(this), id, it->second->h);
}
async_io_op async_file_io_dispatcher_base::op_from_scheduled_id(size_t id) const
{
	async_io_op ret;
    BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock)
    {
        ret=int_op_from_scheduled_id(id);
    }
    BOOST_END_MEMORY_TRANSACTION(p->opslock)
	return ret;
}


#if defined(BOOST_AFIO_ENABLE_BENCHMARKING_COMPLETION)
// Called in unknown thread
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC async_file_io_dispatcher_base::completion_returntype async_file_io_dispatcher_base::invoke_user_completion_fast(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, async_file_io_dispatcher_base::completion_t *callback)
{
    return callback(id, h, e);
}
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> async_file_io_dispatcher_base::completion(const std::vector<async_io_op> &ops, const std::vector<std::pair<async_op_flags, async_file_io_dispatcher_base::completion_t *>> &callbacks)
{
    if(!ops.empty() && ops.size()!=callbacks.size())
        BOOST_AFIO_THROW(std::runtime_error("The sequence of preconditions must either be empty or exactly the same length as callbacks."));
    std::vector<async_io_op> ret;
    ret.reserve(callbacks.size());
    std::vector<async_io_op>::const_iterator i;
    std::vector<std::pair<async_op_flags, async_file_io_dispatcher_base::completion_t *>>::const_iterator c;
    exception_ptr he;
    detail::immediate_async_ops immediates;
    if(ops.empty())
    {
        async_io_op empty;
        BOOST_FOREACH(auto & c, callbacks)
        {
            ret.push_back(chain_async_op(&he, immediates, (int) detail::OpType::UserCompletion, empty, c.first, &async_file_io_dispatcher_base::invoke_user_completion_fast, c.second));
        }
    }
    else for(i=ops.begin(), c=callbacks.begin(); i!=ops.end() && c!=callbacks.end(); ++i, ++c)
        ret.push_back(chain_async_op(&he, immediates, (int) detail::OpType::UserCompletion, *i, c->first, &async_file_io_dispatcher_base::invoke_user_completion_fast, c->second));
    return ret;
}
#endif
// Called in unknown thread
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC async_file_io_dispatcher_base::completion_returntype async_file_io_dispatcher_base::invoke_user_completion_slow(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::function<async_file_io_dispatcher_base::completion_t> callback)
{
    return callback(id, h, e);
}
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> async_file_io_dispatcher_base::completion(const std::vector<async_io_op> &ops, const std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> &callbacks)
{
    if(!ops.empty() && ops.size()!=callbacks.size())
        BOOST_AFIO_THROW(std::runtime_error("The sequence of preconditions must either be empty or exactly the same length as callbacks."));
    std::vector<async_io_op> ret;
    ret.reserve(callbacks.size());
    std::vector<async_io_op>::const_iterator i;
    std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>>::const_iterator c;
    exception_ptr he;
    detail::immediate_async_ops immediates;
    if(ops.empty())
    {
        async_io_op empty;
        BOOST_FOREACH(auto & c, callbacks)
        {
            ret.push_back(chain_async_op(&he, immediates, (int) detail::OpType::UserCompletion, empty, c.first, &async_file_io_dispatcher_base::invoke_user_completion_slow, c.second));
        }
    }
    else for(i=ops.begin(), c=callbacks.begin(); i!=ops.end() && c!=callbacks.end(); ++i, ++c)
            ret.push_back(chain_async_op(&he, immediates, (int) detail::OpType::UserCompletion, *i, c->first, &async_file_io_dispatcher_base::invoke_user_completion_slow, c->second));
    return ret;
}

// Called in unknown thread
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void async_file_io_dispatcher_base::complete_async_op(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr e)
{
    detail::immediate_async_ops immediates;
    std::shared_ptr<detail::async_file_io_dispatcher_op> thisop;
    std::vector<std::pair<size_t, std::shared_ptr<detail::async_file_io_dispatcher_op>>> completions;
    bool docompletions=false;
    BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock)
    {
        // Find me in ops, remove my completions and delete me from extant ops
        auto it=p->ops.find(id);
        if(p->ops.end()==it)
        {
#ifndef NDEBUG
            std::vector<size_t> opsids;
            BOOST_FOREACH(auto &i, p->ops)
            {
                opsids.push_back(i.first);
            }
            std::sort(opsids.begin(), opsids.end());
#endif
            BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this operation in list of currently executing operations"));
        }
        thisop=it->second;
        // Erase me from ops
        p->ops.erase(it);
        // Lookup any completions before losing opslock
        docompletions=!thisop->completions.empty();
    }
    BOOST_END_MEMORY_TRANSACTION(p->opslock)
    // opslock is now free
    if(docompletions)
    {
        completions.reserve(thisop->completions.size());
        BOOST_FOREACH(auto &c, thisop->completions)
        {
            std::pair<const size_t, std::shared_ptr<detail::async_file_io_dispatcher_op>> *temp=nullptr;
            BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock)
            {
                auto it=p->ops.find(c.first);
                if(p->ops.end()==it)
                    BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this completion operation in list of currently executing operations"));
                // unordered_map guaranteed to never relocate references
                temp=&(*it);
                BOOST_AFIO_DEBUG_PRINT("C %u > %u %p\n", (unsigned) id, (unsigned) c.first, h.get());
            }
            BOOST_END_MEMORY_TRANSACTION(p->opslock)
            completions.push_back(*temp);
        }
    }
    if(!completions.empty())
    {
        auto it=thisop->completions.begin();
        BOOST_FOREACH(auto &c, completions)
        {
            if(!!(c.second->flags & async_op_flags::ImmediateCompletion))
            {
                // If he was set up with a detached future, use that instead
                if(c.second->detached_promise)
                {
                    *c.second->h=c.second->detached_promise->get_future();
                    immediates.enqueue(std::bind(it->second, h, &e));
                }
                else
                    *c.second->h=immediates.enqueue(std::bind(it->second, h, &e));
            }
            else
            {
                // If he was set up with a detached future, use that instead
                if(c.second->detached_promise)
                {
                    *c.second->h=c.second->detached_promise->get_future();
                    p->pool->enqueue(std::bind(it->second, h, nullptr));
                }
                else
                    *c.second->h=p->pool->enqueue(std::bind(it->second, h, nullptr));
            }
            ++it;
        }
    }
    if(thisop->detached_promise)
    {
        auto delpromise=boost::afio::detail::Undoer([&thisop]{ thisop->detached_promise.reset(); });
        if(e)
            thisop->detached_promise->set_exception(e);
        else
            thisop->detached_promise->set_value(h);
    }
    BOOST_AFIO_DEBUG_PRINT("X %u %p\n", (unsigned) id, h.get());
}


#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
// Called in unknown thread 
template<class F, class... Args> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::shared_ptr<async_io_handle> async_file_io_dispatcher_base::invoke_async_op_completions(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, Args...), Args... args)
{
    try
    {
#ifndef NDEBUG
        // Find our op
        bool has_detached_promise=false;
        BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock)
        {
            auto it(p->ops.find(id));
            if(p->ops.end()==it)
            {
#ifndef NDEBUG
                std::vector<size_t> opsids;
                BOOST_FOREACH(auto &i, p->ops)
                {
                    opsids.push_back(i.first);
                }
                std::sort(opsids.begin(), opsids.end());
#endif
                BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this operation in list of currently executing operations"));
            }
            has_detached_promise=!!it->second->detached_promise;
        }
        BOOST_END_MEMORY_TRANSACTION(p->opslock)
#endif
        completion_returntype ret((static_cast<F *>(this)->*f)(id, h, e, args...));
        // If boolean is false, reschedule completion notification setting it to ret.second, otherwise complete now
        if(ret.first)
        {
            complete_async_op(id, ret.second);
        }
#ifndef NDEBUG
        else
        {
            // Make sure this was set up for deferred completion
            if(!has_detached_promise)
            {
                // If this trips, it means a completion handler tried to defer signalling
                // completion but it hadn't been set up with a detached future
                BOOST_AFIO_THROW_FATAL(std::runtime_error("Completion handler tried to defer completion but was not configured with detached future"));
            }
        }
#endif
        return ret.second;
    }
    catch(...)
    {
        exception_ptr e(afio::make_exception_ptr(afio::current_exception()));
        assert(e);
        BOOST_AFIO_DEBUG_PRINT("E %u begin\n", (unsigned) id);
        complete_async_op(id, h, e);
        BOOST_AFIO_DEBUG_PRINT("E %u end\n", (unsigned) id);
        BOOST_AFIO_RETHROW;
    }
}
#else

#undef BOOST_PP_LOCAL_MACRO
#define BOOST_PP_LOCAL_MACRO(N)                                                                     \
    template <class F                                                                               \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, class A)>                                                               \
	BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC                                                            \
    std::shared_ptr<async_io_handle>                                                        \
    async_file_io_dispatcher_base::invoke_async_op_completions                                      \
    (size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e,                        \
    completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr * \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, A))                                                                     \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_BINARY_PARAMS(N, A, a))     /* parameters end */                                  \
    {                                                                                               \
        try\
        {\
            /* Find our op*/ \
            bool has_detached_promise=false; \
            BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock) \
            { \
                auto it(p->ops.find(id)); \
                if(p->ops.end()==it) \
                { \
                    std::vector<size_t> opsids; \
                    BOOST_FOREACH(auto &i, p->ops) \
                    { \
                        opsids.push_back(i.first); \
                    } \
                    std::sort(opsids.begin(), opsids.end()); \
                    BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this operation in list of currently executing operations")); \
                } \
                has_detached_promise=!!it->second->detached_promise; \
            } \
            BOOST_END_MEMORY_TRANSACTION(p->opslock) \
            completion_returntype ret((static_cast<F *>(this)->*f)(id, h, e BOOST_PP_COMMA_IF(N) BOOST_PP_ENUM_PARAMS(N, a)));\
            /* If boolean is false, reschedule completion notification setting it to ret.second, otherwise complete now */ \
            if(ret.first)   \
            {\
                complete_async_op(id, ret.second);\
            }\
            else \
            { \
                /* Make sure this was set up for deferred completion */ \
                if(!has_detached_promise) \
                { \
                    /* If this trips, it means a completion handler tried to defer signalling*/ \
                    /* completion but it hadn't been set up with a detached future */ \
                    BOOST_AFIO_THROW_FATAL(std::runtime_error("Completion handler tried to defer completion but was not configured with detached future")); \
                } \
            } \
            return ret.second;\
        }\
        catch(...)\
        {\
            exception_ptr e(afio::make_exception_ptr(afio::current_exception()));\
            BOOST_AFIO_DEBUG_PRINT("E %u begin\n", (unsigned) id);\
            complete_async_op(id, h, e);\
            BOOST_AFIO_DEBUG_PRINT("E %u end\n", (unsigned) id);\
            BOOST_AFIO_RETHROW;\
        }\
    } //end of invoke_async_op_completions
  
#define BOOST_PP_LOCAL_LIMITS     (0, BOOST_AFIO_MAX_PARAMETERS) //should this be 0 or 1 for the min????
#include BOOST_PP_LOCAL_ITERATE()
#endif
#undef BOOST_PP_LOCAL_MACRO




#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
// You MUST hold opslock before entry!
template<class F, class... Args> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC async_io_op async_file_io_dispatcher_base::chain_async_op(exception_ptr *he, detail::immediate_async_ops &immediates, int optype, const async_io_op &precondition, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, Args...), Args... args)
{   
    size_t thisid=0;
    while(!(thisid=++p->monotoniccount));
#if 0 //ndef NDEBUG
    if(!p->ops.empty())
    {
        std::vector<size_t> opsids;
        BOOST_FOREACH(auto &i, p->ops)
        {
            opsids.push_back(i.first);
        }
        std::sort(opsids.begin(), opsids.end());
        assert(thisid==opsids.back()+1);
    }
#endif
    // Wrap supplied implementation routine with a completion dispatcher
    auto wrapperf=&async_file_io_dispatcher_base::invoke_async_op_completions<F, Args...>;
    // Bind supplied implementation routine to this, unique id and any args they passed
    typename detail::async_file_io_dispatcher_op::completion_t boundf(std::make_pair(thisid, std::bind(wrapperf, this, thisid, std::placeholders::_1, std::placeholders::_2, f, args...)));
    // Make a new async_io_op ready for returning
    async_io_op ret(this, thisid);
    auto thisop=std::make_shared<detail::async_file_io_dispatcher_op>((detail::OpType) optype, flags, ret.h, boundf);
    if(!!(flags & async_op_flags::DetachedFuture))
    {
        thisop->detached_promise.reset(new promise<std::shared_ptr<async_io_handle>>);
    }
    {
        auto item=std::make_pair(thisid, thisop);
        BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock)
        {
            auto opsit=p->ops.insert(std::move(item));
            assert(opsit.second);
        }
        BOOST_END_MEMORY_TRANSACTION(p->opslock)
    }
    BOOST_AFIO_DEBUG_PRINT("I %u < %u (%s)\n", (unsigned) thisid, (unsigned) precondition.id, detail::optypes[static_cast<int>(optype)]);
    auto unopsit=boost::afio::detail::Undoer([this, thisid](){
        BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock)
        {
            auto opsit=p->ops.find(thisid);
            p->ops.erase(opsit);
            BOOST_AFIO_DEBUG_PRINT("E X %u\n", (unsigned) thisid);
        }
        BOOST_END_MEMORY_TRANSACTION(p->opslock)
    });
    bool done=false;
    if(precondition.id)
    {
        // If still in flight, chain boundf to be executed when precondition completes
        BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock)
        {
            auto dep(p->ops.find(precondition.id));
            if(p->ops.end()!=dep)
            {
                dep->second->completions.push_back(boundf);
                done=true;
            }
        }
        BOOST_END_MEMORY_TRANSACTION(p->opslock)
    }
    auto undep=boost::afio::detail::Undoer([done, this, precondition](){
        if(done)
        {
            BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock)
            {
                auto dep(p->ops.find(precondition.id));
                dep->second->completions.pop_back();
            }
            BOOST_END_MEMORY_TRANSACTION(p->opslock)
        }
    });
    if(!done)
    {
        // Bind input handle now and queue immediately to next available thread worker
        std::shared_ptr<async_io_handle> h;
        if(precondition.h->valid())
        {
            // Boost's shared_future has get() as non-const which is weird, because it doesn't
            // delete the data after retrieval.
            shared_future<std::shared_ptr<async_io_handle>> &f=const_cast<shared_future<std::shared_ptr<async_io_handle>> &>(*precondition.h);
            // The reason we don't throw here is consistency: we throw at point of batch API entry
            // if any of the preconditions are errored, but if they became errored between then and
            // now we can hardly abort half way through a batch op without leaving around stuck ops.
            // No, instead we simulate as if the precondition had not yet become ready.
            if(precondition.h->has_exception())
                *he=get_exception_ptr(f);
            else
                h=f.get();
        }
        else if(precondition.id)
        {
            // It should never happen that precondition.id is valid but removed from extant ops
            // which indicates it completed and yet h remains invalid
            BOOST_AFIO_THROW_FATAL(std::runtime_error("Precondition was not in list of extant ops, yet its future is invalid. This should never happen for any real op, so it's probably memory corruption."));
        }
        if(!!(flags & async_op_flags::DetachedFuture))
        {
            *thisop->h=thisop->detached_promise->get_future();
        }
        if(!!(flags & async_op_flags::ImmediateCompletion))
        {
            *ret.h=immediates.enqueue(std::bind(boundf.second, h, he)).share();
        }
        else
            *ret.h=p->pool->enqueue(std::bind(boundf.second, h, nullptr)).share();
    }
    unopsit.dismiss();
    undep.dismiss();
    return ret;
}

#else

// removed because we can't have preprocessor directives inside of a macro
  #if 0 //ndef NDEBUG
        if(!p->ops.empty())
        {
            std::vector<size_t> opsids;
            BOOST_FOREACH(auto &i, p->ops)
            {
                opsids.push_back(i.first);
            }
            std::sort(opsids.begin(), opsids.end());
            assert(thisid==opsids.back()+1);
        }
    #endif

#undef BOOST_PP_LOCAL_MACRO
#define BOOST_PP_LOCAL_MACRO(N)                                                                     \
    template <class F                                /* template start */                           \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, class A)>                /* template end */                             \
	BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC                                                            \
    async_io_op                                      /* return type */                              \
    async_file_io_dispatcher_base::chain_async_op       /* function name */             \
    (exception_ptr *he, detail::immediate_async_ops &immediates, int optype,           /* parameters start */          \
    const async_io_op &precondition,async_op_flags flags,                                           \
    completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr * \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, A))                                                                     \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_BINARY_PARAMS(N, A, a))     /* parameters end */                                  \
    {\
        size_t thisid=0;\
        while(!(thisid=++p->monotoniccount));\
        /* Wrap supplied implementation routine with a completion dispatcher*/ \
        auto wrapperf=&async_file_io_dispatcher_base::invoke_async_op_completions<F BOOST_PP_COMMA_IF(N) BOOST_PP_ENUM_PARAMS(N, A)>;\
        /* Bind supplied implementation routine to this, unique id and any args they passed*/ \
        typename detail::async_file_io_dispatcher_op::completion_t boundf(std::make_pair(thisid, std::bind(wrapperf, this, thisid, std::placeholders::_1, std::placeholders::_2, f BOOST_PP_COMMA_IF(N) BOOST_PP_ENUM_PARAMS(N, a))));\
        /* Make a new async_io_op ready for returning*/\
        async_io_op ret(this, thisid);\
        auto thisop=std::make_shared<detail::async_file_io_dispatcher_op>((detail::OpType) optype, flags, ret.h, boundf); \
        if(!!(flags & async_op_flags::DetachedFuture)) \
        { \
            thisop->detached_promise.reset(new promise<std::shared_ptr<async_io_handle>>); \
        } \
        { \
            auto item=std::make_pair(thisid, thisop); \
            BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock) \
            { \
                auto opsit=p->ops.insert(std::move(item)); \
                assert(opsit.second); \
            } \
            BOOST_END_MEMORY_TRANSACTION(p->opslock) \
        } \
        BOOST_AFIO_DEBUG_PRINT("I %u < %u (%s)\n", (unsigned) thisid, (unsigned) precondition.id, detail::optypes[static_cast<int>(optype)]); \
        auto unopsit=boost::afio::detail::Undoer([this, thisid](){ \
            BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock) \
            { \
                auto opsit=p->ops.find(thisid); \
                p->ops.erase(opsit); \
                BOOST_AFIO_DEBUG_PRINT("E X %u\n", (unsigned) thisid); \
            } \
            BOOST_END_MEMORY_TRANSACTION(p->opslock) \
        }); \
        bool done=false;\
        if(precondition.id)\
        {\
            /* If still in flight, chain boundf to be executed when precondition completes*/ \
            BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock) \
            { \
                auto dep(p->ops.find(precondition.id)); \
                if(p->ops.end()!=dep) \
                { \
                    dep->second->completions.push_back(boundf); \
                    done=true; \
                } \
            } \
            BOOST_END_MEMORY_TRANSACTION(p->opslock) \
        }\
        auto undep=boost::afio::detail::Undoer([done, this, precondition](){\
            if(done)\
            {\
                BOOST_BEGIN_MEMORY_TRANSACTION(p->opslock) \
                { \
                    auto dep(p->ops.find(precondition.id)); \
                    dep->second->completions.pop_back(); \
                } \
                BOOST_END_MEMORY_TRANSACTION(p->opslock) \
            }\
        });\
        if(!done)\
        {\
            /* Bind input handle now and queue immediately to next available thread worker*/ \
            std::shared_ptr<async_io_handle> h;\
            if(precondition.h->valid()) \
            { \
                /* Boost's shared_future has get() as non-const which is weird, because it doesn't*/ \
                /* delete the data after retrieval.*/ \
                shared_future<std::shared_ptr<async_io_handle>> &f=const_cast<shared_future<std::shared_ptr<async_io_handle>> &>(*precondition.h);\
                /* The reason we don't throw here is consistency: we throw at point of batch API entry*/ \
                /* if any of the preconditions are errored, but if they became errored between then and*/ \
                /* now we can hardly abort half way through a batch op without leaving around stuck ops.*/ \
                /* No, instead we simulate as if the precondition had not yet become ready.*/ \
                if(precondition.h->has_exception()) \
                    *he=get_exception_ptr(f);\
                else \
                    h=f.get();\
            }\
            else if(precondition.id)\
            {\
                /* It should never happen that precondition.id is valid but removed from extant ops*/\
                /* which indicates it completed and yet h remains invalid*/\
                BOOST_AFIO_THROW_FATAL(std::runtime_error("Precondition was not in list of extant ops, yet its future is invalid. This should never happen for any real op, so it's probably memory corruption.")); \
            }\
            if(!!(flags & async_op_flags::ImmediateCompletion))\
            {\
                *ret.h=immediates.enqueue(std::bind(boundf.second, h, he)).share();\
            }\
            else\
                *ret.h=p->pool->enqueue(std::bind(boundf.second, h, nullptr)).share();\
        }\
        unopsit.dismiss();\
        undep.dismiss();\
        return ret;\
    }

  
#define BOOST_PP_LOCAL_LIMITS     (0, BOOST_AFIO_MAX_PARAMETERS) //should this be 0 or 1 for the min????
#include BOOST_PP_LOCAL_ITERATE()
#undef BOOST_PP_LOCAL_MACRO

#endif

// General non-specialised implementation taking some arbitrary parameter T with precondition
template<class F, class T> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<async_io_op> &preconditions, const std::vector<T> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, T))
{
    std::vector<async_io_op> ret;
    ret.reserve(container.size());
    assert(preconditions.size()==container.size());
    if(preconditions.size()!=container.size())
        BOOST_AFIO_THROW(std::runtime_error("preconditions size does not match size of ops data"));
    exception_ptr he;
    detail::immediate_async_ops immediates;
    auto precondition_it=preconditions.cbegin();
    auto container_it=container.cbegin();
    for(; precondition_it!=preconditions.cend() && container_it!=container.cend(); ++precondition_it, ++container_it)
        ret.push_back(chain_async_op(&he, immediates, optype, *precondition_it, flags, f, *container_it));
    return ret;
}
// General non-specialised implementation taking some arbitrary parameter T without precondition
template<class F, class T> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<T> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, T))
{
    std::vector<async_io_op> ret;
    ret.reserve(container.size());
    async_io_op precondition;
    exception_ptr he;
    detail::immediate_async_ops immediates;
    BOOST_FOREACH(auto &i, container)
    {
        ret.push_back(chain_async_op(&he, immediates, optype, precondition, flags, f, i));
    }
    return ret;
}
// Generic op receiving specialisation i.e. precondition is also input op. Skips sanity checking.
template<class F> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<async_io_op> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_io_op))
{
    std::vector<async_io_op> ret;
    ret.reserve(container.size());
    exception_ptr he;
    detail::immediate_async_ops immediates;
    BOOST_FOREACH(auto &i, container)
    {
        ret.push_back(chain_async_op(&he, immediates, optype, i, flags, f, i));
    }
    return ret;
}
// Dir/file open specialisation
template<class F> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<async_path_op_req> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_path_op_req))
{
    std::vector<async_io_op> ret;
    ret.reserve(container.size());
    exception_ptr he;
    detail::immediate_async_ops immediates;
    BOOST_FOREACH(auto &i, container)
    {
        ret.push_back(chain_async_op(&he, immediates, optype, i.precondition, flags, f, i));
    }
    return ret;
}
// Data read and write specialisation
template<class F, bool iswrite> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<detail::async_data_op_req_impl<iswrite>> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, detail::async_data_op_req_impl<iswrite>))
{
    std::vector<async_io_op> ret;
    ret.reserve(container.size());
    exception_ptr he;
    detail::immediate_async_ops immediates;
    BOOST_FOREACH(auto &i, container)
    {
        ret.push_back(chain_async_op(&he, immediates, optype, i.precondition, flags, f, i));
    }
    return ret;
}
// Directory enumerate specialisation
template<class F> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::pair<std::vector<future<std::pair<std::vector<directory_entry>, bool>>>, std::vector<async_io_op>> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<async_enumerate_op_req> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_enumerate_op_req, std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> ret))
{
    typedef std::pair<std::vector<directory_entry>, bool> retitemtype;
    std::vector<async_io_op> ret;
    std::vector<future<retitemtype>> retfutures;
    ret.reserve(container.size());
    retfutures.reserve(container.size());
    exception_ptr he;
    detail::immediate_async_ops immediates;
    BOOST_FOREACH(auto &i, container)
    {
        // Unfortunately older C++0x compilers don't cope well with feeding move only std::future<> into std::bind
        auto transport=std::make_shared<promise<retitemtype>>();
        retfutures.push_back(std::move(transport->get_future()));
        ret.push_back(chain_async_op(&he, immediates, optype, i.precondition, flags, f, i, transport));
    }
    return std::make_pair(std::move(retfutures), std::move(ret));
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> async_file_io_dispatcher_base::adopt(const std::vector<std::shared_ptr<async_io_handle>> &hs)
{
    return chain_async_ops(0, hs, async_op_flags::ImmediateCompletion, &async_file_io_dispatcher_base::doadopt);
}

namespace detail
{
    struct barrier_count_completed_state
    {
        atomic<size_t> togo;
        std::vector<std::pair<size_t, std::shared_ptr<async_io_handle>>> out;
        std::vector<std::shared_ptr<shared_future<std::shared_ptr<async_io_handle>>>> insharedstates;
        barrier_count_completed_state(const std::vector<async_io_op> &ops) : togo(ops.size()), out(ops.size())
        {
            insharedstates.reserve(ops.size());
            BOOST_FOREACH(auto &i, ops)
            {
                insharedstates.push_back(i.h);
            }
        }
    };
}

/* This is extremely naughty ... you really shouldn't be using templates to hide implementation
types, but hey it works and is non-header so so what ...
*/
//template<class T> async_file_io_dispatcher_base::completion_returntype async_file_io_dispatcher_base::dobarrier(size_t id, std::shared_ptr<async_io_handle> h, T state);
template<> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC async_file_io_dispatcher_base::completion_returntype async_file_io_dispatcher_base::dobarrier<std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t>>(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t> state)
{
    size_t idx=state.second;
    detail::barrier_count_completed_state &s=*state.first;
    s.out[idx]=std::make_pair(id, h); // This might look thread unsafe, but each idx is unique
    if(--state.first->togo)
        return std::make_pair(false, h);
#if 1
    // On the basis that probably the preceding decrementing thread has yet to signal their future,
    // give up my timeslice
    boost::this_thread::yield();
#endif
#if 1
    // Rather than potentially expend a syscall per wait on each input op to complete, compose a list of input futures and wait on them all
    std::vector<shared_future<std::shared_ptr<async_io_handle>>> notready;
    notready.reserve(s.insharedstates.size()-1);
    for(idx=0; idx<s.insharedstates.size(); idx++)
    {
        shared_future<std::shared_ptr<async_io_handle>> &f=*s.insharedstates[idx];
        if(idx==state.second || f.is_ready()) continue;
        notready.push_back(f);
    }
    if(!notready.empty())
        boost::wait_for_all(notready.begin(), notready.end());
#endif
    // Last one just completed, so issue completions for everything in out except me
    for(idx=0; idx<s.out.size(); idx++)
    {
        if(idx==state.second) continue;
        shared_future<std::shared_ptr<async_io_handle>> &thisresult=*s.insharedstates[idx];
        // TODO: If I include the wait_for_all above, we need only fetch this if thisresult.has_exception().
        exception_ptr e(get_exception_ptr(thisresult));
        complete_async_op(s.out[idx].first, s.out[idx].second, e);
    }
    // Am I being called because my precondition threw an exception so we're actually currently inside an exception catch?
    // If so then duplicate the same exception throw
    if(*e)
        rethrow_exception(*e);
    else
        return std::make_pair(true, h);
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> async_file_io_dispatcher_base::barrier(const std::vector<async_io_op> &ops)
{
#if BOOST_AFIO_VALIDATE_INPUTS
        BOOST_FOREACH(auto &i, ops)
        {
            if(!i.validate(false))
                BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
        }
#endif
    // Create a shared state for the completions to be attached to all the items we are waiting upon
    auto state(std::make_shared<detail::barrier_count_completed_state>(ops));
    // Create each of the parameters to be sent to each dobarrier
    std::vector<std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t>> statev;
    statev.reserve(ops.size());
    size_t idx=0;
    BOOST_FOREACH(auto &op, ops)
    {
        statev.push_back(std::make_pair(state, idx++));
    }
    return chain_async_ops((int) detail::OpType::barrier, ops, statev, async_op_flags::ImmediateCompletion|async_op_flags::DetachedFuture, &async_file_io_dispatcher_base::dobarrier<std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t>>);
}

namespace detail {
    class async_file_io_dispatcher_compat : public async_file_io_dispatcher_base
    {
        // Called in unknown thread
        completion_returntype dodir(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *e, async_path_op_req req)
        {
            int ret=0;
            req.flags=fileflags(req.flags)|file_flags::int_opening_dir|file_flags::Read;
            if(!!(req.flags & (file_flags::Create|file_flags::CreateOnlyIfNotExist)))
            {
                ret=BOOST_AFIO_POSIX_MKDIR(req.path.c_str(), 0x1f8/*770*/);
                if(-1==ret && EEXIST==errno)
                {
                    // Ignore already exists unless we were asked otherwise
                    if(!(req.flags & file_flags::CreateOnlyIfNotExist))
                        ret=0;
                }
                BOOST_AFIO_ERRHOSFN(ret, req.path);
                req.flags=req.flags&~(file_flags::Create|file_flags::CreateOnlyIfNotExist);
            }

            BOOST_AFIO_POSIX_STAT_STRUCT s={0};
            ret=BOOST_AFIO_POSIX_STAT(req.path.c_str(), &s);
            if(0==ret && S_IFDIR!=(s.st_mode&S_IFDIR))
                BOOST_AFIO_THROW(std::runtime_error("Not a directory"));
            if(!(req.flags & file_flags::UniqueDirectoryHandle) && !!(req.flags & file_flags::Read) && !(req.flags & file_flags::Write))
            {
                // Return a copy of the one in the dir cache if available
                return std::make_pair(true, p->get_handle_to_dir(this, id, e, req, &async_file_io_dispatcher_compat::dofile));
            }
            else
            {
                return dofile(id, _, e, req);
            }
        }
        // Called in unknown thread
        completion_returntype dormdir(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *, async_path_op_req req)
        {
            req.flags=fileflags(req.flags);
            BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_RMDIR(req.path.c_str()), req.path);
            auto ret=std::make_shared<async_io_handle_posix>(this, std::shared_ptr<async_io_handle>(), req.path, req.flags, false, -999);
            return std::make_pair(true, ret);
        }
        // Called in unknown thread
        completion_returntype dofile(size_t id, std::shared_ptr<async_io_handle>, exception_ptr *e, async_path_op_req req)
        {
            int flags=0;
            std::shared_ptr<async_io_handle> dirh;
            req.flags=fileflags(req.flags);
            if(!!(req.flags & file_flags::Read) && !!(req.flags & file_flags::Write)) flags|=O_RDWR;
            else if(!!(req.flags & file_flags::Read)) flags|=O_RDONLY;
            else if(!!(req.flags & file_flags::Write)) flags|=O_WRONLY;
            if(!!(req.flags & file_flags::Append)) flags|=O_APPEND;
            if(!!(req.flags & file_flags::Truncate)) flags|=O_TRUNC;
            if(!!(req.flags & file_flags::CreateOnlyIfNotExist)) flags|=O_EXCL|O_CREAT;
            else if(!!(req.flags & file_flags::Create)) flags|=O_CREAT;
#ifdef O_DIRECT
            if(!!(req.flags & file_flags::OSDirect)) flags|=O_DIRECT;
#endif
#ifdef O_SYNC
            if(!!(req.flags & file_flags::AlwaysSync)) flags|=O_SYNC;
#endif
            if(!!(req.flags & file_flags::int_opening_dir))
            {
#ifdef O_DIRECTORY
                flags|=O_DIRECTORY;
#endif
                // Some POSIXs don't like opening directories without buffering.
                if(!!(req.flags & file_flags::OSDirect))
                {
                    req.flags=req.flags & ~file_flags::OSDirect;
#ifdef O_DIRECT
                    flags&=~O_DIRECT;
#endif
                }
            }
            if(!!(req.flags & file_flags::FastDirectoryEnumeration))
                dirh=p->get_handle_to_containing_dir(this, id, e, req, &async_file_io_dispatcher_compat::dofile);
            // If writing and SyncOnClose and NOT synchronous, turn on SyncOnClose
            auto ret=std::make_shared<async_io_handle_posix>(this, dirh, req.path, req.flags, (file_flags::SyncOnClose|file_flags::Write)==(req.flags & (file_flags::SyncOnClose|file_flags::Write|file_flags::AlwaysSync)),
                BOOST_AFIO_POSIX_OPEN(req.path.c_str(), flags, 0x1b0/*660*/));
            static_cast<async_io_handle_posix *>(ret.get())->do_add_io_handle_to_parent();
            if(!(req.flags & file_flags::int_opening_dir) && !(req.flags & file_flags::int_opening_link) && !!(req.flags & file_flags::OSMMap))
                ret->try_mapfile();
            return std::make_pair(true, ret);
        }
        // Called in unknown thread
        completion_returntype dormfile(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *, async_path_op_req req)
        {
            req.flags=fileflags(req.flags);
            BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_UNLINK(req.path.c_str()), req.path);
            auto ret=std::make_shared<async_io_handle_posix>(this, std::shared_ptr<async_io_handle>(), req.path, req.flags, false, -999);
            return std::make_pair(true, ret);
        }
        // Called in unknown thread
        completion_returntype dosymlink(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, async_path_op_req req)
        {
#ifdef WIN32
            BOOST_AFIO_THROW(std::runtime_error("Creating symbolic links via MSVCRT is not supported on Windows."));
#else
            req.flags=fileflags(req.flags)|file_flags::int_opening_link;
            BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_SYMLINK(h->path().c_str(), req.path.c_str()), req.path);
            BOOST_AFIO_POSIX_STAT_STRUCT s={0};
            BOOST_AFIO_ERRHOS(BOOST_AFIO_POSIX_STAT(req.path.c_str(), &s));
            if(S_IFDIR!=(s.st_mode&S_IFDIR))
                return dofile(id, h, e, req);
            else
                return dodir(id, h, e, req);
#endif
        }
        // Called in unknown thread
        completion_returntype dormsymlink(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *, async_path_op_req req)
        {
            req.flags=fileflags(req.flags);
            BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_UNLINK(req.path.c_str()), req.path);
            auto ret=std::make_shared<async_io_handle_posix>(this, std::shared_ptr<async_io_handle>(), req.path, req.flags, false, -999);
            return std::make_pair(true, ret);
        }
        // Called in unknown thread
        completion_returntype dosync(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, async_io_op)
        {
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            off_t bytestobesynced=p->write_count_since_fsync();
            if(bytestobesynced)
                BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_FSYNC(p->fd), p->path());
            p->has_ever_been_fsynced=true;
            p->byteswrittenatlastfsync+=bytestobesynced;
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        completion_returntype doclose(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, async_io_op)
        {
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            if(!!(p->flags() & file_flags::int_opening_dir) && !(p->flags() & file_flags::UniqueDirectoryHandle) && !!(p->flags() & file_flags::Read) && !(p->flags() & file_flags::Write))
            {
                // As this is a directory which may be a fast directory enumerator, ignore close
            }
            else
            {
                p->close();
            }
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        completion_returntype doread(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, detail::async_data_op_req_impl<false> req)
        {
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            ssize_t bytesread=0, bytestoread=0;
            iovec v;
            BOOST_AFIO_DEBUG_PRINT("R %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
            BOOST_FOREACH(auto &b, req.buffers)
            {   
                BOOST_AFIO_DEBUG_PRINT("  R %u: %p %u\n", (unsigned) id, boost::asio::buffer_cast<const void *>(b), (unsigned) boost::asio::buffer_size(b));
            }
#endif
            if(p->mapaddr)
            {
                void *addr=(void *)((char *)p->mapaddr + req.where);
                BOOST_FOREACH(auto &b, req.buffers)
                {
                    memcpy(boost::asio::buffer_cast<void *>(b), addr, boost::asio::buffer_size(b));
                    addr=(void *)((char *)addr + boost::asio::buffer_size(b));
                }
                return std::make_pair(true, h);
            }
            std::vector<iovec> vecs;
            vecs.reserve(req.buffers.size());
            BOOST_FOREACH(auto &b, req.buffers)
            {
                v.iov_base=boost::asio::buffer_cast<void *>(b);
                v.iov_len=boost::asio::buffer_size(b);
                bytestoread+=v.iov_len;
                vecs.push_back(v);
            }
            for(size_t n=0; n<vecs.size(); n+=IOV_MAX)
            {
                ssize_t _bytesread;
                BOOST_AFIO_ERRHOSFN((int) (_bytesread=preadv(p->fd, (&vecs.front())+n, std::min((int) (vecs.size()-n), IOV_MAX), req.where+bytesread)), p->path());
                p->bytesread+=_bytesread;
                bytesread+=_bytesread;
            }
            if(bytesread!=bytestoread)
                BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to read all buffers"));
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        completion_returntype dowrite(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, detail::async_data_op_req_impl<true> req)
        {
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            ssize_t byteswritten=0, bytestowrite=0;
            iovec v;
            std::vector<iovec> vecs;
            vecs.reserve(req.buffers.size());
            BOOST_AFIO_DEBUG_PRINT("W %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
            BOOST_FOREACH(auto &b, req.buffers)
            {   
                BOOST_AFIO_DEBUG_PRINT("  W %u: %p %u\n", (unsigned) id, boost::asio::buffer_cast<const void *>(b), (unsigned) boost::asio::buffer_size(b));
            }
#endif
            BOOST_FOREACH(auto &b, req.buffers)
            {
                v.iov_base=(void *) boost::asio::buffer_cast<const void *>(b);
                v.iov_len=boost::asio::buffer_size(b);
                bytestowrite+=v.iov_len;
                vecs.push_back(v);
            }
            for(size_t n=0; n<vecs.size(); n+=IOV_MAX)
            {
                ssize_t _byteswritten;
                BOOST_AFIO_ERRHOSFN((int) (_byteswritten=pwritev(p->fd, (&vecs.front())+n, std::min((int) (vecs.size()-n), IOV_MAX), req.where+byteswritten)), p->path());
                p->byteswritten+=_byteswritten;
                byteswritten+=_byteswritten;
            }
            if(byteswritten!=bytestowrite)
                BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to write all buffers"));
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        completion_returntype dotruncate(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, off_t newsize)
        {
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            BOOST_AFIO_DEBUG_PRINT("T %u %p (%c)\n", (unsigned) id, h.get(), p->path().native().back());
            BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_FTRUNCATE(p->fd, newsize), p->path());
            return std::make_pair(true, h);
        }
#ifdef __linux__
        static int int_getdents(int fd, char *buf, int count) { return syscall(SYS_getdents64, fd, buf, count); }
#endif
        // Called in unknown thread
        completion_returntype doenumerate(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, async_enumerate_op_req req, std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> ret)
        {
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            try
            {
#ifdef WIN32
                BOOST_AFIO_THROW(std::runtime_error("Enumerating directories via MSVCRT is not supported."));
#else
#ifdef __linux__
                // Unlike FreeBSD, Linux doesn't define a getdents() function, so we'll do that here.
                typedef int (*getdents64_t)(int, char *, int);
                getdents64_t getdents=(getdents64_t)(&async_file_io_dispatcher_compat::int_getdents);
                typedef dirent64 dirent;
#endif
                auto buffer=std::unique_ptr<dirent[]>(new dirent[req.maxitems]);
                if(req.restart)
                {
#ifdef __linux__
                    BOOST_AFIO_ERRHOS(lseek64(p->fd, 0, SEEK_SET));
#else
                    BOOST_AFIO_ERRHOS(lseek(p->fd, 0, SEEK_SET));
#endif
                }
                int bytes;
                bool done;
                do
                {
                    bytes=getdents(p->fd, (char *) buffer.get(), sizeof(dirent)*req.maxitems);
                    if(-1==bytes && EINVAL==errno)
                    {
                        req.maxitems++;
                        buffer=std::unique_ptr<dirent[]>(new dirent[req.maxitems]);
                        done=false;
                    }
                    else done=true;
                } while(!done);
                BOOST_AFIO_ERRHOS(bytes);
                if(!bytes)
                {
                    ret->set_value(std::make_pair(std::vector<directory_entry>(), false));
                    return std::make_pair(true, h);
                }
                VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(buffer.get(), bytes);
                bool thisbatchdone=(sizeof(dirent)*req.maxitems-bytes)>sizeof(dirent);
                std::vector<directory_entry> _ret;
                _ret.reserve(req.maxitems);
                directory_entry item;
                // This is what POSIX returns with getdents()
                item.have_metadata=item.have_metadata|metadata_flags::ino|metadata_flags::type;
                done=false;
                for(dirent *dent=buffer.get(); !done; dent=(dirent *)((size_t) dent + dent->d_reclen))
                {
                    if((bytes-=dent->d_reclen)<=0) done=true;
                    if(!dent->d_ino)
                        continue;
                    size_t length=strchr(dent->d_name, 0)-dent->d_name;
                    if(length<=2 && '.'==dent->d_name[0])
                        if(1==length || '.'==dent->d_name[1]) continue;
                    if(!req.glob.empty() && fnmatch(req.glob.native().c_str(), dent->d_name, 0)) continue;
                    std::filesystem::path::string_type leafname(dent->d_name, length);
                    item.leafname=std::move(leafname);
                    item.stat.st_ino=dent->d_ino;
                    char d_type=dent->d_type;
                    if(DT_UNKNOWN==d_type)
                        item.have_metadata=item.have_metadata&~metadata_flags::type;
                    else
                    {
                        item.have_metadata=item.have_metadata|metadata_flags::type;
                        switch(d_type)
                        {
                        case DT_BLK:
                            item.stat.st_type=S_IFBLK;
                            break;
                        case DT_CHR:
                            item.stat.st_type=S_IFCHR;
                            break;
                        case DT_DIR:
                            item.stat.st_type=S_IFDIR;
                            break;
                        case DT_FIFO:
                            item.stat.st_type=S_IFIFO;
                            break;
                        case DT_LNK:
                            item.stat.st_type=S_IFLNK;
                            break;
                        case DT_REG:
                            item.stat.st_type=S_IFREG;
                            break;
                        case DT_SOCK:
                            item.stat.st_type=S_IFSOCK;
                            break;
                        default:
                            item.have_metadata=item.have_metadata&~metadata_flags::type;
                            item.stat.st_type=0;
                            break;
                        }
                    }
                    _ret.push_back(std::move(item));
                }
                ret->set_value(std::make_pair(std::move(_ret), !thisbatchdone));
#endif
                return std::make_pair(true, h);
            }
            catch(...)
            {
                ret->set_exception(afio::make_exception_ptr(afio::current_exception()));
                throw;
            }
        }

    public:
        async_file_io_dispatcher_compat(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask) : async_file_io_dispatcher_base(threadpool, flagsforce, flagsmask)
        {
        }


        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> dir(const std::vector<async_path_op_req> &reqs)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::dir, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dodir);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> rmdir(const std::vector<async_path_op_req> &reqs)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::rmdir, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dormdir);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> file(const std::vector<async_path_op_req> &reqs)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::file, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dofile);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> rmfile(const std::vector<async_path_op_req> &reqs)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::rmfile, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dormfile);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> symlink(const std::vector<async_path_op_req> &reqs)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::symlink, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dosymlink);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> rmsymlink(const std::vector<async_path_op_req> &reqs)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::rmsymlink, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dormsymlink);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> sync(const std::vector<async_io_op> &ops)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::sync, ops, async_op_flags::None, &async_file_io_dispatcher_compat::dosync);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> close(const std::vector<async_io_op> &ops)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::close, ops, async_op_flags::None, &async_file_io_dispatcher_compat::doclose);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> read(const std::vector<detail::async_data_op_req_impl<false>> &reqs)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::read, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::doread);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> write(const std::vector<detail::async_data_op_req_impl<true>> &reqs)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::write, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dowrite);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> truncate(const std::vector<async_io_op> &ops, const std::vector<off_t> &sizes)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::truncate, ops, sizes, async_op_flags::None, &async_file_io_dispatcher_compat::dotruncate);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::pair<std::vector<future<std::pair<std::vector<directory_entry>, bool>>>, std::vector<async_io_op>> enumerate(const std::vector<async_enumerate_op_req> &reqs)
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            BOOST_FOREACH(auto &i, reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::enumerate, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::doenumerate);
        }
    };
}

} } // namespace

#if defined(WIN32) && !defined(USE_POSIX_ON_WIN32)
#include "afio_iocp.ipp"
#endif

namespace boost { namespace afio {

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void directory_entry::_int_fetch(metadata_flags wanted, std::shared_ptr<async_io_handle> dirh)
{
#ifdef WIN32
    detail::async_file_io_dispatcher_windows *dispatcher=dynamic_cast<detail::async_file_io_dispatcher_windows *>(dirh->parent());
    if(dispatcher)
    {
        windows_nt_kernel::init();
        using namespace windows_nt_kernel;
        bool slowPath=(!!(wanted&metadata_flags::nlink) || !!(wanted&metadata_flags::blocks) || !!(wanted&metadata_flags::blksize));
        if(!slowPath)
        {
            // Fast path skips opening a handle per file by enumerating the containing directory using a glob
            // exactly matching the leafname. This is about 10x quicker, so it's very much worth it.
            BOOST_AFIO_TYPEALIGNMENT(8) std::filesystem::path::value_type buffer[sizeof(FILE_ALL_INFORMATION)/sizeof(std::filesystem::path::value_type)+32769];
            IO_STATUS_BLOCK isb={ 0 };
            UNICODE_STRING _glob;
            NTSTATUS ntstat;
            _glob.Buffer=const_cast<std::filesystem::path::value_type *>(leafname.c_str());
            _glob.MaximumLength=(_glob.Length=(USHORT) (leafname.native().size()*sizeof(std::filesystem::path::value_type)))+sizeof(std::filesystem::path::value_type);
            FILE_ID_FULL_DIR_INFORMATION *ffdi=(FILE_ID_FULL_DIR_INFORMATION *) buffer;
            ntstat=NtQueryDirectoryFile(dirh->native_handle(), NULL, NULL, NULL, &isb, ffdi, sizeof(buffer),
                FileIdFullDirectoryInformation, TRUE, &_glob, FALSE);
            if(STATUS_PENDING==ntstat)
                ntstat=NtWaitForSingleObject(dirh->native_handle(), FALSE, NULL);
            BOOST_AFIO_ERRHNTFN(ntstat, dirh->path());
            if(!!(wanted&metadata_flags::ino)) { stat.st_ino=ffdi->FileId.QuadPart; }
            if(!!(wanted&metadata_flags::type)) { stat.st_type=to_st_type(ffdi->FileAttributes); }
            if(!!(wanted&metadata_flags::atim)) { stat.st_atim=to_timepoint(ffdi->LastAccessTime); }
            if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=to_timepoint(ffdi->LastWriteTime); }
            if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=to_timepoint(ffdi->ChangeTime); }
            if(!!(wanted&metadata_flags::size)) { stat.st_size=ffdi->EndOfFile.QuadPart; }
            if(!!(wanted&metadata_flags::allocated)) { stat.st_allocated=ffdi->AllocationSize.QuadPart; }
            if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=to_timepoint(ffdi->CreationTime); }
        }
        else
        {
            // No choice here, open a handle and stat it.
            async_path_op_req req(dirh->path()/name(), file_flags::Read);
            auto fileh=dispatcher->dofile(0, std::shared_ptr<async_io_handle>(), nullptr, req).second;
            auto direntry=fileh->direntry(wanted);
            wanted=wanted & direntry.metadata_ready(); // direntry() can fail to fill some entries on Win XP
            if(!!(wanted&metadata_flags::dev)) { stat.st_dev=direntry.stat.st_dev; }
            if(!!(wanted&metadata_flags::ino)) { stat.st_ino=direntry.stat.st_ino; }
            if(!!(wanted&metadata_flags::type)) { stat.st_type=direntry.stat.st_mode; }
            if(!!(wanted&metadata_flags::mode)) { stat.st_mode=direntry.stat.st_mode; }
            if(!!(wanted&metadata_flags::nlink)) { stat.st_nlink=direntry.stat.st_nlink; }
            if(!!(wanted&metadata_flags::uid)) { stat.st_uid=direntry.stat.st_uid; }
            if(!!(wanted&metadata_flags::gid)) { stat.st_gid=direntry.stat.st_gid; }
            if(!!(wanted&metadata_flags::rdev)) { stat.st_rdev=direntry.stat.st_rdev; }
            if(!!(wanted&metadata_flags::atim)) { stat.st_atim=direntry.stat.st_atim; }
            if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=direntry.stat.st_mtim; }
            if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=direntry.stat.st_ctim; }
            if(!!(wanted&metadata_flags::size)) { stat.st_size=direntry.stat.st_size; }
            if(!!(wanted&metadata_flags::allocated)) { stat.st_allocated=direntry.stat.st_blocks*direntry.stat.st_blksize; }
            if(!!(wanted&metadata_flags::blocks)) { stat.st_blocks=direntry.stat.st_blocks; }
            if(!!(wanted&metadata_flags::blksize)) { stat.st_blksize=direntry.stat.st_blksize; }
#ifdef HAVE_STAT_FLAGS
            if(!!(wanted&metadata_flags::flags)) { stat.st_flags=direntry.stat.st_flags; }
#endif
#ifdef HAVE_STAT_GEN
            if(!!(wanted&metadata_flags::gen)) { stat.st_gen=direntry.stat.st_gen; }
#endif
#ifdef HAVE_BIRTHTIMESPEC
            if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=direntry.stat.st_birthtim; }
#endif
        }
    }
    else
#endif
    {
        BOOST_AFIO_POSIX_STAT_STRUCT s={0};
        std::filesystem::path path(dirh->path());
        path/=leafname;
        BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTAT(path.c_str(), &s), path);
        fill_stat_t(stat, s, wanted);
    }
    have_metadata=have_metadata | wanted;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::shared_ptr<async_file_io_dispatcher_base> make_async_file_io_dispatcher(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask)
{
#if defined(WIN32) && !defined(USE_POSIX_ON_WIN32)
    return std::make_shared<detail::async_file_io_dispatcher_windows>(threadpool, flagsforce, flagsmask);
#else
    return std::make_shared<detail::async_file_io_dispatcher_compat>(threadpool, flagsforce, flagsmask);
#endif
}

} } // namespace

#ifdef _MSC_VER
#pragma warning(pop)
#endif

