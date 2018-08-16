#include "afio_pch.hpp"

int main(void)
{        
    try
    {
        //[readwrite_example
        namespace afio = BOOST_AFIO_V2_NAMESPACE;
        namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;

        // Set a dispatcher as current for this thread
        afio::current_dispatcher_guard h(afio::make_dispatcher().get());

        // Schedule an opening of a file called example_file.txt
        afio::future<> openfile = afio::async_file("example_file.txt",
          afio::file_flags::create | afio::file_flags::read_write);
        
        // Something a bit surprising for many people is that writing off
        // the end of a file in AFIO does NOT extend the file and writes
        // which go past the end will simply fail instead. Why not?
        // Simple: that's the convention with async file i/o, because
        // synchronising multiple processes concurrently adjusting a
        // file's length has significant overhead which is wasted if you
        // don't need that functionality. Luckily, there is an easy
        // workaround: either open a file for append-only access, in which
        // case all writes extend the file for you, or else you explicitly
        // extend files before writing, like this:
        afio::future<> resizedfile = afio::async_truncate(openfile, 12);
    
        // Config a write gather. You could do this of course as a batch
        // of writes, but a write gather has optimised host OS support in most
        // cases, so it's one syscall instead of many.
        std::vector<asio::const_buffer> buffers;
        buffers.push_back(asio::const_buffer("He", 2));
        buffers.push_back(asio::const_buffer("ll", 2));
        buffers.push_back(asio::const_buffer("o ", 2));
        buffers.push_back(asio::const_buffer("Wo", 2));
        buffers.push_back(asio::const_buffer("rl", 2));
        buffers.push_back(asio::const_buffer("d\n", 2));
        // Schedule the write gather to offset zero after the resize file
        afio::future<> written(afio::async_write(resizedfile, buffers, 0));
        
        // Have the compiler config the exact same write gather as earlier for you
        // The compiler assembles an identical sequence of ASIO write gather
        // buffers for you
        std::vector<std::string> buffers2={ "He", "ll", "o ", "Wo", "rl", "d\n" };
        // Schedule this to occur after the previous write completes
        afio::future<> written2(afio::async_write(written, buffers2, 0));
        
        // Schedule making sure the previous batch has definitely reached physical storage
        // This won't complete until the write is on disc
        afio::future<> stored(afio::async_sync(written2));
                
        // Schedule filling this array from the file. Note how convenient std::array
        // is and completely replaces C style char buffer[bytes]
        std::array<char, 12> buffer;
        afio::future<> read(afio::async_read(stored, buffer, 0));
            
        // Schedule the closing and deleting of example_file.txt after the contents read
        afio::future<> deletedfile(afio::async_rmfile(afio::async_close(read)));
        
        // Wait until the buffer has been filled, checking all steps for errors
        afio::when_all_p(openfile, resizedfile, written, written2, stored, read).get(); /*< waits for file open, resize, write, sync and read to complete, throwing any exceptions encountered >*/
        
        // There is actually a io_req<std::string> specialisation you
        // can use to skip this bit by reading directly into a string ...
        std::string contents(buffer.begin(), buffer.end());
        std::cout << "Contents of file is '" << contents << "'" << std::endl;

        // Check remaining ops for errors
        deletedfile.get();
        //]
    }
    catch(const BOOST_AFIO_V2_NAMESPACE::system_error &e) { std::cerr << "ERROR: program exits via system_error code " << e.code().value() << " (" << e.what() << ")" << std::endl; return 1; }
    catch(const std::exception &e) { std::cerr << "ERROR: program exits via exception (" << e.what() << ")" << std::endl; return 1; }
    catch(...) { std::cerr << "ERROR: program exits via " << boost::current_exception_diagnostic_information(true) << std::endl; return 1; }
    return 0;
}
