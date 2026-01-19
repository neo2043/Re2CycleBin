#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <future>
#include <random>
#include <iomanip>
#include <numeric>
#include <functional>

// Include the rapidhash header file.
// Make sure rapidhash.h is in the same directory or your include path.
#include "rapidhash.h"

// Function to format bytes into a more readable string.
std::string format_bytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
}

// A type alias for the hash function signature to make the code cleaner.
using HashFunction = std::function<uint64_t(const void*, size_t)>;

// --- Single-Threaded Benchmark ---
void run_single_threaded_benchmark(const std::vector<uint8_t>& data) {
    // We will benchmark all three variants of rapidhash.
    const std::vector<std::pair<std::string, HashFunction>> hash_variants = {
        {"rapidhash     ", rapidhash},
        {"rapidhashMicro", rapidhashMicro},
        {"rapidhashNano ", rapidhashNano}
    };

    std::cout << "\n--- Single-Threaded ---" << std::endl;
    for (const auto& variant : hash_variants) {
        auto start_time = std::chrono::high_resolution_clock::now();
        // Use volatile to prevent the compiler from optimizing out the hash call.
        volatile uint64_t hash_result = variant.second(data.data(), data.size());
        (void)hash_result; // Suppress unused variable warning
        auto end_time = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double> diff = end_time - start_time;
        double throughput = (static_cast<double>(data.size()) / (1024 * 1024 * 1024)) / diff.count();

        std::cout << variant.first << ": " << std::fixed << std::setprecision(2) << std::setw(8)
                  << throughput << " GB/s" << std::endl;
    }
}

// --- Multi-Threaded Benchmark ---
// Worker function for each thread
uint64_t hash_worker(HashFunction hash_func, const uint8_t* data, size_t size) {
    return hash_func(data, size);
}

void run_multi_threaded_benchmark(const std::vector<uint8_t>& data) {
    const unsigned int num_threads = std::thread::hardware_concurrency();
    
    std::cout << "\n--- Multi-Threaded (" << num_threads << " threads) ---" << std::endl;
    
    // Don't use multithreading for small inputs where the overhead isn't worth it.
    if (num_threads < 2 || data.size() < (256 * 1024)) {
        std::cout << "Buffer size is too small for meaningful multi-threading. Skipping." << std::endl;
        return;
    }

    const std::vector<std::pair<std::string, HashFunction>> hash_variants = {
        {"rapidhash     ", rapidhash},
        {"rapidhashMicro", rapidhashMicro},
        {"rapidhashNano ", rapidhashNano}
    };

    for (const auto& variant : hash_variants) {
        size_t chunk_size = data.size() / num_threads;
        std::vector<std::future<uint64_t>> futures;

        auto start_time = std::chrono::high_resolution_clock::now();

        // Launch threads to compute hashes on chunks of data
        for (unsigned int i = 0; i < num_threads; ++i) {
            size_t offset = i * chunk_size;
            size_t current_chunk_size = (i == num_threads - 1) ? (data.size() - offset) : chunk_size;
            futures.push_back(std::async(std::launch::async, hash_worker, variant.second, data.data() + offset, current_chunk_size));
        }

        // Collect the partial hashes from each thread
        std::vector<uint64_t> partial_hashes;
        partial_hashes.reserve(num_threads);
        for(auto& f : futures) {
            partial_hashes.push_back(f.get());
        }

        // Combine the partial hashes by hashing them together to get a final, single result.
        volatile uint64_t final_hash = variant.second(partial_hashes.data(), partial_hashes.size() * sizeof(uint64_t));
        (void)final_hash; // Suppress unused variable warning

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end_time - start_time;
        double throughput = (static_cast<double>(data.size()) / (1024 * 1024 * 1024)) / diff.count();

        std::cout << variant.first << ": " << std::fixed << std::setprecision(2) << std::setw(8)
                  << throughput << " GB/s" << std::endl;
    }
}


int main() {
    std::cout << "--- Nicoshev/rapidhash Benchmark ---" << std::endl;

    const std::vector<size_t> buffer_sizes = {
        1024,        // 1 KB
        4096,        // 4 KB
        16384,       // 16 KB
        65536,       // 64 KB
        262144,      // 256 KB
        1048576,     // 1 MB
        4194304,     // 4 MB
        16777216,    // 16 MB
        67108864,    // 64 MB
        134217728    // 128 MB
    };

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (size_t size : buffer_sizes) {
        std::cout << "\n=============================================" << std::endl;
        std::cout << "Benchmarking with Input Size: " << format_bytes(size) << std::endl;
        
        std::vector<uint8_t> data(size);
        for(size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(dis(gen));
        }

        run_single_threaded_benchmark(data);
        run_multi_threaded_benchmark(data);
    }
    std::cout << "\n=============================================" << std::endl;

    return 0;
}