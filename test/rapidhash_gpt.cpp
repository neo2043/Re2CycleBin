#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <random>
#include <functional>
#include "rapidhash.h"

using namespace std;

using HashFunc = function<uint64_t(const void*, size_t)>;

// Generate random data buffer
void fill_random(vector<uint8_t>& data) {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<int> dist(0, 255);
    for (auto& b : data) b = dist(gen);
}

// Benchmark (single-threaded)
void benchmark_single(const string& name, HashFunc hash_fn, size_t data_size, size_t iterations) {
    vector<uint8_t> data(data_size);
    fill_random(data);

    uint64_t hash_accum = 0;
    auto start = chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        hash_accum ^= hash_fn(data.data(), data.size());
    }

    auto end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(end - start).count();
    double total_bytes = double(data_size) * iterations;
    double throughput_MB_s = total_bytes / (1024 * 1024) / elapsed;
    double ns_per_hash = (elapsed * 1e9) / iterations;

    cout << "Variant: " << name << " (Single-threaded)\n";
    cout << "  Data size: " << data_size << " bytes\n";
    cout << "  Iterations: " << iterations << "\n";
    cout << "  Throughput: " << fixed << setprecision(2) << throughput_MB_s << " MB/s\n";
    cout << "  Time per hash: " << fixed << setprecision(2) << ns_per_hash << " ns/hash\n";
    cout << "  Dummy hash accumulator: " << hash_accum << "\n\n";
}

// Benchmark (multi-threaded)
void benchmark_multi(const string& name, HashFunc hash_fn, size_t data_size, size_t iterations, size_t threads_count) {
    vector<thread> threads;
    atomic<uint64_t> hash_accum(0);

    auto start = chrono::high_resolution_clock::now();

    auto worker = [&](int tid) {
        vector<uint8_t> data(data_size);
        fill_random(data);
        uint64_t local_accum = 0;
        size_t per_thread_iters = iterations / threads_count;
        for (size_t i = 0; i < per_thread_iters; ++i) {
            local_accum ^= hash_fn(data.data(), data.size());
        }
        hash_accum.fetch_xor(local_accum, memory_order_relaxed);
    };

    for (size_t t = 0; t < threads_count; ++t)
        threads.emplace_back(worker, int(t));

    for (auto& th : threads)
        th.join();

    auto end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(end - start).count();
    double total_bytes = double(data_size) * iterations;
    double throughput_MB_s = total_bytes / (1024 * 1024) / elapsed;
    double ns_per_hash = (elapsed * 1e9) / iterations;

    cout << "Variant: " << name << " (" << threads_count << " threads)\n";
    cout << "  Data size: " << data_size << " bytes\n";
    cout << "  Iterations: " << iterations << "\n";
    cout << "  Throughput: " << fixed << setprecision(2) << throughput_MB_s << " MB/s\n";
    cout << "  Time per hash: " << fixed << setprecision(2) << ns_per_hash << " ns/hash\n";
    cout << "  Dummy hash accumulator: " << hash_accum.load() << "\n\n";
}

int main(int argc, char** argv) {
    size_t data_size = 1024;       // default 1 KB
    size_t iterations = 100000;    // default 100k hashes
    size_t threads_count = thread::hardware_concurrency();

    if (argc > 1) data_size = stoul(argv[1]);
    if (argc > 2) iterations = stoul(argv[2]);
    if (argc > 3) threads_count = stoul(argv[3]);

    cout << "==== RapidHash Benchmark ====\n";
    cout << "Data size per hash: " << data_size << " bytes\n";
    cout << "Iterations: " << iterations << "\n";
    cout << "Detected hardware threads: " << thread::hardware_concurrency() << "\n";
    cout << "Using " << threads_count << " threads for multi-thread test.\n\n";

    // Hash variants
    vector<pair<string, HashFunc>> variants = {
        {"rapidhash",       [](const void* d, size_t n){ return rapidhash(d, n); }},
        {"rapidhashMicro",  [](const void* d, size_t n){ return rapidhashMicro(d, n); }},
        {"rapidhashNano",   [](const void* d, size_t n){ return rapidhashNano(d, n); }}
    };

    for (auto& [name, fn] : variants) {
        benchmark_single(name, fn, data_size, iterations);
        benchmark_multi(name, fn, data_size, iterations, threads_count);
    }

    return 0;
}
