//
//  sha-256-amd64.c
//  sha

#include "sha256.h"

#include <emmintrin.h>
#include <stdint.h>

static const uint32_t sha256_consts[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, /*  0 */
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, /*  8 */
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, /* 16 */
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, /* 24 */
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, /* 32 */
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, /* 40 */
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, /* 48 */
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, /* 56 */
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};


static inline __m128i Ch(__m128i b, __m128i c, __m128i d) {
    return _mm_xor_si128(_mm_and_si128(b, c), _mm_andnot_si128(b, d));
}

static inline __m128i Maj(__m128i b, __m128i c, __m128i d) {
    return _mm_xor_si128(_mm_xor_si128(_mm_and_si128(b, c), _mm_and_si128(b, d)), _mm_and_si128(c, d));
}

static inline __m128i ROTR(__m128i x, int n) {
    return _mm_or_si128(_mm_srli_epi32(x, n), _mm_slli_epi32(x, 32 - n));
}

static inline __m128i SHR(__m128i x, int n) {
    return _mm_srli_epi32(x, n);
}

/* SHA256 Functions */
#define	BIGSIGMA0_256(x)	(_mm_xor_si128(_mm_xor_si128(ROTR((x), 2), ROTR((x), 13)), ROTR((x), 22)))
#define	BIGSIGMA1_256(x)	(_mm_xor_si128(_mm_xor_si128(ROTR((x), 6), ROTR((x), 11)), ROTR((x), 25)))
#define	SIGMA0_256(x)		(_mm_xor_si128(_mm_xor_si128(ROTR((x), 7), ROTR((x), 18)), SHR((x), 3)))
#define	SIGMA1_256(x)		(_mm_xor_si128(_mm_xor_si128(ROTR((x), 17), ROTR((x), 19)), SHR((x), 10)))

static inline __m128i load_epi32(uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3) {
	return _mm_set_epi32(x0, x1, x2, x3);
}

static inline uint32_t store32(__m128i x) {
    union { uint32_t ret[1]; __m128i x; } box;
    box.x = x;
    return box.ret[0];
}

static inline void store_epi32(__m128i x, uint32_t *x0, uint32_t *x1, uint32_t *x2, uint32_t *x3) {
    union { uint32_t ret[4]; __m128i x; } box; box.x=x;
    *x0 = box.ret[3]; *x1 = box.ret[2]; *x2 = box.ret[1]; *x3 = box.ret[0];
}

static inline __m128i SHA256_CONST(int i) {
    return _mm_set1_epi32(sha256_consts[i]);
}

#define add4(x0, x1, x2, x3) _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(x0, x1), x2), x3)
#define add5(x0, x1, x2, x3, x4) _mm_add_epi32(add4(x0, x1, x2, x3), x4)


#define	SHA256ROUND(a, b, c, d, e, f, g, h, i, w)                       \
    T1 = add5(h, BIGSIGMA1_256(e), Ch(e, f, g), SHA256_CONST(i), w);	\
    d = _mm_add_epi32(d, T1);                                           \
    T2 = _mm_add_epi32(BIGSIGMA0_256(a), Maj(a, b, c));                 \
    h = _mm_add_epi32(T1, T2);

static inline uint32_t SWAP32(const void *addr) {
#ifdef _MSC_VER
	return _byteswap_ulong(*((uint32_t *)(addr)));
#else
    return __builtin_bswap32(*((uint32_t *)(addr)));
#endif
}

static inline __m128i LOAD(const __sha256_block_t *blk[4], int i) {
    return load_epi32(SWAP32(*blk[0] + i * 4), SWAP32(*blk[1] + i * 4), SWAP32(*blk[2] + i * 4), SWAP32(*blk[3] + i * 4));
}

static inline void dumpreg(__m128i x, char *msg) {
    union { uint32_t ret[4]; __m128i x; } box; box.x = x;
    printf("%s %08x %08x %08x %08x\n", msg, box.ret[0], box.ret[1], box.ret[2], box.ret[3]);
}

#if 0
#define dumpstate() printf("%s: %08x %08x %08x %08x %08x %08x %08x %08x %08x\n", \
__func__, store32(w0), store32(a), store32(b), store32(c), store32(d), store32(e), store32(f), store32(g), store32(h));
#else
#define dumpstate()
#endif

void __sha256_int(const __sha256_block_t *blk[4], __sha256_hash_t *hash[4])
{
    __sha256_hash_t *h0 = hash[0], *h1 = hash[1], *h2 = hash[2], *h3 = hash[3];
#define load(x, i) __m128i x = load_epi32((*h0)[i], (*h1)[i], (*h2)[i], (*h3)[i])

    load(a, 0);
    load(b, 1);
    load(c, 2);
    load(d, 3);
    load(e, 4);
    load(f, 5);
    load(g, 6);
    load(h, 7);    
    
    __m128i w0, w1, w2, w3, w4, w5, w6, w7;
    __m128i w8, w9, w10, w11, w12, w13, w14, w15;
    __m128i T1, T2;
    
    w0 =  LOAD(blk, 0);
    SHA256ROUND(a, b, c, d, e, f, g, h, 0, w0);    
    w1 =  LOAD(blk, 1);
    SHA256ROUND(h, a, b, c, d, e, f, g, 1, w1);
    w2 =  LOAD(blk, 2);
    SHA256ROUND(g, h, a, b, c, d, e, f, 2, w2);
    w3 =  LOAD(blk, 3);
    SHA256ROUND(f, g, h, a, b, c, d, e, 3, w3);
    w4 =  LOAD(blk, 4);
    SHA256ROUND(e, f, g, h, a, b, c, d, 4, w4);
    w5 =  LOAD(blk, 5);
    SHA256ROUND(d, e, f, g, h, a, b, c, 5, w5);
    w6 =  LOAD(blk, 6);
    SHA256ROUND(c, d, e, f, g, h, a, b, 6, w6);
    w7 =  LOAD(blk, 7);
    SHA256ROUND(b, c, d, e, f, g, h, a, 7, w7);
    w8 =  LOAD(blk, 8);
    SHA256ROUND(a, b, c, d, e, f, g, h, 8, w8);
    w9 =  LOAD(blk, 9);
    SHA256ROUND(h, a, b, c, d, e, f, g, 9, w9);
    w10 =  LOAD(blk, 10);
    SHA256ROUND(g, h, a, b, c, d, e, f, 10, w10);
    w11 =  LOAD(blk, 11);
    SHA256ROUND(f, g, h, a, b, c, d, e, 11, w11);
    w12 =  LOAD(blk, 12);
    SHA256ROUND(e, f, g, h, a, b, c, d, 12, w12);
    w13 =  LOAD(blk, 13);
    SHA256ROUND(d, e, f, g, h, a, b, c, 13, w13);
    w14 =  LOAD(blk, 14);
    SHA256ROUND(c, d, e, f, g, h, a, b, 14, w14);
    w15 =  LOAD(blk, 15);
    SHA256ROUND(b, c, d, e, f, g, h, a, 15, w15);
    
    w0 = add4(SIGMA1_256(w14), w9, SIGMA0_256(w1), w0);
    SHA256ROUND(a, b, c, d, e, f, g, h, 16, w0);
    w1 = add4(SIGMA1_256(w15), w10, SIGMA0_256(w2), w1);
    SHA256ROUND(h, a, b, c, d, e, f, g, 17, w1);
    w2 = add4(SIGMA1_256(w0), w11, SIGMA0_256(w3), w2);
    SHA256ROUND(g, h, a, b, c, d, e, f, 18, w2);
    w3 = add4(SIGMA1_256(w1), w12, SIGMA0_256(w4), w3);
    SHA256ROUND(f, g, h, a, b, c, d, e, 19, w3);
    w4 = add4(SIGMA1_256(w2), w13, SIGMA0_256(w5), w4);
    SHA256ROUND(e, f, g, h, a, b, c, d, 20, w4);
    w5 = add4(SIGMA1_256(w3), w14, SIGMA0_256(w6), w5);
    SHA256ROUND(d, e, f, g, h, a, b, c, 21, w5);
    w6 = add4(SIGMA1_256(w4), w15, SIGMA0_256(w7), w6);
    SHA256ROUND(c, d, e, f, g, h, a, b, 22, w6);
    w7 = add4(SIGMA1_256(w5), w0, SIGMA0_256(w8), w7);
    SHA256ROUND(b, c, d, e, f, g, h, a, 23, w7);
    w8 = add4(SIGMA1_256(w6), w1, SIGMA0_256(w9), w8);
    SHA256ROUND(a, b, c, d, e, f, g, h, 24, w8);
    w9 = add4(SIGMA1_256(w7), w2, SIGMA0_256(w10), w9);
    SHA256ROUND(h, a, b, c, d, e, f, g, 25, w9);
    w10 = add4(SIGMA1_256(w8), w3, SIGMA0_256(w11), w10);
    SHA256ROUND(g, h, a, b, c, d, e, f, 26, w10);
    w11 = add4(SIGMA1_256(w9), w4, SIGMA0_256(w12), w11);
    SHA256ROUND(f, g, h, a, b, c, d, e, 27, w11);
    w12 = add4(SIGMA1_256(w10), w5, SIGMA0_256(w13), w12);
    SHA256ROUND(e, f, g, h, a, b, c, d, 28, w12);
    w13 = add4(SIGMA1_256(w11), w6, SIGMA0_256(w14), w13);
    SHA256ROUND(d, e, f, g, h, a, b, c, 29, w13);
    w14 = add4(SIGMA1_256(w12), w7, SIGMA0_256(w15), w14);
    SHA256ROUND(c, d, e, f, g, h, a, b, 30, w14);
    w15 = add4(SIGMA1_256(w13), w8, SIGMA0_256(w0), w15);
    SHA256ROUND(b, c, d, e, f, g, h, a, 31, w15);
    
    w0 = add4(SIGMA1_256(w14), w9, SIGMA0_256(w1), w0);
    SHA256ROUND(a, b, c, d, e, f, g, h, 32, w0);
    w1 = add4(SIGMA1_256(w15), w10, SIGMA0_256(w2), w1);
    SHA256ROUND(h, a, b, c, d, e, f, g, 33, w1);
    w2 = add4(SIGMA1_256(w0), w11, SIGMA0_256(w3), w2);
    SHA256ROUND(g, h, a, b, c, d, e, f, 34, w2);
    w3 = add4(SIGMA1_256(w1), w12, SIGMA0_256(w4), w3);
    SHA256ROUND(f, g, h, a, b, c, d, e, 35, w3);
    w4 = add4(SIGMA1_256(w2), w13, SIGMA0_256(w5), w4);
    SHA256ROUND(e, f, g, h, a, b, c, d, 36, w4);
    w5 = add4(SIGMA1_256(w3), w14, SIGMA0_256(w6), w5);
    SHA256ROUND(d, e, f, g, h, a, b, c, 37, w5);
    w6 = add4(SIGMA1_256(w4), w15, SIGMA0_256(w7), w6);
    SHA256ROUND(c, d, e, f, g, h, a, b, 38, w6);
    w7 = add4(SIGMA1_256(w5), w0, SIGMA0_256(w8), w7);
    SHA256ROUND(b, c, d, e, f, g, h, a, 39, w7);
    w8 = add4(SIGMA1_256(w6), w1, SIGMA0_256(w9), w8);
    SHA256ROUND(a, b, c, d, e, f, g, h, 40, w8);
    w9 = add4(SIGMA1_256(w7), w2, SIGMA0_256(w10), w9);
    SHA256ROUND(h, a, b, c, d, e, f, g, 41, w9);
    w10 = add4(SIGMA1_256(w8), w3, SIGMA0_256(w11), w10);
    SHA256ROUND(g, h, a, b, c, d, e, f, 42, w10);
    w11 = add4(SIGMA1_256(w9), w4, SIGMA0_256(w12), w11);
    SHA256ROUND(f, g, h, a, b, c, d, e, 43, w11);
    w12 = add4(SIGMA1_256(w10), w5, SIGMA0_256(w13), w12);
    SHA256ROUND(e, f, g, h, a, b, c, d, 44, w12);
    w13 = add4(SIGMA1_256(w11), w6, SIGMA0_256(w14), w13);
    SHA256ROUND(d, e, f, g, h, a, b, c, 45, w13);
    w14 = add4(SIGMA1_256(w12), w7, SIGMA0_256(w15), w14);
    SHA256ROUND(c, d, e, f, g, h, a, b, 46, w14);
    w15 = add4(SIGMA1_256(w13), w8, SIGMA0_256(w0), w15);
    SHA256ROUND(b, c, d, e, f, g, h, a, 47, w15);
    
    w0 = add4(SIGMA1_256(w14), w9, SIGMA0_256(w1), w0);
    SHA256ROUND(a, b, c, d, e, f, g, h, 48, w0);
    w1 = add4(SIGMA1_256(w15), w10, SIGMA0_256(w2), w1);
    SHA256ROUND(h, a, b, c, d, e, f, g, 49, w1);
    w2 = add4(SIGMA1_256(w0), w11, SIGMA0_256(w3), w2);
    SHA256ROUND(g, h, a, b, c, d, e, f, 50, w2);
    w3 = add4(SIGMA1_256(w1), w12, SIGMA0_256(w4), w3);
    SHA256ROUND(f, g, h, a, b, c, d, e, 51, w3);
    w4 = add4(SIGMA1_256(w2), w13, SIGMA0_256(w5), w4);
    SHA256ROUND(e, f, g, h, a, b, c, d, 52, w4);
    w5 = add4(SIGMA1_256(w3), w14, SIGMA0_256(w6), w5);
    SHA256ROUND(d, e, f, g, h, a, b, c, 53, w5);
    w6 = add4(SIGMA1_256(w4), w15, SIGMA0_256(w7), w6);
    SHA256ROUND(c, d, e, f, g, h, a, b, 54, w6);
    w7 = add4(SIGMA1_256(w5), w0, SIGMA0_256(w8), w7);
    SHA256ROUND(b, c, d, e, f, g, h, a, 55, w7);
    w8 = add4(SIGMA1_256(w6), w1, SIGMA0_256(w9), w8);
    SHA256ROUND(a, b, c, d, e, f, g, h, 56, w8);
    w9 = add4(SIGMA1_256(w7), w2, SIGMA0_256(w10), w9);
    SHA256ROUND(h, a, b, c, d, e, f, g, 57, w9);
    w10 = add4(SIGMA1_256(w8), w3, SIGMA0_256(w11), w10);
    SHA256ROUND(g, h, a, b, c, d, e, f, 58, w10);
    w11 = add4(SIGMA1_256(w9), w4, SIGMA0_256(w12), w11);
    SHA256ROUND(f, g, h, a, b, c, d, e, 59, w11);
    w12 = add4(SIGMA1_256(w10), w5, SIGMA0_256(w13), w12);
    SHA256ROUND(e, f, g, h, a, b, c, d, 60, w12);
    w13 = add4(SIGMA1_256(w11), w6, SIGMA0_256(w14), w13);
    SHA256ROUND(d, e, f, g, h, a, b, c, 61, w13);
    w14 = add4(SIGMA1_256(w12), w7, SIGMA0_256(w15), w14);
    SHA256ROUND(c, d, e, f, g, h, a, b, 62, w14);
    w15 = add4(SIGMA1_256(w13), w8, SIGMA0_256(w0), w15);
    SHA256ROUND(b, c, d, e, f, g, h, a, 63, w15);
    
    //dumpreg(d, "last d");


#define store(x,i)  \
    w0 = load_epi32((*h0)[i], (*h1)[i], (*h2)[i], (*h3)[i]); \
    w1 = _mm_add_epi32(w0, x); \
    store_epi32(w1, &(*h0)[i], &(*h1)[i], &(*h2)[i], &(*h3)[i]);

    //printf("%s a: %08x %08x\n", __func__, store32(a), (*h0)[0]);
    store(a, 0);
    //printf("%s a: %08x\n", __func__, (*h0)[0]);
    store(b, 1);
    store(c, 2);
    store(d, 3);
    store(e, 4);
    store(f, 5);
    store(g, 6);
    store(h, 7);
}
