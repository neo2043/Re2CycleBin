#include "blake3.h"
#include "spdlog/common.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "thread_pool/thread_pool.h"
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"

#include <codecvt>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <mutex>
#include <iomanip>
#include <chrono> // Added for timing
#include <atomic> // Added for file counter

namespace fs = std::filesystem;

// Mutex to prevent garbled console output when multiple threads print simultaneously
std::mutex output_mutex;

// Atomic counter to track total files processed for stats
std::atomic<size_t> files_processed{0};
std::atomic<size_t> folders_processed{0};
std::atomic<size_t> bytes_processed{0};

thread_local blake3_hasher hasher;

std::string simple_file_hash(const fs::path& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return "ERROR";

    // Simple DJB2-style hash on file content for demonstration
    unsigned long hash = 5381;
    char buffer[4096];
    
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        for (std::streamsize i = 0; i < file.gcount(); ++i) {
            hash = ((hash << 5) + hash) + buffer[i]; /* hash * 33 + c */
        }
        bytes_processed += file.gcount();
    }
    
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

std::string blake3_File_hash(const fs::path& fp) {
    std::ifstream file(fp, std::ios::binary);
    if (!file.is_open()) return "ERROR";

    char buffer[4096];
    while(file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        std::streamsize bytes_read = file.gcount();
        if (bytes_read > 0) {
            blake3_hasher_update(
                &hasher,
                buffer,
                static_cast<size_t>(bytes_read)
            );
        }
    }

    uint8_t out[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
    blake3_hasher_reset(&hasher);

    std::stringstream ss;
    for (size_t i = 0; i < BLAKE3_OUT_LEN; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(out[i]);
    }
    // std::cout << "\n";

    return ss.str();
}

/**
 * The Consumer: Hashes the file
 */
void task_hash_file(const fs::path& filepath) {
    // 1. Perform the heavy work (Hashing)
    std::string hash = blake3_File_hash(filepath);
    // files_processed++;

    // auto logger = spdlog::get("app");
    // logger->debug("File: {}, Hash: {}, hasher_ptr: {}", filepath.filename().string(), hash.c_str(), fmt::ptr(&hasher));

    // 2. Print output (Must use critical section for I/O)
    // We only lock briefly to print, so we don't block the hashing itself
    // std::lock_guard<std::mutex> lock(output_mutex);
    // {
    //     std::cout << "[Hasher Thread " << std::this_thread::get_id() << "] " 
    //               << "File: " << filepath.filename() 
    //               << " | Hash: " << hash << "\n";
    // }
}

/**
 * @brief Task for the Walker Pool.
 * Scans a directory. Enqueues subdirectories back to Walker Pool, 
 * and files to Hasher Pool.
 */
void task_walk_directory(fs::path dir) {
// void task_walk_directory(fs::path dir, dp::thread_pool<>& walker_pool, dp::thread_pool<>& hasher_pool) {
// void task_walk_directory(fs::path dir, dp::thread_pool<>& walker_pool) {
    // auto dir_str = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(dir);

    // if (dir.string()[0] == '$' || dir.string() == "System Volume Information") {
    //     std::cout << dir << std::endl;
    //     return ;
    // }

    // if (dir_str[0] == '$' || dir_str == "System Volume Information") {
    //     std::cout << dir << std::endl;
    //     return ;
    // }

    // if (entry_str[0] == '$' || entry_str == "System Volume Information") {
    //     std::cout << dir << std::endl;
    //     continue;
    // }
    
    try {
        // std::cout << "path: " << dir << std::endl;
        // Iterate over the directory
        for (const std::filesystem::directory_entry& entry : fs::directory_iterator(dir)) {
            
            // if (auto dir_str = (std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(dir.filename()));
            // (!dir_str.empty() && dir_str[0]=='$') || (dir_str == "System Volume Information") || (dir_str == "System Volume Information")) {
            //     continue;
            // }

            auto entry_str = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(entry.path().filename());
            if ((!entry_str.empty() && entry_str[0]=='$') || (entry_str == "System Volume Information")) {
                continue;
            }

            // if ((entry.path().filename().string()[0] == '$') || entry.path().filename().string() == "System Volume Information") {
            //     std::cout << entry.path().string() << std::endl;
            //     continue;
            // }

            auto log = spdlog::get("app");
            log->trace("path: {}", std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(entry.path()));
            
            if (entry.is_directory() && !entry.is_symlink()) {
                // RECURSION STEP:
                // Enqueue the subdirectory to the WALKER pool.
                folders_processed++;
                task_walk_directory(entry.path());
                // walker_pool.enqueue_detach(task_walk_directory, entry.path(), std::ref(walker_pool));
            } 
            else if (entry.is_regular_file()) {
                files_processed++;
                // auto log = spdlog::get("app");
                // log->trace("path: {}", std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(entry.path()));
                // std::cout << "path: " << entry << std::endl;
                // PROCESSING STEP:
                // Enqueue the file to the HASHER pool.
                // hasher_pool.enqueue_detach(task_hash_file, entry.path());
            } else {
                std::cout << "else hit" << std::endl;
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cerr << "[Error] Accessing " << dir << ": " << e.what() << "\n";
        // auto logger = spdlog::get("app");
        // logger->error("Accessing {}: {}", dir.string(), e.what());
    }
}

int main(int argc, char** argv) {
    std::string root_path_str = (argc > 1) ? argv[1] : ".";
    fs::path root_path(fs::absolute(root_path_str));

    if (!fs::exists(root_path) || !fs::is_directory(root_path)) {
        std::cerr << "Invalid directory provided.\n";
        return 1;
    }

    spdlog::init_thread_pool(10000, 1);
    auto basic_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "logs/file_mt.log",
        // false
        true  // truncate on open
    );
    basic_file_sink->set_level(spdlog::level::trace);
    basic_file_sink->set_pattern("%v");
    // basic_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%-5l] [%-10n] [tid:%-5t] [%g:%#:%!] %v");

    std::vector<spdlog::sink_ptr> sinks{basic_file_sink};
    auto main_logger = std::make_shared<spdlog::async_logger>(
        "app",
        sinks.begin(), sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );
    main_logger->set_level(spdlog::level::trace);
    spdlog::register_logger(main_logger);
    spdlog::set_default_logger(main_logger);

    // 1. Setup Pools
    // dp::thread_pool walker_pool(4);
    // dp::thread_pool hasher_pool(
    // 12, 
    // [](uint64_t size){
    //     blake3_hasher_init(&hasher);
    // });

    std::cout << "Starting scan of: " << fs::absolute(root_path) << "\n";
    // std::cout << "Walker Threads: " << walker_pool.size() << "\n";
    // std::cout << "Hasher Threads: " << hasher_pool.size() << "\n\n";

    // --- START TIMER ---
    auto start_time = std::chrono::high_resolution_clock::now();

    // 2. Kick off the process
    task_walk_directory(root_path);
    // walker_pool.enqueue_detach(task_walk_directory, root_path, std::ref(walker_pool), std::ref(hasher_pool));
    // walker_pool.enqueue_detach(task_walk_directory, root_path, std::ref(walker_pool));

    // 3. Synchronization Logic
    // walker_pool.wait_for_tasks();
    // hasher_pool.wait_for_tasks();
    
    // --- END TIMER ---
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << "walker done" << std::endl;

    // 4. Calculate Duration
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    double duration_sec = duration_ms / 1000.0;

    std::cout << "\n------------------------------------------------\n";
    std::cout << "Traversal & Hashing Complete.\n";
    std::cout << "Total Files Processed: " << files_processed.load() << "\n";
    std::cout << "Total Folders Processed: " << folders_processed.load() << "\n";
    std::cout << "Total Time: " << duration_sec << " seconds (" << duration_ms << " ms)\n";
    
    if (duration_sec > 0) {
        std::cout << "Throughput: " << (files_processed.load() / duration_sec) << " files/sec\n";
    }
    std::cout << "------------------------------------------------\n";

    return 0;
}

// #include <thread_pool/thread_pool.h>
// #include <iostream>

// // Declare thread-local variable at namespace scope
// thread_local int tls_thread_id = -1;
// std::mutex output_mutex;

// int main() {
//     dp::thread_pool pool(4, [](std::size_t id) {
//         // Initialize thread-local variable during thread setup
//         tls_thread_id = static_cast<int>(id);
//         std::cout << "Thread " << id << " initialized TLS\n";
//     });
    
//     // Access the thread-local variable in tasks
//     for (int i=0; i<1000; i++) {
//         pool.enqueue_detach([]() {
//             std::lock_guard<std::mutex> lock(output_mutex);
//             std::cout << "Running on thread: " << tls_thread_id << "\n";
//         });
//     }
    
//     pool.wait_for_tasks();
//     return 0;
// }