/* Niall's Nasty 256 bit hash
(C) 2013 Niall Douglas http://www.nedprod.com

Literally combines cityhash 128 bit and spookyhash 128 bit to make a 256 bit hash
which isn't fast, but it's the fastest reasonably good 256 bit hash I can make quickly.
Created: Feb 2013
*/

#include "niallsnasty256hash.hpp"
#include "../cityhash/src/city.cc"
#include "../spookyhash/SpookyV2.cpp"
#include <omp.h>

#if ALLOW_UNALIGNED_READS
#error ALLOW_UNALIGNED_READS needs to be zero for ARM compatibility
#endif

namespace NiallsNasty256Hash {

void AddToHash256(Hash256 &hash, const char *data, size_t length)
{
	uint64 *spookyhash=const_cast<unsigned long long *>(hash.asLongLongs());
	uint128 cityhash=*(uint128 *)(hash.asLongLongs()+2);
#pragma omp parallel for if(length>=1024)
	for(int n=0; n<2; n++)
	{
		if(!n)
			SpookyHash::Hash128(data, length, spookyhash, spookyhash+1);
		else
			cityhash=CityHash128WithSeed(data, length, cityhash);
	}
	*(uint128 *)(hash.asLongLongs()+2)=cityhash;
}

} // namespace
