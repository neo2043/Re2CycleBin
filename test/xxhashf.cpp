#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstring>

// Use xxHash in header-only mode
#define XXH_INLINE_ALL
#include "xxhash.h"

static inline double now_sec() {
    using namespace std::chrono;
    return duration<double>(high_resolution_clock::now().time_since_epoch()).count();
}

template <typename T>
void print_hex(const T &val) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&val);
    std::cout << "0x";
    for (size_t i = 0; i < sizeof(T); ++i)
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(bytes[i]);
    std::cout << std::dec;
}

int main(int argc, char **argv) {
    std::cout << "=== xxHash Verification ===\n";

    const char *input = "xxHash Integration Test";
    size_t len = std::strlen(input);

    // Seed value (can be 0)
    unsigned long long seed = 0x1234567890abcdefULL;

    // Compute various hashes
    uint32_t h32 = XXH32(input, len, static_cast<uint32_t>(seed));
    uint64_t h64 = XXH64(input, len, seed);
    XXH128_hash_t h128 = XXH3_128bits_withSeed(input, len, seed);

    std::cout << "Input : \"" << input << "\"\n";
    std::cout << "Seed  : 0x" << std::hex << seed << std::dec << "\n";
    std::cout << "XXH32 : "; print_hex(h32);  std::cout << "\n";
    std::cout << "XXH64 : "; print_hex(h64);  std::cout << "\n";
    std::cout << "XXH3  : "; print_hex(h128.low64); print_hex(h128.high64); std::cout << "\n";

    if (h32 == 0 && h64 == 0 && h128.low64 == 0 && h128.high64 == 0) {
        std::cerr << "❌ Something is wrong: all hashes are zero.\n";
        return 1;
    }

    std::cout << "✅ xxHash successfully included and functioning.\n\n";

    // --- Optional Benchmark ---
    size_t bytes = (argc > 1) ? std::stoull(argv[1]) : size_t(2048) * 1024 * 1024;
    std::vector<uint8_t> buf(bytes);
    for (size_t i = 0; i < bytes; ++i)
        buf[i] = static_cast<uint8_t>(i);

    std::cout << "Benchmarking " << bytes / (1024 * 1024) << " MB buffer...\n";

    XXH64_hash_t checksum = 0;
    int iterations = 3;
    double total = 0.0;

    for (int it = 0; it < iterations; ++it) {
        double t0 = now_sec();
        checksum ^= XXH64(buf.data(), buf.size(), seed + it);
        double t1 = now_sec();
        total += (t1 - t0);
    }

    double avg = total / iterations;
    double mb = bytes / (1024.0 * 1024.0);
    std::cout << "Average time : " << avg << " s\n";
    std::cout << "Throughput   : " << std::fixed << std::setprecision(2)
              << (mb / avg) << " MB/s\n";
    std::cout << "Checksum hash: "; print_hex(checksum); std::cout << "\n";

    return 0;
}