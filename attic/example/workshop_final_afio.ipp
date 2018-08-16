#include "../detail/SpookyV2.h"

//[workshop_final_interface
namespace afio = BOOST_AFIO_V2_NAMESPACE;
namespace filesystem = BOOST_AFIO_V2_NAMESPACE::filesystem;
using BOOST_OUTCOME_V1_NAMESPACE::outcome;
using BOOST_OUTCOME_V1_NAMESPACE::lightweight_futures::shared_future;

class data_store
{
  struct _ostream;
  friend struct _ostream;
  afio::dispatcher_ptr _dispatcher;
  // The small blob store keeps non-memory mappable blobs at 32 byte alignments
  afio::handle_ptr _small_blob_store, _small_blob_store_append, _small_blob_store_ecc;
  // The large blob store keeps memory mappable blobs at 4Kb alignments
  afio::handle_ptr _large_blob_store, _large_blob_store_append, _large_blob_store_ecc;
  // The index is where we keep the map of keys to blobs
  afio::handle_ptr _index_store, _index_store_append, _index_store_ecc;
  struct index;
  std::unique_ptr<index> _index;
public:
  // Type used for read streams
  using istream = std::shared_ptr<std::istream>;
  // Type used for write streams
  using ostream = std::shared_ptr<std::ostream>;
  // Type used for lookup
  using lookup_result_type = shared_future<istream>;
  // Type used for write
  using write_result_type = outcome<ostream>;

  // Disposition flags
  static constexpr size_t writeable = (1<<0);

  // Open a data store at path
  data_store(size_t flags = 0, afio::path path = "store");
  
  // Look up item named name for reading, returning an istream for the item
  shared_future<istream> lookup(std::string name) noexcept;
  // Look up item named name for writing, returning an ostream for that item
  outcome<ostream> write(std::string name) noexcept;
};
//]

//[workshop_final1]
namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
using BOOST_AFIO_V2_NAMESPACE::error_code;
using BOOST_AFIO_V2_NAMESPACE::generic_category;
using BOOST_OUTCOME_V1_NAMESPACE::outcome;

// A special allocator of highly efficient file i/o memory
using file_buffer_type = std::vector<char, afio::utils::page_allocator<char>>;

// Serialisation helper types
#pragma pack(push, 1)
struct ondisk_file_header  // 20 bytes
{
  union
  {
    afio::off_t length;            // Always 32 in byte order of whoever wrote this
    char endian[8];
  };
  afio::off_t index_offset_begin;  // Hint to the length of the store when the index was last written
  unsigned int time_count;         // Racy monotonically increasing count
};
struct ondisk_record_header  // 28 bytes - ALWAYS ALIGNED TO 32 BYTE FILE OFFSET
{
  afio::off_t magic : 16;   // 0xad magic
  afio::off_t kind : 2;     // 0 for zeroed space, 1,2 for blob, 3 for index
  afio::off_t _spare : 6;
  afio::off_t length : 40;  // Size of the object (including this preamble, regions, key values) (+8)
  unsigned int age;         // file header time_count when this was added (+12)
  uint64 hash[2];           // 128-bit SpookyHash of the object (from below onwards) (+28)
  // ondisk_index_regions
  // ondisk_index_key_value (many until length)
};
struct ondisk_index_regions  // 12 + regions_size * 32
{
  afio::off_t thisoffset;     // this index only valid if equals this offset
  unsigned int regions_size;  // count of regions with their status (+28)
  struct ondisk_index_region
  {
    afio::off_t offset;       // offset to this region
    ondisk_record_header r;   // copy of the header at the offset to avoid a disk seek
  } regions[1];
};
struct ondisk_index_key_value // 8 + sizeof(key)
{
  unsigned int region_index;  // Index into regions
  unsigned int name_size;     // Length of key
  char name[1];               // Key string (utf-8)
};
#pragma pack(pop)

struct data_store::index
{
  struct region
  {
    enum kind_type { zeroed=0, small_blob=1, large_blob=2, index=3 } kind;
    afio::off_t offset, length;
    uint64 hash[2];
    region(ondisk_index_regions::ondisk_index_region *r) : kind(static_cast<kind_type>(r->r.kind)), offset(r->offset), length(r->r.length) { memcpy(hash, r->r.hash, sizeof(hash)); }
    bool operator<(const region &o) const noexcept { return offset<o.offset; }
    bool operator==(const region &o) const noexcept { return offset==o.offset && length==o.length; }
  };
  afio::off_t offset_loaded_from;  // Offset this index was loaded from
  unsigned int last_time_count;    // Header time count
  std::vector<region> regions;
  std::unordered_map<std::string, size_t> key_to_region;
  index() : offset_loaded_from(0) { }
//]
//[workshop_final2]
  struct last_good_ondisk_index_info
  {
    afio::off_t offset;
    std::unique_ptr<char[]> buffer;
    size_t size;
    last_good_ondisk_index_info() : offset(0), size(0) { }
  };
  // Finds the last good index in the store
  outcome<last_good_ondisk_index_info> find_last_good_ondisk_index(afio::handle_ptr h) noexcept
  {
    last_good_ondisk_index_info ret;
    error_code ec;
    try
    {
      // Read the front of the store file to get the index offset hint
      ondisk_file_header header;
      afio::read(h, header, 0);
      afio::off_t offset=0;
      if(header.length==32)
        offset=header.index_offset_begin;
      else if(header.endian[0]==32)  // wrong endian
        return error_code(ENOEXEC, generic_category());
      else  // corrupted
        return error_code(ENOTSUP, generic_category());
      last_time_count=header.time_count;
      // Fetch the valid extents
      auto valid_extents(afio::extents(h));
      auto valid_extents_it=valid_extents.begin();
      // Iterate the records starting from index offset hint, keeping track of last known good index
      bool done=true;
      do
      {
        afio::off_t linear_scanning=0;
        ondisk_record_header record;
        afio::off_t file_length=h->lstat(afio::metadata_flags::size).st_size;
        for(; offset<file_length;)
        {
          // Round to 32 byte boundary
          offset&=~31ULL;
          // Find my valid extent
          while(offset>=valid_extents_it->first+valid_extents_it->second)
          {
            if(valid_extents.end()==++valid_extents_it)
            {
              valid_extents=afio::extents(h);
              valid_extents_it=valid_extents.begin();
            }
          }
          // Is this offset within a valid extent? If not, bump it.
          if(offset<valid_extents_it->first)
            offset=valid_extents_it->first;
          afio::read(ec, h, record, offset);
          if(ec) return ec;
          // If this does not contain a valid record, linear scan
          // until we find one
          if(record.magic!=0xad || (record.length & 31))
          {
start_linear_scan:
            if(!linear_scanning)
            {
              std::cerr << "WARNING: Corrupt record detected at " << offset << ", linear scanning ..." << std::endl;
              linear_scanning=offset;
            }
            offset+=32;
            continue;
          }
          // Is this the first good record after a corrupted section?
          if(linear_scanning)
          {
            std::cerr << "NOTE: Found valid record after linear scan at " << offset << std::endl;
            std::cerr << "      Removing invalid data between " << linear_scanning << " and " << offset << std::endl;
            // Rewrite a valid record to span the invalid section
            ondisk_record_header temp={0};
            temp.magic=0xad;
            temp.length=offset-linear_scanning;
            temp.age=last_time_count;
            afio::write(ec, h, temp, linear_scanning);
            // Deallocate the physical storage for the invalid section
            afio::zero(ec, h, {{linear_scanning+12, offset-linear_scanning-12}});
            linear_scanning=0;
          }
          // If not an index, skip entire record
          if(record.kind!=3 /*index*/)
          {
            // If this record length is wrong, start a linear scan
            if(record.length>file_length-offset)
              offset+=32;
            else
              offset+=record.length;
            continue;
          }
          std::unique_ptr<char[]> buffer;
          // If record.length is corrupt, this will throw bad_alloc
          try
          {
            buffer.reset(new char[(size_t) record.length-sizeof(header)]);
          }
          catch(...)
          {
            // Either we are out of memory, or it's a corrupted record
            // TODO: Try a ECC heal pass here
            // If this record length is wrong, start a linear scan
            if(record.length>file_length-offset)
              goto start_linear_scan;
            else
              offset+=record.length;
            continue;
          }
          afio::read(ec, h, buffer.get(), (size_t) record.length-sizeof(header), offset+sizeof(header));
          if(ec)
            return ec;
          uint64 hash[2]={0, 0};
          SpookyHash::Hash128(buffer.get(), (size_t) record.length-sizeof(header), hash, hash+1);
          // Is this record correct?
          if(!memcmp(hash, record.hash, sizeof(hash)))
          {
            // Is this index not conflicted? If not, it's the new most recent index
            ondisk_index_regions *r=(ondisk_index_regions *) buffer.get();
            if(r->thisoffset==offset)
            {
              ret.buffer=std::move(buffer);
              ret.size=(size_t) record.length-sizeof(header);
              ret.offset=offset;
            }
          }
          offset+=record.length;
        }
        if(ret.offset)  // we have a valid most recent index so we're done
          done=true;
        else if(header.index_offset_begin>sizeof(header))
        {
          // Looks like the end of the store got trashed.
          // Reparse from the very beginning
          offset=32;
          header.index_offset_begin=0;
        }
        else
        {
          // No viable records at all, or empty store.
          done=true;
        }
      } while(!done);
      return ret;
    }
    catch(...)
    {
      return std::current_exception();
    }
  }
//]
//[workshop_final3]
  // Loads the index from the store
  outcome<void> load(afio::handle_ptr h) noexcept
  {
    // If find_last_good_ondisk_index() returns error or exception, return those, else
    // initialise ondisk_index_info to monad.get()
    BOOST_OUTCOME_AUTO(ondisk_index_info, find_last_good_ondisk_index(h));
    error_code ec;
    try
    {
      offset_loaded_from=0;
      regions.clear();
      key_to_region.clear();
      ondisk_index_regions *r=(ondisk_index_regions *) ondisk_index_info.buffer.get();
      regions.reserve(r->regions_size);
      for(size_t n=0; n<r->regions_size; n++)
        regions.push_back(region(r->regions+n));
      ondisk_index_key_value *k=(ondisk_index_key_value *)(r+r->regions_size), *end=(ondisk_index_key_value *)(ondisk_index_info.buffer.get()+ondisk_index_info.size);
      while(k<end)
        key_to_region.insert(std::make_pair(std::string(k->name, k->name_size), k->region_index));
      offset_loaded_from=ondisk_index_info.offset;
      return {};
    }
    catch(...)
    {
      return std::current_exception();
    }
  }
//]
//[workshop_final4]
  // Writes the index to the store
  outcome<void> store(afio::handle_ptr rwh, afio::handle_ptr appendh) noexcept
  {
    error_code ec;
    std::vector<ondisk_index_regions::ondisk_index_region> ondisk_regions;
    ondisk_regions.reserve(65536);
    for(auto &i : regions)
    {
      ondisk_index_regions::ondisk_index_region r;
      r.offset=i.offset;
      r.r.kind=i.kind;
      r.r.length=i.length;
      memcpy(r.r.hash, i.hash, sizeof(i.hash));
      ondisk_regions.push_back(r);
    }

    size_t bytes=0;
    for(auto &i : key_to_region)
      bytes+=8+i.first.size();
    std::vector<char> ondisk_key_values(bytes);
    ondisk_index_key_value *kv=(ondisk_index_key_value *) ondisk_key_values.data();
    for(auto &i : key_to_region)
    {
      kv->region_index=(unsigned int) i.second;
      kv->name_size=(unsigned int) i.first.size();
      memcpy(kv->name, i.first.c_str(), i.first.size());
      kv=(ondisk_index_key_value *)(((char *) kv) + 8 + i.first.size());
    }

    struct
    {
      ondisk_record_header header;
      ondisk_index_regions header2;
    } h;
    h.header.magic=0xad;
    h.header.kind=3;  // writing an index
    h.header._spare=0;
    h.header.length=sizeof(h.header)+sizeof(h.header2)+sizeof(ondisk_regions.front())*(regions.size()-1)+bytes;
    h.header.age=last_time_count;
    // hash calculated later
    // thisoffset calculated later
    h.header2.regions_size=(unsigned int) regions.size();
    // Pad zeros to 32 byte multiple
    std::vector<char> zeros((h.header.length+31)&~31ULL);
    h.header.length+=zeros.size();

    // Create the gather buffer sequence
    std::vector<asio::const_buffer> buffers(4);
    buffers[0]=asio::const_buffer(&h, 36);
    buffers[1]=asio::const_buffer(ondisk_regions.data(), sizeof(ondisk_regions.front())*ondisk_regions.size());
    buffers[2]=asio::const_buffer(ondisk_key_values.data(), ondisk_key_values.size());
    if(zeros.empty())
      buffers.pop_back();
    else
      buffers[3]=asio::const_buffer(zeros.data(), zeros.size());
    file_buffer_type reread(sizeof(h));
    ondisk_record_header *reread_header=(ondisk_record_header *) reread.data();
    bool success=false;
    do
    {
      // Is this index stale?
      BOOST_OUTCOME_AUTO(ondisk_index_info, find_last_good_ondisk_index(rwh));
      if(ondisk_index_info.offset!=offset_loaded_from)
      {
        // A better conflict resolution strategy might check to see if deltas
        // are compatible, but for the sake of brevity we always report conflict
        return error_code(EDEADLK, generic_category());
      }
      // Take the current length of the store file. Any index written will be at or after this.
      h.header2.thisoffset=appendh->lstat(afio::metadata_flags::size).st_size;
      memset(h.header.hash, 0, sizeof(h.header.hash));
      // Hash the end of the first gather buffer and all the remaining gather buffers
      SpookyHash::Hash128(asio::buffer_cast<const char *>(buffers[0])+24, asio::buffer_size(buffers[0])-24, h.header.hash, h.header.hash+1);
      SpookyHash::Hash128(asio::buffer_cast<const char *>(buffers[1]),    asio::buffer_size(buffers[1]),    h.header.hash, h.header.hash+1);
      SpookyHash::Hash128(asio::buffer_cast<const char *>(buffers[2]),    asio::buffer_size(buffers[2]),    h.header.hash, h.header.hash+1);
      if(buffers.size()>3)
        SpookyHash::Hash128(asio::buffer_cast<const char *>(buffers[3]),    asio::buffer_size(buffers[3]),    h.header.hash, h.header.hash+1);
      // Atomic append the record
      afio::write(ec, appendh, buffers, 0);
      if(ec) return ec;
      // Reread the record
      afio::read(ec, rwh, reread.data(), reread.size(), h.header2.thisoffset);
      if(ec) return ec;
      // If the record doesn't match it could be due to a lag in st_size between open handles,
      // so retry until success or stale index
    } while(memcmp(reread_header->hash, h.header.hash, sizeof(h.header.hash)));

    // New index has been successfully written. Update the hint at the front of the file.
    // This update is racy of course, but as it's merely a hint we don't care.
    ondisk_file_header file_header;
    afio::read(ec, rwh, file_header, 0);
    if(!ec && file_header.index_offset_begin<h.header2.thisoffset)
    {
      file_header.index_offset_begin=h.header2.thisoffset;
      file_header.time_count++;
      afio::write(ec, rwh, file_header, 0);
    }
    offset_loaded_from=h.header2.thisoffset;
    last_time_count=file_header.time_count;
    return {};
  }
//]
//[workshop_final5]
  // Reloads the index if needed
  outcome<void> refresh(afio::handle_ptr h) noexcept
  {
    static afio::off_t last_size;
    error_code ec;
    afio::off_t size=h->lstat(afio::metadata_flags::size).st_size;
    // Has the size changed? If so, need to check the hint
    if(size>last_size)
    {
      last_size=size;
      ondisk_file_header header;
      afio::read(ec, h, header, 0);
      if(ec) return ec;
      // If the hint is moved, we are stale
      if(header.index_offset_begin>offset_loaded_from)
        return load(h);
    }
    return {};
  }
};
//]

//[workshop_final6]
namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
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
    // From a mmap
    directstreambuf(afio::handle_ptr _h, char *addr, size_t length) : h(std::move(_h))
    {
      // Set the get buffer this streambuf is to use
      setg(addr, addr, addr + length);
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
//]
//[workshop_final7]
// An iostream which buffers all the output, then commits on destruct
struct data_store::_ostream : public std::ostream
{
  struct ostreambuf : public std::streambuf
  {
    using int_type = std::streambuf::int_type;
    using traits_type = std::streambuf::traits_type;
    data_store *ds;
    std::string name;
    file_buffer_type buffer;
    ostreambuf(data_store *_ds, std::string _name) : ds(_ds), name(std::move(_name)), buffer(afio::utils::page_sizes().front())
    {
      // Set the put buffer this streambuf is to use
      setp(buffer.data(), buffer.data() + buffer.size());
    }
    virtual ~ostreambuf() override
    {
      try
      {
        ondisk_index_regions::ondisk_index_region r;
        r.r.magic=0xad;
        r.r.kind=1;  // small blob
        r.r.length=sizeof(r.r)+buffer.size();
        r.r.length=(r.r.length+31)&~31ULL;  // pad to 32 byte multiple
        r.r.age=ds->_index->last_time_count;
        memset(r.r.hash, 0, sizeof(r.r.hash));
        SpookyHash::Hash128(buffer.data(), (size_t)(r.r.length-sizeof(r.r)), r.r.hash, r.r.hash+1);

        // Create the gather buffer sequence and atomic append the blob
        std::vector<asio::const_buffer> buffers(2);
        buffers[0]=asio::const_buffer((char *) &r.r, sizeof(r.r));
        buffers[1]=asio::const_buffer(buffer.data(), (size_t)(r.r.length-sizeof(r.r)));
        error_code ec;
        auto offset=ds->_small_blob_store_append->lstat(afio::metadata_flags::size).st_size;
        afio::write(ec, ds->_small_blob_store_append, buffers, 0);
        if(ec)
          abort();  // should really do something better here

        // Find out where my blob ended up
        ondisk_record_header header;
        do
        {
          afio::read(ec, ds->_small_blob_store_append, header, offset);
          if(ec) abort();
          if(header.kind==1 /*small blob*/ && !memcmp(header.hash, r.r.hash, sizeof(header.hash)))
          {
            r.offset=offset;
            break;
          }
          offset+=header.length;
        } while(offset<ds->_small_blob_store_append->lstat(afio::metadata_flags::size).st_size);

        for(;;)
        {
          // Add my blob to the regions
          ds->_index->regions.push_back(&r);
          // Add my key to the key index
          ds->_index->key_to_region[name]=ds->_index->regions.size()-1;
          // Commit the index, and if successful exit
          if(!ds->_index->store(ds->_index_store, ds->_index_store_append))
            return;
          // Reload the index and retry
          if(ds->_index->load(ds->_index_store))
            abort();
        }
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
      buffer.resize(buffer.size()*2);
      setp(buffer.data() + buffer.size()/2, buffer.data() + buffer.size());
      return 0;
    }
  };
  std::unique_ptr<ostreambuf> buf;
  _ostream(data_store *ds, std::string name) : std::ostream(new ostreambuf(ds, std::move(name))), buf(static_cast<ostreambuf *>(rdbuf()))
  {
  }
  virtual ~_ostream() override
  {
    // Reset the stream before deleting the buffer
    rdbuf(nullptr);
  }
};
//]

//[workshop_final8]
data_store::data_store(size_t flags, afio::path path)
{
  // Make a dispatcher for the local filesystem URI, masking out write flags on all operations if not writeable
  _dispatcher=afio::make_dispatcher("file:///", afio::file_flags::none, !(flags & writeable) ? afio::file_flags::write : afio::file_flags::none).get();
  // Set the dispatcher for this thread, and create/open a handle to the store directory
  afio::current_dispatcher_guard h(_dispatcher);
  auto dirh(afio::dir(std::move(path), afio::file_flags::create));  // throws if there was an error

  // The small blob store keeps non-memory mappable blobs at 32 byte alignments
  _small_blob_store_append=afio::file(dirh, "small_blob_store", afio::file_flags::create | afio::file_flags::append);  // throws if there was an error
  _small_blob_store=afio::file(dirh, "small_blob_store", afio::file_flags::read_write);          // throws if there was an error
  _small_blob_store_ecc=afio::file(dirh, "small_blob_store.ecc", afio::file_flags::create | afio::file_flags::read_write);  // throws if there was an error

  // The large blob store keeps memory mappable blobs at 4Kb alignments
  // TODO

  // The index is where we keep the map of keys to blobs
  _index_store_append=afio::file(dirh, "index", afio::file_flags::create | afio::file_flags::append);  // throws if there was an error
  _index_store=afio::file(dirh, "index", afio::file_flags::read_write);          // throws if there was an error
  _index_store_ecc=afio::file(dirh, "index.ecc", afio::file_flags::create | afio::file_flags::read_write);  // throws if there was an error
  // Is this store just created?
  if(!_index_store_append->lstat(afio::metadata_flags::size).st_size)
  {
    ondisk_file_header header;
    header.length=32;
    header.index_offset_begin=32;
    header.time_count=0;
    // This is racy, but the file format doesn't care
    afio::write(_index_store_append, header, 0);
  }
  _index.reset(new index);
}

shared_future<data_store::istream> data_store::lookup(std::string name) noexcept
{
  try
  {
    BOOST_OUTCOME_PROPAGATE(_index->refresh(_index_store));
    auto it=_index->key_to_region.find(name);
    if(_index->key_to_region.end()==it)
      return error_code(ENOENT, generic_category());  // not found
    auto &r=_index->regions[it->second];
    afio::off_t offset=r.offset+24, length=r.length-24;
    // Schedule the reading of the file into a buffer
    auto buffer=std::make_shared<file_buffer_type>((size_t) length);
    afio::future<> h(afio::async_read(_small_blob_store, buffer->data(), (size_t) length, offset));
    // When the read completes call this continuation
    return h.then([buffer, length](const afio::future<> &h) -> shared_future<data_store::istream> {
      // If read failed, return the error or exception immediately
      BOOST_OUTCOME_PROPAGATE(h);
      data_store::istream ret(std::make_shared<idirectstream>(h.get_handle(), buffer, (size_t) length));
      return ret;
    });
  }
  catch(...)
  {
    return std::current_exception();
  }
}

outcome<data_store::ostream> data_store::write(std::string name) noexcept
{
  try
  {
    return std::make_shared<_ostream>(this, std::move(name));
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
