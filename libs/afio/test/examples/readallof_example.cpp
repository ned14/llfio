#include "afio_pch.hpp"

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
    auto file_enumed = dispatcher->enumerate(async_enumerate_op_req(dir_opened, metadata_flags::size, 2, true, "foo.txt"));
    // Schedule in parallel opening the file
    auto file_opened = dispatcher->file(async_path_op_req("foo.txt"));
    // Get the directory_entry for the first result
    directory_entry &de = file_enumed.first.get().first.front(); // blocks!
    // Schedule a file read once we know the file size
    auto file_read = dispatcher->read(make_async_data_op_req(file_opened,
        (void *) input,
        (size_t) de.st_size(), // won't block
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
    // Fetch ONLY the size metadata. Blocks because it's synchronous!
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
