/* TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#include "../include/triplegit.hpp"
#include "../include/async_file_io.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996) // Function call with parameters that may be unsafe
#endif
#include "boost/lockfree/spsc_queue.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <atomic>

using namespace std;
using namespace NiallsCPP11Utilities;

namespace triplegit
{
namespace detail
{
	// Entropy is somewhat expensive to construct and tear down
	// per 16 or 32 byte request, so batch gather into a lockfree
	// ringbuffer
	static boost::lockfree::spsc_queue<unsigned long long, boost::lockfree::capacity<1024>> randomness_cache; // 8Kb
	static atomic<size_t> randomness_cache_left;
	static void fill_randomness_cache()
	{
		const size_t amount=4096/sizeof(Int256);
		Int256 buffer[amount];
		Int256::FillQualityRandom(buffer, amount);
		unsigned long long *_buffer=(unsigned long long *) buffer;
		randomness_cache.push(_buffer, amount*4);
		randomness_cache_left+=amount*sizeof(Int256);
	}
	void prefetched_unique_id_source(void *ptr, size_t size)
	{
		if(!randomness_cache_left)
			fill_randomness_cache();
		else if(randomness_cache_left<4096)
			async_io::process_threadpool().enqueue(fill_randomness_cache);
		size/=8;
		for(size_t n=0; n<size; n+=randomness_cache.pop(((unsigned long long *) ptr)+n, size-n));
	}

	void *storable_vertices::begin_batch_attachdetach()
	{
		return nullptr;
	}
	void storable_vertices::do_batch_attach(void *p, base_store &store, collection_id id)
	{
	}
	void storable_vertices::do_batch_detach(void *p, base_store &store, collection_id id)
	{
	}
	void storable_vertices::end_batch_attachdetach(void *p)
	{
	}

} // namespace


} // namespace
