#include "../../../boost/afio/afio.hpp"
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700)
#include <deque>
#include <regex>
#include <chrono>
#include <mutex>
#include <future>
#include <initializer_list>
#include "boost/exception/diagnostic_information.hpp"
#include "../../../boost/afio/detail/Aligned_Allocator.hpp"
#endif

/* My Intel Core i7 3770K running Windows 8 x64 with 7200rpm drive, using
Sysinternals RAMMap to clear disc cache (http://technet.microsoft.com/en-us/sysinternals/ff700229.aspx)

Warm cache:
91 files matched out of 36422 files which was 6108537728 bytes.
The search took 2.79223 seconds which was 13044 files per second or 2086.34 Mb/sec.

Cold cache:
91 files matched out of 36422 files which was 6108537728 bytes.
The search took 388.74 seconds which was 93.6925 files per second or 14.9857 Mb/sec.

Warm cache mmaps:
91 files matched out of 36422 files which was 6109258426 bytes.
The search took 1.54928 seconds which was 23508.9 files per second or 3760.6 Mb/sec.

Cold cache mmaps:
91 files matched out of 36422 files which was 6109258426 bytes.
The search took 242.76 seconds which was 150.033 files per second or 24 Mb/sec.
*/

#define USE_MMAPS

#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) && !(defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */)
//[find_in_files_afio
using namespace boost::afio;

// Often it's easiest for a lot of nesting callbacks to carry state via a this pointer
class find_in_files
{
public:
	std::promise<int> finished;
	std::regex regexpr; // The precompiled regular expression
	std::shared_ptr<async_file_io_dispatcher_base> dispatcher;
	recursive_mutex opslock;
	std::deque<async_io_op> ops; // For exception gathering
	std::atomic<size_t> bytesread, filesread, filesmatched, scheduled, completed;
	std::vector<std::pair<std::filesystem::path, size_t>> filepaths;

	// Signals finish once all scheduled ops have completed
	void docompleted(size_t inc)
	{
		size_t c=(completed+=inc);
		if(c==scheduled)
			finished.set_value(0);
	};
	// Adds ops to the list of scheduled
	void doscheduled(std::initializer_list<async_io_op> list)
	{
		scheduled+=list.size();
		//boost::lock_guard<decltype(opslock)> lock(opslock);
		//ops.insert(ops.end(), list.begin(), list.end());
	}
	void doscheduled(std::vector<async_io_op> list)
	{
		scheduled+=list.size();
		//boost::lock_guard<decltype(opslock)> lock(opslock);
		//ops.insert(ops.end(), list.begin(), list.end());
	}
	void dosearch(std::shared_ptr<async_io_handle> h, const char *buffer, size_t length)
	{
		// Search the buffer for the regular expression
		if(std::regex_search(buffer, regexpr))
		{
#pragma omp critical
			{
				std::cout << h->path() << std::endl;
			}
			++filesmatched;
		}
		++filesread;
		bytesread+=length;
	}
	// A file searching completion, called when each file read completes
	std::pair<bool, std::shared_ptr<async_io_handle>> file_read(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::shared_ptr<std::vector<char, detail::aligned_allocator<char, 4096, false>>> _buffer, size_t length)
	{
		//std::cout << "R " << h->path() << std::endl;
		char *buffer=_buffer->data();
		buffer[length]=0;
		dosearch(h, buffer, length);
		docompleted(2);
		// Throw away the buffer now rather than later to keep memory consumption down
		_buffer->clear();
		// Schedule an immediate close rather than lazy close to keep file handle count down
		//auto close=dispatcher->close(dispatcher->op_from_scheduled_id(id));
		return std::make_pair(true, h);
	}
	// A file reading completion, called when each file open completes
	std::pair<bool, std::shared_ptr<async_io_handle>> file_opened(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, size_t length)
	{
		//std::cout << "F " << h->path() << std::endl;
#ifdef USE_MMAPS
		if(!!(h->flags() & file_flags::OSMMap))
		{
			dosearch(h, (const char *) h->try_mapfile(), length);
		}
		else
#endif
		{
			// Allocate a sufficient 4Kb aligned buffer
			size_t _length=(4095+length)&~4095;
			auto buffer=std::make_shared<std::vector<char, detail::aligned_allocator<char, 4096, false>>>(_length+1);
			// Schedule a read of the file
			auto read=dispatcher->read(make_async_data_op_req(dispatcher->op_from_scheduled_id(id), buffer->data(), _length, 0));
			auto read_done=dispatcher->completion(read, std::make_pair(async_op_flags::None/*regex search might be slow*/, std::bind(&find_in_files::file_read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, buffer, length)));
			doscheduled({ read, read_done });
		}
		docompleted(2);
		return std::make_pair(true, h);
	}
	// An enumeration parsing completion, called when each directory enumeration completes
	std::pair<bool, std::shared_ptr<async_io_handle>> dir_enumerated(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::shared_ptr<future<std::pair<std::vector<directory_entry>, bool>>> listing)
	{
		async_io_op lastdir, thisop(dispatcher->op_from_scheduled_id(id));
		// Get the entries from the ready future
		std::vector<directory_entry> entries(std::move(listing->get().first));
		//std::cout << "E " << h->path() << std::endl;
		// For each of the directories schedule an open and enumeration
#if 0
		{
			std::vector<async_path_op_req> dir_reqs; dir_reqs.reserve(entries.size());
			for(auto &entry : entries)
			{
				if((entry.st_type()&S_IFDIR)==S_IFDIR)
				{
					dir_reqs.push_back(async_path_op_req(thisop, h->path()/entry.name()));
				}
			}
			if(!dir_reqs.empty())
			{
				std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> dir_openedfs(dir_reqs.size(), std::make_pair(async_op_flags::None, std::bind(&find_in_files::dir_opened, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
				auto dir_opens=dispatcher->dir(dir_reqs);
				doscheduled(dir_opens);
				auto dir_openeds=dispatcher->completion(dir_opens, dir_openedfs);
				doscheduled(dir_openeds);
				// Hold back some of the concurrency
				lastdir=dir_openeds.back();
			}
		}
#else
		// The Windows NT kernel filing system driver gets upset with too much concurrency
		// when used with OSDirect so throttle directory enumerations to enforce some depth first traversal.
		{
			auto dir_openedf=std::make_pair(async_op_flags::None, std::bind(&find_in_files::dir_opened, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
			for(auto &entry : entries)
			{
				if((entry.st_type()&S_IFDIR)==S_IFDIR)
				{
					auto dir_open=dispatcher->dir(async_path_op_req(lastdir, h->path()/entry.name()));
					auto dir_opened=dispatcher->completion(dir_open, dir_openedf);
					doscheduled({ dir_open, dir_opened });
					lastdir=dir_opened;
				}
			}
		}
#endif

		// For each of the files schedule an open and search
#if 0
		{
			std::vector<async_path_op_req> file_reqs; file_reqs.reserve(entries.size());
			std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> file_openedfs; file_openedfs.reserve(entries.size());
			for(auto &entry : entries)
			{
				if((entry.st_type()&S_IFREG)==S_IFREG)
				{
					size_t length=(size_t)entry.st_size();
					if(length)
					{
						file_flags flags=file_flags::Read;
#ifdef USE_MMAPS
						if(length>16384) flags=flags|file_flags::OSMMap;
#endif
						file_reqs.push_back(async_path_op_req(lastdir, h->path()/entry.name(), flags));
						file_openedfs.push_back(std::make_pair(async_op_flags::None, std::bind(&find_in_files::file_opened, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, length)));
					}
				}
			}
			auto file_opens=dispatcher->file(file_reqs);
			doscheduled(file_opens);
			auto file_openeds=dispatcher->completion(file_opens, file_openedfs);
			doscheduled(file_openeds);
		}
#else
		{
			for(auto &entry : entries)
			{
				if((entry.st_type()&S_IFREG)==S_IFREG)
				{
					size_t length=(size_t)entry.st_size();
					if(length)
					{
						file_flags flags=file_flags::Read;
#ifdef USE_MMAPS
						if(length>16384) flags=flags|file_flags::OSMMap;
#endif
						auto file_open=dispatcher->file(async_path_op_req(lastdir, h->path()/entry.name(), flags));
						auto file_opened=dispatcher->completion(file_open, std::make_pair(async_op_flags::None, std::bind(&find_in_files::file_opened, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, length)));
						doscheduled({ file_open, file_opened });
						lastdir=file_opened;
					}
				}
			}
		}
#endif
		docompleted(2);
		return std::make_pair(true, h);
	}
	// A directory enumerating completion, called once per directory open in the tree
	std::pair<bool, std::shared_ptr<async_io_handle>> dir_opened(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e)
	{
		//std::cout << "D " << h->path() << std::endl;
		// Now we have an open directory handle, schedule an enumeration
		auto enumeration=dispatcher->enumerate(async_enumerate_op_req(dispatcher->op_from_scheduled_id(id), 1000));
		auto listing=std::make_shared<future<std::pair<std::vector<directory_entry>, bool>>>(std::move(enumeration.first));
		auto enumeration_done=dispatcher->completion(enumeration.second, make_pair(async_op_flags::None, std::bind(&find_in_files::dir_enumerated, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, listing)));
		doscheduled({enumeration.second, enumeration_done});
		docompleted(2);
		// Complete only if not the cur dir opened
		return std::make_pair(true, h);
	};
	void dowait()
	{
		// Prepare finished
		auto finished_waiter=finished.get_future();
#if 0
		// Wait till done, retrieving any exceptions as they come in to keep memory consumption down
		std::future_status status;
		do
		{
			status=finished_waiter.wait_for(std::chrono::milliseconds(1000));
			std::cout << "\nDispatcher has " << dispatcher->count() << " fds open and " << dispatcher->wait_queue_depth() << " ops outstanding." << std::endl;
			std::vector<async_io_op> batch; batch.reserve(10000);
			{
				boost::lock_guard<decltype(opslock)> lock(opslock);
				while(status==std::future_status::timeout ? (ops.size()>5000) : !ops.empty())
				{
					batch.push_back(std::move(ops.front()));
					ops.pop_front();
				}
			}
			// Retrieve any exceptions
			std::cout << "Processed " << batch.size() << " ops for exception state." << std::endl;
			if(!batch.empty())
				when_all(batch.begin(), batch.end()).wait();
		} while(status==std::future_status::timeout);
#else
		finished_waiter.wait();
#endif
	}
	// Constructor, which starts the ball rolling
	find_in_files(const char *_regexpr) : regexpr(_regexpr),
		// Create an AFIO dispatcher that bypasses any filing system buffers
		dispatcher(make_async_file_io_dispatcher(process_threadpool(), file_flags::WillBeSequentiallyAccessed/*|file_flags::OSDirect*/)),
		bytesread(0), filesread(0), filesmatched(0), scheduled(0), completed(0)
	{
#if 0
		std::cout << "Attach profiler, and press return to continue." << std::endl;
		getchar();
#endif
		filepaths.reserve(50000);

		// Schedule the recursive enumeration of the current directory
		std::cout << "\n\nStarting directory enumerations ..." << std::endl;
		auto cur_dir=dispatcher->dir(async_path_op_req(""));
		auto cur_dir_opened=dispatcher->completion(cur_dir, std::make_pair(async_op_flags::None, std::bind(&find_in_files::dir_opened, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
		doscheduled({cur_dir, cur_dir_opened});
		dowait();

#if 0
		// Reset the waiter, and issue the searches
		finished=std::promise<int>();
		std::cout << "\n\nStarting searches of " << filepaths.size() << " files ..." << std::endl;
		for(auto &filepath : filepaths)
		{
			auto cur=dispatcher->file(async_path_op_req(filepath.first, file_flags::Read));
			auto cur_opened=dispatcher->completion(cur, std::make_pair(async_op_flags::None, std::bind(&find_in_files::file_opened, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, filepath.second)));
			doscheduled({cur, cur_opened});
		}
		dowait();
#endif
#if 0
		std::cout << "Stop profiler, and press return to continue." << std::endl;
		getchar();
#endif
	}
};

int main(int argc, const char *argv[])
{
	using std::placeholders::_1; using std::placeholders::_2; using std::placeholders::_3;
	using namespace boost::afio;
	typedef chrono::duration<double, ratio<1>> secs_type;
	if(argc<2)
	{
		std::cerr << "ERROR: Specify a regular expression to search all files in the current directory." << std::endl;
		return 1;
	}
	// Prime SpeedStep
    auto begin=chrono::high_resolution_clock::now();
    while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<1);
	try
	{
	    begin=chrono::high_resolution_clock::now();
		find_in_files finder(argv[1]);
	    auto end=chrono::high_resolution_clock::now();
        auto diff=chrono::duration_cast<secs_type>(end-begin);
		std::cout << "\n" << finder.filesmatched << " files matched out of " << finder.filesread << " files which was " << finder.bytesread << " bytes." << std::endl;
		std::cout << "The search took " << diff.count() << " seconds which was " << finder.filesread/diff.count() << " files per second or " << (finder.bytesread/diff.count()/1024/1024) << " Mb/sec." << std::endl;
	}
	catch(...)
	{
		std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
		return 1;
	}
	return 0;
}
//]
#else
int main(void) { return 0; }
#endif
