#define _CRT_SECURE_NO_WARNINGS

#include "../test_functions.hpp"

std::ostream &operator<<(std::ostream &s, const std::chrono::system_clock::time_point &ts)
{
	char buf[32];
    struct tm *t;
    size_t len=sizeof(buf);
    size_t ret;
	time_t v=std::chrono::system_clock::to_time_t(ts);
	std::chrono::system_clock::duration remainder(ts-std::chrono::system_clock::from_time_t(v));

#ifdef _MSC_VER
	_tzset();
#else
    tzset();
#endif
    if ((t=localtime(&v)) == NULL)
    {
    	s << "<bad timespec>";
        return s;
    }

    ret = strftime(buf, len, "%Y-%m-%d %H:%M:%S", t);
    if (ret == 0)
    {
    	s << "<bad timespec>";
        return s;
    }
    len -= ret - 1;

    sprintf(&buf[strlen(buf)], ".%09ld", remainder.count());
    s << buf;

    return s;
}

static boost::afio::stat_t print_stat(std::shared_ptr<boost::afio::async_io_handle> h)
{
	using namespace boost::afio;
	auto entry=h->lstat(metadata_flags::All);
	std::cout << "Entry " << h->path() << " is a ";
	if(entry.st_type & S_IFLNK)
		std::cout << "link";
	else if(entry.st_type & S_IFDIR)
		std::cout << "directory";
	else
		std::cout << "file";
	std::cout << " and it has the following information:" << std::endl;
#define PRINT_FIELD(field) \
    std::cout << "  st_" #field ": "; if(!!(directory_entry::metadata_supported()&metadata_flags::field)) std::cout << entry.st_##field; else std::cout << "unknown"; std::cout << std::endl
    PRINT_FIELD(dev);
    PRINT_FIELD(ino);
    PRINT_FIELD(type);
    PRINT_FIELD(mode);
    PRINT_FIELD(nlink);
    PRINT_FIELD(uid);
    PRINT_FIELD(gid);
    PRINT_FIELD(rdev);
    PRINT_FIELD(atim);
    PRINT_FIELD(mtim);
    PRINT_FIELD(ctim);
    PRINT_FIELD(size);
    PRINT_FIELD(allocated);
    PRINT_FIELD(blocks);
    PRINT_FIELD(blksize);
    PRINT_FIELD(flags);
    PRINT_FIELD(gen);
    PRINT_FIELD(birthtim);
	return entry;
}

BOOST_AFIO_AUTO_TEST_CASE(async_io_lstat_works, "Tests that async i/o lstat() works", 60)
{
	if(boost::filesystem::exists("testdir"))
		boost::filesystem::remove_all("testdir");

	using namespace boost::afio;
    auto dispatcher=make_async_file_io_dispatcher();
    auto test(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
    auto mkdir(dispatcher->dir(async_path_op_req(test, "testdir/dir", file_flags::Create)));
	auto mkfile(dispatcher->file(async_path_op_req(mkdir, "testdir/dir/file", file_flags::Create)));
	auto mklink(dispatcher->symlink(async_path_op_req(mkdir, "testdir/linktodir", file_flags::Create)));

	auto mkdirstat=print_stat(mkdir.h->get());
	BOOST_CHECK(mkdirstat.st_type==S_IFDIR);
	auto mkfilestat=print_stat(mkfile.h->get());
	BOOST_CHECK(mkfilestat.st_type==S_IFREG);
	auto mklinkstat=print_stat(mklink.h->get());
	BOOST_CHECK(mklinkstat.st_type==S_IFLNK);
}