// #include <iostream>
// #include <fstream>
// #include <vector>
// #include <iomanip>
// #include "crc64fast_nvme.h"  // <-- your header file
// #include <windows.h>

// int main(int argc, char* argv[]) {
//     // SetDllDirectoryA("");

//     if (argc < 2) {
//         std::cerr << "Usage: " << argv[0] << " <file_path>\n";
//         return 1;
//     }

//     const char* file_path = argv[1];

//     // Create a new digest
//     DigestHandle* handle = digest_new();
//     if (!handle) {
//         std::cerr << "Failed to create CRC64 digest\n";
//         return 1;
//     }

//     // Open file in binary mode
//     std::ifstream file(file_path, std::ios::binary);
//     if (!file) {
//         std::cerr << "Failed to open file: " << file_path << "\n";
//         digest_free(handle);
//         return 1;
//     }

//     // Read file in chunks
//     const size_t BUF_SIZE = 4096;
//     std::vector<char> buffer(BUF_SIZE);

//     while (file) {
//         file.read(buffer.data(), BUF_SIZE);
//         std::streamsize bytes_read = file.gcount();

//         if (bytes_read > 0) {
//             digest_write(handle, buffer.data(), static_cast<uintptr_t>(bytes_read));
//         }
//     }

//     // Compute the checksum
//     uint64_t checksum = digest_sum64(handle);

//     // Print as hexadecimal
//     std::cout << "CRC64(" << file_path << ") = 0x"
//               << std::hex << std::uppercase << std::setfill('0')
//               << std::setw(16) << checksum << "\n";

//     // Free resources
//     digest_free(handle);

//     return 0;
// }

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <cstring>
#include <iomanip>

extern "C" {
    struct DigestHandle;
    DigestHandle* digest_new();
    void digest_write(DigestHandle* handle, const char* data, uintptr_t len);
    uint64_t digest_sum64(const DigestHandle* handle);
    void digest_free(DigestHandle* handle);
}

using namespace std;
using namespace std::chrono;

int main(int argc, char** argv) {
    cout << "=== CRC64-Fast NVME Benchmark ===\n";

    size_t data_size = (argc > 1) ? stoull(argv[1]) : size_t(512) * 1024 * 1024; // 512MB
    int iterations = 5;
    cout << "Buffer size : " << (data_size / (1024.0 * 1024.0)) << " MB\n";
    cout << "Iterations  : " << iterations << "\n";

    // Prepare test data
    vector<char> data(data_size);
    mt19937_64 rng(12345);
    uniform_int_distribution<int> dist(0, 255);
    for (auto& b : data) b = static_cast<char>(dist(rng));

    // Warm-up
    DigestHandle* digest = digest_new();
    digest_write(digest, data.data(), data.size());
    uint64_t warmup_crc = digest_sum64(digest);
    digest_free(digest);
    cout << "Warm-up CRC: 0x" << hex << warmup_crc << dec << "\n";

    // Benchmark loop
    double total_time = 0.0;
    uint64_t final_crc = 0;
    for (int i = 0; i < iterations; ++i) {
        digest = digest_new();
        auto t0 = high_resolution_clock::now();
        digest_write(digest, data.data(), data.size());
        final_crc = digest_sum64(digest);
        auto t1 = high_resolution_clock::now();
        digest_free(digest);

        double elapsed = duration<double>(t1 - t0).count();
        total_time += elapsed;
        double mbps = (data_size / (1024.0 * 1024.0)) / elapsed;
        cout << "Iteration " << (i + 1) << ": " << fixed << setprecision(2) << mbps << " MB/s\n";
    }

    double avg_time = total_time / iterations;
    double avg_mbps = (data_size / (1024.0 * 1024.0)) / avg_time;
    cout << "\nAverage time: " << avg_time << " s\n";
    cout << "Throughput  : " << avg_mbps << " MB/s\n";
    cout << "Final CRC64 : 0x" << hex << final_crc << dec << "\n";

    // Test vector check
    const char* test_str = "hello world";
    digest = digest_new();
    digest_write(digest, test_str, strlen(test_str));
    uint64_t test_crc = digest_sum64(digest);
    digest_free(digest);

    cout << "\nTest string: \"" << test_str << "\"\n";
    cout << "CRC64(NVME): 0x" << hex << test_crc << dec << "\n";

    return 0;
}
