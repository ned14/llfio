#include "afio_pch.hpp"

int main(void)
{
    using namespace boost::afio;
    auto dispatcher=make_dispatcher().get();
    current_dispatcher_guard h(dispatcher);

{
    //[readallof_example_bad
    char input[1024];
    auto file_opened = file("foo.txt");
    read(file_opened, input, 0);
    //]
}

{   
    //[readallof_example_many
    char input[1024];
    // Schedule enumerating the containing directory, but only for foo.txt
    auto dir_opened = async_dir("");  // "" means current directory in AFIO
    auto file_enumed = async_enumerate(dir_opened, metadata_flags::size,
      2, true, "foo.txt");
    // Schedule in parallel opening the file
    auto file_opened = async_file(dir_opened, "foo.txt");
    auto file_read = file_enumed.then(
      [&input](future<std::pair<std::vector<directory_entry>, bool>> &f) {
        // Get the directory_entry for the first result
        directory_entry &de = f.get().first.front();
        // Schedule a file read once we know the file size
        return async_read(f, (void *)input,
          (size_t)de.st_size(), // won't block
          0);
    });
    file_read.get();
    //]
}

{   
    //[readallof_example_single
    char input[1024];
    // Open the file synchronously
    auto file_opened = file("foo.txt");
    // Fetch ONLY the size metadata. Blocks because it's synchronous!
    directory_entry de = file_opened->direntry(metadata_flags::size);
    // Read now we know the file size
    read(file_opened,
        (void *) input,
        (size_t) de.st_size(), // doesn't block, as size was fetched before.
        0);
    //]
}
    return 0;
}
