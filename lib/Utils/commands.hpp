#pragma once

#include <cstdint>
#include <expected>

#include "NTFS/ntfs_mft.hpp"
#include "NTFS/ntfs_explorer.hpp"
#include "NTFS/ntfs_mft_record.hpp"
#include "Utils/error.hpp"

#include <memory>

namespace commands
{
    namespace helpers
    {
        int is_ntfs(std::shared_ptr<Volume> vol);

        std::expected<std::shared_ptr<MFTRecord>, win_error> find_record(std::shared_ptr<NTFSExplorer> ex, std::string from="", uint64_t inode=0);

        // std::expected<std::pair<std::shared_ptr<MFTRecord>, LONGLONG>, win_error> find_record_with_offset(std::shared_ptr<NTFSExplorer> ex, std::string from, uint64_t inode);
    }
}