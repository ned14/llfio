#include "../../../boost/afio/afio.hpp"

void test_inline_linkage2()
{
    using namespace boost::afio;
    using namespace std;
    vector<char> buffer(64, 'n');
    auto dispatcher = boost::afio::make_async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::AlwaysSync);
    std::cout << "\n\nTesting synchronous directory and file creation:\n";
    {
        auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
        auto mkfile(dispatcher->file(async_path_op_req(mkdir, "testdir/foo", file_flags::Create | file_flags::ReadWrite)));
        auto writefile1(dispatcher->write(async_data_op_req < vector < char >> (mkfile, buffer, 0)));
        auto sync1(dispatcher->sync(writefile1));
        auto writefile2(dispatcher->write(async_data_op_req < vector < char >> (sync1, buffer, 0)));
        auto closefile1(dispatcher->close(writefile2));
        auto openfile(dispatcher->file(async_path_op_req(closefile1, "testdir/foo", file_flags::Read|file_flags::OSMMap)));
        char b[64];
        auto readfile(dispatcher->read(make_async_data_op_req(openfile, b, 0)));
        auto closefile2=dispatcher->close(readfile);
        auto delfile(dispatcher->rmfile(async_path_op_req(closefile2, "testdir/foo")));
        auto deldir(dispatcher->rmdir(async_path_op_req(delfile, "testdir")));
        when_all(mkdir).wait();
        when_all(mkfile).wait();
        when_all(writefile1).wait();
        when_all(sync1).wait();
        when_all(writefile2).wait();
        when_all(closefile1).wait();
        when_all(openfile).wait();
        when_all(readfile).wait();
        when_all(closefile2).wait();
        when_all(delfile).wait();
        when_all(deldir).wait();
    }
}