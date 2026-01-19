#pragma once

#include <Windows.h>
#include <string>

#include "ntfs.hpp"
#include "Drive/reader.hpp"

class NTFSReader : public Reader {
	friend class MFT;
	friend class MFTRecord;

    public:
        explicit NTFSReader(std::wstring volume_name, DWORD64 volume_offset = 0);
        ~NTFSReader();

        struct {
            ULONG32 cluster_size = 0;
            ULONG32 record_size = 0;
            ULONG32 block_size = 0;
            ULONG32 sector_size = 0;
        } sizes;
        
        PBOOT_SECTOR_NTFS boot_record() { return (PBOOT_SECTOR_NTFS)&_boot_record; }
};