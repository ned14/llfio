//
//  sha-256-amd64.c
//  sha

#include "sha256.h"

#include <arm_neon.h>
#include <stdint.h>

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

// I'm really lazy so ...
typedef uint32x4_t __m128i;
#define _mm_andnot_si128(a, b) vbicq_u32((b), (a))
#define _mm_and_si128(a, b) vandq_u32((a), (b))
#define _mm_or_si128(a, b) vorrq_u32((a), (b))
#define _mm_xor_si128(a, b) veorq_u32((a), (b))
#define _mm_set1_epi32(a) vdupq_n_u32((a))
#define _mm_add_epi32(a, b) vaddq_u32((a), (b))

static inline __m128i Ch(__m128i b, __m128i c, __m128i d) {
    return _mm_xor_si128(_mm_and_si128(b, c), _mm_andnot_si128(b, d));
}

static inline __m128i Maj(__m128i b, __m128i c, __m128i d) {
    return _mm_xor_si128(_mm_xor_si128(_mm_and_si128(b, c), _mm_and_si128(b, d)), _mm_and_si128(c, d));
}

/*static inline __m128i ROTR(__m128i x, int n) {
    return _mm_or_si128(_mm_srli_epi32(x, n), _mm_slli_epi32(x, 32 - n));
}*/
#define ROTR(a, b) vorrq_u32(vshrq_n_u32((a), (b)), vshlq_n_u32((a), 32 - (b)))

/*static inline __m128i SHR(__m128i x, int n) {
    return _mm_srli_epi32(x, n);
}*/
#define SHR(a, b) vshrq_n_u32((a), (b))

/* SHA256 Functions */
#define	BIGSIGMA0_256(x)	(_mm_xor_si128(_mm_xor_si128(ROTR((x), 2), ROTR((x), 13)), ROTR((x), 22)))
#define	BIGSIGMA1_256(x)	(_mm_xor_si128(_mm_xor_si128(ROTR((x), 6), ROTR((x), 11)), ROTR((x), 25)))
#define	SIGMA0_256(x)		(_mm_xor_si128(_mm_xor_si128(ROTR((x), 7), ROTR((x), 18)), SHR((x), 3)))
#define	SIGMA1_256(x)		(_mm_xor_si128(_mm_xor_si128(ROTR((x), 17), ROTR((x), 19)), SHR((x), 10)))

/*static inline __m128i load_epi32(uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3) {
	return _mm_set_epi32(x0, x1, x2, x3);
}*/
static inline __m128i load_epi32(uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3) {
#if 1
  uint32_t v[4] = { (x3), (x2), (x1), (x0) };
  return vld1q_u32(v);
#else
  uint32x4_t v;
  v=vsetq_lane_u32(x0, v, 3);
  v=vsetq_lane_u32(x1, v, 2);
  v=vsetq_lane_u32(x2, v, 1);
  v=vsetq_lane_u32(x3, v, 0);
  return v;
#endif
}

/*static inline uint32_t store32(__m128i x) {
    union { uint32_t ret[0]; __m128i x; } box;
    box.x = x;
    return box.ret[0];
}*/
//#define store32(x) vgetq_lane_u32((x), 0)

/*static inline void store_epi32(__m128i x, uint32_t *x0, uint32_t *x1, uint32_t *x2, uint32_t *x3) {
    union { uint32_t ret[4]; __m128i x; } box; box.x=x;
    *x0 = box.ret[3]; *x1 = box.ret[2]; *x2 = box.ret[1]; *x3 = box.ret[0];
}*/
#if 0
static inline void store_epi32(__m128i x, uint32_t *x0, uint32_t *x1, uint32_t *x2, uint32_t *x3) {
    union { uint32_t ret[4]; __m128i x; } box; box.x=x;
    *x0 = box.ret[3]; *x1 = box.ret[2]; *x2 = box.ret[1]; *x3 = box.ret[0];
}
#else
#define store_epi32(x, x0, x1, x2, x3) (*(x0)=vgetq_lane_u32((x), 3), *(x1)=vgetq_lane_u32((x), 2), *(x2)=vgetq_lane_u32((x), 1), *(x3)=vgetq_lane_u32((x), 0))
#endif

/*static inline __m128i SHA256_CONST(int i) {
    return _mm_set1_epi32(sha256_consts[i]);
}*/
#define SHA256_CONST(i) vdupq_n_u32(SHA256_CONST_(i))

#define add4(x0, x1, x2, x3) _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(x0, x1), x2), x3)
#define add5(x0, x1, x2, x3, x4) _mm_add_epi32(add4(x0, x1, x2, x3), x4)


#define	SHA256ROUND(a, b, c, d, e, f, g, h, i, w)                       \
    T1 = add5(h, BIGSIGMA1_256(e), Ch(e, f, g), SHA256_CONST(i), w);	\
    d = _mm_add_epi32(d, T1);                                           \
    T2 = _mm_add_epi32(BIGSIGMA0_256(a), Maj(a, b, c));                 \
    h = _mm_add_epi32(T1, T2);

/*static inline uint32_t SWAP32(const void *addr) {
#ifdef _MSC_VER
	return _byteswap_ulong(*((uint32_t *)(addr)));
#else
    return __builtin_bswap32(*((uint32_t *)(addr)));
#endif
}*/

/*static inline __m128i LOAD(const __sha256_block_t *blk[4], int i) {
    return load_epi32(SWAP32(*blk[0] + i * 4), SWAP32(*blk[1] + i * 4), SWAP32(*blk[2] + i * 4), SWAP32(*blk[3] + i * 4));
}*/
#if 0
static inline __m128i LOAD(const __sha256_block_t *__restrict blk[4], int i) {
#if 1
    uint32_t v[4];
    v[3]=*((uint32_t *)(*blk[0] + i * 4));
    v[2]=*((uint32_t *)(*blk[1] + i * 4));
    v[1]=*((uint32_t *)(*blk[2] + i * 4));
    v[0]=*((uint32_t *)(*blk[3] + i * 4));
    return vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32(v))));
#else
    uint32x4_t v;
    v=vsetq_lane_u32(*((uint32_t *)(*blk[0] + i * 4)), v, 3);
    v=vsetq_lane_u32(*((uint32_t *)(*blk[1] + i * 4)), v, 2);
    v=vsetq_lane_u32(*((uint32_t *)(*blk[2] + i * 4)), v, 1);
    v=vsetq_lane_u32(*((uint32_t *)(*blk[3] + i * 4)), v, 0);
    return vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(v)));
#endif
}
static inline void LOAD4(__m128i *__restrict _out, const __sha256_block_t *__restrict blk[4], int i) {
    size_t n;
    for(n=0; n<4; n++)
        _out[n]=LOAD(blk, i+n);
}
#else
static inline void LOAD4(__m128i *__restrict _out, const __sha256_block_t *__restrict blk[4], int i) {
    uint32_t *__restrict out=(uint32_t *__restrict) _out;
    uint32x4x4_t b;
    b.val[3]=vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32((uint32_t *)(*blk[0] + i * 4)))));
    b.val[2]=vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32((uint32_t *)(*blk[1] + i * 4)))));
    b.val[1]=vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32((uint32_t *)(*blk[2] + i * 4)))));
    b.val[0]=vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32((uint32_t *)(*blk[3] + i * 4)))));
#if 0
    // Transpose
    vst4q_u32(out, b);
#else
    uint32x4x2_t x=vtrnq_u32(b.val[0], b.val[1]), y=vtrnq_u32(b.val[2], b.val[3]);
    _out[0]=vcombine_u32(vget_low_u32(x.val[0]), vget_low_u32(y.val[0]));
    _out[1]=vcombine_u32(vget_low_u32(x.val[1]), vget_low_u32(y.val[1]));
    _out[2]=vcombine_u32(vget_high_u32(x.val[0]), vget_high_u32(y.val[0]));
    _out[3]=vcombine_u32(vget_high_u32(x.val[1]), vget_high_u32(y.val[1]));
#endif
}
#endif


static inline void dumpreg(__m128i x, const char *msg) {
    union { uint32_t ret[4]; __m128i x; } box; box.x = x;
    printf("%s %08x %08x %08x %08x\n", msg, box.ret[0], box.ret[1], box.ret[2], box.ret[3]);
}

#if 0
#define dumpstate() printf("%s: %08x %08x %08x %08x %08x %08x %08x %08x %08x\n", \
__func__, store32(w0), store32(a), store32(b), store32(c), store32(d), store32(e), store32(f), store32(g), store32(h));
#else
#define dumpstate()
#endif

void __sha256_int(const __sha256_block_t *__restrict blk[4], __sha256_hash_t *__restrict hash[4])
{
    __sha256_hash_t *__restrict h0 = hash[0], *__restrict h1 = hash[1], *__restrict h2 = hash[2], *__restrict h3 = hash[3];
#define load(x, i) __m128i x = load_epi32((*h0)[i], (*h1)[i], (*h2)[i], (*h3)[i])

    load(a, 0);
    load(b, 1);
    load(c, 2);
    load(d, 3);
    load(e, 4);
    load(f, 5);
    load(g, 6);
    load(h, 7);    
    
    __m128i w[16];
    __m128i T1, T2;
    
    LOAD4(w+0, blk, 0);
    //w[0] =  LOAD(blk, 0);
    SHA256ROUND(a, b, c, d, e, f, g, h, 0, w[0]);    
    //w[1] =  LOAD(blk, 1);
    SHA256ROUND(h, a, b, c, d, e, f, g, 1, w[1]);
    //w[2] =  LOAD(blk, 2);
    SHA256ROUND(g, h, a, b, c, d, e, f, 2, w[2]);
    //w[3] =  LOAD(blk, 3);
    SHA256ROUND(f, g, h, a, b, c, d, e, 3, w[3]);
    LOAD4(w+4, blk, 4);
    //w[4] =  LOAD(blk, 4);
    SHA256ROUND(e, f, g, h, a, b, c, d, 4, w[4]);
    //w[5] =  LOAD(blk, 5);
    SHA256ROUND(d, e, f, g, h, a, b, c, 5, w[5]);
    //w[6] =  LOAD(blk, 6);
    SHA256ROUND(c, d, e, f, g, h, a, b, 6, w[6]);
    //w[7] =  LOAD(blk, 7);
    SHA256ROUND(b, c, d, e, f, g, h, a, 7, w[7]);
    LOAD4(w+8, blk, 8);
    //w[8] =  LOAD(blk, 8);
    SHA256ROUND(a, b, c, d, e, f, g, h, 8, w[8]);
    //w[9] =  LOAD(blk, 9);
    SHA256ROUND(h, a, b, c, d, e, f, g, 9, w[9]);
    //w[10] =  LOAD(blk, 10);
    SHA256ROUND(g, h, a, b, c, d, e, f, 10, w[10]);
    //w[11] =  LOAD(blk, 11);
    SHA256ROUND(f, g, h, a, b, c, d, e, 11, w[11]);
    LOAD4(w+12, blk, 12);
    //w[12] =  LOAD(blk, 12);
    SHA256ROUND(e, f, g, h, a, b, c, d, 12, w[12]);
    //w[13] =  LOAD(blk, 13);
    SHA256ROUND(d, e, f, g, h, a, b, c, 13, w[13]);
    //w[14] =  LOAD(blk, 14);
    SHA256ROUND(c, d, e, f, g, h, a, b, 14, w[14]);
    //w[15] =  LOAD(blk, 15);
    SHA256ROUND(b, c, d, e, f, g, h, a, 15, w[15]);
    
    w[0] = add4(SIGMA1_256(w[14]), w[9], SIGMA0_256(w[1]), w[0]);
    SHA256ROUND(a, b, c, d, e, f, g, h, 16, w[0]);
    w[1] = add4(SIGMA1_256(w[15]), w[10], SIGMA0_256(w[2]), w[1]);
    SHA256ROUND(h, a, b, c, d, e, f, g, 17, w[1]);
    w[2] = add4(SIGMA1_256(w[0]), w[11], SIGMA0_256(w[3]), w[2]);
    SHA256ROUND(g, h, a, b, c, d, e, f, 18, w[2]);
    w[3] = add4(SIGMA1_256(w[1]), w[12], SIGMA0_256(w[4]), w[3]);
    SHA256ROUND(f, g, h, a, b, c, d, e, 19, w[3]);
    w[4] = add4(SIGMA1_256(w[2]), w[13], SIGMA0_256(w[5]), w[4]);
    SHA256ROUND(e, f, g, h, a, b, c, d, 20, w[4]);
    w[5] = add4(SIGMA1_256(w[3]), w[14], SIGMA0_256(w[6]), w[5]);
    SHA256ROUND(d, e, f, g, h, a, b, c, 21, w[5]);
    w[6] = add4(SIGMA1_256(w[4]), w[15], SIGMA0_256(w[7]), w[6]);
    SHA256ROUND(c, d, e, f, g, h, a, b, 22, w[6]);
    w[7] = add4(SIGMA1_256(w[5]), w[0], SIGMA0_256(w[8]), w[7]);
    SHA256ROUND(b, c, d, e, f, g, h, a, 23, w[7]);
    w[8] = add4(SIGMA1_256(w[6]), w[1], SIGMA0_256(w[9]), w[8]);
    SHA256ROUND(a, b, c, d, e, f, g, h, 24, w[8]);
    w[9] = add4(SIGMA1_256(w[7]), w[2], SIGMA0_256(w[10]), w[9]);
    SHA256ROUND(h, a, b, c, d, e, f, g, 25, w[9]);
    w[10] = add4(SIGMA1_256(w[8]), w[3], SIGMA0_256(w[11]), w[10]);
    SHA256ROUND(g, h, a, b, c, d, e, f, 26, w[10]);
    w[11] = add4(SIGMA1_256(w[9]), w[4], SIGMA0_256(w[12]), w[11]);
    SHA256ROUND(f, g, h, a, b, c, d, e, 27, w[11]);
    w[12] = add4(SIGMA1_256(w[10]), w[5], SIGMA0_256(w[13]), w[12]);
    SHA256ROUND(e, f, g, h, a, b, c, d, 28, w[12]);
    w[13] = add4(SIGMA1_256(w[11]), w[6], SIGMA0_256(w[14]), w[13]);
    SHA256ROUND(d, e, f, g, h, a, b, c, 29, w[13]);
    w[14] = add4(SIGMA1_256(w[12]), w[7], SIGMA0_256(w[15]), w[14]);
    SHA256ROUND(c, d, e, f, g, h, a, b, 30, w[14]);
    w[15] = add4(SIGMA1_256(w[13]), w[8], SIGMA0_256(w[0]), w[15]);
    SHA256ROUND(b, c, d, e, f, g, h, a, 31, w[15]);
    
    w[0] = add4(SIGMA1_256(w[14]), w[9], SIGMA0_256(w[1]), w[0]);
    SHA256ROUND(a, b, c, d, e, f, g, h, 32, w[0]);
    w[1] = add4(SIGMA1_256(w[15]), w[10], SIGMA0_256(w[2]), w[1]);
    SHA256ROUND(h, a, b, c, d, e, f, g, 33, w[1]);
    w[2] = add4(SIGMA1_256(w[0]), w[11], SIGMA0_256(w[3]), w[2]);
    SHA256ROUND(g, h, a, b, c, d, e, f, 34, w[2]);
    w[3] = add4(SIGMA1_256(w[1]), w[12], SIGMA0_256(w[4]), w[3]);
    SHA256ROUND(f, g, h, a, b, c, d, e, 35, w[3]);
    w[4] = add4(SIGMA1_256(w[2]), w[13], SIGMA0_256(w[5]), w[4]);
    SHA256ROUND(e, f, g, h, a, b, c, d, 36, w[4]);
    w[5] = add4(SIGMA1_256(w[3]), w[14], SIGMA0_256(w[6]), w[5]);
    SHA256ROUND(d, e, f, g, h, a, b, c, 37, w[5]);
    w[6] = add4(SIGMA1_256(w[4]), w[15], SIGMA0_256(w[7]), w[6]);
    SHA256ROUND(c, d, e, f, g, h, a, b, 38, w[6]);
    w[7] = add4(SIGMA1_256(w[5]), w[0], SIGMA0_256(w[8]), w[7]);
    SHA256ROUND(b, c, d, e, f, g, h, a, 39, w[7]);
    w[8] = add4(SIGMA1_256(w[6]), w[1], SIGMA0_256(w[9]), w[8]);
    SHA256ROUND(a, b, c, d, e, f, g, h, 40, w[8]);
    w[9] = add4(SIGMA1_256(w[7]), w[2], SIGMA0_256(w[10]), w[9]);
    SHA256ROUND(h, a, b, c, d, e, f, g, 41, w[9]);
    w[10] = add4(SIGMA1_256(w[8]), w[3], SIGMA0_256(w[11]), w[10]);
    SHA256ROUND(g, h, a, b, c, d, e, f, 42, w[10]);
    w[11] = add4(SIGMA1_256(w[9]), w[4], SIGMA0_256(w[12]), w[11]);
    SHA256ROUND(f, g, h, a, b, c, d, e, 43, w[11]);
    w[12] = add4(SIGMA1_256(w[10]), w[5], SIGMA0_256(w[13]), w[12]);
    SHA256ROUND(e, f, g, h, a, b, c, d, 44, w[12]);
    w[13] = add4(SIGMA1_256(w[11]), w[6], SIGMA0_256(w[14]), w[13]);
    SHA256ROUND(d, e, f, g, h, a, b, c, 45, w[13]);
    w[14] = add4(SIGMA1_256(w[12]), w[7], SIGMA0_256(w[15]), w[14]);
    SHA256ROUND(c, d, e, f, g, h, a, b, 46, w[14]);
    w[15] = add4(SIGMA1_256(w[13]), w[8], SIGMA0_256(w[0]), w[15]);
    SHA256ROUND(b, c, d, e, f, g, h, a, 47, w[15]);
    
    w[0] = add4(SIGMA1_256(w[14]), w[9], SIGMA0_256(w[1]), w[0]);
    SHA256ROUND(a, b, c, d, e, f, g, h, 48, w[0]);
    w[1] = add4(SIGMA1_256(w[15]), w[10], SIGMA0_256(w[2]), w[1]);
    SHA256ROUND(h, a, b, c, d, e, f, g, 49, w[1]);
    w[2] = add4(SIGMA1_256(w[0]), w[11], SIGMA0_256(w[3]), w[2]);
    SHA256ROUND(g, h, a, b, c, d, e, f, 50, w[2]);
    w[3] = add4(SIGMA1_256(w[1]), w[12], SIGMA0_256(w[4]), w[3]);
    SHA256ROUND(f, g, h, a, b, c, d, e, 51, w[3]);
    w[4] = add4(SIGMA1_256(w[2]), w[13], SIGMA0_256(w[5]), w[4]);
    SHA256ROUND(e, f, g, h, a, b, c, d, 52, w[4]);
    w[5] = add4(SIGMA1_256(w[3]), w[14], SIGMA0_256(w[6]), w[5]);
    SHA256ROUND(d, e, f, g, h, a, b, c, 53, w[5]);
    w[6] = add4(SIGMA1_256(w[4]), w[15], SIGMA0_256(w[7]), w[6]);
    SHA256ROUND(c, d, e, f, g, h, a, b, 54, w[6]);
    w[7] = add4(SIGMA1_256(w[5]), w[0], SIGMA0_256(w[8]), w[7]);
    SHA256ROUND(b, c, d, e, f, g, h, a, 55, w[7]);
    w[8] = add4(SIGMA1_256(w[6]), w[1], SIGMA0_256(w[9]), w[8]);
    SHA256ROUND(a, b, c, d, e, f, g, h, 56, w[8]);
    w[9] = add4(SIGMA1_256(w[7]), w[2], SIGMA0_256(w[10]), w[9]);
    SHA256ROUND(h, a, b, c, d, e, f, g, 57, w[9]);
    w[10] = add4(SIGMA1_256(w[8]), w[3], SIGMA0_256(w[11]), w[10]);
    SHA256ROUND(g, h, a, b, c, d, e, f, 58, w[10]);
    w[11] = add4(SIGMA1_256(w[9]), w[4], SIGMA0_256(w[12]), w[11]);
    SHA256ROUND(f, g, h, a, b, c, d, e, 59, w[11]);
    w[12] = add4(SIGMA1_256(w[10]), w[5], SIGMA0_256(w[13]), w[12]);
    SHA256ROUND(e, f, g, h, a, b, c, d, 60, w[12]);
    w[13] = add4(SIGMA1_256(w[11]), w[6], SIGMA0_256(w[14]), w[13]);
    SHA256ROUND(d, e, f, g, h, a, b, c, 61, w[13]);
    w[14] = add4(SIGMA1_256(w[12]), w[7], SIGMA0_256(w[15]), w[14]);
    SHA256ROUND(c, d, e, f, g, h, a, b, 62, w[14]);
    w[15] = add4(SIGMA1_256(w[13]), w[8], SIGMA0_256(w[0]), w[15]);
    SHA256ROUND(b, c, d, e, f, g, h, a, 63, w[15]);
    
    //dumpreg(d, "last d");


#define store(x,i)  \
    w[0] = load_epi32((*h0)[i], (*h1)[i], (*h2)[i], (*h3)[i]); \
    w[1] = _mm_add_epi32(w[0], x); \
    store_epi32(w[1], &(*h0)[i], &(*h1)[i], &(*h2)[i], &(*h3)[i]);

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
