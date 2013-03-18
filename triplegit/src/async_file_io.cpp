/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#include "../include/async_file_io.hpp"
#include "boost/smart_ptr/detail/spinlock.hpp"
#include <mutex>

#include <fcntl.h>
#ifdef WIN32
#include <io.h>
#define open _open
#define close _close
#else
#endif

namespace triplegit { namespace async_io {

thread_pool &process_threadpool()
{
	static thread_pool ret(8);
	return ret;
}

namespace detail {
	class async_io_handle
	{
		async_io_handle() { }
	public:
		virtual ~async_io_handle();
		//! Asynchonrously synchronises this open item to storage
		virtual shared_future<async_io_handle> sync()=0;
	};

	struct async_file_io_dispatcher_base_p
	{
		thread_pool &pool;
		file_flags flagsforce, flagsmask;

		typedef boost::detail::spinlock opslock_t;
		opslock_t opslock;
		size_t monotoniccount;
		std::unordered_map<size_t, shared_future<async_io_handle>> ops;

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
	std::lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	return p->ops.size();
}

namespace detail {
	class async_file_io_dispatcher_compat : public async_file_io_dispatcher_base
	{
		future<async_io_handle> doopen(const std::string &path, file_flags _flags)
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
			async_io_handle_posix ret(open(path.c_str(), flags));
			std::lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
			remove my id from ops?;
			return ret;
		}

	public:
		async_file_io_dispatcher_compat(thread_pool &threadpool, file_flags flagsforce, file_flags flagsmask) : async_file_io_dispatcher_base(threadpool, flagsforce, flagsmask)
		{
		}

		virtual shared_future<async_io_handle> dir(const std::string &path, file_flags flags=file_flags::None)
		{
			auto op(threadpool().enqueue(std::bind(*this, &doopen, path, fileflags(flags))));
			return int_addop(std::move(op));
		}
		virtual shared_future<async_io_handle> dir(shared_future<async_io_handle> first, const std::string &path, file_flags flags=file_flags::None)
		{
			auto op(first.then(std::bind(*this, &doopen, path, fileflags(flags))));
			return int_addop(std::move(op));
		}
		virtual shared_future<async_io_handle> file(const std::string &path, file_flags flags=file_flags::None)
		{
		}
		virtual shared_future<async_io_handle> file(shared_future<async_io_handle> first, const std::string &path, file_flags flags=file_flags::None)
		{
		}
		virtual shared_future<async_io_handle> sync(shared_future<async_io_handle> h, file_flags flags=file_flags::None)
		{
		}
		virtual shared_future<async_io_handle> close(shared_future<async_io_handle> h, file_flags flags=file_flags::None)
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
