#include "boost/afio/afio.hpp"
#include <iostream>

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC <= 1700) && !(defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */) && !defined(__clang__)
    //[readwrite_example
    std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher=
        boost::afio::make_async_file_io_dispatcher();
        
    try
    {
        // Schedule an opening of a file called example_file.txt
        boost::afio::async_path_op_req req("example_file.txt",
            boost::afio::file_flags::ReadWrite);
        boost::afio::async_io_op openfile(dispatcher->file(req)); /*< schedules open file as soon as possible >*/
        
        // Something a bit surprising for many people is that writing off
        // the end of a file in AFIO does NOT extend the file and writes
        // which go past the end will simply fail instead. Why not?
        // Simple: that's the convention with async file i/o. You must 
        // explicitly extend files before writing, like this:
        boost::afio::async_io_op resizedfile(dispatcher->truncate(openfile, 12)); /*< schedules resize file ready for writing after open file completes >*/
    
        // Config a write gather. You could do this of course as a batch
        // of writes, but a write gather has optimised host OS support in most
        // cases, so it's one syscall instead of many.
        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::const_buffer("He", 2));
        buffers.push_back(boost::asio::const_buffer("ll", 2));
        buffers.push_back(boost::asio::const_buffer("o ", 2));
        buffers.push_back(boost::asio::const_buffer("Wo", 2));
        buffers.push_back(boost::asio::const_buffer("rl", 2));
        buffers.push_back(boost::asio::const_buffer("d\n", 2));
        // Schedule the write gather to offset zero
        boost::afio::async_io_op written(dispatcher->write(
            boost::afio::make_async_data_op_req(resizedfile, std::move(buffers), 0))); /*< schedules write after resize file completes >*/
        
        // Schedule making sure the previous batch has definitely reached physical storage
        // This won't complete until the write is on disc
        boost::afio::async_io_op stored(dispatcher->sync(written)); /*< schedules sync after write completes >*/
                
        // Schedule filling this array from the file. Note how convenient std::array
        // is and completely replaces C style char buffer[bytes]
        std::array<char, 12> buffer;
        boost::afio::async_io_op read(dispatcher->read(
            boost::afio::make_async_data_op_req(stored, buffer, 0))); /*< schedules read after sync completes >*/
            
        // Schedule the closing and deleting of example_file.txt after the contents read
        req.precondition=dispatcher->close(read); /*< schedules close file after read completes >*/
        boost::afio::async_io_op deletedfile(dispatcher->rmfile(req)); /*< schedules delete file after close completes >*/
        
        // Wait until the buffer has been filled, checking all steps for errors
        boost::afio::when_all({openfile, resizedfile, written, stored, read}).wait(); /*< waits for file open, resize, write, sync and read to complete, throwing any exceptions encountered >*/
        
        // There is actually a async_data_op_req<std::string> specialisation you
        // can use to skip this bit by reading directly into a string ...
        std::string contents(buffer.begin(), buffer.end());
        std::cout << "Contents of file is '" << contents << "'" << std::endl;

        // Check remaining ops for errors
        boost::afio::when_all({req.precondition /*close*/, deletedfile}).wait();        /*< waits for file close and delete to complete, throwing any exceptions encountered >*/
    }
    catch(...)
    {
        std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
        throw;
    }
    //]
#endif
}
