#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <omp.h>

// Use C++17 filesystem
namespace fs = std::filesystem;

// Global atomics for statistics
std::atomic<long> files_processed{0};
std::atomic<long> directories_visited{0};
std::atomic<long> bytes_processed{0};

/**
 * A simulated hash function. 
 * In a real app, replace this with SHA256 (OpenSSL) or MD5.
 * We simulate CPU work here to make the parallelism obvious.
 */
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

/**
 * The Consumer: Hashes the file
 */
void task_hash_file(const fs::path& filepath) {
    // 1. Perform the heavy work (Hashing)
    std::string hash = simple_file_hash(filepath);
    files_processed++;

    // 2. Print output (Must use critical section for I/O)
    // We only lock briefly to print, so we don't block the hashing itself
    // #pragma omp critical(output_lock)
    // {
    //     std::cout << "[Hasher Thread " << omp_get_thread_num() << "] " 
    //               << "File: " << filepath.filename() 
    //               << " | Hash: " << hash << std::endl;
    // }
}

/**
 * The Producer/Walker: Scans folders and creates tasks
 */
void task_traverse_dir(const fs::path& dir) {
    directories_visited++;
    
    try {
        // Iterate through the directory
        for (const auto& entry : fs::directory_iterator(dir)) {
            
            // 1. Found a Subdirectory? -> Add to Walker Queue
            if (entry.is_directory() && !entry.is_symlink()) {
                
                // Capture 'path' by value (firstprivate) is crucial here!
                fs::path sub_path = entry.path();

                #pragma omp task firstprivate(sub_path)
                {
                    // This task might be picked up by this thread 
                    // or another thread later
                    task_traverse_dir(sub_path);
                }
            } 
            // 2. Found a File? -> Add to Hash Queue
            else if (entry.is_regular_file()) {
                
                fs::path file_path = entry.path();

                // We create a task for hashing. 
                // The 'walker' thread doesn't wait; it keeps walking.
                #pragma omp task firstprivate(file_path)
                {
                    task_hash_file(file_path);
                }
            }
        }
    } catch (const std::exception& e) {
        #pragma omp critical(error_lock)
        std::cerr << "Error accessing " << dir << ": " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Check 1: Is OpenMP enabled in the compiler?
    #if defined(_OPENMP)
        std::cout << "OpenMP Enabled. Version: " << _OPENMP << std::endl;
    #else
        std::cout << "WARNING: OpenMP NOT enabled. Directives ignored." << std::endl;
    #endif

    // Check 2: How many threads are available?
    std::cout << "Max Threads Available: " << omp_get_max_threads() << std::endl;

    // ... rest of your code ...

    std::string root_path_str = (argc > 1) ? argv[1] : ".";
    fs::path root = root_path_str;

    if (!fs::exists(root) || !fs::is_directory(root)) {
        std::cerr << "Invalid directory." << std::endl;
        return 1;
    }

    std::cout << "Starting Parallel Hasher on: " << fs::absolute(root) << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    double start = omp_get_wtime();

    // Start the thread pool
    #pragma omp parallel
    {
        // Only one thread starts the traversal tree
        #pragma omp single
        {
            // This is the root task that will spawn all other tasks
            task_traverse_dir(root);
        }
        // Implicit Barrier: All threads stay here until 
        // every single task created above is finished.
    }

    double end = omp_get_wtime();

    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "Done." << std::endl;
    std::cout << "Directories Walked: " << directories_visited << std::endl;
    std::cout << "Files Hashed:       " << files_processed << std::endl;
    std::cout << "Total Data Read:    " << (bytes_processed / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "Time Elapsed:       " << (end - start) << " seconds" << std::endl;

    return 0;
}