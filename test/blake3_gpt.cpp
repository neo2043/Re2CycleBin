// // // #include "blake3.h"

// // // int main() {

// // //     uint8_t hash[BLAKE3_OUT_LEN];
// // //     blake3_hasher hasher;

// // //     blake3_hasher_init(&hasher);
// // //     blake3_hasher_update(&hasher, "Hello, world!", 13);
// // //     blake3_hasher_finalize(&hasher, hash, BLAKE3_OUT_LEN);

// // // }

// // blake3_benchmark_and_stress_test.cpp
// // Cross-platform C++17 benchmark + stress test for BLAKE3 (supports MSVC and GCC/Clang)
// //
// // Usage (Linux/macOS):
// //   g++ -O3 -march=native -std=c++17 blake3_benchmark_and_stress_test.cpp blake3.c -pthread -o b3bench
// //
// // Usage (Windows / MSVC):
// //   cl /O2 /std:c++17 blake3_benchmark_and_stress_test.cpp blake3.c /Fe:b3bench.exe
// //
// // Example runs:
// //   b3bench --mode benchmark --threads 4 --size 1048576 --iterations 200
// //   b3bench --mode stress --threads 8 --duration 30

// #include <cstring>
// #include <cstdint>
// #include <iostream>
// #include <vector>
// #include <thread>
// #include <atomic>
// #include <random>
// #include <chrono>
// #include <iomanip>

// #ifdef _WIN32
// #include <windows.h>
// // #include <getopt.h> // You can use GNU getopt for Windows or replace with simple arg parsing
// #else
// #include <getopt.h>
// #endif

// extern "C" {
// #include "blake3.h"
// }

// using namespace std;
// using ns = chrono::nanoseconds;
// using steady_clock = chrono::steady_clock;

// struct Options {
//     string mode = "benchmark"; // or "stress"
//     size_t size = 100 * 1024 * 1024; // bytes per message
//     size_t iterations = 200; // per-thread iterations
//     unsigned threads = thread::hardware_concurrency();
//     unsigned duration = 10; // seconds for stress mode
//     size_t chunk = 65536; // chunk size for streaming
//     bool verify = true;
// };

// static void print_usage(const char* prog) {
//     cerr << "Usage: " << prog << " [--mode benchmark|stress] [--threads N] [--size BYTES] [--iterations N] [--duration S] [--chunk BYTES] [--no-verify]\n";
// }

// static double mb_per_sec(double bytes, double secs) {
//     return (bytes / (1024.0 * 1024.0)) / secs;
// }

// static void blake3_hash_oneshot(const uint8_t* data, size_t len, uint8_t out[BLAKE3_OUT_LEN]) {
//     blake3_hasher hasher;
//     blake3_hasher_init(&hasher);
//     blake3_hasher_update(&hasher, data, len);
//     blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
// }

// static void blake3_hash_streaming(const uint8_t* data, size_t len, size_t chunk, uint8_t out[BLAKE3_OUT_LEN]) {
//     blake3_hasher hasher;
//     blake3_hasher_init(&hasher);
//     size_t pos = 0;
//     while (pos < len) {
//         size_t n = min(chunk, len - pos);
//         blake3_hasher_update(&hasher, data + pos, n);
//         pos += n;
//     }
//     blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
// }

// static void benchmark_worker(const uint8_t* buf, size_t len, size_t iterations, atomic<uint64_t>& bytes_hashed, atomic<uint64_t>& iterations_done,
// atomic<uint64_t>& cycles) {
//     uint8_t out[BLAKE3_OUT_LEN];
//     for (size_t i = 0; i < iterations; ++i) {
//         auto t0 = steady_clock::now();
//         blake3_hash_oneshot(buf, len, out);
//         auto t1 = steady_clock::now();
//         bytes_hashed += len;
//         iterations_done++;
//         cycles += chrono::duration_cast<ns>(t1 - t0).count();
//     }
// }

// static void stress_worker(const uint8_t* buf, size_t len, size_t chunk, atomic<bool>& stop_flag, atomic<uint64_t>& ops, atomic<uint64_t>& errors, bool
// verify) {
//     std::mt19937_64 rng(steady_clock::now().time_since_epoch().count() ^ (uintptr_t)buf);
//     std::uniform_int_distribution<uint32_t> dist32(0);

//     vector<uint8_t> local(buf, buf + len);
//     uint8_t out1[BLAKE3_OUT_LEN];
//     uint8_t out2[BLAKE3_OUT_LEN];

//     while (!stop_flag.load(std::memory_order_relaxed)) {
//         if ((dist32(rng) & 0xFF) == 0) {
//             size_t idx = dist32(rng) % len;
//             local[idx] = (uint8_t)dist32(rng);
//         }

//         blake3_hash_oneshot(local.data(), len, out1);
//         ops.fetch_add(1, std::memory_order_relaxed);

//         if (verify && ((dist32(rng) & 0x1F) == 0)) {
//             blake3_hash_streaming(local.data(), len, chunk, out2);
//             if (memcmp(out1, out2, BLAKE3_OUT_LEN) != 0) {
//                 errors.fetch_add(1, std::memory_order_relaxed);
//                 cerr << "[stress] mismatch detected!\n";
//             }
//         }
//     }
// }

// int main(int argc, char** argv) {
//     Options opt;

//     cerr << "BLAKE3 benchmark/stress test\n";
//     cerr << "Mode: " << opt.mode << ", threads: " << opt.threads << ", size: " << opt.size << " bytes\n";

//     vector<uint8_t> buf(opt.size);
//     std::mt19937_64 rng(1234567);
//     for (size_t i = 0; i < opt.size; ++i) buf[i] = (uint8_t)(rng() & 0xFF);

//     if (opt.mode == "benchmark") {
//         atomic<uint64_t> bytes_hashed{0};
//         atomic<uint64_t> iterations_done{0};
//         atomic<uint64_t> cycles{0};

//         vector<thread> workers;
//         for (unsigned i = 0; i < opt.threads; ++i) {
//             workers.emplace_back(benchmark_worker, buf.data(), buf.size(), opt.iterations, ref(bytes_hashed), ref(iterations_done), ref(cycles));
//         }

//         auto start = steady_clock::now();
//         for (auto &t : workers) t.join();
//         auto end = steady_clock::now();

//         double secs = chrono::duration_cast<chrono::duration<double>>(end - start).count();
//         uint64_t total_iters = iterations_done.load();
//         uint64_t total_bytes = bytes_hashed.load();
//         double avg_ns = (double)cycles.load() / (double)total_iters;

//         cout << fixed << setprecision(3);
//         cout << "Total time: " << secs << " s\n";
//         cout << "Total iterations: " << total_iters << "\n";
//         cout << "Total bytes hashed: " << total_bytes << "\n";
//         cout << "Throughput: " << mb_per_sec((double)total_bytes, secs) << " MB/s\n";
//         cout << "Avg per-hash latency: " << (avg_ns / 1e6) << " ms\n";

//     } else if (opt.mode == "stress") {
//         atomic<bool> stop_flag{false};
//         atomic<uint64_t> ops{0};
//         atomic<uint64_t> errors{0};

//         vector<thread> workers;
//         for (unsigned i = 0; i < opt.threads; ++i) {
//             workers.emplace_back(stress_worker, buf.data(), buf.size(), opt.chunk, ref(stop_flag), ref(ops), ref(errors), opt.verify);
//         }

//         for (unsigned s = 0; s < opt.duration; ++s) {
// #ifdef _WIN32
//             Sleep(1000);
// #else
//             this_thread::sleep_for(chrono::seconds(1));
// #endif
//             uint64_t current_ops = ops.load();
//             cerr << "[stress] elapsed " << (s+1) << "s, ops=" << current_ops << ", errors=" << errors.load() << "\n";
//         }

//         stop_flag.store(true);
//         for (auto &t : workers) t.join();

//         cout << "Stress test finished. total ops=" << ops.load() << ", errors=" << errors.load() << "\n";
//     } else {
//         cerr << "Unknown mode: " << opt.mode << "\n";
//         print_usage(argv[0]);
//         return 1;
//     }

//     return 0;
// }

// #include <atomic>
// #include <chrono>
// #include <cstring>
// #include <iomanip>
// #include <iostream>
// #include <random>
// #include <thread>
// #include <vector>

// extern "C" {
// #include "blake3.h"
// }

// #ifdef _WIN32
// #include <windows.h>
// #else
// #include <unistd.h>
// #endif

// using namespace std;
// using namespace std::chrono;

// struct Result {
//     size_t size_bytes;
//     double secs;
//     double mbps;
// };

// void fill_random(vector<uint8_t> &buf) {
//     std::mt19937_64 rng(0xBEEF);
//     for (auto &b : buf)
//         b = static_cast<uint8_t>(rng());
// }

// static inline void blake3_hash_oneshot(const uint8_t *data, size_t len, uint8_t out[BLAKE3_OUT_LEN]) {
//     blake3_hasher hasher;
//     blake3_hasher_init(&hasher);
//     blake3_hasher_update(&hasher, data, len);
//     blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
// }

// Result benchmark_once(size_t buf_size, unsigned thread_count = 1) {
//     vector<uint8_t> buf(buf_size);
//     fill_random(buf);

//     atomic<uint64_t> total_bytes = 0;
    
//     auto worker = [&]() {
//         uint8_t out[BLAKE3_OUT_LEN];
//         auto t0 = steady_clock::now();
//         uint64_t bytes = 0;
//         while (duration<double>(steady_clock::now() - t0).count() < 20) {
//             blake3_hash_oneshot(buf.data(), buf.size(), out);
//             bytes += buf.size();
//         }
//         total_bytes.fetch_add(bytes, memory_order_relaxed);
//     };
//     auto start = steady_clock::now();

//     vector<thread> threads;
//     for (unsigned i = 0; i < thread_count; i++)
//         threads.emplace_back(worker);
//     for (auto &t : threads)
//         t.join();

//     auto end = steady_clock::now();
//     double secs = duration<double>(end - start).count();
//     double mbps = (double)total_bytes / (1024.0 * 1024.0) / secs;
//     return {buf_size, secs, mbps};
// }

// int main(int argc, char **argv) {
//     unsigned threads = 1;
//     if (argc >= 2) threads = max(1, atoi(argv[1]));

//     vector<size_t> sizes = {
//         1ull * 1024,
//         16ull * 1024,
//         64ull * 1024,
//         1ull * 1024 * 1024,
//         16ull * 1024 * 1024, 
//         64ull * 1024 * 1024
//     };

//     cout << "BLAKE3 variable-size benchmark (" << threads << " thread" << (threads > 1 ? "s" : "") << ")\n";
//     cout << left << setw(12) << "Size" << setw(12) << "Time(s)" << setw(16) << "Throughput(MB/s)" << "\n";
//     cout << string(40, '-') << "\n";

//     for (auto sz : sizes) {
//         Result r = benchmark_once(sz, threads);
//         cout << setw(10) << (r.size_bytes / 1024) << " KB" << setw(12) << fixed << setprecision(3) << r.secs << setw(16) << fixed << setprecision(2) << r.mbps
//              << "\n";
//     }
// }


#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <blake3.h>

constexpr size_t CHUNK_SIZE = 1 << 16; // 64 KB

int main(int argc, char** argv) {
    // if (argc < 2) {
    //     std::cerr << "Usage: " << argv[0] << " <file>\n";
    //     return 1;
    // }

    const char* filename = "C:/Users/AUTO/Desktop/temp.bin";

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: unable to open file: " << filename << "\n";
        return 1;
    }

    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    std::vector<uint8_t> buffer(CHUNK_SIZE);

    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        std::streamsize bytes_read = file.gcount();
        if (bytes_read > 0) {
            blake3_hasher_update(&hasher, buffer.data(),
                                 static_cast<size_t>(bytes_read));
        }
    }

    uint8_t out[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);

    // print as hex
    for (size_t i = 0; i < BLAKE3_OUT_LEN; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(out[i]);
    }
    std::cout << "\n";

    return 0;
}
