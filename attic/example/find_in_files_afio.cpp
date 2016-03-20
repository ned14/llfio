#include "afio_pch.hpp"
#include <deque>
#include <regex>
#include <chrono>
#include <mutex>
#include <future>
#include <initializer_list>
#include "boost/exception/diagnostic_information.hpp"
#include "boost/../libs/afio/test/Aligned_Allocator.hpp"

/* My Intel Core i7 3770K running Windows 8 x64 with 7200rpm drive, using
Sysinternals RAMMap to clear disc cache (http://technet.microsoft.com/en-us/sysinternals/ff700229.aspx)

Warm cache:
92 files matched out of 36734 files which was 7031545071 bytes.
The search took 2.5173 seconds which was 14592.6 files per second or 2663.89 Mb/sec.

Cold cache:
91 files matched out of 36422 files which was 6108537728 bytes.
The search took 388.74 seconds which was 93.6925 files per second or 14.9857 Mb/sec.

Warm cache mmaps:
92 files matched out of 36734 files which was 7031400686 bytes.
The search took 1.02974 seconds which was 35673.1 files per second or 6512.01 Mb/sec.

Cold cache mmaps:
91 files matched out of 36422 files which was 6109258426 bytes.
The search took 242.76 seconds which was 150.033 files per second or 24 Mb/sec.
*/

#define USE_MMAPS

//[find_in_files_afio
using namespace boost::afio;

// Often it's easiest for a lot of nesting callbacks to carry state via a this pointer
class find_in_files
{
public:
    std::promise<int> finished;
    std::regex regexpr; // The precompiled regular expression
    dispatcher_ptr dispatcher;
    recursive_mutex opslock;
    std::deque<future<>> ops; // For exception gathering
    std::atomic<size_t> bytesread, filesread, filesmatched, scheduled, completed;
    std::vector<std::pair<boost::afio::filesystem::path, size_t>> filepaths;

    // Signals finish once all scheduled ops have completed
    void docompleted(size_t inc)
    {
        size_t c=(completed+=inc);
        if(c==scheduled)
            finished.set_value(0);
    };
    // Adds ops to the list of scheduled
    void doscheduled(std::initializer_list<future<>> list)
    {
        scheduled+=list.size();
        //boost::lock_guard<decltype(opslock)> lock(opslock);
        //ops.insert(ops.end(), list.begin(), list.end());
    }
    void doscheduled(std::vector<future<>> list)
    {
        scheduled+=list.size();
        //boost::lock_guard<decltype(opslock)> lock(opslock);
        //ops.insert(ops.end(), list.begin(), list.end());
    }
    void dosearch(handle_ptr h, const char *buffer, size_t length)
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
    std::pair<bool, handle_ptr> file_read(size_t id, 
        future<> op, std::shared_ptr<std::vector<char,
        detail::aligned_allocator<char, 4096, false>>> _buffer, size_t length)
    {
        handle_ptr h(op.get_handle());
        //std::cout << "R " << h->path() << std::endl;
        char *buffer=_buffer->data();
        buffer[length]=0;
        dosearch(h, buffer, length);
        docompleted(2);
        // Throw away the buffer now rather than later to keep memory consumption down
        _buffer->clear();
        return std::make_pair(true, h);
    }
    // A file reading completion, called when each file open completes
    std::pair<bool, handle_ptr> file_opened(size_t id, 
        future<> op, size_t length)
    {
        handle_ptr h(op.get_handle());
        //std::cout << "F " << h->path() << std::endl;
        if (length)
        {
#ifdef USE_MMAPS
          auto map(h->map_file());
          if (map)
          {
            dosearch(h, (const char *)map->addr, length);
          }
          else
#endif
          {
            // Allocate a sufficient 4Kb aligned buffer
            size_t _length = (4095 + length)&~4095;
            auto buffer = std::make_shared < std::vector<char,
              detail::aligned_allocator<char, 4096, false >> >(_length + 1);
            // Schedule a read of the file
            auto read = dispatcher->read(make_io_req(
              dispatcher->op_from_scheduled_id(id), buffer->data(), _length, 0));
            auto read_done = dispatcher->completion(read,
              std::make_pair(async_op_flags::none/*regex search might be slow*/,
                std::function<dispatcher::completion_t>(
                  std::bind(&find_in_files::file_read, this, std::placeholders::_1,
                    std::placeholders::_2, buffer, length))));
            doscheduled({ read, read_done });
          }
        }
        docompleted(2);
        return std::make_pair(true, h);
    }
    // An enumeration parsing completion, called when each directory enumeration completes
    std::pair<bool, handle_ptr> dir_enumerated(size_t id, 
        future<> op,
        std::shared_ptr<future<std::pair<std::vector<directory_entry>, bool>>> listing)
    {
        handle_ptr h(op.get_handle());
        future<> lastdir, thisop(dispatcher->op_from_scheduled_id(id));
        // Get the entries from the ready stl_future
        std::vector<directory_entry> entries(std::move(listing->get().first));
        //std::cout << "E " << h->path() << std::endl;
        // For each of the directories schedule an open and enumeration
#if 0
        // Algorithm 1
        {
            std::vector<path_req> dir_reqs; dir_reqs.reserve(entries.size());
            for(auto &entry : entries)
            {
                if((entry.st_type()&S_IFDIR)==S_IFDIR)
                {
                    dir_reqs.push_back(path_req(thisop, h->path()/entry.name()));
                }
            }
            if(!dir_reqs.empty())
            {
                std::vector<std::pair<async_op_flags, std::function<dispatcher::completion_t>>> dir_openedfs(dir_reqs.size(), std::make_pair(async_op_flags::None, std::bind(&find_in_files::dir_opened, this, std::placeholders::_1, std::placeholders::_2)));
                auto dir_opens=dispatcher->dir(dir_reqs);
                doscheduled(dir_opens);
                auto dir_openeds=dispatcher->completion(dir_opens, dir_openedfs);
                doscheduled(dir_openeds);
                // Hold back some of the concurrency
                lastdir=dir_openeds.back();
            }
        }
#else
        // Algorithm 2
        // The Windows NT kernel filing system driver gets upset with too much concurrency
        // when used with OSDirect so throttle directory enumerations to enforce some depth first traversal.
        {
            std::pair<async_op_flags, std::function<dispatcher::completion_t>> dir_openedf=std::make_pair(async_op_flags::none, std::bind(&find_in_files::dir_opened, this, 
                std::placeholders::_1, std::placeholders::_2));
            for(auto &entry : entries)
            {
                if(entry.st_type()==
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
                  boost::afio::filesystem::file_type::directory_file)
#else
                  boost::afio::filesystem::file_type::directory)
#endif
                {
                    auto dir_open=dispatcher->dir(path_req::absolute(lastdir, h->path()/entry.name()));
                    auto dir_opened=dispatcher->completion(dir_open, dir_openedf);
                    doscheduled({ dir_open, dir_opened });
                    lastdir=dir_opened;
                }
            }
        }
#endif

        // For each of the files schedule an open and search
#if 0
        // Algorithm 1
        {
            std::vector<path_req> file_reqs; file_reqs.reserve(entries.size());
            std::vector<std::pair<async_op_flags, std::function<dispatcher::completion_t>>> file_openedfs; file_openedfs.reserve(entries.size());
            for(auto &entry : entries)
            {
                if((entry.st_type()&S_IFREG)==S_IFREG)
                {
                    size_t length=(size_t)entry.st_size();
                    if(length)
                    {
                        file_flags flags=file_flags::read;
#ifdef USE_MMAPS
                        if(length>16384) flags=flags|file_flags::os_mmap;
#endif
                        file_reqs.push_back(path_req(lastdir, h->path()/entry.name(), flags));
                        file_openedfs.push_back(std::make_pair(async_op_flags::None, std::bind(&find_in_files::file_opened, this, std::placeholders::_1, std::placeholders::_2, length)));
                    }
                }
            }
            auto file_opens=dispatcher->file(file_reqs);
            doscheduled(file_opens);
            auto file_openeds=dispatcher->completion(file_opens, file_openedfs);
            doscheduled(file_openeds);
        }
#else
        // Algorithm 2
        {
            for(auto &entry : entries)
            {
                if(entry.st_type()==
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
                  boost::afio::filesystem::file_type::regular_file)
#else
                  boost::afio::filesystem::file_type::regular)
#endif
                {
                    size_t length=(size_t)entry.st_size();
                    if(length)
                    {
                        file_flags flags=file_flags::read;
                        auto file_open=dispatcher->file(path_req::absolute(lastdir, h->path()/entry.name(), flags));
                        auto file_opened=dispatcher->completion(file_open, 
                            std::make_pair(async_op_flags::none, 
                                std::function<dispatcher::completion_t>(
                                    std::bind(&find_in_files::file_opened, this, 
                                        std::placeholders::_1, std::placeholders::_2, length))));
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
    std::pair<bool, handle_ptr> dir_opened(size_t id,
     future<> op)
    {
        handle_ptr h(op.get_handle());
        //std::cout << "D " << h->path() << std::endl;
        // Now we have an open directory handle, schedule an enumeration
        auto enumeration=dispatcher->enumerate(enumerate_req(
            dispatcher->op_from_scheduled_id(id), metadata_flags::size, 1000));
        future<> enumeration_op(enumeration);
        auto listing=std::make_shared<future<std::pair<std::vector<directory_entry>, 
            bool>>>(std::move(enumeration));
        auto enumeration_done=dispatcher->completion(enumeration_op, 
            make_pair(async_op_flags::none,
                std::function<dispatcher::completion_t>(
                    std::bind(&find_in_files::dir_enumerated, this, 
                        std::placeholders::_1, std::placeholders::_2, listing))));
        doscheduled({enumeration, enumeration_done});
        docompleted(2);
        // Complete only if not the cur dir opened
        return std::make_pair(true, h);
    };
    void dowait()
    {
        // Prepare finished
        auto finished_waiter=finished.get_future();
#if 0 // Disabled to maximise performance
        // Wait till done, retrieving any exceptions as they come in to keep memory consumption down
        std::future_status status;
        do
        {
            status=finished_waiter.wait_for(std::chrono::milliseconds(1000));
            std::cout << "\nDispatcher has " << dispatcher->count() << " fds open and " << dispatcher->wait_queue_depth() << " ops outstanding." << std::endl;
            std::vector<future<>> batch; batch.reserve(10000);
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
                when_all_p(batch.begin(), batch.end()).wait();
        } while(status==std::future_status::timeout);
#else
        finished_waiter.wait();
#endif
    }
    // Constructor, which starts the ball rolling
    find_in_files(const char *_regexpr) : regexpr(_regexpr),
        // Create an AFIO dispatcher that bypasses any filing system buffers
        dispatcher(make_dispatcher("file:///", file_flags::will_be_sequentially_accessed/*|file_flags::os_direct*/).get()),
        bytesread(0), filesread(0), filesmatched(0), scheduled(0), completed(0)
    {
        filepaths.reserve(50000);

        // Schedule the recursive enumeration of the current directory
        std::cout << "\n\nStarting directory enumerations ..." << std::endl;
        auto cur_dir=dispatcher->dir(path_req(""));
        auto cur_dir_opened=dispatcher->completion(cur_dir, std::make_pair(async_op_flags::none, 
            std::function<dispatcher::completion_t>(
                std::bind(&find_in_files::dir_opened, this, 
                    std::placeholders::_1, std::placeholders::_2))));
        doscheduled({cur_dir, cur_dir_opened});
        dowait();
    }
};

int main(int argc, const char *argv[])
{
    using std::placeholders::_1; using std::placeholders::_2;
    using namespace boost::afio;
    typedef chrono::duration<double, ratio<1, 1>> secs_type;
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
        std::cout << "\n" << finder.filesmatched << " files matched out of " << finder.filesread 
            << " files which was " << finder.bytesread << " bytes." << std::endl;
        std::cout << "The search took " << diff.count() << " seconds which was " << finder.filesread/diff.count() 
            << " files per second or " << (finder.bytesread/diff.count()/1024/1024) << " Mb/sec." << std::endl;
    }
    catch(...)
    {
        std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
        return 1;
    }
    return 0;
}
//]
