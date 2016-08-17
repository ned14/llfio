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

  // Create a temporary file and put some text into it
  auto temph = file_handle::file("tempfile", file_handle::mode::write, file_handle::creation::if_needed).get();
  temph.write(0, "niall is not here", 17).get();
  auto boundf = [&](auto... pars) { return f(temph, pars...); };

  // clang-format off
  static const auto permuter(mt_permute_parameters<
    result<void>,                               
    parameters<                              
      typename section_handle::extent_type,
      typename section_handle::flag
    >,
    precondition::filesystem_setup_parameters
  >(
    {
      // Does the mode parameter have the expected side effects?
      {   make_ready_result<void>(), { 1, section_handle::flag::read }, { "_" } },
    },
    precondition::filesystem_setup()
    ));
  // clang-format on

  auto results = permuter(boundf);
  check_results_with_boost_test(permuter, results);
}

BOOST_KERNELTEST_TEST_KERNEL(integration, afio, section_handle_create_close, section_handle, "Tests that afio::section_handle's creation parameters work as expected", section_handle_create_close_(section_handle_create_close::test_kernel_section_handle))
