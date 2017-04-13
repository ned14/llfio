/* Integration test kernel for map_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Aug 2016
*/

#include "../kerneltest/include/boost/kerneltest.hpp"
#include "kernel_map_handle.cpp.hpp"

template <class U> inline void map_handle_create_close_(U &&f)
{
  using namespace BOOST_KERNELTEST_V1_NAMESPACE;
  using namespace BOOST_AFIO_V2_NAMESPACE;
  using BOOST_AFIO_V2_NAMESPACE::file_handle;

  // Create a temporary file and put some text into it
  file_handle temph;
  auto boundf = [&](auto... pars) { return f(temph, pars...); };

  // clang-format off
  static const auto permuter(st_permute_parameters<
    result<void>,                               
    parameters<                              
      typename map_handle::size_type,
      typename section_handle::flag
    >,
    precondition::filesystem_setup_parameters,
    postcondition::custom_parameters<bool>
  >(
    {
      { make_valued_result<void>(),{ 1, section_handle::flag::none },{ "_" },{ false } },
      { make_valued_result<void>(),{ 1, section_handle::flag::read },{ "_" },{ false } },
      { make_valued_result<void>(),{ 1, section_handle::flag::write },{ "_" },{ false } },
      { make_valued_result<void>(),{ 1, section_handle::flag::cow },{ "_" },{ false } },
      //{ make_valued_result<void>(),{ 1, section_handle::flag::execute },{ "_" },{ false } },
      { make_valued_result<void>(),{ 1, section_handle::flag::write|section_handle::flag::nocommit },{ "_" },{ false } },
      { make_valued_result<void>(),{ 1, section_handle::flag::write|section_handle::flag::prefault },{ "_" },{ false } },
      //{ make_valued_result<void>(),{ 1, section_handle::flag::write|section_handle::flag::executable },{ "_" },{ false } },

      { make_valued_result<void>(),{ 1, section_handle::flag::none },{ "_" },{ true } },
      { make_valued_result<void>(),{ 1, section_handle::flag::read },{ "_" },{ true } },
      { make_valued_result<void>(),{ 1, section_handle::flag::write },{ "_" },{ true } },
      { make_valued_result<void>(),{ 1, section_handle::flag::cow },{ "_" },{ true } },
      //{ make_valued_result<void>(),{ 1, section_handle::flag::execute },{ "_" },{ true } },
      { make_valued_result<void>(),{ 1, section_handle::flag::write | section_handle::flag::nocommit },{ "_" },{ true } },
      { make_valued_result<void>(),{ 1, section_handle::flag::write | section_handle::flag::prefault },{ "_" },{ true } },
      //{ make_valued_result<void>(),{ 1, section_handle::flag::write|section_handle::flag::executable },{ "_" },{ true } },
  },
    precondition::filesystem_setup(),
    postcondition::custom(
      [&](auto &permuter, auto &testreturn, size_t idx, int use_file_backing) {
        if (use_file_backing)
        {
          // Create a temporary backing file
          temph = file_handle::file("tempfile", file_handle::mode::write, file_handle::creation::if_needed).get();
          temph.write(0, "I am some file data", 19).get();
        }
        else
          // Use the page file
          temph = file_handle();
        return std::make_tuple(std::ref(permuter), std::ref(testreturn), idx, use_file_backing);
      },
      [&](auto tuplestate) {
        auto &permuter = std::get<0>(tuplestate);
        auto &testreturn = std::get<1>(tuplestate);
        size_t idx = std::get<2>(tuplestate);
        int use_file_backing = std::get<3>(tuplestate);
        map_handle maph;
        if (testreturn)
          maph = std::move(testreturn.get());
        // Need to close the map and any backing file as otherwise filesystem_setup won't be able to clear up the working dir on Windows
        auto onexit = BOOST_AFIO_V2_NAMESPACE::undoer([&]{
          maph.close();
          temph.close();
        });
        if (testreturn)
        {
          char *addr = maph.address();
          section_handle::flag flags = std::get<1>(std::get<1>(permuter[idx]));
          BOOST_KERNELTEST_CHECK(testreturn, maph.length() > 0);
          BOOST_KERNELTEST_CHECK(testreturn, addr != nullptr);
          if (!(flags & section_handle::flag::nocommit))
          {
            char buffer[64];
            // Make sure the backing file is appearing in the map
            if (use_file_backing && maph.is_readable())
            {
              BOOST_KERNELTEST_CHECK(testreturn, !memcmp(addr, "I am some file data", 19));
            }
            // Make sure maph's read() does what it is supposed to
            {
              auto b = maph.read(0, nullptr, 20).get();
              BOOST_KERNELTEST_CHECK(testreturn, b.first == addr);
              BOOST_KERNELTEST_CHECK(testreturn, b.second == 20);
            }
            {
              auto b = maph.read(5, nullptr, 5000).get();
              BOOST_KERNELTEST_CHECK(testreturn, b.first == addr+5);
              BOOST_KERNELTEST_CHECK(testreturn, b.second == 4091);
            }
            // If we are writable, write straight into the map
            if (maph.is_writable() && addr)
            {
              memcpy(addr, "NIALL was here", 14);
              // Make sure maph's write() works
              maph.write(1, "iall", 4);
              // Make sure data written to the map turns up in the file
              if (use_file_backing)
              {
                temph.read(0, buffer, 64);
                if (flags & section_handle::flag::cow)
                {
                  BOOST_KERNELTEST_CHECK(testreturn, !memcmp(buffer, "I am some file data", 19));
                }
                else
                {
                  BOOST_KERNELTEST_CHECK(testreturn, !memcmp(buffer, "Niall was here data", 19));
                }
              }
            }
            // The OS should not auto expand storage to 4Kb
            if (use_file_backing)
            {
              BOOST_KERNELTEST_CHECK(testreturn, temph.length().get() == 19);
            }
          }
        }
      },
    "check mmap")
  ));
  // clang-format on

  auto results = permuter(boundf);
  check_results_with_boost_test(permuter, results);
}

BOOST_KERNELTEST_TEST_KERNEL(unit, afio, map_handle_create_close, map_handle, "Tests that afio::map_handle's creation parameters work as expected", map_handle_create_close_(map_handle_create_close::test_kernel_map_handle))
