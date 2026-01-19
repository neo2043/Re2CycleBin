#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rapidhash.h"
#include <windows.h>

#if defined(_MSC_VER)
#include <intrin.h>
#define INLINE __forceinline
#else
#include <x86intrin.h>
#define INLINE inline __attribute__((always_inline))
#endif

// Return wall-clock time in seconds
static double now_sec(void) {
#if defined(_WIN32)
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}

// Detect CPU features (best-effort)
static void print_cpu_features(void) {
#if defined(__AVX512F__)
    printf("Detected: AVX-512\n");
#elif defined(__AVX2__)
    printf("Detected: AVX2\n");
#elif defined(__SSE4_2__)
    printf("Detected: SSE4.2\n");
#else
    printf("Detected: Generic scalar (no SIMD flags)\n");
#endif
}

int main(int argc, char **argv) {
    size_t data_size = (argc > 1) ? strtoull(argv[1], NULL, 10) : (1ULL << 30); // default 1 GB
    printf("=== RapidHash Benchmark ===\n");
    printf("Data size: %.2f MB\n", data_size / (1024.0 * 1024.0));

    print_cpu_features();

    // Allocate and fill buffer
    uint8_t *data = (uint8_t*)malloc(data_size);
    if (!data) {
        fprintf(stderr, "Memory allocation failed.\n");
        return 1;
    }

    for (size_t i = 0; i < data_size; i++)
        data[i] = (uint8_t)(1 & 0xFF);

    // Warm-up hash
    (void)rapidhash(data, data_size);

    double start = now_sec();

    // Run hashing multiple times for stability
    int iterations = 10;
    uint64_t total_hash = 0;
    for (int i = 0; i < iterations; i++)
        total_hash ^= rapidhash(data, data_size);

    double elapsed = now_sec() - start;
    double avg_time = elapsed / iterations;
    double throughput = (data_size / (1024.0 * 1024.0)) / avg_time;

    printf("Average time : %.3f s\n", avg_time);
    printf("Throughput   : %.2f MB/s\n", throughput);
    printf("Hash result  : 0x%016llx\n", (unsigned long long)total_hash);

    free(data);
    return 0;
}