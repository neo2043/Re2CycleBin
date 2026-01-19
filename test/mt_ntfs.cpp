#include <Windows.h>
#include <errhandlingapi.h>
#include <iostream>
#include <memory>
#include <filesystem>

// #include "NTFS/ntfs_explorer.hpp"
#include "Utils/error.hpp"
#include "Utils/options.hpp"
#include "Drive/disk.hpp"
// #include "Utils/commands.hpp"
// #include "Utils/constant_names.hpp"
#include "mt_NTFS/mt_ntfs_mft_walker.hpp"
#include "mt_NTFS/mt_ntfs_reader.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);

    if (!utils::processes::elevated(GetCurrentProcess())) {
        std::cerr << "Administrator rights are required to read physical drives" << std::endl;
        return 1;
    }

    std::string root_path_str = (argc > 1) ? argv[1] : ".";
    fs::path root_path(fs::absolute(root_path_str));

    if (!fs::exists(root_path) || !fs::is_directory(root_path)) {
        std::cerr << "Invalid directory provided.\n";
        return 1;
    }

    std::ios_base::fmtflags flag_backup(std::cout.flags());

    auto i = get_disk(0);
	if (!i.has_value()) {
		std::cout << i.error();
    }
	std::shared_ptr<Disk> disk = i.value();
	std::shared_ptr<Volume> volume = disk->volumes(1);

    // std::shared_ptr<NTFSExplorer> explorer = std::make_shared<NTFSExplorer>(volume);

    // auto temp_record = commands::helpers::find_record(explorer, "D:\\all\\prog_elec_phil\\projects\\ReRecycleBin\\mytry\\rust\\one\\target\\debug\\build\\proc-macro2-18b24de3376e47e4", 0);
	// if (!temp_record.has_value()) {
	// 	win_error::print(temp_record.error().add_to_error_stack("Caller: Record Error", ERROR_LOC));
	// }
	// std::shared_ptr<MFTRecord> record = temp_record.value();

    std::shared_ptr<mt_ntfs_reader> reader = std::make_shared<mt_ntfs_reader>(utils::strings::from_string(volume->name()));

    std::shared_ptr<mt_ntfs_mft_walker> walker = std::make_shared<mt_ntfs_mft_walker>(reader);
    // walker->init_mft_record_datarun();

    // reader->print_sizes();
    // walk("path");

    // std::cout << "path: " << root_path.string() << std::endl;

    auto record = walker->record_from_path("D:\\all\\prog_elec_phil\\projects\\ReRecycleBin\\mytry\\rust\\one\\target\\debug\\build\\proc-macro2-18b24de3376e47e4");

    for (auto& i: record.value()->index_records().value()) {
        std::cout << "record: " << i << std::endl;
    }
    
    std::cout.flags(flag_backup);
}

// #include <iostream>
// #include <vector>
// #include <random>
// #include <thread>
// #include <atomic>

// #include "Drive/mt_reader.hpp"
// #include "Utils/error.hpp"


// constexpr uint32_t SECTOR_SIZE = 0x200;   // 512 bytes
// constexpr uint32_t READ_SIZE   = 0x200;   // read 1 sector
// constexpr int READ_LOOPS       = 50;

// uint64_t random_sector_offset(uint64_t max_sectors) {
//     static thread_local std::mt19937_64 rng{ std::random_device{}() };
//     std::uniform_int_distribution<uint64_t> dist(0, max_sectors - 1);
//     return dist(rng) * SECTOR_SIZE;
// }

// void reader_thread(mt_reader& reader, int thread_id) {
//     std::vector<std::uint8_t> buffer(READ_SIZE);

//     // Example limit: first 1 GB of disk
//     constexpr uint64_t MAX_SECTORS = (1ull << 30) / SECTOR_SIZE;

//     for (int i = 0; i < READ_LOOPS; ++i) {
//         uint64_t offset = random_sector_offset(MAX_SECTORS);

//         auto result = reader.read(offset, buffer.data(), READ_SIZE);
//         if (!result.has_value()) {
//             std::cerr << "[thread " << thread_id
//                       << "] read failed at offset 0x"
//                       << std::hex << offset << std::dec << "\n";
//             win_error::print(result.error());
//         } else {
//             std::cout << "[thread " << thread_id
//                       << "] read ok at offset 0x"
//                       << std::hex << offset << std::dec << "\n";
//         }
//     }
// }

// int main() {
//     // Example targets:
//     //   L"\\\\.\\PhysicalDrive0"
//     //   L"\\\\.\\C:"

//     mt_reader reader(L"\\\\.\\PhysicalDrive1");

//     std::vector<std::thread> threads;

//     threads.emplace_back(reader_thread, std::ref(reader), 0);
//     threads.emplace_back(reader_thread, std::ref(reader), 1);
//     threads.emplace_back(reader_thread, std::ref(reader), 2);

//     for (auto& t : threads) {
//         t.join();
//     }

//     return 0;
// }
