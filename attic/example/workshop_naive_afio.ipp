//[workshop_naive_llfio_interface
namespace llfio = BOOST_AFIO_V2_NAMESPACE;
namespace filesystem = BOOST_AFIO_V2_NAMESPACE::filesystem;
using BOOST_OUTCOME_V1_NAMESPACE::outcome;

class data_store
{
  llfio::dispatcher_ptr _dispatcher;
  llfio::handle_ptr _store;
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
  data_store(size_t flags = 0, llfio::path path = "store");
  
  // Look up item named name for reading, returning a std::istream for the item if it exists
  outcome<istream> lookup(std::string name) noexcept;
  // Look up item named name for writing, returning an ostream for that item
  outcome<ostream> write(std::string name) noexcept;
};
//]

//[workshop_naive_llfio2]
namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
using BOOST_OUTCOME_V1_NAMESPACE::empty;
using BOOST_AFIO_V2_NAMESPACE::error_code;
using BOOST_AFIO_V2_NAMESPACE::generic_category;

// A special allocator of highly efficient file i/o memory
using file_buffer_type = std::vector<char, llfio::utils::page_allocator<char>>;

// An iostream which reads directly from a memory mapped AFIO file
struct idirectstream : public std::istream
{
  struct directstreambuf : public std::streambuf
  {
    llfio::handle_ptr h;  // Holds the file open and therefore mapped
    file_buffer_type buffer;
    llfio::handle::mapped_file_ptr mfp;
    directstreambuf(error_code &ec, llfio::handle_ptr _h) : h(std::move(_h))
    {
      // Get the size of the file. If greater than 128Kb mmap it
      size_t length=(size_t) h->lstat(llfio::metadata_flags::size).st_size;
      char *p=nullptr;
      if(length>=128*1024)
      {
        if((mfp=h->map_file()))
          p = (char *) mfp->addr;
      }
      if(!p)
      {
        buffer.resize(length);
        llfio::read(ec, h, buffer.data(), length, 0);
        p=buffer.data();
      }
      // Set the get buffer this streambuf is to use
      setg(p, p, p + length);
    }
  };
  std::unique_ptr<directstreambuf> buf;
  idirectstream(error_code &ec, llfio::handle_ptr h) : std::istream(new directstreambuf(ec, std::move(h))), buf(static_cast<directstreambuf *>(rdbuf()))
  {
  }
  virtual ~idirectstream() override
  {
    // Reset the stream before deleting the buffer
    rdbuf(nullptr);
  }
};

// An iostream which writes to an AFIO file in 4Kb pages
struct odirectstream : public std::ostream
{
  struct directstreambuf : public std::streambuf
  {
    using int_type = std::streambuf::int_type;
    using traits_type = std::streambuf::traits_type;
    llfio::future<> lastwrite; // the last async write performed
    llfio::off_t offset;       // offset of next write
    file_buffer_type buffer;  // a page size on this machine
    file_buffer_type lastbuffer;
    directstreambuf(llfio::handle_ptr _h) : lastwrite(std::move(_h)), offset(0), buffer(llfio::utils::page_sizes().front())
    {
      // Set the put buffer this streambuf is to use
      setp(buffer.data(), buffer.data() + buffer.size());
    }
    virtual ~directstreambuf() override
    {
      try
      {
        // Flush buffers and wait until last write completes
        // Schedule an asynchronous write of the buffer to storage
        size_t thisbuffer = pptr() - pbase();
        if(thisbuffer)
          lastwrite = llfio::async_write(llfio::async_truncate(lastwrite, offset+thisbuffer), buffer.data(), thisbuffer, offset);
        lastwrite.get();
      }
      catch(...)
      {
      }
    }
    virtual int_type overflow(int_type c) override
    {
      size_t thisbuffer=pptr()-pbase();
      if(thisbuffer>=buffer.size())
        sync();
      if(c!=traits_type::eof())
      {
        *pptr()=(char)c;
        pbump(1);
        return traits_type::to_int_type(c);
      }
      return traits_type::eof();
    }
    virtual int sync() override
    {
      // Wait for the last write to complete, propagating any exceptions
      lastwrite.get();
      size_t thisbuffer=pptr()-pbase();
      if(thisbuffer > 0)
      {
        // Detach the current buffer and replace with a fresh one to allow the kernel to steal the page
        lastbuffer=std::move(buffer);
        buffer.resize(lastbuffer.size());
        setp(buffer.data(), buffer.data() + buffer.size());
        // Schedule an extension of physical storage by an extra page
        lastwrite = llfio::async_truncate(lastwrite, offset + thisbuffer);
        // Schedule an asynchronous write of the buffer to storage
        lastwrite=llfio::async_write(lastwrite, lastbuffer.data(), thisbuffer, offset);
        offset+=thisbuffer;
      }
      return 0;
    }
  };
  std::unique_ptr<directstreambuf> buf;
  odirectstream(llfio::handle_ptr h) : std::ostream(new directstreambuf(std::move(h))), buf(static_cast<directstreambuf *>(rdbuf()))
  {
  }
  virtual ~odirectstream() override
  {
    // Reset the stream before deleting the buffer
    rdbuf(nullptr);
  }
};
//]

//[workshop_naive_llfio1]
namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
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

data_store::data_store(size_t flags, llfio::path path)
{
  // Make a dispatcher for the local filesystem URI, masking out write flags on all operations if not writeable
  _dispatcher=llfio::make_dispatcher("file:///", llfio::file_flags::none, !(flags & writeable) ? llfio::file_flags::write : llfio::file_flags::none).get();
  // Set the dispatcher for this thread, and open a handle to the store directory
  llfio::current_dispatcher_guard h(_dispatcher);
  _store=llfio::dir(std::move(path), llfio::file_flags::create);  // throws if there was an error
}

outcome<data_store::istream> data_store::lookup(std::string name) noexcept
{
  if(!is_valid_name(name))
    return error_code(EINVAL, generic_category());
  try
  {
    error_code ec;
    // Open the file using the handle to the store directory as the base.
    // The store directory can be freely renamed by any third party process
    // and everything here will work perfectly.
    llfio::handle_ptr h(llfio::file(ec, _store, name, llfio::file_flags::read));
    if(ec)
    {
      // If the file was not found, return empty else the error
      if(ec==error_code(ENOENT, generic_category()))
        return empty;
      return ec;
    }
    // Create an istream which directly uses the mapped file.
    data_store::istream ret(std::make_shared<idirectstream>(ec, std::move(h)));
    if(ec)
      return ec;
    return ret;
  }
  catch(...)
  {
    return std::current_exception();
  }
}

outcome<data_store::ostream> data_store::write(std::string name) noexcept
{
  if(!is_valid_name(name))
    return error_code(EINVAL, generic_category());
  try
  {
    error_code ec;
    // Open the file using the handle to the store directory as the base.
    // The store directory can be freely renamed by any third party process
    // and everything here will work perfectly. You could enable direct
    // buffer writing - this sends 4Kb pages directly to the physical hardware
    // bypassing the kernel file page cache, however this is not optimal if reads of
    // the value are likely to occur soon.
    llfio::handle_ptr h(llfio::file(ec, _store, name, llfio::file_flags::create | llfio::file_flags::write
      /*| llfio::file_flags::os_direct*/));
    if(ec)
      return ec;
    // Create an ostream which directly uses the mapped file.
    return outcome<data_store::ostream>(std::make_shared<odirectstream>(std::move(h)));
  }
  catch (...)
  {
    return std::current_exception();
  }
}
//]

int main(void)
{
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
  return 0;
}
