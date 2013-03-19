/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_DEPRECATE(a)

#include "../include/async_file_io.hpp"
#include "boost/smart_ptr/detail/spinlock.hpp"
#include "../../NiallsCPP11Utilities/ErrorHandling.hpp"
#include <atomic>
#include <mutex>

#include <fcntl.h>
#ifdef WIN32
#include <io.h>
#define posix_open _open
#define posix_close _close
#define posix_read _read
#define posix_write _write
#else
#define posix_open open
#define posix_close close
#define posix_read read
#define posix_write write
#endif

namespace triplegit { namespace async_io {

// libstdc++ doesn't come with std::lock_guard
#define lock_guard boost::lock_guard


thread_pool &process_threadpool()
{
	static thread_pool ret(8);
	return ret;
}

namespace detail {
	class async_io_handle_posix : public async_io_handle
	{
		int fd;
	public:
		async_io_handle_posix(async_file_io_dispatcher_base *parent, int _fd) : async_io_handle(parent), fd(_fd)
		{
			ERRHOS(fd);
			parent->int_add_io_handle((void *)(size_t)fd, shared_from_this());
		}
		~async_io_handle_posix()
		{
			parent->int_del_io_handle((void *)(size_t)fd);
			if(fd>=0)
			{
				close(fd);
				fd=-1;
			}
		}
	};

	struct async_file_io_dispatcher_base_p
	{
		thread_pool &pool;
		file_flags flagsforce, flagsmask;

		typedef boost::detail::spinlock opslock_t;
		opslock_t fdslock; std::unordered_map<void *, std::weak_ptr<async_io_handle>> fds;
		opslock_t opslock; size_t monotoniccount; std::unordered_map<size_t, shared_future<async_io::async_io_handle>> ops;

		async_file_io_dispatcher_base_p(thread_pool &_pool, file_flags _flagsforce, file_flags _flagsmask) : pool(_pool),
			flagsforce(_flagsforce), flagsmask(_flagsmask), monotoniccount(0) { }
	};
	class async_file_io_dispatcher_compat;
	class async_file_io_dispatcher_windows;
	class async_file_io_dispatcher_linux;
	class async_file_io_dispatcher_qnx;
}

async_file_io_dispatcher_base::async_file_io_dispatcher_base(thread_pool &threadpool, file_flags flagsforce, file_flags flagsmask) : p(new detail::async_file_io_dispatcher_base_p(threadpool, flagsforce, flagsmask))
{
}

async_file_io_dispatcher_base::~async_file_io_dispatcher_base()
{
	delete p;
}

void async_file_io_dispatcher_base::int_add_io_handle(void *key, std::shared_ptr<detail::async_io_handle> h)
{
	lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> lockh(p->fdslock);
	p->fds.insert(make_pair(key, std::weak_ptr<detail::async_io_handle>(h)));
}

void async_file_io_dispatcher_base::int_del_io_handle(void *key)
{
	lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> lockh(p->fdslock);
	p->fds.erase(key);
}

thread_pool &async_file_io_dispatcher_base::threadpool() const
{
	return p->pool;
}

file_flags async_file_io_dispatcher_base::fileflags(file_flags flags) const
{
	return (flags&p->flagsmask)|p->flagsforce;
}

size_t async_file_io_dispatcher_base::wait_queue_depth() const
{
	lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	return p->ops.size();
}

shared_future<async_io_handle> async_file_io_dispatcher_base::add_async_op(std::function<async_io_handle (size_t)> _op)
{
	lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	size_t thisid=++p->monotoniccount;
	auto op(std::bind(_op, thisid));
	auto ret(threadpool().enqueue(op).share());
	p->ops.insert(std::make_pair(thisid, ret));
	return ret;
}


namespace detail {
	class async_file_io_dispatcher_compat : public async_file_io_dispatcher_base
	{
		async_io::async_io_handle doopen(size_t id, std::string path, file_flags _flags)
		{
			int flags=0;
			if(file_flags::Read==(_flags & file_flags::Read) && file_flags::Write==(_flags & file_flags::Write)) flags|=O_RDWR;
			else if(file_flags::Read==(_flags & file_flags::Read)) flags|=O_RDONLY;
			else if(file_flags::Write==(_flags & file_flags::Write)) flags|=O_WRONLY;
			if(file_flags::Append==(_flags & file_flags::Append)) flags|=O_APPEND;
			if(file_flags::Truncate==(_flags & file_flags::Truncate)) flags|=O_TRUNC;
			if(file_flags::CreateOnlyIfNotExist==(_flags & file_flags::CreateOnlyIfNotExist)) flags|=O_EXCL|O_CREAT;
			else if(file_flags::Create==(_flags & file_flags::Create)) flags|=O_CREAT;
#ifdef O_DIRECT
			if(file_flags::OSDirect==(_flags & file_flags::OSDirect)) flags|=O_DIRECT;
#endif
#ifdef OS_SYNC
			if(file_flags::OSSync==(_flags & file_flags::OSSync)) flags|=O_SYNC;
#endif
			return async_io::async_io_handle(new async_io_handle_posix(this, posix_open(path.c_str(), flags)));
		}

	public:
		async_file_io_dispatcher_compat(thread_pool &threadpool, file_flags flagsforce, file_flags flagsmask) : async_file_io_dispatcher_base(threadpool, flagsforce, flagsmask)
		{
		}

		virtual shared_future<async_io::async_io_handle> dir(const std::string &path, file_flags flags=file_flags::None)
		{
			return add_async_op(std::bind(&async_file_io_dispatcher_compat::doopen, this, std::placeholders::_1, std::string(path), fileflags));
		}
		virtual shared_future<async_io::async_io_handle> dir(shared_future<async_io::async_io_handle> first, const std::string &path, file_flags flags=file_flags::None)
		{
		}
		virtual shared_future<async_io::async_io_handle> file(const std::string &path, file_flags flags=file_flags::None)
		{
		}
		virtual shared_future<async_io::async_io_handle> file(shared_future<async_io::async_io_handle> first, const std::string &path, file_flags flags=file_flags::None)
		{
		}
		virtual shared_future<async_io::async_io_handle> sync(shared_future<async_io::async_io_handle> h, file_flags flags=file_flags::None)
		{
		}
		virtual shared_future<async_io::async_io_handle> close(shared_future<async_io::async_io_handle> h, file_flags flags=file_flags::None)
		{
		}
	};
}

std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool, file_flags flagsforce, file_flags flagsmask)
{
#ifdef WIN32
	return std::make_shared<detail::async_file_io_dispatcher_compat>(threadpool, flagsforce, flagsmask);
#else
	return std::make_shared<detail::async_file_io_dispatcher_compat>(threadpool, flagsforce, flagsmask);
#endif
}

} } // namespace
