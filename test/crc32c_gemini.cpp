#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>
#include <numeric>
#include <future>
#include "crc32c/crc32c.h"

// Function to format bytes into a more readable string.
std::string format_bytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
}

// --- Single-Threaded Benchmark ---
void run_single_threaded_benchmark(const std::vector<uint8_t>& data) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Perform the CRC32C calculation
    crc32c::Crc32c(data.data(), data.size());

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end_time - start_time;

    double throughput = (static_cast<double>(data.size()) / (1024 * 1024 * 1024)) / diff.count();

    std::cout << " | Single-Threaded: " << std::fixed << std::setprecision(2) << std::setw(8)
              << throughput << " GB/s";
}


// --- Multi-Threaded Benchmark ---
// Worker function for each thread
uint32_t crc32c_worker(const uint8_t* data, size_t size) {
    return crc32c::Crc32c(data, size);
}

void run_multi_threaded_benchmark(const std::vector<uint8_t>& data) {
    const unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0 || data.size() < 1024 * 1024) { // Don't use multithreading for small inputs
         std::cout << " | Multi-Threaded:  " << std::fixed << std::setprecision(2) << std::setw(8)
                  << "N/A" << " GB/s" << std::endl;
        return;
    }


    size_t chunk_size = data.size() / num_threads;
    std::vector<std::future<uint32_t>> futures;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Launch threads to compute CRC32C on chunks of data
    for (unsigned int i = 0; i < num_threads; ++i) {
        size_t offset = i * chunk_size;
        size_t current_chunk_size = (i == num_threads - 1) ? (data.size() - offset) : chunk_size;
        futures.push_back(std::async(std::launch::async, crc32c_worker, data.data() + offset, current_chunk_size));
    }

    // The crc32c::Extend function is used to chain the results together.
    // However, for a pure throughput benchmark, we only need to wait for all threads to finish.
    // The actual checksum value is not the focus of the benchmark, but the speed.
    uint32_t final_crc = 0;
    for(auto& f : futures) {
         uint32_t partial_crc = f.get(); 
        // We get the result to ensure the computation is finished.
        // In a real application, you would chain these results.
        final_crc = crc32c::Extend(final_crc, reinterpret_cast<const uint8_t*>(&final_crc), sizeof(final_crc));
        final_crc = crc32c::Extend(final_crc, reinterpret_cast<const uint8_t*>(&partial_crc), sizeof(partial_crc));
    }


    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end_time - start_time;

    double throughput = (static_cast<double>(data.size()) / (1024 * 1024 * 1024)) / diff.count();

    std::cout << " | Multi-Threaded:  " << std::fixed << std::setprecision(2) << std::setw(8)
              << throughput << " GB/s" << std::endl;
}


int main() {
    std::cout << "--- google/crc32c Benchmark ---" << std::endl;

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
        std::vector<uint8_t> data(size);
        for(size_t i = 0; i < size; ++i) {
            data[i] = dis(gen);
        }

        std::cout << "Input Size: " << std::setw(7) << format_bytes(size);

        run_single_threaded_benchmark(data);
        run_multi_threaded_benchmark(data);
    }

    return 0;
}