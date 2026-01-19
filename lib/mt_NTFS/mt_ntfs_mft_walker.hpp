#pragma once

// #include <cstdint>
#include <memory>

// class mt_ntfs_mft_record;
// class mt_ntfs_reader;

#include "mt_ntfs_reader.hpp"
#include "NTFS/ntfs.hpp"
#include "mt_ntfs_mft_record.hpp"
// #include "thread_pool/thread_pool.h"

class mt_ntfs_mft_walker {
// class mt_ntfs_mft_walker : public std::enable_shared_from_this<mt_ntfs_mft_walker> {
    private:
        std::shared_ptr<mt_ntfs_reader> _reader;
        std::vector<MFT_DATARUN> _dataruns;
        std::shared_ptr<mt_ntfs_mft_record> _record;
        
    public:
        explicit mt_ntfs_mft_walker(std::shared_ptr<mt_ntfs_reader> reader);
        ~mt_ntfs_mft_walker() { std::cout << "mt_ntfs_mft_walker closed" << std::endl; };

        // void init_mft_record_datarun();

        mt_ntfs_mft_record* get_record() { return _record.get(); }
        std::vector<MFT_DATARUN>* get_vector() {return &_dataruns; };
        std::expected<std::shared_ptr<mt_ntfs_mft_record>, win_error> record_from_path(std::string path, ULONG64 directory_record_number = ROOT_FILE_NAME_INDEX_NUMBER);
        std::expected<std::shared_ptr<mt_ntfs_mft_record>, win_error> record_from_number(ULONG64 record_number);
        std::expected<std::vector<std::tuple<std::wstring, ULONG64>>, win_error> list(std::string path, bool directory = true, bool files = true);

        // friend void walk(std::string path);
};