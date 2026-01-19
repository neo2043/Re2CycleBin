#pragma once

#include <Windows.h>
#include <string>

#include "NTFS/ntfs.hpp"
#include "Drive/mt_reader.hpp"
// #include "mt_ntfs_mft_walker.hpp"

class mt_ntfs_reader : public mt_reader {
    friend class mt_ntfs_mft_walker;
    friend class mt_ntfs_mft_record;
    public:
        explicit mt_ntfs_reader(std::wstring volume_name);
        ~mt_ntfs_reader() { std::cout << "mt_ntfs_reader closed" << std::endl; };
        
        struct {
            ULONG32 cluster_size = 0;
            ULONG32 record_size = 0;
            ULONG32 block_size = 0;
            ULONG32 sector_size = 0;
        } sizes;
        
        PBOOT_SECTOR_NTFS boot_record() { return (PBOOT_SECTOR_NTFS)&_boot_record; }

        void print_sizes() {
            std::cout << std::endl << "cluster size: " << sizes.cluster_size << std::endl
                                   << "record_size:  " << sizes.record_size  << std::endl
                                   << "block size:   " << sizes.block_size   << std::endl
                                   << "sector_size:  " << sizes.sector_size  << std::endl;
        }
};