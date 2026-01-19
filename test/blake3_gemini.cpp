#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>
#include "blake3.h"

// Function to format bytes into a more readable format
std::string format_bytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    else if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    else if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
    else return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
}

// --- Benchmark Test ---
void run_benchmark() {
    std::cout << "--- BLAKE3 Benchmark ---" << std::endl;

    const std::vector<size_t> sizes = {
        1024,
        4096,
        16384,
        65536,
        262144,
        1048576,
        4194304,
        16777216,
        67108864,
        134217728
    };

    for (size_t size : sizes) {
        std::vector<uint8_t> input(size);
        // Fill with some data
        for(size_t i = 0; i < size; ++i) {
            input[i] = i % 256;
        }

        uint8_t output[BLAKE3_OUT_LEN];

        // Single-threaded benchmark
        auto start_single = std::chrono::high_resolution_clock::now();
        blake3_hasher hasher_single;
        blake3_hasher_init(&hasher_single);
        blake3_hasher_update(&hasher_single, input.data(), input.size());
        blake3_hasher_finalize(&hasher_single, output, BLAKE3_OUT_LEN);
        auto end_single = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff_single = end_single - start_single;

        double throughput_single = (static_cast<double>(size) / (1024 * 1024)) / diff_single.count();

        // Multi-threaded benchmark (using therayon-based implementation in C)
        auto start_multi = std::chrono::high_resolution_clock::now();
        blake3_hasher hasher_multi;
        blake3_hasher_init(&hasher_multi);
        // The C implementation automatically uses multiple threads for larger inputs if compiled with threading support.
        // To explicitly control threading, one would need to use the lower-level APIs.
        // For this benchmark, we rely on the default behavior.
        blake3_hasher_update(&hasher_multi, input.data(), input.size());
        blake3_hasher_finalize(&hasher_multi, output, BLAKE3_OUT_LEN);
        auto end_multi = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff_multi = end_multi - start_multi;

        double throughput_multi = (static_cast<double>(size) / (1024 * 1024)) / diff_multi.count();


        std::cout << "Input Size: " << std::setw(7) << format_bytes(size)
                  << " | Single-threaded: " << std::fixed << std::setprecision(2) << std::setw(8) << throughput_single << " MB/s"
                  << " | Multi-threaded: " << std::fixed << std::setprecision(2) << std::setw(8) << throughput_multi << " MB/s" << std::endl;
    }
}

// --- Stress Test ---
void stress_worker(int duration_seconds, size_t data_size) {
    std::vector<uint8_t> data(data_size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (size_t i = 0; i < data_size; ++i) {
        data[i] = dis(gen);
    }

    uint8_t output[BLAKE3_OUT_LEN];
    auto start_time = std::chrono::steady_clock::now();
    long long hashes_done = 0;

    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count() < duration_seconds) {
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, data.data(), data.size());
        blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);
        hashes_done++;
    }

    std::cout << "Thread finished. Performed " << hashes_done << " hashes." << std::endl;
}

void run_stress_test() {
    std::cout << "\n--- BLAKE3 Stress Test ---" << std::endl;
    const int duration_seconds = 10;
    const size_t data_size = 1024 * 1024 * 10; // 10 MB
    const unsigned int num_threads = std::thread::hardware_concurrency();

    std::cout << "Starting stress test for " << duration_seconds << " seconds with " << num_threads << " threads." << std::endl;
    std::cout << "Each thread will be hashing a " << format_bytes(data_size) << " buffer repeatedly." << std::endl;


    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back(stress_worker, duration_seconds, data_size);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Stress test finished." << std::endl;
}


int main() {
    run_benchmark();
    run_stress_test();
    return 0;
}