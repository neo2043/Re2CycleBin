#include "commands.hpp"
#include "Utils/error.hpp"
#include <expected>

int commands::helpers::is_ntfs(std::shared_ptr<Volume> vol) {
    if ((vol->filesystem() != "NTFS") && (vol->filesystem() != "Bitlocker")) {
        win_error::print(win_error("Error: NTFS volume required", ERROR_LOC));
        return 0;
    }
    return 1;
}

std::expected<std::shared_ptr<MFTRecord>, win_error> commands::helpers::find_record(std::shared_ptr<NTFSExplorer> ex, std::string from, uint64_t inode) {
    std::shared_ptr<MFTRecord> rec = nullptr;

    if (from != "") {
        auto temp_record = ex->mft()->record_from_path(from);
        if (!temp_record.has_value()) {
            return std::unexpected(temp_record.error().add_to_error_stack("Caller: Record not found Error", ERROR_LOC));
        } else {
            rec = temp_record.value();
        }
    } else {
        auto temp_record = ex->mft()->record_from_number(inode);
        if (!temp_record.has_value()) {
            return std::unexpected(temp_record.error().add_to_error_stack("Caller: Record not found Error", ERROR_LOC));
        } else {
            rec = temp_record.value();
        }
    }

    return rec;
}

// std::expected<std::pair<std::shared_ptr<MFTRecord>, LONGLONG>, win_error> commands::helpers::find_record_with_offset(std::shared_ptr<NTFSExplorer> ex, std::string from, int64_t inode) {
//     std::pair<std::shared_ptr<MFTRecord>, LONGLONG> ret;

//     if (from != "") {
//         auto temp_record = ex->mft()->record_from_path_with_offset(from);
//         if (!temp_record.has_value()) {
//             return std::unexpected(temp_record.error().add_to_error_stack("Caller: Record not found Error", ERROR_LOC));
//         } else {
//             ret = temp_record.value();
//         }
//     } else {
//         auto temp_record = ex->mft()->record_from_number_with_offset(inode);
//         if (!temp_record.has_value()) {
//             return std::unexpected(temp_record.error().add_to_error_stack("Caller: Record not found Error", ERROR_LOC));
//         } else {
//             ret = temp_record.value();
//         }
//     }

//     return ret;
// }