/* Int128_256.hpp
(C) 2013 Niall Douglas http://www.nedprod.com
Created: Feb 2013

AVX2 accelerated 256 bit integer implementation. Falls back to SSE2/NEON 128 bit integers, or even char[32] if needed.

Literally combines cityhash 128 bit and spookyhash 128 bit to make a 256 bit hash
which isn't fast, but it's the fastest reasonably good 256 bit hash I can make quickly.
*/

#include "Int128_256.hpp"
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include "hashes/cityhash/src/city.cc"
#include "hashes/spookyhash/SpookyV2.cpp"
#include "hashes/sha256/sha256-ref.c"
#if HAVE_M128
#include "hashes/sha256/sha256-sse.c"
#endif
#if HAVE_NEON128
#include "hashes/sha256/sha256-neon.c"
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#ifdef _OPENMP
#include <omp.h>
#endif
#include <random>
#ifdef WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

#if ALLOW_UNALIGNED_READS
#error ALLOW_UNALIGNED_READS needs to be zero for ARM compatibility
#endif

namespace NiallsCPP11Utilities {

template<class generator_type> void FillRandom(char *buffer, size_t length)
{
	// No speed benefit so disabled
//#pragma omp parallel if(0 && length>=1024) num_threads((int)(length/256)) schedule(dynamic)
	{
#if 0 //def _OPENMP
		const int partitions=omp_get_num_threads();
		const int thispartition=omp_get_thread_num();
#else
		const int partitions=1;
		const int thispartition=0;
#endif
		const size_t thislength=(length/partitions)&~(sizeof(typename generator_type::result_type)-1);
		const size_t thisno=thislength/sizeof(typename generator_type::result_type);
		random_device rd;
		generator_type gen(rd());
		typename generator_type::result_type *tofill=(typename generator_type::result_type *) buffer;
		if(thispartition)
			tofill+=thisno*thispartition;
		for(size_t n=0; n<thisno; n++)
			*tofill++=gen();
		if(thispartition==partitions-1)
		{
			for(size_t remaining=length-(thislength*partitions); remaining>0; remaining-=sizeof(typename generator_type::result_type))
			{
				*tofill++=gen();
			}
		}
	}
}

void Int128::FillFastRandom(Int128 *ints, size_t no)
{
	size_t length=no*sizeof(*ints);
	if(no && no!=length/sizeof(*ints)) abort();
#if HAVE_M128 || HAVE_NEON128
	// The Mersenne Twister's SIMD implementation beats all else
#ifdef __LP64__
	typedef mt19937_64 generator_type;
#else
	typedef mt19937 generator_type;
#endif
#else
#ifdef __LP64__
	typedef ranlux48_base generator_type;
#else
	typedef ranlux24_base generator_type;
#endif
#endif
	FillRandom<generator_type>((char *) ints, length);
}

void Int128::FillQualityRandom(Int128 *ints, size_t no)
{
	size_t length=no*sizeof(*ints);
	if(no && no!=length/sizeof(*ints)) abort();
#ifdef __LP64__
	typedef mt19937_64 generator_type;
#else
	typedef mt19937 generator_type;
#endif
	FillRandom<generator_type>((char *) ints, length);
}

void Int256::FillFastRandom(Int256 *ints, size_t no)
{
	size_t length=no*sizeof(*ints);
	if(no && no!=length/sizeof(*ints)) abort();
#if HAVE_M128 || HAVE_NEON128
	// The Mersenne Twister's SIMD implementation beats all else
#ifdef __LP64__
	typedef mt19937_64 generator_type;
#else
	typedef mt19937 generator_type;
#endif
#else
#ifdef __LP64__
	typedef ranlux48_base generator_type;
#else
	typedef ranlux24_base generator_type;
#endif
#endif
	FillRandom<generator_type>((char *) ints, length);
}

void Int256::FillQualityRandom(Int256 *ints, size_t no)
{
	size_t length=no*sizeof(*ints);
	if(no && no!=length/sizeof(*ints)) abort();
#ifdef __LP64__
	typedef mt19937_64 generator_type;
#else
	typedef mt19937 generator_type;
#endif
	FillRandom<generator_type>((char *) ints, length);
}

void Hash128::AddFastHashTo(const char *data, size_t length)
{
	uint64 *spookyhash=(uint64 *) const_cast<unsigned long long *>(asLongLongs());
	SpookyHash::Hash128(data, length, spookyhash, spookyhash+1);
}

void Hash128::BatchAddFastHashTo(size_t no, Hash128 *hashs, const char **data, size_t *length)
{
	// TODO: Implement a SIMD version of SpookyHash, and parallelise that too :)
#pragma omp parallel for schedule(dynamic)
	for(ptrdiff_t n=0; n<(ptrdiff_t) no; n++)
		hashs[n].AddFastHashTo(data[n], length[n]);
}


void Hash256::AddFastHashTo(const char *data, size_t length)
{
	uint64 *spookyhash=(uint64 *) const_cast<unsigned long long *>(asLongLongs());
	uint128 cityhash=*(uint128 *)(asLongLongs()+2);
#pragma omp parallel for if(length>=1024)
	for(int n=0; n<2; n++)
	{
		if(!n)
			SpookyHash::Hash128(data, length, spookyhash, spookyhash+1);
		else
			cityhash=CityHash128WithSeed(data, length, cityhash);
	}
	*(uint128 *)(asLongLongs()+2)=cityhash;
}

void Hash256::AddSHA256To(const char *data, size_t length)
{
	const __sha256_block_t *blks=(const __sha256_block_t *) data;
	size_t no=length/sizeof(__sha256_block_t);
	size_t remaining=length-(no*sizeof(__sha256_block_t));
	for(size_t n=0; n<no; n++)
		__sha256_osol(*blks++, const_cast<unsigned int *>(asInts())); 

	// Do termination
	__sha256_block_t temp;
	memset(temp, 0, sizeof(temp));
	memcpy(temp, blks, remaining);
	// Pad to 56 bytes
	if(remaining<56)
		temp[remaining]=0x80;
	else
	{
		temp[remaining]=0x80;
		// Insufficient space for termination, so another round
		__sha256_osol(temp, const_cast<unsigned int *>(asInts()));
		memset(temp, 0, sizeof(temp));
	}
	*(uint64_t *)(temp+56)=bswap_64(8*length);
	__sha256_osol(temp, const_cast<unsigned int *>(asInts())); 
	// Finally, as we're little endian flip back the words
	for(int n=0; n<8; n++)
		const_cast<unsigned int *>(asInts())[n]=LOAD_BIG_32(asInts()+n);
}

struct HashOp
{
	size_t no;
	Hash256 *hashs;
	enum class HashType
	{
		Unknown,
		FastHash,
		SHA256
	} hashType;
	struct TYPEALIGNMENT(32) Scratch
	{
		char d[64];
		size_t pos, length;
		char ___pad[32-2*sizeof(size_t)];
	};
	aligned_allocator<Scratch, 32> alloc;
	Scratch *scratch; // Only used for SHA-256
	HashOp(size_t _no, Hash256 *_hashs) : no(_no), hashs(_hashs), hashType(HashType::Unknown), scratch(0) { }
	void make_scratch()
	{
		if(!scratch)
		{
			scratch=alloc.allocate(no);
			memset(scratch, 0, no*sizeof(Scratch));
		}
	}
	~HashOp()
	{
		if(scratch)
			alloc.deallocate(scratch, no);
	}
};

Hash256::BatchHashOp Hash256::BeginBatch(size_t no, Hash256 *hashs)
{
	return new HashOp(no, hashs);
}
void Hash256::AddFastHashToBatch(BatchHashOp _h, size_t items, const BatchItem *datas)
{
	auto h=(HashOp *) _h;
	if(h->hashType==HashOp::HashType::Unknown)
		h->hashType=HashOp::HashType::FastHash;
	else if(h->hashType!=HashOp::HashType::FastHash)
		throw std::runtime_error("You can't add a fast hash to a SHA-256 hash");
#pragma omp parallel for schedule(dynamic)
	for(ptrdiff_t n=0; n<(ptrdiff_t) items; n++)
	{
		auto &data=datas[n];
		h->hashs[get<0>(data)].AddFastHashTo(get<1>(data), get<2>(data));
	}
}
void Hash256::AddSHA256ToBatch(BatchHashOp _h, size_t no, const BatchItem *datas)
{
	auto h=(HashOp *) _h;
	if(h->hashType==HashOp::HashType::Unknown)
		h->hashType=HashOp::HashType::SHA256;
	else if(h->hashType!=HashOp::HashType::SHA256)
		throw std::runtime_error("You can't add a SHA-256 hash to a fast hash");
	h->make_scratch();
	// TODO: No reason this can't OpenMP parallelise given sufficient no
	__sha256_block_t emptyblk;
	size_t hashidxs[4]={0};
	const __sha256_block_t *blks[4]={&emptyblk, &emptyblk, &emptyblk, &emptyblk};
	size_t togos[4]={0};
	__sha256_hash_t emptyout;
	__sha256_hash_t *out[4]={&emptyout, &emptyout, &emptyout, &emptyout};
	int inuse=0;
	auto retire=[h, &hashidxs, &emptyblk, &blks, &togos, &emptyout, &out](size_t n){
		size_t hashidx=hashidxs[n];
		memcpy(h->scratch[hashidx].d, blks[n], togos[n]);
		h->scratch[hashidx].pos=togos[n];
		blks[n]=&emptyblk;
		out[n]=&emptyout;
	};
	do
	{
		// Fill SHA streams with work
		if(no)
		{
			for(size_t n=0; n<4; n++)
			{
				while(&emptyblk==blks[n] && no)
				{
					auto &data=*datas;
					hashidxs[n]=get<0>(data);
					if(h->scratch[hashidxs[n]].pos)
						throw std::runtime_error("Feeding SHA-256 with chunks not exactly divisible by 64 bytes, except as the very final chunk, is currently not supported.");
					else
						blks[n]=(const __sha256_block_t *) get<1>(data);
					out[n]=(__sha256_hash_t *) const_cast<unsigned int *>(h->hashs[hashidxs[n]].asInts());
					h->scratch[hashidxs[n]].length=togos[n]=get<2>(data);
					datas++;
					no--;
					// Too small, so retire instantly
					if(togos[n]<sizeof(__sha256_block_t))
						retire(n);
					else
						inuse++;
				}
			}
		}
#if HAVE_M128 || defined(HAVE_NEON128)
		__sha256_int(blks, out); 
#else
//#pragma omp parallel for
		for(size_t n=0; n<4; n++)
			__sha256_osol(*blks[n], *out[n]);
#endif
		for(size_t n=0; n<4; n++)
		{
			if(&emptyblk==blks[n]) continue;
			blks[n]++;
			togos[n]-=sizeof(__sha256_block_t);
			if(togos[n]<sizeof(__sha256_block_t))
			{
				retire(n);
				inuse--;
			}
		}
		// We know from benchmarking that the above can push 3.5 streams in the time of a single stream,
		// so keep going if there are at least two streams remaining
	} while(inuse>1);
	if(inuse)
	{
		for(size_t n=0; n<4; n++)
		{
			if(&emptyblk!=blks[n])
			{
				do
				{
					__sha256_osol(*blks[n], *out[n]);
					blks[n]++;
					togos[n]-=sizeof(__sha256_block_t);
				} while(togos[n]>=sizeof(__sha256_block_t));
				retire(n);
				inuse--;
			}
		}
	}
}
static void _FinishBatch(HashOp *h)
{
	switch(h->hashType)
	{
	case HashOp::HashType::FastHash:
		{
			break;
		}
	case HashOp::HashType::SHA256:
		{
			// Terminate all remaining hashes
			__sha256_block_t emptyblk;
			const __sha256_block_t *blks[4]={&emptyblk, &emptyblk, &emptyblk, &emptyblk};
			__sha256_hash_t emptyout;
			__sha256_hash_t *out[4]={&emptyout, &emptyout, &emptyout, &emptyout};
			int inuse=0;
			// First run is to find all hashes with scratchpos>=56 as these need an extra round
			for(size_t n=0; n<h->no; n++)
			{
				if(h->scratch[n].pos>=56)
				{
					memset(h->scratch[n].d+h->scratch[n].pos, 0, sizeof(__sha256_block_t)-h->scratch[n].pos);
					h->scratch[n].d[h->scratch[n].pos]=(unsigned char) 0x80;
					blks[inuse]=(const __sha256_block_t *) h->scratch[n].d;
					out[inuse]=(__sha256_hash_t *) h->hashs[n].asInts();
					if(4==++inuse)
					{
#if HAVE_M128 || defined(HAVE_NEON128)
						__sha256_int(blks, out);
#else
//#pragma omp parallel for
						for(size_t z=0; z<4; z++)
							__sha256_osol(*blks[z], *out[z]);
#endif
						inuse=0;
					}
					h->scratch[n].pos=0;
				}
			}
			if(inuse)
			{
				for(size_t n=inuse; n<4; n++)
				{
					blks[n]=&emptyblk;
					out[n]=&emptyout;
				}
#if HAVE_M128 || defined(HAVE_NEON128)
				__sha256_int(blks, out);
#else
//#pragma omp parallel for
				for(size_t z=0; z<4; z++)
					__sha256_osol(*blks[z], *out[z]);
#endif
				inuse=0;
			}
			// First run is to find all hashes with scratchpos>=56 as these need an extra round
			for(size_t n=0; n<h->no; n++)
			{
				PACKEDTYPE(struct termination_t
				{
					char data[56];
					uint64_t length;
				});
				termination_t *termination=(termination_t *) h->scratch[n].d;
				static_assert(sizeof(*termination)==64, "termination_t is not sized exactly 64 bytes!");
				memset(termination->data+h->scratch[n].pos, 0, sizeof(__sha256_block_t)-h->scratch[n].pos);
				termination->data[h->scratch[n].pos]=(unsigned char) 0x80;
				termination->length=bswap_64(8*h->scratch[n].length);
				blks[inuse]=(const __sha256_block_t *) h->scratch[n].d;
				out[inuse]=(__sha256_hash_t *) h->hashs[n].asInts();
				if(4==++inuse)
				{
#if HAVE_M128 || defined(HAVE_NEON128)
					__sha256_int(blks, out);
#else
//#pragma omp parallel for
					for(size_t z=0; z<4; z++)
						__sha256_osol(*blks[z], *out[z]);
#endif
					inuse=0;
				}
			}
			if(inuse)
			{
				for(size_t n=inuse; n<4; n++)
				{
					blks[n]=&emptyblk;
					out[n]=&emptyout;
				}
#if HAVE_M128 || defined(HAVE_NEON128)
				__sha256_int(blks, out);
#else
//#pragma omp parallel for
				for(size_t z=0; z<4; z++)
					__sha256_osol(*blks[z], *out[z]);
#endif
				inuse=0;
			}
			// As we're little endian flip back the words
			for(size_t n=0; n<h->no; n++)
			{
				for(int m=0; m<8; m++)
					*const_cast<unsigned int *>(h->hashs[n].asInts()+m)=LOAD_BIG_32(h->hashs[n].asInts()+m);
			}
			break;
		}
	}
}
void Hash256::FinishBatch(BatchHashOp _h, bool free)
{
	auto h=(HashOp *) _h;
	_FinishBatch(h);
	if(free)
		delete h;
	else
		h->hashType=HashOp::HashType::Unknown;
}

void Hash256::BatchAddFastHashTo(size_t no, Hash256 *hashs, const char **data, size_t *length)
{
	HashOp h(no, hashs);
	BatchItem *items=(BatchItem *) alloca(sizeof(BatchItem)*no);
	for(size_t n=0; n<no; n++)
		items[n]=BatchItem(n, data[n], length[n]);
	AddFastHashToBatch(&h, no, items);
	_FinishBatch(&h);
}

void Hash256::BatchAddSHA256To(size_t no, Hash256 *hashs, const char **data, size_t *length)
{
	HashOp h(no, hashs);
	BatchItem *items=(BatchItem *) alloca(sizeof(BatchItem)*no);
	for(size_t n=0; n<no; n++)
		items[n]=BatchItem(n, data[n], length[n]);
	AddSHA256ToBatch(&h, no, items);
	_FinishBatch(&h);
}



} // namespace
