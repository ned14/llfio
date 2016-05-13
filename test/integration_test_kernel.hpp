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

  /* Sets up a workspace directory for the test to run inside and checks it is correct after
  The working directory on process start is assumed to be the correct place to put test workspaces.
  */
  class filesystem_workspace
  {
    stl1z::filesystem::path _before, _after, _current;

  public:
    // Record the current working directory and store it
    static const stl1z::filesystem::path &starting_path()
    {
      static stl1z::filesystem::path p = stl1z::filesystem::current_path();
      return p;
    }

  private:
    stl1z::filesystem::path _has_afio(stl1z::filesystem::path dir)
    {
      if(stl1z::filesystem::exists(dir / "boost.afio"))
        return dir / "boost.afio";
      if(stl1z::filesystem::exists(dir / "afio"))
        return dir / "afio";
      return stl1z::filesystem::path();
    }

  public:
    // Figure out an absolute path to the correct test workspace templates
    stl1z::filesystem::path workspace_template_path(const char *test_name)
    {
      // Layout is <boost.afio>/test/tests/<test_name>/<workspace_templates>
      stl1z::filesystem::path afiodir = starting_path(), temp;
      do
      {
        temp = _has_afio(afiodir);
        if(!temp.empty() && stl1z::filesystem::exists(temp / "test" / "tests" / test_name))
        {
          return temp / "test" / "tests" / test_name;
        }
        afiodir = stl1z::filesystem::absolute(afiodir / "..");
      } while(afiodir.native().size() > 3);
      std::cerr << "FATAL: Couldn't figure out where the test workspace templates live for test " << test_name << ". You need a boost.afio or afio directory somewhere in or above the directory you run the tests from." << std::endl;
      std::terminate();
    }

  private:
    void _remove_workspace()
    {
      if(!stl1z::filesystem::exists(_current))
        return;
      auto begin = stl11::chrono::steady_clock::now();
      stl11::error_code ec;
      do
      {
        if(!stl1z::filesystem::exists(_current))
          return;
        stl1z::filesystem::remove_all(_current, ec);
      } while(stl11::chrono::duration_cast<stl11::chrono::seconds>(stl11::chrono::steady_clock::now() - begin).count() < 5);
      std::cerr << "FATAL: Couldn't delete " << _current << " after five seconds of trying." << std::endl;
      std::terminate();
    }
    void _setup_workspace()
    {
      if(!stl1z::filesystem::exists(_before))
      {
        stl1z::filesystem::create_directory(_current);
        return;
      }
      auto begin = stl11::chrono::steady_clock::now();
      stl11::error_code ec;
      do
      {
        stl1z::filesystem::copy(_before, _current, stl1z::filesystem::copy_options::recursive, ec);
        if(!ec)
          return;
      } while(stl11::chrono::duration_cast<stl11::chrono::seconds>(stl11::chrono::steady_clock::now() - begin).count() < 5);
      std::cerr << "FATAL: Couldn't copy " << _before << " to " << _current << " after five seconds of trying." << std::endl;
      std::terminate();
    }
    // We use a depth first strategy. f(directory_entry) can return something to early exit.
    template <class U> static auto _walk(stl1z::filesystem::path path, U &&f) -> decltype(f(std::declval<stl1z::filesystem::directory_entry>()))
    {
      for(stl1z::filesystem::directory_iterator it(path); it != stl1z::filesystem::directory_iterator(); ++it)
      {
        if(stl1z::filesystem::is_directory(it->status()))
        {
          auto ret = _walk(it->path(), std::forward<U>(f));
          if(ret)
            return ret;
        }
      }
      for(stl1z::filesystem::directory_iterator it(path); it != stl1z::filesystem::directory_iterator(); ++it)
      {
        if(!stl1z::filesystem::is_directory(it->status()))
        {
          auto ret = f(*it);
          if(ret)
            return ret;
        }
      }
      return std::declval<decltype(f(std::declval<stl1z::filesystem::directory_entry>()))>();
    }
    // We only compare location, names and sizes. Other metadata like timestamps or perms not compared.
    option<stl1z::filesystem::path> _compare_workspace()
    {
      // TODO FIXME: Need list of everything in _after first
      // We need to remove each item as we check, if anything remains we fail
      return _walk(_current, [this](stl1z::filesystem::directory_entry dirent) -> option<stl1z::filesystem::path> {
        stl1z::filesystem::path leafpath(dirent.path().native().substr(_current.native().size()));
        stl1z::filesystem::path afterpath(_after / leafpath);
        if(stl1z::filesystem::is_symlink(dirent.symlink_status()) != stl1z::filesystem::is_symlink(stl1z::filesystem::symlink_status(afterpath)))
          return leafpath;
        auto beforestatus = dirent.status(), afterstatus = stl1z::filesystem::status(afterpath);
        if(stl1z::filesystem::is_directory(beforestatus) != stl1z::filesystem::is_directory(afterstatus))
          return leafpath;
        if(stl1z::filesystem::is_regular_file(beforestatus) != stl1z::filesystem::is_regular_file(afterstatus))
          return leafpath;
        return option<stl1z::filesystem::path>();
      });
    }

  public:
    filesystem_workspace(const char *test_name, const stl1z::filesystem::path &before, const stl1z::filesystem::path &after)
    {
      auto template_path = workspace_template_path(test_name);
      _before = template_path / before;
      _after = template_path / after;
      _current = starting_path() / "workspace";
      _remove_workspace();
      _setup_workspace();
    }
    ~filesystem_workspace()
    {
      option<stl1z::filesystem::path> workspaces_not_identical = _compare_workspace();
      BOOST_CHECK(!workspaces_not_identical);
      BOOST_WARN_MESSAGE(!workspaces_not_identical, "Item " << workspaces_not_identical.get() << " is not identical");
      _remove_workspace();
    }
  };
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
