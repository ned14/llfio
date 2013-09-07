#include "../test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_enumerate, "Tests that async i/o enumerate() works", 60)
{
    auto dispatcher=boost::afio::make_async_file_io_dispatcher();
	std::cout << "Opening root directory for enumeration" << std::endl;
	using namespace boost::afio;
	auto rootdir(dispatcher->dir(async_path_op_req("/", file_flags::Read)));
	auto rootdir2(dispatcher->dir(async_path_op_req("/", file_flags::Read)));
	when_all(rootdir).wait();
	when_all(rootdir2).wait();
	// Make sure directory cache is working
	BOOST_CHECK(rootdir.h->get()->native_handle()==rootdir2.h->get()->native_handle());

	std::cout << "The root directory contains the following items:" << std::endl << std::endl;
	std::vector<directory_entry> rootdircontents1, rootdircontents2;
	// Read everything in one go
	std::pair<std::vector<directory_entry>, bool> list;
	bool first=true;
	do
	{
		auto enumeration(dispatcher->enumerate(async_enumerate_op_req(rootdir, directory_entry::compatibility_maximum(), first)));
		first=false;
		list=enumeration.first.get();
		BOOST_FOREACH(auto &i, list.first)
		{
			print_stat(rootdir.h->get(), i);
		}
		rootdircontents1.insert(rootdircontents1.end(), std::make_move_iterator(list.first.begin()), std::make_move_iterator(list.first.end()));
	} while(list.second);
	// Now read everything one at a time
	first=true;
	do
	{
		auto enumeration(dispatcher->enumerate(async_enumerate_op_req(rootdir, 1, first)));
		first=false;
		std::cout << ".";
		list=enumeration.first.get();
		rootdircontents2.insert(rootdircontents2.end(), std::make_move_iterator(list.first.begin()), std::make_move_iterator(list.first.end()));
	} while(list.second);
	std::cout << "rootdircontents1 has " << rootdircontents1.size() << " items and rootdircontents2 has " << rootdircontents2.size() << " items." << std::endl;
	BOOST_CHECK(rootdircontents1.size()==rootdircontents2.size());
}
