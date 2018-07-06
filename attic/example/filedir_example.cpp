#include "llfio_pch.hpp"

int main(void)
{
  //[filedir_example
  using namespace BOOST_AFIO_V2_NAMESPACE;
  using BOOST_AFIO_V2_NAMESPACE::rmdir;
  std::shared_ptr<boost::llfio::dispatcher> dispatcher =
    boost::llfio::make_dispatcher().get();
  current_dispatcher_guard h(dispatcher);

  // Free function
  try
  {
    // Schedule creating a directory called testdir
    auto mkdir(async_dir("testdir", boost::llfio::file_flags::create));
    // Schedule creating a file called testfile in testdir only when testdir has been created
    auto mkfile(async_file(mkdir, "testfile", boost::llfio::file_flags::create));
    // Schedule creating a symbolic link called linktodir to the item referred to by the precondition
    // i.e. testdir. Note that on Windows you can only symbolic link directories.
    auto mklink(async_symlink(mkdir, "linktodir", mkdir, boost::llfio::file_flags::create));

    // Schedule deleting the symbolic link only after when it has been created
    auto rmlink(async_rmsymlink(mklink));
    // Schedule deleting the file only after when it has been created
    auto rmfile(async_close(async_rmfile(mkfile)));
    // Schedule waiting until both the preceding operations have finished
    auto barrier(dispatcher->barrier({ rmlink, rmfile }));
    // Schedule deleting the directory only after the barrier completes
    auto rmdir(async_rmdir(depends(barrier.front(), mkdir)));
    // Check ops for errors
    boost::llfio::when_all_p(mkdir, mkfile, mklink, rmlink, rmfile, rmdir).wait();
  }
  catch (...)
  {
    std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
    throw;
  }

  // Batch
  try
  {
      // Schedule creating a directory called testdir
      auto mkdir(dispatcher->dir(std::vector<boost::llfio::path_req>(1,
        boost::llfio::path_req("testdir", boost::llfio::file_flags::create))).front());
      // Schedule creating a file called testfile in testdir only when testdir has been created
      auto mkfile(dispatcher->file(std::vector<boost::llfio::path_req>(1,
        boost::llfio::path_req::relative(mkdir, "testfile",
          boost::llfio::file_flags::create))).front());
      // Schedule creating a symbolic link called linktodir to the item referred to by the precondition
      // i.e. testdir. Note that on Windows you can only symbolic link directories. Note that creating
      // symlinks must *always* be as an absolute path, as that is how they are stored.
      auto mklink(dispatcher->symlink(std::vector<boost::llfio::path_req>(1, 
        boost::llfio::path_req::absolute(mkdir, "testdir/linktodir",
          boost::llfio::file_flags::create))).front());

      // Schedule deleting the symbolic link only after when it has been created
      auto rmlink(dispatcher->close(std::vector<future<>>(1, dispatcher->rmsymlink(mklink))).front());
      // Schedule deleting the file only after when it has been created
      auto rmfile(dispatcher->close(std::vector<future<>>(1, dispatcher->rmfile(mkfile))).front());
      // Schedule waiting until both the preceding operations have finished
      auto barrier(dispatcher->barrier({rmlink, rmfile}));
      // Schedule deleting the directory only after the barrier completes
      auto rmdir(dispatcher->rmdir(std::vector<path_req>(1, dispatcher->depends(barrier.front(), mkdir))).front());
      // Check ops for errors
      boost::llfio::when_all_p(mkdir, mkfile, mklink, rmlink, rmfile, rmdir).wait();
  }
  catch(...)
  {
      std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
      throw;
  }
  //]
}
