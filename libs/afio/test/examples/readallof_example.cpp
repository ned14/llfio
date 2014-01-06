#include "boost/afio/afio.hpp"
// Need to include a copy of ASIO
#ifdef BOOST_ASIO_SEPARATE_COMPILATION
#include "../../../../boost/asio/impl/src.hpp"
#endif

int main(void)
{
    using namespace boost::afio;
    auto dispatcher=make_async_file_io_dispatcher();
    
{
    //[readallof_example_bad
    char input[1024];
    auto file_opened = dispatcher->file(async_path_op_req("foo.txt"));
    auto file_read = dispatcher->read(make_async_data_op_req(file_opened, input, 0));
    //]
}

{   
    //[readallof_example_many
    char input[1024];
    // Schedule enumerating the containing directory, but only for foo.txt
    auto dir_opened = dispatcher->dir(async_path_op_req("")); // "" means current directory in AFIO
    auto file_enumed = dispatcher->enumerate(async_enumerate_op_req(dir_opened, "foo.txt"));
    // Schedule in parallel opening the file
    auto file_opened = dispatcher->file(async_path_op_req("foo.txt"));
    // Get the directory_entry for the first result
    directory_entry &de = file_enumed.first.get().first.front(); // blocks!
    // Get the handle to the opened directory
    auto &dirh = when_all(dir_opened).get().front(); // blocks!
    // Schedule a file read once we know the file size
    auto file_read = dispatcher->read(make_async_data_op_req(file_opened,
        (void *) input,
        (size_t) de.st_size(dirh), // blocks, as it may fetch the size now!
        0));
    //]
}

{   
    //[readallof_example_single
    char input[1024];
    // Schedule opening the file
    auto file_opened = dispatcher->file(async_path_op_req("foo.txt"));
    // Wait till it is opened
    auto fileh = when_all(file_opened).get().front();
    // Fetch ONLY the size metadata. Blocks!
    directory_entry de = fileh->direntry(metadata_flags::size);
    // Schedule a file read now we know the file size
    auto file_read = dispatcher->read(make_async_data_op_req(file_opened,
        (void *) input,
        (size_t) de.st_size(), // doesn't block, as size was fetched before.
        0));
    //]
}
    return 0;
}
