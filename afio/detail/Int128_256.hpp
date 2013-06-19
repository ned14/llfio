/* Int128_256.hpp
(C) 2013 Niall Douglas http://www.nedprod.com
Created: Feb 2013

AVX2 accelerated 256 bit integer implementation. Falls back to SSE2/NEON 128 bit integers, or even char[32] if needed.
*/

#ifndef NIALLSINT256_H
#define NIALLSINT256_H

/*! \file Int128_256.hpp
\brief Provides the Int128 and Int256 hardware accelerated types
*/

#include "NiallsCPP11Utilities.hpp"
#include <cstring>
#include <exception>
#include <string>

/*! \def HAVE_M256
\brief Turns on support for the __m256i hardware accelerated type
*/
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

/*! \def HAVE_M128
\brief Turns on support for the __m128i hardware accelerated type
*/
#ifndef HAVE_M128
#ifdef _MSC_VER
#if _M_IX86_FP>=2 || defined(_M_AMD64) || _M_ARM_FP>=40
#define HAVE_M128 1
#include <emmintrin.h>
#endif
#elif defined(__GNUC__)
#if defined(__SSE2__)
#define HAVE_M128 1
#include <emmintrin.h>
#elif defined(__ARM_NEON__)
#define HAVE_NEON128 1
#include <arm_neon.h>

// Taken from http://stackoverflow.com/questions/11870910/sse-mm-movemask-epi8-equivalent-method-for-arm-neon
inline unsigned _mm_movemask_epi8_neon(uint32x4_t Input)
{
  static const uint8_t __attribute__ ((aligned (16))) _Powers[16]= 
      { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };

  // Set the powers of 2 (do it once for all, if applicable)
  static uint8x16_t Powers= vld1q_u8(_Powers);

  // Compute the mask from the input
  uint64x2_t Mask= vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(vandq_u8(vreinterpretq_u8_u32(Input), Powers))));

  // Get the resulting bytes
  uint16_t Output;
  vst1q_lane_u8((uint8_t*)&Output + 0, (uint8x16_t)Mask, 0);
  vst1q_lane_u8((uint8_t*)&Output + 1, (uint8x16_t)Mask, 8);

  return (unsigned) Output;
}
#endif
#endif
#endif

#ifndef HAVE_M128
#define HAVE_M128 0
#endif
#ifndef HAVE_NEON128
#define HAVE_NEON128 0
#endif
#ifndef HAVE_M256
#define HAVE_M256 0
#endif

namespace NiallsCPP11Utilities {

/*! \class Int128
\brief Declares a 128 bit SSE2/NEON compliant container. WILL throw exception if initialised unaligned.

Implemented as a __m128i or NEON uint32x4_t if available, otherwise as long longs.
*/
class NIALLSCPP11UTILITIES_API TYPEALIGNMENT(16) Int128
{
	union
	{
		char asBytes[16];
		unsigned int asInts[4];
		unsigned long long asLongLongs[2];
		size_t asSize_t;
#if HAVE_M128
		__m128i asM128;
#endif
#if HAVE_NEON128
		uint32x4_t asNEON;
#endif
#ifdef __GNUC__
		int __attribute__ ((vector_size(16))) vect16;
#endif
	} mydata;
	void int_testAlignment() const
	{
#ifndef NDEBUG
		if(((size_t)this) & 15) throw std::runtime_error("This object must be aligned to 16 bytes in memory!");
#endif
	}
	static char int_tohex(int x)
	{
		return (char)((x>9) ? 'a'+x-10 : '0'+x);
	}
public:
	//! Constructs an empty int
	Int128() { int_testAlignment(); memset(this, 0, sizeof(*this)); }
#if HAVE_M128
	Int128(const Int128 &o) { int_testAlignment(); mydata.asM128=_mm_load_si128(&o.mydata.asM128); }
	Int128 &operator=(const Int128 &o) { mydata.asM128=_mm_load_si128(&o.mydata.asM128); return *this; }
	explicit Int128(const char *bytes) { int_testAlignment(); mydata.asM128=_mm_loadu_si128((const __m128i *) bytes); }
	bool operator==(const Int128 &o) const
	{
		__m128i result;
		result=_mm_cmpeq_epi32(mydata.asM128, o.mydata.asM128);
		unsigned r=_mm_movemask_epi8(result);
		return r==0xffff;
	}
#elif HAVE_NEON128
	Int128(const Int128 &o) { int_testAlignment(); mydata.asNEON=o.mydata.asNEON; }
	Int128 &operator=(const Int128 &o) { mydata.asNEON=o.mydata.asNEON; return *this; }
	explicit Int128(const char *bytes) { int_testAlignment(); mydata.asNEON=vld1q_u32((const uint32_t *) bytes); }
	bool operator==(const Int128 &o) const
	{
		uint32x4_t result;
		result=vceqq_u32(mydata.asNEON, o.mydata.asNEON);
		unsigned r=_mm_movemask_epi8_neon(result);
		return r==0xffff;
	}
#else
	//! Copies another int
	Int128(const Int128 &o) { int_testAlignment(); memcpy(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes)); }
	//! Copies another int
	Int128 &operator=(const Int128 &o) { memcpy(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes)); return *this; }
	//! Constructs a int from unaligned data
	explicit Int128(const char *bytes) { int_testAlignment(); memcpy(mydata.asBytes, bytes, sizeof(mydata.asBytes)); }
	bool operator==(const Int128 &o) const { return !memcmp(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes)); }
#endif
	Int128(Int128 &&o) { *this=o; }
	Int128 &operator=(Int128 &&o) { *this=o; return *this; }
	bool operator!=(const Int128 &o) const { return !(*this==o); }
	bool operator>(const Int128 &o) const { return memcmp(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes))>0; }
	bool operator<(const Int128 &o) const { return o>*this; }
	bool operator>=(const Int128 &o) const { return !(o>*this); }
	bool operator<=(const Int128 &o) const { return !(*this>o); }
	//! Returns the int as bytes
	const char *asBytes() const { return mydata.asBytes; }
	//! Returns the int as ints
	const unsigned int *asInts() const { return mydata.asInts; }
	//! Returns the int as long longs
	const unsigned long long *asLongLongs() const { return mydata.asLongLongs; }
	//! Returns the front of the int as a size_t
	size_t asSize_t() const { return mydata.asSize_t; }
	//! Returns the int as a 64 character hexadecimal string
	std::string asHexString() const
	{
		std::string ret(32, '0');
		for(int i=0; i<16; i++)
		{
			unsigned char c=mydata.asBytes[i];
			ret[i*2]=int_tohex(c>>4);
			ret[i*2+1]=int_tohex(c&15);
		}
		return ret;
	}
	/*! \brief Fast gets \em no random Int128s.

	Intel Ivy Bridge: Performance on 32 bit is approx. 2.95 cycles/byte. Performance on 64 bit is approx. 1.52 cycles/byte.

	Intel Atom: Performance on 32 bit is approx. 9.38 cycles/byte. 
	
	ARM Cortex-A15: Performance on 32 bit is approx. 26.17 cycles/byte.
	*/
	static void FillFastRandom(Int128 *ints, size_t no);
	//! Fast fills a vector with random Int128s
	static inline void FillFastRandom(std::vector<Int128> &ints);
	/*! \brief Quality gets \em no random Int128s.

	Intel Ivy Bridge: Performance on 32 bit is approx. 2.95 cycles/byte. Performance on 64 bit is approx. 1.52 cycles/byte.

	Intel Atom: Performance on 32 bit is approx. 9.38 cycles/byte. 
	
	ARM Cortex-A15: Performance on 32 bit is approx. 26.84 cycles/byte.
	*/
	static void FillQualityRandom(Int128 *ints, size_t no);
	//! Quality fills a vector with random Int128s
	static inline void FillQualityRandom(std::vector<Int128> &ints);
};

/*! \class Int256
\brief Declares a 256 bit AVX2/SSE2/NEON compliant container. WILL throw exception if initialised unaligned.

Implemented as a __m256i if available (AVX2), otherwise two __m128i's or two NEON uint32x4_t's if available, otherwise as many long longs.
*/
class NIALLSCPP11UTILITIES_API TYPEALIGNMENT(32) Int256
{
	union
	{
		char asBytes[32];
		unsigned int asInts[8];
		unsigned long long asLongLongs[4];
		size_t asSize_t;
#if HAVE_M128
		__m128i asM128s[2];
#endif
#if HAVE_NEON128
		uint32x4_t asNEONs[2];
#endif
#if HAVE_M256
		__m256i asM256;
#endif
#ifdef __GNUC__
		int __attribute__ ((vector_size(32))) vect32;
#endif
	} mydata;
	void int_testAlignment() const
	{
#ifndef NDEBUG
		if(((size_t)this) & 31) throw std::runtime_error("This object must be aligned to 32 bytes in memory!");
#endif
	}
	static char int_tohex(int x)
	{
		return (char)((x>9) ? 'a'+x-10 : '0'+x);
	}
public:
	//! Constructs an empty int
	Int256() { int_testAlignment(); memset(this, 0, sizeof(*this)); }
#if HAVE_M256
	Int256(const Int256 &o) { int_testAlignment(); mydata.asM256=_mm256_load_si256(&o.mydata.asM256); }
	Int256 &operator=(const Int256 &o) { mydata.asM256=_mm256_load_si256(&o.mydata.asM256); return *this; }
	explicit Int256(const char *bytes) { int_testAlignment(); mydata.asM256=_mm256_loadu_si256((const __m256i *) bytes); }
	bool operator==(const Int256 &o) const
	{
		__m256i result=_mm256_cmpeq_epi64(mydata.asM256, o.mydata.asM256);
		return !(~_mm256_movemask_epi8(result));
	}
#elif HAVE_M128
	Int256(const Int256 &o) { int_testAlignment(); mydata.asM128s[0]=_mm_load_si128(&o.mydata.asM128s[0]); mydata.asM128s[1]=_mm_load_si128(&o.mydata.asM128s[1]); }
	Int256 &operator=(const Int256 &o) { mydata.asM128s[0]=_mm_load_si128(&o.mydata.asM128s[0]); mydata.asM128s[1]=_mm_load_si128(&o.mydata.asM128s[1]); return *this; }
	explicit Int256(const char *bytes) { int_testAlignment(); mydata.asM128s[0]=_mm_loadu_si128((const __m128i *) bytes); mydata.asM128s[1]=_mm_loadu_si128((const __m128i *)(bytes+16)); }
	bool operator==(const Int256 &o) const
	{
		__m128i result[2];
		result[0]=_mm_cmpeq_epi32(mydata.asM128s[0], o.mydata.asM128s[0]);
		result[1]=_mm_cmpeq_epi32(mydata.asM128s[1], o.mydata.asM128s[1]);
		unsigned r=_mm_movemask_epi8(result[0]);
		r|=_mm_movemask_epi8(result[1])<<16;
		return !(~r);
	}
#elif HAVE_NEON128
	Int256(const Int256 &o) { int_testAlignment(); mydata.asNEONs[0]=o.mydata.asNEONs[0]; mydata.asNEONs[1]=o.mydata.asNEONs[1]; }
	Int256 &operator=(const Int256 &o) { mydata.asNEONs[0]=o.mydata.asNEONs[0]; mydata.asNEONs[1]=o.mydata.asNEONs[1]; return *this; }
	explicit Int256(const char *bytes) { int_testAlignment(); mydata.asNEONs[0]=vld1q_u32((const uint32_t *) bytes); mydata.asNEONs[1]=vld1q_u32((const uint32_t *) (bytes+16)); }
	bool operator==(const Int256 &o) const
	{
		uint32x4_t result[2];
		result[0]=vceqq_u32(mydata.asNEONs[0], o.mydata.asNEONs[0]);
		result[1]=vceqq_u32(mydata.asNEONs[1], o.mydata.asNEONs[1]);
		unsigned r=_mm_movemask_epi8_neon(result[0]);
		r|=_mm_movemask_epi8_neon(result[1])<<16;
		return !(~r);
	}
#else
	//! Copies another int
	Int256(const Int256 &o) { int_testAlignment(); memcpy(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes)); }
	//! Copies another int
	Int256 &operator=(const Int256 &o) { memcpy(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes)); return *this; }
	//! Constructs a int from unaligned data
	explicit Int256(const char *bytes) { int_testAlignment(); memcpy(mydata.asBytes, bytes, sizeof(mydata.asBytes)); }
	bool operator==(const Int256 &o) const { return !memcmp(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes)); }
#endif
	Int256(Int256 &&o) { *this=o; }
	Int256 &operator=(Int256 &&o) { *this=o; return *this; }
	bool operator!=(const Int256 &o) const { return !(*this==o); }
	bool operator>(const Int256 &o) const { return memcmp(mydata.asBytes, o.mydata.asBytes, sizeof(mydata.asBytes))>0; }
	bool operator<(const Int256 &o) const { return o>*this; }
	bool operator>=(const Int256 &o) const { return !(o>*this); }
	bool operator<=(const Int256 &o) const { return !(*this>o); }
	//! Returns the int as bytes
	const char *asBytes() const { return mydata.asBytes; }
	//! Returns the int as ints
	const unsigned int *asInts() const { return mydata.asInts; }
	//! Returns the int as long longs
	const unsigned long long *asLongLongs() const { return mydata.asLongLongs; }
	//! Returns the front of the int as a size_t
	size_t asSize_t() const { return mydata.asSize_t; }
	//! Returns the int as a 64 character hexadecimal string
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
	/*! \brief Fast gets \em no random Int256s.

	Intel Ivy Bridge: Performance on 32 bit is approx. 2.95 cycles/byte. Performance on 64 bit is approx. 1.52 cycles/byte.

	Intel Atom: Performance on 32 bit is approx. 9.38 cycles/byte. 
	
	ARM Cortex-A15: Performance on 32 bit is approx. 16.63 cycles/byte.
	*/
	static void FillFastRandom(Int256 *ints, size_t no);
	//! Fast fills a vector with random Int256s.
	static inline void FillFastRandom(std::vector<Int256> &ints);
	/*! \brief Quality gets \em no random Int256s.

	Intel Ivy Bridge: Performance on 32 bit is approx. 2.95 cycles/byte. Performance on 64 bit is approx. 1.52 cycles/byte.

	Intel Atom: Performance on 32 bit is approx. 9.38 cycles/byte. 
	
	ARM Cortex-A15: Performance on 32 bit is approx. 16.47 cycles/byte.
	*/
	static void FillQualityRandom(Int256 *ints, size_t no);
	//! Quality fills a vector with random Int256s.
	static inline void FillQualityRandom(std::vector<Int256> &ints);
};

/*! \class Hash128
\brief Provides a 128 bit hash.

To use this you must compile Int128_256.cpp.

Intel Ivy Bridge: Fasthash (SpookyHash) performance on 32 bit is approx. 1.17 cycles/byte. Performance on 64 bit is approx. 0.31 cycles/byte.

Intel Atom: Performance on 32 bit is approx. 3.38 cycles/byte

ARM Cortex-A15: Performance on 32 bit is approx. 1.49 cycles/byte.
*/
class NIALLSCPP11UTILITIES_API Hash128 : public Int128
{
	static Int128 int_init()
	{
		// First 32 bits of the fractional parts of the square roots of the first 8 primes 2..19
		static TYPEALIGNMENT(16) const unsigned int_iv[]={0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
			0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
		return *((Int128 *) int_iv);
	}
public:
	//! Constructs an instance
	Hash128() : Int128(int_init()) { }
	explicit Hash128(const char *bytes) : Int128(bytes) { }
	//! Adds fast hashed data to this hash.
	void AddFastHashTo(const char *data, size_t length);
	//! Batch adds fast hashed data to hashes.
	static void BatchAddFastHashTo(size_t no, Hash128 *hashs, const char **data, size_t *length);
};

/*! \class Hash256
\brief Provides a 256 bit hash.

To use this you must compile Int128_256.cpp.

Intel Ivy Bridge: Fasthash (combined SpookyHash + CityHash) performance on 32 bit is approx. 2.71 cycles/byte. Performance on 64 bit is approx. 0.46 cycles/byte.

Intel Atom (single hyperthreaded core): Fasthash (combined SpookyHash + CityHash) performance on 32 bit is approx. 9.31 cycles/byte.

ARM Cortex-A15: Fasthash (combined SpookyHash + CityHash) performance on 32 bit is approx. 2.96 cycles/byte.


Intel Ivy Bridge: SHA-256 performance on 32 bit is approx. 17.23 cycles/byte (batch 6.89 cycles/byte). Performance on 64 bit is approx. 14.89 cycles/byte (batch 4.23 cycles/byte).

Intel Atom (single hyperthreaded core): SHA-256 performance on 32 bit is approx. 40.35 cycles/byte (batch 24.46 cycles/byte).

ARM Cortex-A15: SHA-256 performance on 32 bit is approx. 21.24 cycles/byte (batch 15.11 cycles/byte).


SHA-256, being cryptographically secure, requires a setup, data contribution and finalisation stage in order to produce FIPS compliant
output (mainly because the total bits hashed must be appended at the end). Only AddSHA256ToBatch() can therefore correctly handle
incremental hashing if you want the "correct" hash to be output. Internally, this implies having to construct scratch space and having
to cope with non-block multiple incremental data. For speed, and for ease of programming the SSE2 SIMD units, the batch implementation
currently throws an exception if you supply non-64 byte multiples to AddSHA256ToBatch() except as the final increment before FinishBatch().
*/
class NIALLSCPP11UTILITIES_API Hash256 : public Int256
{
	static Int256 int_init()
	{
		// First 32 bits of the fractional parts of the square roots of the first 8 primes 2..19
		static TYPEALIGNMENT(32) const unsigned int_iv[]={0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
			0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
		return *((Int256 *) int_iv);
	}
public:
	//! Constructs an instance
	Hash256() : Int256(int_init()) { }
	explicit Hash256(const char *bytes) : Int256(bytes) { }
	//! Adds fast hashed data to this hash. Uses two threads if given >=1024 bytes and OpenMP support.
	void AddFastHashTo(const char *data, size_t length);
	//! Adds SHA-256 data to this hash as a single operation.
	void AddSHA256To(const char *data, size_t length);

	//! A handle to an ongoing batch hash operation
	typedef void *BatchHashOp;
	//! Specifies which batch item this data is for. Format is hash idx, data, length of data.
	typedef std::tuple<size_t, const char *, size_t> BatchItem;
	//! Begins an incremental batch hash. Tip: use FinishBatch(h, false) to avoid recreating this.
	static BatchHashOp BeginBatch(size_t no, Hash256 *hashs);
	//! Adds data to an incremental fast hash operation. Don't mix this with AddSHA256ToBatch() on the same BatchHashOp.
	static void AddFastHashToBatch(BatchHashOp h, size_t items, const BatchItem *datas);
	//! Adds data to an incremental SHA-256 operation. Don't mix this with AddSHA256ToBatch() on the same BatchHashOp.
	static void AddSHA256ToBatch(BatchHashOp h, size_t items, const BatchItem *datas);
	//! Finishes an incremental batch hash
	static void FinishBatch(BatchHashOp h, bool free=true);

	//! Batch adds fast hashed data to hashes as a single operation.
	static void BatchAddFastHashTo(size_t no, Hash256 *hashs, const char **data, size_t *length);
	//! Batch adds SHA-256 data to hashes as a single operation.
	static void BatchAddSHA256To(size_t no, Hash256 *hashs, const char **data, size_t *length);
};

} //namespace

namespace std
{
	//! Defines a hash for a Int128 (simply truncates)
	template<> class hash<NiallsCPP11Utilities::Int128>
	{
	public:
		size_t operator()(const NiallsCPP11Utilities::Int128 &v) const
		{
			return v.asSize_t();
		}
	};
	//! Defines a hash for a Int256 (simply truncates)
	template<> class hash<NiallsCPP11Utilities::Int256>
	{
	public:
		size_t operator()(const NiallsCPP11Utilities::Int256 &v) const
		{
			return v.asSize_t();
		}
	};
#define TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE NiallsCPP11Utilities::Int128
#include "incl_stl_allocator_override.hpp"
#undef TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE
#define TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE NiallsCPP11Utilities::Hash128
#include "incl_stl_allocator_override.hpp"
#undef TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE
#define TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE NiallsCPP11Utilities::Int256
#include "incl_stl_allocator_override.hpp"
#undef TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE
#define TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE NiallsCPP11Utilities::Hash256
#include "incl_stl_allocator_override.hpp"
#undef TYPE_TO_BE_OVERRIDEN_FOR_STL_ALLOCATOR_USAGE
}

namespace NiallsCPP11Utilities {
	inline void Int128::FillFastRandom(std::vector<Int128> &ints) { FillFastRandom(ints.data(), ints.size()); }
	inline void Int128::FillQualityRandom(std::vector<Int128> &ints) { FillQualityRandom(ints.data(), ints.size()); }
	inline void Int256::FillFastRandom(std::vector<Int256> &ints) { FillFastRandom(ints.data(), ints.size()); }
	inline void Int256::FillQualityRandom(std::vector<Int256> &ints) { FillQualityRandom(ints.data(), ints.size()); }
}

#endif
