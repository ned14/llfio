#include <regex>
#include <chrono>
#include <mutex>
#include <initializer_list>
#include "boost/exception/diagnostic_information.hpp"
#include "../../../boost/afio/afio.hpp"

/* My Intel Core i7 3770K running Windows 8 x64 with 7200rpm drive, using
Sysinternals RAMMap to clear disc cache (http://technet.microsoft.com/en-us/sysinternals/ff700229.aspx)

Warm cache:

Cold cache:
*/

#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700)
//[find_in_files_afio
using namespace boost::afio;

// Often it's easiest for a lot of nesting callbacks to carry state via a this pointer
class find_in_files
{
public:

	std::regex regexpr; // The precompiled regular expression
	std::shared_ptr<async_file_io_dispatcher_base> dispatcher;
	recursive_mutex opslock;
	std::vector<async_io_op> ops; // For exception gathering
	std::atomic<size_t> bytesread, filesread, filesmatched, completed;
	async_io_op cur_dir_opened;

	// Signals finish once all scheduled ops have completed
	void docompleted(size_t inc)
	{
		size_t scheduled;
		{
			boost::lock_guard<decltype(opslock)> lock(opslock);
			scheduled=ops.size();
		}
		size_t c=(completed+=inc);
		if(c==scheduled)
			dispatcher->complete_async_op(cur_dir_opened.id, std::shared_ptr<async_io_handle>());
	};
	// Adds ops to the list of scheduled
	void scheduled(std::initializer_list<async_io_op> list)
	{
		boost::lock_guard<decltype(opslock)> lock(opslock);
		ops.insert(ops.end(), list.begin(), list.end());
	}
	// A file searching completion, called when each file read completes
	std::pair<bool, std::shared_ptr<async_io_handle>> file_read(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::shared_ptr<std::unique_ptr<char[]>> _buffer, size_t length)
	{
		char *buffer=_buffer->get();
		buffer[length]=0;
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
		docompleted(2);
		return std::make_pair(true, h);
	}
	// A file reading completion, called when each file open completes
	std::pair<bool, std::shared_ptr<async_io_handle>> file_opened(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, size_t length)
	{
		// Allocate a sufficient buffer, avoiding the byte clearing vector would do
		auto buffer=std::make_shared<std::unique_ptr<char[]>>(std::unique_ptr<char[]>(new char[length+1]));
		// Schedule a read of the file
		auto read=dispatcher->read(make_async_data_op_req(dispatcher->op_from_running_id(id), buffer->get(), length, 0));
		auto read_done=dispatcher->completion(read, std::make_pair(async_op_flags::ImmediateCompletion, std::bind(&find_in_files::file_read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, buffer, length)));
		scheduled({read, read_done});
		docompleted(2);
		return std::make_pair(true, h);
	}
	// An enumeration parsing completion, called when each directory enumeration completes
	std::pair<bool, std::shared_ptr<async_io_handle>> dir_enumerated(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::shared_ptr<future<std::pair<std::vector<directory_entry>, bool>>> listing)
	{
		// Get the entries from the ready future
		std::vector<directory_entry> entries(std::move(listing->get().first));
		for(auto &entry : entries)
		{
			// For each of the files and directories, schedule an open
			if((entry.st_type()&S_IFDIR)==S_IFDIR)
			{
				auto cur=dispatcher->dir(async_path_op_req(h->path()/entry.name()));
				auto cur_opened=dispatcher->completion(cur, std::make_pair(async_op_flags::ImmediateCompletion, std::bind(&find_in_files::dir_opened, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
				scheduled({cur, cur_opened});
			}
			else if((entry.st_type()&S_IFREG)==S_IFREG)
			{
				auto cur=dispatcher->file(async_path_op_req(h->path()/entry.name()));
				auto cur_opened=dispatcher->completion(cur, std::make_pair(async_op_flags::ImmediateCompletion, std::bind(&find_in_files::file_opened, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, entry.st_size())));
				scheduled({cur, cur_opened});
			}
		}
		docompleted(2);
		return std::make_pair(true, h);
	}
	// A directory enumerating lambda, called once per directory open in the tree
	std::pair<bool, std::shared_ptr<async_io_handle>> dir_opened(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e)
	{
		// Now we have an open directory handle, schedule an enumeration
		auto enumeration=dispatcher->enumerate(async_enumerate_op_req(dispatcher->op_from_running_id(id), 1000));
		auto listing=std::make_shared<future<std::pair<std::vector<directory_entry>, bool>>>(std::move(enumeration.first));
		auto enumeration_done=dispatcher->completion(enumeration.second, make_pair(async_op_flags::ImmediateCompletion, std::bind(&find_in_files::dir_enumerated, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, listing)));
		scheduled({enumeration.second, enumeration_done});
		docompleted(2);
		// Complete only if not the cur dir opened
		return std::make_pair(id!=cur_dir_opened.id, h);
	};
	// Constructor, which starts the ball rolling
	find_in_files(const char *_regexpr) : regexpr(_regexpr),
		// Create an AFIO dispatcher that bypasses any filing system buffers
		dispatcher(make_async_file_io_dispatcher(process_threadpool(), file_flags::OSDirect | file_flags::AlwaysSync)),
		bytesread(0), filesread(0), filesmatched(0), completed(0)
	{
		// Schedule the recursive enumeration of the current directory
		auto cur_dir=dispatcher->dir(async_path_op_req("."));
		cur_dir_opened=dispatcher->completion(cur_dir, std::make_pair(async_op_flags::DetachedFuture|async_op_flags::ImmediateCompletion, std::bind(&find_in_files::dir_opened, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
		scheduled({cur_dir, cur_dir_opened});
		// Wait until last of searches completes
		when_all(cur_dir_opened).wait();
		// Retrieve any exceptions
		when_all(ops.begin(), ops.end()).wait();
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
