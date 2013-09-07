#include "../test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_lstat_works, "Tests that async i/o lstat() works", 60)
{
	if(boost::filesystem::exists("testdir"))
		boost::filesystem::remove_all("testdir");

	{
		using namespace boost::afio;
		auto dispatcher=make_async_file_io_dispatcher();
		{
			auto test(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
			auto mkdir(dispatcher->dir(async_path_op_req(test, "testdir/dir", file_flags::Create|file_flags::FastDirectoryEnumeration)));
			auto mkfile(dispatcher->file(async_path_op_req(mkdir, "testdir/dir/file", file_flags::Create)));
			auto mklink(dispatcher->symlink(async_path_op_req(mkdir, "testdir/linktodir", file_flags::Create)));
			when_all(mklink).wait();

			auto mkdirstat=print_stat(mkdir.h->get());
			BOOST_CHECK((mkdirstat.st_type&S_IFDIR)==S_IFDIR);
			auto mkfilestat=print_stat(mkfile.h->get());
			BOOST_CHECK((mkfilestat.st_type&S_IFREG)==S_IFREG);
			auto mklinkstat=print_stat(mklink.h->get());
			BOOST_CHECK((mklinkstat.st_type&S_IFLNK)==S_IFLNK);

			// Some sanity stuff
			BOOST_CHECK(mkdirstat.st_ino!=mkfilestat.st_ino);
			BOOST_CHECK(mkfilestat.st_ino!=mklinkstat.st_ino);
			BOOST_CHECK(mkdirstat.st_ino!=mklinkstat.st_ino);
			BOOST_CHECK(mklink.h->get()->target()==mkdir.h->get()->path());
                        BOOST_CHECK(mkdir.h->get()->container()->native_handle()==test.h->get()->native_handle());
		}

		// Let the handles close before deleting
		while(dispatcher->count()) boost::this_thread::yield();
		auto rmlink(dispatcher->rmsymlink(async_path_op_req("testdir/linktodir")));
		auto rmfile(dispatcher->rmfile(async_path_op_req(rmlink, "testdir/dir/file")));
		auto rmdir(dispatcher->rmdir(async_path_op_req(rmfile, "testdir/dir")));
		auto rmtest(dispatcher->rmdir(async_path_op_req(rmdir, "testdir")));
		when_all(rmtest).wait();
	}
}
