/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

//#define USE_POSIX_ON_WIN32 // Useful for testing

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_DEPRECATE(a)

#include "../include/async_file_io.hpp"
#include "boost/smart_ptr/detail/spinlock.hpp"
#include "../../NiallsCPP11Utilities/ErrorHandling.hpp"
#include <mutex>

#include <fcntl.h>
#include <sys/stat.h>
#ifdef WIN32
#include <Windows.h>
// We also compile the posix compat layer for catching silly compile errors for POSIX
#include <io.h>
#include <direct.h>
#define posix_mkdir(path, mode) _wmkdir(path)
#define posix_rmdir _wrmdir
#define posix_stat _wstat64
#define stat _stat64
#define S_ISREG(m) ((m) & _S_IFREG)
#define S_ISDIR(m) ((m) & _S_IFDIR)
#define posix_open _wopen
#define posix_close _close
#define posix_unlink _wunlink
#define posix_read _read
#define posix_write _write
#define posix_fsync _commit
#else
#define posix_mkdir mkdir
#define posix_rmdir rmdir
#define posix_stat stat
#define posix_open open
#define posix_close ::close
#define posix_unlink unlink
#define posix_read read
#define posix_write write
#define posix_fsync fsync
#endif

namespace triplegit { namespace async_io {

// libstdc++ doesn't come with std::lock_guard
#define lock_guard boost::lock_guard


thread_pool &process_threadpool()
{
	// This is basically how many file i/o operations can occur at once
	// Obviously the kernel also has a limit
	static thread_pool ret(64);
	return ret;
}

namespace detail {
#if defined(WIN32)
	struct async_io_handle_windows : public async_io_handle
	{
		std::shared_ptr<async_file_io_dispatcher_base> parent;
		std::unique_ptr<boost::asio::windows::random_access_handle> h;
		void *myid;
		bool has_been_added, autoflush;

		async_io_handle_windows(std::shared_ptr<async_file_io_dispatcher_base> _parent, const std::filesystem::path &path) : async_io_handle(_parent.get(), path), parent(_parent), myid(nullptr), has_been_added(false), autoflush(false) { }
		async_io_handle_windows(std::shared_ptr<async_file_io_dispatcher_base> _parent, const std::filesystem::path &path, bool _autoflush, HANDLE _h) : async_io_handle(_parent.get(), path), parent(_parent), h(new boost::asio::windows::random_access_handle(process_threadpool().io_service(), _h)), myid(_h), has_been_added(false), autoflush(_autoflush) { }
		// You can't use shared_from_this() in a constructor so ...
		void do_add_io_handle_to_parent()
		{
			if(h)
			{
				parent->int_add_io_handle(myid, shared_from_this());
				has_been_added=true;
			}
		}
		~async_io_handle_windows()
		{
			if(has_been_added)
				parent->int_del_io_handle(myid);
			if(h)
			{
				if(autoflush)
					ERRHWINFN(FlushFileBuffers(h->native_handle()), path());
				h->close();
			}
		}
	};
#endif
	struct async_io_handle_posix : public async_io_handle
	{
		std::shared_ptr<async_file_io_dispatcher_base> parent;
		int fd;
		bool has_been_added, autoflush, has_ever_been_fsynced;

		async_io_handle_posix(std::shared_ptr<async_file_io_dispatcher_base> _parent, const std::filesystem::path &path, bool _autoflush, int _fd) : async_io_handle(_parent.get(), path), parent(_parent), fd(_fd), has_been_added(false), autoflush(_autoflush),has_ever_been_fsynced(false)
		{
			if(fd!=-999)
				ERRHOSFN(fd, path);
		}
		// You can't use shared_from_this() in a constructor so ...
		void do_add_io_handle_to_parent()
		{
			parent->int_add_io_handle((void *)(size_t)fd, shared_from_this());
			has_been_added=true;
		}
		~async_io_handle_posix()
		{
			if(has_been_added)
				parent->int_del_io_handle((void *)(size_t)fd);
			if(fd>=0)
			{
				// Flush synchronously here? I guess ...
				if(autoflush)
					ERRHOSFN(posix_fsync(fd), path());
				ERRHOSFN(posix_close(fd), path());
				fd=-1;
			}
		}
	};

	struct async_file_io_dispatcher_op
	{
		shared_future<std::shared_ptr<detail::async_io_handle>> h;
		typedef std::pair<size_t, std::function<std::shared_ptr<detail::async_io_handle> (std::shared_ptr<detail::async_io_handle>)>> completion_t;
		std::vector<completion_t> completions;
		async_file_io_dispatcher_op(shared_future<std::shared_ptr<detail::async_io_handle>> _h) : h(_h) { }
	};
	struct async_file_io_dispatcher_base_p
	{
		thread_pool &pool;
		file_flags flagsforce, flagsmask;

		typedef boost::detail::spinlock opslock_t;
		opslock_t fdslock; std::unordered_map<void *, std::weak_ptr<async_io_handle>> fds;
		opslock_t opslock; size_t monotoniccount; std::unordered_map<size_t, async_file_io_dispatcher_op> ops;

		async_file_io_dispatcher_base_p(thread_pool &_pool, file_flags _flagsforce, file_flags _flagsmask) : pool(_pool),
			flagsforce(_flagsforce), flagsmask(_flagsmask), monotoniccount(0)
		{
			// Boost's spinlock is so lightweight it has no constructor ...
			fdslock.unlock();
			opslock.unlock();
		}
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
	return (flags&~p->flagsmask)|p->flagsforce;
}

size_t async_file_io_dispatcher_base::wait_queue_depth() const
{
	lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	return p->ops.size();
}

size_t async_file_io_dispatcher_base::count() const
{
	lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> lockh(p->fdslock);
	return p->fds.size();
}

// Called in unknown thread
std::shared_ptr<detail::async_io_handle> async_file_io_dispatcher_base::invoke_user_completion(size_t id, std::shared_ptr<detail::async_io_handle> h, std::function<std::shared_ptr<detail::async_io_handle> (size_t, std::shared_ptr<detail::async_io_handle>)> callback)
{
	return callback(id, h);
}

std::vector<async_io_op> async_file_io_dispatcher_base::completion(const std::vector<async_io_op> &ops, const std::vector<std::function<std::shared_ptr<detail::async_io_handle> (size_t, std::shared_ptr<detail::async_io_handle>)>> &callbacks)
{
	std::vector<async_io_op> ret;
	ret.reserve(ops.size());
	std::vector<async_io_op>::const_iterator i;
	std::vector<std::function<std::shared_ptr<detail::async_io_handle> (size_t, std::shared_ptr<detail::async_io_handle>)>>::const_iterator c;
	lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	for(i=ops.begin(), c=callbacks.begin(); i!=ops.end() && c!=callbacks.end(); ++i, ++c)
		ret.push_back(chain_async_op(*i, &async_file_io_dispatcher_base::invoke_user_completion, *c));
	return ret;
}

// Called in unknown thread
template<class F, class... Args> std::shared_ptr<detail::async_io_handle> async_file_io_dispatcher_base::invoke_async_op_completions(size_t id, std::shared_ptr<detail::async_io_handle> h, std::shared_ptr<detail::async_io_handle> (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, Args...), Args... args)
{
	std::shared_ptr<detail::async_io_handle> ret((static_cast<F *>(this)->*f)(id, h, args...));
	// Find me in ops, remove my completions and delete me from extant ops
	lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	typename std::unordered_map<size_t, detail::async_file_io_dispatcher_op>::iterator it(p->ops.find(id));
	if(p->ops.end()==it)
		throw std::runtime_error("Failed to find this operation in list of currently executing operations");
	std::vector<typename detail::async_file_io_dispatcher_op::completion_t> completions(std::move(it->second.completions));
	p->ops.erase(it);
	for(auto &c : completions)
	{
		// Enqueue each completion
		it=p->ops.find(c.first);
		if(p->ops.end()==it)
			throw std::runtime_error("Failed to find this completion operation in list of currently executing operations");
		it->second.h=threadpool().enqueue(std::bind(c.second, ret));
	}
	return ret;
}

// You MUST hold opslock before entry!
template<class F, class... Args> async_io_op async_file_io_dispatcher_base::chain_async_op(const async_io_op &precondition, std::shared_ptr<detail::async_io_handle> (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, Args...), Args... args)
{
	size_t thisid=0;
#ifndef NDEBUG
	if(p->opslock.try_lock())
	{
		assert(0);
		std::terminate();
	}
#endif
	while(!(thisid=++p->monotoniccount));
	// Wrap supplied implementation routine with a completion dispatcher
	auto wrapperf=&async_file_io_dispatcher_base::invoke_async_op_completions<F, Args...>;
	// Bind supplied implementation routine to this, unique id and any args they passed
	typename detail::async_file_io_dispatcher_op::completion_t boundf(std::make_pair(thisid, std::bind(wrapperf, this, thisid, std::placeholders::_1, f, args...)));
	// Make a new async_io_op ready for returning
	async_io_op ret(shared_from_this(), thisid);
	bool done=false;
	if(precondition.id)
	{
		// If still in flight, chain boundf to be executed when precondition completes
		auto dep(p->ops.find(precondition.id));
		if(p->ops.end()!=dep)
		{
			dep->second.completions.push_back(boundf);
			done=true;
		}
	}
	if(!done)
	{
		// Bind input handle now and queue immediately to next available thread worker
		std::shared_ptr<detail::async_io_handle> h;
		// Boost's shared_future has get() as non-const which is weird, because it doesn't
		// delete the data after retrieval.
		if(precondition.h.valid())
			h=const_cast<shared_future<std::shared_ptr<detail::async_io_handle>> &>(precondition.h).get();
		ret.h=threadpool().enqueue(std::bind(boundf.second, h)).share();
	}
	p->ops.insert(std::make_pair(thisid, ret.h));
	return ret;
}
template<class F, class T> std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(const std::vector<T> &container, std::shared_ptr<detail::async_io_handle> (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, T))
{
	std::vector<async_io_op> ret;
	ret.reserve(container.size());
	lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	for(auto &i : container)
		ret.push_back(chain_async_op(i, f, i));
	return ret;
}
template<class F> std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(const std::vector<async_path_op_req> &container, std::shared_ptr<detail::async_io_handle> (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, async_path_op_req))
{
	std::vector<async_io_op> ret;
	ret.reserve(container.size());
	lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	for(auto &i : container)
		ret.push_back(chain_async_op(i.precondition, f, i));
	return ret;
}


namespace detail {
#if defined(WIN32)
	class async_file_io_dispatcher_windows : public async_file_io_dispatcher_base
	{
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> dodir(size_t id, std::shared_ptr<detail::async_io_handle> _, async_path_op_req req)
		{
			BOOL ret=0;
			req.flags=fileflags(req.flags);
			if(file_flags::Create==(req.flags & file_flags::Create))
			{
				ret=CreateDirectory(req.path.c_str(), NULL);
				if(!ret && ERROR_ALREADY_EXISTS==GetLastError())
				{
					// Ignore already exists unless we were asked otherwise
					if(file_flags::CreateOnlyIfNotExist!=(req.flags & file_flags::CreateOnlyIfNotExist))
						ret=1;
				}
				req.flags=req.flags&~(file_flags::Create|file_flags::CreateOnlyIfNotExist);
			}
			DWORD attr=GetFileAttributes(req.path.c_str());
			if(INVALID_FILE_ATTRIBUTES!=attr && !(attr & FILE_ATTRIBUTE_DIRECTORY))
				throw std::runtime_error("Not a directory");
			if(file_flags::Read==(req.flags & file_flags::Read))
				return dofile(id, _, req);
			else
			{
				// Create empty handle so
				auto ret=std::shared_ptr<detail::async_io_handle>(new async_io_handle_windows(shared_from_this(), req.path));
				return ret;
			}
		}
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> dormdir(size_t id, std::shared_ptr<detail::async_io_handle> _, async_path_op_req req)
		{
			req.flags=fileflags(req.flags);
			ERRHWINFN(RemoveDirectory(req.path.c_str()), req.path);
			auto ret=std::shared_ptr<detail::async_io_handle>(new async_io_handle_windows(shared_from_this(), req.path));
			return ret;
		}
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> dofile(size_t id, std::shared_ptr<detail::async_io_handle>, async_path_op_req req)
		{
			DWORD access=0, creation=0, flags=FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED;
			req.flags=fileflags(req.flags);
			if(file_flags::Append==(req.flags & file_flags::Append)) access|=FILE_APPEND_DATA;
			else
			{
				if(file_flags::Read==(req.flags & file_flags::Read)) access|=GENERIC_READ;
				if(file_flags::Write==(req.flags & file_flags::Write)) access|=GENERIC_WRITE;
			}
			if(file_flags::CreateOnlyIfNotExist==(req.flags & file_flags::CreateOnlyIfNotExist)) creation|=CREATE_NEW;
			else if(file_flags::Create==(req.flags & file_flags::Create)) creation|=CREATE_ALWAYS;
			else if(file_flags::Truncate==(req.flags & file_flags::Truncate)) creation|=TRUNCATE_EXISTING;
			else creation|=OPEN_EXISTING;
			if(file_flags::WillBeSequentiallyAccessed==(req.flags & file_flags::WillBeSequentiallyAccessed))
				flags|=FILE_FLAG_SEQUENTIAL_SCAN;
			if(file_flags::OSDirect==(req.flags & file_flags::OSDirect)) flags|=FILE_FLAG_NO_BUFFERING;
			if(file_flags::OSSync==(req.flags & file_flags::OSSync)) flags|=FILE_FLAG_WRITE_THROUGH;
			auto ret=std::shared_ptr<detail::async_io_handle>(new async_io_handle_windows(shared_from_this(), req.path, (file_flags::AutoFlush|file_flags::Write)==(req.flags & (file_flags::AutoFlush|file_flags::Write)),
				CreateFile(req.path.c_str(), access, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
					NULL, creation, flags, NULL)));
			static_cast<async_io_handle_windows *>(ret.get())->do_add_io_handle_to_parent();
			return ret;
		}
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> dormfile(size_t id, std::shared_ptr<detail::async_io_handle> _, async_path_op_req req)
		{
			req.flags=fileflags(req.flags);
			ERRHWINFN(DeleteFile(req.path.c_str()), req.path);
			auto ret=std::shared_ptr<detail::async_io_handle>(new async_io_handle_windows(shared_from_this(), req.path));
			return ret;
		}
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> dosync(size_t id, std::shared_ptr<detail::async_io_handle> h, async_io_op)
		{
			async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
			ERRHWINFN(FlushFileBuffers(p->h->native_handle()), p->path());
			return h;
		}
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> doclose(size_t id, std::shared_ptr<detail::async_io_handle> h, async_io_op)
		{
			async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
			// Windows doesn't provide an async fsync so do it synchronously
			if(p->autoflush)
				ERRHWINFN(FlushFileBuffers(p->h->native_handle()), p->path());
			p->h->close();
			p->h.reset();
			return h;
		}

	public:
		async_file_io_dispatcher_windows(thread_pool &threadpool, file_flags flagsforce, file_flags flagsmask) : async_file_io_dispatcher_base(threadpool, flagsforce, flagsmask)
		{
		}

		virtual std::vector<async_io_op> dir(const std::vector<async_path_op_req> &reqs)
		{
			return chain_async_ops(reqs, &async_file_io_dispatcher_windows::dodir);
		}
		virtual std::vector<async_io_op> rmdir(const std::vector<async_path_op_req> &reqs)
		{
			return chain_async_ops(reqs, &async_file_io_dispatcher_windows::dormdir);
		}
		virtual std::vector<async_io_op> file(const std::vector<async_path_op_req> &reqs)
		{
			return chain_async_ops(reqs, &async_file_io_dispatcher_windows::dofile);
		}
		virtual std::vector<async_io_op> rmfile(const std::vector<async_path_op_req> &reqs)
		{
			return chain_async_ops(reqs, &async_file_io_dispatcher_windows::dormfile);
		}
		virtual std::vector<async_io_op> sync(const std::vector<async_io_op> &ops)
		{
			return chain_async_ops(ops, &async_file_io_dispatcher_windows::dosync);
		}
		virtual std::vector<async_io_op> close(const std::vector<async_io_op> &ops)
		{
			return chain_async_ops(ops, &async_file_io_dispatcher_windows::doclose);
		}
	};
#endif
	class async_file_io_dispatcher_compat : public async_file_io_dispatcher_base
	{
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> dodir(size_t id, std::shared_ptr<detail::async_io_handle> _, async_path_op_req req)
		{
			int ret=0;
			req.flags=fileflags(req.flags);
			if(file_flags::Create==(req.flags & file_flags::Create))
			{
				ret=posix_mkdir(req.path.c_str(), 0x1f8/*770*/);
				if(-1==ret && EEXIST==errno)
				{
					// Ignore already exists unless we were asked otherwise
					if(file_flags::CreateOnlyIfNotExist!=(req.flags & file_flags::CreateOnlyIfNotExist))
						ret=0;
				}
				req.flags=req.flags&~(file_flags::Create|file_flags::CreateOnlyIfNotExist);
			}

			struct stat s={0};
			ret=posix_stat(req.path.c_str(), &s);
			if(0==ret && !S_ISDIR(s.st_mode))
				throw std::runtime_error("Not a directory");
			if(file_flags::Read==(req.flags & file_flags::Read))
				return dofile(id, _, req);
			else
			{
				// Create dummy handle so
				auto ret=std::shared_ptr<detail::async_io_handle>(new async_io_handle_posix(shared_from_this(), req.path, false, -999));
				return ret;
			}
		}
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> dormdir(size_t id, std::shared_ptr<detail::async_io_handle> _, async_path_op_req req)
		{
			req.flags=fileflags(req.flags);
			ERRHOSFN(posix_rmdir(req.path.c_str()), req.path);
			auto ret=std::shared_ptr<detail::async_io_handle>(new async_io_handle_posix(shared_from_this(), req.path, false, -999));
			return ret;
		}
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> dofile(size_t id, std::shared_ptr<detail::async_io_handle>, async_path_op_req req)
		{
			int flags=0;
			req.flags=fileflags(req.flags);
			if(file_flags::Read==(req.flags & file_flags::Read) && file_flags::Write==(req.flags & file_flags::Write)) flags|=O_RDWR;
			else if(file_flags::Read==(req.flags & file_flags::Read)) flags|=O_RDONLY;
			else if(file_flags::Write==(req.flags & file_flags::Write)) flags|=O_WRONLY;
			if(file_flags::Append==(req.flags & file_flags::Append)) flags|=O_APPEND;
			if(file_flags::Truncate==(req.flags & file_flags::Truncate)) flags|=O_TRUNC;
			if(file_flags::CreateOnlyIfNotExist==(req.flags & file_flags::CreateOnlyIfNotExist)) flags|=O_EXCL|O_CREAT;
			else if(file_flags::Create==(req.flags & file_flags::Create)) flags|=O_CREAT;
#ifdef O_DIRECT
			if(file_flags::OSDirect==(req.flags & file_flags::OSDirect)) flags|=O_DIRECT;
#endif
#ifdef OS_SYNC
			if(file_flags::OSSync==(req.flags & file_flags::OSSync)) flags|=O_SYNC;
#endif
			auto ret=std::shared_ptr<detail::async_io_handle>(new async_io_handle_posix(shared_from_this(), req.path, (file_flags::AutoFlush|file_flags::Write)==(req.flags & (file_flags::AutoFlush|file_flags::Write)),
				posix_open(req.path.c_str(), flags, 0x1b0/*660*/)));
			static_cast<async_io_handle_posix *>(ret.get())->do_add_io_handle_to_parent();
			return ret;
		}
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> dormfile(size_t id, std::shared_ptr<detail::async_io_handle> _, async_path_op_req req)
		{
			req.flags=fileflags(req.flags);
			ERRHOSFN(posix_unlink(req.path.c_str()), req.path);
			auto ret=std::shared_ptr<detail::async_io_handle>(new async_io_handle_posix(shared_from_this(), req.path, false, -999));
			return ret;
		}
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> dosync(size_t id, std::shared_ptr<detail::async_io_handle> h, async_io_op)
		{
			async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
			ERRHOSFN(posix_fsync(p->fd), p->path());
			p->has_ever_been_fsynced=true;
			return h;
		}
		// Called in unknown thread
		std::shared_ptr<detail::async_io_handle> doclose(size_t id, std::shared_ptr<detail::async_io_handle> h, async_io_op)
		{
			async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
			if(p->autoflush)
				ERRHOSFN(posix_fsync(p->fd), p->path());
			ERRHOSFN(posix_close(p->fd), p->path());
			p->fd=-1;
			return h;
		}

	public:
		async_file_io_dispatcher_compat(thread_pool &threadpool, file_flags flagsforce, file_flags flagsmask) : async_file_io_dispatcher_base(threadpool, flagsforce, flagsmask)
		{
		}


		virtual std::vector<async_io_op> dir(const std::vector<async_path_op_req> &reqs)
		{
			return chain_async_ops(reqs, &async_file_io_dispatcher_compat::dodir);
		}
		virtual std::vector<async_io_op> rmdir(const std::vector<async_path_op_req> &reqs)
		{
			return chain_async_ops(reqs, &async_file_io_dispatcher_compat::dormdir);
		}
		virtual std::vector<async_io_op> file(const std::vector<async_path_op_req> &reqs)
		{
			return chain_async_ops(reqs, &async_file_io_dispatcher_compat::dofile);
		}
		virtual std::vector<async_io_op> rmfile(const std::vector<async_path_op_req> &reqs)
		{
			return chain_async_ops(reqs, &async_file_io_dispatcher_compat::dormfile);
		}
		virtual std::vector<async_io_op> sync(const std::vector<async_io_op> &ops)
		{
			return chain_async_ops(ops, &async_file_io_dispatcher_compat::dosync);
		}
		virtual std::vector<async_io_op> close(const std::vector<async_io_op> &ops)
		{
			std::vector<async_io_op> ret;
			ret.reserve(ops.size());
			lock_guard<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
			for(auto &i : ops)
			{
				auto op(chain_async_op(i, &async_file_io_dispatcher_compat::doclose, i));
#ifdef __linux__
				// It's a real shame Linux's unsafe default forces a synchronisation here ...
				async_io_handle_posix *p=static_cast<async_io_handle_posix *>(const_cast<shared_future<std::shared_ptr<detail::async_io_handle>> &>(i.h).get().get());
				if(p->has_ever_been_fsynced)
				{
					// Need to fsync the containing directory on Linux file systems, otherwise the file doesn't
					// necessarily appear where it's supposed to
					async_path_op_req containingdir(op, p->path().parent_path(), file_flags::Read);
					auto diropenop(chain_async_op(containingdir.precondition, &async_file_io_dispatcher_compat::dofile, containingdir));
					auto syncdirop(chain_async_op(diropenop, &async_file_io_dispatcher_compat::dosync, diropenop));
					auto closedirop(chain_async_op(syncdirop, &async_file_io_dispatcher_compat::doclose, syncdirop));
					op=closedirop;
				}
#endif
				// On non-Linux file systems, closing a file guarantees the storage for its containing directory
				// will be atomically updated as soon as the file's contents reach storage. In other words,
				// if you fsync() a file before closing it, closing it auto-fsyncs its containing directory.
				ret.push_back(op);
			}
			return ret;
		}
	};
}

std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool, file_flags flagsforce, file_flags flagsmask)
{
#if defined(WIN32) && !defined(USE_POSIX_ON_WIN32)
	return std::make_shared<detail::async_file_io_dispatcher_windows>(threadpool, flagsforce, flagsmask);
#else
	return std::make_shared<detail::async_file_io_dispatcher_compat>(threadpool, flagsforce, flagsmask);
#endif
}

} } // namespace
