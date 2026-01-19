#include "mt_ntfs_reader.hpp"

mt_ntfs_reader::mt_ntfs_reader(std::wstring volume_name) : mt_reader(volume_name) {
    PBOOT_SECTOR_NTFS pbs = (PBOOT_SECTOR_NTFS)_boot_record;
	ULONG32 real_sector_per_cluster;
	
	if (pbs->sectorPerCluster > 0x80) {
		real_sector_per_cluster = 1 << -pbs->sectorPerCluster;
	}
	else {
		real_sector_per_cluster = pbs->sectorPerCluster;
	}

	sizes.cluster_size = pbs->bytePerSector * real_sector_per_cluster;
	sizes.record_size = pbs->clusterPerRecord >= 0 ? pbs->clusterPerRecord * sizes.cluster_size : 1 << -pbs->clusterPerRecord;
	sizes.block_size = pbs->clusterPerBlock >= 0 ? pbs->clusterPerBlock * sizes.cluster_size : 1 << -pbs->clusterPerBlock;
	sizes.sector_size = pbs->bytePerSector;
}

// mt_ntfs_reader::~mt_ntfs_reader(){}