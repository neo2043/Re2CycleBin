#include "constant_names.hpp"
#include "Drive/mbr_gpt.hpp"

#include <Windows.h>
#include <cstdint>
#include <string>

#include "Drive/mbr_gpt.hpp"
#include "NTFS/ntfs.hpp"
#include "Utils/utils.hpp"
#include <vss.h>

std::string constants::disk::mbr_type(uint8_t type) {
    const char *mbr_types[] = {
        "Unused",
        "FAT12",
        "XENIX root",
        "XENIX usr",
        "FAT16",
        "DOS Extended",
        "FAT16 (huge)",
        "NTFS / exFAT",
        "DELL (spanning) / AIX Boot",
        "AIX Data",
        "OS/2 Boot / OPUS",
        "FAT32",
        "FAT32 (LBA)",
        "Unknown",
        "FAT16 (LBA)",
        "DOS Extended (LBA)",
        "Unknown",
        "FAT12 (hidden)",
        "Config / Diagnostics",
        "Unknown",
        "FAT16 (hidden)",
        "Unknown",
        "FAT16 (huge, hidden)",
        "NTFS / exFAT (hidden)",
        "Unknown",
        "Unknown",
        "Unknown",
        "FAT32 (hidden)",
        "FAT32 (LBA, hidden)",
        "Unknown",
        "FAT16 (LBA, hidden)",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Rescue (FAT32 or NTFS)",
        "Unknown",
        "Unknown",
        "AFS",
        "SylStor",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "JFS",
        "Unknown",
        "Unknown",
        "THEOS",
        "THEOS",
        "THEOS",
        "THEOS",
        "PartitionMagic Recovery",
        "Netware (hidden)",
        "Unknown",
        "Unknown",
        "Pick",
        "PPC PReP / RISC Boot",
        "LDM/SFS",
        "Unknown",
        "GoBack",
        "Boot-US",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Oberon",
        "QNX4.x",
        "QNX4.x",
        "QNX4.x",
        "Lynx RTOS",
        "Novell",
        "Unknown",
        "DM 6.0 Aux3",
        "DM 6.0 DDO",
        "EZ-Drive",
        "FAT (AT&T MS-DOS)",
        "DrivePro",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "SpeedStor",
        "Unknown",
        "UNIX",
        "Netware 2",
        "Netware 3/4",
        "Netware SMS",
        "Novell",
        "Novell",
        "Netware 5+, NSS",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "DiskSecure Multi-Boot",
        "Unknown",
        "V7/x86",
        "Unknown",
        "Scramdisk",
        "IBM PC/IX",
        "Unknown",
        "M2FS/M2CS",
        "XOSL FS",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "NTFT",
        "MINIX",
        "Linux Swap / Solaris",
        "Linux",
        "Hibernation",
        "Linux Extended",
        "FAT16 (fault tolerant)",
        "NTFS (fault tolerant)",
        "Linux plaintext part tbl",
        "Unknown",
        "Linux Kernel",
        "FAT32 (fault tolerant)",
        "FAT32 (LBA, fault tolerant)",
        "FAT12 (hidden, fd)",
        "Linux LVM",
        "Unknown",
        "FAT16 (hidden, fd)",
        "DOS Extended (hidden, fd)",
        "FAT16 (huge, hidden, fd)",
        "Hidden Linux",
        "Unknown",
        "Unknown",
        "CHRP ISO-9660",
        "FAT32 (hidden, fd)",
        "FAT32 (LBA, hidden, fd)",
        "Unknown",
        "FAT16 (hidden, fd)",
        "DOS Extended (LBA, hidden, fd)",
        "Unknown",
        "Unknown",
        "ForthOS",
        "BSD/OS",
        "Hibernation",
        "Hibernation",
        "Unknown",
        "Unknown",
        "Unknown",
        "BSD",
        "OpenBSD",
        "NeXTStep",
        "Mac OS-X",
        "NetBSD",
        "Unknown",
        "Mac OS-X Boot",
        "Unknown",
        "Unknown",
        "Unknown",
        "MacOS X HFS",
        "BootStar Dummy",
        "QNX Neurtino Power-Safe",
        "QNX Neurtino Power-Safe",
        "QNX Neurtino Power-Safe",
        "Unknown",
        "Unknown",
        "Corrupted FAT16",
        "BSDI / Corrupted NTFS",
        "BSDI Swap",
        "Unknown",
        "Unknown",
        "Acronis Boot Wizard Hidden",
        "Acronis Backup",
        "Unknown",
        "Solaris 8 Boot",
        "Solaris",
        "Valid NTFT",
        "Unknown",
        "Hidden Linux",
        "Hidden Linux Swap",
        "Unknown",
        "Unknown",
        "Corrupted FAT16",
        "Syrinx Boot / Corrupted NTFS",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "CP/M",
        "Unknown",
        "Powercopy Backup",
        "CP/M",
        "Unknown",
        "Unknown",
        "Dell PowerEdge Server",
        "BootIt EMBRM",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "LUKS",
        "Unknown",
        "Unknown",
        "BeOS BFS",
        "SkyOS SkyFS",
        "Unknown",
        "EFI Header",
        "EFI",
        "Linux/PA-RISC Boot",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Unknown",
        "Bochs",
        "VMware",
        "VMware Swap",
        "Linux RAID",
        "Windows NT (hidden)",
        "Xenix Bad Block Table",
    };
    return mbr_types[type];
}

std::string constants::disk::gpt_type(GUID type) {
    const GUID gpt_guids[] = {
        PARTITION_ENTRY_UNUSED_GUID,
        PARTITION_MBR_SCHEME_GUID,
        PARTITION_SYSTEM_GUID,
        PARTITION_BIOS_BOOT_GUID,
        PARTITION_MSFT_RESERVED_GUID,
        PARTITION_BASIC_DATA_GUID,
        PARTITION_LDM_METADATA_GUID,
        PARTITION_LDM_DATA_GUID,
        PARTITION_MSFT_RECOVERY_GUID,
        PARTITION_IBM_GPFS_GUID,
        PARTITION_HPUX_DATA_GUID,
        PARTITION_HPUX_SERVICE_GUID,
        PARTITION_LINUX_RAID_GUID,
        PARTITION_LINUX_SWAP_GUID,
        PARTITION_LINUX_LVM_GUID,
        PARTITION_LINUX_RESERVED_GUID,
        PARTITION_FREEBSD_BOOT_GUID,
        PARTITION_FREEBSD_DATA_GUID,
        PARTITION_FREEBSD_SWAP_GUID,
        PARTITION_FREEBSD_UFS_GUID,
        PARTITION_FREEBSD_VINUM_VM_GUID,
        PARTITION_FREEBSD_ZFS_GUID,
        PARTITION_APPLE_HFSP_GUID,
        PARTITION_APPLE_UFS_GUID,
        PARTITION_APPLE_RAID_GUID,
        PARTITION_APPLE_RAID_OFFLINE_GUID,
        PARTITION_APPLE_BOOT_GUID,
        PARTITION_APPLE_LABEL_GUID,
        PARTITION_APPLE_TV_RECOVERY_GUID,
        PARTITION_SOLARIS_BOOT_GUID,
        PARTITION_SOLARIS_ROOT_GUID,
        PARTITION_SOLARIS_SWAP_GUID,
        PARTITION_SOLARIS_BACKUP_GUID,
        PARTITION_SOLARIS_USR_GUID,
        PARTITION_SOLARIS_VAR_GUID,
        PARTITION_SOLARIS_HOME_GUID,
        PARTITION_SOLARIS_ALTERNATE_GUID,
        PARTITION_SOLARIS_RESERVED_1_GUID,
        PARTITION_SOLARIS_RESERVED_2_GUID,
        PARTITION_SOLARIS_RESERVED_3_GUID,
        PARTITION_SOLARIS_RESERVED_4_GUID,
        PARTITION_SOLARIS_RESERVED_5_GUID,
        PARTITION_NETBSD_SWAP_GUID,
        PARTITION_NETBSD_FFS_GUID,
        PARTITION_NETBSD_LFS_GUID,
        PARTITION_NETBSD_RAID_GUID,
        PARTITION_NETBSD_CONCATENATED_GUID,
        PARTITION_NETBSD_ENCRYPTED_GUID,
        PARTITION_CHROME_KERNEL_GUID,
        PARTITION_CHROME_ROOTFS_GUID,
        PARTITION_CHROME_RESERVED_GUID,
    };

    const char *gpt_types[] = {
        "Entry Unused",
        "MBR Partition Scheme",
        "EFI System",
        "BIOS Boot",
        "Microsoft Reserved",
        "Basic Data",
        "LDM Metadata",
        "LDM Data",
        "WinRE",
        "IBM GPFS",
        "HPUX Data",
        "HPUX Service",
        "Linux RAID",
        "Linux Swap",
        "Linux LVM",
        "Linux Reserved",
        "FreeBSD Boot",
        "FreeBSD Data",
        "FreeBSD Swap",
        "FreeBSD UFS",
        "FreeBSD Vinum VM",
        "FreeBSD ZFS",
        "Apple HFS+",
        "Apple UFS",
        "Apple RAID",
        "Apple RAID Offline",
        "Apple Boot",
        "Apple Labe",
        "Apple TV Recovery",
        "Solaris Boot",
        "Solaris Root",
        "Solaris Swap",
        "Solaris Backup",
        "Solaris /usr",
        "Solaris /var",
        "Solaris /home",
        "Solaris Alternate",
        "Solaris Reserved",
        "Solaris Reserved",
        "Solaris Reserved",
        "Solaris Reserved",
        "Solaris Reserved",
        "NetBSD Swap",
        "NetBSD FFS",
        "NetBSD LFS",
        "NetBSD RAID",
        "NetBSD Concatenated",
        "NetBSD Encrypted",
        "ChromeOS Kerne",
        "ChromeOS rootfs",
        "ChromeOS Reserved",
    };

    size_t j;
    std::string ret = "Unknown";
    for (j = 0; j < ARRAYSIZE(gpt_guids); ++j) {
        if (IsEqualGUID(type, gpt_guids[j])) {
            ret = gpt_types[j];
            break;
        }
    }

    return ret;
}

std::string constants::disk::media_type(MEDIA_TYPE t) {
    switch (t) {
        case FixedMedia:
            return "Fixed";
        case RemovableMedia:
            return TEXT("Removable");
        case Unknown:
            return TEXT("Unknown");
        case F5_1Pt2_512:
            return TEXT("5.25\", 1.2MB");
        case F3_1Pt44_512:
            return TEXT("3.5\", 1.44MB");
        case F3_2Pt88_512:
            return TEXT("3.5\", 2.88MB");
        case F3_20Pt8_512:
            return TEXT("3.5\", 20.8MB");
        case F3_720_512:
            return TEXT("3.5\", 720KB");
        case F5_360_512:
            return TEXT("5.25\", 360KB");
        case F5_320_512:
            return TEXT("5.25\", 320KB");
        case F5_320_1024:
            return TEXT("5.25\", 320KB");
        case F5_180_512:
            return TEXT("5.25\", 180KB");
        case F5_160_512:
            return TEXT("5.25\", 160KB");
        case F3_120M_512:
            return TEXT("3.5\", 120M");
        case F3_640_512:
            return TEXT("3.5\" , 640KB");
        case F5_640_512:
            return TEXT("5.25\", 640KB");
        case F5_720_512:
            return TEXT("5.25\", 720KB");
        case F3_1Pt2_512:
            return TEXT("3.5\" , 1.2Mb");
        case F3_1Pt23_1024:
            return TEXT("3.5\" , 1.23Mb");
        case F5_1Pt23_1024:
            return TEXT("5.25\", 1.23MB");
        case F3_128Mb_512:
            return TEXT("3.5\" MO 128Mb");
        case F3_230Mb_512:
            return TEXT("3.5\" MO 230Mb");
        case F8_256_128:
            return TEXT("8\", 256KB");
        case F3_200Mb_512:
            return TEXT("3.5\", 200M");
        case F3_240M_512:
            return TEXT("3.5\", 240Mb");
        case F3_32M_512:
            return TEXT("3.5\", 32Mb");
        default:
            return TEXT("Unknown");
    }
}

std::string constants::disk::partition_type(DWORD t) {
    switch (t) {
        case PARTITION_STYLE_GPT:
            return TEXT("GPT");
        case PARTITION_STYLE_MBR:
            return TEXT("MBR");
        case PARTITION_STYLE_RAW:
            return TEXT("RAW");
        default:
            return TEXT("UNKNOWN");
    }
}

std::string constants::disk::drive_type(DWORD t) {
    switch (t) {
        case DRIVE_UNKNOWN:
            return TEXT("UNKNOWN");
        case DRIVE_NO_ROOT_DIR:
            return TEXT("No mounted volume");
        case DRIVE_REMOVABLE:
            return TEXT("Removable");
        case DRIVE_FIXED:
            return TEXT("Fixed");
        case DRIVE_REMOTE:
            return TEXT("Remote");
        case DRIVE_CDROM:
            return TEXT("CD-ROM");
        case DRIVE_RAMDISK:
            return TEXT("RAM disk");
        default:
            return TEXT("UNKNOWN");
    }
}

std::string constants::disk::mft::file_record_filename_name_type(UCHAR t) {
    switch (t) {
        case 0:
            return TEXT("POSIX");
        case 1:
            return TEXT("WIN32");
        case 2:
            return TEXT("DOS");
        case 3:
            return TEXT("DOS & WIN32");
        default:
            return TEXT("UNKNOWN");
    }
}

std::string constants::disk::mft::file_record_flags(ULONG32 f) {
    switch (f) {
        case 0:
            return "Not in use";
        case MFT_RECORD_IN_USE:
            return "In use";
        case MFT_RECORD_IS_DIRECTORY:
            return "Directory";
        case MFT_RECORD_IN_USE | MFT_RECORD_IS_DIRECTORY:
            return "Directory in use";
        default:
            return "Unknown (" + utils::format::hex(f) + ")";
    }
}

std::string constants::disk::mft::file_record_attribute_type(ULONG32 a) {
    switch (a) {
        case $STANDARD_INFORMATION:
            return "$STANDARD_INFORMATION";
        case $ATTRIBUTE_LIST:
            return "$ATTRIBUTE_LIST";
        case $FILE_NAME:
            return "$FILE_NAME";
        case $OBJECT_ID:
            return "$OBJECT_ID";
        case $SECURITY_DESCRIPTOR:
            return "$SECURITY_DESCRIPTOR";
        case $VOLUME_NAME:
            return "$VOLUME_NAME";
        case $VOLUME_INFORMATION:
            return "$VOLUME_INFORMATION";
        case $DATA:
            return "$DATA";
        case $INDEX_ROOT:
            return "$INDEX_ROOT";
        case $INDEX_ALLOCATION:
            return "$INDEX_ALLOCATION";
        case $BITMAP:
            return "$BITMAP";
        case $REPARSE_POINT:
            return "$REPARSE_POINT";
        case $EA_INFORMATION:
            return "$EA_INFORMATION";
        case $EA:
            return "$EA";
        case $LOGGED_UTILITY_STREAM:
            return "$LOGGED_UTILITY_STREAM";
        case $END:
            return "$END";
        default:
            return "Unknown (" + utils::format::hex(a) + ")";
    }
}

std::string constants::disk::mft::file_record_index_root_attribute_flag(ULONG32 f) {
    switch (f) {
        case MFT_ATTRIBUTE_INDEX_ROOT_FLAG_SMALL:
            return "Small Index";
        case MFT_ATTRIBUTE_INDEX_ROOT_FLAG_LARGE:
            return "Large Index";
        default:
            return "Unknown (" + utils::format::hex(f) + ")";
    }
}

std::string constants::disk::mft::file_record_index_root_attribute_type(ULONG32 a) {
    switch (a) {
        case 0x30:
            return "Filename";
        case 0x00:
            return "Reparse points";
        default:
            return "Unknown (" + utils::format::hex(a) + ")";
    }
}

std::string constants::disk::mft::file_record_reparse_point_type(ULONG32 tag) {
    switch (tag) {
        case IO_REPARSE_TAG_MOUNT_POINT:
            return "Mount Point";
        case IO_REPARSE_TAG_HSM:
            return "Hierarchical Storage Manager";
        case IO_REPARSE_TAG_HSM2:
            return "Hierarchical Storage Manager 2";
        case IO_REPARSE_TAG_SIS:
            return "Single-instance Storage";
        case IO_REPARSE_TAG_WIM:
            return "WIM Mount";
        case IO_REPARSE_TAG_CSV:
            return "Clustered Shared Volumes";
        case IO_REPARSE_TAG_DFS:
            return "Distributed File System";
        case IO_REPARSE_TAG_SYMLINK:
            return "Symbolic Link";
        case IO_REPARSE_TAG_DFSR:
            return "DFS filter";
        case IO_REPARSE_TAG_DEDUP:
            return "Data Deduplication";
        case IO_REPARSE_TAG_NFS:
            return "Network File System";
        case IO_REPARSE_TAG_FILE_PLACEHOLDER:
            return "Windows Shell 8.1";
        case IO_REPARSE_TAG_WOF:
            return "Windows Overlay";
        case IO_REPARSE_TAG_WCI:
            return "Windows Container Isolation";
        case IO_REPARSE_TAG_WCI_1:
            return "Windows Container Isolation";
        case IO_REPARSE_TAG_GLOBAL_REPARSE:
            return "NPFS";
        case IO_REPARSE_TAG_CLOUD:
            return "Cloud Files";
        case IO_REPARSE_TAG_CLOUD_1:
            return "Cloud Files (1)";
        case IO_REPARSE_TAG_CLOUD_2:
            return "Cloud Files (2)";
        case IO_REPARSE_TAG_CLOUD_3:
            return "Cloud Files (3)";
        case IO_REPARSE_TAG_CLOUD_4:
            return "Cloud Files (4)";
        case IO_REPARSE_TAG_CLOUD_5:
            return "Cloud Files (5)";
        case IO_REPARSE_TAG_CLOUD_6:
            return "Cloud Files (6)";
        case IO_REPARSE_TAG_CLOUD_7:
            return "Cloud Files (7)";
        case IO_REPARSE_TAG_CLOUD_8:
            return "Cloud Files (8)";
        case IO_REPARSE_TAG_CLOUD_9:
            return "Cloud Files (9)";
        case IO_REPARSE_TAG_CLOUD_A:
            return "Cloud Files (A)";
        case IO_REPARSE_TAG_CLOUD_B:
            return "Cloud Files (B)";
        case IO_REPARSE_TAG_CLOUD_C:
            return "Cloud Files (C)";
        case IO_REPARSE_TAG_CLOUD_D:
            return "Cloud Files (D)";
        case IO_REPARSE_TAG_CLOUD_E:
            return "Cloud Files (E)";
        case IO_REPARSE_TAG_CLOUD_F:
            return "Cloud Files (F)";
        case IO_REPARSE_TAG_CLOUD_MASK:
            return "Cloud Files Mask";
        case IO_REPARSE_TAG_APPEXECLINK:
            return "AppExecLink";
        case IO_REPARSE_TAG_PROJFS:
            return "Windows Projected File System";
        case IO_REPARSE_TAG_STORAGE_SYNC:
            return "Azure File Sync";
        case IO_REPARSE_TAG_WCI_TOMBSTONE:
            return "Windows Container Isolation";
        case IO_REPARSE_TAG_UNHANDLED:
            return "Windows Container Isolation";
        case IO_REPARSE_TAG_ONEDRIVE:
            return "One Drive";
        case IO_REPARSE_TAG_PROJFS_TOMBSTONE:
            return "Windows Projected File System";
        case IO_REPARSE_TAG_AF_UNIX:
            return "Windows Subsystem for Linux Socket";
        default:
            return "Unknown (0x" + utils::format::hex(tag) + ")";
    }
}

std::string constants::disk::mft::efs_type(ULONG32 t) {
    switch (t) {
        case 1:
            return "CryptoAPI Container";
        case 3:
            return "Certificate Fingerprint";
        default:
            return "Unknown type";
    }
}

std::string constants::disk::mft::wof_compression(DWORD c) {
    switch (c) {
        case 0:
            return "XPRESS 4k";
        case 1:
            return "LZX 32k";
        case 2:
            return "XPRESS 8k";
        case 3:
            return "XPRESS 16k";
        default:
            return TEXT("UNKNOWN");
    }
}
