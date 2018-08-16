#include "boost/afio/afio.hpp"

void test_inline_linkage1()
{
    using namespace boost::afio;
    using namespace std;
    vector<char> buffer(64, 'n');
    auto dispatcher = boost::afio::make_dispatcher("file:///", boost::afio::file_flags::always_sync).get();
    std::cout << "\n\nTesting synchronous directory and file creation:\n";
    {
      auto mkdir(dispatcher->dir(path_req("testdir", file_flags::create)));
      auto mkfile(dispatcher->file(path_req::relative(mkdir, "foo", file_flags::create | file_flags::read_write)));
      auto writefile1(dispatcher->write(io_req < vector < char >>(mkfile, buffer, 0)));
      auto sync1(dispatcher->sync(writefile1));
      auto writefile2(dispatcher->write(io_req < vector < char >>(sync1, buffer, 0)));
      auto closefile1(dispatcher->close(writefile2));
      auto openfile(dispatcher->file(path_req::relative(closefile1, file_flags::read)));
      char b[64];
      auto readfile(dispatcher->read(make_io_req(openfile, b, 0)));
      auto delfile(dispatcher->close(dispatcher->rmfile(readfile)));
      auto deldir(dispatcher->close(dispatcher->rmdir(delfile)));
      when_all_p(mkdir).wait();
      when_all_p(mkfile).wait();
      when_all_p(writefile1).wait();
      when_all_p(sync1).wait();
      when_all_p(writefile2).wait();
      when_all_p(closefile1).wait();
      when_all_p(openfile).wait();
      when_all_p(readfile).wait();
      when_all_p(delfile).wait();
      when_all_p(deldir).wait();
    }
}