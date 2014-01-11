#include "afio_pch.hpp"
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) // Don't bother with VS2010, its result_of can't cope.
//[closure_execution_traditional_example
#include <iostream>
#include <fstream>

int main()
{
    
    const int ary_size = 10;

    // set up a file to read from
    std::ofstream out_file("somefile.dat", std::ios::binary);
    for (int i = 0; i < ary_size; ++i)
    {
        out_file.write(reinterpret_cast<const char*>(&i), sizeof(i));
    }
    out_file.close();
    
    //setup an array of integers
    int ary[ary_size];
    //file open
    std::ifstream file("somefile.dat");

    //read in ints to ary
    if (file)
    {
        for (int i = 0; i < ary_size; ++i)
        {
            file.read((char*) &ary[i], sizeof(ary[i]));
        }
        //close file
        file.close();

        
        //do some work with the array of ints
        for (int i = 0; i < ary_size; ++i)
            ary[i] *= 2;
    } 

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
#endif