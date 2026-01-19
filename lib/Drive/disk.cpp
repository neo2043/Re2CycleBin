#include "disk.hpp"
#include "Utils/error.hpp"
#include "mbr_gpt.hpp"
#include "Utils/buffer.hpp"
#include "Utils/utils.hpp"
#include "volume.hpp"
#include "virtual_disk.hpp"

#include <Intsafe.h>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cstdio>
#include <errhandlingapi.h>
#include <expected>
#include <fileapi.h>
#include <handleapi.h>
#include <ioapiset.h>
#include <memory>
#include <minwindef.h>
#include <string>
#include <vector>
#include <winnt.h>
#include <winscard.h>
#include <iostream>
#include <wchar.h>
#include <iostream>

std::expected<std::shared_ptr<Disk>, win_error> try_add_disk(int i) {
    wchar_t diskname[MAX_PATH];
    _swprintf_p(diskname, MAX_PATH, L"\\\\.\\PhysicalDrive%d", i);
    
    HANDLE hDisk = CreateFileW(
        diskname,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        0
    );
    
    if (hDisk!=INVALID_HANDLE_VALUE) {
        std::shared_ptr<Disk> d = std::make_shared<Disk>(hDisk, i);
        CloseHandle(hDisk);
        return d;
    }
    return std::unexpected(win_error(GetLastError(),ERROR_LOC));
}

namespace core {
    namespace win {
        namespace disks {
            std::expected<std::vector<std::shared_ptr<Disk>>, win_error> list() {
                std::vector<std::shared_ptr<Disk>> disks;
                int i = 0;
                for (i = 0; ; i++) {
                    auto j = try_add_disk(i);
                    if (j.has_value()) {
                        std::shared_ptr<Disk> d = j.value();
                        disks.push_back(d);
                    } else {
                        auto k = j.error().add_to_error_stack("Caller: try_add_disk error", ERROR_LOC);
                        if (k.Get_First_Error_Code()!=0) {
                            win_error::print(k);
                        }
                        break;
                    }
                }
                int check_more_disk = i + 5;
                for (i = i + 1; i < check_more_disk; i++) {
                    auto j = try_add_disk(i);
                    if (j.has_value()) {
                        std::shared_ptr<Disk> d = j.value();
                        disks.push_back(d);
                    } else {
                        auto k = j.error().add_to_error_stack("Caller: try_add_disk error", ERROR_LOC);
                        if (k.Get_First_Error_Code()!=0) {
                            win_error::print(k);
                        }
                        break;
                    }
                }
                
                i = static_cast<int>(disks.size());
                auto j = core::win::virtualdisk::list();
                if (!j.has_value()) {
                    return std::unexpected(j.error().add_to_error_stack("Caller: virtual disk list error", ERROR_LOC));
                }
                for (auto virtual_disk : j.value()) {
                    virtual_disk->set_index(i++);
                    disks.push_back(virtual_disk);
                }

                return disks;
            }

            std::expected<std::shared_ptr<Disk>, win_error> by_index(DWORD index) {
                auto i = list();
                if (!i.has_value()) {
                    return std::unexpected(i.error().add_to_error_stack("Caller: disk list error", ERROR_LOC));
                }
                std::vector<std::shared_ptr<Disk>> disks = i.value();
				if (index < disks.size()) {
					return disks[index];
				}
				return std::unexpected(win_error("Index Out of range",ERROR_LOC));;
            }
        }
    }
}

chs add_chs(chs& a, chs& b) {
    chs r= {0};
    r.cylinder = a.cylinder + b.cylinder;
    r.head = a.head + b.head;
    r.sector = a.sector + b.sector;
    return r;
}

std::expected<void, win_error> Disk::_get_mbr(HANDLE h) {
    DWORD ior = 0;
    _protective_mbr = false;
    _partition_type = PARTITION_STYLE_RAW;
    LARGE_INTEGER pos = { {0} };
    LARGE_INTEGER result = { {0} };
    
    if (!SetFilePointerEx(
        h,
        pos,
        &result,
        SEEK_SET
    )) {
        return std::unexpected(win_error(GetLastError(),ERROR_LOC));
    }
    if (!ReadFile(
        h,
        &_mbr,
        sizeof(MBR),
        &ior,
        NULL
    )) {
        return std::unexpected(win_error(GetLastError(),ERROR_LOC));
    }
    
    int n_partitions = 0;
    for (int i = 0; i<4; i++) {
        if (_mbr.partition[i].partition_type != PARTITION_ENTRY_UNUSED) {
            n_partitions++;
        }
    }
    if (n_partitions) {
        _partition_type = PARTITION_STYLE_MBR;
    }
    _protective_mbr = (n_partitions == 1) && (_mbr.partition[0].partition_type == PARTITION_EDI_HEADER);
    if (_protective_mbr) {
        _partition_type = PARTITION_STYLE_GPT;
    }
    for (int i = 0; i < 4; i++) {
        if ((_mbr.partition[i].partition_type == PARTITION_EXTENDED) 
         || (_mbr.partition[i].partition_type == PARTITION_XINT13_EXTENDED)
        ) {
            EBR curr_ebr = { {0} };
            uint32_t last_lba = _mbr.partition[i].first_sector_lba;
            chs last_first = _mbr.partition[i].first_sector;
            chs last_last = _mbr.partition[i].last_sector;
            pos.QuadPart = (ULONG64)_mbr.partition[i].first_sector_lba * LOGICAL_SECTOR_SIZE;

            if (!SetFilePointerEx(
                h,
                pos,
                &result,
                SEEK_SET
            )) {
                return std::unexpected(win_error(GetLastError(),ERROR_LOC));
            }
            if (!ReadFile(
                h,
                &curr_ebr,
                sizeof(EBR),
                &ior,
                NULL
            )) {
                std::cout << win_error(GetLastError(),ERROR_LOC);
                break;
            }
            while (curr_ebr.mbr_signature == 0xAA55) {
                curr_ebr.partition[0].first_sector_lba = last_lba + curr_ebr.partition[0].first_sector_lba;
                curr_ebr.partition[0].first_sector = add_chs(last_first, curr_ebr.partition[0].first_sector);
                curr_ebr.partition[0].last_sector = add_chs(last_last, curr_ebr.partition[0].last_sector);
                last_lba = _mbr.partition[i].first_sector_lba + curr_ebr.partition[1].first_sector_lba;
                last_first = add_chs(_mbr.partition[i].first_sector, curr_ebr.partition[1].first_sector);
                last_last = add_chs(_mbr.partition[i].first_sector, curr_ebr.partition[1].first_sector);
                _ebrs.push_back(curr_ebr);
                if (curr_ebr.partition[1].first_sector_lba) {
                    pos.QuadPart = ((ULONG64)_mbr.partition[i].first_sector_lba + (ULONG64)curr_ebr.partition[1].first_sector_lba) * LOGICAL_SECTOR_SIZE;
                    if (!SetFilePointerEx(
                        h,
                        pos,
                        &result,
                        SEEK_SET
                    )) {
                        return std::unexpected(win_error(GetLastError(),ERROR_LOC));
                    }
                    if (!ReadFile(
                        h,
                        &curr_ebr,
                        sizeof(EBR),
                        &ior,
                        NULL
                    )) {
                        std::cout << win_error(GetLastError(),ERROR_LOC);
                        break;
                    }
                } else {
                    break;
                }
            }
        }
    }
    return {};
}

std::expected<void, win_error> Disk::_get_gpt(HANDLE h) {
    DWORD ior = 0;
    LARGE_INTEGER pos = { {0} };
    LARGE_INTEGER result = { {0} };
    GPT_HEADER loc_gpt = { {0} };

    if (_protective_mbr) {
        pos.QuadPart = (ULONG64)_mbr.partition[0].first_sector_lba * LOGICAL_SECTOR_SIZE;
        if (!SetFilePointerEx(
            h,
            pos,
            &result,
            SEEK_SET
        )) {
            return std::unexpected(win_error(GetLastError(),ERROR_LOC));
        }
        if (!ReadFile(
            h,
            &loc_gpt,
            sizeof(GPT_HEADER),
            &ior,
            NULL
        )) {
            return std::unexpected(win_error(GetLastError(),ERROR_LOC));
        }
        memcpy(&_gpt, &loc_gpt, 512);
        Buffer<PGPT_PARTITION_ENTRY> pentries(LOGICAL_SECTOR_SIZE);
        for (ULONG64 entries_offset = 0; entries_offset < 128; entries_offset += 4) {
            pos.QuadPart = 2 * LOGICAL_SECTOR_SIZE + (entries_offset / 4 * LOGICAL_SECTOR_SIZE);
            if (!SetFilePointerEx(
                h,
                pos,
                &result,
                SEEK_SET
            )) {
                return std::unexpected(win_error(GetLastError(),ERROR_LOC));
            }
            if (!ReadFile(
                h,
                pentries.data(),
                LOGICAL_SECTOR_SIZE,
                &ior,
                NULL
            )) {
                return std::unexpected(win_error(GetLastError(),ERROR_LOC));
            }
            PGPT_PARTITION_ENTRY pentry = nullptr;
            for (int i = 0; i < 4; i++) {
                pentry = pentries.data() + i;
                if (IsEqualGUID(pentry->PartitionTypeGUID, PARTITION_ENTRY_UNUSED_GUID)) {
                    return {};
                } else {
                    _gpt_entries.push_back(*pentry);
                }
            }
        }
    }
    return {};
}

std::expected<void, win_error> Disk::_get_info_using_ioctl(HANDLE h) {
    DWORD ior;
    _size=0;
    if (DeviceIoControl(
        h,
        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
        NULL,
        0,
        &_geometry,
        sizeof(DISK_GEOMETRY_EX),
        &ior,
        NULL
    )) {
        _size = _geometry.DiskSize.QuadPart;
    } else {
        LARGE_INTEGER size;
        if (!GetFileSizeEx(
            h,
            &size
        )) {
            return std::unexpected(win_error(GetLastError(),ERROR_LOC));
        }
        _size = size.QuadPart;
    }

    auto i = win_error(GetLastError(),ERROR_LOC);
    if (i.Get_First_Error_Code()!=0) {
        std::cout << i;
    }

    _is_ssd = false;
    DWORD bytesReturned = 0;
    STORAGE_PROPERTY_QUERY spq;
    spq.PropertyId = (STORAGE_PROPERTY_ID)StorageDeviceTrimProperty;
    spq.QueryType = PropertyStandardQuery;
    DEVICE_TRIM_DESCRIPTOR dtd = { 0 };

    if (!DeviceIoControl(h,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &spq,
        sizeof(spq),
        &dtd,
        sizeof(dtd),
        &bytesReturned,
        NULL
    )) {
        return std::unexpected(win_error(GetLastError(),ERROR_LOC));
    }

    _is_ssd = (dtd.TrimEnabled == TRUE);
    
    DWORD cbBytesReturned = 0;
    std::shared_ptr<Buffer<char*>> buf = std::make_shared<Buffer<char*>>(8192);
    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    if (!DeviceIoControl(
        h,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query,
        sizeof(query),
        (LPVOID)buf->data(),
        buf->size(),
        &cbBytesReturned,
        NULL
    )) {
        return std::unexpected(win_error(GetLastError(),ERROR_LOC));
    }
    PSTORAGE_DEVICE_DESCRIPTOR descrip = (PSTORAGE_DEVICE_DESCRIPTOR)buf->data();
    if (descrip->VendorIdOffset != 0) {
        _vendor_id = std::string((char*)(buf->address() + descrip->VendorIdOffset));

        if (_vendor_id.length() > 0) {
            if (_vendor_id.back() == ',') _vendor_id.pop_back();
        }
    }
    if (descrip->ProductIdOffset != 0) {
        _product_id = std::string((char*)(buf->address() + descrip->ProductIdOffset));
    }
    if (descrip->ProductRevisionOffset != 0) {
        _product_version = std::string((char*)(buf->address() + descrip->ProductRevisionOffset));
    }
    if (descrip->SerialNumberOffset != 0)
    {
        _serial_number = std::string((char*)(buf->address() + descrip->SerialNumberOffset));
        if (_serial_number.length() > 0) {
            if (_serial_number.back() == '.') {
                _serial_number.pop_back();
            }
        }
        utils::strings::trim(_serial_number);
    }
    return {};
}

std::expected<void, win_error> Disk::_get_volumes(HANDLE h) {
    DWORD partitionsSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 127 * sizeof(PARTITION_INFORMATION_EX);
    Buffer<PDRIVE_LAYOUT_INFORMATION_EX> partitions(partitionsSize);
    DWORD ior = 0;
    int res = DeviceIoControl(
        h,
        IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
        NULL,
        0,
        (LPVOID)partitions.data(),
        partitionsSize,
        &ior,
        NULL
    );
    if (!res) {
        auto i = win_error(GetLastError(),ERROR_LOC);
        if (i.Get_First_Error_Code()!=0) {
            std::cout << i;
        }
    }
    if (_index != DISK_INDEX_IMAGE && res) {
        _partition_type = partitions.data()->PartitionStyle;
        
        for (int iPart = 0; iPart < int(partitions.data()->PartitionCount); iPart++) {
            if (partitions.data()->PartitionEntry[iPart].PartitionLength.QuadPart > 0) {
                std::shared_ptr<Volume> v = std::make_shared<Volume>(h, partitions.data()->PartitionEntry[iPart], _index, this);
                if (v->name().length() > 0) {
                    _volumes.push_back(v);
                }
            }
        }
    } else {
        PMBR pmbr = mbr();
		int partition_index = 0;
		if (has_protective_mbr()) {
			auto entries = gpt_entries();
			for (auto& entry : entries) {
				PARTITION_INFORMATION_EX pex;
				pex.PartitionStyle = PARTITION_STYLE_GPT;
				pex.PartitionNumber = partition_index++;
				pex.StartingOffset.QuadPart = (LONGLONG)entry.StartingLBA * 512;
				pex.PartitionLength.QuadPart = (LONGLONG)entry.EndingLBA * 512;
				pex.Gpt.PartitionType = entry.PartitionTypeGUID;
				pex.Gpt.PartitionId = entry.UniquePartitionGUID;
				std::shared_ptr<Volume> v = std::make_shared<Volume>(h, pex, _index, this);
				_volumes.push_back(v);
			}
		} else {
            for (int i = 0; i < 4; i++) {
				if (pmbr->partition[i].partition_type != 0) {
					PARTITION_INFORMATION_EX pex;
					pex.PartitionStyle = PARTITION_STYLE_MBR;
					pex.PartitionNumber = partition_index++;
					pex.StartingOffset.QuadPart = (LONGLONG)pmbr->partition[i].first_sector_lba * 512;
					pex.PartitionLength.QuadPart = (LONGLONG)pmbr->partition[i].sectors * 512;
					pex.Mbr.BootIndicator = pmbr->partition[i].status == 0x80;
					pex.Mbr.PartitionType = pmbr->partition[i].partition_type;
                    
					if (pex.Mbr.PartitionType == 0xf) {
						for (const auto& ebr_entry : _ebrs) {
							pex.PartitionNumber = partition_index++;
							pex.StartingOffset.QuadPart = (LONGLONG)ebr_entry.partition[0].first_sector_lba * 512;
							pex.PartitionLength.QuadPart = (LONGLONG)ebr_entry.partition[0].sectors * 512;
							pex.Mbr.BootIndicator = ebr_entry.partition[0].status == 0x80;
							pex.Mbr.PartitionType = ebr_entry.partition[0].partition_type;
                            
							std::shared_ptr<Volume> v = std::make_shared<Volume>(h, pex, _index, this);
							_volumes.push_back(v);
						}
					}
					else {
						std::shared_ptr<Volume> v = std::make_shared<Volume>(h, pex, _index, this);
						_volumes.push_back(v);
					}
				}
			}
        }
    }
    return {};
}

Disk::~Disk(){}

Disk::Disk(HANDLE h, int index) {
    _index = index;
    _name = "\\\\.\\PhysicalDrive" + std::to_string(index);
    
    auto i = _get_mbr(h);
    if (!i.has_value()) {
        std::cout << i.error();
    }
    
	i = _get_gpt(h);
    if (!i.has_value()) {
        std::cout << i.error();
    }
    
	i = _get_info_using_ioctl(h);
    if (!i.has_value()) {
        std::cout << i.error();
    }
    
	i = _get_volumes(h);
    if (!i.has_value()) {
        std::cout << i.error();
    }
}

Disk::Disk(HANDLE h, std::string filename) {
	_index = DISK_INDEX_IMAGE;
	_name = filename;
    
	auto i = _get_mbr(h);
    if (!i.has_value()) {
        std::cout << i.error();
    }
    
	i = _get_gpt(h);
    if (!i.has_value()) {
        std::cout << i.error();
    }
    
	i = _get_info_using_ioctl(h);
    if (!i.has_value()) {
        std::cout << i.error();
    }
    
	i = _get_volumes(h);
    if (!i.has_value()) {
        std::cout << i.error();
    }
}

std::shared_ptr<Volume> Disk::volumes(DWORD index) const
{
	std::shared_ptr<Volume> volume = nullptr;
	for (unsigned int i = 0; i < _volumes.size(); i++)
	{
		std::shared_ptr<Volume> v = _volumes[i];
		if (v->index() == index)
		{
			volume = v;
			break;
		}
	}
	return volume;
}

HANDLE Disk::open()
{
	wchar_t diskname[MAX_PATH];
	_swprintf_p(diskname, MAX_PATH, L"\\\\.\\PhysicalDrive%d", _index);
    HANDLE i = CreateFileW(
        diskname,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, 
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        0
    );
    if (i == INVALID_HANDLE_VALUE){
        win_error j = win_error(GetLastError(),ERROR_LOC);
        if (j.Get_First_Error_Code() != 0) {
            std::cout << j;
        }
    }
	return i;
}