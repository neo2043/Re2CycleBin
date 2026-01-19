#include "NTFS/ntfs.hpp"
#include <codecvt>
#include <cstdlib>
#define NOMINMAX
#include <Windows.h>

#include <errhandlingapi.h>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <string> // Include for std::string
#include <cstdint>
#include <errhandlingapi.h>
#include <expected>
#include <iostream>
#include <memory>
#include <string> // Include for std::string
#include <thread>
#include <filesystem>
#include <vector>
#include <fstream>
// #include <omp.h>

// Assuming these are your custom headers and are correct
#include "Utils/error.hpp" 
#include "Drive/disk.hpp"
#include "Utils/commands.hpp"
#include "mt_NTFS/mt_ntfs_mft_record.hpp"
#include "Utils/utils.hpp"
#include "Utils/options.hpp"
#include "blake3.h"
#include "commands/info/disk_info.cpp"
#include "argparse/argparse.hpp"
#include "blake3.h"
#include "BS_thread_pool.hpp"
#include "commands/backup/container.hpp"

#include "spdlog/common.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"


thread_local blake3_hasher hasher;
thread_local sqlite_filetable* sqlite_ft = nullptr;
thread_local sqlite_dataruntable* sqlite_dt = nullptr;

std::atomic<uint64_t> files_processed{0};
// std::atomic<uint64_t> files_reached{0};
std::atomic<uint64_t> folders_processed{0};
// std::atomic<uint64_t> folders_reached{0};
std::atomic<uint64_t> bytes_processed{0};
std::string output_path;
bool verbose = false;

// std::string blake3_File_hash(const std::filesystem::path& fp) {
//     std::ifstream file(fp, std::ios::binary);
//     if (!file.is_open()) return "ERROR";

//     char buffer[4096];
//     while(file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
//         std::streamsize bytes_read = file.gcount();
//         if (bytes_read > 0) {
//             blake3_hasher_update(
//                 &hasher,
//                 buffer,
//                 static_cast<size_t>(bytes_read)
//             );
//         }
//     }

//     uint8_t out[BLAKE3_OUT_LEN];
//     blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
//     blake3_hasher_reset(&hasher);

//     std::stringstream ss;
//     for (size_t i = 0; i < BLAKE3_OUT_LEN; i++) {
//         ss << std::hex << std::setw(2) << std::setfill('0')
//                   << static_cast<int>(out[i]);
//     }

//     return ss.str();
// }

void task_backup_hash_fs(PBYTE buffer_ptr, uint64_t buffer_length) {
    // char buffer[1024];
    // uint64_t offset = 0;
    // while (offset < buffer_length) {
    // blake3_hasher_update(&hasher, buffer_ptr, buffer_length);

    // }
}

void task_backup_walk_fs(std::shared_ptr<mt_ntfs_mft_record> child_record, std::shared_ptr<mt_ntfs_mft_walker> walker_ctx) {
    // sqlite_ft->sqlite_filetable_insert(
    //     int64_t record_id,
    //     const std::string &name,
    //     int64_t parent_id,
    //     uint8_t is_dir
    // );
    auto temp_child_rec_ids = child_record->index_records();
    if (!temp_child_rec_ids.has_value()) {
        win_error::print(temp_child_rec_ids.error().add_to_error_stack("Caller: Index Error", ERROR_LOC));
        return;
    }
    std::set<DWORD64> index = temp_child_rec_ids.value();
            
    for (auto& entry : index) {
        // auto log = spdlog::get("app");

        auto temp_record = walker_ctx->record_from_number(entry);
        if (!temp_record.has_value()) {
            win_error::print(temp_record.error().add_to_error_stack("Caller: Record Error", ERROR_LOC));
            continue;
        }
        std::shared_ptr<mt_ntfs_mft_record> entry_record = temp_record.value();

        auto temp_entry_record_filename = entry_record->filename();
        if (!temp_entry_record_filename.has_value()) {
            win_error::print(temp_entry_record_filename.error().add_to_error_stack("Caller: filename error", ERROR_LOC));
        }
        std::wstring entry_record_filename = temp_entry_record_filename.value();

        if (entry_record_filename[0] == L'$' || !(entry_record_filename.compare(L".")) || !(entry_record_filename.compare(L"System Volume Information"))) {
            std::wcout << "$ or . hit     " << entry_record_filename << std::endl;
            continue;
        }

        // log->trace("file name: {}", std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(entry_record_filename));
        // log->trace("normal file, record id: {}, file name: {}", entry, std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(entry_record_filename));

        // if (entry_record->is_valid()) {
            if (entry_record->header()->flag & MFT_RECORD_IS_DIRECTORY) {
                // folders_reached++;
                const std::optional<void*> temp_pool = BS::this_thread::get_pool();
                
                auto pool = static_cast<BS::priority_thread_pool*>(*temp_pool);
                pool->detach_task([entry_record, walker_ctx](){
                    task_backup_walk_fs(entry_record, walker_ctx);
                }, BS::pr::highest);
                
                // walker_pool.enqueue_detach(task_walk_fs, entry_record, walker_ctx, std::ref(walker_pool));
                folders_processed++;
            } else if (((entry_record->header()->flag & MFT_RECORD_IS_DIRECTORY) == 0)) {
                // files_reached++;
                // auto temp_entry_record_data = entry_record->attribute_header($DATA);
                // if (!temp_entry_record_data.has_value()) {
                //     win_error::print(temp_entry_record_data.error().add_to_error_stack("Caller: No Data Attribute Error", ERROR_LOC));
                //     return;
                // }
                // PMFT_RECORD_ATTRIBUTE_HEADER pAttributeData = temp_entry_record_data.value();
                // if (pAttributeData->Form.Resident.ValueOffset + pAttributeData->Form.Resident.ValueLength <= pAttributeData->RecordLength) {
                    
                //     // co_yield pAttributeData->Form.Resident.ValueLength;
                // } else {
                //     // co_yield std::unexpected(win_error("Invalid size of resident data",ERROR_LOC));
                //     // co_return;
                // }
                files_processed++;
            }
        // }
    }
}

// void mt_mft_walker(uint64_t inode) {

// }

// std::expected<void, win_error> mt_backup_handler(std::string brp, uint64_t inode, uint16_t threadNos, std::shared_ptr<Volume> volume) {
//     // std::vector<std::shared_ptr<NTFSExplorer>> vec_ntfsexplorer;
//     // for(uint16_t i = 0; i < threadNos; i++) {
//     //     vec_ntfsexplorer[i] = std::make_shared<NTFSExplorer>(volume);
//     // }
//     std::shared_ptr<NTFSExplorer> ntfsexplorer = std::make_shared<NTFSExplorer>(volume);

//     dp::thread_pool walker_pool(3);
//     dp::thread_pool hasher_pool(threadNos);

//     auto root_mft_record_temp = commands::helpers::find_record(ntfsexplorer, brp, inode);
//     if (!root_mft_record_temp.has_value()) {
//         return std::unexpected(root_mft_record_temp.error().add_to_error_stack("Caller: root mft record not found", ERROR_LOC));
//     }
//     std::shared_ptr<MFTRecord> root_mft_record = root_mft_record_temp.value();

//     // walker_pool.enqueue_detach(mt_backup_handler);
// }

int main(int argc, char *argv[]) {
    if (!utils::processes::elevated(GetCurrentProcess())) {
        std::cerr << "Administrator rights are required to read physical drives" << std::endl;
        return 1;
    }

    spdlog::init_thread_pool(10000, 1);
    auto basic_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "logs/file_main.log",
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
    
    SetConsoleOutputCP(CP_UTF8);
    std::ios_base::fmtflags flag_backup(std::cout.flags());

    argparse::ArgumentParser program("re2", "0.0.1");
    
    argparse::ArgumentParser info("info", "0.0.1", argparse::default_arguments::help);
    info.add_argument("-d","--disk")
        .help("Disk Number.")
        .scan<'u', uint64_t>();
    info.add_argument("-v","--volume")
        .help("Volume Number.")
        .scan<'u', uint64_t>();
    
    argparse::ArgumentParser backup("backup", "0.0.1", argparse::default_arguments::help);
    backup.add_argument("-o", "--output-file")
        // .default_value(std::string("./backupdb.re2"))
        .help("Output File Path. Default is './backupdb.re2'")
        .required();
    backup.add_argument("-d", "--disk")
        .scan<'u', uint16_t>()
        .help("Disk Number.")
        .required();
    backup.add_argument("-v","--volume")
        .scan<'u', uint16_t>()
        .help("Volume Number.")
        .required();
    backup.add_argument("-j", "--threads")
        .default_value(static_cast<uint16_t>(std::thread::hardware_concurrency()))
        .scan<'u', uint16_t>()
        .help("Number of Threads to use. Default is your logical cores.");
    backup.add_argument("-V", "--verbose")
        .help("Verbose flag.")
        .flag();

    auto &group = backup.add_mutually_exclusive_group(true);
    group.add_argument("-p","--backup-root-path")
        .help("Root path to backup.");
        // .required();
    group.add_argument("-i", "--inode")
        // .default_value(static_cast<uint64_t>(5))
        .scan<'u', uint64_t>()
        .help("MFT Record inode. Default is 5 (base of the volume)");

    program.add_subparser(info);
    program.add_subparser(backup);
    program.set_assign_chars(":=");

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    if (program.is_subcommand_used(info)) {
        if (!info.is_used("-d") && !info.is_used("-v")) {
            auto i = core::win::disks::list();
            if (!i.has_value()) {
                win_error::print(i.error().add_to_error_stack("Caller: disk list error", ERROR_LOC));
                std::exit(1);
            }
            print_disks(core::win::disks::list().value());
        } else if (info.is_used("-d") && !info.is_used("-v")) {
            auto i = get_disk(static_cast<uint32_t>(info.get<uint64_t>("-d")));
            if (!i.has_value()) {
                win_error::print(i.error().add_to_error_stack("Caller: disk volume list error", ERROR_LOC));
                std::exit(1);
            }
            print_disk_volumes(i.value());
        } else if (!info.is_used("-d") && info.is_used("-v")) {
            std::cerr << "Error: Argument '-v, --volume' requires '-d, --disk' to be present." << std::endl << std::endl;
            std::cerr << info << std::endl;
            std::exit(1);
        } else if (info.is_used("-d") && info.is_used("-v")) {
            auto i = get_disk(static_cast<uint32_t>(info.get<uint64_t>("-d")));
            if (!i.has_value()) {
                win_error::print(i.error().add_to_error_stack("Caller: disk volume info error", ERROR_LOC));
            }
            std::shared_ptr<Disk> disk = i.value();
            std::shared_ptr<Volume> volume = disk->volumes(static_cast<uint32_t>(info.get<uint64_t>("-v")));
            print_volume_info(disk, volume);
        }
    } else if (program.is_subcommand_used(backup)) {

        output_path = backup.get<std::string>("-o");
        uint64_t diskNo = backup.get<uint16_t>("-d");
        uint64_t volumeNo = backup.get<uint16_t>("-v");
        uint16_t threadNos = backup.get<uint16_t>("-j");
        verbose = backup.get<bool>("-V");
        auto brp = backup.present<std::string>("-p");
        auto inode = backup.present<uint64_t>("-i");

        std::cout << "output path:        " << output_path << std::endl;
        std::cout << "disk number:        " << diskNo << std::endl;
        std::cout << "volume number:      " << volumeNo << std::endl;
        std::cout << "thread count used:  " << std::boolalpha << backup.is_used("-j") << std::endl;
        std::cout << "thread count:       " << threadNos << std::endl;
        std::cout << "verbose used:       " << std::boolalpha << backup.is_used("-V") << std::endl;
        std::cout << "verbose:            " << std::boolalpha << verbose << std::endl;
        std::cout << "backup root used:   " << std::boolalpha << backup.is_used("-p") << std::endl;
        std::cout << "backup root path:   " << brp.value_or("no brp provided") << std::endl;
        std::cout << "inode used:         " << std::boolalpha << backup.is_used("-i") << std::endl;
        std::cout << "inode:              " << inode.transform([](uint64_t val){return std::to_string(val);}).value_or("no inode provided") << std::endl;

        output_path = utils::strings::to_utf8(std::filesystem::absolute(output_path));
        std::cout << "sqlite file created in " << output_path << std::endl;
        auto i = sqlite_commander::open(output_path);
        // auto i = sqlite_commander::open(":memory:");
        if (!i.has_value()) {
            win_error::print(i.error().add_to_error_stack("Caller: sqlite open error", ERROR_LOC));
        }
        auto commander = std::move(i.value());

        auto temp_comm = commander->init(Init_State::backup);
        if (!temp_comm.has_value()) {
            win_error::print(temp_comm.error().add_to_error_stack("Caller: commader innit error", ERROR_LOC));
            exit(1);
        }

        auto temp_disk = get_disk(diskNo);
        if (!temp_disk.has_value()) {
            win_error::print(temp_disk.error().add_to_error_stack("Caller: get disk error", ERROR_LOC));
            std::exit(1);
        }
        std::shared_ptr<Disk> disk = temp_disk.value();
        std::shared_ptr<Volume> volume = disk->volumes(volumeNo);
        if (!commands::helpers::is_ntfs(volume)) {
            std::cerr << "Error: The selected volume does not use NTFS filesystem." << std::endl << std::endl;
            std::exit(1);
        }

        std::shared_ptr<mt_ntfs_reader> reader = std::make_shared<mt_ntfs_reader>(utils::strings::from_string(volume->name()));
    
        std::shared_ptr<mt_ntfs_mft_walker> walker = std::make_shared<mt_ntfs_mft_walker>(reader);
        
        BS::priority_thread_pool pool(threadNos, [&](){
            blake3_hasher_init(&hasher);
            
            auto temp_sqlite_ft = commander->filetable_push_back_return();
            if (!temp_sqlite_ft.has_value()) {
                win_error::print(temp_sqlite_ft.error().add_to_error_stack("Caller: sqlite ft error", ERROR_LOC));
                exit(1);
            }
            sqlite_ft = temp_sqlite_ft.value();
            
            auto temp_sqlite_dt = commander->dataruntable_push_back_return();
            if (!temp_sqlite_dt.has_value()) {
                win_error::print(temp_sqlite_dt.error().add_to_error_stack("Caller: sqlite dt error", ERROR_LOC));
                exit(1);
            }
            sqlite_dt = temp_sqlite_dt.value();
        });

        std::shared_ptr<mt_ntfs_mft_record> record = nullptr;
        if (backup.is_used("-p")) {
            auto temp_root_record = walker->record_from_path(std::filesystem::absolute(brp.value()).string());
            if (!temp_root_record.has_value()) {
                win_error::print(temp_root_record.error().add_to_error_stack("caller: root record not found", ERROR_LOC));
                return 1;
            }
            record = temp_root_record.value();
        } else if (backup.is_used("-i")) {
            auto temp_root_record = walker->record_from_number(inode.value());
            if (!temp_root_record.has_value()) {
                win_error::print(temp_root_record.error().add_to_error_stack("caller: root record not found", ERROR_LOC));
                return 1;
            }
            record = temp_root_record.value();
        }

        std::cout << "record no." << record->header()->MFTRecordIndex << std::endl;

        std::cout << "Starting scan of: " << (backup.is_used("-p") ? std::filesystem::absolute(brp.value()) : inode.transform([](int val){return std::to_string(val);}).value()) << "\n";

        auto start_time = std::chrono::high_resolution_clock::now();

        pool.detach_task([record, walker](){
            task_backup_walk_fs(record, walker);
        }, BS::pr::highest);

        pool.wait();

        auto end_time = std::chrono::high_resolution_clock::now();

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

    // --- Step 1: Get the original record via the standard path ---
    // auto disk_result = get_disk(1);
    // if (!disk_result.has_value()) {
    //     win_error::print(disk_result.error());
    //     return 1;
    // }
    // std::shared_ptr<Disk> disk = disk_result.value();
    // std::shared_ptr<Volume> vol = disk->volumes(3);
    
    // if (!commands::helpers::is_ntfs(vol)) return 1;
    
    // // std::string from_file = "C:\\Users\\AUTO\\Desktop\\inside_linux_win_path.bin";
    // std::string from_file = "C:/Users/AUTO/Desktop/temp.bin";
    // // std::string from_file = "C:\\Users\\AUTO\\Downloads\\Clair Obscur - Expedition 33 [FitGirl Repack]\\fg-01.bin";
    
    // std::shared_ptr<NTFSExplorer> explorer = std::make_shared<NTFSExplorer>(vol);
    // auto temp_record = commands::helpers::find_record(explorer, from_file, 0);
    // if (!temp_record.has_value()) {
    //     win_error::print(temp_record.error().add_to_error_stack("Caller: Original record not found", ERROR_LOC));
    //     return 1;
    // }
    // auto original_record = temp_record.value();

    // auto _reader = original_record->get_reader();
    // auto c_size = _reader->sizes.cluster_size;
    // blake3_hasher hash;
    // blake3_hasher_init(&hash);

    // for (auto i: original_record->_process_raw_pointers_with_hash(hash)) {
    //     if (!i.has_value()) {
    //         win_error::print(i.error().add_to_error_stack("Caller: error", ERROR_LOC));
    //         return 1;
    //     }
    //     // auto run = i.value();
    //     // if (std::holds_alternative<std::tuple<uint64_t, uint64_t, std::array<uint8_t, 32>>>(run)) {
    //     //     auto y = get<std::tuple<uint64_t, uint64_t, std::array<uint8_t, 32>>>(run);
    //     //     std::cout << "offset: " << get<0>(y) << " size: " << get<1>(y) << " hash: ";
    //     //     for (uint8_t b : get<2>(y)) {
    //     //         std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    //     //     }
    //     //     std::cout << std::dec << std::endl; // reset formatting
    //     // } else if (std::holds_alternative<flags>(run)) {
    //     //     auto j = get<flags>(run);
    //     //     std::cout << "[+] flags: " << original_record->print_flags(j) << std::endl;
    //     // }
    // }

    // std::array<uint8_t, BLAKE3_OUT_LEN> hash_out{};

    // blake3_hasher_finalize(&hash, hash_out.data(), hash_out.size());

    // std::cout << std::endl << std::endl;
    // for (uint8_t b : hash_out) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    // }
    // std::cout << std::dec << std::endl; // reset formatting
    std::cout.flags(flag_backup);
}