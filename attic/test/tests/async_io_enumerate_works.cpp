#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_enumerate, "Tests that async i/o enumerate() works", 60)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    auto dispatcher=make_dispatcher().get();
    std::cout << "Opening root directory for enumeration" << std::endl;
    auto rootdir(dispatcher->dir(path_req("/", file_flags::read)));
    auto rootdir2(dispatcher->dir(path_req("/", file_flags::read)));
    when_all_p(rootdir).wait();
    when_all_p(rootdir2).wait();
    // Make sure directory cache is working
    BOOST_CHECK(rootdir.get_handle()->native_handle()==rootdir2.get_handle()->native_handle());

    std::cout << "The root directory contains the following items:" << std::endl << std::endl;
    std::vector<directory_entry> rootdircontents1, rootdircontents2;
    std::unordered_set<directory_entry> rootdircontents1a, rootdircontents2a;
    // Read everything in one go
    std::pair<std::vector<directory_entry>, bool> list;
    bool first=true;
    do
    {
        auto enumeration(dispatcher->enumerate(enumerate_req(rootdir, directory_entry::compatibility_maximum(), first, path(), directory_entry::metadata_fastpath())));
        first=false;
        list=enumeration.get();
        if(!list.first.empty()) BOOST_CHECK((list.first.front().metadata_ready()&directory_entry::metadata_fastpath())== directory_entry::metadata_fastpath());
        for(auto &i: list.first)
        {
            print_stat(rootdir.get_handle(), i);
        }
        rootdircontents1a.insert(list.first.begin(), list.first.end());
        rootdircontents1.insert(rootdircontents1.end(), std::make_move_iterator(list.first.begin()), std::make_move_iterator(list.first.end()));
    } while(list.second);
    // Now read everything one at a time
    first=true;
    do
    {
        auto enumeration(dispatcher->enumerate(enumerate_req(rootdir, 1, first, path(), directory_entry::metadata_fastpath())));
        first=false;
        std::cout << ".";
        list=enumeration.get();
        if(!list.first.empty()) BOOST_CHECK((list.first.front().metadata_ready()&directory_entry::metadata_fastpath())== directory_entry::metadata_fastpath());
        rootdircontents2a.insert(list.first.begin(), list.first.end());
        rootdircontents2.insert(rootdircontents2.end(), std::make_move_iterator(list.first.begin()), std::make_move_iterator(list.first.end()));
    } while(list.second);
    std::cout << "rootdircontents1 has " << rootdircontents1.size() << " items and rootdircontents2 has " << rootdircontents2.size() << " items." << std::endl;
    BOOST_CHECK(rootdircontents1.size()==rootdircontents2.size());
    BOOST_CHECK(rootdircontents1.size()==rootdircontents1a.size());
    BOOST_CHECK(rootdircontents2.size()==rootdircontents2a.size());
    
    // Make sure unwildcarded single glob works (it has a fastpath on POSIX)
    auto enumeration1(dispatcher->enumerate(enumerate_req(rootdir)));
    auto &&direntries1=enumeration1.get().first;
    BOOST_REQUIRE(direntries1.size()>0);
    auto direntry1(direntries1.front());
    auto enumeration2(dispatcher->enumerate(enumerate_req(rootdir, path(direntry1.name()))));
    auto &&direntries2=enumeration2.get().first;
    BOOST_REQUIRE(direntries2.size()>0);
    auto direntry2(direntries2.front());
    BOOST_CHECK(direntry1==direntry2);
}
