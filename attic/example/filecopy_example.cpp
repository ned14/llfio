#include "afio_pch.hpp"

static uint32_t crc32(const void* data, size_t length, uint32_t previousCrc32 = 0)
{
  const uint32_t Polynomial = 0xEDB88320;
  uint32_t crc = ~previousCrc32;
  unsigned char* current = (unsigned char*) data;
  while (length--)
  {
    crc ^= *current++;
    for (unsigned int j = 0; j < 8; j++)
      crc = (crc >> 1) ^ (-int(crc & 1) & Polynomial);
  }
  return ~crc; // same as crc ^ 0xFFFFFFFF
}

//[filecopy_example
namespace {
    using namespace boost::afio;
    using boost::afio::off_t;

    // Keep memory buffers around
    // A special allocator of highly efficient file i/o memory
    typedef std::vector<char, utils::page_allocator<char>> file_buffer_type;
    static std::vector<std::unique_ptr<file_buffer_type>> buffers;

    // Parallel copy files in sources into dest, concatenating
    stl_future<std::vector<handle_ptr>> async_concatenate_files(
        atomic<off_t> &written, off_t &totalbytes,
        dispatcher_ptr dispatcher,
        boost::afio::filesystem::path dest, std::vector<boost::afio::filesystem::path> sources,
        size_t chunk_size=1024*1024 /* 1Mb */)
    {
        // Schedule the opening of the output file for writing
        auto oh = async_file(dest, file_flags::create | file_flags::write);
        // Schedule the opening of all the input files for reading as a batch
        std::vector<path_req> ihs_reqs; ihs_reqs.reserve(sources.size());
        for(auto &&source : sources)
            ihs_reqs.push_back(path_req(source, file_flags::read
            |file_flags::will_be_sequentially_accessed));
        auto ihs=dispatcher->file(ihs_reqs);
        // Retrieve any error from opening the output
        oh.get();
        // Wait for the input file handles to open so we can get their sizes
        // (plus any failures to open)
        when_all_p(ihs).get();

        // Need to figure out the sizes of the sources so we can resize output
        // correctly. We also need to allocate scratch buffers for each source.
        std::vector<std::tuple<off_t, off_t>> offsets;
        offsets.reserve(ihs.size());
        off_t offset=0, max_individual=0;
        for(auto &ih : ihs)
        {
            // Get the file's size in bytes
            off_t bytes=ih->direntry(metadata_flags::size).st_size();
            if(bytes>max_individual) max_individual=bytes;
            //std::cout << "File " << ih->path() << " size " << bytes << " to offset " << offset << std::endl;
            // Push the offset to write at, amount to write, and a scratch buffer
            offsets.push_back(std::make_tuple(offset, bytes));
            buffers.push_back(detail::make_unique<file_buffer_type>(chunk_size));
            offset+=bytes;
        }
        // Schedule resizing output to correct size, retrieving errors
        totalbytes=offset;
        auto ohresize=async_truncate(oh, offset);
        when_all_p(ohresize).get();

        // Schedule the parallel processing of all input files, sequential per file,
        // but only after the output file has been resized
        std::vector<future<>> lasts;
        for(auto &i : ihs)
          lasts.push_back(dispatcher->depends(ohresize, i));
        for(off_t o=0; o<max_individual; o+=chunk_size)
        {
          for(size_t idx=0; idx<ihs_reqs.size(); idx++)
          {
            auto offset=std::get<0>(offsets[idx]), bytes=std::get<1>(offsets[idx]);
            auto &buffer=buffers[idx];
            if(o<bytes)
            {
              off_t thischunk=bytes-o;
              if(thischunk>chunk_size) thischunk=chunk_size;
              //std::cout << "Writing " << thischunk << " from offset " << o << " in  " << lasts[idx]->path() << std::endl;
              // Schedule a filling of buffer from offset o after last has completed
              auto readchunk = async_read(lasts[idx], buffer->data(), (size_t)thischunk, o);
              // Schedule a writing of buffer to offset offset+o after readchunk is ready
              // Note the call to dispatcher->depends() to make sure the write only occurs
              // after the read completes
              auto writechunk = async_write(depends(readchunk, ohresize), buffer->data(), (size_t)thischunk, offset + o);
              // Schedule incrementing written after write has completed
              auto incwritten = writechunk.then([&written, thischunk](future<> f) {
                written += thischunk;
                return f;
              });
              // Don't do next read until written is incremented
              lasts[idx]=dispatcher->depends(incwritten, readchunk);
            }
          }
        }
        // Having scheduled all the reads and write, return a stl_future which returns when
        // they're done
        return when_all_p(lasts);
    }
}

int main(int argc, const char *argv[])
{
    using namespace boost::afio;
    using boost::afio::off_t;
    typedef chrono::duration<double, ratio<1, 1>> secs_type;
    if(argc<3)
    {
        std::cerr << "ERROR: Need to specify destination path and source paths"
            << std::endl;
        return 1;
    }        
    try
    {
        atomic<off_t> written(0);
        off_t totalbytes=0;
        std::shared_ptr<boost::afio::dispatcher> dispatcher=
            boost::afio::make_dispatcher().get();
        // Set a dispatcher as current for this thread
        boost::afio::current_dispatcher_guard guard(dispatcher);

        boost::afio::filesystem::path dest=argv[1];
        std::vector<boost::afio::filesystem::path> sources;
        std::cout << "Concatenating into " << dest << " the files ";
        for(int n=2; n<argc; ++n)
        {
            sources.push_back(argv[n]);
            std::cout << sources.back();
            if(n<argc-1) std::cout << ", ";
        }
        std::cout << " ..." << std::endl;

        auto begin=chrono::steady_clock::now();
        auto h=async_concatenate_files(written, totalbytes, dispatcher, dest, sources);
        // Print progress once a second until it's done
        while(future_status::timeout==h.wait_for(boost::afio::chrono::seconds(1)))
        {
            std::cout << "\r" << (100*written)/totalbytes << "% complete (" << written
                << " out of " << totalbytes << " @ " << (written/chrono::duration_cast<secs_type>(
                    chrono::steady_clock::now()-begin).count()/1024/1024) << "Mb/sec) ..." << std::flush;
        }
        // Retrieve any errors
        h.get();
        std::cout << std::endl;
    }
    catch(...)
    {
        std::cerr << "ERROR: " << boost::current_exception_diagnostic_information(true)
            << std::endl;
        return 1;
    }
    // Make sure output really is input concatenated
    std::cout << "CRC checking that the output exactly matches the inputs ..." << std::endl;
    uint32_t crc1 = 0, crc2 = 0;
    off_t offset = 0;
    std::ifstream o(argv[1], std::ifstream::in);
    for (int n = 2; n < argc; n++)
    {
      std::ifstream i(argv[n], std::ifstream::in);
      i.seekg(0, i.end);
      std::vector<char> buffer1((size_t)i.tellg()), buffer2((size_t)i.tellg());
      i.seekg(0, i.beg);
      o.read(buffer1.data(), buffer1.size());
      i.read(buffer2.data(), buffer2.size());
      bool quiet = false;
      for (size_t n = 0; n<buffer1.size(); n += 64)
      {
        crc1 = crc32(buffer1.data() + n, 64, crc1);
        crc2 = crc32(buffer2.data() + n, 64, crc2);
        if (crc1 != crc2 && !quiet)
        {
          std::cerr << "ERROR: Offset " << offset+n << " not copied correctly!" << std::endl;
          quiet = true;
        }
      }
      offset += buffer1.size();
    }
    return 0;
}
//]
