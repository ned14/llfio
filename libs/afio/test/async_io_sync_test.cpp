#include "test_functions.hpp"

BOOST_AUTO_TEST_CASE(async_io_sync)
{
    BOOST_TEST_MESSAGE("Tests async fsync");
    using namespace boost::afio;
    using namespace std;
    vector<char> buffer(64, 'n');
	auto dispatcher = boost::afio::make_async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::AlwaysSync);
    std::cout << "\n\nTesting synchronous directory and file creation:\n";
    auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
    auto mkfile(dispatcher->file(async_path_op_req(mkdir, "testdir/foo", file_flags::Create | file_flags::ReadWrite)));
    auto writefile1(dispatcher->write(async_data_op_req < vector < char >> (mkfile, buffer, 0)));
    auto sync1(dispatcher->sync(writefile1));
    auto writefile2(dispatcher->write(async_data_op_req < vector < char >> (sync1, buffer, 0)));
    auto closefile(dispatcher->close(writefile2));
    auto delfile(dispatcher->rmfile(async_path_op_req(closefile, "testdir/foo")));
    auto deldir(dispatcher->rmdir(async_path_op_req(delfile, "testdir")));
    BOOST_CHECK_NO_THROW(when_all(deldir).wait());
}