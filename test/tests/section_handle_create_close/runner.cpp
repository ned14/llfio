/* Integration test kernel for section_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Aug 2016
*/

#include "../kerneltest/include/boost/kerneltest.hpp"
#include "kernel_section_handle.cpp.hpp"

template <class U> inline void section_handle_create_close_(U &&f)
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
      typename section_handle::extent_type,
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
      { make_valued_result<void>(),{ 1, section_handle::flag::execute },{ "_" },{ false } },
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
      [&](auto &, auto &testreturn, size_t, int use_file_backing) {
        if (use_file_backing)
        {
          temph = file_handle::file("tempfile", file_handle::mode::write, file_handle::creation::if_needed).get();
          temph.write(0, "niall is not here", 17).get();
        }
        else
          temph = file_handle();
        return &testreturn;
      },
      [&](auto *testreturn) {
        // Need to close the section and any backing file as otherwise filesystem_setup won't be able to clear up the working dir
        if (*testreturn)
          testreturn->get().close();
        temph.close();
      },
    "check section")
  ));
  // clang-format on

  auto results = permuter(boundf);
  check_results_with_boost_test(permuter, results);
}

BOOST_KERNELTEST_TEST_KERNEL(unit, afio, section_handle_create_close, section_handle, "Tests that afio::section_handle's creation parameters work as expected", section_handle_create_close_(section_handle_create_close::test_kernel_section_handle))
