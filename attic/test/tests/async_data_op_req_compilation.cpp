#include "test_functions.hpp"

//#define BOOST_AFIO_TEST_ASYNC_DATA_OP_REQ_FAILURE_TO_COMPILE

BOOST_AFIO_AUTO_TEST_CASE(io_req_compilation, "Tests that all the use cases for io_req compile", 10)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    // Note that this test is mainly for testing metaprogramming compilation, it doesn't really do much
    {
        auto dispatcher=make_dispatcher().get();
        auto mkdir(dispatcher->dir(path_req("testdir", file_flags::create)));
        mkdir.get();
        auto mkfile(dispatcher->file(path_req::relative(mkdir, "foo", file_flags::create|file_flags::read_write)));
        mkfile.get();
        auto last(dispatcher->truncate(mkfile, 1024));
        last.get();
        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        size_t length=sizeof(buffer);

        static_assert(detail::is_container<std::array<char, 2>>::value, "detail::is_container<std::array> isn't detecting a container");
        static_assert(!std::is_const<detail::is_container<std::array<char, 2>>::type>::value, "detail::is_container<std::array> thinks the container iterator const");
        static_assert(std::is_const<detail::is_container<const std::array<char, 2>>::type>::value, "detail::is_container<const std::array> thinks the container iterator not const");
        static_assert(detail::is_container<std::string>::value, "detail::is_container<std::string> isn't detecting a container");
        static_assert(!std::is_const<detail::is_container<std::string>::type>::value, "detail::is_container<std::string> thinks the container iterator const");
        static_assert(std::is_const<detail::is_container<const std::string>::type>::value, "detail::is_container<const std::string> thinks the container iterator not const");

        // ***************************** Static ASIO buffers composure checking *******************************
        // void *
        {
          typedef void type;
          type *buffer=(type *)(size_t)0xdeadbeef;
          size_t length=78;
          auto req(make_io_req(last, buffer, length, 0));
          BOOST_CHECK(typeid(req)==typeid(io_req<type>));
          BOOST_CHECK(typeid(req.buffers.front())==typeid(asio::mutable_buffer));
          BOOST_CHECK(req.buffers.size()==1);
          if(!req.buffers.empty())
          {
            BOOST_CHECK(asio::buffer_cast<type *>(req.buffers.front())==buffer);
            BOOST_CHECK(asio::buffer_size(req.buffers.front())==length*1);
          }
        }
        // const double *
        {
          typedef const double type;
          type *buffer=(type *)(size_t)0xdeadbeef;
          size_t length=78;
          auto req(make_io_req(last, buffer, length, 0));
          BOOST_CHECK(typeid(req)==typeid(io_req<type>));
          BOOST_CHECK(typeid(req.buffers.front())==typeid(asio::const_buffer));
          BOOST_CHECK(req.buffers.size()==1);
          if(!req.buffers.empty())
          {
            BOOST_CHECK(asio::buffer_cast<type *>(req.buffers.front())==buffer);
            BOOST_CHECK(asio::buffer_size(req.buffers.front())==length*sizeof(type));
          }
        }
        // double[]
        {
          typedef double type[78];
          double buffer[78];
          auto req(make_io_req(last, buffer, 0));
          BOOST_CHECK(typeid(req)==typeid(io_req<double>));
          BOOST_CHECK(typeid(req.buffers.front())==typeid(asio::mutable_buffer));
          BOOST_CHECK(req.buffers.size()==1);
          if(!req.buffers.empty())
          {
            BOOST_CHECK(asio::buffer_cast<double *>(req.buffers.front())==buffer);
            BOOST_CHECK(asio::buffer_size(req.buffers.front())==sizeof(buffer));
          }
        }
        // asio::mutable_buffer
        {
          typedef asio::mutable_buffer type;
          type b(buffer, length);
          auto req(make_io_req(last, b, 0));
          BOOST_CHECK(typeid(req)==typeid(io_req<type>));
          BOOST_CHECK(typeid(req.buffers.front())==typeid(asio::mutable_buffer));
          BOOST_CHECK(req.buffers.size()==1);
          if(!req.buffers.empty())
          {
            BOOST_CHECK(asio::buffer_cast<void *>(req.buffers.front())==buffer);
            BOOST_CHECK(asio::buffer_size(req.buffers.front())==length);
          }
        }
        // vector<asio::mutable_buffer>
        {
          typedef std::vector<asio::mutable_buffer> type;
          type b(4, type::value_type(buffer, length));
          auto req(make_io_req(last, b, 0));
          BOOST_CHECK(typeid(req)==typeid(io_req<type>));
          BOOST_CHECK(typeid(req.buffers.front())==typeid(asio::mutable_buffer));
          BOOST_CHECK(req.buffers.size()==b.size());
          for(auto &i : req.buffers)
          {
            BOOST_CHECK(asio::buffer_cast<void *>(i)==buffer);
            BOOST_CHECK(asio::buffer_size(i)==length);
          }
        }
        // asio::mutable_buffer[]
        {
          typedef asio::mutable_buffer type[2];
          type b={asio::mutable_buffer(buffer, length), asio::mutable_buffer(buffer, length)};
          auto req(make_io_req(last, b, 0));
          // Sequences of asio buffers are passed through
          BOOST_CHECK(typeid(req)==typeid(io_req<asio::mutable_buffer>));
          BOOST_CHECK(typeid(req.buffers.front())==typeid(asio::mutable_buffer));
          BOOST_CHECK(req.buffers.size()==2);
          for(auto &i : req.buffers)
          {
            BOOST_CHECK(asio::buffer_cast<void *>(i)==buffer);
            BOOST_CHECK(asio::buffer_size(i)==length);
          }
        }
        // std::list<std::list<std::string>>
        {
          typedef std::list<std::list<std::string>> type;
          type b={
            { "Niall", "Douglas"},
            { "was", "here" }
          };
          auto req(make_io_req(last, b, 0));
          BOOST_CHECK(typeid(req)==typeid(io_req<type>));
          BOOST_CHECK(typeid(req.buffers.front())==typeid(asio::mutable_buffer));
          BOOST_CHECK(req.buffers.size()==4);
          BOOST_CHECK(asio::buffer_cast<void *>(req.buffers[0])==b.front().front().c_str());
          BOOST_CHECK(asio::buffer_size(req.buffers[0])==b.front().front().size());
          BOOST_CHECK(asio::buffer_cast<void *>(req.buffers[1])==b.front().back().c_str());
          BOOST_CHECK(asio::buffer_size(req.buffers[1])==b.front().back().size());
          BOOST_CHECK(asio::buffer_cast<void *>(req.buffers[2])==b.back().front().c_str());
          BOOST_CHECK(asio::buffer_size(req.buffers[2])==b.back().front().size());
          BOOST_CHECK(asio::buffer_cast<void *>(req.buffers[3])==b.back().back().c_str());
          BOOST_CHECK(asio::buffer_size(req.buffers[3])==b.back().back().size());
        }
        // std::vector<std::vector<unsigned long>>
        {
          typedef std::vector<std::vector<unsigned long>> type;
          type b;
          b.push_back(std::vector<unsigned long>(5));
          b.push_back(std::vector<unsigned long>(6));
          b.push_back(std::vector<unsigned long>(7));
          b.push_back(std::vector<unsigned long>(8));
          auto req(make_io_req(last, b, 0));
          BOOST_CHECK(typeid(req)==typeid(io_req<type>));
          BOOST_CHECK(typeid(req.buffers.front())==typeid(asio::mutable_buffer));
          BOOST_CHECK(req.buffers.size()==4);
          for(size_t n=0; n<b.size(); n++)
          {
            BOOST_CHECK(asio::buffer_cast<void *>(req.buffers[n])==b[n].data());
            BOOST_CHECK(asio::buffer_size(req.buffers[n])==b[n].size()*sizeof(unsigned long));
          }
        }
        // std::unordered_set<std::string>
        {
          typedef std::unordered_set<std::string> type;
          type b;
          b.insert("Niall");
          auto req(make_io_req(last, b, 0));
          // unordered_set only provides const access to its iterators, so this needs to be a const buffer
          BOOST_CHECK(typeid(req)==typeid(io_req<const type>));
          BOOST_CHECK(typeid(req.buffers.front())==typeid(asio::const_buffer));
          BOOST_CHECK(req.buffers.size()==1);
          BOOST_CHECK(asio::buffer_cast<const void *>(req.buffers.front())==b.begin()->data());
          BOOST_CHECK(asio::buffer_size(req.buffers.front())==b.begin()->size());
        }
#if 0
        // std::initializer_list<const char *>
        {
          auto b={"Ni", "al", "l "};
          typedef decltype(b) type;
          auto req(make_io_req(last, b, 0));
          BOOST_CHECK(typeid(req)==typeid(io_req<type>));
          BOOST_CHECK(typeid(req.buffers.front())==typeid(asio::const_buffer));
          BOOST_CHECK(req.buffers.size()==3);
          auto it(b.begin());
          for(size_t n=0; n<b.size(); n++, ++it)
          {
            BOOST_CHECK(asio::buffer_cast<const void *>(req.buffers[n])==*it);
            BOOST_CHECK(asio::buffer_size(req.buffers[n])==2);
          }
        }
#endif

        // ***************************** Buffers into read/write *******************************
        // Base void * specialisation
        {
            typedef void type;
            typedef const type const_type;

            type *out=buffer;
            // works
            last=dispatcher->write(io_req<const_type>(last, out, length, 0));
            last.get();
            // auto-consts
            last=dispatcher->write(io_req<type>(last, out, length, 0));
            last.get();
            // works
            last=dispatcher->read(io_req<type>(last, out, length, 0));
            last.get();
            // deduces
            last=dispatcher->write(make_io_req(last, out, length, 0));
            last.get();
        }
        // char * specialisation
        {
            typedef char type;
            typedef const type const_type;

            type *out=buffer;
            // works
            last=dispatcher->write(io_req<const_type>(last, out, length, 0));
            last.get();
            // auto-consts
            last=dispatcher->write(io_req<type>(last, out, length, 0));
            last.get();
            // works
            last=dispatcher->read(io_req<type>(last, out, length, 0));
            last.get();
            // deduces
            last=dispatcher->write(make_io_req(last, out, length, 0));
            last.get();
        }
        // char array specialisation
        {
            typedef char type;
            typedef const type const_type;

            auto &out=buffer;
            // works
            last=dispatcher->write(io_req<const_type>(last, out, 0));
            last.get();
            // auto-consts
            last=dispatcher->write(io_req<type>(last, out, 0));
            last.get();
            // works
            last=dispatcher->read(io_req<type>(last, out, 0));
            last.get();
            // deduces
            last=dispatcher->write(make_io_req(last, out, 0));
            last.get();
        }
        // Arbitrary integral type array specialisation
        {
            typedef wchar_t type;
            typedef const type const_type;

            wchar_t out[sizeof(buffer)/sizeof(wchar_t)];
            memset(out, 0, sizeof(out));
            // works
            last=dispatcher->write(io_req<const_type>(last, out, 0));
            last.get();
            // auto-consts
            last=dispatcher->write(io_req<type>(last, out, 0));
            last.get();
            // works
            last=dispatcher->read(io_req<type>(last, out, 0));
            last.get();
            // deduces
            last=dispatcher->write(make_io_req(last, out, 0));
            last.get();
        }
        // string specialisation
        {
          typedef std::string type;
          typedef const type const_type;

          type out(sizeof(buffer)/sizeof(type::value_type), ' ');
          // works
          last=dispatcher->write(io_req<const_type>(last, out, 0));
          last.get();
          // auto-consts
          last=dispatcher->write(io_req<type>(last, out, 0));
          last.get();
          // works
          last=dispatcher->read(io_req<type>(last, out, 0));
          last.get();
          // deduces
          last=dispatcher->write(make_io_req(last, out, 0));
          last.get();
        }
        // asio::mutable_buffer specialisation
        {
          typedef asio::mutable_buffer type;
          typedef asio::const_buffer const_type;

          unsigned word=0xdeadbeef;
          type out(&word, 1);
          // works
          last=dispatcher->write(io_req<const_type>(last, out, 0));
          last.get();
          // auto-consts
          last=dispatcher->write(io_req<type>(last, out, 0));
          last.get();
          // works
          last=dispatcher->read(io_req<type>(last, out, 0));
          last.get();
          // deduces
          last=dispatcher->write(make_io_req(last, out, 0));
          last.get();
        }
        // vector specialisation
        {
            typedef std::vector<char> type;
            typedef const type const_type;

            type out(sizeof(buffer)/sizeof(type::value_type), ' ');
            // works
            last=dispatcher->write(io_req<const_type>(last, out, 0));
            last.get();
            // auto-consts
            last=dispatcher->write(io_req<type>(last, out, 0));
            last.get();
            // works
            last=dispatcher->read(io_req<type>(last, out, 0));
            last.get();
            // deduces
            last=dispatcher->write(make_io_req(last, out, 0));
            last.get();
        }
        // vector of asio::mutable_buffer specialisation
        {
            typedef std::vector<asio::mutable_buffer> type;
            typedef std::vector<asio::const_buffer> const_type;

            unsigned word=0xdeadbeef;
            type out(1, asio::mutable_buffer(&word, 1));
            // works
            last=dispatcher->write(io_req<const_type>(last, out, 0));
            last.get();
            // auto-consts
            last=dispatcher->write(io_req<type>(last, out, 0));
            last.get();
            // works
            last=dispatcher->read(io_req<type>(last, out, 0));
            last.get();
            // deduces
            last=dispatcher->write(make_io_req(last, out, 0));
            last.get();
        }
        // vector of string specialisation
        {
          typedef std::vector<std::string> type;
          typedef const type const_type;

          type out(sizeof(buffer)/sizeof(type::value_type), " ");
          // works
          last=dispatcher->write(io_req<const_type>(last, out, 0));
          last.get();
          // auto-consts
          last=dispatcher->write(io_req<type>(last, out, 0));
          last.get();
          // works
          last=dispatcher->read(io_req<type>(last, out, 0));
          last.get();
          // deduces
          last=dispatcher->write(make_io_req(last, out, 0));
          last.get();
        }
        // array specialisation
        {
            typedef std::array<char, sizeof(buffer)> type;
            typedef const type const_type;

            type out;
            out.fill(' ');
            // works
            last=dispatcher->write(io_req<const_type>(last, out, 0));
            last.get();
            // auto-consts
            last=dispatcher->write(io_req<type>(last, out, 0));
            last.get();
            // works
            last=dispatcher->read(io_req<type>(last, out, 0));
            last.get();
            // deduces
            last=dispatcher->write(make_io_req(last, out, 0));
            last.get();
        }
        // array of asio::mutable_buffer specialisation
        {
            typedef std::array<asio::mutable_buffer, 1> type;
            typedef std::array<asio::const_buffer, 1> const_type;

            unsigned word=0xdeadbeef;
            type out;
            out[0]=asio::mutable_buffer(&word, 1);
            // works
            last=dispatcher->write(io_req<const_type>(last, out, 0));
            last.get();
            // auto-consts
            last=dispatcher->write(io_req<type>(last, out, 0));
            last.get();
            // works
            last=dispatcher->read(io_req<type>(last, out, 0));
            last.get();
            // deduces
            last=dispatcher->write(make_io_req(last, out, 0));
            last.get();
        }
        // array of string specialisation
        {
          typedef std::array<std::string, sizeof(buffer)/sizeof(std::string::value_type)> type;
          typedef const type const_type;

          type out;
          out.fill(" ");
          // works
          last=dispatcher->write(io_req<const_type>(last, out, 0));
          last.get();
          // auto-consts
          last=dispatcher->write(io_req<type>(last, out, 0));
          last.get();
          // works
          last=dispatcher->read(io_req<type>(last, out, 0));
          last.get();
          // deduces
          last=dispatcher->write(make_io_req(last, out, 0));
          last.get();
        }
        // Test read-only container
        {
          typedef std::unordered_set<char> type;
          typedef const type const_type;

          type out; out.insert(32);
          // works
          last=dispatcher->write(io_req<const_type>(last, out, 0));
          last.get();
#ifdef BOOST_AFIO_TEST_ASYNC_DATA_OP_REQ_FAILURE_TO_COMPILE
          // auto-consts
          last=dispatcher->write(io_req<type>(last, out, 0));
          last.get();
          // works
          last=dispatcher->read(io_req<type>(last, out, 0));
          last.get();
#endif
          // deduces
          last=dispatcher->write(make_io_req(last, out, 0));
          last.get();
        }
        last=dispatcher->rmfile(last);
        last.get();
        last=dispatcher->close(last);
        last.get();
    }
    try { filesystem::remove_all("testdir"); } catch(...) {}
}