#include "../test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_enumerate, "Tests that async i/o enumerate() works", 60)
{
    auto dispatcher=boost::afio::make_async_file_io_dispatcher();
	std::cout << "The root directory contains the following items:" << std::endl << std::endl;
	using namespace boost::afio;
	auto rootdir(dispatcher->dir(async_path_op_req("/", file_flags::Read)));
	auto rootdir2(dispatcher->dir(async_path_op_req("/", file_flags::Read)));
	when_all(rootdir).wait();
	when_all(rootdir2).wait();
	// Make sure directory cache is working
	BOOST_CHECK(rootdir.h->get()->native_handle()==rootdir2.h->get()->native_handle());

	std::pair<std::vector<directory_entry>, bool> list;
	do
	{
		auto enumeration(dispatcher->enumerate(async_enumerate_op_req(rootdir, 4096)));
		list=enumeration.first.get();
		BOOST_FOREACH(auto &i, list.first)
		{
			print_stat(rootdir.h->get(), i);
		}
	} while(list.second);
}
