#include "boost/afio/afio.hpp"
#include <iostream>

/*  My Intel Core i7 3770K running Windows 8 x64: 337955 closures/sec
    My Intel Core i7 3770K running     Linux x64: 398537 closures/sec
*/

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) && !(defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */)
	using namespace boost::afio;
    auto dispatcher=make_async_file_io_dispatcher();
	typedef std::chrono::duration<double, std::ratio<1>> secs_type;
	auto begin=std::chrono::high_resolution_clock::now();
	while(std::chrono::duration_cast<secs_type>(std::chrono::high_resolution_clock::now()-begin).count()<3);
	
	auto callback=std::function<int()>([]
	{
		return 1;
	});
	std::vector<async_io_op> preconditions;
	std::vector<std::function<int()>> callbacks(100000, callback);
	begin=std::chrono::high_resolution_clock::now();
	for(size_t n=0; n<50; n++)
		dispatcher->call(preconditions, callbacks);
	while(dispatcher->count())
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	auto end=std::chrono::high_resolution_clock::now();
	auto diff=std::chrono::duration_cast<secs_type>(end-begin);
	std::cout << "It took " << diff.count() << " secs to execute 5,000,000 closures which is " << (5000000/diff.count()) << " unchained closures/sec" << std::endl;
	std::cout << "\nPress Return to exit ..." << std::endl;
	getchar();
#endif
	return 0;
}
