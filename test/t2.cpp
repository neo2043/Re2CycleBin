#include "blake3.h"
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>

#define CHUNK_SIZE (1024 * 1024) // 1 MB

int main(int argc, char **argv) {
    if (argc < 2) return 1;

    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;

    uint8_t *buffer = (uint8_t*)malloc(CHUNK_SIZE);
    uint8_t chunk_hash[BLAKE3_OUT_LEN];
    blake3_hasher final_hasher;
    blake3_hasher_init(&final_hasher);

    size_t n;
    while ((n = fread(buffer, 1, CHUNK_SIZE, f)) > 0) {
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, buffer, n);
        blake3_hasher_finalize(&hasher, chunk_hash, BLAKE3_OUT_LEN);

        // Feed this chunk’s hash into the overall hasher
        blake3_hasher_update(&final_hasher, chunk_hash, BLAKE3_OUT_LEN);
    }

    uint8_t final_hash[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&final_hasher, final_hash, BLAKE3_OUT_LEN);

    for (int i = 0; i < BLAKE3_OUT_LEN; i++)
        printf("%02x", final_hash[i]);
    printf("\n");

    free(buffer);
    fclose(f);
    return 0;
}
