#include "boost/afio/afio.hpp"
#include <iostream>

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) && !(defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */) && !defined(__clang__)
	//[filedir_example
    std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher=
        boost::afio::make_async_file_io_dispatcher();
		
	try
	{
		// Schedule creating a directory called testdir
		auto mkdir(dispatcher->dir(boost::afio::async_path_op_req("testdir",
			boost::afio::file_flags::Create)));
		// Schedule creating a file called testfile in testdir only when testdir has been created
		auto mkfile(dispatcher->file(boost::afio::async_path_op_req(mkdir,
			"testdir/testfile", boost::afio::file_flags::Create)));
		// Schedule creating a symbolic link called linktodir to the item referred to by the precondition
		// i.e. testdir. Note that on Windows you can only symbolic link directories.
		auto mklink(dispatcher->symlink(boost::afio::async_path_op_req(mkdir,
			"testdir/linktodir", boost::afio::file_flags::Create)));

		// Schedule deleting the symbolic link only after when it has been created
		auto rmlink(dispatcher->rmsymlink(boost::afio::async_path_op_req(mklink,
			"testdir/linktodir")));
		// Schedule deleting the file only after when it has been created
		auto rmfile(dispatcher->rmfile(boost::afio::async_path_op_req(mkfile,
			"testdir/testfile")));
		// Schedule waiting until both the preceding operations have finished
		auto barrier(dispatcher->barrier({rmlink, rmfile}));
		// Schedule deleting the directory only after the barrier completes
		auto rmdir(dispatcher->rmdir(boost::afio::async_path_op_req(barrier.front(),
			"testdir")));
		// Check ops for errors
		boost::afio::when_all({mkdir, mkfile, mklink, rmlink, rmfile, rmdir}).wait();
	}
	catch(...)
	{
		std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
		throw;
	}
    //]
#endif
}
