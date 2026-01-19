#pragma once

#include <Windows.h>
#include <memory>

#include "ntfs_reader.hpp"
#include "ntfs_mft.hpp"
#include "Drive/volume.hpp"

class NTFSExplorer {
    private:
        std::wstring _volume_name;
        std::shared_ptr<MFT> _MFT;
        std::shared_ptr<NTFSReader> _reader;

    public:
        explicit NTFSExplorer(std::shared_ptr<Volume> volume);
        ~NTFSExplorer();

        std::wstring volume_name() const { return _volume_name; }
        std::shared_ptr<NTFSReader> reader() { return _reader; }
        HANDLE handle() { return _reader->handle(); }
        std::shared_ptr<MFT> mft() { return _MFT; }
};

