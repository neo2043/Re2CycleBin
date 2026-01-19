#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "wyhash.h"

#if defined(_WIN32)
#include <windows.h>
static double now_sec(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}
#else
#include <time.h>
static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
#endif

static void print_hex_u64(uint64_t v) {
    printf("0x%016llx", (unsigned long long)v);
}

int main(int argc, char **argv) {
    printf("=== WYHash v4.2 Verification ===\n");

    const char *input = "WYHash Integration Test";
    uint64_t seed = 0x1234567890abcdefULL;
    uint64_t h = wyhash(input, strlen(input), seed, _wyp);

    printf("Input : \"%s\"\n", input);
    printf("Seed  : 0x%016llx\n", (unsigned long long)seed);
    printf("Hash  : ");
    print_hex_u64(h);
    printf("\n");

    if (h == 0ULL) {
        printf("❌ Something is wrong: hash returned 0.\n");
        return 1;
    } else {
        printf("✅ WYHash successfully included and functioning.\n\n");
    }

    // Optional benchmark: default 512 MB buffer
    size_t bytes = (argc > 1) ? strtoull(argv[1], NULL, 10) : (size_t)512 * 1024 * 1024;
    printf("Benchmarking %zu bytes buffer...\n", bytes);

    uint8_t *buf = (uint8_t *)malloc(bytes);
    if (!buf) {
        fprintf(stderr, "Allocation failed.\n");
        return 1;
    }

    for (size_t i = 0; i < bytes; ++i) buf[i] = (uint8_t)(i & 0xFF);

    // Warm-up
    wyhash(buf, bytes / 4, seed, _wyp);

    int iterations = 3;
    double t0 = now_sec();
    uint64_t acc = 0;
    for (int it = 0; it < iterations; ++it) {
        acc ^= wyhash(buf, bytes, seed + it, _wyp);
    }
    double t1 = now_sec();
    double elapsed = (t1 - t0) / iterations;
    double mb = (double)bytes / (1024.0 * 1024.0);

    printf("Average time : %.6fs\n", elapsed);
    printf("Throughput   : %.2f MB/s\n", mb / elapsed);
    printf("Checksum hash: 0x%016llx\n", (unsigned long long)acc);

    free(buf);
    return 0;
}
