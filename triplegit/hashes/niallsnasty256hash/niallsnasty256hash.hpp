/* Niall's Nasty 256 bit hash
(C) 2013 Niall Douglas http://www.nedprod.com

Literally combines cityhash 128 bit and spookyhash 128 bit to make a 256 bit hash
which isn't fast, but it's the fastest reasonably good 256 bit hash I can make quickly.
Created: Feb 2013
*/

#ifndef NIALLSNASTY256HASH_H
#define NIALLSNASTY256HASH_H

#include "../../../NiallsCPP11Utilities/NiallsCPP11Utilities.hpp"
#include <exception>
#include <string>

#ifndef HAVE_M256
#ifdef _MSC_VER
#if _M_IX86_FP>=3 || defined(__AVX2__)
#define HAVE_M256 1
#include <immintrin.h>
#endif
#elif defined(__GNUC__)
#if defined(__AVX2__)
#define HAVE_M256 1
#include <immintrin.h>
#endif
#endif
#endif

#ifndef HAVE_M128
#ifdef _MSC_VER
#if _M_IX86_FP>=2 || defined(_M_AMD64) || _M_ARM_FP>=40
#define HAVE_M128 1
#include <emmintrin.h>
#endif
#elif defined(__GNUC__)
#if defined(__SSE2__) || defined(__ARM_NEON__)
#define HAVE_M128 1
#include <emmintrin.h>
#endif
#endif
#endif

#ifndef HAVE_M128
#define HAVE_M128 0
#endif
#ifndef HAVE_M256
#define HAVE_M256 0
#endif

namespace NiallsNasty256Hash {

/*! \class Hash256
\brief Declares a 256 bit AVX/SSE/NEON compliant container. WILL throw exception if initialised unaligned.
*/
class TYPEALIGNMENT(32) Hash256
{
	union
	{
		char asBytes[32];
		unsigned long long asLongLongs[4];
		size_t asSize_t;
#if HAVE_M128
		__m128i asM128s[2];
#endif
#if HAVE_M256
		__m256i asM256;
#endif
	} mydata;
	void int_testAlignment() const
	{
		if(((size_t)this) & 31) throw std::runtime_error("This object must be aligned to 32 bytes in memory!");
	}
	static char int_tohex(int x)
	{
		return (x>9) ? 'a'+x-10 : '0'+x;
	}
public:
	//! Constructs an empty hash
	Hash256() { int_testAlignment(); memset(this, 0, sizeof(*this)); }
#if HAVE_M256
	Hash256(const Hash256 &o) { int_testAlignment(); mydata.asM256=_mm256_load_si256(&o.mydata.asM256); }
	Hash256 &operator=(const Hash256 &o) { mydata.asM256=_mm256_load_si256(&o.mydata.asM256); return *this; }
	explicit Hash256(const char *bytes) { int_testAlignment(); mydata.asM256=_mm256_loadu_si256((const __m256i *) bytes); }
	bool operator==(const Hash256 &o) const
	{
		__m256i result=_mm256_cmpeq_epi64(mydata.asM256, o.mydata.asM256);
		return !(~_mm256_movemask_epi8(result));
	}
	bool operator>(const Hash256 &o) const
	{
		__m256i result[2];
		result[0]=_mm256_cmpgt_epi64(mydata.asM256, o.mydata.asM256);
		result[1]=_mm256_cmpgt_epi64(o.mydata.asM256, mydata.asM256);
		unsigned r[2];
		r[0]=_mm256_movemask_epi8(result[0]);
		r[1]=_mm256_movemask_epi8(result[1]);
		return r[0]>r[1];
	}
#elif HAVE_M128
	Hash256(const Hash256 &o) { int_testAlignment(); mydata.asM128s[0]=_mm_load_si128(&o.mydata.asM128s[0]); mydata.asM128s[1]=_mm_load_si128(&o.mydata.asM128s[1]); }
	Hash256 &operator=(const Hash256 &o) { mydata.asM128s[0]=_mm_load_si128(&o.mydata.asM128s[0]); mydata.asM128s[1]=_mm_load_si128(&o.mydata.asM128s[1]); return *this; }
	explicit Hash256(const char *bytes) { int_testAlignment(); mydata.asM128s[0]=_mm_loadu_si128((const __m128i *) bytes); mydata.asM128s[1]=_mm_loadu_si128((const __m128i *)(bytes+16)); }
	bool operator==(const Hash256 &o) const
	{
		__m128i result[2];
		result[0]=_mm_cmpeq_epi32(mydata.asM128s[0], o.mydata.asM128s[0]);
		result[1]=_mm_cmpeq_epi32(mydata.asM128s[1], o.mydata.asM128s[1]);
		unsigned r=_mm_movemask_epi8(result[0]);
		r|=_mm_movemask_epi8(result[1])<<16;
		return !(~r);
	}
	bool operator>(const Hash256 &o) const
	{
		__m128i result[4];
		result[0]=_mm_cmpgt_epi32(mydata.asM128s[0], o.mydata.asM128s[0]);
		result[1]=_mm_cmpgt_epi32(mydata.asM128s[1], o.mydata.asM128s[1]);
		result[2]=_mm_cmpgt_epi32(o.mydata.asM128s[0], mydata.asM128s[0]);
		result[3]=_mm_cmpgt_epi32(o.mydata.asM128s[1], mydata.asM128s[1]);
		unsigned r[2];
		r[0]=_mm_movemask_epi8(result[0])|(_mm_movemask_epi8(result[1])<<16);
		r[1]=_mm_movemask_epi8(result[2])|(_mm_movemask_epi8(result[3])<<16);
		return r[0]>r[1];
	}
#else
	//! Copies another hash
	Hash256(const Hash256 &o) { int_testAlignment(); memcpy(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes)); }
	//! Copies another hash
	Hash256 &operator=(const Hash256 &o) { memcpy(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes)); return *this; }
	//! Constructs a hash from unaligned data
	explicit Hash256(const char *bytes) { int_testAlignment(); memcpy(mydata.asBytes, bytes, sizeof(mydata.asBytes)); }
	bool operator==(const Hash256 &o) const { return !memcmp(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes)); }
	bool operator>(const Hash256 &o) const { return memcmp(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes))>0; }
#endif
	Hash256(Hash256 &&o) { *this=o; }
	Hash256 &operator=(Hash256 &&o) { *this=o; return *this; }
	bool operator!=(const Hash256 &o) const { return !(*this==o); }
	bool operator<(const Hash256 &o) const { return o>*this; }
	bool operator>=(const Hash256 &o) const { return !(o>*this); }
	bool operator<=(const Hash256 &o) const { return !(*this>o); }
	//! Returns the hash as bytes
	const char *asBytes() const { return mydata.asBytes; }
	//! Returns the hash as long longs
	const unsigned long long *asLongLongs() const { return mydata.asLongLongs; }
	//! Returns the front of the hash as a size_t
	size_t asSize_t() const { return mydata.asSize_t; }
	//! Returns the hash as a 64 character hexadecimal string
	std::string asHexString() const
	{
		std::string ret(64, '0');
		for(int i=0; i<32; i++)
		{
			unsigned char c=mydata.asBytes[i];
			ret[i*2]=int_tohex(c>>4);
			ret[i*2+1]=int_tohex(c&15);
		}
		return ret;
	}
};

//! Adds data to hash. Uses two threads if length equals or exceeds 1024 bytes.
extern void AddToHash256(Hash256 &hash, const char *data, size_t length);

} //namespace

namespace std
{
	//! Defines a hash for a Hash256 (simply truncates)
	template<> struct hash<::NiallsNasty256Hash::Hash256>
	{
		size_t operator()(const ::NiallsNasty256Hash::Hash256 &v) const
		{
			return v.asSize_t();
		}
	};
	//! Stop the default std::vector<> doing unaligned storage
	template<> class vector<::NiallsNasty256Hash::Hash256, allocator<::NiallsNasty256Hash::Hash256>> : public vector<::NiallsNasty256Hash::Hash256, NiallsCPP11Utilities::aligned_allocator<::NiallsNasty256Hash::Hash256>>
	{
		typedef vector<::NiallsNasty256Hash::Hash256, NiallsCPP11Utilities::aligned_allocator<::NiallsNasty256Hash::Hash256>> Base;
	public:
		explicit vector (const allocator_type& alloc = allocator_type()) : Base(alloc) { }
		explicit vector (size_type n) : Base(n) { }
		vector (size_type n, const value_type& val,
			const allocator_type& alloc = allocator_type()) : Base(n, val, alloc) { }
		template <class InputIterator> vector (InputIterator first, InputIterator last,
			const allocator_type& alloc = allocator_type()) : Base(first, last, alloc) { }
		vector (const vector& x) : Base(x) { }
		vector (const vector& x, const allocator_type& alloc) : Base(x, alloc) { }
		vector (vector&& x) : Base(std::move(x)) { }
		vector (vector&& x, const allocator_type& alloc) : Base(std::move(x), alloc) { }
#if defined(_MSC_VER) && _MSC_VER>1700
		vector (initializer_list<value_type> il, const allocator_type& alloc = allocator_type()) : Base(il, alloc) { }
#endif
	};
}

#endif
