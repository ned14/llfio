#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_zero, "Tests async range content zeroing of sparse and compressed files", 60)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    std::vector<char> buffer(1024*1024, 'n');
    ranctx ctx; raninit(&ctx, 1);
    u4 *buf=(u4 *) buffer.data();
    for(size_t n=0; n<buffer.size()/sizeof(*buf); n++)
      buf[n]=ranval(&ctx);
    auto dispatcher = make_dispatcher().get();
    std::cout << "\n\nTesting async range content zeroing of sparse and compressed files:\n";
    {
      bool compressed_file_support = true;
      // Create a 1Mb file
      auto mkdir(dispatcher->dir(path_req("testdir", file_flags::create)));
      auto mkfilesp(dispatcher->file(path_req::relative(mkdir, "sparse", file_flags::create | file_flags::read_write)));
      auto mkfilec(dispatcher->file(path_req::relative(mkdir, "compressed", file_flags::create | file_flags::create_compressed | file_flags::read_write)));
      // FAT32 or ReFS will error out on compressed file creation
      mkfilec.wait();
      if (mkfilec.has_error())
      {
        mkfilec = future<>();
        compressed_file_support = false;
      }
      auto resizefilesp(dispatcher->truncate(mkfilesp, buffer.size()));
      auto resizefilec(compressed_file_support ? dispatcher->truncate(mkfilec, buffer.size()) : mkfilec);
      auto writefilesp(dispatcher->write(make_io_req(resizefilesp, buffer, 0)));
      auto writefilec(compressed_file_support ? dispatcher->write(make_io_req(resizefilec, buffer, 0)) : resizefilec);
      // Need to fsync to work around lazy or delayed allocation
      auto syncfilesp1(dispatcher->sync(writefilesp));
      auto syncfilec1(compressed_file_support ? dispatcher->sync(writefilec) : writefilec);
      BOOST_REQUIRE_NO_THROW(when_all_p(mkdir, mkfilesp, resizefilesp, writefilesp, syncfilesp1).get());
      if(compressed_file_support)
        BOOST_REQUIRE_NO_THROW(when_all_p(mkfilec, resizefilec, writefilec, syncfilec1).get());

      // Verify they really does consume 1Mb of disc space
      stat_t beforezerostatsp=mkfilesp->lstat(metadata_flags::All);
      stat_t beforezerostatc= compressed_file_support ? mkfilec->lstat(metadata_flags::All) : stat_t();
      BOOST_REQUIRE(beforezerostatsp.st_size==buffer.size());
      if(compressed_file_support) BOOST_REQUIRE(beforezerostatc.st_size==buffer.size());
      if(beforezerostatsp.st_allocated<buffer.size())
      {
        BOOST_WARN_MESSAGE(false, "The sparse file allocation is smaller than expected, is this file system compressed?");
        std::cout << "WARNING: The sparse file allocation is smaller than expected, is this file system compressed? allocated=" << beforezerostatsp.st_allocated << "." << std::endl;
      }
      if(compressed_file_support && beforezerostatc.st_compressed)
      {
        std::cout << "The compressed file consumes " << beforezerostatc.st_allocated << " bytes on disc for " << beforezerostatc.st_size << " bytes." << std::endl;
      }
      else
      {
        BOOST_WARN_MESSAGE(false, "File isn't marked as compressed, assuming no filing system support for per-file compression.");
        std::cout << "File isn't marked as compressed, assuming no filing system support for per-file compression." << std::endl;
      }
      
      // Punch a 256Mb hole into the front and make sure it's definitely zero, again fsyncing to workaround delayed deallocation
      std::vector<char> buffer2(buffer.size()/4, 'n');
      auto punchholesp(dispatcher->zero(writefilesp, {{0, buffer2.size()}, {buffer.size()/2, buffer2.size()}}));
      auto syncfilesp2(dispatcher->sync(punchholesp));
      auto readfilesp1(dispatcher->read(make_io_req(syncfilesp2, buffer2, 0)));
      BOOST_CHECK_NO_THROW(when_all_p(punchholesp, syncfilesp2, readfilesp1).get());
      bool allzero=true;
      for(auto &i : buffer2)
        if(i) { allzero=false; break; }
      BOOST_CHECK(allzero);
      auto punchholec(compressed_file_support ? dispatcher->zero(writefilec, {{0, buffer2.size()}, {buffer.size()/2, buffer2.size()}}) : writefilec);
      auto syncfilec2(compressed_file_support ? dispatcher->sync(punchholec) : punchholec);
      auto readfilec1(compressed_file_support ? dispatcher->read(make_io_req(syncfilec2, buffer2, 0)) : syncfilec2);
      if(compressed_file_support)
        BOOST_CHECK_NO_THROW(when_all_p(punchholec, syncfilec2, readfilec1).get());
      allzero = true;
      for(auto &i : buffer2)
        if(i) { allzero = false; break; }
      BOOST_CHECK(allzero);
      
      // Sleep for a second to let the filing system do delayed deallocation
      this_thread::sleep_for(chrono::seconds(1));

      // Verify they now consumes less than 1Mb of disc space
      stat_t afterzerostatsp=mkfilesp->lstat(metadata_flags::All);
      stat_t afterzerostatc=compressed_file_support ? mkfilec->lstat(metadata_flags::All) : stat_t();
      BOOST_REQUIRE(afterzerostatsp.st_size==buffer.size());
      if(compressed_file_support)
        BOOST_REQUIRE(afterzerostatc.st_size==buffer.size());
      std::cout << "The sparse file now consumes " << afterzerostatsp.st_allocated << " bytes on disc for " << afterzerostatsp.st_size << " bytes." << std::endl;
      if(compressed_file_support)
        std::cout << "The compressed file now consumes " << afterzerostatc.st_allocated << " bytes on disc for " << afterzerostatc.st_size << " bytes." << std::endl;
      if(afterzerostatsp.st_allocated==buffer.size())
      {
        BOOST_WARN_MESSAGE(false, "This filing system does not support sparse files, so skipping some tests.");
        std::cout << "This filing system does not support sparse files, so skipping some tests." << std::endl;
        BOOST_CHECK(!afterzerostatsp.st_sparse);
      }
      else if(beforezerostatsp.st_allocated==buffer.size())
      {
        BOOST_CHECK(afterzerostatsp.st_sparse);
        BOOST_CHECK((afterzerostatsp.st_allocated+2*buffer2.size())==beforezerostatsp.st_allocated);
        auto extentssp=dispatcher->extents(mkfilesp).get();
        // Tolerate systems which can't enumerate extents, one those a single file sized extent is returned
        BOOST_CHECK(!extentssp.empty());
        std::cout << "extents() reports " << extentssp.size() << " extents. These were:" << std::endl;
        for(auto &i : extentssp)
          std::cout << "  " << i.first << ", " << i.second << std::endl;
        BOOST_CHECK((extentssp.size()==2 || extentssp.size()==1));
        if(extentssp.size()==1)
        {
          BOOST_WARN_MESSAGE(false, "This filing system reports one extent where there should be two, skipping some tests.");
        }
        else if(extentssp.size()==2)
        {
          BOOST_CHECK(extentssp[0].first==buffer.size()/4);
          BOOST_CHECK(extentssp[0].second==buffer.size()/4);
          BOOST_CHECK(extentssp[1].first==buffer.size()/4*3);
          BOOST_CHECK(extentssp[1].second==buffer.size()/4);
        }
      }

      auto delfilesp(dispatcher->rmfile(readfilesp1));
      auto delfilec(compressed_file_support ? dispatcher->rmfile(readfilec1) : readfilec1);
      auto closefilesp=dispatcher->close(delfilesp);
      auto closefilec= compressed_file_support ? dispatcher->close(delfilec) : delfilec;
      BOOST_CHECK_NO_THROW(when_all_p(closefilesp, delfilesp).get());
      if(compressed_file_support)
        BOOST_CHECK_NO_THROW(when_all_p(closefilec, delfilec).get());
      auto deldir(dispatcher->rmdir(mkdir));
      BOOST_CHECK_NO_THROW(when_all_p(deldir).wait());  // virus checkers sometimes make this spuriously fail
    }
}