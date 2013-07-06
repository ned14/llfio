
#include "sha256.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define NBLOCKS 4

__attribute__((aligned(16)))
static __sha256_block_t blocks[NBLOCKS];

__attribute__((aligned(16)))
static __sha256_hash_t hash[2][NBLOCKS];


static const uint32_t __sha256_init[] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};


/* Reference implementation taken from opensolaris */
extern void __sha256_osol(const __sha256_block_t blk, __sha256_hash_t hash);
void sha256_osol(int result)
{
    for (int i = 0; i < NBLOCKS; ++i) {
        __builtin_memcpy(hash[result][i], __sha256_init, 32);
        __sha256_osol(blocks[i], hash[result][i]);
    }
}

extern void __sha256_int(__sha256_block_t *blk[4], __sha256_hash_t *hash[4]);
void sha256_int(int result)
{
    for (int i = 0; i < NBLOCKS; ++i) {
        __builtin_memcpy(hash[result][i], __sha256_init, 32);
    }

    __sha256_block_t *blk[4] = { &blocks[0], &blocks[1], &blocks[2], &blocks[3] };
    __sha256_hash_t *hsh[4] = { &hash[result][0], &hash[result][1], &hash[result][2], &hash[result][3] };
    __sha256_int(blk, hsh);
    
    //if (memcmp(&hash[result][0], &hash[result][1], 8*4)) {
    //    printf("error\n");
    //}
}



#define SHA256_HASH_SIZE	(8)
#define SHA256_BLOCK_SIZE	(16)

void dump_H(const uint32_t *h)
{
    int i;
    
    for (i = 0; i < SHA256_HASH_SIZE; ++i) {
        printf(" 0x%08x", h[i]);
    }
}

void one(unsigned n)
{
	unsigned i, j;
	struct timeval tv_start, tv_end;
	double delta;
	double best;
	unsigned n_iter;
    
	n_iter =  1000*(8192/n);
	best = UINT32_MAX;
	for (j = 0; j < 10; ++j) {
		gettimeofday(&tv_start, 0);
		for (i = 0; i < n_iter; ++i) {
			sha256_osol(0);
		}
		gettimeofday(&tv_end, 0);
        
		__asm volatile("emms");
        
		delta = (double)(tv_end.tv_sec - tv_start.tv_sec)
        + (double)(tv_end.tv_usec - tv_start.tv_usec) / 1000000.0;
		if (delta < best) {
			best = delta;
		}
	}
	/* print a number similar to what openssl reports */
    printf("%.2f blocks per second\n", (double)(4 * n_iter) / best / 1000.0 + 0.005);
    
    n_iter =  1000*(8192/n);
	best = UINT32_MAX;
	for (j = 0; j < 10; ++j) {
		gettimeofday(&tv_start, 0);
		for (i = 0; i < n_iter; ++i) {
			sha256_int(0);
		}
		gettimeofday(&tv_end, 0);
        
		__asm volatile("emms");
        
		delta = (double)(tv_end.tv_sec - tv_start.tv_sec)
        + (double)(tv_end.tv_usec - tv_start.tv_usec) / 1000000.0;
		if (delta < best) {
			best = delta;
		}
	}
	/* print a number similar to what openssl reports */
    printf("%.2f blocks per second\n", (double)(4 * n_iter) / best / 1000.0 + 0.005);
}

static void test()
{
    srand(time(NULL));
    uint8_t *ptr = (uint8_t *) blocks;
    for (int i = 0; i < NBLOCKS * sizeof(__sha256_block_t); ++i) {
        *ptr++ = rand();
    }
    
    sha256_osol(0);
    sha256_int(1);
    
    for (int i = 0; i < NBLOCKS; ++i) {
        if (memcmp(hash[0][i], hash[1][i], sizeof(__sha256_hash_t))) {
            printf("FAILED at %d\n", i);
            dump_H(hash[0][i]); printf("\n");
            dump_H(hash[1][i]); printf("\n");
        }
    }
}

int main(int argc, const char * argv[])
{
	test();
	one(1024);
    
    return 0;
}

