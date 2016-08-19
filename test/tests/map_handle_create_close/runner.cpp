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
      { make_ready_result<void>(),{ 1, section_handle::flag::none },{ "_" },{ false } },
      { make_ready_result<void>(),{ 1, section_handle::flag::read },{ "_" },{ false } },
      { make_ready_result<void>(),{ 1, section_handle::flag::write },{ "_" },{ false } },
      { make_ready_result<void>(),{ 1, section_handle::flag::cow },{ "_" },{ false } },
      //{ make_ready_result<void>(),{ 1, section_handle::flag::execute },{ "_" },{ false } },
      { make_ready_result<void>(),{ 1, section_handle::flag::write|section_handle::flag::nocommit },{ "_" },{ false } },
      { make_ready_result<void>(),{ 1, section_handle::flag::write|section_handle::flag::prefault },{ "_" },{ false } },
      //{ make_ready_result<void>(),{ 1, section_handle::flag::write|section_handle::flag::executable },{ "_" },{ false } },

      { make_ready_result<void>(),{ 1, section_handle::flag::none },{ "_" },{ true } },
      { make_ready_result<void>(),{ 1, section_handle::flag::read },{ "_" },{ true } },
      { make_ready_result<void>(),{ 1, section_handle::flag::write },{ "_" },{ true } },
      { make_ready_result<void>(),{ 1, section_handle::flag::cow },{ "_" },{ true } },
      //{ make_ready_result<void>(),{ 1, section_handle::flag::execute },{ "_" },{ true } },
      { make_ready_result<void>(),{ 1, section_handle::flag::write | section_handle::flag::nocommit },{ "_" },{ true } },
      { make_ready_result<void>(),{ 1, section_handle::flag::write | section_handle::flag::prefault },{ "_" },{ true } },
      //{ make_ready_result<void>(),{ 1, section_handle::flag::write|section_handle::flag::executable },{ "_" },{ true } },
  },
    precondition::filesystem_setup(),
    postcondition::custom(
      [&](auto &permuter, auto &testreturn, size_t idx, int use_file_backing) {
        if (use_file_backing)
        {
          temph = file_handle::file("tempfile", file_handle::mode::write, file_handle::creation::if_needed).get();
          temph.write(0, "I am some file data", 19).get();
        }
        else
          temph = file_handle();
        return std::make_tuple(std::ref(permuter), std::ref(testreturn), idx, use_file_backing);
      },
      [&](auto tuplestate) {
        //auto &permuter = std::get<0>(tuplestate);
        auto &testreturn = std::get<1>(tuplestate);
        //size_t idx = std::get<2>(tuplestate);
        int use_file_backing = std::get<3>(tuplestate);
        if (testreturn)
        {
          map_handle &h = testreturn.get();
          char *addr = h.address();
          BOOST_KERNELTEST_CHECK(testreturn, h.length() > 0);
          BOOST_KERNELTEST_CHECK(testreturn, addr != nullptr);
          if (use_file_backing)
          {
            BOOST_KERNELTEST_CHECK(testreturn, !memcmp(addr, "I am some file data", 19));
          }
        }
        // Need to close the map and any backing file as otherwise filesystem_setup won't be able to clear up the working dir on Windows
        if(testreturn)
          testreturn.get().close();
        temph.close();
      },
    "check mmap")
  ));
  // clang-format on

  auto results = permuter(boundf);
  check_results_with_boost_test(permuter, results);
}

BOOST_KERNELTEST_TEST_KERNEL(integration, afio, map_handle_create_close, map_handle, "Tests that afio::map_handle's creation parameters work as expected", map_handle_create_close_(map_handle_create_close::test_kernel_map_handle))
