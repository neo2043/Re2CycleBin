#include <Windows.h>
// #include <codecvt>
// #include <cstdint>
#include <codecvt>
#include <cstdint>
#include <errhandlingapi.h>
#include <ios>
#include <iostream>
#include <memory>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>

// #include "Utils/buffer.hpp"
#include "Utils/error.hpp"
#include "Utils/options.hpp"
#include "Drive/disk.hpp"
// #include "Utils/constant_names.hpp"
#include "Utils/utils.hpp"
#include "blake3.h"
#include "commands/backup/container.hpp"
#include "mt_NTFS/mt_ntfs_mft_record.hpp"
#include "mt_NTFS/mt_ntfs_mft_walker.hpp"
#include "mt_NTFS/mt_ntfs_reader.hpp"
#include "thread_pool/thread_pool.h"

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/cfg/env.h"
#include "spdlog/stopwatch.h"

namespace fs = std::filesystem;

thread_local blake3_hasher hasher;
std::mutex output_mutex;
std::atomic<size_t> files_processed{0};
std::atomic<size_t> folders_processed{0};
std::atomic<size_t> bytes_processed{0};

sqlite_filetable* sql_ft = nullptr;

bool flag = false;
bool flag1 = false;

bool is_subpath_lexical(fs::path child, fs::path parent)
{
    child  = fs::absolute(child).lexically_normal();
    parent = fs::absolute(parent).lexically_normal();

    auto cit = child.begin();
    auto pit = parent.begin();

    for (; pit != parent.end(); ++pit, ++cit) {
        if (cit == child.end() || *cit != *pit)
            return false;
    }

    return true;
}

// void task_walk_fs(fs::path path, std::shared_ptr<mt_ntfs_mft_record> child_record, std::shared_ptr<mt_ntfs_mft_walker> walker_ctx) {
// void task_walk_fs(std::shared_ptr<mt_ntfs_mft_record> child_record, std::shared_ptr<mt_ntfs_mft_walker> walker_ctx, dp::thread_pool<>& walker_pool) {
// void task_walk_fs(uint64_t inode, std::shared_ptr<mt_ntfs_mft_walker> walker_ctx, dp::thread_pool<>& walker_pool, dp::thread_pool<>& hasher_pool) {
void task_walk_fs(fs::path path, uint64_t parent_record_id, uint64_t record_id, std::shared_ptr<mt_ntfs_mft_record> record, std::shared_ptr<mt_ntfs_mft_walker> walker_ctx) {
    auto temp_filename = record->filename();
    if (!temp_filename.has_value()) {
        win_error::print(temp_filename.error().add_to_error_stack("Caller: filename error", ERROR_LOC));
    }
    std::wstring filename = temp_filename.value_or(L"-error-");
    if (flag) {
        path = path / std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(filename);
        auto log = spdlog::get("app");
        log->trace("path: {}", path.string());
    }
    flag = true;

    auto out = sql_ft->sqlite_filetable_insert(record_id, utils::strings::to_utf8(filename), parent_record_id, 1);
    if (!out.has_value()) {
        std::wcout << "filename: " << filename << ", record id: " << record_id << " parent record id: " << parent_record_id << std::endl;
        win_error::print(out.error().add_to_error_stack("Caller: filename error", ERROR_LOC));
        exit(1);
    }
    
    auto temp_child_rec_ids = record->index_records();
    if (!temp_child_rec_ids.has_value()) {
        win_error::print(temp_child_rec_ids.error().add_to_error_stack(std::string("Caller: Index Error, Record ID: "+std::to_string(record->header()->MFTRecordIndex)), ERROR_LOC));
        return;
    }
    std::set<DWORD64> index = temp_child_rec_ids.value();

    // if (is_subpath_lexical(path, "D:\\Games\\Cyberpunk 2077\\bin\\x64")) {
    //     flag1 = true;
    //     for (auto &i : index) {
    //         auto temp_name_record = walker_ctx->record_from_number(i);
    //         if (!temp_name_record.has_value()) {
    //             win_error::print(temp_name_record.error().add_to_error_stack("Caller: name record error", ERROR_LOC));
    //         }
    //         auto name_record = temp_name_record.value();

    //         auto temp_filename = name_record->filename().value_or(L"-error-");
    //         std::wcout << "record: " << i << ",  subpath/filename: " << temp_filename << std::endl;
    //     }
    // }
    
    // std::set<DWORD64> win32_named_entries;
    // for (auto i : index) {
    //     if (i->name_type() != 2) {
    //         win32_named_entries.insert(i->record_number());
    //     }
    // }
            
    for (auto& entry : index) {
        // if ((entry->name_type() == 2) && (win32_named_entries.find(entry->record_number()) != win32_named_entries.end())) continue;
        // auto temp_record = walker_ctx->record_from_number(entry->record_number());

        auto temp_record = walker_ctx->record_from_number(entry);
        if (!temp_record.has_value()) {
            win_error::print(temp_record.error().add_to_error_stack("Caller: Record Error", ERROR_LOC));
            continue;
        }
        std::shared_ptr<mt_ntfs_mft_record> entry_rec = temp_record.value();
        
        auto temp_filename = entry_rec->filename();
        if (!temp_filename.has_value()) {
            win_error::print(temp_filename.error().add_to_error_stack("Caller: filename error", ERROR_LOC));
        }
        std::wstring filename = temp_filename.value_or(L"-error-");

        if (filename[0] == L'$' || !(filename.compare(L".")) || !(filename.compare(L"System Volume Information"))) {
            std::wcout << "$ or . hit     " << filename << std::endl;
            continue;
        }
                
        // if (entry_rec->is_valid()) {
            if (entry_rec->header()->flag & MFT_RECORD_IS_DIRECTORY) {
                folders_processed++;

                task_walk_fs(path, record_id, entry, entry_rec, walker_ctx);
                // walker_pool.enqueue_detach(task_walk_fs, entry->record_number(), walker_ctx);
                // walker_pool.enqueue_detach(task_walk_fs, entry_rec, walker_ctx, std::ref(walker_pool));
                // walker_pool.enqueue_detach(task_walk_fs, entry->record_number(), walker_ctx, std::ref(walker_pool), std::ref(hasher_pool));
                // ret.push_back(std::make_tuple(entry->name(), entry->record_number()));
            } else if (((entry_rec->header()->flag & MFT_RECORD_IS_DIRECTORY) == 0)) {
                files_processed++;
                // std::wcout << "filename: " << filename << ". is a file" << std::endl;

                auto temp_filename = entry_rec->filename();
                if (!temp_filename.has_value()) {
                    win_error::print(temp_filename.error().add_to_error_stack("Caller: filename error", ERROR_LOC));
                }
                std::wstring filename = temp_filename.value_or(L"-error-");

                auto out = sql_ft->sqlite_filetable_insert(entry, utils::strings::to_utf8(filename), record_id, 0);
                if (!out.has_value()) {
                    std::wcout << "filename: " << filename << ", record id: " << entry << " parent record id: " << record_id << std::endl;
                    win_error::print(out.error().add_to_error_stack("Caller: filename error", ERROR_LOC));
                    exit(1);
                }
                
                auto log = spdlog::get("app");
                fs::path temp3_path = path / std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(filename);
                log->trace("path: {}", temp3_path.string());
            }
        // }
    }
}

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);

    if (!utils::processes::elevated(GetCurrentProcess())) {
        std::cerr << "Administrator rights are required to read physical drives" << std::endl;
        return 1;
    }

    std::ios_base::fmtflags flag_backup(std::cout.flags());

    spdlog::init_thread_pool(10000, 1);
    auto basic_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "logs/file_mt_ntfs_walker.log",
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

    // main_logger->trace("started");
    // SPDLOG_TRACE("started");

    std::string root_path_str = (argc > 1) ? argv[1] : ".";
    fs::path root_path(fs::absolute(root_path_str));

    if (!fs::exists(root_path) || !fs::is_directory(root_path)) {
        std::cerr << "Invalid directory provided.\n";
        return 1;
    }

    auto sql_comm = sqlite_commander::open("./db/db.sqlite");
    if (!sql_comm.has_value()) {
        win_error::print(sql_comm.error().add_to_error_stack("Caller: sqlite open error", ERROR_LOC));
        return 1;
    }
    auto commander = std::move(sql_comm.value());
    auto j = commander->init(Init_State::backup);
    if (!j.has_value()) {
        win_error::print(j.error().add_to_error_stack("Caller: sqlite open error", ERROR_LOC));
        return 1;
    }

    sql_ft = commander->filetable_push_back_return().value();
    // auto sql_dt = commander->dataruntable_push_back_return().value();
    
    auto i = get_disk(0);
	if (!i.has_value()) {
        std::cout << i.error();
    }
	std::shared_ptr<Disk> disk = i.value();
	std::shared_ptr<Volume> volume = disk->volumes(1);
    
    std::shared_ptr<mt_ntfs_reader> reader = std::make_shared<mt_ntfs_reader>(utils::strings::from_string(volume->name()));
    
    std::shared_ptr<mt_ntfs_mft_walker> walker = std::make_shared<mt_ntfs_mft_walker>(reader);
    auto temp_root_record = walker->record_from_path(root_path.string());
    if (!temp_root_record.has_value()) {
        win_error::print(temp_root_record.error().add_to_error_stack("caller: root record not found", ERROR_LOC));
        return 1;
    }
    auto record = temp_root_record.value();

    std::cout << "record id: " << record->header()->MFTRecordIndex << ". valid: " << std::boolalpha << record->is_valid() << std::endl;

    // dp::thread_pool walker_pool(12);
    // dp::thread_pool hasher_pool(
    // 12, [](uint64_t size){
    //     blake3_hasher_init(&hasher);
    // });

    std::cout << "Starting scan of: " << fs::absolute(root_path) << "\n";
    // std::cout << "Walker Threads: " << walker_pool.size() << "\n";
    // std::cout << "Hasher Threads: " << hasher_pool.size() << "\n\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    
    task_walk_fs(root_path, -1, 5, record, walker);
    // walker_pool.enqueue_detach(task_walk_fs, record, walker, std::ref(walker_pool));
    // walker_pool.enqueue_detach(task_walk_fs, record->header()->MFTRecordIndex, walker, std::ref(walker_pool), std::ref(hasher_pool));
    
    // walker_pool.wait_for_tasks();
    // hasher_pool.wait_for_tasks();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    std::cout << "walker done" << std::endl;

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
    
    std::cout.flags(flag_backup);
}