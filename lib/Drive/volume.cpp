#pragma once

#include "volume.hpp"
#include "Utils/error.hpp"
#include "disk.hpp"
#include "vbr.hpp"
#include "Utils/utils.hpp"
#include "Utils/constant_names.hpp"

#include <Windows.h>
#include <expected>
#include <cstring>
#include <winnt.h>
#include <iostream>


namespace core {
    namespace win {
        namespace volumes {
            std::expected<std::vector<std::shared_ptr<Volume>>, win_error> list() {
                std::vector<std::shared_ptr<Volume>> volumes;
                auto i = disks::list();
                if (!i.has_value()) {
                    return std::unexpected(i.error().add_to_error_stack("Caller: Disk List Error", ERROR_LOC));
                }
                std::vector<std::shared_ptr<Disk>> disks = i.value();
                for (auto& disk : disks) {
                    std::vector<std::shared_ptr<Volume>> diskVols = disk->volumes();
                    for (auto& diskVol : diskVols){
                        volumes.push_back(diskVol);
                    }
                }
                return volumes;
            }
        }
    }
}


int roundUp(int numToRound, int multiple) {
    if (multiple == 0) {
        return numToRound;
    }
    int remainder = numToRound % multiple;
    if (remainder == 0) {
        return numToRound;
    }
    return numToRound + multiple - remainder;
}

std::expected<bool, win_error> findVolumeName(wchar_t* volName, int diskno, long long offs, long long len) {
    HANDLE vol = FindFirstVolumeW(
        volName,
        MAX_PATH
    );
    auto i = win_error(GetLastError(),ERROR_LOC);
    if (i.Get_First_Error_Code()!=0 && i.Get_First_Error_Code()!=234) { // 234 because "More data is available." and thats intentional here.
        std::cout << i;
    }
    bool success = vol != INVALID_HANDLE_VALUE;
    bool found = false;
    while (success && !found) {
        volName[wcslen(volName) - 1] = '\0';
        HANDLE volH = CreateFileW(
            volName,
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            0
        );
        volName[wcslen(volName)] = '\\';
        if (volH != INVALID_HANDLE_VALUE) {
            DWORD bret = sizeof(VOLUME_DISK_EXTENTS) + 256 * sizeof(DISK_EXTENT);
            std::shared_ptr<Buffer<PVOLUME_DISK_EXTENTS>> vde = std::make_shared<Buffer<PVOLUME_DISK_EXTENTS>>(bret);
            if (!DeviceIoControl(
                volH,
                IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                NULL,
                0,
                vde->data(),
                bret,
                &bret,
                NULL
            )) {
                i = win_error(GetLastError(),ERROR_LOC);
                if (i.Get_First_Error_Code()!=0) {
                    std::cout << i;
                }
            } else {
                for (unsigned i = 0; i < vde->data()->NumberOfDiskExtents; i++) {
                    if (vde->data()->Extents[i].DiskNumber == diskno &&
                        vde->data()->Extents[i].StartingOffset.QuadPart == offs &&
                        vde->data()->Extents[i].ExtentLength.QuadPart == len) {
                        found = true;
                        break;
                    }
                }
            }
            CloseHandle(volH);
        } else {
            i = win_error(GetLastError(),ERROR_LOC);
            if (i.Get_First_Error_Code()!=0) {
                std::cout << i;
            }
        }
        if (!found) {
            success = FindNextVolumeW(
                vol,
                volName,
                MAX_PATH
            ) != 0;
        }
    }
    FindVolumeClose(vol);
    i = win_error(GetLastError(),ERROR_LOC);
    if (i.Get_First_Error_Code()!=0 && i.Get_First_Error_Code()!=18) {  //18 because "There are no more files." and thats intentional here.
        std::cout << i;
    }
    return found;
}

std::expected<std::shared_ptr<Buffer<PBYTE>>, win_error> read_fve(HANDLE h,LARGE_INTEGER offset) {
    DWORD read = 0;
    std::shared_ptr<Buffer<PBYTE>> fve = std::make_shared<Buffer<PBYTE>>(512);
    if (!SetFilePointer(
        h,
        offset.LowPart,
        &offset.HighPart,
        FILE_BEGIN
    )) {
        return std::unexpected(win_error(GetLastError(),ERROR_LOC));
    }
    if (!ReadFile(
        h,
        fve->data(),
        fve->size(),
        &read,
        NULL
    )) {
        return std::unexpected(win_error(GetLastError(),ERROR_LOC));
    }
    DWORD size = ((PFVE_BLOCK_HEADER)fve->data())->size;
    if (((PFVE_BLOCK_HEADER)fve->data())->version == 2) size *= 16;
    size = roundUp(size, 512);
    fve->resize(size);
    if (!SetFilePointer(
        h,
        offset.LowPart,
        &offset.HighPart,
        FILE_BEGIN
    )) {
        return std::unexpected(win_error(GetLastError(),ERROR_LOC));
    }
    if (!ReadFile(
        h,
        fve->data(),
        fve->size(),
        &read,
        NULL
    )) {
        return std::unexpected(win_error(GetLastError(),ERROR_LOC));
    }
    return fve;
}

std::expected<void, win_error> Volume::_detect_bitlocker(HANDLE h, PARTITION_INFORMATION_EX p) {
    DWORD read;
    if (h != INVALID_HANDLE_VALUE) {
        _bootsector.resize(512);
        if (!SetFilePointer(
            h,
            p.StartingOffset.LowPart,
            &p.StartingOffset.HighPart,
            FILE_BEGIN
        )) {
            return std::unexpected(win_error(GetLastError(),ERROR_LOC));
        }
        if (!ReadFile(
            h,
            _bootsector.data(),
            512,
            &read,
            NULL
        )) {
            return std::unexpected(win_error(GetLastError(),ERROR_LOC));
        }
        if (std::memcmp(((PBOOT_SECTOR_COMMON)_bootsector.data())->oemID, "-FVE-FS-", 8) == 0) {
            _bitlocker.bitlocked = TRUE;
        }
        if (std::memcmp(((PBOOT_SECTOR_COMMON)_bootsector.data())->oemID, "MSWIN4.1", 8) == 0) {
            _bitlocker.bitlocked = TRUE;
        }
        if (_bitlocker.bitlocked) {
            PBOOT_SECTOR_BITLOCKER pbsb = (PBOOT_SECTOR_BITLOCKER)_bootsector.data();
            for (int block_index = 0; block_index < 3; block_index++) {
                LARGE_INTEGER fve_pos = p.StartingOffset;
                fve_pos.QuadPart += pbsb->fveBlockOffset[block_index];
                std::shared_ptr<Buffer<PBYTE>> fve;
                auto i = read_fve(h, fve_pos);
                if (i.has_value()) {
                    fve = read_fve(h, fve_pos).value();
                } else {
                    return std::unexpected(i.error().add_to_error_stack("caller", ERROR_LOC));
                }
                
                if (fve != nullptr) {
                    std::memcpy((void*)&_bitlocker.metadata[block_index].block_header, fve->data(), sizeof(FVE_BLOCK_HEADER));
                    std::memcpy((void*)&_bitlocker.metadata[block_index].header, fve->data() + sizeof(FVE_BLOCK_HEADER), sizeof(FVE_HEADER));
                    DWORD size_to_read = _bitlocker.metadata[block_index].header.size - _bitlocker.metadata[block_index].header.header_size;
                    PFVE_ENTRY entry = (PFVE_ENTRY)(fve->data() + sizeof(FVE_BLOCK_HEADER) + sizeof(FVE_HEADER));
                    
                    while (size_to_read > 0) {
                        std::shared_ptr<Buffer<PFVE_ENTRY>> entrybuf = std::make_shared<Buffer<PFVE_ENTRY>>(entry->size);
                        std::memcpy(entrybuf->data(), entry, entry->size);
                        _bitlocker.metadata[block_index].entries.push_back(entrybuf);
                        entrybuf = nullptr;
                        size_to_read -= entry->size;
                        entry = (PFVE_ENTRY)(((PBYTE)entry) + entry->size);
                    }
                }
            }
        }
    } else {
        return std::unexpected(win_error("Invalid Handle Value",ERROR_LOC));
    }
    return {};
}

std::expected<void, win_error> Volume::_get_info_by_name(PWCHAR volume_name) {
    _name = utils::strings::to_utf8(volume_name);
    ULARGE_INTEGER li;
    if (!GetDiskFreeSpaceExW(
        volume_name,
        NULL,
        NULL,
        &li
    )) {
        auto i = win_error(GetLastError(),ERROR_LOC);
        if (i.Get_First_Error_Code()!=0) {
            std::cout << i;
        }
    } else {
        _free = li.QuadPart;
    }

    wchar_t labelName[MAX_PATH + 1] = { 0 };
    wchar_t fileSystemName[MAX_PATH + 1] = { 0 };
    DWORD serialNumber = 0;
    DWORD maxComponentLen = 0;
    DWORD fileSystemFlags = 0;

    if (!GetVolumeInformationW(
        volume_name,
        labelName,
        MAX_PATH + 1,
        &serialNumber,
        &maxComponentLen,
        &fileSystemFlags,
        fileSystemName,
        ARRAYSIZE(fileSystemName)
    )) {
        auto i = win_error(GetLastError(),ERROR_LOC);
        if (i.Get_First_Error_Code()!=0) {
            std::cout << i;
        }
    } else {
        _label = utils::strings::to_utf8(labelName);
        _serial_number = serialNumber;
    }
    DWORD charCount = MAX_PATH;
    std::shared_ptr<Buffer<wchar_t*>> mps = std::make_shared<Buffer<wchar_t*>>(charCount);
    if (!GetVolumePathNamesForVolumeNameW(
        volume_name,
        mps->data(),
        charCount,
        &charCount
    )) {
        if (GetLastError() == ERROR_MORE_DATA) {
			mps->resize(charCount);
		} else {
            auto i = win_error(GetLastError(),ERROR_LOC);
            if (i.Get_First_Error_Code()!=0) {
                std::cout << i;
            }
        }
    }
    if (!GetVolumePathNamesForVolumeNameW(
        volume_name,
        mps->data(),
        charCount,
        &charCount
    )) {
        auto i = win_error(GetLastError(),ERROR_LOC);
        if (i.Get_First_Error_Code()!=0) {
            std::cout << i;
        }
    } else {
        wchar_t* letters = (wchar_t*)mps->data();
		size_t i = 0;
		while (letters[i] != NULL)
		{
			_mountpoints.push_back(utils::strings::to_utf8(letters));
			i += wcslen(letters);
		}
    }
    _type = GetDriveTypeW(volume_name);
    return {};
}

std::expected<void, win_error> Volume::_detect_filesystem(PARTITION_INFORMATION_EX p) {
    if (_filesystem == "") {
        _filesystem = "Unknown";

        PBOOT_SECTOR_COMMON pbsc = (PBOOT_SECTOR_COMMON)_bootsector.data();
        if (strncmp((PCHAR)pbsc->oemID, "NTFS", 4) == 0) {
            _filesystem = "NTFS";
        }
        else if (strncmp((PCHAR)pbsc->oemID, "-FVE-FS-", 8) == 0) {
            _filesystem = "Bitlocker";
        }
        else if (strncmp((PCHAR)pbsc->oemID, "MSDOS5.0", 8) == 0) {
            if (strncmp(((PBOOT_SECTOR_FAT32)pbsc)->fsName, "FAT32   ", 8) == 0) {
                _filesystem = "FAT32";
            }
            if (strncmp(((PBOOT_SECTOR_FAT1X)pbsc)->fsName, "FAT16   ", 8) == 0) {
                _filesystem = "FAT16";
            }
            if (strncmp(((PBOOT_SECTOR_FAT1X)pbsc)->fsName, "FAT12   ", 8) == 0) {
                _filesystem = "FAT12";
            }
        }
        else if (_partition_type == PARTITION_STYLE_MBR) {
            _filesystem = constants::disk::mbr_type(p.Mbr.PartitionType);
        }
    }
    return {};
}

Volume::Volume(HANDLE h, PARTITION_INFORMATION_EX p, int index, PVOID parent, PWCHAR name) {
	_partition_type = p.PartitionStyle;
	_offset = p.StartingOffset.QuadPart;
	_size = p.PartitionLength.QuadPart;
	_index = p.PartitionNumber;
	_serial_number = 0;
	_free = 0;
	_type = 0;
	_bootable = false;
	_bitlocker.bitlocked = false;
	_parent = parent;

	if (_partition_type == PARTITION_STYLE_MBR) {
		_bootable = p.Mbr.BootIndicator;
	} else {
		_bootable = FALSE;
	}

	if (_partition_type == PARTITION_STYLE_GPT) {
		_guid_type = constants::disk::gpt_type(p.Gpt.PartitionType);
	}

	if (_partition_type == PARTITION_STYLE_GPT || _partition_type == PARTITION_STYLE_MBR || _partition_type == PARTITION_STYLE_RAW) {
		auto i = _detect_bitlocker(h, p);
        if (!i.has_value()) {
            std::cout << i.error();
        }

		if (name) {
			auto i = _get_info_by_name(name);
            if (!i.has_value()) {
                std::cout << i.error();
            }
		} else {
			wchar_t volumeName[MAX_PATH];
			if (findVolumeName(volumeName, index, _offset, _size)) {
				auto i = _get_info_by_name(volumeName);
                if (!i.has_value()) {
                    std::cout << i.error();
                }
			}
		}

		i = _detect_filesystem(p);
        if (!i.has_value()) {
            std::cout << i.error();
        }
	}
}

DWORD Volume::disk_index() {
	if (_parent) {
        return reinterpret_cast<Disk*>(_parent)->index();
    } else {
        return 0;
    }
}