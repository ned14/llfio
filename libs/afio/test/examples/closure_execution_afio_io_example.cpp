#include <boost/afio/afio.hpp>
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) // Don't bother with VS2010, its result_of can't cope.
//[closure_execution_afio_example
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/afio/afio.hpp>

int main()
{
    const int ary_size = 10;

    //set up a file to read from
    std::ofstream out_file("somefile.dat", std::ios::binary);
    for (int i = 0; i < ary_size; ++i)
    {
        out_file.write(reinterpret_cast<const char*>(&i), sizeof(i));
    }
    out_file.close();
    

    //set up the afio dispatcher
    auto dispatcher = boost::afio::make_async_file_io_dispatcher();

    //set up an array to hold our integers
    int ary[ary_size];

    //schedule the file open
    auto opened_file = dispatcher->file(boost::afio::async_path_op_req("somefile.dat", 
        boost::afio::file_flags::Read));

    //set up vectors for the individual read operations, and the work on each integer
    std::vector<boost::afio::async_io_op> read_ops(ary_size);
    std::vector<std::function<void()>> vec_func(ary_size);
    for (int i = 0; i < ary_size; ++i)
    {
         read_ops[i] = dispatcher->read(boost::afio::async_data_op_req<int>(opened_file, 
            &ary[i], sizeof(int), i*sizeof(int)));
        
         vec_func[i] = std::bind([](int* a){ *a *= 2 ; }, &ary[i]);
    }
    
    // schedule the work to be done after reading in an integer
    auto work = dispatcher->call(read_ops, vec_func);
    
    //schedule the file to be closed after reads are finished
    auto closed_file = dispatcher->close(dispatcher->barrier(read_ops).front());
    
    // make sure work has completed before trying to print data from the array
    boost::afio::when_all(work.second.begin(), work.second.end()).wait();
    
    //verify the out put is as expected: "0, 2, 4, 6, 8, 10, 12, 14, 16, 18"
     for (int i = 0; i < ary_size; ++i)
    {
        std::cout << ary[i];
        if(i == ary_size-1)
            std::cout << std::endl;
        else
            std::cout << ", ";
    }

    return 0;
}
//]
#else
int main()
{
    return 0;
}
#endif