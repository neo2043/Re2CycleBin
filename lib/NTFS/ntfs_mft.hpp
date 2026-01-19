#pragma once

#include <Windows.h>
#include <vector>
#include <string>
#include <memory>
#include <expected>

#include "ntfs.hpp"
#include "ntfs_reader.hpp"

class MFTRecord;
class NTFSReader;
#include "ntfs_mft_record.hpp"

// forward declaration to avoid circular include: ntfs_explorer.hpp references MFT

class MFT {
    private:
        std::shared_ptr<MFTRecord> _record;
        std::shared_ptr<NTFSReader> _reader;
        std::vector<MFT_DATARUN> _dataruns;

    public:
        explicit MFT(std::shared_ptr<NTFSReader> reader);
        ~MFT();

        std::shared_ptr<MFTRecord> record() { return _record; }
        std::expected<std::shared_ptr<MFTRecord>, win_error> record_from_path(std::string path, ULONG64 directory_record_number = ROOT_FILE_NAME_INDEX_NUMBER);
        std::expected<std::shared_ptr<MFTRecord>, win_error> record_from_number(ULONG64 record_number);
        std::expected<std::vector<std::tuple<std::wstring, ULONG64>>, win_error> list(std::string path, bool directory = true, bool files = true);
        // std::expected<std::pair<std::shared_ptr<MFTRecord>, LONGLONG>, win_error> record_from_number_with_offset(ULONG64 record_number);
        // std::expected<std::pair<std::shared_ptr<MFTRecord>, LONGLONG>, win_error> record_from_path_with_offset(std::string path, ULONG64 directory_record_number = ROOT_FILE_NAME_INDEX_NUMBER);
};