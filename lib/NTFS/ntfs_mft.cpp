#include <cstdint>
#include <cstring>
#include <memory>
#include <set>
#include <expected>
#include <filesystem>

#include "ntfs.hpp"
#include "Utils/error.hpp"
#include "ntfs_index_entry.hpp"
#include "ntfs_reader.hpp"
#include "ntfs_mft.hpp"
#include "ntfs_mft_record.hpp"
#include "Utils/buffer.hpp"

MFT::MFT(std::shared_ptr<NTFSReader> reader) {
    _reader = reader;

    auto err = _reader->seek(_reader->boot_record()->MFTCluster * _reader->sizes.cluster_size);
    if (!err.has_value()) {
        win_error::print(err.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
    }

    Buffer<PMFT_RECORD_HEADER> rec(_reader->sizes.record_size);
    err = _reader->read(rec.data(), _reader->sizes.record_size);
    if (!err.has_value()) {
        win_error::print(err.error().add_to_error_stack("Caller: Read Error", ERROR_LOC));
        return;
    }
    
    _record = std::make_shared<MFTRecord>(rec.data(), this, _reader, _reader->boot_record()->MFTCluster * _reader->sizes.cluster_size);

    auto temp_data = _record->attribute_header($DATA);
    if (!temp_data.has_value()) {
        win_error::print(err.error().add_to_error_stack("Caller: No Data Attribute", ERROR_LOC));
        return;
    }
    PMFT_RECORD_ATTRIBUTE_HEADER pAttributeData = temp_data.value();
    
    auto temp_dataruns = MFTRecord::read_dataruns(pAttributeData);
    if (!temp_dataruns.has_value()) {
        win_error::print(temp_data.error().add_to_error_stack("Caller: No Data Runs", ERROR_LOC));
    }
    _dataruns = temp_dataruns.value();
}

MFT::~MFT(){}

// std::expected<std::pair<std::shared_ptr<MFTRecord>, LONGLONG>, win_error> MFT::record_from_path_with_offset(std::string path, ULONG64 directory_record_number) {
//     std::vector<std::wstring> parts;
//     std::filesystem::path p(path);
    
//     if (p.root_directory() != "\\") {
//         return std::unexpected(win_error("Only absolute paths are supported", ERROR_LOC));
//     }
    
//     if (p.has_root_name()) {
//         path = path.substr(2);
//         p = std::filesystem::path(path);
//     }
    
//     for (const auto& part : p) {
//         parts.push_back(part.generic_wstring());
//     }
    
//     auto temp_record = record_from_number_with_offset(directory_record_number);
//     if (!temp_record.has_value()) {
//         return std::unexpected(temp_record.error().add_to_error_stack("Caller: Record Error", ERROR_LOC));
//     }
//     std::shared_ptr<MFTRecord> current_dir = temp_record.value().first;
//     LONGLONG offset = temp_record.value().second;
//     std::shared_ptr<MFTRecord> next_dir = nullptr;
//     LONGLONG next_offset = 0;
    
//     for (size_t i = 1; i < parts.size(); i++) {
//         bool found = false;
//         next_dir = nullptr;
//         next_offset = 0;
//         auto temp_index = current_dir->index();
//         if (!temp_index.has_value()) {
//             return std::unexpected(temp_index.error().add_to_error_stack("Caller: Index error", ERROR_LOC));
//         }
//         std::vector<std::shared_ptr<IndexEntry>> index = temp_index.value();
//         for (std::shared_ptr<IndexEntry>& entry : index) {
//             if (_wcsicmp(entry->name().c_str(), parts[i].c_str()) == 0) {
//                 auto temp_record = record_from_number_with_offset(entry->record_number());
//                 if (!temp_record.has_value()) {
//                     win_error::print(temp_record.error().add_to_error_stack("Caller: Record Error", ERROR_LOC));
//                 }
//                 next_dir = temp_record.value().first;
//                 next_offset = temp_record.value().second;
//                 found = true;
//                 break;
//             }
//         }
//         if (!found) {
//             return std::unexpected(win_error("Record not found", ERROR_LOC));
//         } else {
//             current_dir = next_dir;
//             offset = next_offset;
//         }
//     }
//     return std::pair<std::shared_ptr<MFTRecord>, LONGLONG>(current_dir, offset);
// }

// std::expected<std::pair<std::shared_ptr<MFTRecord>, LONGLONG>, win_error> MFT::record_from_number_with_offset(ULONG64 record_number) {
//     LONGLONG sectorOffset = record_number * _reader->sizes.record_size / _reader->boot_record()->bytePerSector;
//     DWORD sectorNumber = _reader->sizes.record_size / _reader->boot_record()->bytePerSector;
//     LONGLONG offset = -1LL;

//     std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> buffer = std::make_shared<Buffer<PMFT_RECORD_HEADER>>(_reader->sizes.record_size);
    
//     for (DWORD sector = 0; sector < sectorNumber; sector++) {
//         ULONGLONG cluster = (sectorOffset + sector) / (_reader->sizes.cluster_size / _reader->boot_record()->bytePerSector);
//         LONGLONG vcn = 0LL;
//         // LONGLONG offset = -1LL;
        
//         for (const MFT_DATARUN& run : _dataruns) {
//             if (cluster < vcn + run.length) {
//                 offset = (run.offset + cluster - vcn) * _reader->sizes.cluster_size
//                 + (sectorOffset + sector) * _reader->boot_record()->bytePerSector % _reader->sizes.cluster_size;
//                 break;
//             }
//             vcn += run.length;
//         }
//         if (offset == -1LL) {
//             return std::unexpected(win_error("Record not found", ERROR_LOC));
//         }
        
//         auto temp_seek = _reader->seek(offset);
//         if (!temp_seek.has_value()) {
//             return std::unexpected(temp_seek.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
//         }
        
//         auto temp_read = _reader->read(buffer->address() + sector * _reader->boot_record()->bytePerSector, _reader->boot_record()->bytePerSector);
//         if (!temp_read.has_value()) {
//             return std::unexpected(temp_read.error().add_to_error_stack("Caller: Read Error", ERROR_LOC));
//         }
//     }
    
//     return std::pair<std::shared_ptr<MFTRecord>, LONGLONG>(std::make_shared<MFTRecord>(buffer->data(), this, _reader), offset - 512);
// }

std::expected<std::shared_ptr<MFTRecord>, win_error> MFT::record_from_path(std::string path, ULONG64 directory_record_number) {
    std::vector<std::wstring> parts;
    std::filesystem::path p(path);
    
    if (p.root_directory() != "\\") {
        return std::unexpected(win_error("Only absolute paths are supported", ERROR_LOC));
    }
    
    if (p.has_root_name()) {
        path = path.substr(2);
        p = std::filesystem::path(path);
    }
    
    for (const auto& part : p) {
        parts.push_back(part.generic_wstring());
    }
    
    auto temp_record = record_from_number(directory_record_number);
    if (!temp_record.has_value()) {
        return std::unexpected(temp_record.error().add_to_error_stack("Caller: Record Error", ERROR_LOC));
    }
    std::shared_ptr<MFTRecord> current_dir = temp_record.value();
    std::shared_ptr<MFTRecord> next_dir = nullptr;
    
    for (size_t i = 1; i < parts.size(); i++) {
        bool found = false;
        next_dir = nullptr;
        auto temp_index = current_dir->index();
        if (!temp_index.has_value()) {
            return std::unexpected(temp_index.error().add_to_error_stack("Caller: Index error", ERROR_LOC));
        }
        std::vector<std::shared_ptr<IndexEntry>> index = temp_index.value();
        for (std::shared_ptr<IndexEntry>& entry : index) {
            if (_wcsicmp(entry->name().c_str(), parts[i].c_str()) == 0) {
                auto temp_record = record_from_number(entry->record_number());
                if (!temp_record.has_value()) {
                    win_error::print(temp_record.error().add_to_error_stack("Caller: Record Error", ERROR_LOC));
                }
                next_dir = temp_record.value();
                found = true;
                break;
            }
        }
        if (!found) {
            return std::unexpected(win_error("Record not found", ERROR_LOC));
        } else {
            current_dir = next_dir;
        }
    }
    return current_dir;
}

std::expected<std::shared_ptr<MFTRecord>, win_error> MFT::record_from_number(ULONG64 record_number) {
    LONGLONG sectorOffset = record_number * _reader->sizes.record_size / _reader->boot_record()->bytePerSector;
    DWORD sectorNumber = _reader->sizes.record_size / _reader->boot_record()->bytePerSector;

    std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> buffer = std::make_shared<Buffer<PMFT_RECORD_HEADER>>(_reader->sizes.record_size);
    uint64_t offset = -1LL;
    
    for (DWORD sector = 0; sector < sectorNumber; sector++) {
        ULONGLONG cluster = (sectorOffset + sector) / (_reader->sizes.cluster_size / _reader->boot_record()->bytePerSector);
        LONGLONG vcn = 0LL;
        
        for (const MFT_DATARUN& run : _dataruns) {
            if (cluster < vcn + run.length) {
                offset = (run.offset + cluster - vcn) * _reader->sizes.cluster_size
                + (sectorOffset + sector) * _reader->boot_record()->bytePerSector % _reader->sizes.cluster_size;
                break;
            }
            vcn += run.length;
        }
        if (offset == -1LL) {
            return std::unexpected(win_error("Record not found", ERROR_LOC));
        }
        
        auto temp_seek = _reader->seek(offset);
        if (!temp_seek.has_value()) {
            return std::unexpected(temp_seek.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
        }
        
        auto temp_read = _reader->read(buffer->address() + sector * _reader->boot_record()->bytePerSector, _reader->boot_record()->bytePerSector);
        if (!temp_read.has_value()) {
            return std::unexpected(temp_read.error().add_to_error_stack("Caller: Read Error", ERROR_LOC));
        }
    }   
    
    return std::make_shared<MFTRecord>(buffer->data(), this, _reader, offset - 512);
}

std::expected<std::vector<std::tuple<std::wstring, ULONG64>>, win_error> MFT::list(std::string path, bool directory, bool files)
{
    std::vector<std::tuple<std::wstring, ULONG64>> ret;
    
    auto temp_record = record_from_path(path);
    if (!temp_record.has_value()) {
        return std::unexpected(temp_record.error().add_to_error_stack("Caller: Record Error", ERROR_LOC));
    }
    std::shared_ptr<MFTRecord> record = temp_record.value();
    
    if (record != nullptr) {
        auto temp_index = record->index();
        if (!temp_index.has_value()) {
            return std::unexpected(temp_index.error().add_to_error_stack("Caller: Index Error", ERROR_LOC));
        }
        std::vector<std::shared_ptr<IndexEntry>> index = temp_index.value();
        
        std::set<DWORD64> win32_named_entries;
        for (auto i : index) {
            if (i->name_type() != 2) {
                win32_named_entries.insert(i->record_number());
            }
        }

        for (auto& entry : index) {
            if ((entry->name_type() == 2) && (win32_named_entries.find(entry->record_number()) != win32_named_entries.end())) continue;
            
            auto temp_record = record_from_number(entry->record_number());
            if (!temp_record.has_value()) {
                return std::unexpected(temp_record.error().add_to_error_stack("Caller: Record Error", ERROR_LOC));
            }
            std::shared_ptr<MFTRecord> entry_rec = temp_record.value();
            if (directory && (entry_rec->header()->flag & MFT_RECORD_IS_DIRECTORY)) {
                ret.push_back(std::make_tuple(entry->name(), entry->record_number()));
            }
            if (files && ((entry_rec->header()->flag & MFT_RECORD_IS_DIRECTORY) == 0)) {
                ret.push_back(std::make_tuple(entry->name(), entry->record_number()));
            }
        }
    }

    return ret;
}
