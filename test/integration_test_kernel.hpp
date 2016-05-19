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
#include <unordered_map>

// TODO: Outcome's config.hpp should bind this
BOOST_OUTCOME_V1_NAMESPACE_BEGIN
namespace stl1z
{
  namespace filesystem = std::experimental::filesystem;
  struct path_hasher
  {
    size_t operator()(const filesystem::path &p) const { return hash_value(p.native()); }
  };
}
BOOST_OUTCOME_V1_NAMESPACE_END

BOOST_OUTCOME_V1_NAMESPACE_BEGIN
namespace console_colours
{
#ifdef _WIN32
  namespace detail
  {
    inline bool &am_in_bold()
    {
      static bool v;
      return v;
    }
    inline void set(WORD v)
    {
      if(am_in_bold())
        v |= FOREGROUND_INTENSITY;
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), v);
    }
  }
  inline std::ostream &red(std::ostream &s)
  {
    s.flush();
    detail::set(FOREGROUND_RED);
    return s;
  }
  inline std::ostream &green(std::ostream &s)
  {
    s.flush();
    detail::set(FOREGROUND_GREEN);
    return s;
  }
  inline std::ostream &blue(std::ostream &s)
  {
    s.flush();
    detail::set(FOREGROUND_BLUE);
    return s;
  }
  inline std::ostream &yellow(std::ostream &s)
  {
    s.flush();
    detail::set(FOREGROUND_RED | FOREGROUND_GREEN);
    return s;
  }
  inline std::ostream &magenta(std::ostream &s)
  {
    s.flush();
    detail::set(FOREGROUND_RED | FOREGROUND_BLUE);
    return s;
  }
  inline std::ostream &cyan(std::ostream &s)
  {
    s.flush();
    detail::set(FOREGROUND_GREEN | FOREGROUND_BLUE);
    return s;
  }
  inline std::ostream &white(std::ostream &s)
  {
    s.flush();
    detail::set(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    return s;
  }
  inline std::ostream &bold(std::ostream &s)
  {
    detail::am_in_bold() = true;
    return s;
  }
  inline std::ostream &normal(std::ostream &s)
  {
    detail::am_in_bold() = false;
    return white(s);
  }
#else
  constexpr const char red[] = {0x1b, '[', '3', '1', 'm', 0};
  constexpr const char green[] = {0x1b, '[', '3', '2', 'm', 0};
  constexpr const char blue[] = {0x1b, '[', '3', '4', 'm', 0};
  constexpr const char yellow[] = {0x1b, '[', '3', '3', 'm', 0};
  constexpr const char magenta[] = {0x1b, '[', '3', '5', 'm', 0};
  constexpr const char cyan[] = {0x1b, '[', '3', '6', 'm', 0};
  constexpr const char white[] = {0x1b, '[', '3', '7', 'm', 0};
  constexpr const char bold[] = {0x1b, '[', '1', 'm', 0};
  constexpr const char normal[] = {0x1b, '[', '0', 'm', 0};
#endif
}
namespace integration_test
{
  template <class T> inline void print_result(bool v, const T &result)
  {
    using namespace console_colours;
    if(v)
      std::cout << bold << green << result << normal << std::endl;
    else
      std::cout << bold << red << "FAILED" << normal << std::endl;
  }
}
BOOST_OUTCOME_V1_NAMESPACE_END


#define BOOST_OUTCOME_INTEGRATION_TEST_KERNEL(suite, name, desc, ...)                                                                                                                                                                                                                                                          \
  \
BOOST_AUTO_TEST_CASE(name, desc)                                                                                                                                                                                                                                                                                               \
  {                                                                                                                                                                                                                                                                                                                            \
    \
static constexpr const char __integration_test_kernel_suite[] = #suite;                                                                                                                                                                                                                                                        \
    \
static constexpr const char __integration_test_kernel_name[] = #name;                                                                                                                                                                                                                                                          \
    \
static constexpr const char __integration_test_kernel_description[] = desc;                                                                                                                                                                                                                                                    \
    using namespace BOOST_OUTCOME_V1_NAMESPACE;                                                                                                                                                                                                                                                                                \
    \
std::cout                                                                                                                                                                                                                                                                                                                      \
    << "\n\n"                                                                                                                                                                                                                                                                                                                  \
    << console_colours::bold << console_colours::blue << __integration_test_kernel_suite << " / " << __integration_test_kernel_name << ":\n"                                                                                                                                                                                   \
    << console_colours::bold << console_colours::white << desc << console_colours::normal << std::endl;                                                                                                                                                                                                                        \
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
        afiodir = stl1z::filesystem::canonical(afiodir / "..");
      } while(afiodir.native().size() > 3);
      std::cerr << "FATAL: Couldn't figure out where the test workspace templates live for test " << test_name << ". You need a boost.afio or afio directory somewhere in or above the directory you run the tests from." << std::endl;
      std::terminate();
    }

  private:
    void _remove_workspace() noexcept
    {
      stl11::error_code ec;
      auto begin = stl11::chrono::steady_clock::now();
      do
      {
        bool exists = stl1z::filesystem::exists(_current, ec);
        if(!ec && !exists)
          return;
        stl1z::filesystem::remove_all(_current, ec);
      } while(stl11::chrono::duration_cast<stl11::chrono::seconds>(stl11::chrono::steady_clock::now() - begin).count() < 5);
      std::cerr << "FATAL: Couldn't delete " << _current << " due to " << ec.message() << " after five seconds of trying." << std::endl;
      std::terminate();
    }
    void _setup_workspace() noexcept
    {
      stl11::error_code ec;
      // Is the input workspace no workspace?
      bool exists = stl1z::filesystem::exists(_before, ec);
      if(ec)
        goto fatalexit;
      if(!exists)
      {
        stl1z::filesystem::create_directory(_current, ec);
        if(ec)
          goto fatalexit;
        return;
      }
      {
        auto begin = stl11::chrono::steady_clock::now();
        do
        {
          stl1z::filesystem::copy(_before, _current, stl1z::filesystem::copy_options::recursive, ec);
          if(!ec)
            return;
        } while(stl11::chrono::duration_cast<stl11::chrono::seconds>(stl11::chrono::steady_clock::now() - begin).count() < 5);
      }
    fatalexit:
      std::cerr << "FATAL: Couldn't copy " << _before << " to " << _current << " due to " << ec.message() << " after five seconds of trying." << std::endl;
      std::terminate();
    }
    // We use a depth first strategy. f(directory_entry) can return something to early exit.
    template <class U> static auto _walk(stl1z::filesystem::path path, U &&f) -> decltype(f(std::declval<stl1z::filesystem::directory_entry>()))
    {
      for(stl1z::filesystem::directory_iterator it(path); it != stl1z::filesystem::directory_iterator(); ++it)
      {
        if(stl1z::filesystem::is_directory(it->status()))
        {
          auto ret(_walk(it->path(), std::forward<U>(f)));
          if(ret)
            return ret;
        }
      }
      for(stl1z::filesystem::directory_iterator it(path); it != stl1z::filesystem::directory_iterator(); ++it)
      {
        if(!stl1z::filesystem::is_directory(it->status()))
        {
          auto ret(f(*it));
          if(ret)
            return ret;
        }
      }
      // Return default constructed edition of the type returned by the callable
      return decltype(f(std::declval<stl1z::filesystem::directory_entry>()))();
    }
    // We only compare location, names and sizes. Other metadata like timestamps or perms not compared.
    // Returns empty result if identical, else path of first differing item
    result<stl1z::filesystem::path> _compare_workspace() const noexcept
    {
      // Make list of everything in _after
      std::unordered_map<stl1z::filesystem::path, stl1z::filesystem::directory_entry, stl1z::path_hasher> _after_items;
      _walk(_after, [&](stl1z::filesystem::directory_entry dirent) -> int {
        _after_items[dirent.path()] = std::move(dirent);
        return 0;
      });

      // We need to remove each item as we check, if anything remains we fail
      result<stl1z::filesystem::path> ret = _walk(_current, [&](stl1z::filesystem::directory_entry dirent) -> result<stl1z::filesystem::path> {
        stl1z::filesystem::path leafpath(dirent.path().native().substr(_current.native().size() + 1));
        stl1z::filesystem::path afterpath(_after / leafpath);
        if(stl1z::filesystem::is_symlink(dirent.symlink_status()) != stl1z::filesystem::is_symlink(stl1z::filesystem::symlink_status(afterpath)))
          goto differs;
        {
          auto beforestatus = dirent.status(), afterstatus = _after_items[afterpath].status();
          if(stl1z::filesystem::is_directory(beforestatus) != stl1z::filesystem::is_directory(afterstatus))
            goto differs;
          if(stl1z::filesystem::is_regular_file(beforestatus) != stl1z::filesystem::is_regular_file(afterstatus))
            goto differs;
        }
        // This item is identical
        _after_items.erase(afterpath);
        return make_empty_result<stl1z::filesystem::path>();
      differs:
        return leafpath;
      });
      // If anything different, return that
      if(ret)
        return ret;
      // If anything in after not in current, return that
      if(!_after_items.empty())
        return _after_items.begin()->first;
      // Otherwise both current and after are identical
      return make_empty_result<stl1z::filesystem::path>();
    }

  public:
    template <class ParamType> filesystem_workspace(const char *test_name, const ParamType &param, size_t no, size_t total)
    {
      auto template_path = workspace_template_path(test_name);
      _before = template_path / param.before;
      _after = template_path / param.after;
      _current = starting_path() / "workspace";
      _remove_workspace();
      _setup_workspace();
      stl1z::filesystem::current_path(_current);
      using namespace console_colours;
      std::cout << std::endl
                << yellow << (no + 1) << "/" << total << ":" << normal << " Running filesystem integration test kernel "       //
                << magenta << test_name << normal << " to see if input parameter " << cyan << param.parameter_value << normal  //
                << " causes input workspace " << bold << white << param.before << normal << " to become output workspace "     //
                << bold << white << param.after << normal << std::endl
                << "         Test kernel execution: " << std::flush;
    }

    ~filesystem_workspace()
    {
      std::cout << "   Test file system comparison: " << std::flush;
      result<stl1z::filesystem::path> workspaces_not_identical = _compare_workspace();
      print_result(!workspaces_not_identical, "MATCHES");
      BOOST_CHECK(!workspaces_not_identical);
      if(workspaces_not_identical.has_error())
        std::cout << "NOTE: Filesystem workspace comparison failed due to " << workspaces_not_identical.get_error().message() << std::endl;
      if(workspaces_not_identical.has_value())
        std::cout << "NOTE: Filesystem workspace comparison failed because item " << workspaces_not_identical.get() << " is not identical" << std::endl;
      stl1z::filesystem::current_path(starting_path());
      _remove_workspace();
    }
  };
}
BOOST_OUTCOME_V1_NAMESPACE_END

#define BOOST_OUTCOME_INTEGRATION_TEST_REMOVE_BRACKETS(...) __VA_ARGS__

// outcomes has format { parvalue, dirbefore, outcome, dirafter }
#define BOOST_OUTCOME_INTEGRATION_TEST_ST_KERNEL_PARAMETER_TO_FILESYSTEM(__outcometype, __param, __testdir, __outcomes_initialiser, ...)                                                                                                                                                                                       \
  \
static const BOOST_OUTCOME_V1_NAMESPACE::integration_test::parameters_type<decltype(__param), BOOST_OUTCOME_INTEGRATION_TEST_REMOVE_BRACKETS __outcometype>                                                                                                                                                                    \
  __outcomes = BOOST_OUTCOME_INTEGRATION_TEST_REMOVE_BRACKETS __outcomes_initialiser;                                                                                                                                                                                                                                          \
  \
size_t __no = 0;                                                                                                                                                                                                                                                                                                               \
  \
for(const auto &__outcome                                                                                                                                                                                                                                                                                                      \
    : __outcomes)                                                                                                                                                                                                                                                                                                              \
  \
{                                                                                                                                                                                                                                                                                                                         \
    \
BOOST_OUTCOME_V1_NAMESPACE::integration_test::filesystem_workspace __workspace((__testdir), __outcome, __no++, __outcomes.size());                                                                                                                                                                                             \
    \
(__param) = __outcome.parameter_value;                                                                                                                                                                                                                                                                                         \
    \
__VA_ARGS__                                                                                                                                                                                                                                                                                                             \
  \
}

#define BOOST_OUTCOME_INTEGRATION_TEST_MT_KERNEL_PARAMETER_TO_FILESYSTEM(__outcometype, __param, __testdir, __outcomes_initialiser, ...) BOOST_OUTCOME_INTEGRATION_TEST_ST_KERNEL_PARAMETER_TO_FILESYSTEM(__outcometype, __param, __testdir, __outcomes_initialiser, __VA_ARGS__)

BOOST_OUTCOME_V1_NAMESPACE_BEGIN namespace integration_test
{
  template <class T> void check_result(const outcome<T> &kernel_outcome, const outcome<T> &shouldbe)
  {
    print_result(kernel_outcome == shouldbe, kernel_outcome);
    BOOST_CHECK(kernel_outcome == shouldbe);
  };
  template <class T> void check_result(const result<T> &kernel_outcome, const result<T> &shouldbe)
  {
    print_result(kernel_outcome == shouldbe, kernel_outcome);
    BOOST_CHECK(kernel_outcome == shouldbe);
  };
  template <class T> void check_result(const option<T> &kernel_outcome, const option<T> &shouldbe)
  {
    print_result(kernel_outcome == shouldbe, kernel_outcome);
    BOOST_CHECK(kernel_outcome == shouldbe);
  };

  // If should be has type void, we only care kernel_outcome has a value
  template <class T> void check_result(const outcome<T> &kernel_outcome, const outcome<void> &shouldbe)
  {
    if(kernel_outcome.has_value() && shouldbe.has_value())
    {
      print_result(kernel_outcome.has_value() == shouldbe.has_value(), kernel_outcome);
      BOOST_CHECK(kernel_outcome.has_value() == shouldbe.has_value());
    }
    else
    {
      print_result(kernel_outcome == shouldbe, kernel_outcome);
      BOOST_CHECK(kernel_outcome == shouldbe);
    }
  };
  template <class T> void check_result(const result<T> &kernel_outcome, const result<void> &shouldbe)
  {
    if(kernel_outcome.has_value() && shouldbe.has_value())
    {
      print_result(kernel_outcome.has_value() == shouldbe.has_value(), kernel_outcome);
      BOOST_CHECK(kernel_outcome.has_value() == shouldbe.has_value());
    }
    else
    {
      print_result(kernel_outcome == shouldbe, kernel_outcome);
      BOOST_CHECK(kernel_outcome == shouldbe);
    }
  };
  template <class T> void check_result(const option<T> &kernel_outcome, const option<void> &shouldbe)
  {
    if(kernel_outcome.has_value() && shouldbe.has_value())
    {
      print_result(kernel_outcome.has_value() == shouldbe.has_value(), kernel_outcome);
      BOOST_CHECK(kernel_outcome.has_value() == shouldbe.has_value());
    }
    else
    {
      print_result(kernel_outcome == shouldbe, kernel_outcome);
      BOOST_CHECK(kernel_outcome == shouldbe);
    }
  };
}
BOOST_OUTCOME_V1_NAMESPACE_END

// If __outcome.outcome has type void, it means we don't care what the value is in the non-errored outcome
#define BOOST_OUTCOME_INTEGRATION_TEST_KERNEL_RESULT(value) BOOST_OUTCOME_V1_NAMESPACE::integration_test::check_result(value, __outcome.result);

#include "../include/boost/afio.hpp"
