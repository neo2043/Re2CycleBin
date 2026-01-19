#include "blake3.h"
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>

#define CHUNK_SIZE (1024 * 1024) // 1 MB

int main(int argc, char **argv) {
    if (argc < 2) return 1;

    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;

    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    uint8_t *buffer = (uint8_t*)malloc(CHUNK_SIZE);
    size_t n;
    while ((n = fread(buffer, 1, CHUNK_SIZE, f)) > 0) {
        blake3_hasher_update(&hasher, buffer, n);
    }

    uint8_t out[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);

    for (int i = 0; i < BLAKE3_OUT_LEN; i++)
        printf("%02x", out[i]);
    printf("\n");

    free(buffer);
    fclose(f);
    return 0;
}
