
#include "sha256.h"

#include <stdint.h>
//#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#define	SHA256_CONST_(x)		(SHA256_CONST_ ## x)

/* constants, as provided in FIPS 180-2 */

#define	SHA256_CONST_0		0x428a2f98U
#define	SHA256_CONST_1		0x71374491U
#define	SHA256_CONST_2		0xb5c0fbcfU
#define	SHA256_CONST_3		0xe9b5dba5U
#define	SHA256_CONST_4		0x3956c25bU
#define	SHA256_CONST_5		0x59f111f1U
#define	SHA256_CONST_6		0x923f82a4U
#define	SHA256_CONST_7		0xab1c5ed5U

#define	SHA256_CONST_8		0xd807aa98U
#define	SHA256_CONST_9		0x12835b01U
#define	SHA256_CONST_10		0x243185beU
#define	SHA256_CONST_11		0x550c7dc3U
#define	SHA256_CONST_12		0x72be5d74U
#define	SHA256_CONST_13		0x80deb1feU
#define	SHA256_CONST_14		0x9bdc06a7U
#define	SHA256_CONST_15		0xc19bf174U

#define	SHA256_CONST_16		0xe49b69c1U
#define	SHA256_CONST_17		0xefbe4786U
#define	SHA256_CONST_18		0x0fc19dc6U
#define	SHA256_CONST_19		0x240ca1ccU
#define	SHA256_CONST_20		0x2de92c6fU
#define	SHA256_CONST_21		0x4a7484aaU
#define	SHA256_CONST_22		0x5cb0a9dcU
#define	SHA256_CONST_23		0x76f988daU

#define	SHA256_CONST_24		0x983e5152U
#define	SHA256_CONST_25		0xa831c66dU
#define	SHA256_CONST_26		0xb00327c8U
#define	SHA256_CONST_27		0xbf597fc7U
#define	SHA256_CONST_28		0xc6e00bf3U
#define	SHA256_CONST_29		0xd5a79147U
#define	SHA256_CONST_30		0x06ca6351U
#define	SHA256_CONST_31		0x14292967U

#define	SHA256_CONST_32		0x27b70a85U
#define	SHA256_CONST_33		0x2e1b2138U
#define	SHA256_CONST_34		0x4d2c6dfcU
#define	SHA256_CONST_35		0x53380d13U
#define	SHA256_CONST_36		0x650a7354U
#define	SHA256_CONST_37		0x766a0abbU
#define	SHA256_CONST_38		0x81c2c92eU
#define	SHA256_CONST_39		0x92722c85U

#define	SHA256_CONST_40		0xa2bfe8a1U
#define	SHA256_CONST_41		0xa81a664bU
#define	SHA256_CONST_42		0xc24b8b70U
#define	SHA256_CONST_43		0xc76c51a3U
#define	SHA256_CONST_44		0xd192e819U
#define	SHA256_CONST_45		0xd6990624U
#define	SHA256_CONST_46		0xf40e3585U
#define	SHA256_CONST_47		0x106aa070U

#define	SHA256_CONST_48		0x19a4c116U
#define	SHA256_CONST_49		0x1e376c08U
#define	SHA256_CONST_50		0x2748774cU
#define	SHA256_CONST_51		0x34b0bcb5U
#define	SHA256_CONST_52		0x391c0cb3U
#define	SHA256_CONST_53		0x4ed8aa4aU
#define	SHA256_CONST_54		0x5b9cca4fU
#define	SHA256_CONST_55		0x682e6ff3U

#define	SHA256_CONST_56		0x748f82eeU
#define	SHA256_CONST_57		0x78a5636fU
#define	SHA256_CONST_58		0x84c87814U
#define	SHA256_CONST_59		0x8cc70208U
#define	SHA256_CONST_60		0x90befffaU
#define	SHA256_CONST_61		0xa4506cebU
#define	SHA256_CONST_62		0xbef9a3f7U
#define	SHA256_CONST_63		0xc67178f2U

/* Ch and Maj are the basic SHA2 functions. */
#define	Ch_(b, c, d)	(((b) & (c)) ^ ((~b) & (d)))
#define	Maj_(b, c, d)	(((b) & (c)) ^ ((b) & (d)) ^ ((c) & (d)))

/* Rotates x right n bits. */
#define	ROTR_(x, n)	\
(((x) >> (n)) | ((x) << ((sizeof (x) * 8)-(n))))

/* Shift x right n bits */
#define	SHR_(x, n)	((x) >> (n))

/* SHA256 Functions */
#define	BIGSIGMA0_256_(x)	(ROTR_((x), 2) ^ ROTR_((x), 13) ^ ROTR_((x), 22))
#define	BIGSIGMA1_256_(x)	(ROTR_((x), 6) ^ ROTR_((x), 11) ^ ROTR_((x), 25))
#define	SIGMA0_256_(x)		(ROTR_((x), 7) ^ ROTR_((x), 18) ^ SHR_((x), 3))
#define	SIGMA1_256_(x)		(ROTR_((x), 17) ^ ROTR_((x), 19) ^ SHR_((x), 10))

#define	SHA256ROUND_(a, b, c, d, e, f, g, h, i, w)			\
T1 = h + BIGSIGMA1_256_(e) + Ch_(e, f, g) + SHA256_CONST_(i) + w;	\
d += T1;							\
T2 = BIGSIGMA0_256_(a) + Maj_(a, b, c);				\
h = T1 + T2


/*
 * sparc optimization:
 *
 * on the sparc, we can load big endian 32-bit data easily.  note that
 * special care must be taken to ensure the address is 32-bit aligned.
 * in the interest of speed, we don't check to make sure, since
 * careful programming can guarantee this for us.
 */

#if	defined(_BIG_ENDIAN)
#define	LOAD_BIG_32(addr)	(*(uint32_t *)(addr))
#define	LOAD_BIG_64(addr)	(*(uint64_t *)(addr))

#elif defined(_MSC_VER)

#include <stdlib.h>
#define	LOAD_BIG_32(addr) _byteswap_ulong(*((uint32_t *)(addr)))
#define	LOAD_BIG_64(addr) _byteswap_uint64(*((uint64_t *)(addr)))

#elif defined(__APPLE__)

// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
#define	LOAD_BIG_32(addr) OSSwapInt32(*((uint32_t *)(addr)))
#define	LOAD_BIG_64(addr) OSSwapInt64(*((uint64_t *)(addr)))

#elif defined(__GNUC__)

//#include <byteswap.h>
#define	LOAD_BIG_32(addr) __builtin_bswap32(*((uint32_t *)(addr)))
#define	LOAD_BIG_64(addr) __builtin_bswap64(*((uint64_t *)(addr)))

#elif	defined(HAVE_HTONL)
#define	LOAD_BIG_32(addr) htonl(*((uint32_t *)(addr)))
#define	LOAD_BIG_64(addr) htonll(*((uint64_t *)(addr)))

#else
/* little endian -- will work on big endian, but slowly */
#define	LOAD_BIG_32(addr)	\
(((addr)[0] << 24) | ((addr)[1] << 16) | ((addr)[2] << 8) | (addr)[3])
#define	LOAD_BIG_64(addr)	\
(((uint64_t)(addr)[0] << 56) | ((uint64_t)(addr)[1] << 48) |	\
((uint64_t)(addr)[2] << 40) | ((uint64_t)(addr)[3] << 32) |	\
((uint64_t)(addr)[4] << 24) | ((uint64_t)(addr)[5] << 16) |	\
((uint64_t)(addr)[6] << 8) | (uint64_t)(addr)[7])
#endif	/* _BIG_ENDIAN */

#if 0
#define dumpstate() printf("%s: %08x %08x %08x %08x %08x %08x %08x %08x %08x\n", __func__, w0, a, b, c, d, e, f, g, h);
#else
#define dumpstate()
#endif

void
__sha256_osol(const __sha256_block_t blk, __sha256_hash_t ctx)
{    
	uint32_t a = ctx[0];
	uint32_t b = ctx[1];
	uint32_t c = ctx[2];
	uint32_t d = ctx[3];
	uint32_t e = ctx[4];
	uint32_t f = ctx[5];
	uint32_t g = ctx[6];
	uint32_t h = ctx[7];
    
	uint32_t w0, w1, w2, w3, w4, w5, w6, w7;
	uint32_t w8, w9, w10, w11, w12, w13, w14, w15;
	uint32_t T1, T2;
    
#if	defined(__sparc)
	static const uint32_t sha256_consts[] = {
		SHA256_CONST_0, SHA256_CONST_1, SHA256_CONST_2,
		SHA256_CONST_3, SHA256_CONST_4, SHA256_CONST_5,
		SHA256_CONST_6, SHA256_CONST_7, SHA256_CONST_8,
		SHA256_CONST_9, SHA256_CONST_10, SHA256_CONST_11,
		SHA256_CONST_12, SHA256_CONST_13, SHA256_CONST_14,
		SHA256_CONST_15, SHA256_CONST_16, SHA256_CONST_17,
		SHA256_CONST_18, SHA256_CONST_19, SHA256_CONST_20,
		SHA256_CONST_21, SHA256_CONST_22, SHA256_CONST_23,
		SHA256_CONST_24, SHA256_CONST_25, SHA256_CONST_26,
		SHA256_CONST_27, SHA256_CONST_28, SHA256_CONST_29,
		SHA256_CONST_30, SHA256_CONST_31, SHA256_CONST_32,
		SHA256_CONST_33, SHA256_CONST_34, SHA256_CONST_35,
		SHA256_CONST_36, SHA256_CONST_37, SHA256_CONST_38,
		SHA256_CONST_39, SHA256_CONST_40, SHA256_CONST_41,
		SHA256_CONST_42, SHA256_CONST_43, SHA256_CONST_44,
		SHA256_CONST_45, SHA256_CONST_46, SHA256_CONST_47,
		SHA256_CONST_48, SHA256_CONST_49, SHA256_CONST_50,
		SHA256_CONST_51, SHA256_CONST_52, SHA256_CONST_53,
		SHA256_CONST_54, SHA256_CONST_55, SHA256_CONST_56,
		SHA256_CONST_57, SHA256_CONST_58, SHA256_CONST_59,
		SHA256_CONST_60, SHA256_CONST_61, SHA256_CONST_62,
		SHA256_CONST_63
	};
#endif	/* __sparc */
    
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w0 =  LOAD_BIG_32(blk + 4 * 0);
    dumpstate();
	SHA256ROUND_(a, b, c, d, e, f, g, h, 0, w0);
    dumpstate();

	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w1 =  LOAD_BIG_32(blk + 4 * 1);
	SHA256ROUND_(h, a, b, c, d, e, f, g, 1, w1);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w2 =  LOAD_BIG_32(blk + 4 * 2);
	SHA256ROUND_(g, h, a, b, c, d, e, f, 2, w2);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w3 =  LOAD_BIG_32(blk + 4 * 3);
	SHA256ROUND_(f, g, h, a, b, c, d, e, 3, w3);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w4 =  LOAD_BIG_32(blk + 4 * 4);
	SHA256ROUND_(e, f, g, h, a, b, c, d, 4, w4);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w5 =  LOAD_BIG_32(blk + 4 * 5);
	SHA256ROUND_(d, e, f, g, h, a, b, c, 5, w5);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w6 =  LOAD_BIG_32(blk + 4 * 6);
	SHA256ROUND_(c, d, e, f, g, h, a, b, 6, w6);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w7 =  LOAD_BIG_32(blk + 4 * 7);
	SHA256ROUND_(b, c, d, e, f, g, h, a, 7, w7);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w8 =  LOAD_BIG_32(blk + 4 * 8);
	SHA256ROUND_(a, b, c, d, e, f, g, h, 8, w8);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w9 =  LOAD_BIG_32(blk + 4 * 9);
	SHA256ROUND_(h, a, b, c, d, e, f, g, 9, w9);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w10 =  LOAD_BIG_32(blk + 4 * 10);
	SHA256ROUND_(g, h, a, b, c, d, e, f, 10, w10);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w11 =  LOAD_BIG_32(blk + 4 * 11);
	SHA256ROUND_(f, g, h, a, b, c, d, e, 11, w11);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w12 =  LOAD_BIG_32(blk + 4 * 12);
	SHA256ROUND_(e, f, g, h, a, b, c, d, 12, w12);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w13 =  LOAD_BIG_32(blk + 4 * 13);
	SHA256ROUND_(d, e, f, g, h, a, b, c, 13, w13);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w14 =  LOAD_BIG_32(blk + 4 * 14);
	SHA256ROUND_(c, d, e, f, g, h, a, b, 14, w14);
	/* LINTED E_BAD_PTR_CAST_ALIGN */
	w15 =  LOAD_BIG_32(blk + 4 * 15);
	SHA256ROUND_(b, c, d, e, f, g, h, a, 15, w15);
    
	w0 = SIGMA1_256_(w14) + w9 + SIGMA0_256_(w1) + w0;
	SHA256ROUND_(a, b, c, d, e, f, g, h, 16, w0);
	w1 = SIGMA1_256_(w15) + w10 + SIGMA0_256_(w2) + w1;
	SHA256ROUND_(h, a, b, c, d, e, f, g, 17, w1);
	w2 = SIGMA1_256_(w0) + w11 + SIGMA0_256_(w3) + w2;
	SHA256ROUND_(g, h, a, b, c, d, e, f, 18, w2);
	w3 = SIGMA1_256_(w1) + w12 + SIGMA0_256_(w4) + w3;
	SHA256ROUND_(f, g, h, a, b, c, d, e, 19, w3);
	w4 = SIGMA1_256_(w2) + w13 + SIGMA0_256_(w5) + w4;
	SHA256ROUND_(e, f, g, h, a, b, c, d, 20, w4);
	w5 = SIGMA1_256_(w3) + w14 + SIGMA0_256_(w6) + w5;
	SHA256ROUND_(d, e, f, g, h, a, b, c, 21, w5);
	w6 = SIGMA1_256_(w4) + w15 + SIGMA0_256_(w7) + w6;
	SHA256ROUND_(c, d, e, f, g, h, a, b, 22, w6);
	w7 = SIGMA1_256_(w5) + w0 + SIGMA0_256_(w8) + w7;
	SHA256ROUND_(b, c, d, e, f, g, h, a, 23, w7);
	w8 = SIGMA1_256_(w6) + w1 + SIGMA0_256_(w9) + w8;
	SHA256ROUND_(a, b, c, d, e, f, g, h, 24, w8);
	w9 = SIGMA1_256_(w7) + w2 + SIGMA0_256_(w10) + w9;
	SHA256ROUND_(h, a, b, c, d, e, f, g, 25, w9);
	w10 = SIGMA1_256_(w8) + w3 + SIGMA0_256_(w11) + w10;
	SHA256ROUND_(g, h, a, b, c, d, e, f, 26, w10);
	w11 = SIGMA1_256_(w9) + w4 + SIGMA0_256_(w12) + w11;
	SHA256ROUND_(f, g, h, a, b, c, d, e, 27, w11);
	w12 = SIGMA1_256_(w10) + w5 + SIGMA0_256_(w13) + w12;
	SHA256ROUND_(e, f, g, h, a, b, c, d, 28, w12);
	w13 = SIGMA1_256_(w11) + w6 + SIGMA0_256_(w14) + w13;
	SHA256ROUND_(d, e, f, g, h, a, b, c, 29, w13);
	w14 = SIGMA1_256_(w12) + w7 + SIGMA0_256_(w15) + w14;
	SHA256ROUND_(c, d, e, f, g, h, a, b, 30, w14);
	w15 = SIGMA1_256_(w13) + w8 + SIGMA0_256_(w0) + w15;
	SHA256ROUND_(b, c, d, e, f, g, h, a, 31, w15);
    
	w0 = SIGMA1_256_(w14) + w9 + SIGMA0_256_(w1) + w0;
	SHA256ROUND_(a, b, c, d, e, f, g, h, 32, w0);
	w1 = SIGMA1_256_(w15) + w10 + SIGMA0_256_(w2) + w1;
	SHA256ROUND_(h, a, b, c, d, e, f, g, 33, w1);
	w2 = SIGMA1_256_(w0) + w11 + SIGMA0_256_(w3) + w2;
	SHA256ROUND_(g, h, a, b, c, d, e, f, 34, w2);
	w3 = SIGMA1_256_(w1) + w12 + SIGMA0_256_(w4) + w3;
	SHA256ROUND_(f, g, h, a, b, c, d, e, 35, w3);
	w4 = SIGMA1_256_(w2) + w13 + SIGMA0_256_(w5) + w4;
	SHA256ROUND_(e, f, g, h, a, b, c, d, 36, w4);
	w5 = SIGMA1_256_(w3) + w14 + SIGMA0_256_(w6) + w5;
	SHA256ROUND_(d, e, f, g, h, a, b, c, 37, w5);
	w6 = SIGMA1_256_(w4) + w15 + SIGMA0_256_(w7) + w6;
	SHA256ROUND_(c, d, e, f, g, h, a, b, 38, w6);
	w7 = SIGMA1_256_(w5) + w0 + SIGMA0_256_(w8) + w7;
	SHA256ROUND_(b, c, d, e, f, g, h, a, 39, w7);
	w8 = SIGMA1_256_(w6) + w1 + SIGMA0_256_(w9) + w8;
	SHA256ROUND_(a, b, c, d, e, f, g, h, 40, w8);
	w9 = SIGMA1_256_(w7) + w2 + SIGMA0_256_(w10) + w9;
	SHA256ROUND_(h, a, b, c, d, e, f, g, 41, w9);
	w10 = SIGMA1_256_(w8) + w3 + SIGMA0_256_(w11) + w10;
	SHA256ROUND_(g, h, a, b, c, d, e, f, 42, w10);
	w11 = SIGMA1_256_(w9) + w4 + SIGMA0_256_(w12) + w11;
	SHA256ROUND_(f, g, h, a, b, c, d, e, 43, w11);
	w12 = SIGMA1_256_(w10) + w5 + SIGMA0_256_(w13) + w12;
	SHA256ROUND_(e, f, g, h, a, b, c, d, 44, w12);
	w13 = SIGMA1_256_(w11) + w6 + SIGMA0_256_(w14) + w13;
	SHA256ROUND_(d, e, f, g, h, a, b, c, 45, w13);
	w14 = SIGMA1_256_(w12) + w7 + SIGMA0_256_(w15) + w14;
	SHA256ROUND_(c, d, e, f, g, h, a, b, 46, w14);
	w15 = SIGMA1_256_(w13) + w8 + SIGMA0_256_(w0) + w15;
	SHA256ROUND_(b, c, d, e, f, g, h, a, 47, w15);
    
	w0 = SIGMA1_256_(w14) + w9 + SIGMA0_256_(w1) + w0;
	SHA256ROUND_(a, b, c, d, e, f, g, h, 48, w0);
	w1 = SIGMA1_256_(w15) + w10 + SIGMA0_256_(w2) + w1;
	SHA256ROUND_(h, a, b, c, d, e, f, g, 49, w1);
	w2 = SIGMA1_256_(w0) + w11 + SIGMA0_256_(w3) + w2;
	SHA256ROUND_(g, h, a, b, c, d, e, f, 50, w2);
	w3 = SIGMA1_256_(w1) + w12 + SIGMA0_256_(w4) + w3;
	SHA256ROUND_(f, g, h, a, b, c, d, e, 51, w3);
	w4 = SIGMA1_256_(w2) + w13 + SIGMA0_256_(w5) + w4;
	SHA256ROUND_(e, f, g, h, a, b, c, d, 52, w4);
	w5 = SIGMA1_256_(w3) + w14 + SIGMA0_256_(w6) + w5;
	SHA256ROUND_(d, e, f, g, h, a, b, c, 53, w5);
	w6 = SIGMA1_256_(w4) + w15 + SIGMA0_256_(w7) + w6;
	SHA256ROUND_(c, d, e, f, g, h, a, b, 54, w6);
	w7 = SIGMA1_256_(w5) + w0 + SIGMA0_256_(w8) + w7;
	SHA256ROUND_(b, c, d, e, f, g, h, a, 55, w7);
	w8 = SIGMA1_256_(w6) + w1 + SIGMA0_256_(w9) + w8;
	SHA256ROUND_(a, b, c, d, e, f, g, h, 56, w8);
	w9 = SIGMA1_256_(w7) + w2 + SIGMA0_256_(w10) + w9;
	SHA256ROUND_(h, a, b, c, d, e, f, g, 57, w9);
	w10 = SIGMA1_256_(w8) + w3 + SIGMA0_256_(w11) + w10;
	SHA256ROUND_(g, h, a, b, c, d, e, f, 58, w10);
	w11 = SIGMA1_256_(w9) + w4 + SIGMA0_256_(w12) + w11;
	SHA256ROUND_(f, g, h, a, b, c, d, e, 59, w11);
	w12 = SIGMA1_256_(w10) + w5 + SIGMA0_256_(w13) + w12;
	SHA256ROUND_(e, f, g, h, a, b, c, d, 60, w12);
	w13 = SIGMA1_256_(w11) + w6 + SIGMA0_256_(w14) + w13;
	SHA256ROUND_(d, e, f, g, h, a, b, c, 61, w13);
	w14 = SIGMA1_256_(w12) + w7 + SIGMA0_256_(w15) + w14;
	SHA256ROUND_(c, d, e, f, g, h, a, b, 62, w14);
	w15 = SIGMA1_256_(w13) + w8 + SIGMA0_256_(w0) + w15;
	SHA256ROUND_(b, c, d, e, f, g, h, a, 63, w15);
    
    //printf("%s last d: %08x\n", __func__, d);
    
    //printf("%s a: %08x %08x\n", __func__, a, ctx[0]);
	ctx[0] += a;
    //printf("%s a: %08x\n", __func__, ctx[0]);
	ctx[1] += b;
	ctx[2] += c;
	ctx[3] += d;
	ctx[4] += e;
	ctx[5] += f;
	ctx[6] += g;
	ctx[7] += h;
    
}
