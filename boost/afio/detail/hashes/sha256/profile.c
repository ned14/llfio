
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
static __sha256_hash_t hash[NBLOCKS];


static const uint32_t __sha256_init[] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};


/* Reference implementation taken from opensolaris */
extern void __sha256_osol(const __sha256_block_t blk, __sha256_hash_t hash);
void sha256_osol()
{
    for (int i = 0; i < NBLOCKS; ++i) {
        __builtin_memcpy(hash[i], __sha256_init, 32);
        __sha256_osol(blocks[i], hash[i]);
    }
}

extern void __sha256_int(__sha256_block_t *blk[4], __sha256_hash_t *hash[4]);
void sha256_sse()
{
    for (int i = 0; i < NBLOCKS; ++i) {
        __builtin_memcpy(hash[i], __sha256_init, 32);
    }

    __sha256_block_t *blk[4] = { &blocks[0], &blocks[1], &blocks[2], &blocks[3] };
    __sha256_hash_t *hsh[4] = { &hash[0], &hash[1], &hash[2], &hash[3] };
    __sha256_int(blk, hsh);
}


int main(int argc, const char * argv[])
{
	for (;;) {
		sha256_sse();
	}
    
    return 0;
}

