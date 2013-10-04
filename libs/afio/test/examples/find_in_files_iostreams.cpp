#include "boost/exception/diagnostic_information.hpp"
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700)
#include <iostream>
#include <fstream>
#include <regex>
#include <chrono>
#include "boost/afio/detail/std_filesystem.hpp" // in lieu of <filesystem>
#endif

/* My Intel Core i7 3770K running Windows 8 x64 with 7200rpm drive, using
Sysinternals RAMMap to clear disc cache (http://technet.microsoft.com/en-us/sysinternals/ff700229.aspx)

Single threaded, warm cache:
92 files matched out of 39279 files which was 7093894518 bytes.
The search took 8.32834 seconds which was 4716.3 files per second or 812.318 Mb/sec.

Single threaded, cold cache:
91 files matched out of 38967 files which was 6170927489 bytes.
The search took 369.046 seconds which was 105.588 files per second or 15.9467 Mb/sec.

OpenMP, warm cache:
92 files matched out of 38943 files which was 7092655881 bytes.
The search took 3.73611 seconds which was 10423.4 files per second or 1810.46 Mb/sec.

OpenMP, cold cache:
91 files matched out of 38886 files which was 6170656567 bytes.
The search took 741.131 seconds which was 52.4684 files per second or 7.94029 Mb/sec.
*/

#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700)
//[find_in_files_iostreams
int main(int argc, const char *argv[])
{
    using namespace std;
    using filesystem::ifstream;
    typedef chrono::duration<double, ratio<1>> secs_type;
    if(argc<2)
    {
        cerr << "ERROR: Specify a regular expression to search all files in the current directory." << endl;
        return 1;
    }
    // Prime SpeedStep
    auto begin=chrono::high_resolution_clock::now();
    while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<1);
    size_t bytesread=0, filesread=0, filesmatched=0;
    try
    {
        begin=chrono::high_resolution_clock::now();

        // Generate a list of all files here and below the current working directory
        vector<filesystem::path> filepaths;
        for(auto it=filesystem::recursive_directory_iterator("."); it!=filesystem::recursive_directory_iterator(); ++it)
        {
            if(it->status().type()!=filesystem::regular_file)
                continue;
            filepaths.push_back(it->path());
        }

        // Compile the regular expression, and have OpenMP parallelise the loop
        regex regexpr(argv[1]);
#pragma omp parallel for schedule(dynamic)
        for(int n=0; n<(int) filepaths.size(); n++)
        {
            // Open the file
            ifstream s(filepaths[n], ifstream::binary);
            s.exceptions(fstream::failbit | fstream::badbit); // Turn on exception throwing
            // Get its length
            s.seekg(0, ios::end);
            size_t length=(size_t) s.tellg();
            s.seekg(0, ios::beg);
            // Allocate a sufficient buffer, avoiding the byte clearing vector would do
            unique_ptr<char[]> buffer(new char[length+1]);
            // Read in the file, terminating with zero
            s.read(buffer.get(), length);
            buffer.get()[length]=0;
            // Search the buffer for the regular expression
            if(regex_search(buffer.get(), regexpr))
            {
#pragma omp critical
                {
                    cout << filepaths[n] << endl;
                }
                filesmatched++;
            }
            filesread++;
            bytesread+=length;
        }
        auto end=chrono::high_resolution_clock::now();
        auto diff=chrono::duration_cast<secs_type>(end-begin);
        cout << "\n" << filesmatched << " files matched out of " << filesread << " files which was " << bytesread << " bytes." << endl;
        cout << "The search took " << diff.count() << " seconds which was " << filesread/diff.count() << " files per second or " << (bytesread/diff.count()/1024/1024) << " Mb/sec." << endl;
    }
    catch(...)
    {
        cerr << boost::current_exception_diagnostic_information(true) << endl;
        return 1;
    }
    return 0;
}
//]
#else
int main(void) { return 0; }
#endif
