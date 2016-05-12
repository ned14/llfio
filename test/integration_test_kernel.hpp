/* Integration test kernel
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Apr 2016
*/

// TODO: This stuff will be moved into Boost.Outcome
#define BOOST_OUTCOME_ERROR_CODE_EXTENDED_CREATION_HOOK /* todo */
#include "../include/boost/afio/outcome/include/boost/outcome.hpp"
//#define BOOST_CATCH_CUSTOM_MAIN_DEFINED
#include "../include/boost/afio/outcome/include/boost/outcome/bindlib/include/boost/test/unit_test.hpp"

#include <filesystem>
// TODO: Outcome's config.hpp should bind this
BOOST_OUTCOME_V1_NAMESPACE_BEGIN
namespace stl1z
{
  namespace filesystem = std::experimental::filesystem;
}
BOOST_OUTCOME_V1_NAMESPACE_END


#define BOOST_OUTCOME_INTEGRATION_TEST_KERNEL(suite, name, desc, ...)                                                                                                                                                                                                                                                          \
  \
BOOST_AUTO_TEST_CASE(name, desc)                                                                                                                                                                                                                                                                                               \
  {                                                                                                                                                                                                                                                                                                                            \
    using namespace BOOST_OUTCOME_V1_NAMESPACE;                                                                                                                                                                                                                                                                                \
    \
__VA_ARGS__;                                                                                                                                                                                                                                                                                                                   \
  }

BOOST_OUTCOME_V1_NAMESPACE_BEGIN
namespace integration_test
{
  template <class T, class Outcome> struct kernel_parameter_to_filesystem
  {
    T parameter_value;
    stl1z::filesystem::path before;
    Outcome result;
    stl1z::filesystem::path after;
    kernel_parameter_to_filesystem(T v, stl1z::filesystem::path b, Outcome o, stl1z::filesystem::path a)
        : parameter_value(std::move(v))
        , before(std::move(b))
        , result(std::move(o))
        , after(std::move(a))
    {
    }
  };
  template <class T, class Outcome> using parameters_type = std::initializer_list<kernel_parameter_to_filesystem<T, Outcome>>;
}
BOOST_OUTCOME_V1_NAMESPACE_END

#define BOOST_OUTCOME_INTEGRATION_TEST_REMOVE_BRACKETS(...) __VA_ARGS__

// outcomes has format { parvalue, dirbefore, outcome<dirafter> }
#define BOOST_OUTCOME_INTEGRATION_TEST_ST_KERNEL_PARAMETER_TO_FILESYSTEM(__outcometype, __param, __outcomes_initialiser, ...)                                                                                                                                                                                                  \
  \
static const BOOST_OUTCOME_V1_NAMESPACE::integration_test::parameters_type<decltype(__param), __outcometype>                                                                                                                                                                                                                   \
  __outcomes = BOOST_OUTCOME_INTEGRATION_TEST_REMOVE_BRACKETS __outcomes_initialiser;                                                                                                                                                                                                                                          \
  \
for(const auto &__outcome                                                                                                                                                                                                                                                                                                      \
    : __outcomes)                                                                                                                                                                                                                                                                                                              \
  \
{                                                                                                                                                                                                                                                                                                                         \
    \
(__param) = __outcome.parameter_value;                                                                                                                                                                                                                                                                                         \
    __VA_ARGS__                                                                                                                                                                                                                                                                                                                \
  }

#define BOOST_OUTCOME_INTEGRATION_TEST_MT_KERNEL_PARAMETER_TO_FILESYSTEM(param, outcomes_initialiser, ...) BOOST_OUTCOME_INTEGRATION_TEST_ST_KERNEL_PARAMETER_TO_FILESYSTEM(param, outcomes_initialiser, __VA_ARGS__)

BOOST_OUTCOME_V1_NAMESPACE_BEGIN namespace integration_test
{
  template <class T> void check_result(const outcome<T> &value, const outcome<T> &shouldbe) { BOOST_CHECK(value == shouldbe); };
  template <class T> void check_result(const result<T> &value, const result<T> &shouldbe) { BOOST_CHECK(value == shouldbe); };
  template <class T> void check_result(const option<T> &value, const option<T> &shouldbe) { BOOST_CHECK(value == shouldbe); };

  // If should be has type void, we only care value has a value
  template <class T> void check_result(const outcome<T> &value, const outcome<void> &shouldbe)
  {
    if(value.has_value() && shouldbe.has_value())
      BOOST_CHECK(value.has_value() == shouldbe.has_value());
    else
      BOOST_CHECK(value == shouldbe);
  };
  template <class T> void check_result(const result<T> &value, const result<void> &shouldbe)
  {
    if(value.has_value() && shouldbe.has_value())
      BOOST_CHECK(value.has_value() == shouldbe.has_value());
    else
      BOOST_CHECK(value == shouldbe);
  };
  template <class T> void check_result(const option<T> &value, const option<void> &shouldbe)
  {
    if(value.has_value() && shouldbe.has_value())
      BOOST_CHECK(value.has_value() == shouldbe.has_value());
    else
      BOOST_CHECK(value == shouldbe);
  };
}
BOOST_OUTCOME_V1_NAMESPACE_END

// If __outcome.outcome has type void, it means we don't care what the value is in the non-errored outcome
#define BOOST_OUTCOME_INTEGRATION_TEST_KERNEL_RESULT(value) BOOST_OUTCOME_V1_NAMESPACE::integration_test::check_result(value, __outcome.result);

#include "../include/boost/afio.hpp"
