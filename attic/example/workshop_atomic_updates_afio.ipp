//[workshop_atomic_updates_afio_interface
namespace afio = BOOST_AFIO_V2_NAMESPACE;
namespace filesystem = BOOST_AFIO_V2_NAMESPACE::filesystem;
using BOOST_OUTCOME_V1_NAMESPACE::lightweight_futures::shared_future;

class data_store
{
  afio::dispatcher_ptr _dispatcher;
  afio::handle_ptr _store;
public:
  // Type used for read streams
  using istream = std::shared_ptr<std::istream>;
  // Type used for write streams
  using ostream = std::shared_ptr<std::ostream>;
  // Type used for lookup
  using lookup_result_type = shared_future<istream>;
  // Type used for write
  using write_result_type = shared_future<ostream>;

  // Disposition flags
  static constexpr size_t writeable = (1<<0);

  // Open a data store at path
  data_store(size_t flags = 0, afio::path path = "store");
  
  // Look up item named name for reading, returning an istream for the item
  shared_future<istream> lookup(std::string name) noexcept;
  // Look up item named name for writing, returning an ostream for that item
  shared_future<ostream> write(std::string name) noexcept;
};
//]

//[workshop_atomic_updates_afio3]
namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
using BOOST_OUTCOME_V1_NAMESPACE::empty;
using BOOST_AFIO_V2_NAMESPACE::error_code;
using BOOST_AFIO_V2_NAMESPACE::generic_category;

// A special allocator of highly efficient file i/o memory
using file_buffer_type = std::vector<char, afio::utils::page_allocator<char>>;

// An iostream which reads directly from a memory mapped AFIO file
struct idirectstream : public std::istream
{
  struct directstreambuf : public std::streambuf
  {
    afio::handle_ptr h;  // Holds the file open
    std::shared_ptr<file_buffer_type> buffer;
    afio::handle::mapped_file_ptr mfp;
    // From a mmap
    directstreambuf(afio::handle_ptr _h, afio::handle::mapped_file_ptr _mfp, size_t length) : h(std::move(_h)), mfp(std::move(_mfp))
    {
      // Set the get buffer this streambuf is to use
      setg((char *) mfp->addr, (char *) mfp->addr, (char *) mfp->addr + length);
    }
    // From a malloc
    directstreambuf(afio::handle_ptr _h, std::shared_ptr<file_buffer_type> _buffer, size_t length) : h(std::move(_h)), buffer(std::move(_buffer))
    {
      // Set the get buffer this streambuf is to use
      setg(buffer->data(), buffer->data(), buffer->data() + length);
    }
  };
  std::unique_ptr<directstreambuf> buf;
  template<class U> idirectstream(afio::handle_ptr h, U &&buffer, size_t length) : std::istream(new directstreambuf(std::move(h), std::forward<U>(buffer), length)), buf(static_cast<directstreambuf *>(rdbuf()))
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
    afio::future<> lastwrite; // the last async write performed
    afio::off_t offset;       // offset of next write
    file_buffer_type buffer;  // a page size on this machine
    file_buffer_type lastbuffer;
    directstreambuf(afio::handle_ptr _h) : lastwrite(std::move(_h)), offset(0), buffer(afio::utils::page_sizes().front())
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
          lastwrite = afio::async_write(afio::async_truncate(lastwrite, offset+thisbuffer), buffer.data(), thisbuffer, offset);
        lastwrite.get();
        // TODO: On Windows do I need to close and reopen the file to flush metadata before
        //       the rename or does the rename do it for me?
        // Get handle to the parent directory
        auto dirh(lastwrite->container());
        // Atomically rename "tmpXXXXXXXXXXXXXXXX" to "0"
        lastwrite->atomic_relink(afio::path_req::relative(dirh, "0"));
#ifdef __linux__
        // Journalled Linux filing systems don't need this, but if you enabled afio::file_flags::always_sync
        // you might want to issue this too.
        afio::sync(dirh);
#endif
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
        lastwrite = afio::async_truncate(lastwrite, offset + thisbuffer);
        // Schedule an asynchronous write of the buffer to storage
        lastwrite=afio::async_write(lastwrite, lastbuffer.data(), thisbuffer, offset);
        offset+=thisbuffer;
      }
      return 0;
    }
  };
  std::unique_ptr<directstreambuf> buf;
  odirectstream(afio::handle_ptr h) : std::ostream(new directstreambuf(std::move(h))), buf(static_cast<directstreambuf *>(rdbuf()))
  {
  }
  virtual ~odirectstream() override
  {
    // Reset the stream before deleting the buffer
    rdbuf(nullptr);
  }
};
//]

//[workshop_atomic_updates_afio1]
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

// Keep a cache of crypto strong random names
static std::string random_name()
{
  static struct random_names_type
  {
    std::vector<std::string> names;
    size_t idx;
    random_names_type(size_t count) : names(count), idx(0)
    {
      for(size_t n=0; n<count; n++)
        names[n]=afio::utils::random_string(16);  // 128 bits
    }
    std::string get()
    {
      if(idx==names.size())
        idx=0;
      return names[idx++];
    }
  } random_names(10000);
  return random_names.get();
}

data_store::data_store(size_t flags, afio::path path)
{
  // Make a dispatcher for the local filesystem URI, masking out write flags on all operations if not writeable
  _dispatcher=afio::make_dispatcher("file:///", afio::file_flags::none, !(flags & writeable) ? afio::file_flags::write : afio::file_flags::none).get();
  // Set the dispatcher for this thread, and open a handle to the store directory
  afio::current_dispatcher_guard h(_dispatcher);
  _store=afio::dir(std::move(path), afio::file_flags::create);  // throws if there was an error
  // Precalculate the cache of random names
  random_name();
}

shared_future<data_store::istream> data_store::lookup(std::string name) noexcept
{
  if(!is_valid_name(name))
    return error_code(EINVAL, generic_category());
  try
  {
    name.append("/0");
    // Schedule the opening of the file for reading
    afio::future<> h(afio::async_file(_store, name, afio::file_flags::read));
    // When it completes, call this continuation
    return h.then([](afio::future<> &_h) -> shared_future<data_store::istream> {
      // If file didn't open, return the error or exception immediately
      BOOST_OUTCOME_PROPAGATE(_h);
      size_t length=(size_t) _h->lstat(afio::metadata_flags::size).st_size;
      // Is a memory map more appropriate?
      if(length>=128*1024)
      {
        afio::handle::mapped_file_ptr mfp;
        if((mfp=_h->map_file()))
        {
          data_store::istream ret(std::make_shared<idirectstream>(_h.get_handle(), std::move(mfp), length));
          return ret;
        }
      }
      // Schedule the reading of the file into a buffer
      auto buffer=std::make_shared<file_buffer_type>(length);
      afio::future<> h(afio::async_read(_h, buffer->data(), length, 0));
      // When the read completes call this continuation
      return h.then([buffer, length](const afio::future<> &h) -> shared_future<data_store::istream> {
        // If read failed, return the error or exception immediately
        BOOST_OUTCOME_PROPAGATE(h);
        data_store::istream ret(std::make_shared<idirectstream>(h.get_handle(), buffer, length));
        return ret;
      });
    });
  }
  catch(...)
  {
    return std::current_exception();
  }
}
//]

//[workshop_atomic_updates_afio2]
shared_future<data_store::ostream> data_store::write(std::string name) noexcept
{
  if(!is_valid_name(name))
    return error_code(EINVAL, generic_category());
  try
  {
    // Schedule the opening of the directory
    afio::future<> dirh(afio::async_dir(_store, name, afio::file_flags::create));
#ifdef __linux__
    // Flush metadata on Linux only. This will be a noop unless we created a new directory
    // above, and if we don't flush the new key directory it and its contents may not appear
    // in the store directory after a suddenly power loss, even if it and its contents are
    // all on physical storage.
    dirh.then([this](const afio::future<> &h) { async_sync(_store); });
#endif
    // Make a crypto strong random file name
    std::string randomname("tmp");
    randomname.append(random_name());
    // Schedule the opening of the file for writing
    afio::future<> h(afio::async_file(dirh, randomname, afio::file_flags::create | afio::file_flags::write
      | afio::file_flags::hold_parent_open    // handle should keep a handle_ptr to its parent dir
      /*| afio::file_flags::always_sync*/     // writes don't complete until upon physical storage
      ));
    // When it completes, call this continuation
    return h.then([](const afio::future<> &h) -> shared_future<data_store::ostream> {
      // If file didn't open, return the error or exception immediately
      BOOST_OUTCOME_PROPAGATE(h);
      // Create an ostream which directly uses the file.
      data_store::ostream ret(std::make_shared<odirectstream>(h.get_handle()));
      return std::move(ret);
    });
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
