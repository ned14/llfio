#include "../test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_enumerate, "Tests that async i/o enumerate() works", 60)
{
    auto dispatcher=boost::afio::make_async_file_io_dispatcher();
	std::cout << "The root directory contains the following items:" << std::endl << std::endl;
	using namespace boost::afio;
	auto rootdir(dispatcher->dir(async_path_op_req("/")));
	auto enumeration(dispatcher->enumerate(async_enumerate_op_req(rootdir, 4096)));
	std::pair<std::vector<directory_entry>, bool> list(enumeration.first.get());
	BOOST_FOREACH(auto &i, list.first)
	{
		print_stat(rootdir.h->get(), i);
	}
}