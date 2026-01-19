#pragma once

#include <cstdint>
#define NOMINMAX
#include <Windows.h>
#undef min
#undef max
#include <map>
#include <memory>
#include <set>
#include <algorithm>

// class mt_ntfs_mft_walker;

#include "Utils/buffer.hpp"
#include "mt_ntfs_mft_walker.hpp"
#include "mt_ntfs_reader.hpp"
#include "Utils/utils.hpp"
#include "NTFS/ntfs_index_entry.hpp"

#define MAGIC_FILE 0x454C4946
#define MAGIC_INDX 0x58444E49

typedef struct flags_t{
	bool is_Resident = false;
	bool is_NonResident = false;
	bool is_Compressed = false;
	bool is_InAttributeList = false;
} flags;

class mt_ntfs_mft_record {
    private:
        std::shared_ptr<mt_ntfs_reader> _reader = nullptr;
        std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> _record = nullptr;
        mt_ntfs_mft_walker* _mft_walker = nullptr;
        // std::shared_ptr<mt_ntfs_mft_walker> _mft_walker = nullptr;
        mt_ntfs_mft_walker* _p_mft_walker = nullptr;
        uint64_t offset = 0;
    
    public:
        // mt_ntfs_mft_record(
        //     PMFT_RECORD_HEADER pRH,
        //     std::shared_ptr<mt_ntfs_mft_walker> mft,
        //     std::shared_ptr<mt_ntfs_reader> reader,
        //     uint64_t offset
        // );

        mt_ntfs_mft_record(
            PMFT_RECORD_HEADER pRH,
            mt_ntfs_mft_walker* mft,
            std::shared_ptr<mt_ntfs_reader> reader,
            uint64_t offset
        );

        ~mt_ntfs_mft_record();

        PMFT_RECORD_HEADER header() { return _record->data(); }

        uint64_t raw_address();

        std::expected<uint64_t, win_error> raw_address(PMFT_RECORD_ATTRIBUTE_HEADER pAttr, uint64_t offset);

        void apply_fixups(PVOID buffer, DWORD buffer_size, WORD updateOffset, WORD updateSize);

        std::expected<PMFT_RECORD_ATTRIBUTE_HEADER, win_error> attribute_header(DWORD type, std::string name = "", int index = 0);

        template<typename T>
        std::shared_ptr<Buffer<T>> attribute_data(PMFT_RECORD_ATTRIBUTE_HEADER pAttributeData, bool real_size = true) {
            std::shared_ptr<Buffer<T>> ret = nullptr;

            if (pAttributeData->FormCode == RESIDENT_FORM) {
                // std::cout << "resident data len: " << pAttributeData->Form.Resident.ValueLength << std::endl;
                ret = std::make_shared<Buffer<T>>(pAttributeData->Form.Resident.ValueLength);
                memcpy_s(ret->data(), ret->size(), POINTER_ADD(LPBYTE, pAttributeData, pAttributeData->Form.Resident.ValueOffset), pAttributeData->Form.Resident.ValueLength);
            } else if (pAttributeData->FormCode == NON_RESIDENT_FORM) {
                ULONGLONG readSize = 0;
                ULONGLONG filesize = pAttributeData->Form.Nonresident.FileSize;

                ret = std::make_shared<Buffer<T>>(pAttributeData->Form.Nonresident.AllocatedLength);

                bool err = false;

                auto temp_dataruns = read_dataruns(pAttributeData);
                if (!temp_dataruns.has_value()) {
                    win_error::print(temp_dataruns.error().add_to_error_stack("Caller: Datarun Error", ERROR_LOC));
                }
                std::vector<MFT_DATARUN> runList = temp_dataruns.value();
                for (const MFT_DATARUN& run : runList) {
                    if (err) break; //-V547

                    if (run.offset == 0) {
                        for (ULONGLONG i = 0; i < run.length; i++) {
                            readSize += std::min<uint64_t>(filesize - readSize, _reader->sizes.cluster_size);
                        }
                    } else {
                        auto temp_read = _reader->read(
                            run.offset * _reader->sizes.cluster_size,
                            POINTER_ADD(PBYTE, ret->data(), DWORD(readSize)),
                            static_cast<DWORD>(run.length) * _reader->sizes.cluster_size
                        );
                        if (!temp_read.has_value()) {
                            win_error::print(temp_read.error().add_to_error_stack("Caller: Read error", ERROR_LOC));
                            err=true;
                            break;
                        } else {
                            readSize += std::min<uint64_t>(filesize - readSize, static_cast<DWORD>(run.length) * _reader->sizes.cluster_size);
                        }
                    }
                }
                if (readSize != filesize) {
                    std::cout << "[!] Invalid read file size" << std::endl;
                    ret = nullptr;
                }
                if (real_size) {
                    ret->shrink(static_cast<DWORD>(filesize));
                }
            }
            return ret;
        }

        std::expected<std::wstring, win_error> filename();

        std::expected<ULONG64, win_error> datasize(std::string stream_name = "", bool real_size = true);
        
        std::shared_ptr<Buffer<PBYTE>> data(std::string stream_name = "", bool real_size = true);

        std::expected<std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>, win_error> _parse_index_block(std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>> pIndexBlock, DWORD IBSize);
        // std::expected<std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>, win_error> _parse_index_block(std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>> pIndexBlock);

        std::expected<std::vector<std::shared_ptr<IndexEntry>>, win_error> index();

        std::expected<std::set<DWORD64>, win_error> index_records();
        
        static std::expected<std::vector<MFT_DATARUN>, win_error> read_dataruns(PMFT_RECORD_ATTRIBUTE_HEADER pAttribute);
        
        cppcoro::generator<std::variant<std::pair<uint64_t, uint64_t>, flags>> _process_raw_pointers(std::string stream_name = "");
        
        std::shared_ptr<mt_ntfs_reader> get_reader();
        
        std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> get_record();

        mt_ntfs_mft_walker* get_mft_walker();
        
        static bool is_valid(PMFT_RECORD_HEADER pmfth);

        bool is_valid();

        // std::shared_ptr<mt_ntfs_mft_walker> get_mft_walker();

        uint64_t get_offset();
        
        flags get_flags(PMFT_RECORD_ATTRIBUTE_HEADER data_attr);

        std::string print_flags(flags f);
};