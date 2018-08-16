#include "afio_pch.hpp"

int main(void)
{
    //[enumerate_example
    boost::afio::current_dispatcher_guard h(boost::afio::make_dispatcher().get());

    // Schedule an opening of the root directory
    boost::afio::future<> rootdir(boost::afio::async_dir("/"));
    
    std::pair<std::vector<boost::afio::directory_entry>, bool> list;
    // This is used to reset the enumeration to the start
    bool restart=true;
    do
    {
        // Schedule an enumeration of an open directory handle
        boost::afio::future<std::pair<std::vector<boost::afio::directory_entry>, bool>>
          enumeration(boost::afio::async_enumerate(rootdir,
                /* This is the maximum entries to enumerate. Note
                the use of compatibility_maximum() which is the
                same value your libc uses. The problem with smaller
                enumerations is that the directory contents can change
                out from underneath you more frequently. */
                boost::afio::directory_entry::compatibility_maximum(),
                /* True if to reset enumeration */
                restart));
        restart=false;
        
        // People using AFIO often forget that futures can be waited
        // on normally without needing to wait on the op handle
        list=enumeration.get();
        for(boost::afio::directory_entry &i : list.first)
        {
#ifdef WIN32
            std::wcout << i.name();
#else
            std::cout << i.name();
#endif
            std::cout << " type " << static_cast<int>(i.st_type()) << std::endl;
        }
    } while(list.second);
    //]
}
