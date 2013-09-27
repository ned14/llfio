#include "boost/afio/afio.hpp"
#include <iostream>

/*  My Intel Core i7 3770K running Windows 8 x64: 143409 closures/sec
    My Intel Core i7 3770K running     Linux x64: 244907 closures/sec
*/

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) && !(defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */)
	using namespace boost::afio;
    auto dispatcher=make_async_file_io_dispatcher();
	typedef std::chrono::duration<double, std::ratio<1>> secs_type;
	auto begin=std::chrono::high_resolution_clock::now();
	while(std::chrono::duration_cast<secs_type>(std::chrono::high_resolution_clock::now()-begin).count()<3);
	
	auto callback=std::make_pair(async_op_flags::None,
		std::function<async_file_io_dispatcher_base::completion_t>([](size_t, std::shared_ptr<async_io_handle> h, exception_ptr *)
	{
#if 0
		Sleep(0);
#endif
		return std::make_pair(true, h);
	}));
	size_t threads=0;
#if 0
	std::cout << "Attach profiler now and hit Return" << std::endl;
	getchar();
#endif
	begin=std::chrono::high_resolution_clock::now();
#pragma omp parallel num_threads(64)
	{
		async_io_op last;
		threads++;
		for(size_t n=0; n<50000; n++)
		{
			last=dispatcher->completion(last, callback);
		}
	}
	auto end=std::chrono::high_resolution_clock::now();
	auto diff=std::chrono::duration_cast<secs_type>(end-begin);
	std::cout << "It took " << diff.count() << " secs to execute " << (50000*threads) << " closures which is " << (50000*threads/diff.count()) << " chained closures/sec" << std::endl;
	std::cout << "\nPress Return to exit ..." << std::endl;
	getchar();
#endif
	return 0;
}
