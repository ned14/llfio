#include "boost/afio/afio.hpp"
#include <iostream>

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) // Don't bother with VS2010.
    //[enumerate_example
    std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher=
        boost::afio::make_async_file_io_dispatcher();
    
    // Schedule an opening of the root directory
    boost::afio::async_io_op rootdir(dispatcher->dir(boost::afio::async_path_op_req("/")));
    
    std::pair<std::vector<boost::afio::directory_entry>, bool> list;
    // This is used to reset the enumeration to the start
    bool restart=true;
    do
    {
        // Schedule an enumeration of an open directory handle
        // Note it returns a future to the results and an op ref 
        std::pair<
            boost::afio::future<std::pair<std::vector<boost::afio::directory_entry>, bool>>,
            boost::afio::async_io_op
        >  enumeration(
            dispatcher->enumerate(boost::afio::async_enumerate_op_req(
                /* This is the handle to enumerate */
                rootdir,
                /* This is the maximum entries to enumerate. Note
                the use of compatibility_maximum() which is the
                same value your libc uses. The problem with smaller
                enumerations is that the directory contents can change
                out from underneath you more frequently. */
                boost::afio::directory_entry::compatibility_maximum(),
                /* True if to reset enumeration */
                restart)));
        restart=false;
        
        // People using AFIO often forget that futures can be waited
        // on normally without needing to wait on the op handle
        list=enumeration.first.get();
        for(boost::afio::directory_entry &i : list.first)
        {
            std::cout << i.name() << " type " << i.st_type() << std::endl;
        }
    } while(list.second);
    //]
#endif
}
