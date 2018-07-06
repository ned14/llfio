#include "llfio_pch.hpp"

int main(void)
{
    //[statfs_example
    boost::llfio::current_dispatcher_guard h(boost::llfio::make_dispatcher().get());
    
    // Open the root directory
    boost::llfio::handle_ptr rootdir(boost::llfio::dir("/"));

    // Ask the filing system of the root directory how much free space there is
    boost::llfio::statfs_t statfs(boost::llfio::statfs(rootdir,
        boost::llfio::fs_metadata_flags::bsize|boost::llfio::fs_metadata_flags::bfree));
    
    std::cout << "Your root filing system has "
        << (statfs.f_bfree*statfs.f_bsize/1024.0/1024.0/1024.0) << " Gb free." << std::endl;
    //]
}
