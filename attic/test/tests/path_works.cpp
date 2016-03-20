#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(path_works, "Tests that the path functions work as they are supposed to", 20)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    auto dispatcher = make_dispatcher().get();
    auto dirh = dispatcher->dir(path_req("testdir", file_flags::create));
    dirh.get();
    {
#ifdef WIN32
#define BOOST_AFIO_PATH_WORKS_STR(s) L ## s
#else
#define BOOST_AFIO_PATH_WORKS_STR(s) s
#endif
      static const auto hellobabystr = BOOST_AFIO_PATH_WORKS_STR("hellobaby"), testfilestr = BOOST_AFIO_PATH_WORKS_STR("testfile"), foostr = BOOST_AFIO_PATH_WORKS_STR("foo");
#undef BOOST_AFIO_PATH_WORKS_STR
      {
        future<> op = dispatcher->file(path_req("testdir/testfile", file_flags::create | file_flags::read_write));
        auto h = op.get_handle();
        auto originalpath = h->path();
        print_stat(h);
        auto originalpath2 = h->path();
        BOOST_CHECK(originalpath == originalpath2);

#ifdef WIN32
        // Verify pass through
        path p("testfile");
        BOOST_CHECK(p.native() == L"testfile");

        p = path::make_absolute("testfile");
        BOOST_CHECK(p.native() != L"testfile");
        // Make sure it prepended \??\ to make a NT kernel path
        BOOST_CHECK((p.native()[0] == '\\' && p.native()[1] == '?' && p.native()[2] == '?' && p.native()[3] == '\\'));
        BOOST_CHECK((isalpha(p.native()[4]) && p.native()[5] == ':'));
        // Make sure it converts back via fast path
        auto fp = p.filesystem_path();
        BOOST_CHECK((fp.native()[0] == '\\' && fp.native()[1] == '\\' && fp.native()[2] == '?' && fp.native()[3] == '\\'));
        BOOST_CHECK(p.native().substr(4) == fp.native().substr(4));
        // Make sure it converts back perfectly via slow path
        fp = normalise_path(p);
        auto a = fp.native();
        auto b = filesystem::absolute("testfile").native();
        // Filesystem has been known to return lower case drive letters ...
        std::transform(a.begin(), a.end(), a.begin(), ::tolower);
        std::transform(b.begin(), b.end(), b.begin(), ::tolower);
        if (b.size() >= 260)
          b = L"\\\\?\\" + b;
        std::wcout << a << " (sized " << a.size() << ")" << std::endl;
        std::wcout << b << " (sized " << b.size() << ")" << std::endl;
        BOOST_CHECK(a == b);

        // Make sure it handles extended path inputs
        p = filesystem::path("\\\\?") / filesystem::absolute("testfile");
        BOOST_CHECK((p.native()[0] == '\\' && p.native()[1] == '?' && p.native()[2] == '?' && p.native()[3] == '\\'));
        BOOST_CHECK((isalpha(p.native()[4]) && p.native()[5] == ':'));

        // Make sure native NT path inputs are preserved
        filesystem::path o("\\\\.\\Device1");
        p = o;
        BOOST_CHECK(p.native() == L"\\Device1");
        fp = p.filesystem_path();
        BOOST_CHECK(fp == o);

#endif

        std::cout << "\nRenaming testfile to hellobaby using OS ..." << std::endl;
        filesystem::rename("testdir/testfile", "testdir/hellobaby");
        print_stat(h);
        auto afterrename = h->path();
#ifndef __FreeBSD__  // FreeBSD can't track file renames
        BOOST_CHECK((originalpath.parent_path() / hellobabystr) == afterrename);
#endif
        std::cout << "\nDeleting hellobaby file using OS ..." << std::endl;
        filesystem::remove("testdir/hellobaby");
        auto afterdelete = print_stat(h);
        BOOST_CHECK(h->path() == BOOST_AFIO_V2_NAMESPACE::path());
        std::cout << "\nEnumerating directory to make sure hellobaby is not there ..." << std::endl;
        auto contents = dispatcher->enumerate(enumerate_req(dirh, metadata_flags::All, 50)).get().first;
        for (auto &i : contents)
        {
          std::cout << "  " << i.name() << std::endl;
#ifndef WIN32  // Windows only marks for deletion, doesn't actually delete
          BOOST_CHECK(i.name() != hellobabystr);
#endif
        }
      }

      std::cout << "\nCreating hard links testfile2 and testfile3 from testfile ..." << std::endl;
      handle_ptr h;
      future<> op = dispatcher->file(path_req::relative(dirh, testfilestr, file_flags::create | file_flags::read_write));
      h = op.get_handle();
      BOOST_CHECK(h->path(true) == dirh->path() / testfilestr);
      bool supports_hard_links = true;
      try
      {
        h->link(path_req::relative(dirh, "testfile2"));
      }
      catch (...)
      {
        supports_hard_links = false;
        dispatcher->rmfile(path_req::relative(dirh, "testfile")).get();
      }
      if (supports_hard_links)
      {
        BOOST_CHECK(h->path(true) == dirh->path() / testfilestr);
        h->link(path_req::relative(dirh, "testfile3"));
        BOOST_CHECK(h->path(true) == dirh->path() / testfilestr);
        dispatcher->truncate(op, 78).get();
        dispatcher->sync(op).get();
        auto entry = h->lstat();
        BOOST_CHECK(entry.st_size == 78);
        auto contents = dispatcher->enumerate(enumerate_req(dirh, metadata_flags::All, 50)).get().first;
        BOOST_CHECK(contents.size() == 3);
        for (auto &i : contents)
        {
          print_stat(dirh.get_handle(), i);
          BOOST_CHECK(i.st_ino() == entry.st_ino);
#ifndef WIN32  // Windows takes too long to update this
          BOOST_CHECK(i.st_size() == entry.st_size);
#endif
          BOOST_CHECK(i.st_nlink() == 3);
        }

        std::cout << "\nRelinking hard link testfile to foo ..." << std::endl;
        h->atomic_relink(path_req::relative(dirh, foostr));
        BOOST_CHECK(h->path(true) == dirh->path() / foostr);
        dispatcher->truncate(op, 79).get();
        dispatcher->sync(op).get();
        entry = h->lstat();
        BOOST_CHECK(entry.st_size == 79);
        contents = dispatcher->enumerate(enumerate_req(dirh, metadata_flags::All, 50)).get().first;
        BOOST_CHECK(contents.size() == 3);
        for (auto &i : contents)
        {
          print_stat(dirh.get_handle(), i);
          BOOST_CHECK(i.name() != testfilestr);
          BOOST_CHECK(i.st_ino() == entry.st_ino);
#ifndef WIN32  // Windows takes too long to update this
          BOOST_CHECK(i.st_size() == entry.st_size);
#endif
          BOOST_CHECK(i.st_nlink() == 3);
        }

        std::cout << "\nUnlinking hard links ..." << std::endl;
        h->unlink();
#ifndef __FreeBSD__  // FreeBSD will not notice a file with multiple hard links is deleted
        BOOST_CHECK(h->path(true).empty());
#endif
        contents = dispatcher->enumerate(enumerate_req(dirh, metadata_flags::All, 50)).get().first;
        BOOST_CHECK(contents.size() == 2);
        for (auto &i : contents)
        {
          print_stat(dirh.get_handle(), i);
          BOOST_CHECK(i.name() != foostr); // This should get filtered out by AFIO on Windows due to magic naming
          BOOST_CHECK(i.st_nlink() == 2);
        }
        op = future<>();
        h.reset(); // Should actually cause the unlink to really happen on Windows
        contents = dispatcher->enumerate(enumerate_req(dirh, metadata_flags::All, 50)).get().first;
        BOOST_CHECK(contents.size() == 2);
        for (auto &i : contents)
        {
          print_stat(dirh.get_handle(), i);
          BOOST_CHECK(i.name() != foostr);
          BOOST_CHECK(i.st_nlink() == 2);
        }
        dispatcher->rmfile(path_req::relative(dirh, "testfile2")).get();
        dispatcher->rmfile(path_req::relative(dirh, "testfile3")).get();
        contents = dispatcher->enumerate(enumerate_req(dirh, metadata_flags::All, 50)).get().first;
        BOOST_CHECK(contents.size() == 0);
      }
    }

    // Reopen with write privs in order to unlink
    dirh = dispatcher->dir(path_req("testdir", file_flags::read_write));
    dirh->unlink();
}
