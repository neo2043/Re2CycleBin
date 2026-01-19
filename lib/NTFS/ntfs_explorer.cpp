#include "ntfs_explorer.hpp"
#include "ntfs_mft_record.hpp"
#include "ntfs_mft.hpp"
#include "Drive\volume.hpp"
#include "Drive\disk.hpp"

NTFSExplorer::NTFSExplorer(std::shared_ptr<Volume> volume) {
    if (volume->disk_index() != DISK_INDEX_IMAGE) {
        _reader = std::make_shared<NTFSReader>(utils::strings::from_string(volume->name()));
    } else {
        auto pdisk = reinterpret_cast<Disk*>(volume->parent());
        if (pdisk->is_virtual()) {
            _reader = std::make_shared<NTFSReader>(utils::strings::from_string(volume->name()), volume->offset());
        }
        else {
            _reader = std::make_shared<NTFSReader>(utils::strings::from_string(reinterpret_cast<Disk*>(volume->parent())->name()), volume->offset());
        }
    }
    _MFT = std::make_shared<MFT>(_reader);
}

NTFSExplorer::~NTFSExplorer(){}