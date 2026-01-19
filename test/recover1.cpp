#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <errhandlingapi.h>
#include <expected>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <string> // Include for std::string
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

// Assuming these are your custom headers and are correct
#include "Compression/lznt1.hpp"
#include "Drive/disk.hpp"
#include "NTFS/ntfs_explorer.hpp" // Include necessary headers
#include "NTFS/ntfs_mft_record.hpp"
#include "NTFS/ntfs_reader.hpp"
#include "Utils/buffer.hpp"
#include "Utils/commands.hpp"
#include "Utils/error.hpp"
#include "Utils/options.hpp"
#include "Utils/utils.hpp"
#include "blake3.h"
#include "cppcoro/generator.hpp"

// struct ReadOperation {
//     ULONG64 physical_disk_offset;
//     DWORD bytes_to_read;
// };

// cppcoro::generator<ReadOperation> _process_read_operations(
//     std::shared_ptr<MFTRecord> orig_record,
//     std::string stream_name
// ) {
//     auto size_result = orig_record->datasize(stream_name, true);
//     if (!size_result.has_value() || size_result.value() == 0) {
//         co_return;
//     }
//     ULONG64 bytes_remaining = size_result.value();
//     auto dataruns = orig_record->_process_raw_pointers(stream_name);
//     auto _reader = orig_record->get_reader();
//     for (const auto& run : dataruns) {
//         if (std::holds_alternative<std::pair<uint64_t, uint64_t>>(run)) {
//             if (bytes_remaining == 0) {
//                 break;
//             }
//             auto irun = get<std::pair<uint64_t, uint64_t>>(run);
//             if (irun.first == 0) {
//                 ULONG64 sparse_size = irun.second * _reader->sizes.cluster_size;
//                 bytes_remaining -= min(bytes_remaining, sparse_size);
//                 continue;
//             }
//             ULONG64 run_start_physical_offset = irun.first * _reader->sizes.cluster_size;
//             ULONG64 run_total_bytes = irun.second * _reader->sizes.cluster_size;
//             DWORD bytes_to_read_from_this_run = static_cast<DWORD>(min(run_total_bytes, bytes_remaining));
//             ReadOperation op;
//             op.physical_disk_offset = run_start_physical_offset;
//             op.bytes_to_read = bytes_to_read_from_this_run;
//             co_yield op;
//             bytes_remaining -= bytes_to_read_from_this_run;
//         }
//     }
// }

cppcoro::generator<std::expected<uint64_t, win_error>> _post_raw_pointer_datasize(
    std::vector<std::variant<std::pair<uint64_t, uint64_t>, flags>> datapoints,
    uint64_t final_size,
    uint32_t cluster_size,
    std::string stream_name,
    std::shared_ptr<NTFSReader> _reader,
    MFT* mft
) {
    flags outer_flag;
    for (const auto& run : datapoints) {
        if (std::holds_alternative<std::pair<uint64_t, uint64_t>>(run)) {
            auto irun = get<std::pair<uint64_t, uint64_t>>(run);
            if (outer_flag.is_Resident) {
                std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> buffer = std::make_shared<Buffer<PMFT_RECORD_HEADER>>(irun.second);

                auto temp_seek = _reader->seek(irun.first);
                if (!temp_seek.has_value()) {
                    co_yield std::unexpected(temp_seek.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
                    co_return;
                }

                auto temp_read = _reader->read(buffer->address(), irun.second);
                if (!temp_read.has_value()) {
                    co_yield std::unexpected(temp_read.error().add_to_error_stack("Caller: Read Error", ERROR_LOC));
                    co_return;
                }

                std::shared_ptr<MFTRecord> rec = std::make_shared<MFTRecord>(buffer->data(), mft, _reader, irun.first);

                if (!rec->is_valid()) {
                    co_yield std::unexpected(win_error("Caller: Invalid MFT", ERROR_LOC));
                    co_return;
                }

                auto temp_data = rec->attribute_header($DATA);
                if (!temp_data.has_value()) {
                    co_yield std::unexpected(temp_data.error().add_to_error_stack("Caller: No Data Attribute Error", ERROR_LOC));
                    co_return ;
                }
                PMFT_RECORD_ATTRIBUTE_HEADER pAttributeData = temp_data.value();

                if (pAttributeData->FormCode == RESIDENT_FORM) {
                    if (pAttributeData->Form.Resident.ValueOffset + pAttributeData->Form.Resident.ValueLength <= pAttributeData->RecordLength) {
                        co_yield pAttributeData->Form.Resident.ValueLength;
                    } else {
                        co_yield std::unexpected(win_error("Invalid size of resident data",ERROR_LOC));
                        co_return;
                    }
                }else {
                    co_yield std::unexpected(win_error("Data not resident",ERROR_LOC));
                    co_return;
                }
            } else if (outer_flag.is_NonResident) {
                if (!outer_flag.is_Compressed) {
                    if (final_size == 0) {
                        break;
                    }
                    if (irun.first == 0) {
                        ULONG64 sparse_size = irun.second * cluster_size;
                        final_size -= min(final_size, sparse_size);
                        continue;
                    }
                    ULONG64 run_total_bytes = irun.second * cluster_size;
                    DWORD bytes_to_read_from_this_run = static_cast<DWORD>(min(run_total_bytes, final_size));

                    co_yield bytes_to_read_from_this_run;
                    final_size -= bytes_to_read_from_this_run;
                } else {
                    co_yield std::unexpected(win_error("Compressed data detected",ERROR_LOC));
                    co_return;
                }
            }
        } else if (std::holds_alternative<flags>(run)) {
            outer_flag = get<flags>(run);
        }
    }
}

cppcoro::generator<std::expected<std::pair<PBYTE, uint64_t>, win_error>> _post_raw_pointers_process_raw_data(
    std::vector<std::variant<std::pair<uint64_t, uint64_t>, flags>> datapoints,
    uint64_t original_file_size,
    uint32_t cluster_size, std::shared_ptr<NTFSReader> _reader,
    MFT *mft, std::string stream_name,
    uint32_t block_size,
    bool skip_sparse
) {
    flags outer_flag;
    bool err = false;
    uint64_t last_offset = 0;
    uint64_t writeSize = 0;
    uint64_t fixed_blocksize = 0;
    for (const auto &run : datapoints) {
        if (std::holds_alternative<std::pair<uint64_t, uint64_t>>(run)) {
            auto irun = get<std::pair<uint64_t, uint64_t>>(run);
            if (outer_flag.is_Resident) {
                std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> buffer = std::make_shared<Buffer<PMFT_RECORD_HEADER>>(irun.second);

                auto temp_seek = _reader->seek(irun.first);
                if (!temp_seek.has_value()) {
                    co_yield std::unexpected(temp_seek.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
                    co_return;
                }

                auto temp_read = _reader->read(buffer->address(), irun.second);
                if (!temp_read.has_value()) {
                    co_yield std::unexpected(temp_read.error().add_to_error_stack("Caller: Read Error", ERROR_LOC));
                    co_return;
                }

                std::shared_ptr<MFTRecord> rec = std::make_shared<MFTRecord>(buffer->data(), mft, _reader, irun.first);

                if (!rec->is_valid()) {
                    co_yield std::unexpected(win_error("Caller: Invalid MFT", ERROR_LOC));
                    co_return;
                }

                auto temp_data = rec->attribute_header($DATA);
                if (!temp_data.has_value()) {
                    co_yield std::unexpected(temp_data.error().add_to_error_stack("Caller: No Data Attribute Error", ERROR_LOC));
                    co_return;
                }
                PMFT_RECORD_ATTRIBUTE_HEADER pAttributeData = temp_data.value();

                if (pAttributeData->FormCode == RESIDENT_FORM) {
                    if (pAttributeData->Form.Resident.ValueOffset + pAttributeData->Form.Resident.ValueLength <= pAttributeData->RecordLength) {
                        PBYTE data = POINTER_ADD(PBYTE, pAttributeData, pAttributeData->Form.Resident.ValueOffset);
                        for (DWORD offset = 0; offset < pAttributeData->Form.Resident.ValueLength; offset += block_size) {
                            co_yield std::pair<PBYTE, DWORD>(data + offset, min(block_size, pAttributeData->Form.Resident.ValueLength - offset));
                        }
                    } else {
                        co_yield std::unexpected(win_error("Invalid size of resident data", ERROR_LOC));
                        co_return;
                    }
                } else {
                    co_yield std::unexpected(win_error("Data not resident", ERROR_LOC));
                    co_return;
                }
            } else if (outer_flag.is_NonResident) {
                if (outer_flag.is_Compressed) {
                    auto expansion_factor = 0x10ULL;
                    if (err) break; //-V547

                    if (last_offset == irun.first) { // Padding run
                        continue;
                    }
                    last_offset = irun.first;

                    if (irun.first == 0) {
                        Buffer<PBYTE> buffer_decompressed(static_cast<DWORD>(block_size));

                        RtlZeroMemory(buffer_decompressed.data(), block_size);
                        DWORD64 total_size = irun.second * _reader->sizes.cluster_size;
                        for (DWORD64 i = 0; i < total_size; i += block_size) {
                            fixed_blocksize = DWORD(min(original_file_size - writeSize, block_size));
                            co_yield std::pair<PBYTE, DWORD>(buffer_decompressed.data(), static_cast<DWORD>(fixed_blocksize));
                            writeSize += fixed_blocksize;
                        }
                    } else {
                        auto temp_pos = _reader->seek(irun.first * cluster_size);
                        if (!temp_pos.has_value()) {
                            co_yield std::unexpected(temp_pos.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
                            co_return;
                        }
                        DWORD64 total_size = irun.first * cluster_size;
                        std::shared_ptr<Buffer<PBYTE>> buffer_compressed = std::make_shared<Buffer<PBYTE>>(static_cast<DWORD>(total_size));

                        auto temp_read = _reader->read(buffer_compressed->data(), static_cast<DWORD>(total_size));
                        if (!temp_read.has_value()) {
                            co_yield std::unexpected(temp_read.error().add_to_error_stack("Caller: ReadFile compressed failed", ERROR_LOC));
                            co_return;
                            err = true;
                            break;
                        }

                        if (irun.second > 0x10) { // Uncompressed
                            co_yield std::pair<PBYTE, DWORD>(buffer_compressed->data(), buffer_compressed->size());
                            writeSize += buffer_compressed->size();
                        } else {
                            std::shared_ptr<Buffer<PBYTE>> buffer_decompressed =
                                std::make_shared<Buffer<PBYTE>>(static_cast<DWORD>(total_size * expansion_factor));

                            DWORD final_size = 0;
                            int dec_status = decompress_lznt1(buffer_compressed, buffer_decompressed, &final_size);

                            if (!dec_status) {
                                co_yield std::pair<PBYTE, DWORD>(buffer_decompressed->data(), final_size);
                                writeSize += final_size;
                            } else {
                                break;
                            }
                        }
                    }
                } else {
                    Buffer<PBYTE> buffer(block_size);

                    if (err) break;

                    if (irun.first == 0) {
                        if (!skip_sparse) {
                            RtlZeroMemory(buffer.data(), block_size);
                            DWORD64 total_size = irun.second * _reader->sizes.cluster_size;
                            for (DWORD64 i = 0; i < total_size; i += block_size) {
                                fixed_blocksize = DWORD(min(original_file_size - writeSize, block_size));
                                co_yield std::pair<PBYTE, DWORD>(buffer.data(), static_cast<DWORD>(fixed_blocksize));
                                writeSize += fixed_blocksize;
                            }
                        }
                    } else {
                        auto temp_seek = _reader->seek(irun.first * cluster_size);
                        if (!temp_seek.has_value()) {
                            co_yield std::unexpected(temp_seek.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
                            co_return;
                        }
                        DWORD64 total_size = irun.second * cluster_size;
                        DWORD64 read_block_size = static_cast<DWORD>(min(block_size, total_size));
                        for (DWORD64 i = 0; i < total_size; i += read_block_size) {
                            auto temp_read = _reader->read(buffer.data(), static_cast<DWORD>(read_block_size));
                            if (!temp_read.has_value()) {
                                co_yield std::unexpected(temp_read.error().add_to_error_stack("Caller: Read Error", ERROR_LOC));
                                co_return;
                                err = true;
                                break;
                            }
                            read_block_size = min(read_block_size, total_size - i);
                            fixed_blocksize = read_block_size;
                            co_yield std::pair<PBYTE, DWORD>(buffer.data(), static_cast<DWORD>(fixed_blocksize));
                            writeSize += fixed_blocksize;
                        }
                    }
                }
            }
        } else if (std::holds_alternative<flags>(run)) {
            outer_flag = get<flags>(run);
        }
    }
}

cppcoro::generator<std::expected<std::pair<PBYTE, uint64_t>, win_error>> _post_raw_pointers_process_data(
    std::vector<std::variant<std::pair<uint64_t, uint64_t>, flags>> datapoints,
    uint64_t original_file_size,
    uint32_t cluster_size,
    std::shared_ptr<NTFSReader> _reader,
    MFT *mft,
    std::string stream_name,
    uint32_t block_size,
    bool skip_sparse
) {
    bool check_size = original_file_size != 0;
    for (auto &i : _post_raw_pointers_process_raw_data(datapoints, original_file_size, cluster_size, _reader, mft, stream_name, block_size, skip_sparse)) {
        if (!i.has_value()) {
            co_yield std::unexpected(i.error().add_to_error_stack("Caller: _post_raw_pointers_process_raw_data Error", ERROR_LOC));
            co_return;
        }
        auto block = i.value();
        if (block.second > original_file_size && check_size) { block.second = static_cast<DWORD>(original_file_size); }

        co_yield block;

        if (check_size) { original_file_size -= block.second; }
    }
}

std::expected<uint64_t, win_error> data_to_file(
    std::wstring dest_filename,
    std::vector<std::variant<std::pair<uint64_t, uint64_t>, flags>> datapoints,
    uint64_t original_file_size,
    uint32_t cluster_size,
    std::shared_ptr<NTFSReader> _reader,
    MFT *mft,
    uint32_t block_size,
    std::string stream_name,
    bool skip_sparse
) {
    ULONG64 written_bytes = 0ULL;
    HANDLE output = CreateFileW(dest_filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);

    if (output == INVALID_HANDLE_VALUE) { return std::unexpected(win_error(GetLastError(), ERROR_LOC)); }

    for (auto &i : _post_raw_pointers_process_data(datapoints, original_file_size, cluster_size, _reader, mft, stream_name, block_size, skip_sparse)) {
        DWORD written_block;
        if (!i.has_value()) { return std::unexpected(i.error().add_to_error_stack("Caller: _post_raw_pointers_process_data Error", ERROR_LOC)); }
        auto data_block = i.value();
        if (!WriteFile(output, data_block.first, data_block.second, &written_block, NULL)) {
            return std::unexpected(win_error(GetLastError(), ERROR_LOC));
        } else {
            written_bytes += written_block;
        }
    }
    CloseHandle(output);
    return written_bytes;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    if (!utils::processes::elevated(GetCurrentProcess())) {
        std::cerr << "Administrator rights are required to read physical drives" << std::endl;
        return 1;
    }

    auto disk_result = get_disk(1);
    if (!disk_result.has_value()) {
        win_error::print(disk_result.error());
        return 1;
    }
    std::shared_ptr<Disk> disk = disk_result.value();
    std::shared_ptr<Volume> vol = disk->volumes(3);

    if (!commands::helpers::is_ntfs(vol)) return 1;

    // std::string from_file = "C:/Users/AUTO/Desktop/inside_linux_win_path.bin";
    std::string from_file = "C:/Users/AUTO/Desktop/random_2gb.bin";
    // std::string from_file = "C:/Users/AUTO/Desktop/New Text Document.txt";
    std::string out_file = "C:/Users/AUTO/Desktop/new_blob.bin";
    std::wstring wout_file = utils::strings::from_string(out_file);

    std::shared_ptr<NTFSExplorer> explorer = std::make_shared<NTFSExplorer>(vol);
    auto temp_record = commands::helpers::find_record(explorer, from_file, 0);
    if (!temp_record.has_value()) {
        win_error::print(temp_record.error().add_to_error_stack("Caller: Original record not found", ERROR_LOC));
        return 1;
    }
    auto original_record = temp_record.value();

    auto _reader = original_record->get_reader();
    auto _mft = original_record->get_mft();
    auto c_size = _reader->sizes.cluster_size;
    std::vector<std::variant<std::pair<uint64_t, uint64_t>, flags>> datapoints;

    uint64_t orig_tot_size = original_record->datasize("", true).value();

    uint64_t tot_size = 0;

    blake3_hasher hash;
    blake3_hasher_init(&hash);

    // static_assert(std::is_trivially_copyable_v<blake3_hasher>);

    for (auto temp : original_record->_process_raw_pointers_with_hash(hash)) {
        if (!temp.has_value()) {
            win_error::print(temp.error());
            return 1;
        }
        auto some = temp.value();
        if (std::holds_alternative<std::tuple<uint64_t, uint64_t, std::array<uint8_t, 32>>>(some)) {
            auto y = get<std::tuple<uint64_t, uint64_t, std::array<uint8_t, 32>>>(some);
            std::cout << "offset: " << get<0>(y) << " size: " << get<1>(y) << " hash: ";
            for (uint8_t b : get<2>(y)) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
            }
            std::cout << std::dec << std::endl; // reset formatting
        }
    }

    std::array<uint8_t, BLAKE3_OUT_LEN> hash_out{};

    blake3_hasher_finalize(&hash, hash_out.data(), hash_out.size());

    std::cout << std::endl << std::endl;
    for (uint8_t b : hash_out) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    std::cout << std::dec << std::endl; // reset formatting

    // for (auto i: original_record->_process_raw_pointers()) {
    //     if (std::holds_alternative<std::pair<uint64_t, uint64_t>>(i)) {
    //         auto j = get<std::pair<uint64_t, uint64_t>>(i);
    //         std::cout << "[+] pointer: " << j.first << "  length: " << j.second << std::endl;

    //         // tot_size += j.second * c_size;
    //     } else if (std::holds_alternative<flags>(i)) {
    //         auto j = get<flags>(i);
    //         // glob = j;
    //         std::cout << "[+] flags: " << original_record->print_flags(j) << std::endl;
    //     }
    //     datapoints.push_back(i);
    // }

    // for (auto& i: _post_raw_pointer_datasize(
    //     datapoints,
    //     orig_tot_size,
    //     c_size,
    //     "",
    //     _reader,
    //     _mft
    // )){
    //     tot_size+=i.value();
    // }

    // std::cout << "tot size: " << tot_size << std::endl;

    // for (auto& i: datapoints) {
    //     if (std::holds_alternative<std::pair<uint64_t, uint64_t>>(i)) {
    //         auto j = get<std::pair<uint64_t, uint64_t>>(i);
    //         tot_size+=j.second;
    //     }
    // }

    // for (auto& i: _post_raw_pointer_datasize(
    //     datapoints,
    //     final_datasize,
    //     c_size,
    //     "",
    //     _reader,
    //     _mft
    // )) {
    //     if (!i.has_value()) {
    //         win_error::print(i.error().add_to_error_stack("Caller: Post_raw_pointer_datasize error", ERROR_LOC));
    //         return 1;
    //     }
    //     tot_size+=i.value();
    // }

    // for (auto& i: _process_read_operations(original_record, "")) {
    //     tot_size+=i.bytes_to_read;
    // }

    // std::cout << "total size: " << final_datasize << std::endl << "total counted size: " << tot_size << std::endl;

    // try {
    //     if (std::filesystem::remove(from_file)) {
    //         std::cout << "File deleted successfully.\n";
    //     } else {
    //         std::cout << "File not found or not deleted.\n";
    //     }
    // }
    // catch (const std::filesystem::filesystem_error& e) {
    //     std::cerr << "Error: " << e.what() << "\n";
    // }

    // for (auto &i: datapoints) {
    //     if (std::holds_alternative<std::pair<uint64_t, uint64_t>>(i)) {
    //         auto j = get<std::pair<uint64_t, uint64_t>>(i);
    //         std::cout << "[+] pointer: " << j.first << "  length: " << j.second << std::endl;
    //     } else if (std::holds_alternative<flags>(i)) {
    //         auto j = get<flags>(i);
    //         std::cout << "[+] flags: " << original_record->print_flags(j) << std::endl;
    //     }
    // }
    // auto j = data_to_file(
    //     wout_file,
    //     datapoints,
    //     orig_tot_size,
    //     c_size,
    //     _reader,
    //     _mft,
    //     1024*1024,
    //     "",
    //     true
    // );
    // if (!j.has_value()) {
    //     win_error::print(j.error().add_to_error_stack("Caller: data_to_file Error",ERROR_LOC));
    // }
    // std::cout << "total data written to file: " << j.value();
}