#include "boost/afio/afio.hpp"
#include <iostream>

/*  My Intel Core i7 3770K running Windows 8 x64: 2591360 closures/sec
    My Intel Core i7 3770K running     Linux x64: 1611040 closures/sec (4 threads)
*/

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) && !(defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */)
	using namespace boost::afio;
	typedef std::chrono::duration<double, std::ratio<1>> secs_type;
	auto threadpool=process_threadpool();
	std::atomic<size_t> togo(0);
	auto begin=std::chrono::high_resolution_clock::now();
	while(std::chrono::duration_cast<secs_type>(std::chrono::high_resolution_clock::now()-begin).count()<3);
	
	auto callback=[&togo]()
	{
#if 0
		Sleep(0);
#endif
		--togo;
		return 1;
	};
	std::atomic<size_t> threads(0);
#if 0
	std::cout << "Attach profiler now and hit Return" << std::endl;
	getchar();
#endif
	begin=std::chrono::high_resolution_clock::now();
#pragma omp parallel
	{
		++threads;
		for(size_t n=0; n<5000000; n++)
		{
			++togo;
			threadpool->enqueue(callback);
		}
	}
	while(togo)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	auto end=std::chrono::high_resolution_clock::now();
	auto diff=std::chrono::duration_cast<secs_type>(end-begin);
	std::cout << "It took " << diff.count() << " secs to execute " << (5000000*threads) << " closures which is " << (5000000*threads/diff.count()) << " closures/sec" << std::endl;
	std::cout << "\nPress Return to exit ..." << std::endl;
	getchar();
#endif
	return 0;
}
