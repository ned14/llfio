/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#include "../include/async_file_io.hpp"

namespace triplegit { namespace async_io {

thread_pool &process_threadpool()
{
	static thread_pool ret(8);
	return ret;
}

} } // namespace
