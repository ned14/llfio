//[workshop_naive_interface
namespace filesystem = BOOST_AFIO_V2_NAMESPACE::filesystem;
using BOOST_OUTCOME_V1_NAMESPACE::outcome;

class data_store
{
  filesystem::path _store_path;
  bool _writeable;
public:
  // Type used for read streams
  using istream = std::shared_ptr<std::istream>;
  // Type used for write streams
  using ostream = std::shared_ptr<std::ostream>;
  // Type used for lookup
  using lookup_result_type = outcome<istream>;
  // Type used for write
  using write_result_type = outcome<ostream>;

  // Disposition flags
  static constexpr size_t writeable = (1<<0);

  // Open a data store at path
  data_store(size_t flags = 0, filesystem::path path = "store");
  
  // Look up item named name for reading, returning a std::istream for the item if it exists
  outcome<istream> lookup(std::string name) noexcept;
  // Look up item named name for writing, returning an ostream for that item
  outcome<ostream> write(std::string name) noexcept;
};
//]

//[workshop_naive]
using BOOST_OUTCOME_V1_NAMESPACE::empty;
using BOOST_AFIO_V2_NAMESPACE::error_code;
using BOOST_AFIO_V2_NAMESPACE::generic_category;

static bool is_valid_name(const std::string &name) noexcept
{
  static const std::string banned("<>:\"/\\|?*\0", 10);
  if(std::string::npos!=name.find_first_of(banned))
    return false;
  // No leading period
  return name[0]!='.';
}

static std::shared_ptr<std::fstream> name_to_fstream(const filesystem::path &store_path, std::string name, std::ios::openmode mode)
{
  auto to_lookup(store_path / name);
  return std::make_shared<std::fstream>(to_lookup.native(), mode);
}

data_store::data_store(size_t flags, filesystem::path path) : _store_path(std::move(path)), _writeable(flags & writeable)
{
}

outcome<data_store::istream> data_store::lookup(std::string name) noexcept
{
  if(!is_valid_name(name))
    return error_code(EINVAL, generic_category());
  try
  {
    istream ret(name_to_fstream(_store_path, std::move(name), std::ios::in | std::ios::binary));
    if(!ret->fail())
      return std::move(ret);
    return empty;
  }
  catch(...)
  {
    return std::current_exception();
  }
}

outcome<data_store::ostream> data_store::write(std::string name) noexcept
{
  if(!_writeable)
    return error_code(EROFS, generic_category());
  if(!is_valid_name(name))
    return error_code(EINVAL, generic_category());
  try
  {
    if (!filesystem::exists(_store_path))
      filesystem::create_directory(_store_path);
    ostream ret(name_to_fstream(_store_path, std::move(name), std::ios::out | std::ios::binary));
    return std::move(ret);
  }
  catch (...)
  {
    return std::current_exception();
  }
}
//]

int main(void)
{
  //[workshop_use_naive]
  // To write a key-value item called "dog"
  {
    data_store ds;
    auto dogh = ds.write("dog").get();
    auto &dogs = *dogh;
    dogs << "I am a dog";
  }

  // To retrieve a key-value item called "dog"
  {
    data_store ds;
    auto dogh = ds.lookup("dog");
    if (dogh.empty())
      std::cerr << "No item called dog" << std::endl;
    else if(dogh.has_error())
      std::cerr << "ERROR: Looking up dog returned error " << dogh.get_error().message() << std::endl;
    else if (dogh.has_exception())
    {
      std::cerr << "FATAL: Looking up dog threw exception" << std::endl;
      std::terminate();
    }
    else
    {
      std::string buffer;
      *dogh.get() >> buffer;
      std::cout << "Item dog has value " << buffer << std::endl;
    }
  }
  //]
  return 0;
}
