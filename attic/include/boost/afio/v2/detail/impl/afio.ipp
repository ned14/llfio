/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013-2015 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

//#define BOOST_AFIO_USE_CONCURRENT_UNORDERED_MAP
//#define BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH 1

#ifndef BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH
#define BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH 8
#endif
//#define USE_POSIX_ON_WIN32 // Useful for testing

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4456) // declaration hides previous local declaration
#pragma warning(disable: 4458) // declaration hides class member
#pragma warning(disable: 4996) // This function or variable may be unsafe
#endif

// This always compiles in input validation for this file only (the header file
// disables at the point of instance validation in release builds)
#if !defined(BOOST_AFIO_NEVER_VALIDATE_INPUTS) && !defined(BOOST_AFIO_COMPILING_FOR_GCOV)
#define BOOST_AFIO_VALIDATE_INPUTS 1
#endif

// Define this to have every allocated op take a backtrace from where it was allocated
#ifndef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
  #if !defined(NDEBUG) || defined(BOOST_AFIO_RUNNING_IN_CI)
    #define BOOST_AFIO_OP_STACKBACKTRACEDEPTH 8
  #endif
#endif
// Right now only Windows, Linux and FreeBSD supported
#if (!defined(__linux__) && !defined(__FreeBSD__) && !defined(WIN32)) || defined(BOOST_AFIO_COMPILING_FOR_GCOV)
#  undef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
#endif

// Define this to have every part of AFIO print, in extremely terse text, what it is doing and why.
#if (defined(BOOST_AFIO_DEBUG_PRINTING) && BOOST_AFIO_DEBUG_PRINTING)
#ifndef BOOST_AFIO_DEBUG_PRINTING
# define BOOST_AFIO_DEBUG_PRINTING 1
#endif
#ifdef WIN32
#define BOOST_AFIO_DEBUG_PRINT(...) \
    { \
    char buffer[16384]; \
    sprintf(buffer, __VA_ARGS__); \
    fprintf(stderr, buffer); \
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

#include "../../afio.hpp"
#ifndef BOOST_AFIO_DISABLE_VALGRIND
# include "../valgrind/memcheck.h"
#else
# define VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(addr, len)
#endif

#include <limits>
#include <random>
#include <unordered_map>
BOOST_AFIO_V2_NAMESPACE_BEGIN
  template<class Key, class T, class Hash=std::hash<Key>, class Pred=std::equal_to<Key>, class Alloc=std::allocator<std::pair<const Key, T>>> using engine_unordered_map_t=std::unordered_map<Key, T, Hash, Pred, Alloc>;
BOOST_AFIO_V2_NAMESPACE_END

#include <sys/types.h>
#ifdef __MINGW32__
// Workaround bad headers as usual in mingw
typedef __int64 off64_t;
#endif
#include <fcntl.h>
#include <sys/stat.h>
#ifdef __FreeBSD__
// Unfortunately these spazz all over the place which causes ambiguous symbol resolution for C++ thread
// So hack around the problem
#define thread freebsd_thread
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#undef thread
#define HAVE_STAT_FLAGS
#define HAVE_STAT_GEN
#define HAVE_BIRTHTIMESPEC
#endif
#ifdef __APPLE__
#define HAVE_STAT_FLAGS
#define HAVE_STAT_GEN
#define HAVE_BIRTHTIMESPEC
#endif
#ifdef WIN32
#ifndef S_IFSOCK
#define S_IFSOCK 0xC000
#endif
#ifndef S_IFBLK
#define S_IFBLK 0x6000
#endif
#ifndef S_IFIFO
#define S_IFIFO 0x1000
#endif
#ifndef S_IFLNK
#define S_IFLNK 0xA000
#endif
#endif
BOOST_AFIO_V2_NAMESPACE_BEGIN
static inline filesystem::file_type to_st_type(uint16_t mode)
{
    switch(mode & S_IFMT)
    {
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
    case S_IFBLK:
        return filesystem::file_type::block_file;
    case S_IFCHR:
        return filesystem::file_type::character_file;
    case S_IFDIR:
        return filesystem::file_type::directory_file;
    case S_IFIFO:
        return filesystem::file_type::fifo_file;
    case S_IFLNK:
        return filesystem::file_type::symlink_file;
    case S_IFREG:
        return filesystem::file_type::regular_file;
    case S_IFSOCK:
        return filesystem::file_type::socket_file;
    default:
        return filesystem::file_type::type_unknown;
#else
    case S_IFBLK:
        return filesystem::file_type::block;
    case S_IFCHR:
        return filesystem::file_type::character;
    case S_IFDIR:
        return filesystem::file_type::directory;
    case S_IFIFO:
        return filesystem::file_type::fifo;
    case S_IFLNK:
        return filesystem::file_type::symlink;
    case S_IFREG:
        return filesystem::file_type::regular;
    case S_IFSOCK:
        return filesystem::file_type::socket;
    default:
        return filesystem::file_type::unknown;
#endif
    }
}
#ifdef AT_FDCWD
static BOOST_CONSTEXPR_OR_CONST int at_fdcwd=AT_FDCWD;
#else
static BOOST_CONSTEXPR_OR_CONST int at_fdcwd=-100;
#endif
// Should give a compile time error on compilers with constexpr
static inline BOOST_CONSTEXPR_OR_CONST int check_fdcwd(int dirh) { return (dirh!=at_fdcwd) ? throw std::runtime_error("dirh is not at_fdcwd on a platform which does not have XXXat() API support") : 0; }
BOOST_AFIO_V2_NAMESPACE_END
#ifdef WIN32
// We also compile the posix compat layer for catching silly compile errors for POSIX
#include <io.h>
#include <direct.h>
#ifdef AT_FDCWD
#error AT_FDCWD should not be defined on MSVCRT, best check the thunk wrappers!
#endif
#define BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS 0
#define BOOST_AFIO_POSIX_MKDIRAT(dirh, path, mode) (check_fdcwd(dirh), _wmkdir(path))
#define BOOST_AFIO_POSIX_RMDIRAT(dirh, path) (check_fdcwd(dirh), _wrmdir(path))
#define BOOST_AFIO_POSIX_STAT_STRUCT struct __stat64
#define BOOST_AFIO_POSIX_STATAT(dirh, path, s) (check_fdcwd(dirh), _wstat64((path), (s)))
#define BOOST_AFIO_POSIX_LSTATAT(dirh, path, s) (check_fdcwd(dirh), _wstat64((path), (s)))
#define BOOST_AFIO_POSIX_FSTAT(fd, s) _fstat64((fd), (s))
#define BOOST_AFIO_POSIX_OPENAT(dirh, path, flags, mode) (check_fdcwd(dirh), _wopen((path), (flags), (mode)))
#define BOOST_AFIO_POSIX_CLOSE _close
#define BOOST_AFIO_POSIX_RENAMEAT(olddirh, oldpath, newdirh, newpath) (check_fdcwd(olddirh), check_fdcwd(newdirh), _wrename((oldpath), (newpath)))
#define AT_REMOVEDIR 0x200
#define BOOST_AFIO_POSIX_UNLINKAT(dirh, path, flags) (check_fdcwd(dirh), (flags&AT_REMOVEDIR) ? _wrmdir(path) : _wunlink(path))
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
#include <sys/mount.h>
#ifdef __linux__
# include <sys/statfs.h>
# include <mntent.h>
#endif
#include <limits.h>
// Does this POSIX provides at(dirh) support?
#ifdef AT_FDCWD
#define BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS 1
#define BOOST_AFIO_POSIX_MKDIRAT(dirh, path, mode) ::mkdirat((dirh), (path), (mode))
#define BOOST_AFIO_POSIX_RMDIRAT(dirh, path) ::rmdir((dirh), (path))
#define BOOST_AFIO_POSIX_STAT_STRUCT struct stat 
#define BOOST_AFIO_POSIX_STATAT(dirh, path, s) ::fstatat((dirh), (path), (s))
#define BOOST_AFIO_POSIX_LSTATAT(dirh, path, s) ::fstatat((dirh), (path), (s), AT_SYMLINK_NOFOLLOW)
#define BOOST_AFIO_POSIX_FSTAT ::fstat
#define BOOST_AFIO_POSIX_OPENAT(dirh, path, flags, mode) ::openat((dirh), (path), (flags), (mode))
#define BOOST_AFIO_POSIX_SYMLINKAT(from, dirh, to) ::symlinkat((from), (dirh), (to))
#define BOOST_AFIO_POSIX_CLOSE ::close
#define BOOST_AFIO_POSIX_RENAMEAT(olddirh, oldpath, newdirh, newpath) ::renameat((olddirh), (oldpath), (newdirh), (newpath))
#define BOOST_AFIO_POSIX_LINKAT(olddirh, oldpath, newdirh, newpath, flags) ::linkat((olddirh), (oldpath), (newdirh), (newpath), (flags))
#define BOOST_AFIO_POSIX_UNLINKAT(dirh, path, flags) ::unlinkat((dirh), (path), (flags))
#define BOOST_AFIO_POSIX_FSYNC ::fsync
#define BOOST_AFIO_POSIX_FTRUNCATE ::ftruncate
#define BOOST_AFIO_POSIX_MMAP ::mmap
#define BOOST_AFIO_POSIX_MUNMAP ::munmap
#else
#define BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS 0
#define BOOST_AFIO_POSIX_MKDIRAT(dirh, path, mode) (check_fdcwd(dirh), ::mkdir((path), (mode)))
#define BOOST_AFIO_POSIX_RMDIRAT(dirh, path) (check_fdcwd(dirh), ::rmdir(path))
#define BOOST_AFIO_POSIX_STAT_STRUCT struct stat 
#define BOOST_AFIO_POSIX_STATAT(dirh, path, s) (check_fdcwd(dirh), ::stat((path), (s)))
#define BOOST_AFIO_POSIX_LSTATAT(dirh, path, s) (check_fdcwd(dirh), ::lstat((path), (s)))
#define BOOST_AFIO_POSIX_FSTAT ::fstat
#define BOOST_AFIO_POSIX_OPENAT(dirh, path, flags, mode) (check_fdcwd(dirh), ::open((path), (flags), (mode)))
#define BOOST_AFIO_POSIX_SYMLINKAT(from, dirh, to) (check_fdcwd(dirh), ::symlink((from), (to)))
#define BOOST_AFIO_POSIX_CLOSE ::close
#define BOOST_AFIO_POSIX_RENAMEAT(olddirh, oldpath, newdirh, newpath) (check_fdcwd(olddirh), check_fdcwd(newdirh), ::rename((oldpath), (newpath)))
#define BOOST_AFIO_POSIX_LINKAT(olddirh, oldpath, newdirh, newpath, flags) (check_fdcwd(olddirh), check_fdcwd(newdirh), ::link((oldpath), (newpath)))
#define AT_REMOVEDIR 0x200
#define BOOST_AFIO_POSIX_UNLINKAT(dirh, path, flags) (check_fdcwd(dirh), (flags&AT_REMOVEDIR) ? ::rmdir(path) : ::unlink(path))
#define BOOST_AFIO_POSIX_FSYNC ::fsync
#define BOOST_AFIO_POSIX_FTRUNCATE ::ftruncate
#define BOOST_AFIO_POSIX_MMAP ::mmap
#define BOOST_AFIO_POSIX_MUNMAP ::munmap
#endif

BOOST_AFIO_V2_NAMESPACE_BEGIN
static inline chrono::system_clock::time_point to_timepoint(struct timespec ts)
{
    // Need to have this self-adapt to the STL being used
    static BOOST_CONSTEXPR_OR_CONST unsigned long long STL_TICKS_PER_SEC=(unsigned long long) chrono::system_clock::period::den/chrono::system_clock::period::num;
    static BOOST_CONSTEXPR_OR_CONST unsigned long long multiplier=STL_TICKS_PER_SEC>=1000000000ULL ? STL_TICKS_PER_SEC/1000000000ULL : 1;
    static BOOST_CONSTEXPR_OR_CONST unsigned long long divider=STL_TICKS_PER_SEC>=1000000000ULL ? 1 : 1000000000ULL/STL_TICKS_PER_SEC;
    // We make the big assumption that the STL's system_clock is based on the time_t epoch 1st Jan 1970.
    chrono::system_clock::duration duration(ts.tv_sec*STL_TICKS_PER_SEC+ts.tv_nsec*multiplier/divider);
    return chrono::system_clock::time_point(duration);
}
static inline void fill_stat_t(stat_t &stat, BOOST_AFIO_POSIX_STAT_STRUCT s, metadata_flags wanted)
{
    using namespace boost::afio;
    if(!!(wanted&metadata_flags::dev)) { stat.st_dev=s.st_dev; }
    if(!!(wanted&metadata_flags::ino)) { stat.st_ino=s.st_ino; }
    if(!!(wanted&metadata_flags::type)) { stat.st_type=to_st_type(s.st_mode); }
    if(!!(wanted&metadata_flags::perms)) { stat.st_perms=s.st_mode & 0xfff; }
    if(!!(wanted&metadata_flags::nlink)) { stat.st_nlink=s.st_nlink; }
    if(!!(wanted&metadata_flags::uid)) { stat.st_uid=s.st_uid; }
    if(!!(wanted&metadata_flags::gid)) { stat.st_gid=s.st_gid; }
    if(!!(wanted&metadata_flags::rdev)) { stat.st_rdev=s.st_rdev; }
#ifdef __ANDROID__
    if(!!(wanted&metadata_flags::atim)) { stat.st_atim=to_timepoint(*((struct timespec *)&s.st_atime)); }
    if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=to_timepoint(*((struct timespec *)&s.st_mtime)); }
    if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=to_timepoint(*((struct timespec *)&s.st_ctime)); }
#elif defined(__APPLE__)
    if(!!(wanted&metadata_flags::atim)) { stat.st_atim=to_timepoint(s.st_atimespec); }
    if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=to_timepoint(s.st_mtimespec); }
    if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=to_timepoint(s.st_ctimespec); }
#else  // Linux and BSD
    if(!!(wanted&metadata_flags::atim)) { stat.st_atim=to_timepoint(s.st_atim); }
    if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=to_timepoint(s.st_mtim); }
    if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=to_timepoint(s.st_ctim); }
#endif
    if(!!(wanted&metadata_flags::size)) { stat.st_size=s.st_size; }
    if(!!(wanted&metadata_flags::allocated)) { stat.st_allocated=(off_t) s.st_blocks*512; }
    if(!!(wanted&metadata_flags::blocks)) { stat.st_blocks=s.st_blocks; }
    if(!!(wanted&metadata_flags::blksize)) { stat.st_blksize=s.st_blksize; }
#ifdef HAVE_STAT_FLAGS
    if(!!(wanted&metadata_flags::flags)) { stat.st_flags=s.st_flags; }
#endif
#ifdef HAVE_STAT_GEN
    if(!!(wanted&metadata_flags::gen)) { stat.st_gen=s.st_gen; }
#endif
#ifdef HAVE_BIRTHTIMESPEC
#if defined(__APPLE__)
    if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=to_timepoint(s.st_birthtimespec); }
#else
    if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=to_timepoint(s.st_birthtim); }
#endif
#endif
    if(!!(wanted&metadata_flags::sparse)) { stat.st_sparse=((off_t) s.st_blocks*512)<(off_t) s.st_size; }
}
BOOST_AFIO_V2_NAMESPACE_END
#endif

#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
#  ifdef WIN32
BOOST_AFIO_V2_NAMESPACE_BEGIN
typedef std::vector<void *> stack_type;
inline void collect_stack(stack_type &stack)
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  stack.resize(BOOST_AFIO_OP_STACKBACKTRACEDEPTH);
  stack.resize(RtlCaptureStackBackTrace(0, (ULONG) stack.size(), &stack.front(), nullptr));
}
inline void print_stack(std::ostream &s, const stack_type &stack)
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  size_t n=0;
  for(void *addr: stack)
  {
      DWORD displ;
      IMAGEHLP_LINE64 ihl={ sizeof(IMAGEHLP_LINE64) };
      s << "    " << ++n << ". 0x" << std::hex << addr << std::dec << ": ";
      if(SymGetLineFromAddr64 && SymGetLineFromAddr64(GetCurrentProcess(), (size_t) addr, &displ, &ihl))
      {
          if(ihl.FileName && ihl.FileName[0])
          {
              char buffer[32769];
              int len=WideCharToMultiByte(CP_UTF8, 0, ihl.FileName, -1, buffer, sizeof(buffer), nullptr, nullptr);
              s.write(buffer, len-1);
              s << ":" << ihl.LineNumber << " (+0x" << std::hex << displ << ")" << std::dec;
          }
          else
              s << "unknown:0";
      }
      else
          s << "completely unknown";
      s << std::endl;
  }
}
BOOST_AFIO_V2_NAMESPACE_END
#  else
#    include <dlfcn.h>
// Set to 1 to use libunwind instead of glibc's stack backtracer
#    if 0
#      define UNW_LOCAL_ONLY
#      include <libunwind.h>
#    else
#      if defined(__linux__) || defined(__FreeBSD__) && !defined(__ANDROID__)
#        include <execinfo.h>
#        include <cxxabi.h>
#      endif
#    endif
BOOST_AFIO_V2_NAMESPACE_BEGIN
typedef std::vector<void *> stack_type;
inline void collect_stack(stack_type &stack)
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
  stack.resize(BOOST_AFIO_OP_STACKBACKTRACEDEPTH);
  stack.resize(backtrace(&stack.front(), stack.size()));
#endif
}
extern "C" const char *__progname;
inline void print_stack(std::ostream &s, const stack_type &stack)
{
  size_t n=0;
  for(void *addr: stack)
  {
      s << "    " << ++n << ". " << std::hex << addr << std::dec << ": ";
      Dl_info info;
      if(dladdr(addr, &info))
      {
          if(info.dli_fbase)
          {
            // This is hacky ...
            char buffer2[4096];
            sprintf(buffer2, "/usr/bin/addr2line -C -f -i -e \"%s\" %lx", info.dli_fname, (long)((size_t) addr - (size_t) info.dli_fbase));
            //std::cout << buffer2 << std::endl;
            FILE *ih=nullptr; // popen(buffer2, "r");  addr2line isn't currently working with my binaries on Linux nor FreeBSD, and it's very slow.
            auto unih=detail::Undoer([&ih]{ if(ih) pclose(ih); });
            bool done=false;
            if(ih)
            {
                size_t length=fread(buffer2, 1, sizeof(buffer2), ih);
                buffer2[length]=0;
                std::string buffer;
                for(size_t n=0; n<length-1; n++)
                {
                  if(buffer2[n]=='\n')
                    buffer.append("\n       ");
                  else
                    buffer.push_back(buffer2[n]);
                }
                if(buffer[0]!='?' && buffer[1]!='?')
                {
                  s << buffer << " (+0x" << std::hex << ((size_t) addr - (size_t) info.dli_saddr) << ")" << std::dec;
                  done=true;
                }
            }
            if(!done)
            {
              s << info.dli_fname << " (+0x" << std::hex << ((size_t) addr - (size_t) info.dli_fbase) << ") ";
              if(info.dli_saddr)
              {
                char *demangled=nullptr;
                auto undemangled=detail::Undoer([&demangled]{ if(demangled) free(demangled); });
                int status;
                if((demangled=abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status)))
                {
                  s << demangled << " (0x" << info.dli_saddr << "+0x" << std::hex << ((size_t) addr - (size_t) info.dli_saddr) << ")" << std::dec;
                  done=true;
                }
              }
              if(!done)
                s << info.dli_sname << " (0x" << info.dli_saddr << "+0x" << std::hex << ((size_t) addr - (size_t) info.dli_saddr) << ")" << std::dec;
            }
          }
          else
              s << "unknown:0";
      }
      else
          s << "completely unknown";
      s << std::endl;
  }
}
BOOST_AFIO_V2_NAMESPACE_END
#endif

BOOST_AFIO_V2_NAMESPACE_BEGIN
struct afio_exception_stack_entry
{
  std::string name;
  stack_type stack;
};
typedef std::vector<afio_exception_stack_entry> afio_exception_stack_t;
inline afio_exception_stack_t *&afio_exception_stack()
{
  static BOOST_AFIO_THREAD_LOCAL afio_exception_stack_t *s;
  return s;
}
BOOST_AFIO_V2_NAMESPACE_END
#endif

#include "ErrorHandling.ipp"

#ifdef WIN32
#ifndef IOV_MAX
#define IOV_MAX 1024
#endif
BOOST_AFIO_V2_NAMESPACE_BEGIN
struct iovec {
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
};
typedef ptrdiff_t ssize_t;
static spinlock<bool> preadwritelock;
inline ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    off_t at=offset;
    ssize_t transferred;
    lock_guard<decltype(preadwritelock)> lockh(preadwritelock);
    if(-1==_lseeki64(fd, offset, SEEK_SET)) return -1;
    for(; iovcnt; iov++, iovcnt--, at+=(off_t) transferred)
        if(-1==(transferred=_read(fd, iov->iov_base, (unsigned) iov->iov_len))) return -1;
    return (ssize_t)(at-offset);
}
inline ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    off_t at=offset;
    ssize_t transferred;
    lock_guard<decltype(preadwritelock)> lockh(preadwritelock);
    if(-1==_lseeki64(fd, offset, SEEK_SET)) return -1;
    for(; iovcnt; iov++, iovcnt--, at+=(off_t) transferred)
        if(-1==(transferred=_write(fd, iov->iov_base, (unsigned) iov->iov_len))) return -1;
    return (ssize_t)(at-offset);
}
inline ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    off_t at=0;
    ssize_t transferred;
    lock_guard<decltype(preadwritelock)> lockh(preadwritelock);
    for(; iovcnt; iov++, iovcnt--, at+=(off_t) transferred)
        if(-1==(transferred=_write(fd, iov->iov_base, (unsigned) iov->iov_len))) return -1;
    return (ssize_t)(at);
}
BOOST_AFIO_V2_NAMESPACE_END

#elif !defined(IOV_MAX) || defined(__APPLE__)  // Android lacks preadv() and pwritev()
BOOST_AFIO_V2_NAMESPACE_BEGIN
#ifndef IOV_MAX
#define IOV_MAX 1024
struct iovec {
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
};
typedef ptrdiff_t ssize_t;
#endif
inline ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    off_t at=offset;
    ssize_t transferred;
    for(; iovcnt; iov++, iovcnt--, at+=(off_t) transferred)
        if(-1==(transferred=pread(fd, iov->iov_base, (unsigned) iov->iov_len, at))) return -1;
    return (ssize_t)(at-offset);
}
inline ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    off_t at=offset;
    ssize_t transferred;
    for(; iovcnt; iov++, iovcnt--, at+=(off_t) transferred)
        if(-1==(transferred=pwrite(fd, iov->iov_base, (unsigned) iov->iov_len, at))) return -1;
    return (ssize_t)(at-offset);
}
BOOST_AFIO_V2_NAMESPACE_END

#endif


BOOST_AFIO_V2_NAMESPACE_BEGIN

std::shared_ptr<std_thread_pool> process_threadpool()
{
    // This is basically how many file i/o operations can occur at once
    // Obviously the kernel also has a limit
    static std::weak_ptr<std_thread_pool> shared;
    static spinlock<bool> lock;
    std::shared_ptr<std_thread_pool> ret(shared.lock());
    if(!ret)
    {
        lock_guard<decltype(lock)> lockh(lock);
        ret=shared.lock();
        if(!ret)
        {
            shared=ret=std::make_shared<std_thread_pool>(BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH);
        }
    }
    return ret;
}

#ifndef BOOST_AFIO_COMPILING_FOR_GCOV
// Experimental file region locking
namespace detail {
  struct process_lockfile_registry;
  struct actual_lock_file;
  template<class T> struct lock_file;
  typedef spinlock<bool> process_lockfile_registry_lock_t;
  static process_lockfile_registry_lock_t process_lockfile_registry_lock;
  static std::unique_ptr<process_lockfile_registry> process_lockfile_registry_ptr;

  struct process_lockfile_registry
  {
    std::unordered_map<handle *, lock_file<actual_lock_file> *> handle_to_lockfile;
    std::unordered_map<path, std::weak_ptr<actual_lock_file>, path_hash> path_to_lockfile;
    template<class T> static std::unique_ptr<T> open(handle *h)
    {
      lock_guard<process_lockfile_registry_lock_t> g(process_lockfile_registry_lock);
      if(!process_lockfile_registry_ptr)
        process_lockfile_registry_ptr=make_unique<process_lockfile_registry>();
      auto p=detail::make_unique<T>(h);
      process_lockfile_registry_ptr->handle_to_lockfile.insert(std::make_pair(h, (lock_file<actual_lock_file> *) p.get()));
      return p;
    }
  };
  struct actual_lock_file : std::enable_shared_from_this<actual_lock_file>
  {
    // TODO FIXME This needs to cope with sudden file relocations
    BOOST_AFIO_V2_NAMESPACE::path path, lockfilepath;
  protected:
    actual_lock_file(BOOST_AFIO_V2_NAMESPACE::path p) : path(p), lockfilepath(p)
    {
      lockfilepath+=".afiolockfile";
    }
  public:
    ~actual_lock_file()
    {
      lock_guard<process_lockfile_registry_lock_t> g(process_lockfile_registry_lock);
      process_lockfile_registry_ptr->path_to_lockfile.erase(path);
    }
    virtual dispatcher::completion_returntype lock(size_t id, future<> op, lock_req req, void *)=0;
  };
  struct posix_actual_lock_file : public actual_lock_file
  {
    int h;
#ifndef WIN32
    std::vector<struct flock> local_locks;
#endif
    std::vector<lock_req> locks;
    posix_actual_lock_file(BOOST_AFIO_V2_NAMESPACE::path p) : actual_lock_file(std::move(p)), h(0)
    {
#ifndef WIN32
      bool done=false;
      do
      {
        struct stat before, after;
        BOOST_AFIO_ERRHOS(lstat(path.c_str(), &before));
        BOOST_AFIO_ERRHOS(h=open(lockfilepath.c_str(), O_CREAT|O_RDWR, before.st_mode));
        // Place a read lock on the final byte as a way of detecting when this lock file no longer in use
        struct flock l;
        l.l_type=F_RDLCK;
        l.l_whence=SEEK_SET;
        l.l_start=std::numeric_limits<decltype(l.l_start)>::max()-1;
        l.l_len=1;
        int retcode;
        while(-1==(retcode=fcntl(h, F_SETLKW, &l)) && EINTR==errno);
        // Between the time of opening the lock file and indicating we are using it did someone else delete it
        // or even delete and recreate it?
        if(-1!=fstat(h, &before) && -1!=lstat(lockfilepath.c_str(), &after) && before.st_ino==after.st_ino)
          done=true;
        else
          ::close(h);
      } while(!done);
#endif
    }
    ~posix_actual_lock_file()
    {
#ifndef WIN32
      // Place a write lock on the final byte as a way of detecting when this lock file no longer in use
      struct flock l;
      l.l_type=F_WRLCK;
      l.l_whence=SEEK_SET;
      l.l_start=std::numeric_limits<decltype(l.l_start)>::max()-1;
      l.l_len=1;
      int retcode;
      while(-1==(retcode=fcntl(h, F_SETLK, &l)) && EINTR==errno);
      if(-1!=retcode)
        BOOST_AFIO_ERRHOS(unlink(lockfilepath.c_str()));
      BOOST_AFIO_ERRHOS(::close(h));
      lock_guard<process_lockfile_registry_lock_t> g(process_lockfile_registry_lock);
      process_lockfile_registry_ptr->path_to_lockfile.erase(path);
#endif
    }
    virtual dispatcher::completion_returntype lock(size_t id, future<> op, lock_req req, void *) override final
    {
#ifndef WIN32
      struct flock l;
      switch(req.type)
      {
        case lock_req::Type::read_lock:
          l.l_type=F_RDLCK;
          break;
        case lock_req::Type::write_lock:
          l.l_type=F_WRLCK;
          break;
        case lock_req::Type::unlock:
          l.l_type=F_UNLCK;
          break;
        default:
          BOOST_AFIO_THROW(std::invalid_argument("Lock type cannot be unknown"));
      }
      l.l_whence=SEEK_SET;
      // We use the last byte for deleting the lock file on last use, so clamp to second last byte
      if(req.offset==(off_t)(std::numeric_limits<decltype(l.l_start)>::max()-1))
        BOOST_AFIO_THROW(std::invalid_argument("offset cannot be (1<<62)-1"));
      l.l_start=(::off_t) req.offset;
      ::off_t end=(::off_t) std::min(req.offset+req.length, (off_t)(std::numeric_limits<decltype(l.l_len)>::max()-1));
      l.l_len=end-l.l_start;
      // TODO FIXME: Run through local_locks with some async algorithm before dropping onto fcntl().
      int retcode;
      while(-1==(retcode=fcntl(h, F_SETLKW, &l)) && EINTR==errno)
        /*empty*/;
      BOOST_AFIO_ERRHOS(retcode);
#endif
      return std::make_pair(true, op.get_handle());
    }
  };
  template<class T> struct lock_file
  {
    friend struct process_lockfile_registry;
    handle *h;
    std::vector<lock_req> locks;
    std::shared_ptr<actual_lock_file> actual;
    lock_file(handle *_h=nullptr) : h(_h)
    {
      if(h)
      {
        auto mypath=h->path(true);
        auto it=process_lockfile_registry_ptr->path_to_lockfile.find(mypath);
        if(process_lockfile_registry_ptr->path_to_lockfile.end()!=it)
          actual=it->second.lock();
        if(!actual)
        {
          if(process_lockfile_registry_ptr->path_to_lockfile.end()!=it)
            process_lockfile_registry_ptr->path_to_lockfile.erase(it);
          actual=std::make_shared<T>(mypath);
          process_lockfile_registry_ptr->path_to_lockfile.insert(std::make_pair(mypath, actual));
        }
      }
    }
    dispatcher::completion_returntype lock(size_t id, future<> op, lock_req req)
    {
      return actual->lock(id, std::move(op), std::move(req), this);
    }
    ~lock_file()
    {
      lock_guard<process_lockfile_registry_lock_t> g(process_lockfile_registry_lock);
      process_lockfile_registry_ptr->handle_to_lockfile.erase(h);
    }
  };
  typedef lock_file<posix_actual_lock_file> posix_lock_file;
}
#endif


namespace detail {
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void print_fatal_exception_message_to_stderr(const char *msg)
    {
        BOOST_AFIO_LOG_FATAL_EXIT("FATAL EXCEPTION: " << msg << std::endl);
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
        stack_type stack;
        collect_stack(stack);
        std::stringstream stacktxt;
        print_stack(stacktxt, stack);
        BOOST_AFIO_LOG_FATAL_EXIT(stacktxt.str() << std::endl);
#endif
    }

    // Returns a handle to the directory to be used as the base for the relative path fragment
    template<class Impl, class Handle> handle_ptr decode_relative_path(path_req &req, bool force_absolute)
    {
      if(!req.is_relative)
        return handle_ptr();
      if(!req.precondition.valid())
        return handle_ptr();
      Handle *p=static_cast<Handle *>(&*req.precondition);
retry:
      path_req parentpath(p->path(true));
      if(!force_absolute)
      {
#if BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS || defined(WIN32)
        if(p->opened_as_dir())  // If base handle is a directory ...
        {
          if(req.path.empty())  // If path fragment means self, return containing directory and set path fragment to leafname
          {
            req.path=parentpath.path.filename();
            parentpath.path=parentpath.path.parent_path();
          }
          else if(p->available_to_directory_cache())
            return req.precondition.get_handle();  // always valid and a directory
          try
          {
            return p->parent()->p->get_handle_to_dir(static_cast<Impl *>(p->parent()), 0, parentpath, &Impl::dofile);
          }
          catch(...)
          {
            // Path of p is stale, so loop
            goto retry;
          }
        }
        else
        {
          // If path fragment isn't empty or doesn't begin with ../, fail
          if(req.path.empty())
            req.path=parentpath.path.filename();
          else
          {
            if(req.path.native()[0]!='.' || req.path.native()[1]!='.')
              BOOST_AFIO_THROW(std::invalid_argument("Cannot use a path fragment inside a file, try prepending a ../"));
            // Trim the preceding ..
            auto &nativepath=const_cast<BOOST_AFIO_V2_NAMESPACE::path::string_type &>(req.path.native());
            if(nativepath.size()>=3)
              nativepath=nativepath.substr(3);
            else
              nativepath.clear();
          }
          handle_ptr dirh=p->container();  // quite likely empty
          if(dirh)
            return dirh;
          else
            return p->parent()->int_get_handle_to_containing_dir(static_cast<Impl *>(p->parent()), 0, parentpath, &Impl::dofile);
        }
    #endif
      }
      req.path=parentpath.path/req.path;
      req.is_relative=false;
      return handle_ptr();
    }

    
    struct async_io_handle_posix : public handle
    {
        int fd;  // -999 is closed handle
        bool has_been_added, DeleteOnClose, SyncOnClose, has_ever_been_fsynced;
        dev_t st_dev;  // Stored on first open. Used to detect races later.
        ino_t st_ino;
        typedef spinlock<bool> pathlock_t;
        mutable pathlock_t pathlock; BOOST_AFIO_V2_NAMESPACE::path _path;
#ifndef BOOST_AFIO_COMPILING_FOR_GCOV
        std::unique_ptr<posix_lock_file> lockfile;
#endif

        async_io_handle_posix(dispatcher *_parent, const BOOST_AFIO_V2_NAMESPACE::path &path, file_flags flags, bool _DeleteOnClose, bool _SyncOnClose, int _fd) : handle(_parent, flags), fd(_fd), has_been_added(false), DeleteOnClose(_DeleteOnClose), SyncOnClose(_SyncOnClose), has_ever_been_fsynced(false), st_dev(0), st_ino(0), _path(path)
        {
            if(fd!=-999)
            {
                BOOST_AFIO_ERRHOSFN(fd, [&path]{return path;});
            }
            if(!(flags & file_flags::no_race_protection))
            {
              BOOST_AFIO_POSIX_STAT_STRUCT s={0};
              if(-999==fd)
                BOOST_AFIO_POSIX_LSTATAT(at_fdcwd, path.c_str(), &s);  // Potential race between symlink creation and lstat here
              else
                BOOST_AFIO_POSIX_FSTAT(fd, &s);
              st_dev=s.st_dev;
              st_ino=s.st_ino;
            }
#ifndef BOOST_AFIO_COMPILING_FOR_GCOV
            if(!!(flags & file_flags::os_lockable))
                lockfile=process_lockfile_registry::open<posix_lock_file>(this);
#endif
        }
        //! Returns the containing directory if my inode appears in it
        inline handle_ptr int_verifymyinode();
        void update_path(BOOST_AFIO_V2_NAMESPACE::path oldpath, BOOST_AFIO_V2_NAMESPACE::path newpath)
        {
          // For now, always zap the container
          dirh.reset();
          if(available_to_directory_cache())
            parent()->int_directory_cached_handle_path_changed(oldpath, newpath, shared_from_this());
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void close() override final
        {
            BOOST_AFIO_DEBUG_PRINT("D %p\n", this);
            int _fd=fd;
            if(fd>=0)
            {
                if(SyncOnClose && write_count_since_fsync())
                    BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_FSYNC(fd), [this]{return path();});
                if(DeleteOnClose)
                    async_io_handle_posix::unlink();
                BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_CLOSE(fd), [this]{return path();});
                fd=-999;
            }
            // Deregister AFTER close of file handle
            if(has_been_added)
            {
                parent()->int_del_io_handle((void *) (size_t) _fd);
                has_been_added=false;
            }
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC open_states is_open() const override final
        {
          if(-999==fd)
            return open_states::closed;
          return available_to_directory_cache() ? open_states::opendir : open_states::open;
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void *native_handle() const override final { return (void *)(uintptr_t)fd; }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC BOOST_AFIO_V2_NAMESPACE::path path(bool refresh=false) override final
        {
          if(refresh)
          {
#if !defined(WIN32)
            if(-999==fd)
            {
#if 0
              auto mycontainer=container();
              if(mycontainer)
                mycontainer->path(true);
#endif
            }
            else
            {
              BOOST_AFIO_V2_NAMESPACE::path newpath;
#if defined(__linux__)
              // Linux keeps a symlink at /proc/self/fd/n
              char in[PATH_MAX+1], out[32769];
              sprintf(in, "/proc/self/fd/%d", fd);
              ssize_t len;
              if((len = readlink(in, out, sizeof(out)-1)) == -1)
                  BOOST_AFIO_ERRGOS(-1);
              path::string_type ret(out, len);
              // Linux prepends or appends a " (deleted)" when a fd is nameless
              // TODO: Should I stat the target to be really sure?
              if(ret.size()<10 || (ret.compare(0, 10, " (deleted)") && ret.compare(ret.size()-10, 10, " (deleted)")))
                newpath=ret;
#elif defined(__APPLE__)
              // Apple returns the previous path when deleted, so lstat to be sure
              struct stat s;
              BOOST_AFIO_ERRHOS(fstat(fd, &s));
              if(s.st_nlink)
              {
                // Apple also annoyingly returns some random hard link when nlink>1, so disable fetching new when that is the case
                if(s.st_nlink>1)
                {
                  {
                    lock_guard<pathlock_t> g(pathlock);
                    newpath=_path;
                  }
                  struct stat ls;
                  bool exists=(-1!=::lstat(newpath.c_str(), &ls)) && s.st_dev==ls.st_dev && s.st_ino==ls.st_ino;
                  if(!exists)
                    newpath.clear();
                }
                else
                {
                  char buffer[PATH_MAX+1];
                  BOOST_AFIO_ERRHOS(fcntl(fd, F_GETPATH, buffer));
                  newpath=path::string_type(buffer);
                }
              }
#elif defined(__FreeBSD__)
              // Unfortunately this call is broken on FreeBSD 10 where it is currently returning
              // null paths most of the time for regular files. Directories work perfectly. I've
              // logged a bug with test case at https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=197695.
              if(!opened_as_dir())
              {
                struct stat s;
                BOOST_AFIO_ERRHOS(fstat(fd, &s));
                if(s.st_nlink)
                {
                  lock_guard<pathlock_t> g(pathlock);
                  newpath=_path;
                }
#if 0  // We don't attempt refresh semantics as none are promised on FreeBSD for files
                //if(s.st_nlink>1)
                {
                  struct stat ls;
                  bool exists=(-1!=::lstat(newpath.c_str(), &ls)) && s.st_dev==ls.st_dev && s.st_ino==ls.st_ino;
                  if(!exists)
                    newpath.clear();
                }
#endif
              }
              else
              {
                size_t len;
                int mib[4]={CTL_KERN, KERN_PROC, KERN_PROC_FILEDESC, getpid()};
                BOOST_AFIO_ERRHOS(sysctl(mib, 4, NULL, &len, NULL, 0));
                std::vector<char> buffer(len*2);
                BOOST_AFIO_ERRHOS(sysctl(mib, 4, buffer.data(), &len, NULL, 0));
#if 0 //ndef NDEBUG
                for(char *p=buffer.data(); p<buffer.data()+len;)
                {
                  struct kinfo_file *kif=(struct kinfo_file *) p;
                  std::cout << kif->kf_type << " " << kif->kf_fd << " " << kif->kf_path << std::endl;
                  p+=kif->kf_structsize;
                }
#endif
                for(char *p=buffer.data(); p<buffer.data()+len;)
                {
                  struct kinfo_file *kif=(struct kinfo_file *) p;
                  if(kif->kf_fd==fd)
                  {
                    //struct stat s;
                    //BOOST_AFIO_ERRHOS(fstat(fd, &s));
                    //if(s.st_nlink && kif->kf_path[0])
                      newpath=path::string_type(kif->kf_path);
                    //else if(!s.st_nlink)
                    //  newpath.clear();
                    break;
                  }
                  p+=kif->kf_structsize;
                }
              }
#else
#error Unknown system
#endif
              bool changed=false;
              BOOST_AFIO_V2_NAMESPACE::path oldpath;
              {
                lock_guard<pathlock_t> g(pathlock);
                if((changed=(_path!=newpath)))
                {
                  oldpath=std::move(_path);
                  _path=newpath;
                }
              }
              if(changed)
                update_path(oldpath, newpath);
            }
#endif
          }
          lock_guard<pathlock_t> g(pathlock);
          return _path;
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC BOOST_AFIO_V2_NAMESPACE::path path() const override final
        {
          lock_guard<pathlock_t> g(pathlock);
          return _path;
        }

        // You can't use shared_from_this() nor virtual functions in a constructor so ...
        void do_add_io_handle_to_parent()
        {
            if(fd!=-999)
            {
                // Canonicalise my path
                auto newpath=path(true);
                parent()->int_add_io_handle((void *) (size_t) fd, shared_from_this());
                has_been_added=true;
                // If I'm the right sort of directory, register myself with the dircache. path(true) on some platforms may have
                // already done this, but it's not much harm to repeat myself.
                if(available_to_directory_cache())
                  parent()->int_directory_cached_handle_path_changed(BOOST_AFIO_V2_NAMESPACE::path(), newpath, shared_from_this());                  
            }
            if(!!(flags() & file_flags::hold_parent_open) && !(flags() & file_flags::int_hold_parent_open_nested))
              dirh=int_verifymyinode();
        }
        ~async_io_handle_posix()
        {
            async_io_handle_posix::close();
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC directory_entry direntry(metadata_flags wanted) override final
        {
            stat_t stat(nullptr);
            BOOST_AFIO_POSIX_STAT_STRUCT s={0};
            if(-999==fd)  // Probably a symlink
            {
#if BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS
              if(!(flags() & file_flags::no_race_protection))
              {
                auto dirh(int_verifymyinode());
                if(!dirh)
                {
                  errno=ENOENT;
                  BOOST_AFIO_ERRHOSFN(-1, [this]{return path();});
                }
                BOOST_AFIO_V2_NAMESPACE::path leaf(path().filename());
                BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTATAT((int)(size_t)dirh->native_handle(), leaf.c_str(), &s), [this]{return path();});
              }
              else
#endif
              {
                BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTATAT(at_fdcwd, path().c_str(), &s), [this]{return path();});
              }
              fill_stat_t(stat, s, wanted);
              return directory_entry(path().filename().native(), stat, wanted);
            }
            else
            {
              BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_FSTAT(fd, &s), [this]{return path();});
              fill_stat_t(stat, s, wanted);
              return directory_entry(const_cast<async_io_handle_posix *>(this)->path(true).filename().native(), stat, wanted);
            }
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC BOOST_AFIO_V2_NAMESPACE::path target() override final
        {
#ifdef WIN32
            return path();
#else
            if(!opened_as_symlink())
                return path();
            char buffer[PATH_MAX+1];
            ssize_t len;
#if BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS
            // Some platforms allow the opening of links as a file descriptor
            if(-999!=fd)
            {
              if((len = readlinkat(fd, "", buffer, sizeof(buffer)-1)) == -1)
                  BOOST_AFIO_ERRGOS(-1);
              return path::string_type(buffer, len);
            }
            if(!(flags() & file_flags::no_race_protection))
            {
              auto dirh(int_verifymyinode());
              if(!dirh)
              {
                errno=ENOENT;
                BOOST_AFIO_ERRHOSFN(-1, [this]{return path();});
              }
              BOOST_AFIO_V2_NAMESPACE::path leaf(path().filename());
              if((len = readlinkat((int)(size_t)dirh->native_handle(), leaf.c_str(), buffer, sizeof(buffer)-1)) == -1)
                  BOOST_AFIO_ERRGOS(-1);
            }
            else
#endif
            {
              if((len = readlink(path().c_str(), buffer, sizeof(buffer)-1)) == -1)
                  BOOST_AFIO_ERRGOS(-1);
            }
            return path::string_type(buffer, len);
#endif
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::unique_ptr<mapped_file> map_file(size_t length, off_t offset, bool read_only) override final
        {
#ifndef WIN32
            BOOST_AFIO_POSIX_STAT_STRUCT s={0};
            if(-1!=fstat(fd, &s))
            {
              if(length>(size_t) s.st_size)
                length=(size_t) s.st_size;
              if(offset+length>(size_t) s.st_size)
                length=(size_t) s.st_size-offset;
              void *mapaddr=nullptr;
              int prot=PROT_READ;
              if(!read_only && !!(flags() & file_flags::write))
                prot|=PROT_WRITE;
              if((mapaddr=BOOST_AFIO_POSIX_MMAP(nullptr, length, prot, MAP_SHARED, fd, offset)))
              {
                  if(!!(flags() & file_flags::will_be_sequentially_accessed))
                    madvise(mapaddr, length, MADV_SEQUENTIAL);
                  else if(!!(flags() & file_flags::will_be_randomly_accessed))
                    madvise(mapaddr, length, MADV_RANDOM);
                  return detail::make_unique<mapped_file>(shared_from_this(), mapaddr, length, offset);
              }
            }
#endif
            return nullptr;
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void link(const path_req &_req) override final
        {
          path_req req(_req);
#ifndef WIN32
          if(-999!=fd)
          {
            auto newdirh=decode_relative_path<async_file_io_dispatcher_compat, async_io_handle_posix>(req);
            if(!(flags() & file_flags::no_race_protection))
            {
#if BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS
              // Some platforms may allow direct hard linking of file handles, in which case we are done
#ifdef AT_EMPTY_PATH
              if(fd>=0 && -1!=BOOST_AFIO_POSIX_LINKAT(fd, "", newdirh ? (int)(size_t)newdirh->native_handle() : at_fdcwd, req.path.c_str(), AT_EMPTY_PATH))
                return;
#endif
              auto dirh(int_verifymyinode());
              if(!dirh)
              {
                errno=ENOENT;
                BOOST_AFIO_ERRHOSFN(-1, [this]{return path();});
              }
              BOOST_AFIO_V2_NAMESPACE::path leaf(path().filename());
              BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LINKAT((int)(size_t)dirh->native_handle(), leaf.c_str(), newdirh ? (int)(size_t)newdirh->native_handle() : at_fdcwd, req.path.c_str(), 0), [this]{return path();});
#else
              // At least check if what I am about to delete matches myself
              BOOST_AFIO_POSIX_STAT_STRUCT s={0};
              BOOST_AFIO_V2_NAMESPACE::path p(path(true));
              BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTATAT(at_fdcwd, p.c_str(), &s), [this]{return path();});
              if(s.st_dev!=st_dev || s.st_ino!=st_ino)
              {
                errno=ENOENT;
                BOOST_AFIO_ERRHOSFN(-1, [this]{return path();});
              }
              // Could of course still race between the lstat() and the unlink() so still unsafe ...
              BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LINKAT(at_fdcwd, p.c_str(), at_fdcwd, req.path.c_str(), 0), [this]{return path();});
#endif
              return;
            }
          }
          decode_relative_path<async_file_io_dispatcher_compat, async_io_handle_posix>(req, true);
          BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LINKAT(at_fdcwd, path(true).c_str(), at_fdcwd, req.path.c_str(), 0), [this]{return path();});
#endif
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlink() override final
        {
          if(-999!=fd)
          {
            if(!(flags() & file_flags::no_race_protection))
            {
#if BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS
              // Some platforms may allow direct deletion of file handles, in which case we are done
#ifdef AT_EMPTY_PATH
              if(fd>=0 && -1!=BOOST_AFIO_POSIX_UNLINKAT(fd, "", opened_as_dir() ? AT_EMPTY_PATH|AT_REMOVEDIR : AT_EMPTY_PATH))
                goto update_path;
#endif
              auto dirh(int_verifymyinode());
              if(!dirh)
              {
                errno=ENOENT;
                BOOST_AFIO_ERRHOSFN(-1, [this]{return path();});
              }
              BOOST_AFIO_V2_NAMESPACE::path leaf(path().filename());
              BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_UNLINKAT((int)(size_t)dirh->native_handle(), leaf.c_str(), opened_as_dir() ? AT_REMOVEDIR : 0), [this]{return path();});
#else
              // At least check if what I am about to delete matches myself
              BOOST_AFIO_POSIX_STAT_STRUCT s={0};
              BOOST_AFIO_V2_NAMESPACE::path p(path(true));
              BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTATAT(at_fdcwd, p.c_str(), &s), [this]{return path();});
              if(s.st_dev!=st_dev || s.st_ino!=st_ino)
              {
                errno=ENOENT;
                BOOST_AFIO_ERRHOSFN(-1, [this]{return path();});
              }
              // Could of course still race between the lstat() and the unlink() so still unsafe ...
              BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_UNLINKAT(at_fdcwd, p.c_str(), opened_as_dir() ? AT_REMOVEDIR : 0), [this]{return path();});
#endif
              goto update_path;
            }
          }
          BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_UNLINKAT(at_fdcwd, path(true).c_str(), opened_as_dir() ? AT_REMOVEDIR : 0), [this]{return path();});
update_path:
          BOOST_AFIO_V2_NAMESPACE::path oldpath, newpath;
          {
            lock_guard<pathlock_t> g(pathlock);
            oldpath=std::move(_path);
          }
          update_path(oldpath, newpath);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void atomic_relink(const path_req &_req) override final
        {
          path_req req(_req);
          if(-999!=fd)
          {
            auto newdirh=decode_relative_path<async_file_io_dispatcher_compat, async_io_handle_posix>(req);
            if(!(flags() & file_flags::no_race_protection))
            {
#if BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS
              auto dirh(int_verifymyinode());
              if(!dirh)
              {
                errno=ENOENT;
                BOOST_AFIO_ERRHOSFN(-1, [this]{return path();});
              }
              BOOST_AFIO_V2_NAMESPACE::path leaf(path().filename());
              BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_RENAMEAT((int)(size_t)dirh->native_handle(), leaf.c_str(), newdirh ? (int)(size_t)newdirh->native_handle() : at_fdcwd, req.path.c_str()), [this]{return path();});
#else
              // At least check if what I am about to delete matches myself
              BOOST_AFIO_POSIX_STAT_STRUCT s={0};
              BOOST_AFIO_V2_NAMESPACE::path p(path(true));
              BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTATAT(at_fdcwd, p.c_str(), &s), [this]{return path();});
              if(s.st_dev!=st_dev || s.st_ino!=st_ino)
              {
                errno=ENOENT;
                BOOST_AFIO_ERRHOSFN(-1, [this]{return path();});
              }
              // Could of course still race between the lstat() and the unlink() so still unsafe ...
              BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_RENAMEAT(at_fdcwd, p.c_str(), at_fdcwd, req.path.c_str()), [this]{return path();});
#endif
              if(newdirh)
                req.path=newdirh->path(true)/req.path;
              goto update_path;
            }
          }
          decode_relative_path<async_file_io_dispatcher_compat, async_io_handle_posix>(req, true);
          BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_RENAMEAT(at_fdcwd, path(true).c_str(), at_fdcwd, req.path.c_str()), [this]{return path();});
update_path:
          BOOST_AFIO_V2_NAMESPACE::path oldpath;
          {
            lock_guard<pathlock_t> g(pathlock);
            oldpath=std::move(_path);
            _path=req.path;
          }
          update_path(oldpath, req.path);
        }
    };

    struct async_file_io_dispatcher_op
    {
        OpType optype;
        async_op_flags flags;
        enqueued_task<handle_ptr()> enqueuement;
        typedef std::pair<size_t, std::shared_ptr<detail::async_file_io_dispatcher_op>> completion_t;
        std::vector<completion_t> completions;
        const shared_future<handle_ptr> &h() const { return enqueuement.get_future(); }
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
        stack_type stack;
        void fillStack() { collect_stack(stack); }
#else
        void fillStack() { }
#endif
        async_file_io_dispatcher_op(OpType _optype, async_op_flags _flags)
            : optype(_optype), flags(_flags)
        {
            // Stop the stl_future from being auto-set on task return
            enqueuement.disable_auto_set_future();
            //completions.reserve(4); // stop needless storage doubling for small numbers
            fillStack();
        }
        async_file_io_dispatcher_op(async_file_io_dispatcher_op &&o) noexcept : optype(o.optype), flags(std::move(o.flags)),
            enqueuement(std::move(o.enqueuement)), completions(std::move(o.completions))
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
            , stack(std::move(o.stack))
#endif
            {
            }
    private:
        async_file_io_dispatcher_op(const async_file_io_dispatcher_op &o) = delete;
    };
    struct dispatcher_p
    {
        std::shared_ptr<thread_source> pool;
        unit_testing_flags testing_flags;
        file_flags flagsforce, flagsmask;
        std::vector<std::pair<detail::OpType, std::function<dispatcher::filter_t>>> filters;
        std::vector<std::pair<detail::OpType, std::function<dispatcher::filter_readwrite_t>>> filters_buffers;

#ifdef BOOST_AFIO_USE_CONCURRENT_UNORDERED_MAP
        typedef null_lock fdslock_t;
        typedef null_lock opslock_t;
#else
        typedef spinlock<size_t> fdslock_t;
        typedef spinlock<size_t,
            spins_to_loop<100>::policy,
            spins_to_yield<500>::policy,
            spins_to_sleep::policy
        > opslock_t;
#endif
        typedef recursive_mutex dircachelock_t;
        fdslock_t fdslock; engine_unordered_map_t<void *, std::weak_ptr<handle>> fds;
        opslock_t opslock; atomic<size_t> monotoniccount; engine_unordered_map_t<size_t, std::shared_ptr<async_file_io_dispatcher_op>> ops;
        dircachelock_t dircachelock; std::unordered_map<path, std::weak_ptr<handle>, path_hash> dirhcache;

        dispatcher_p(std::shared_ptr<thread_source> _pool, file_flags _flagsforce, file_flags _flagsmask) : pool(_pool),
            testing_flags(unit_testing_flags::none), flagsforce(_flagsforce), flagsmask(_flagsmask), monotoniccount(0)
        {
#ifdef BOOST_AFIO_USE_CONCURRENT_UNORDERED_MAP
            // concurrent_unordered_map doesn't lock, so we actually don't need many buckets for max performance
            fds.min_bucket_capacity(2);
            fds.reserve(499);
            ops.min_bucket_capacity(2);
            ops.reserve(499);
#else
            // unordered_map needs to release the lock as quickly as possible
            ops.reserve(10000);
#endif
        }
        ~dispatcher_p()
        {
        }

        // Returns a handle to a directory from the cache, or creates a new directory handle.
        template<class F> handle_ptr get_handle_to_dir(F *parent, size_t id, path_req req, typename dispatcher::completion_returntype(F::*dofile)(size_t, future<>, path_req))
        {
            assert(!req.is_relative);
            req.flags=file_flags::int_hold_parent_open_nested|file_flags::int_opening_dir|file_flags::read;
            // First refresh all paths in the cache, eliminating any stale ptrs
            // One problem is that h->path(true) will update itself in dirhcache if it has changed, thus invalidating all the iterators
            // so we first copy dirhcache into torefresh
            std::vector<std::pair<path, std::weak_ptr<handle>>> torefresh;
            {
              lock_guard<dircachelock_t> dircachelockh(dircachelock);
              torefresh.reserve(dirhcache.size());
              for(auto it=dirhcache.begin(); it!=dirhcache.end();)
              {
                if(it->second.expired())
                {
#ifndef NDEBUG
                  std::cout << "afio: directory cached handle pruned stale " << it->first << std::endl;
#endif
                  it=dirhcache.erase(it);
                  continue;
                }
                torefresh.push_back(*it);
                ++it;
              }
            }
            handle_ptr dirh;
            for(auto &i : torefresh)
            {
              if((dirh=i.second.lock()))
                dirh->path(true);
            }
            dirh.reset();
            lock_guard<dircachelock_t> dircachelockh(dircachelock);
            do
            {
                std::unordered_map<path, std::weak_ptr<handle>, path_hash>::iterator it=dirhcache.find(req.path);
                if(dirhcache.end()!=it)
                  dirh=it->second.lock();
                if(!dirh)
                {
                    dispatcher::completion_returntype result=(parent->*dofile)(id, future<>(), req); // should recurse in to insert itself
                    dirh=std::move(result.second);
#ifndef NDEBUG
                    if(dirh)
                      std::cout << "afio: directory cached handle created for " << req.path << " (" << dirh.get() << ")" << std::endl;
#endif
                    if(dirh)
                    {
                      // May have renamed itself, if so add this entry point to the cache too
                      if(dirh->path()!=req.path)
                        dirhcache.insert(std::make_pair(req.path, dirh));
                      return dirh;
                    }
                    else
                        abort();
                }
            } while(!dirh);
            return dirh;
        }
    };
    class async_file_io_dispatcher_compat;
    class async_file_io_dispatcher_windows;
    class async_file_io_dispatcher_linux;
    class async_file_io_dispatcher_qnx;
    struct immediate_async_ops
    {
        typedef handle_ptr rettype;
        typedef rettype retfuncttype();
        size_t reservation;
        std::vector<enqueued_task<retfuncttype>> toexecute;

        immediate_async_ops(size_t reserve) : reservation(reserve) { }
        // Returns a promise which is fulfilled when this is destructed
        void enqueue(enqueued_task<retfuncttype> task)
        {
          if(toexecute.empty())
            toexecute.reserve(reservation);
          toexecute.push_back(task);
        }
        ~immediate_async_ops()
        {
            for(auto &i: toexecute)
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

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC metadata_flags directory_entry::metadata_supported() noexcept
{
    metadata_flags ret;
#ifdef WIN32
    ret=metadata_flags::None
        //| metadata_flags::dev
        | metadata_flags::ino        // FILE_INTERNAL_INFORMATION, enumerated
        | metadata_flags::type       // FILE_BASIC_INFORMATION, enumerated
        //| metadata_flags::perms
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
        | metadata_flags::sparse     // FILE_BASIC_INFORMATION, enumerated
        | metadata_flags::compressed // FILE_BASIC_INFORMATION, enumerated
        | metadata_flags::reparse_point // FILE_BASIC_INFORMATION, enumerated
        ;
#elif defined(__linux__)
    ret=metadata_flags::None
        | metadata_flags::dev
        | metadata_flags::ino
        | metadata_flags::type
        | metadata_flags::perms
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
        | metadata_flags::sparse
        //| metadata_flags::compressed
        //| metadata_flags::reparse_point
        ;
#else
    // Kinda assumes FreeBSD or OS X really ...
    ret=metadata_flags::None
        | metadata_flags::dev
        | metadata_flags::ino
        | metadata_flags::type
        | metadata_flags::perms
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
        | metadata_flags::flags
        | metadata_flags::gen
        | metadata_flags::birthtim
        | metadata_flags::sparse
        //| metadata_flags::compressed
        //| metadata_flags::reparse_point
        ;
#endif
    return ret;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC metadata_flags directory_entry::metadata_fastpath() noexcept
{
    metadata_flags ret;
#ifdef WIN32
    ret=metadata_flags::None
        //| metadata_flags::dev
        | metadata_flags::ino        // FILE_INTERNAL_INFORMATION, enumerated
        | metadata_flags::type       // FILE_BASIC_INFORMATION, enumerated
        //| metadata_flags::perms
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
        | metadata_flags::sparse     // FILE_BASIC_INFORMATION, enumerated
        | metadata_flags::compressed // FILE_BASIC_INFORMATION, enumerated
        | metadata_flags::reparse_point // FILE_BASIC_INFORMATION, enumerated
        ;
#elif defined(__linux__)
    ret=metadata_flags::None
        | metadata_flags::dev
        | metadata_flags::ino
        | metadata_flags::type
        | metadata_flags::perms
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
        | metadata_flags::sparse
        //| metadata_flags::compressed
        //| metadata_flags::reparse_point
        ;
#else
    // Kinda assumes FreeBSD or OS X really ...
    ret=metadata_flags::None
        | metadata_flags::dev
        | metadata_flags::ino
        | metadata_flags::type
        | metadata_flags::perms
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
        | metadata_flags::flags
        | metadata_flags::gen
        | metadata_flags::birthtim
        | metadata_flags::sparse
        //| metadata_flags::compressed
        //| metadata_flags::reparse_point
        ;
#endif
    return ret;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t directory_entry::compatibility_maximum() noexcept
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

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC dispatcher::dispatcher(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask) : p(new detail::dispatcher_p(threadpool, flagsforce, flagsmask))
{
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void dispatcher::testing_flags(detail::unit_testing_flags flags)
{
  p->testing_flags=flags;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC dispatcher::~dispatcher()
{
#ifndef BOOST_AFIO_COMPILING_FOR_GCOV
    engine_unordered_map_t<const detail::async_file_io_dispatcher_op *, std::pair<size_t, future_status>> reallyoutstanding;
    for(;;)
    {
        std::vector<std::pair<const size_t, std::shared_ptr<detail::async_file_io_dispatcher_op>>> outstanding;
        {
            lock_guard<decltype(p->opslock)> g(p->opslock); // may have no effect under concurrent_unordered_map
            if(!p->ops.empty())
            {
                outstanding.reserve(p->ops.size());
                for(auto &op: p->ops)
                {
                    if(op.second->h().valid())
                    {
                        auto it=reallyoutstanding.find(op.second.get());
                        if(reallyoutstanding.end()!=it)
                        {
                            if(it->second.first>=5)
                            {
                                static const char *statuses[]={ "ready", "timeout", "deferred", "unknown" };
                                int status=static_cast<int>(it->second.second);
                                BOOST_AFIO_LOG_FATAL_EXIT("WARNING: ~async_file_dispatcher_base() detects stuck future<> in total of " << p->ops.size() << " extant ops\n"
                                    "   id=" << op.first << " type=" << detail::optypes[static_cast<size_t>(op.second->optype)] << " flags=0x" << std::hex << static_cast<size_t>(op.second->flags) << std::dec << " status=" << statuses[(status>=0 && status<=2) ? status : 3] << " failcount=" << it->second.first << " Completions:");
                                for(auto &c: op.second->completions)
                                {
                                    BOOST_AFIO_LOG_FATAL_EXIT(" id=" << c.first);
                                }
                                BOOST_AFIO_LOG_FATAL_EXIT(std::endl);
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
                                BOOST_AFIO_LOG_FATAL_EXIT("  Allocation backtrace:" << std::endl);
                                std::stringstream stacktxt;
                                print_stack(stacktxt, op.second->stack);
                                BOOST_AFIO_LOG_FATAL_EXIT(stacktxt.str() << std::endl);
#endif
                            }
                        }
                        outstanding.push_back(op);
                    }
                }
            }
        }
        if(outstanding.empty()) break;
        size_t mincount=(size_t)-1;
        for(auto &op: outstanding)
        {
            future_status status=op.second->h().wait_for(chrono::duration<int, ratio<1, 10>>(1));
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
            BOOST_AFIO_LOG_FATAL_EXIT("WARNING: ~async_file_dispatcher_base() sees no change in " << reallyoutstanding.size() << " stuck async_io_ops, so exiting destructor wait" << std::endl);
            break;
        }
    }
    for(size_t n=0; !p->fds.empty(); n++)
    {
        this_thread::sleep_for(chrono::milliseconds(10));
        if(n>300)
        {
            BOOST_AFIO_LOG_FATAL_EXIT("WARNING: ~async_file_dispatcher_base() sees no change in " << p->fds.size() << " stuck handle_ptr's, so exiting destructor wait" << std::endl);
            break;
        }
    }
#endif
    delete p;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void dispatcher::int_add_io_handle(void *key, handle_ptr h)
{
    {
        lock_guard<decltype(p->fdslock)> g(p->fdslock);
        p->fds.insert(std::make_pair(key, std::weak_ptr<handle>(h)));
    }
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void dispatcher::int_del_io_handle(void *key)
{
    {
        lock_guard<decltype(p->fdslock)> g(p->fdslock);
        p->fds.erase(key);
    }
}

// Returns a handle to a containing directory from the cache, or creates a new directory handle.
template<class F> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC handle_ptr dispatcher::int_get_handle_to_containing_dir(F *parent, size_t id, path_req req, typename dispatcher::completion_returntype(F::*dofile)(size_t, future<>, path_req))
{
    req.path=req.path.parent_path();
    return p->get_handle_to_dir(parent, id, req, dofile);
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::shared_ptr<thread_source> dispatcher::threadsource() const
{
    return p->pool;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC file_flags dispatcher::fileflags(file_flags flags) const
{
    file_flags ret=(flags&~p->flagsmask)|p->flagsforce;
    return ret;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t dispatcher::wait_queue_depth() const
{
    size_t ret=0;
    {
        lock_guard<decltype(p->opslock)> g(p->opslock);
        ret=p->ops.size();
    }
    return ret;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t dispatcher::fd_count() const
{
    size_t ret=0;
    {
        lock_guard<decltype(p->fdslock)> g(p->fdslock);
        ret=p->fds.size();
    }
    return ret;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void dispatcher::int_directory_cached_handle_path_changed(path oldpath, path newpath, handle_ptr h)
{
  lock_guard<decltype(p->dircachelock)> dircachelockh(p->dircachelock);
  if(!oldpath.empty())
  {
    auto it=p->dirhcache.find(oldpath);
    if(it!=p->dirhcache.end())
    {
      //assert(it->second.lock()==h);
      p->dirhcache.erase(it);
    }
  }
#ifndef NDEBUG
  std::cout << "afio: directory cached handle we relocate from " << oldpath << " to " << newpath << " (" << h.get() << ")" << std::endl;
#endif
  if(!newpath.empty())
    p->dirhcache.insert(std::make_pair(std::move(newpath), std::move(h)));
}


// Non op lock holding variant
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC future<> dispatcher::int_op_from_scheduled_id(size_t id) const
{
#ifdef BOOST_AFIO_USE_CONCURRENT_UNORDERED_MAP
    future<> ret;
    typedef decltype(p->ops)::value_type op_value_type;
    if(!p->ops.visit(id, [this, id, &ret](const op_value_type &op){
        ret=future<>(const_cast<dispatcher *>(this), id, op.second->h());
    }))
    {
        BOOST_AFIO_THROW(std::runtime_error("Failed to find this operation in list of currently executing operations"));
    }
    return ret;
#else
    engine_unordered_map_t<size_t, std::shared_ptr<detail::async_file_io_dispatcher_op>>::iterator it=p->ops.find(id);
    if(p->ops.end()==it)
    {
        BOOST_AFIO_THROW(std::runtime_error("Failed to find this operation in list of currently executing operations"));
    }
    return future<>(const_cast<dispatcher *>(this), id, it->second->h());
#endif
}
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC future<> dispatcher::op_from_scheduled_id(size_t id) const
{
    future<> ret;
    {
        lock_guard<decltype(p->opslock)> g(p->opslock);
        ret=int_op_from_scheduled_id(id);
    }
    return ret;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void dispatcher::post_op_filter_clear()
{
    p->filters.clear();
    p->filters_buffers.clear();
}
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void dispatcher::post_op_filter(std::vector<std::pair<detail::OpType, std::function<dispatcher::filter_t>>> filters)
{
    p->filters.reserve(p->filters.size()+filters.size());
    p->filters.insert(p->filters.end(), std::make_move_iterator(filters.begin()), std::make_move_iterator(filters.end()));
}
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void dispatcher::post_readwrite_filter(std::vector<std::pair<detail::OpType, std::function<dispatcher::filter_readwrite_t>>> filters)
{
    p->filters_buffers.reserve(p->filters_buffers.size()+filters.size());
    p->filters_buffers.insert(p->filters_buffers.end(), std::make_move_iterator(filters.begin()), std::make_move_iterator(filters.end()));
}


#if defined(BOOST_AFIO_ENABLE_BENCHMARKING_COMPLETION) || BOOST_AFIO_HEADERS_ONLY==0
// Called in unknown thread
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC dispatcher::completion_returntype dispatcher::invoke_user_completion_fast(size_t id, future<> op, dispatcher::completion_t *callback)
{
    return callback(id, op);
}
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<future<>> dispatcher::completion(const std::vector<future<>> &ops, const std::vector<std::pair<async_op_flags, dispatcher::completion_t *>> &callbacks)
{
    if(!ops.empty() && ops.size()!=callbacks.size())
        BOOST_AFIO_THROW(std::runtime_error("The sequence of preconditions must either be empty or exactly the same length as callbacks."));
    std::vector<future<>> ret;
    ret.reserve(callbacks.size());
    std::vector<future<>>::const_iterator i;
    std::vector<std::pair<async_op_flags, dispatcher::completion_t *>>::const_iterator c;
    detail::immediate_async_ops immediates(callbacks.size());
    if(ops.empty())
    {
        future<> empty;
        for(auto & c: callbacks)
        {
            ret.push_back(chain_async_op(immediates, (int) detail::OpType::UserCompletion, empty, c.first, &dispatcher::invoke_user_completion_fast, c.second));
        }
    }
    else for(i=ops.begin(), c=callbacks.begin(); i!=ops.end() && c!=callbacks.end(); ++i, ++c)
        ret.push_back(chain_async_op(immediates, (int) detail::OpType::UserCompletion, *i, c->first, &dispatcher::invoke_user_completion_fast, c->second));
    return ret;
}
#endif
// Called in unknown thread
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC dispatcher::completion_returntype dispatcher::invoke_user_completion_slow(size_t id, future<> op, std::function<dispatcher::completion_t> callback)
{
    return callback(id, std::move(op));
}
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<future<>> dispatcher::completion(const std::vector<future<>> &ops, const std::vector<std::pair<async_op_flags, std::function<dispatcher::completion_t>>> &callbacks)
{
    if(!ops.empty() && ops.size()!=callbacks.size())
        BOOST_AFIO_THROW(std::runtime_error("The sequence of preconditions must either be empty or exactly the same length as callbacks."));
    std::vector<future<>> ret;
    ret.reserve(callbacks.size());
    std::vector<future<>>::const_iterator i;
    std::vector<std::pair<async_op_flags, std::function<dispatcher::completion_t>>>::const_iterator c;
    detail::immediate_async_ops immediates(callbacks.size());
    if(ops.empty())
    {
        future<> empty;
        for(auto & c: callbacks)
        {
            ret.push_back(chain_async_op(immediates, (int) detail::OpType::UserCompletion, empty, c.first, &dispatcher::invoke_user_completion_slow, c.second));
        }
    }
    else for(i=ops.begin(), c=callbacks.begin(); i!=ops.end() && c!=callbacks.end(); ++i, ++c)
            ret.push_back(chain_async_op(immediates, (int) detail::OpType::UserCompletion, *i, c->first, &dispatcher::invoke_user_completion_slow, c->second));
    return ret;
}

// Called in unknown thread
BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void dispatcher::complete_async_op(size_t id, handle_ptr h, exception_ptr e)
{
    detail::immediate_async_ops immediates(1);
    std::shared_ptr<detail::async_file_io_dispatcher_op> thisop;
    std::vector<detail::async_file_io_dispatcher_op::completion_t> completions;
    {
#ifdef BOOST_AFIO_USE_CONCURRENT_UNORDERED_MAP
        // We can atomically remove without destruction
        auto np=p->ops.extract(id);
        if(!np)
#else
        lock_guard<decltype(p->opslock)> g(p->opslock);
        // Find me in ops, remove my completions and delete me from extant ops
        auto it=p->ops.find(id);
        if(p->ops.end()==it)
#endif
        {
#ifndef NDEBUG
            std::vector<size_t> opsids;
            for(auto &i: p->ops)
            {
                opsids.push_back(i.first);
            }
            std::sort(opsids.begin(), opsids.end());
#endif
            BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this operation in list of currently executing operations"));
        }
#ifdef BOOST_AFIO_USE_CONCURRENT_UNORDERED_MAP
        thisop.swap(np->second);
#else
        thisop.swap(it->second); // thisop=it->second;
        // Erase me from ops
        p->ops.erase(it);
#endif
        // Ok so this op is now removed from the ops list.
        // Because chain_async_op() holds the opslock during the finding of preconditions
        // and adding ops to its completions, we can now safely detach our completions
        // into stack storage and process them from there without holding any locks
        completions=std::move(thisop->completions);
    }
    // Early set stl_future
    if(e)
    {
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
        auto unexception_stack=detail::Undoer([]{
          delete afio_exception_stack();
          afio_exception_stack()=nullptr;
        });
        if(afio_exception_stack() && !(p->testing_flags & detail::unit_testing_flags::no_symbol_lookup))
        {
          std::string originalmsg;
          error_code ec;
          bool is_runtime_error=false, is_system_error=false;
          try { rethrow_exception(e); }
          catch(const system_error &r) { ec=r.code(); originalmsg=r.what(); is_system_error=true; }
          catch(const std::runtime_error &r) { originalmsg=r.what(); is_runtime_error=true; }
          catch(...) { }
          if(is_runtime_error || is_system_error)
          {
            // Append the stack to the runtime error message
            std::ostringstream buffer;
            buffer << originalmsg << ". Op was scheduled at:\n";
            print_stack(buffer, thisop->stack);
            buffer << "Exceptions were thrown within the engine at:\n";
            for(auto &i : *afio_exception_stack())
            {
              //buffer << i.name << " Backtrace:\n";
              print_stack(buffer, i.stack);
            }
            if(is_system_error)
              e=BOOST_AFIO_V2_NAMESPACE::make_exception_ptr(system_error(ec, buffer.str()));
            else
              e=BOOST_AFIO_V2_NAMESPACE::make_exception_ptr(std::runtime_error(buffer.str()));
          }
        }
#endif
        thisop->enqueuement.set_future_exception(e);
        /*if(!thisop->enqueuement.get_future().is_ready())
        {
            int a=1;
        }
        assert(thisop->enqueuement.get_future().is_ready());
        assert(thisop->h().is_ready());
        assert(thisop->enqueuement.get_future().get_exception_ptr()==e);
        assert(thisop->h().get_exception_ptr()==e);*/
    }
    else
    {
        thisop->enqueuement.set_future_value(h);
        /*if(!thisop->enqueuement.get_future().is_ready())
        {
            int a=1;
        }
        assert(thisop->enqueuement.get_future().is_ready());
        assert(thisop->h().is_ready());
        assert(thisop->enqueuement.get_future().get()==h);
        assert(thisop->h().get()==h);*/
    }
    BOOST_AFIO_DEBUG_PRINT("X %u %p e=%d f=%p (uc=%u, c=%u)\n", (unsigned) id, h.get(), !!e, &thisop->h(), (unsigned) h.use_count(), (unsigned) thisop->completions.size());
    // Any post op filters installed? If so, invoke those now.
    if(!p->filters.empty())
    {
        future<> me(this, id, thisop->h(), false);
        for(auto &i: p->filters)
        {
            if(i.first==detail::OpType::Unknown || i.first==thisop->optype)
            {
                try
                {
                    i.second(thisop->optype, me);
                }
                catch(...)
                {
                    // throw it away
                }
            }
        }
    }
    if(!completions.empty())
    {
        for(auto &c: completions)
        {
            detail::async_file_io_dispatcher_op *c_op=c.second.get();
            BOOST_AFIO_DEBUG_PRINT("X %u (f=%u) > %u\n", (unsigned) id, (unsigned) c_op->flags, (unsigned) c.first);
            if(!!(c_op->flags & async_op_flags::immediate))
                immediates.enqueue(c_op->enqueuement);
            else
                p->pool->enqueue(c_op->enqueuement);
        }
    }
}


// Called in unknown thread 
template<class F, class... Args> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC handle_ptr dispatcher::invoke_async_op_completions(size_t id, future<> op, completion_returntype(F::*f)(size_t, future<>, Args...), Args... args)
{
    try
    {
#ifndef NDEBUG
        // Find our op
        {
            lock_guard<decltype(p->opslock)> g(p->opslock);
            auto it(p->ops.find(id));
            if(p->ops.end()==it)
            {
#ifndef NDEBUG
                std::vector<size_t> opsids;
                for(auto &i: p->ops)
                {
                    opsids.push_back(i.first);
                }
                std::sort(opsids.begin(), opsids.end());
#endif
                BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this operation in list of currently executing operations"));
            }
        }
#endif
        completion_returntype ret((static_cast<F *>(this)->*f)(id, std::move(op), args...));
        // If boolean is false, reschedule completion notification setting it to ret.second, otherwise complete now
        if(ret.first)
        {
            complete_async_op(id, ret.second);
        }
        return ret.second;
    }
    catch(...)
    {
        exception_ptr e(current_exception());
        assert(e);
        BOOST_AFIO_DEBUG_PRINT("E %u begin\n", (unsigned) id);
        complete_async_op(id, e);
        BOOST_AFIO_DEBUG_PRINT("E %u end\n", (unsigned) id);
        // complete_async_op() ought to have sent our exception state to our stl_future,
        // so can silently drop the exception now
        return handle_ptr();
    }
}


// You MUST hold opslock before entry!
template<class F, class... Args> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC future<> dispatcher::chain_async_op(detail::immediate_async_ops &immediates, int optype, const future<> &precondition, async_op_flags flags, completion_returntype(F::*f)(size_t, future<>, Args...), Args... args)
{   
    size_t thisid=0;
    while(!(thisid=++p->monotoniccount));
#if 0 //ndef NDEBUG
    if(!p->ops.empty())
    {
        std::vector<size_t> opsids;
        for(auto &i: p->ops)
        {
            opsids.push_back(i.first);
        }
        std::sort(opsids.begin(), opsids.end());
        assert(thisid==opsids.back()+1);
    }
#endif
    // Wrap supplied implementation routine with a completion dispatcher
    auto wrapperf=&dispatcher::invoke_async_op_completions<F, Args...>;
    // Make a new future<> ready for returning
    auto thisop=std::make_shared<detail::async_file_io_dispatcher_op>((detail::OpType) optype, flags);
    // Bind supplied implementation routine to this, unique id, precondition and any args they passed
    thisop->enqueuement.set_task(std::bind(wrapperf, this, thisid, precondition, f, args...));
    // Set the output shared stl_future
    future<> ret(this, thisid, thisop->h());
    typename detail::async_file_io_dispatcher_op::completion_t item(std::make_pair(thisid, thisop));
    bool done=false;
    auto unopsit=detail::Undoer([this, thisid](){
        std::string what;
        try { throw; } catch(std::exception &e) { what=e.what(); } catch(...) { what="not a std exception"; }
        BOOST_AFIO_DEBUG_PRINT("E X %u (%s)\n", (unsigned) thisid, what.c_str());
        {
#ifdef BOOST_AFIO_USE_CONCURRENT_UNORDERED_MAP
            p->ops.erase(thisid);
#else
            lock_guard<decltype(p->opslock)> g(p->opslock);
            auto opsit=p->ops.find(thisid);
            if(p->ops.end()!=opsit) p->ops.erase(opsit);
#endif
        }
    });
#ifdef BOOST_AFIO_USE_CONCURRENT_UNORDERED_MAP
    p->ops.insert(item);
    if(precondition.id())
    {
        // If still in flight, chain item to be executed when precondition completes
        typedef typename decltype(p->ops)::value_type op_value_type;
        p->ops.visit(precondition.id, [&item, &done](const op_value_type &dep){
          dep.second->completions.push_back(item);
          done=true;
        });
    }
#else
    {
        /* This is a weird bug which took me several days to track down ...
        It turns out that libstdc++ 4.8.0 will *move* insert item into
        p->ops because item's type is not *exactly* the value_type wanted
        by unordered_map. That destroys item for use later, which is
        obviously _insane_. The workaround is to feed insert() a copy. */
        auto item2(item);
        {
            lock_guard<decltype(p->opslock)> g(p->opslock);
            auto opsit=p->ops.insert(std::move(item2));
            assert(opsit.second);
            if(precondition.id())
            {
                // If still in flight, chain item to be executed when precondition completes
                auto dep(p->ops.find(precondition.id()));
                if(p->ops.end()!=dep)
                {
                    dep->second->completions.push_back(item);
                    done=true;
                }
            }
        }
    }
#endif
    auto undep=detail::Undoer([done, this, &precondition, &item](){
        if(done)
        {
#ifdef BOOST_AFIO_USE_CONCURRENT_UNORDERED_MAP
            typedef typename decltype(p->ops)::value_type op_value_type;
            p->ops.visit(precondition.id(), [&item](const op_value_type &dep){
                // Items may have been added by other threads ...
                for(auto it=--dep.second->completions.end(); true; --it)
                {
                    if(it->first==item.first)
                    {
                        dep.second->completions.erase(it);
                        break;
                    }
                    if(dep.second->completions.begin()==it) break;
                }              
            });
#else
            lock_guard<decltype(p->opslock)> g(p->opslock);
            auto dep(p->ops.find(precondition.id()));
            // Items may have been added by other threads ...
            for(auto it=--dep->second->completions.end(); true; --it)
            {
                if(it->first==item.first)
                {
                    dep->second->completions.erase(it);
                    break;
                }
                if(dep->second->completions.begin()==it) break;
            }
#endif
        }
    });
    BOOST_AFIO_DEBUG_PRINT("I %u (d=%d) < %u (%s)\n", (unsigned) thisid, done, (unsigned) precondition.id(), detail::optypes[static_cast<int>(optype)]);
    if(!done)
    {
        // Bind input handle now and queue immediately to next available thread worker
#if 0
        if(precondition.id() && !precondition.h.valid())
        {
            // It should never happen that precondition.id is valid but removed from extant ops
            // which indicates it completed and yet h remains invalid
            BOOST_AFIO_THROW_FATAL(std::runtime_error("Precondition was not in list of extant ops, yet its stl_future is invalid. This should never happen for any real op, so it's probably memory corruption."));
        }
#endif
        if(!!(flags & async_op_flags::immediate))
            immediates.enqueue(thisop->enqueuement);
        else
            p->pool->enqueue(thisop->enqueuement);
    }
    undep.dismiss();
    unopsit.dismiss();
    return ret;
}

// Generic op receiving specialisation i.e. precondition is also input op. Skips sanity checking.
template<class F> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<future<>> dispatcher::chain_async_ops(int optype, const std::vector<future<>> &preconditions, async_op_flags flags, completion_returntype(F::*f)(size_t, future<>, future<>))
{
  std::vector<future<>> ret;
  ret.reserve(preconditions.size());
  detail::immediate_async_ops immediates(preconditions.size());
  for (auto &i : preconditions)
  {
    ret.push_back(chain_async_op(immediates, optype, i, flags, f, i));
  }
  return ret;
}
// General non-specialised implementation taking some arbitrary parameter T with precondition
template<class F, class T> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<future<>> dispatcher::chain_async_ops(int optype, const std::vector<future<>> &preconditions, const std::vector<T> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, future<>, T))
{
  std::vector<future<>> ret;
  ret.reserve(container.size());
  assert(preconditions.size() == container.size());
  if (preconditions.size() != container.size())
    BOOST_AFIO_THROW(std::runtime_error("preconditions size does not match size of ops data"));
  detail::immediate_async_ops immediates(preconditions.size());
  auto precondition_it = preconditions.cbegin();
  auto container_it = container.cbegin();
  for (; precondition_it != preconditions.cend() && container_it != container.cend(); ++precondition_it, ++container_it)
    ret.push_back(chain_async_op(immediates, optype, *precondition_it, flags, f, *container_it));
  return ret;
}
// General non-specialised implementation taking some arbitrary parameter T containing precondition returning custom type
template<class R, class F> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<future<R>> dispatcher::chain_async_ops(int optype, const std::vector<future<>> &preconditions, async_op_flags flags, completion_returntype(F::*f)(size_t, future<>, std::shared_ptr<promise<R>>))
{
  std::vector<future<R>> ret;
  ret.reserve(preconditions.size());
  detail::immediate_async_ops immediates(preconditions.size());
  for (auto &i : preconditions)
  {
    auto s(std::make_shared<promise<R>>());
    ret.push_back(future<R>(chain_async_op(immediates, optype, i, flags, f, s), s->get_future()));
  }
  return ret;
}
// General non-specialised implementation taking some arbitrary parameter T with precondition returning custom type
template<class R, class F, class T> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<future<R>> dispatcher::chain_async_ops(int optype, const std::vector<future<>> &preconditions, const std::vector<T> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, future<>, T, std::shared_ptr<promise<R>>))
{
  std::vector<future<R>> ret;
  ret.reserve(container.size());
  assert(preconditions.size() == container.size());
  if (preconditions.size() != container.size())
    BOOST_AFIO_THROW(std::runtime_error("preconditions size does not match size of ops data"));
  detail::immediate_async_ops immediates(preconditions.size());
  auto precondition_it = preconditions.cbegin();
  auto container_it = container.cbegin();
  for (; precondition_it != preconditions.cend() && container_it != container.cend(); ++precondition_it, ++container_it)
  {
    auto s(std::make_shared<promise<R>>());
    ret.push_back(future<R>(chain_async_op(immediates, optype, *precondition_it, flags, f, *container_it, s), s->get_future()));
  }
  return ret;
}
// General non-specialised implementation taking some arbitrary parameter T containing precondition
template<class F, class T> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<future<>> dispatcher::chain_async_ops(int optype, const std::vector<T> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, future<>, T))
{
  std::vector<future<>> ret;
  ret.reserve(container.size());
  detail::immediate_async_ops immediates(container.size());
  for (auto &i : container)
  {
    ret.push_back(chain_async_op(immediates, optype, i.precondition, flags, f, i));
  }
  return ret;
}
// General non-specialised implementation taking some arbitrary parameter T containing precondition returning custom type
template<class R, class F, class T> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<future<R>> dispatcher::chain_async_ops(int optype, const std::vector<T> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, future<>, T, std::shared_ptr<promise<R>>))
{
  std::vector<future<R>> ret;
  ret.reserve(container.size());
  detail::immediate_async_ops immediates(container.size());
  for (auto &i : container)
  {
    auto s(std::make_shared<promise<R>>());
    ret.push_back(future<R>(chain_async_op(immediates, optype, i.precondition, flags, f, i, s), s->get_future()));
  }
  return ret;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<future<>> dispatcher::adopt(const std::vector<handle_ptr> &hs)
{
    std::vector<future<>> preconditions(hs.size());
    return chain_async_ops((int) detail::OpType::adopt, preconditions, hs, async_op_flags::immediate, &dispatcher::doadopt);
}

namespace detail
{
    struct barrier_count_completed_state
    {
        atomic<size_t> togo;
        std::vector<std::pair<size_t, handle_ptr>> out;
        std::vector<shared_future<handle_ptr>> insharedstates;
        barrier_count_completed_state(const std::vector<future<>> &ops) : togo(ops.size()), out(ops.size())
        {
            insharedstates.reserve(ops.size());
            for(auto &i: ops)
            {
                insharedstates.push_back(i._h);
            }
        }
    };
}

/* This is extremely naughty ... you really shouldn't be using templates to hide implementation
types, but hey it works and is non-header so so what ...
*/
//template<class T> dispatcher::completion_returntype dispatcher::dobarrier(size_t id, future<> op, T state);
template<> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC dispatcher::completion_returntype dispatcher::dobarrier<std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t>>(size_t id, future<> op, std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t> state)
{
    handle_ptr h(op.get_handle(true)); // Ignore any error state until later
    size_t idx=state.second;
    detail::barrier_count_completed_state &s=*state.first;
    s.out[idx]=std::make_pair(id, h); // This might look thread unsafe, but each idx is unique
    if(--state.first->togo)
        return std::make_pair(false, h);
#if 1
    // On the basis that probably the preceding decrementing thread has yet to signal their stl_future,
    // give up my timeslice
    this_thread::yield();
#endif
#if BOOST_AFIO_USE_BOOST_THREAD
    // Rather than potentially expend a syscall per wait on each input op to complete, compose a list of input futures and wait on them all
    std::vector<shared_future<handle_ptr>> notready;
    notready.reserve(s.insharedstates.size()-1);
    for(idx=0; idx<s.insharedstates.size(); idx++)
    {
        shared_future<handle_ptr> &f=s.insharedstates[idx];
        if(idx==state.second || is_ready(f)) continue;
        notready.push_back(f);
    }
    if(!notready.empty())
        boost::wait_for_all(notready.begin(), notready.end());
#else
    for(idx=0; idx<s.insharedstates.size(); idx++)
    {
        shared_future<handle_ptr> &f=s.insharedstates[idx];
        if(idx==state.second || is_ready(f)) continue;
        f.wait();
    }
#endif
    // Last one just completed, so issue completions for everything in out including myself
    for(idx=0; idx<s.out.size(); idx++)
    {
        shared_future<handle_ptr> &thisresult=s.insharedstates[idx];
        exception_ptr e(get_exception_ptr(thisresult));
        complete_async_op(s.out[idx].first, s.out[idx].second, e);
    }
    // As I just completed myself above, prevent any further processing
    return std::make_pair(false, handle_ptr());
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<future<>> dispatcher::barrier(const std::vector<future<>> &ops)
{
#if BOOST_AFIO_VALIDATE_INPUTS
        for(auto &i: ops)
        {
            if(!i.validate(false))
                BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
        }
#endif
    // Create a shared state for the completions to be attached to all the items we are waiting upon
    auto state(std::make_shared<detail::barrier_count_completed_state>(ops));
    // Create each of the parameters to be sent to each dobarrier
    std::vector<std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t>> statev;
    statev.reserve(ops.size());
    size_t idx=0;
    for(auto &op: ops)
    {
        statev.push_back(std::make_pair(state, idx++));
    }
    return chain_async_ops((int) detail::OpType::barrier, ops, statev, async_op_flags::immediate, &dispatcher::dobarrier<std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t>>);
}

namespace detail {
    class async_file_io_dispatcher_compat : public dispatcher
    {
        template<class Impl, class Handle> friend handle_ptr detail::decode_relative_path(path_req &req, bool force_absolute);
        friend class dispatcher;
        friend struct async_io_handle_posix;
        handle_ptr decode_relative_path(path_req &req, bool force_absolute=false)
        {
          return detail::decode_relative_path<async_file_io_dispatcher_compat, async_io_handle_posix>(req, force_absolute);
        }
        // Called in unknown thread
        completion_returntype dodir(size_t id, future<> op, path_req req)
        {
            req.flags=fileflags(req.flags)|file_flags::int_opening_dir|file_flags::read;
            // TODO FIXME: Currently file_flags::create may duplicate handles in the dirhcache
            if(!(req.flags & file_flags::unique_directory_handle) && !!(req.flags & file_flags::read) && !(req.flags & file_flags::write) && !(req.flags & (file_flags::create|file_flags::create_only_if_not_exist)))
            {
              path_req req2(req);
              // Return a copy of the one in the dir cache if available as a fast path
              decode_relative_path(req2, true);
              try
              {
                return std::make_pair(true, p->get_handle_to_dir(this, id, req2, &async_file_io_dispatcher_compat::dofile));
              }
              catch(...)
              {
                // fall through
              }
            }
            // This will add itself to the dir cache if it's eligible
            return dofile(id, op, req);
        }
        // Called in unknown thread
        completion_returntype dounlink(bool is_dir, size_t id, future<> op, path_req req)
        {
            req.flags=fileflags(req.flags);
            // Deleting the input op?
            if(req.path.empty())
            {
              handle_ptr h(op.get_handle());
              async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
              p->unlink();
              return std::make_pair(true, op.get_handle());
            }
            auto dirh=decode_relative_path(req);
            BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_UNLINKAT(dirh ? (int)(size_t)dirh->native_handle() : at_fdcwd, req.path.c_str(), is_dir ? AT_REMOVEDIR : 0), [&req]{return req.path;});
            return std::make_pair(true, op.get_handle());
        }
        // Called in unknown thread
        completion_returntype dormdir(size_t id, future<> op, path_req req)
        {
          return dounlink(true, id, std::move(op), std::move(req));
        }
        // Called in unknown thread
        completion_returntype dofile(size_t id, future<> op, path_req req)
        {
            int flags=0, fd;
            req.flags=fileflags(req.flags);
            handle_ptr dirh=decode_relative_path(req);

            // POSIX doesn't permit directories to be opened with write access
            if(!!(req.flags & file_flags::int_opening_dir))
              flags|=O_RDONLY;
            else
            {
              if(!!(req.flags & file_flags::read) && !!(req.flags & file_flags::write)) flags|=O_RDWR;
              else if(!!(req.flags & file_flags::read)) flags|=O_RDONLY;
              else if(!!(req.flags & file_flags::write)) flags|=O_WRONLY;
            }
#ifdef O_SYNC
            if(!!(req.flags & file_flags::always_sync)) flags|=O_SYNC;
#endif
            if(!!(req.flags & file_flags::int_opening_link))
            {
#ifdef WIN32
              BOOST_AFIO_THROW(std::runtime_error("Creating symbolic links via MSVCRT is not supported on Windows."));
#else
              if(!!(req.flags & (file_flags::create|file_flags::create_only_if_not_exist)))
              {
                  fd=BOOST_AFIO_POSIX_SYMLINKAT(op->path().c_str(), dirh ? (int)(size_t)dirh->native_handle() : at_fdcwd, req.path.c_str());
                  if(-1==fd && EEXIST==errno)
                  {
                      // Ignore already exists unless we were asked otherwise
                      if(!(req.flags & file_flags::create_only_if_not_exist))
                          fd=0;
                  }
                  BOOST_AFIO_ERRHOSFN(fd, [&req]{return req.path;});
              }
#endif
#ifdef O_PATH
              // Some platforms allow the direct opening of symbolic links as file descriptors. If so,
              // symlinks become as unracy as files.
              flags|=O_PATH|O_NOFOLLOW;
              // fall through
#else
              // POSIX doesn't allow you to open symbolic links, so return a closed handle
              if(dirh)
                req.path=dirh->path()/req.path;
              auto ret=std::make_shared<async_io_handle_posix>(this, req.path, req.flags, false, false, -999);
              return std::make_pair(true, ret);
#endif
            }
            else if(!!(req.flags & file_flags::int_opening_dir))
            {
#ifdef O_DIRECTORY
                flags|=O_DIRECTORY;
#endif
#ifdef O_SEARCH
                flags|=O_SEARCH;
#endif
                if(!!(req.flags & file_flags::create) || !!(req.flags & file_flags::create_only_if_not_exist))
                {
                    fd=BOOST_AFIO_POSIX_MKDIRAT(dirh ? (int)(size_t)dirh->native_handle() : at_fdcwd, req.path.c_str(), 0x1f8/*770*/);
                    if(-1==fd && EEXIST==errno)
                    {
                        // Ignore already exists unless we were asked otherwise
                        if(!(req.flags & file_flags::create_only_if_not_exist))
                            fd=0;
                    }
                    BOOST_AFIO_ERRHOSFN(fd, [&req]{return req.path;});
                }
            }
            else
            {
                if(!!(req.flags & file_flags::append)) flags|=O_APPEND;
                if(!!(req.flags & file_flags::truncate)) flags|=O_TRUNC;
                if(!!(req.flags & file_flags::create_only_if_not_exist)) flags|=O_EXCL|O_CREAT;
                else if(!!(req.flags & file_flags::create)) flags|=O_CREAT;
#ifdef O_DIRECT
                if(!!(req.flags & file_flags::os_direct)) flags|=O_DIRECT;
#endif
            }
            fd=BOOST_AFIO_POSIX_OPENAT(dirh ? (int)(size_t)dirh->native_handle() : at_fdcwd, req.path.c_str(), flags, 0x1b0/*660*/);
            if(dirh)
              req.path=dirh->path()/req.path;
#ifdef O_PATH
            // Some kernels don't really implement O_PATH
            if(-1==fd && ELOOP==errno && !!(flags&O_PATH))
            {
              auto ret=std::make_shared<async_io_handle_posix>(this, req.path, req.flags, false, false, -999);
              return std::make_pair(true, ret);
            }
#endif
            // If writing and SyncOnClose and NOT synchronous, turn on SyncOnClose
            auto ret=std::make_shared<async_io_handle_posix>(this, req.path, req.flags, (file_flags::create_only_if_not_exist|file_flags::delete_on_close)==(req.flags & (file_flags::create_only_if_not_exist|file_flags::delete_on_close)), (file_flags::sync_on_close|file_flags::write)==(req.flags & (file_flags::sync_on_close|file_flags::write|file_flags::always_sync)), fd);
            static_cast<async_io_handle_posix *>(ret.get())->do_add_io_handle_to_parent();
            if(!(req.flags & file_flags::int_opening_dir) && !(req.flags & file_flags::int_opening_link))
            {              
              if(!!(req.flags & file_flags::will_be_sequentially_accessed) || !!(req.flags & file_flags::will_be_randomly_accessed))
              {
#ifndef WIN32
#if !defined(__APPLE__)
                int advice=!!(req.flags & file_flags::will_be_sequentially_accessed) ?
                  (POSIX_FADV_SEQUENTIAL|POSIX_FADV_WILLNEED) :
                  (POSIX_FADV_RANDOM);
                BOOST_AFIO_ERRHOSFN(::posix_fadvise(fd, 0, 0, advice), [&req]{return req.path;});
#endif
#if defined(__FreeBSD__)
                size_t readaheadsize=!!(req.flags & file_flags::will_be_sequentially_accessed) ? utils::file_buffer_default_size() : 0;
                BOOST_AFIO_ERRHOSFN(::fcntl(fd, F_READAHEAD, readaheadsize), [&req]{return req.path;});
#elif defined(__APPLE__)
                size_t readahead=!!(req.flags & file_flags::will_be_sequentially_accessed) ? 1 : 0;
                BOOST_AFIO_ERRHOSFN(::fcntl(fd, F_RDAHEAD, readahead), [&req]{return req.path;});
#endif
#endif
              }
            }
            return std::make_pair(true, ret);
        }
        // Called in unknown thread
        completion_returntype dormfile(size_t id, future<> op, path_req req)
        {
          return dounlink(false, id, std::move(op), std::move(req));
        }
        // Called in unknown thread
        completion_returntype dosymlink(size_t id, future<> op, path_req req)
        {
            req.flags=fileflags(req.flags)|file_flags::int_opening_link;
            return dofile(id, op, req);
        }
        // Called in unknown thread
        completion_returntype dormsymlink(size_t id, future<> op, path_req req)
        {
          return dounlink(false, id, std::move(op), std::move(req));
        }
        // Called in unknown thread
        completion_returntype dozero(size_t id, future<> op, std::vector<std::pair<off_t, off_t>> ranges)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            bool done=false;
#if defined(__linux__)
            done=true;
            for(auto &i: ranges)
            {
              int ret;
              while(-1==(ret=fallocate(p->fd, 0x02/*FALLOC_FL_PUNCH_HOLE*/|0x01/*FALLOC_FL_KEEP_SIZE*/, i.first, i.second)) && EINTR==errno);
              if(-1==ret)
              {
                // The filing system may not support trim
                if(EOPNOTSUPP==errno)
                {
                  done=false;
                  break;
                }
                BOOST_AFIO_ERRHOSFN(-1, [p]{return p->path();});
              }
            }
#endif
            // Fall back onto a write of zeros
            if(!done)
            {
              std::vector<char, utils::page_allocator<char>> buffer(utils::file_buffer_default_size());
              for(auto &i: ranges)
              {
                ssize_t byteswritten=0;
                std::vector<iovec> vecs(1+(size_t)(i.second/buffer.size()));
                for(size_t n=0; n<vecs.size(); n++)
                {
                  vecs[n].iov_base=buffer.data();
                  vecs[n].iov_len=(n<vecs.size()-1) ? buffer.size() : (size_t)(i.second-(off_t) n*buffer.size());
                }
                for(size_t n=0; n<vecs.size(); n+=IOV_MAX)
                {
                    ssize_t _byteswritten;
                    size_t amount=std::min((int) (vecs.size()-n), IOV_MAX);
                    off_t offset=i.first+byteswritten;
                    while(-1==(_byteswritten=pwritev(p->fd, (&vecs.front())+n, (int) amount, offset)) && EINTR==errno)
                      /*empty*/;
                    BOOST_AFIO_ERRHOSFN((int) _byteswritten, [p]{return p->path();});
                    byteswritten+=_byteswritten;
                } 
              }
            }
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        completion_returntype dosync(size_t id, future<> op, future<>)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            off_t bytestobesynced=p->write_count_since_fsync();
            if(bytestobesynced)
                BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_FSYNC(p->fd), [p]{return p->path();});
            p->has_ever_been_fsynced=true;
            p->byteswrittenatlastfsync+=bytestobesynced;
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        completion_returntype doclose(size_t id, future<> op, future<>)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            if(!!(p->flags() & file_flags::int_opening_dir) && p->available_to_directory_cache())
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
        completion_returntype doread(size_t id, future<> op, detail::io_req_impl<false> req)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            ssize_t bytesread=0, bytestoread=0;
            iovec v;
            BOOST_AFIO_DEBUG_PRINT("R %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
            for(auto &b: req.buffers)
            {   
                BOOST_AFIO_DEBUG_PRINT("  R %u: %p %u\n", (unsigned) id, asio::buffer_cast<const void *>(b), (unsigned) asio::buffer_size(b));
            }
#endif
            std::vector<iovec> vecs;
            vecs.reserve(req.buffers.size());
            for(auto &b: req.buffers)
            {
                v.iov_base=asio::buffer_cast<void *>(b);
                v.iov_len=asio::buffer_size(b);
                bytestoread+=v.iov_len;
                vecs.push_back(v);
            }
            for(size_t n=0; n<vecs.size(); n+=IOV_MAX)
            {
                ssize_t _bytesread;
                size_t amount=std::min((int) (vecs.size()-n), IOV_MAX);
                off_t offset=req.where+bytesread;
                while(-1==(_bytesread=preadv(p->fd, (&vecs.front())+n, (int) amount, offset)) && EINTR==errno);
                if(!this->p->filters_buffers.empty())
                {
                    error_code ec(errno, generic_category());
                    for(auto &i: this->p->filters_buffers)
                    {
                        if(i.first==OpType::Unknown || i.first==OpType::read)
                        {
                            i.second(OpType::read, p, req, offset, n, amount, ec, (size_t)_bytesread);
                        }
                    }
                }
                BOOST_AFIO_ERRHOSFN((int) _bytesread, [p]{return p->path();});
                p->bytesread+=_bytesread;
                bytesread+=_bytesread;
            }
            if(bytesread!=bytestoread)
                BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to read all buffers"));
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        completion_returntype dowrite(size_t id, future<> op, detail::io_req_impl<true> req)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            ssize_t byteswritten=0, bytestowrite=0;
            iovec v;
            std::vector<iovec> vecs;
            vecs.reserve(req.buffers.size());
            BOOST_AFIO_DEBUG_PRINT("W %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
            for(auto &b: req.buffers)
            {   
                BOOST_AFIO_DEBUG_PRINT("  W %u: %p %u\n", (unsigned) id, asio::buffer_cast<const void *>(b), (unsigned) asio::buffer_size(b));
            }
#endif
            for(auto &b: req.buffers)
            {
                v.iov_base=(void *) asio::buffer_cast<const void *>(b);
                v.iov_len=asio::buffer_size(b);
                bytestowrite+=v.iov_len;
                vecs.push_back(v);
            }
            for(size_t n=0; n<vecs.size(); n+=IOV_MAX)
            {
                ssize_t _byteswritten;
                size_t amount=std::min((int) (vecs.size()-n), IOV_MAX);
                off_t offset=req.where+byteswritten;
                // POSIX doesn't actually guarantee pwritev appends to O_APPEND files, and indeed OS X does not.
                if(!!(p->flags() & file_flags::append))
                {
                  while(-1==(_byteswritten=writev(p->fd, (&vecs.front())+n, (int) amount)) && EINTR==errno);
                }
                else
                {
                  while(-1==(_byteswritten=pwritev(p->fd, (&vecs.front())+n, (int) amount, offset)) && EINTR==errno);
                }
                if(!this->p->filters_buffers.empty())
                {
                    error_code ec(errno, generic_category());
                    for(auto &i: this->p->filters_buffers)
                    {
                        if(i.first==OpType::Unknown || i.first==OpType::write)
                        {
                            i.second(OpType::write, p, req, offset, n, amount, ec, (size_t) _byteswritten);
                        }
                    }
                }
                BOOST_AFIO_ERRHOSFN((int) _byteswritten, [p]{return p->path();});
                p->byteswritten+=_byteswritten;
                byteswritten+=_byteswritten;
            }
            if(byteswritten!=bytestowrite)
                BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to write all buffers"));
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        completion_returntype dotruncate(size_t id, future<> op, off_t newsize)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            BOOST_AFIO_DEBUG_PRINT("T %u %p (%c)\n", (unsigned) id, h.get(), p->path().native().back());
            int ret;
            while(-1==(ret=BOOST_AFIO_POSIX_FTRUNCATE(p->fd, newsize)) && EINTR==errno)
              /*empty*/;
            BOOST_AFIO_ERRHOSFN(ret, [p]{return p->path();});
            return std::make_pair(true, h);
        }
#ifdef __linux__
        static int int_getdents(int fd, char *buf, unsigned count) { return syscall(SYS_getdents64, fd, buf, count); }
#endif
#ifdef __APPLE__
        static int int_getdents_emulation(int fd, char *buf, unsigned count) { off_t foo; return syscall(SYS_getdirentries64, fd, buf, count, &foo); }
#endif
        // Called in unknown thread
        completion_returntype doenumerate(size_t id, future<> op, enumerate_req req, std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> ret)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            try
            {
                auto globstr=req.glob.native();
                // Is glob a single entry match? If so, skip enumerating.
                if(!globstr.empty() && std::string::npos==globstr.find('*') && std::string::npos==globstr.find('?') && std::string::npos==globstr.find('['))
                {
                    std::vector<directory_entry> _ret;
                    _ret.reserve(1);
                    BOOST_AFIO_POSIX_STAT_STRUCT s={0};
#if BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS
                    if(-1!=BOOST_AFIO_POSIX_LSTATAT((int)(size_t)p->native_handle(), req.glob.c_str(), &s))
#else
                    path path(p->path(true));
                    path/=req.glob;
                    if(-1!=BOOST_AFIO_POSIX_LSTATAT(at_fdcwd, path.c_str(), &s))
#endif
                    {
                        stat_t stat(nullptr);
                        fill_stat_t(stat, s, req.metadata);
                        _ret.push_back(directory_entry(req.glob.native(), stat, req.metadata));
                    }
                    ret->set_value(std::make_pair(std::move(_ret), false));
                    return std::make_pair(true, h);
                }
#ifdef WIN32
                BOOST_AFIO_THROW(std::runtime_error("Enumerating directories via MSVCRT is not supported."));
#else
#ifdef __linux__
                // Unlike FreeBSD, Linux doesn't define a getdents() function, so we'll do that here.
                typedef int (*getdents64_t)(int, char *, unsigned);
                getdents64_t getdents=(getdents64_t)(&async_file_io_dispatcher_compat::int_getdents);
                typedef dirent64 dirent;
#endif
#ifdef __APPLE__
                // OS X defines a getdirentries64() kernel syscall which can emulate getdents
                typedef int (*getdents_emulation_t)(int, char *, unsigned);
                getdents_emulation_t getdents=(getdents_emulation_t)(&async_file_io_dispatcher_compat::int_getdents_emulation);
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
                bool needmoremetadata=!!(req.metadata&~item.have_metadata);
                done=false;
                for(dirent *dent=buffer.get(); !done; dent=(dirent *)((size_t) dent + dent->d_reclen))
                {
                    if((bytes-=dent->d_reclen)<=0) done=true;
                    if(!dent->d_ino)
                        continue;
                    size_t length=strchr(dent->d_name, 0)-dent->d_name;
                    if(length<=2 && '.'==dent->d_name[0])
                        if(1==length || '.'==dent->d_name[1]) continue;
                    if(!req.glob.empty() && fnmatch(globstr.c_str(), dent->d_name, 0)) continue;
                    path::string_type leafname(dent->d_name, length);
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
                            item.stat.st_type=filesystem::file_type::block_file;
                            break;
                        case DT_CHR:
                            item.stat.st_type=filesystem::file_type::character_file;
                            break;
                        case DT_DIR:
                            item.stat.st_type=filesystem::file_type::directory_file;
                            break;
                        case DT_FIFO:
                            item.stat.st_type=filesystem::file_type::fifo_file;
                            break;
                        case DT_LNK:
                            item.stat.st_type=filesystem::file_type::symlink_file;
                            break;
                        case DT_REG:
                            item.stat.st_type=filesystem::file_type::regular_file;
                            break;
                        case DT_SOCK:
                            item.stat.st_type=filesystem::file_type::socket_file;
                            break;
                        default:
                            item.have_metadata=item.have_metadata&~metadata_flags::type;
                            item.stat.st_type=filesystem::file_type::type_unknown;
                            break;
                        }
                    }
                    _ret.push_back(std::move(item));
                }
                // NOTE: Potentially the OS didn't return type, and we're not checking
                // for that here.
                if(needmoremetadata)
                {
                    for(auto &i: _ret)
                    {
                        i.fetch_metadata(h, req.metadata);
                    }
                }
                ret->set_value(std::make_pair(std::move(_ret), !thisbatchdone));
#endif
                return std::make_pair(true, h);
            }
            catch(...)
            {
                ret->set_exception(current_exception());
                throw;
            }
        }
        // Called in unknown thread
        completion_returntype doextents(size_t id, future<> op, std::shared_ptr<promise<std::vector<std::pair<off_t, off_t>>>> ret)
        {
          try
          {
            handle_ptr h(op.get_handle());
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            std::vector<std::pair<off_t, off_t>> out;
            off_t start=0, end=0;
#ifndef WIN32
            for(;;)
            {
#ifdef __linux__
                start=lseek64(p->fd, end, SEEK_DATA);
                if((off_t)-1==start) break;
                end=lseek64(p->fd, start, SEEK_HOLE);
                if((off_t)-1==end) break;
#elif defined(__APPLE__)
                // Can't find any support for extent enumeration in OS X
                errno=EINVAL;
                break;
#elif defined(__FreeBSD__)
                start=lseek(p->fd, end, SEEK_DATA);
                if((off_t)-1==start) break;
                end=lseek(p->fd, start, SEEK_HOLE);
                if((off_t)-1==end) break;
#else
#error Unknown system
#endif
                // Data region may have been concurrently deleted
                if(end>start)
                  out.push_back(std::make_pair<off_t, off_t>(std::move(start), end-start));
            }
            if(ENXIO!=errno)
            {
              if(EINVAL==errno)
              {
                // If it failed with no output, probably this filing system doesn't support extents
                if(out.empty())
                {
                  struct stat s;
                  BOOST_AFIO_ERRHOS(fstat(p->fd, &s));
                  out.push_back(std::make_pair<off_t, off_t>(0, s.st_size));                
                }
              }
              else
                BOOST_AFIO_ERRHOS(-1);
            }
#endif
            // A problem with SEEK_DATA and SEEK_HOLE is that they are racy under concurrent extents changing
            // Coalesce sequences of contiguous data e.g. 0, 64; 64, 64; 128, 64 ...
            std::vector<std::pair<off_t, off_t>> outfixed; outfixed.reserve(out.size());
            outfixed.push_back(out.front());
            for(size_t n=1; n<out.size(); n++)
            {
              if(outfixed.back().first+outfixed.back().second==out[n].first)
                outfixed.back().second+=out[n].second;
              else
                outfixed.push_back(out[n]);
            }
            ret->set_value(std::move(outfixed));
            return std::make_pair(true, h);
          }
          catch(...)
          {
            ret->set_exception(current_exception());
            throw;
          }
        }
        // Called in unknown thread
        completion_returntype dostatfs(size_t id, future<> op, fs_metadata_flags req, std::shared_ptr<promise<statfs_t>> ret)
        {
          try
          {
            handle_ptr h(op.get_handle());
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            statfs_t out;
#ifndef WIN32
#ifdef __linux__
            struct statfs64 s={0};
            BOOST_AFIO_ERRHOS(fstatfs64(p->fd, &s));
            if(!!(req&fs_metadata_flags::bsize))       out.f_bsize      =s.f_bsize;
            if(!!(req&fs_metadata_flags::iosize))      out.f_iosize     =s.f_frsize;
            if(!!(req&fs_metadata_flags::blocks))      out.f_blocks     =s.f_blocks;
            if(!!(req&fs_metadata_flags::bfree))       out.f_bfree      =s.f_bfree;
            if(!!(req&fs_metadata_flags::bavail))      out.f_bavail     =s.f_bavail;
            if(!!(req&fs_metadata_flags::files))       out.f_files      =s.f_files;
            if(!!(req&fs_metadata_flags::ffree))       out.f_ffree      =s.f_ffree;
            if(!!(req&fs_metadata_flags::namemax))     out.f_namemax    =s.f_namelen;
//            if(!!(req&fs_metadata_flags::owner))       out.f_owner      =s.f_owner;
            if(!!(req&fs_metadata_flags::fsid))        { out.f_fsid[0]=(unsigned) s.f_fsid.__val[0]; out.f_fsid[1]=(unsigned) s.f_fsid.__val[1]; }
            if(!!(req&fs_metadata_flags::flags) || !!(req&fs_metadata_flags::fstypename) || !!(req&fs_metadata_flags::mntfromname) || !!(req&fs_metadata_flags::mntonname))
            {
              struct mountentry
              {
                std::string mnt_fsname, mnt_dir, mnt_type, mnt_opts;
                mountentry(const char *a, const char *b, const char *c, const char *d) : mnt_fsname(a), mnt_dir(b), mnt_type(c), mnt_opts(d) { }
              };
              std::vector<std::pair<mountentry, struct statfs64>> mountentries;
              {
                // Need to parse mount options on Linux
                FILE *mtab=setmntent("/etc/mtab", "r");
                if(!mtab) BOOST_AFIO_ERRHOS(-1);
                auto unmtab=detail::Undoer([mtab]{endmntent(mtab);});
                struct mntent m;
                char buffer[32768];
                while(getmntent_r(mtab, &m, buffer, sizeof(buffer)))
                {
                  struct statfs64 temp={0};
                  if(0==statfs64(m.mnt_dir, &temp) && temp.f_type==s.f_type && !memcmp(&temp.f_fsid, &s.f_fsid, sizeof(s.f_fsid)))
                    mountentries.push_back(std::make_pair(mountentry(m.mnt_fsname, m.mnt_dir, m.mnt_type, m.mnt_opts), temp));
                }
              }
#ifndef BOOST_AFIO_COMPILING_FOR_GCOV
              if(mountentries.empty())
                BOOST_AFIO_THROW("The filing system of this handle does not appear in /etc/mtab!");
              // Choose the mount entry with the most closely matching statfs. You can't choose
              // exclusively based on mount point because of bind mounts
              if(mountentries.size()>1)
              {
                std::vector<std::pair<size_t, size_t>> scores(mountentries.size());
                for(size_t n=0; n<mountentries.size(); n++)
                {
                  const char *a=(const char *) &mountentries[n].second;
                  const char *b=(const char *) &s;
                  scores[n].first=0;
                  for(size_t x=0; x<sizeof(struct statfs64); x++)
                    scores[n].first+=abs(a[x]-b[x]);
                  scores[n].second=n;
                }
                std::sort(scores.begin(), scores.end());
                auto temp(std::move(mountentries[scores.front().second]));
                mountentries.clear();
                mountentries.push_back(std::move(temp));
              }
#endif
              if(!!(req&fs_metadata_flags::flags))
              {
                out.f_flags.rdonly     =!!(s.f_flags & MS_RDONLY);
                out.f_flags.noexec     =!!(s.f_flags & MS_NOEXEC);
                out.f_flags.nosuid     =!!(s.f_flags & MS_NOSUID);
                out.f_flags.acls       =(std::string::npos!=mountentries.front().first.mnt_opts.find("acl") && std::string::npos==mountentries.front().first.mnt_opts.find("noacl"));
                out.f_flags.xattr      =(std::string::npos!=mountentries.front().first.mnt_opts.find("xattr") && std::string::npos==mountentries.front().first.mnt_opts.find("nouser_xattr"));
//                out.f_flags.compression=0;
                // Those filing systems supporting FALLOC_FL_PUNCH_HOLE
                out.f_flags.extents    =(mountentries.front().first.mnt_type=="btrfs"
                                      || mountentries.front().first.mnt_type=="ext4"
                                      || mountentries.front().first.mnt_type=="xfs"
                                      || mountentries.front().first.mnt_type=="tmpfs");
              }
              if(!!(req&fs_metadata_flags::fstypename)) out.f_fstypename=mountentries.front().first.mnt_type;
              if(!!(req&fs_metadata_flags::mntfromname)) out.f_mntfromname=mountentries.front().first.mnt_fsname;
              if(!!(req&fs_metadata_flags::mntonname)) out.f_mntonname=mountentries.front().first.mnt_dir;
            }
#else
            struct statfs s;
            BOOST_AFIO_ERRHOS(fstatfs(p->fd, &s));
            if(!!(req&fs_metadata_flags::flags))
            {
              out.f_flags.rdonly     =!!(s.f_flags & MNT_RDONLY);
              out.f_flags.noexec     =!!(s.f_flags & MNT_NOEXEC);
              out.f_flags.nosuid     =!!(s.f_flags & MNT_NOSUID);
              out.f_flags.acls       =0;
#if defined(MNT_ACLS) && defined(MNT_NFS4ACLS)
              out.f_flags.acls       =!!(s.f_flags & (MNT_ACLS|MNT_NFS4ACLS));
#endif
              out.f_flags.xattr      =1; // UFS and ZFS support xattr. TODO FIXME actually calculate this, zfs get xattr <f_mntfromname> would do it.
              out.f_flags.compression=!strcmp(s.f_fstypename, "zfs");
              out.f_flags.extents    =!strcmp(s.f_fstypename, "ufs") || !strcmp(s.f_fstypename, "zfs");
            }
            if(!!(req&fs_metadata_flags::bsize))       out.f_bsize      =s.f_bsize;
            if(!!(req&fs_metadata_flags::iosize))      out.f_iosize     =s.f_iosize;
            if(!!(req&fs_metadata_flags::blocks))      out.f_blocks     =s.f_blocks;
            if(!!(req&fs_metadata_flags::bfree))       out.f_bfree      =s.f_bfree;
            if(!!(req&fs_metadata_flags::bavail))      out.f_bavail     =s.f_bavail;
            if(!!(req&fs_metadata_flags::files))       out.f_files      =s.f_files;
            if(!!(req&fs_metadata_flags::ffree))       out.f_ffree      =s.f_ffree;
#ifdef __APPLE__
            if(!!(req&fs_metadata_flags::namemax))     out.f_namemax    =255;
#else
            if(!!(req&fs_metadata_flags::namemax))     out.f_namemax    =s.f_namemax;
#endif
            if(!!(req&fs_metadata_flags::owner))       out.f_owner      =s.f_owner;
            if(!!(req&fs_metadata_flags::fsid))        { out.f_fsid[0]=(unsigned) s.f_fsid.val[0]; out.f_fsid[1]=(unsigned) s.f_fsid.val[1]; }
            if(!!(req&fs_metadata_flags::fstypename))  out.f_fstypename =s.f_fstypename;
            if(!!(req&fs_metadata_flags::mntfromname)) out.f_mntfromname=s.f_mntfromname;
            if(!!(req&fs_metadata_flags::mntonname))   out.f_mntonname  =s.f_mntonname;            
#endif
#endif
            ret->set_value(std::move(out));
            return std::make_pair(true, h);
          }
          catch(...)
          {
            ret->set_exception(current_exception());
            throw;
          }
        }
        // Called in unknown thread
        completion_returntype dolock(size_t id, future<> op, lock_req req)
        {
#ifndef BOOST_AFIO_COMPILING_FOR_GCOV
            handle_ptr h(op.get_handle());
            async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
            if(!p->lockfile)
              BOOST_AFIO_THROW(std::invalid_argument("This file handle was not opened with OSLockable."));
            return p->lockfile->lock(id, std::move(op), std::move(req));
#endif
        }

    public:
        async_file_io_dispatcher_compat(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask) : dispatcher(threadpool, flagsforce, flagsmask)
        {
        }


        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> dir(const std::vector<path_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::dir, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::dodir);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> rmdir(const std::vector<path_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::rmdir, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::dormdir);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> file(const std::vector<path_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::file, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::dofile);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> rmfile(const std::vector<path_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::rmfile, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::dormfile);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> symlink(const std::vector<path_req> &reqs, const std::vector<future<>> &targets) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            std::vector<future<>> ops(targets);
            ops.resize(reqs.size());
            for(size_t n=0; n<reqs.size(); n++)
            {
              if(!ops[n].valid())
                ops[n]=reqs[n].precondition;
            }
            return chain_async_ops((int) detail::OpType::symlink, ops, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::dosymlink);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> rmsymlink(const std::vector<path_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::rmsymlink, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::dormsymlink);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> sync(const std::vector<future<>> &ops) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::sync, ops, async_op_flags::none, &async_file_io_dispatcher_compat::dosync);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> zero(const std::vector<future<>> &ops, const std::vector<std::vector<std::pair<off_t, off_t>>> &ranges) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
          for(auto &i: ops)
          {
            if(!i.validate())
              BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
          }
#endif
          return chain_async_ops((int) detail::OpType::zero, ops, ranges, async_op_flags::none, &async_file_io_dispatcher_compat::dozero);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> close(const std::vector<future<>> &ops) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::close, ops, async_op_flags::none, &async_file_io_dispatcher_compat::doclose);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> read(const std::vector<detail::io_req_impl<false>> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::read, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::doread);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> write(const std::vector<detail::io_req_impl<true>> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::write, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::dowrite);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> truncate(const std::vector<future<>> &ops, const std::vector<off_t> &sizes) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::truncate, ops, sizes, async_op_flags::none, &async_file_io_dispatcher_compat::dotruncate);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<std::pair<std::vector<directory_entry>, bool>>> enumerate(const std::vector<enumerate_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::enumerate, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::doenumerate);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<std::vector<std::pair<off_t, off_t>>>> extents(const std::vector<future<>> &ops) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::extents, ops, async_op_flags::none, &async_file_io_dispatcher_compat::doextents);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<statfs_t>> statfs(const std::vector<future<>> &ops, const std::vector<fs_metadata_flags> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
            if(ops.size()!=reqs.size())
                BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
#endif
            return chain_async_ops((int) detail::OpType::statfs, ops, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::dostatfs);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> lock(const std::vector<lock_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::lock, reqs, async_op_flags::none, &async_file_io_dispatcher_compat::dolock);
        }
    };

    inline handle_ptr async_io_handle_posix::int_verifymyinode()
    {
        BOOST_AFIO_POSIX_STAT_STRUCT s={0};
        handle_ptr dirh(container());
        for(size_t n=0; n<10; n++)
        {
          path_req req(path(true));
          if(!dirh)
          {
            dirh=container();
            if(!dirh)
            {
              try
              {
                dirh=parent()->int_get_handle_to_containing_dir(static_cast<async_file_io_dispatcher_compat *>(parent()), 0, req, &async_file_io_dispatcher_compat::dofile);
              }
              catch(...)
              {
                // Path to containing directory may have gone stale
                this_thread::sleep_for(chrono::milliseconds(1));
                continue;
              }
            }
          }
#if BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS
          if(-1!=BOOST_AFIO_POSIX_LSTATAT((int)(size_t)dirh->native_handle(), req.path.filename().c_str(), &s))
#else
          if(-1!=BOOST_AFIO_POSIX_LSTATAT(at_fdcwd, req.path.c_str(), &s))
#endif
          {
            if(s.st_dev==st_dev && s.st_ino==st_ino)
              return dirh;
          }
          // Didn't find an entry with my name, so sleep and retry
#ifndef NDEBUG
          std::cout << "WARNING: Did not find inode " << st_ino << " in directory " << dirh->path() << ", sleeping." << std::endl;
#endif
          dirh.reset();
          this_thread::sleep_for(chrono::milliseconds(1));
        }
        return dirh;
    }
}

BOOST_AFIO_V2_NAMESPACE_END

#if defined(WIN32) && !defined(USE_POSIX_ON_WIN32)
#include "afio_iocp.ipp"
#endif

BOOST_AFIO_V2_NAMESPACE_BEGIN

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void directory_entry::_int_fetch(metadata_flags wanted, handle_ptr dirh)
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
            BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[sizeof(FILE_ID_FULL_DIR_INFORMATION)/sizeof(path::value_type)+32769];
            IO_STATUS_BLOCK isb={ 0 };
            UNICODE_STRING _glob;
            NTSTATUS ntstat;
            _glob.Buffer=const_cast<path::value_type *>(leafname.c_str());
            _glob.MaximumLength=(_glob.Length=(USHORT) (leafname.size()*sizeof(path::value_type)))+sizeof(path::value_type);
            FILE_ID_FULL_DIR_INFORMATION *ffdi=(FILE_ID_FULL_DIR_INFORMATION *) buffer;
            ntstat=NtQueryDirectoryFile(dirh->native_handle(), NULL, NULL, NULL, &isb, ffdi, sizeof(buffer),
                FileIdFullDirectoryInformation, TRUE, &_glob, FALSE);
            if(STATUS_PENDING==ntstat)
                ntstat=NtWaitForSingleObject(dirh->native_handle(), FALSE, NULL);
            BOOST_AFIO_ERRHNTFN(ntstat, [dirh]{return dirh->path();});
            if(!!(wanted&metadata_flags::ino)) { stat.st_ino=ffdi->FileId.QuadPart; }
            if(!!(wanted&metadata_flags::type)) { stat.st_type=windows_nt_kernel::to_st_type(ffdi->FileAttributes, ffdi->ReparsePointTag); }
            if(!!(wanted&metadata_flags::atim)) { stat.st_atim=to_timepoint(ffdi->LastAccessTime); }
            if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=to_timepoint(ffdi->LastWriteTime); }
            if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=to_timepoint(ffdi->ChangeTime); }
            if(!!(wanted&metadata_flags::size)) { stat.st_size=ffdi->EndOfFile.QuadPart; }
            if(!!(wanted&metadata_flags::allocated)) { stat.st_allocated=ffdi->AllocationSize.QuadPart; }
            if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=to_timepoint(ffdi->CreationTime); }
            if(!!(wanted&metadata_flags::sparse)) { stat.st_sparse=!!(ffdi->FileAttributes & FILE_ATTRIBUTE_SPARSE_FILE); }
            if(!!(wanted&metadata_flags::compressed)) { stat.st_compressed=!!(ffdi->FileAttributes & FILE_ATTRIBUTE_COMPRESSED); }
            if(!!(wanted&metadata_flags::reparse_point)) { stat.st_reparse_point=!!(ffdi->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT); }
        }
        else
        {
            // No choice here, open a handle and stat it. Make sure you open with no flags, else
            // files with delete pending will refuse to open
            path_req::relative req(dirh, name(), file_flags::none);
            auto fileh=dispatcher->dofile(0, future<>(), req).second;
            auto direntry=fileh->direntry(wanted);
            wanted=wanted & direntry.metadata_ready(); // direntry() can fail to fill some entries on Win XP
            //if(!!(wanted&metadata_flags::dev)) { stat.st_dev=direntry.stat.st_dev; }
            if(!!(wanted&metadata_flags::ino)) { stat.st_ino=direntry.stat.st_ino; }
            if(!!(wanted&metadata_flags::type)) { stat.st_type=direntry.stat.st_type; }
            //if(!!(wanted&metadata_flags::perms)) { stat.st_mode=direntry.stat.st_perms; }
            if(!!(wanted&metadata_flags::nlink)) { stat.st_nlink=direntry.stat.st_nlink; }
            //if(!!(wanted&metadata_flags::uid)) { stat.st_uid=direntry.stat.st_uid; }
            //if(!!(wanted&metadata_flags::gid)) { stat.st_gid=direntry.stat.st_gid; }
            //if(!!(wanted&metadata_flags::rdev)) { stat.st_rdev=direntry.stat.st_rdev; }
            if(!!(wanted&metadata_flags::atim)) { stat.st_atim=direntry.stat.st_atim; }
            if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=direntry.stat.st_mtim; }
            if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=direntry.stat.st_ctim; }
            if(!!(wanted&metadata_flags::size)) { stat.st_size=direntry.stat.st_size; }
            if(!!(wanted&metadata_flags::allocated)) { stat.st_allocated=direntry.stat.st_allocated; }
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
            if(!!(wanted&metadata_flags::sparse)) { stat.st_sparse=direntry.stat.st_sparse; }
            if(!!(wanted&metadata_flags::compressed)) { stat.st_compressed=direntry.stat.st_compressed; }
            if(!!(wanted&metadata_flags::reparse_point)) { stat.st_reparse_point=direntry.stat.st_reparse_point; }
        }
    }
    else
#endif
    {
        BOOST_AFIO_POSIX_STAT_STRUCT s={0};
#if BOOST_AFIO_POSIX_PROVIDES_AT_PATH_FUNCTIONS
        BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTATAT((int)(size_t)dirh->native_handle(), leafname.c_str(), &s), [&]{return dirh->path()/leafname;});
#else
        BOOST_AFIO_V2_NAMESPACE::path path(dirh->path(true));
        path/=leafname;
        BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTATAT(at_fdcwd, path.c_str(), &s), [&path]{return path;});
#endif
        fill_stat_t(stat, s, wanted);
    }
    have_metadata=have_metadata | wanted;
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC handle::mapped_file::~mapped_file()
{
#ifdef WIN32
  UnmapViewOfFile(addr);
#else
  ::munmap(addr, length);
#endif
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC outcome<dispatcher_ptr> make_dispatcher(std::string uri, file_flags flagsforce, file_flags flagsmask, std::shared_ptr<thread_source> threadpool) noexcept
{
    try
    {
        if(uri!="file:///")
            return error_code(ENXIO, generic_category());
#if defined(WIN32) && !defined(USE_POSIX_ON_WIN32)
        return dispatcher_ptr(std::make_shared<detail::async_file_io_dispatcher_windows>(threadpool, flagsforce, flagsmask));
#else
        return dispatcher_ptr(std::make_shared<detail::async_file_io_dispatcher_compat>(threadpool, flagsforce, flagsmask));
#endif
    }
    catch(...)
    {
        return current_exception();
    }
}

BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC dispatcher_ptr current_dispatcher(option<dispatcher_ptr> new_dispatcher)
{
#if defined(__FreeBSD__) || defined(__APPLE__) || defined(BOOST_AFIO_USE_CXA_THREAD_ATEXIT_WORKAROUND)
    // FreeBSD and OS X haven't implemented __cxa_thread_atexit in their libc yet, so fire and forget a workaround
    static __thread dispatcher_ptr *current;
    if(!current)
    {
      current=new dispatcher_ptr();
      fprintf(stderr, "WARNING: Until libc++ fixes the lack of __cxa_thread_atexit, Boost.AFIO leaks %u bytes for this thread!\n", (unsigned) sizeof(dispatcher_ptr));
    }
    dispatcher_ptr ret(*current);
    if(new_dispatcher)
      *current=new_dispatcher.get();
    return ret;
#else
    static thread_local dispatcher_ptr current;
    dispatcher_ptr ret(current);
    if(new_dispatcher)
      current=new_dispatcher.get();
    return ret;
#endif
}

namespace utils
{
  // Stupid MSVC ...
  namespace detail { using namespace BOOST_AFIO_V2_NAMESPACE::detail; }
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6387) // MSVC sanitiser warns that GetModuleHandleA() might fail (hah!)
#endif
  std::vector<size_t> page_sizes(bool only_actually_available) noexcept
  {
      static spinlock<bool> lock;
      static std::vector<size_t> pagesizes, pagesizes_available;
      lock_guard<decltype(lock)> g(lock);
      if(pagesizes.empty())
      {
#ifdef WIN32
          typedef size_t (WINAPI *GetLargePageMinimum_t)(void);
          SYSTEM_INFO si={ 0 };
          GetSystemInfo(&si);
          pagesizes.push_back(si.dwPageSize);
          pagesizes_available.push_back(si.dwPageSize);
          GetLargePageMinimum_t GetLargePageMinimum_ = (GetLargePageMinimum_t) GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetLargePageMinimum");
          if(GetLargePageMinimum_)
          {
              windows_nt_kernel::init();
              using namespace windows_nt_kernel;
              pagesizes.push_back(GetLargePageMinimum_());
              /* Attempt to enable SeLockMemoryPrivilege */
              HANDLE token;
              if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
              {
                  auto untoken=detail::Undoer([&token]{CloseHandle(token);});
                  TOKEN_PRIVILEGES privs={1};
                  if(LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &privs.Privileges[0].Luid))
                  {
                      privs.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
                      if(AdjustTokenPrivileges(token, FALSE, &privs, 0, NULL, NULL) && GetLastError()==S_OK)
                          pagesizes_available.push_back(GetLargePageMinimum_());
                  }
              }
          }
#elif defined(__FreeBSD__)
          pagesizes.resize(32);
          int out;
          if(-1==(out=getpagesizes(pagesizes.data(), 32)))
          {
            pagesizes.clear();
            pagesizes.push_back(getpagesize());
            pagesizes_available.push_back(getpagesize());
          }
          else
          {
            pagesizes.resize(out);
            pagesizes_available=pagesizes;
          }
#elif defined(__APPLE__)
        // I can't find how to determine what the super page size is on OS X programmatically
        // It appears to be hard coded into mach/vm_statistics.h which says that it's always 2Mb
        // Therefore, we hard code 2Mb
        pagesizes.push_back(getpagesize());
        pagesizes_available.push_back(getpagesize());
        pagesizes.push_back(2*1024*1024);
        pagesizes_available.push_back(2*1024*1024);      
#elif defined(__linux__)
        pagesizes.push_back(getpagesize());
        pagesizes_available.push_back(getpagesize());
        int ih=::open("/proc/meminfo", O_RDONLY);
        if(-1!=ih)
        {
          char buffer[4096], *hugepagesize, *hugepages;
          buffer[::read(ih, buffer, sizeof(buffer)-1)]=0;
          ::close(ih);
          hugepagesize=strstr(buffer, "Hugepagesize:");
          hugepages=strstr(buffer, "HugePages_Total:");
          if(hugepagesize && hugepages)
          {
            unsigned _hugepages=0, _hugepagesize=0;
            while(*++hugepagesize!=' ');
            while(*++hugepages!=' ');
            while(*++hugepagesize==' ');
            while(*++hugepages==' ');
            sscanf(hugepagesize, "%u", &_hugepagesize);
            sscanf(hugepages, "%u", &_hugepages);
            if(_hugepagesize)
            {
              pagesizes.push_back(((size_t)_hugepagesize)*1024);
              if(_hugepages)
                pagesizes_available.push_back(((size_t)_hugepagesize)*1024);
            }
          }
        }
#else
          pagesizes.push_back(getpagesize());
          pagesizes_available.push_back(getpagesize());
#endif
      }
      return only_actually_available ? pagesizes_available : pagesizes;
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  void random_fill(char *buffer, size_t bytes)
  {
#ifdef WIN32
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      BOOST_AFIO_ERRHWIN(RtlGenRandom(buffer, (ULONG) bytes));
#else
      static spinlock<bool> lock;
      static int randomfd=-1;
      if(-1==randomfd)
      {
        lock_guard<decltype(lock)> g(lock);
        BOOST_AFIO_ERRHOS((randomfd=::open("/dev/urandom", O_RDONLY)));
      }
      BOOST_AFIO_ERRHOS(::read(randomfd, buffer, bytes));
#endif
  }


  namespace detail
  {
    large_page_allocation allocate_large_pages(size_t bytes)
    {
      large_page_allocation ret(calculate_large_page_allocation(bytes));
#ifdef WIN32
      DWORD type=MEM_COMMIT|MEM_RESERVE;
      if(ret.page_size_used>65536)
        type|=MEM_LARGE_PAGES;
      if(!(ret.p=VirtualAlloc(nullptr, ret.actual_size, type, PAGE_READWRITE)))
      {
        if(ERROR_NOT_ENOUGH_MEMORY==GetLastError())
          if((ret.p=VirtualAlloc(nullptr, ret.actual_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)))
            return ret;
        BOOST_AFIO_ERRHWIN(0);
      }
#ifndef NDEBUG
      else if(ret.page_size_used>65536)
        std::cout << "afio: Large page allocation successful" << std::endl;
#endif
#else
      int flags=MAP_SHARED|MAP_ANON;
      if(ret.page_size_used>65536)
      {
#ifdef MAP_HUGETLB
        flags|=MAP_HUGETLB;
#endif
#ifdef MAP_ALIGNED_SUPER
        flags|=MAP_ALIGNED_SUPER;
#endif
#ifdef VM_FLAGS_SUPERPAGE_SIZE_ANY
        flags|=VM_FLAGS_SUPERPAGE_SIZE_ANY;
#endif
      }
      if(!(ret.p=mmap(nullptr, ret.actual_size, PROT_WRITE, flags, -1, 0)))
      {
        if(ENOMEM==errno)
          if((ret.p=mmap(nullptr, ret.actual_size, PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0)))
            return ret;
        BOOST_AFIO_ERRHOS(-1);
      }
#ifndef NDEBUG
      else if(ret.page_size_used>65536)
        std::cout << "afio: Large page allocation successful" << std::endl;
#endif
#endif
      return ret;
    }
    void deallocate_large_pages(void *p, size_t bytes)
    {
#ifdef WIN32
      BOOST_AFIO_ERRHWIN(VirtualFree(p, 0, MEM_RELEASE));
#else
      BOOST_AFIO_ERRHOS(munmap(p, bytes));
#endif
    }
  }

}


BOOST_AFIO_V2_NAMESPACE_END

#ifdef _MSC_VER
#pragma warning(pop)
#endif

