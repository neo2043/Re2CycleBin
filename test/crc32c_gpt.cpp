// // #include "crc32c/crc32c.h"

// // int main() {
// //   const std::uint8_t buffer[] = {0, 0, 0, 0};
// //   std::uint32_t result;

// //   // Process a raw buffer.
// //   result = crc32c::Crc32c(buffer, 4);

// //   // Process a std::string.
// //   std::string string;
// //   string.resize(4);
// //   result = crc32c::Crc32c(string);

// //   // If you have C++17 support, process a std::string_view.
// //   std::string_view string_view(string);
// //   result = crc32c::Crc32c(string_view);

// //   return 0;
// // }

// #include <iostream>
// #include <fstream>
// #include <vector>
// #include <string>
// #include <iomanip>
// #include "crc32c/crc32c.h"  // from google/crc32c

// // Helper to print CRC in hex
// void print_crc(const std::string& label, uint32_t crc) {
//     std::cout << std::left << std::setw(25) << label << " : 0x" << std::hex << std::uppercase << crc << std::dec << std::endl;
// }

// // Read file into byte vector
// std::vector<uint8_t> read_file(const std::string& path) {
//     std::ifstream file(path, std::ios::binary);
//     if (!file)
//         throw std::runtime_error("Cannot open file: " + path);

//     return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
// }

// int main() {
//     try {
//         // 1️⃣ Basic CRC32C computation
//         std::string text = "The quick brown fox jumps over the lazy dog";
//         uint32_t crc1 = crc32c::Crc32c(reinterpret_cast<const uint8_t*>(text.data()), text.size());
//         print_crc("CRC of text", crc1);

//         // 2️⃣ Incremental update using Extend()
//         std::string part1 = "The quick brown fox ";
//         std::string part2 = "jumps over the lazy dog";
//         uint32_t crc_part1 = crc32c::Crc32c(reinterpret_cast<const uint8_t*>(part1.data()), part1.size());
//         uint32_t crc_extended = crc32c::Extend(crc_part1, reinterpret_cast<const uint8_t*>(part2.data()), part2.size());
//         print_crc("CRC via Extend()", crc_extended);
//         std::cout << "Same as single-pass: " << (crc_extended == crc1 ? "✅ Yes" : "❌ No") << "\n\n";

//         // 5️⃣ Compute CRC of a file
//         std::string filename = "C:\\Users\\AUTO\\Desktop\\out.bin";
//         // std::string filename = "test.bin";
//         std::ofstream(filename, std::ios::binary) << "CRC32C file content demo";
//         auto file_data = read_file(filename);
//         uint32_t file_crc = crc32c::Crc32c(file_data.data(), file_data.size());
//         print_crc("CRC of file", file_crc);

//         // 6️⃣ Incremental file computation simulation (block by block)
//         uint32_t rolling_crc = 0;
//         size_t block_size = 8;
//         for (size_t i = 0; i < file_data.size(); i += block_size) {
//             size_t len = std::min(block_size, file_data.size() - i);
//             rolling_crc = crc32c::Extend(rolling_crc, file_data.data() + i, len);
//         }
//         print_crc("Rolling CRC", rolling_crc);
//         std::cout << "Matches full file CRC? " << (rolling_crc == file_crc ? "✅ Yes" : "❌ No") << "\n\n";

//         std::cout << "All operations completed successfully!\n";

//     } catch (const std::exception& ex) {
//         std::cerr << "❌ Error: " << ex.what() << std::endl;
//         return 1;
//     }

//     return 0;
// }

// #include <iostream>
// #include <chrono>
// #include <vector>
// #include <random>
// #include <cstring>
// #include "crc32c/crc32c.h"

// using namespace std;
// using namespace std::chrono;

// int main() {
//     cout << "=== Google CRC32C Benchmark ===" << endl;

//     // --- Parameters ---
//     const size_t data_size = 100 * 1024 * 1024; // 100 MB
//     const int iterations = 10;                  // number of repetitions

//     cout << "Buffer size : " << (data_size / (1024.0 * 1024.0)) << " MB" << endl;
//     cout << "Iterations  : " << iterations << endl;

//     // --- Generate random data ---
//     vector<uint8_t> data(data_size);
//     mt19937_64 rng(12345);
//     uniform_int_distribution<unsigned short> dist(0, 255);
//     for (size_t i = 0; i < data_size; ++i)
//         data[i] = dist(rng);

//     // --- Warm up (just to initialize CPU caches, branch predictors, etc.) ---
//     crc32c::Crc32c(data.data(), data.size());

//     // --- Benchmark ---
//     double total_seconds = 0.0;
//     uint32_t checksum = 0;

//     for (int i = 0; i < iterations; ++i) {
//         auto start = high_resolution_clock::now();
//         checksum = crc32c::Crc32c(data.data(), data.size());
//         auto end = high_resolution_clock::now();

//         double seconds = duration<double>(end - start).count();
//         total_seconds += seconds;

//         cout << "Iteration " << (i + 1) << ": "
//              << (data_size / (1024.0 * 1024.0)) / seconds
//              << " MB/s" << endl;
//     }

//     double avg_speed = (data_size / (1024.0 * 1024.0)) / (total_seconds / iterations);
//     cout << "\nAverage speed: " << avg_speed << " MB/s" << endl;

//     // --- Print checksum result ---
//     cout << "\nLast computed CRC32C: 0x" << hex << uppercase << checksum << dec << endl;

//     // --- Verify known test vector ---
//     const char* test_str = "123456789";
//     uint32_t test_crc = crc32c::Crc32c(reinterpret_cast<const uint8_t*>(test_str), strlen(test_str));
//     uint32_t expected_crc = 0xE3069283; // standard CRC32C for "123456789"

//     cout << "\nTest vector ('123456789') :" << endl;
//     cout << "Computed : 0x" << hex << uppercase << test_crc << endl;
//     cout << "Expected : 0x" << hex << uppercase << expected_crc << endl;
//     cout << "Validation: " << (test_crc == expected_crc ? "✅ PASS" : "❌ FAIL") << endl;

//     return test_crc == expected_crc ? 0 : 1;
// }



#include <crc32c/crc32c.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <numeric>

using namespace std;

// Generate a random buffer of given size
vector<uint8_t> generate_random_buffer(size_t size) {
    vector<uint8_t> data(size);
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 255);
    for (auto &byte : data) byte = static_cast<uint8_t>(dis(gen));
    return data;
}

// Single-threaded benchmark
double benchmark_single(const vector<uint8_t> &data, size_t iterations) {
    auto start = chrono::high_resolution_clock::now();
    uint32_t checksum = 0;
    for (size_t i = 0; i < iterations; ++i) {
        checksum ^= crc32c::Crc32c(data.data(), data.size());
    }
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed = end - start;
    return elapsed.count();
}

// Multi-threaded benchmark
double benchmark_multi(const vector<uint8_t> &data, size_t iterations, unsigned num_threads) {
    auto worker = [&](size_t iters, uint32_t &out) {
        uint32_t local_crc = 0;
        for (size_t i = 0; i < iters; ++i) {
            local_crc ^= crc32c::Crc32c(data.data(), data.size());
        }
        out = local_crc;
    };

    vector<thread> threads;
    vector<uint32_t> results(num_threads, 0);
    auto start = chrono::high_resolution_clock::now();
    for (unsigned t = 0; t < num_threads; ++t)
        threads.emplace_back(worker, iterations / num_threads, ref(results[t]));
    for (auto &t : threads)
        t.join();
    auto end = chrono::high_resolution_clock::now();

    chrono::duration<double> elapsed = end - start;
    return elapsed.count();
}

int main(int argc, char **argv) {
    // Default config
    vector<size_t> buffer_sizes = {1 << 10, 1 << 16, 1 << 20, 1 << 24}; // 1 KB, 64 KB, 1 MB, 16 MB
    size_t iterations = 1000;
    unsigned num_threads = thread::hardware_concurrency();

    cout << "google/crc32c benchmark\n";
    cout << "Threads available: " << num_threads << "\n";
    cout << "Iterations per test: " << iterations << "\n\n";

    for (auto size : buffer_sizes) {
        auto buffer = generate_random_buffer(size);

        double single_time = benchmark_single(buffer, iterations);
        double multi_time = benchmark_multi(buffer, iterations, num_threads);

        double data_processed_single = (double)size * iterations / (1 << 20) / single_time; // MB/s
        double data_processed_multi = (double)size * iterations / (1 << 20) / multi_time;

        cout << "Buffer size: " << size / 1024 << " KB\n";
        cout << "  Single-thread: " << data_processed_single << " MB/s (" << single_time << " s)\n";
        cout << "  Multi-thread (" << num_threads << " threads): " << data_processed_multi << " MB/s (" << multi_time << " s)\n\n";
    }

    return 0;
}
