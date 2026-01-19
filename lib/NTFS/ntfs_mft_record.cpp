#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <expected>
#include <utility>
#include <variant>
#include <array>
#include <algorithm>

#include "ntfs_mft_record.hpp"
#include "Utils/error.hpp"
#include "Utils/utils.hpp"
#include "Utils/buffer.hpp"
#include "NTFS/ntfs_mft.hpp"
#include "NTFS/ntfs_reader.hpp"
#include "NTFS/ntfs_index_entry.hpp"
#include "Compression/lznt1.hpp"
#include "Compression/xpress.hpp"
#include "Compression/lzx.hpp"
#include "blake3.h"

#include <cstring>   // for std::memcpy
#include <cstddef>   // for std::size_t

MFTRecord::MFTRecord(PMFT_RECORD_HEADER pRecordHeader, MFT* mft, std::shared_ptr<NTFSReader> reader, uint64_t offset) {
    _reader = reader;
    _mft = mft;
    this->offset = offset;

    if (pRecordHeader != NULL) {
        _record = std::make_shared<Buffer<PMFT_RECORD_HEADER>>(_reader->sizes.record_size);
        memcpy(_record->data(), pRecordHeader, _reader->sizes.record_size);

        apply_fixups(_record->data(), _record->size(), _record->data()->updateOffset, _record->data()->updateNumber);
    }
}

MFTRecord::~MFTRecord() {
    _record = nullptr;
}

uint64_t MFTRecord::raw_address() {
    return _reader->get_volume_offset() + (_reader->boot_record()->MFTCluster * _reader->sizes.cluster_size + (_record->data()->MFTRecordIndex * _reader->sizes.record_size));
}

std::expected<uint64_t, win_error> MFTRecord::raw_address(PMFT_RECORD_ATTRIBUTE_HEADER pAttr, uint64_t offset) {
    auto temp_dataruns = read_dataruns(pAttr);
    if (!temp_dataruns.has_value()) {
        return std::unexpected(temp_dataruns.error().add_to_error_stack("Caller: No Dataruns",ERROR_LOC));
    }

    for (auto& dt : temp_dataruns.value()) {
        if (offset >= (dt.length * _reader->sizes.cluster_size)) {
            offset -= (dt.length * _reader->sizes.cluster_size);
        } else {
            return (dt.offset * _reader->sizes.cluster_size) + offset;
        }
    }
    return std::unexpected(win_error("No raw Address, 0",ERROR_LOC));
}

// std::expected<std::string, win_error> attribute_str(uint32_t attr) {
//     switch (attr) {
//         case $STANDARD_INFORMATION:
//             return "$STANDARD_INFORMATION";
//         case $ATTRIBUTE_LIST:
//             return "$ATTRIBUTE_LIST";
//         case $FILE_NAME:
//             return "$FILE_NAME";
//         case $OBJECT_ID:
//             return "$OBJECT_ID";
//         case $SECURITY_DESCRIPTOR:
//             return "$SECURITY_DESCRIPTOR";
//         case $VOLUME_NAME:
//             return "$VOLUME_NAME";
//         case $VOLUME_INFORMATION:
//             return "$VOLUME_INFORMATION";
//         case $DATA:
//             return "$DATA";
//         case $INDEX_ROOT:
//             return "$INDEX_ROOT";
//         case $INDEX_ALLOCATION:
//             return "$INDEX_ALLOCATION";
//         case $BITMAP:
//             return "$BITMAP";
//         case $REPARSE_POINT:
//             return "$REPARSE_POINT";
//         case $EA_INFORMATION:
//             return "$EA_INFORMATION";
//         case $EA:
//             return "$EA";
//         case $LOGGED_UTILITY_STREAM:
//             return "$LOGGED_UTILITY_STREAM";
//         case $END:
//             return "$END";
//         default:
//             return std::unexpected(win_error("NO VALID ATTRIBUTE",ERROR_LOC));
//     }
// }

std::expected<ULONG64, win_error> MFTRecord::datasize(std::string stream_name, bool real_size) {
    if (_record->data()->flag & FILE_RECORD_FLAG_DIR) {
        return 0;
    }

    auto temp_data_attribute = attribute_header($DATA, stream_name);
    if (temp_data_attribute.has_value()) {
        PMFT_RECORD_ATTRIBUTE_HEADER pAttribute = temp_data_attribute.value();
        
        if (pAttribute->FormCode == RESIDENT_FORM) {
            return pAttribute->Form.Resident.ValueLength;
        } else {
            if (real_size) return pAttribute->Form.Nonresident.FileSize;
            else return pAttribute->Form.Nonresident.AllocatedLength;
        }
    } else {
        win_error::print(temp_data_attribute.error().add_to_error_stack("Caller: No Data Attribute", ERROR_LOC));
        auto temp_attribute_list = attribute_header($ATTRIBUTE_LIST);
        
        if (temp_attribute_list.has_value()) {
            PMFT_RECORD_ATTRIBUTE_HEADER pAttributeList = temp_attribute_list.value();
            
            std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE>> attribute_list_data = attribute_data<PMFT_RECORD_ATTRIBUTE>(pAttributeList);
            if (attribute_list_data != nullptr) {
                DWORD offset = 0;
                while (offset + sizeof(MFT_RECORD_ATTRIBUTE_HEADER) <= attribute_list_data->size()) {
                    PMFT_RECORD_ATTRIBUTE pAttr = POINTER_ADD(PMFT_RECORD_ATTRIBUTE, attribute_list_data->data(), offset);
                    // auto temp_attr_str = attribute_str(pAttr->typeID);
                    // if (!temp_attr_str.has_value()) {
                    //     win_error::print(temp_attr_str.error().add_to_error_stack("Caller: No Attribute String", ERROR_LOC));
                    // }
                    // std::cout << "Attribute: " << temp_attr_str.value() << std::endl;
                    if (pAttr->typeID == $DATA) {
                        std::wstring attr_name = std::wstring(POINTER_ADD(PWCHAR, pAttr, pAttr->nameOffset));
                        attr_name.resize(pAttr->nameLength);
                        if (((pAttr->nameLength == 0) && (stream_name == "")) || ((pAttr->nameLength > 0) && (stream_name == utils::strings::to_utf8(attr_name)))) {
                            if ((pAttr->recordNumber & 0xffffffffffff) != (header()->MFTRecordIndex & 0xffffffffffff)) {
                                auto temp_record = _mft->record_from_number(pAttr->recordNumber & 0xffffffffffff);
                                if (temp_record.has_value()) {
                                    std::shared_ptr<MFTRecord> extRecordHeader = temp_record.value();
                                    return extRecordHeader->datasize(stream_name, real_size);
                                } else {
                                    win_error::print(temp_record.error().add_to_error_stack("Caller: Record error", ERROR_LOC));
                                    break;
                                }
                            } else {
                                break;
                            }
                        }
                    }
                    if (pAttr->recordLength > 0) {
                        offset += pAttr->recordLength;
                    } else {
                        break;
                    }
                }
            }
        } else {
            win_error::print(temp_attribute_list.error().add_to_error_stack("Caller: No Attribute List Attribute", ERROR_LOC));
        }
    }
    
    return std::unexpected(win_error("No Datastream Found",ERROR_LOC));
}

std::expected<std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>, win_error> MFTRecord::_parse_index_block(std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>> pIndexBlock) {
    std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK> mapVCNToIndexBlock;

    PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK pIndexSubBlockData = pIndexBlock->data();
    DWORD IndexSubBlockDataSize = pIndexBlock->size();
    DWORD blockPos = 0;
    while (blockPos < pIndexBlock->size()) {
        if (pIndexSubBlockData->Magic == MAGIC_INDX) {
            apply_fixups(pIndexSubBlockData, IndexSubBlockDataSize - blockPos, pIndexSubBlockData->OffsetOfUS, pIndexSubBlockData->SizeOfUS);
            mapVCNToIndexBlock[pIndexSubBlockData->VCN] = pIndexSubBlockData;
        } 
        // else {
        //     return std::unexpected(win_error("Found block without INDX magic", ERROR_LOC));
        // }

        DWORD blockSize = pIndexSubBlockData->AllocEntrySize + 0x18;
        if (blockSize == 0) {
            return std::unexpected(win_error("Corrupt index block with zero size", ERROR_LOC));
        }

        blockPos += blockSize;
        pIndexSubBlockData = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK, pIndexSubBlockData, pIndexSubBlockData->AllocEntrySize + 0x18);
    }

    if (mapVCNToIndexBlock.empty()) {
        win_error::print(win_error("VCN to Index block map empty, i.e., empty INDEX_ALLOCATION",ERROR_LOC));
    }

    return mapVCNToIndexBlock;
}

// std::expected<std::vector<std::shared_ptr<IndexEntry>>, win_error> parse_entries(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY pIndexEntry, std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK> vcnToBlock, std::string type) {
//     std::vector<std::shared_ptr<IndexEntry>> ret;

//     while (TRUE) {
//         std::shared_ptr<IndexEntry> e = std::make_shared<IndexEntry>(pIndexEntry, type);

//         if (pIndexEntry->Flags & MFT_ATTRIBUTE_INDEX_ENTRY_FLAG_SUBNODE) {
//             auto it = vcnToBlock.find(e->vcn());
//             if (it == vcnToBlock.end()) {
//                 return std::unexpected(win_error("Corrupt index: VCN not found in block map", ERROR_LOC));
//             }
//             PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK block = it->second;
//             // PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK block = vcnToBlock[e->vcn()];
//             if ((block != nullptr) && (block->Magic == MAGIC_INDX)) {
//                 PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY nextEntries = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY, block, block->EntryOffset + 0x18);
//                 auto i = parse_entries(nextEntries, vcnToBlock, type);
//                 if (!i.has_value()) {
//                     return std::unexpected(i.error().add_to_error_stack("Caller", ERROR_LOC));
//                 }
//                 std::vector<std::shared_ptr<IndexEntry>> subentries = i.value();
//                 ret.insert(ret.end(), subentries.begin(), subentries.end());
//             }
//         }

//         if (pIndexEntry->Flags & MFT_ATTRIBUTE_INDEX_ENTRY_FLAG_LAST) {
//             if (pIndexEntry->FileReference != 0) ret.push_back(e);
//             break;
//         }

//         ret.push_back(e);

//         if (pIndexEntry->Length > 0) {
//             pIndexEntry = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY, pIndexEntry, pIndexEntry->Length);
//         } else {
//             break;
//         }
//     }
//     if (ret.empty()) {
//         win_error::print(win_error("Empty Return IndexEntry vector",ERROR_LOC));
//     }
//     return ret;
// }

std::expected<std::wstring, win_error> MFTRecord::filename() {
    auto temp_filename = attribute_header($FILE_NAME, "");
    if (!temp_filename.has_value()) {
        return std::unexpected(win_error("No Filename attribute", ERROR_LOC));
    }
    
    PMFT_RECORD_ATTRIBUTE_HEADER pattr = temp_filename.value();
    PMFT_RECORD_ATTRIBUTE_FILENAME pattr_filename = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_FILENAME, pattr, pattr->Form.Resident.ValueOffset);

    if (pattr_filename->NameType == 2) {
        auto temp_long_filename = attribute_header($FILE_NAME, "", 1);
        if (!temp_long_filename.has_value()) {
            win_error::print(temp_long_filename.error().add_to_error_stack("No second Filename Attribute",ERROR_LOC));
        }
        pattr = temp_long_filename.value();
        pattr_filename = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_FILENAME, pattr, pattr->Form.Resident.ValueOffset);
    }
    
    std::wstring ret_filename(pattr_filename->Name);
    ret_filename.resize(pattr_filename->NameLength);

    if (ret_filename.empty()) {
        win_error::print(win_error("Empty ret_filename", ERROR_LOC));
    }

    return ret_filename;
}

std::expected<std::vector<std::shared_ptr<IndexEntry>>, win_error> MFTRecord::index() {
    std::vector<std::shared_ptr<IndexEntry>> ret;
    PMFT_RECORD_ATTRIBUTE_HEADER pAttr;

    std::string type = MFT_ATTRIBUTE_INDEX_FILENAME;
    auto temp_index_root = attribute_header($INDEX_ROOT, type);
    if (!temp_index_root.has_value()) {
        win_error::print(win_error("No INDEX_ROOT with name: "+type, ERROR_LOC));
        type = MFT_ATTRIBUTE_INDEX_REPARSE;
        temp_index_root = attribute_header($INDEX_ROOT, type);
    }
    if (!temp_index_root.has_value()) {
        win_error::print(win_error("No INDEX_ROOT with name: "+type, ERROR_LOC));
        auto j = attribute_header($ATTRIBUTE_LIST);
        if (!j.has_value()) {
            return std::unexpected(win_error("No Attribute List Found", ERROR_LOC));
        }
        PMFT_RECORD_ATTRIBUTE_HEADER pAttributeList = j.value();
        std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE>> attribute_list_data = attribute_data<PMFT_RECORD_ATTRIBUTE>(pAttributeList);
        if (attribute_list_data != nullptr) {
            DWORD offset = 0;
            while (offset + sizeof(MFT_RECORD_ATTRIBUTE_HEADER) <= attribute_list_data->size()) {
                PMFT_RECORD_ATTRIBUTE pAttrListI = POINTER_ADD(PMFT_RECORD_ATTRIBUTE, attribute_list_data->data(), offset);
                if (pAttrListI->typeID == $INDEX_ROOT) {
                    DWORD64 next_inode = pAttrListI->recordNumber & 0xffffffffffff;
                    if (next_inode != _record->data()->MFTRecordIndex) {
                        auto temp_record = _mft->record_from_number(next_inode);
                        if (!temp_record.has_value()) {
                            win_error::print(temp_record.error().add_to_error_stack("Caller: record error", ERROR_LOC));
                        }
                        std::shared_ptr<MFTRecord> extRecordHeader = temp_record.value();
                        return extRecordHeader->index();
                    }
                }

                if (pAttrListI->recordLength > 0) {
                    offset += pAttrListI->recordLength;
                } else {
                    break;
                }
            }
            return std::unexpected(win_error("No $INDEX_ROOT Attribute found in Attribute List", ERROR_LOC));
        }
    }

    if (temp_index_root.has_value()) {
        pAttr = temp_index_root.value();
        PMFT_RECORD_ATTRIBUTE_INDEX_ROOT pAttrIndexRoot = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ROOT, pAttr, pAttr->Form.Resident.ValueOffset);
    
        std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>> indexBlocks = nullptr;
        std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK> VCNToBlock;
    
        if (pAttrIndexRoot->Flags & MFT_ATTRIBUTE_INDEX_ROOT_FLAG_LARGE) {
            auto j = attribute_header($INDEX_ALLOCATION, type);
            if (!j.has_value()) {
                return std::unexpected(win_error("Attribute $INDEX_ALLOCATION not found", ERROR_LOC));
            }
            PMFT_RECORD_ATTRIBUTE_HEADER pAttrAllocation = j.value();
            
            indexBlocks = attribute_data<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>(pAttrAllocation);
            auto k = _parse_index_block(indexBlocks);
            if (!k.has_value()) {
                return std::unexpected(k.error().add_to_error_stack("Caller: Parse Index block error", ERROR_LOC));
            }
            VCNToBlock = k.value();
        }
    
        // auto j = parse_entries(POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY, pAttrIndexRoot, pAttrIndexRoot->EntryOffset + 0x10), VCNToBlock, type);
        // if (!j.has_value()) {
        //     return std::unexpected(j.error().add_to_error_stack("Caller: Parse Entries error",ERROR_LOC));
        // }
        // ret = j.value();
    }
    if (ret.size() == 0) {
        return std::unexpected(win_error("Error: return vector size: 0", ERROR_LOC));
    }
    return ret;
}

bool MFTRecord::is_valid(PMFT_RECORD_HEADER pmfth) {
    return (
        (memcmp(pmfth->signature, "FILE", 4) == 0) &&
        (pmfth->attributeOffset > 0x30) &&
        (pmfth->attributeOffset < 0x400) &&
        (POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, pmfth, pmfth->attributeOffset)->TypeCode >= 10) &&
        (POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, pmfth, pmfth->attributeOffset)->TypeCode <= 100)
        );

}

bool MFTRecord::is_valid() {
    return (
        (memcmp(_record->data()->signature, "FILE", 4) == 0) &&
        (_record->data()->attributeOffset > 0x30) &&
        (_record->data()->attributeOffset < 0x400) &&
        (POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, _record->data(), _record->data()->attributeOffset)->TypeCode >= 10) &&
        (POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, _record->data(), _record->data()->attributeOffset)->TypeCode <= 100)
        );

}

std::expected<std::vector<MFT_DATARUN>, win_error> MFTRecord::read_dataruns(PMFT_RECORD_ATTRIBUTE_HEADER pAttribute) {
    if (pAttribute == nullptr || pAttribute->FormCode != NON_RESIDENT_FORM) {
        return std::unexpected(win_error("Attribute is null or not non-resident", ERROR_LOC));
    }
    
    std::vector<MFT_DATARUN> result;
    LPBYTE runList = POINTER_ADD(LPBYTE, pAttribute, pAttribute->Form.Nonresident.MappingPairsOffset);
    LONGLONG offset = 0LL;

    while (runList[0] != MFT_DATARUN_END) {
        int offset_len = runList[0] >> 4;
        int length_len = runList[0] & 0xf;
        runList++;

        ULONGLONG length = 0;
        for (int i = 0; i < length_len; i++) {
            length |= (LONGLONG)(runList++[0]) << (i * 8);
        }

        if (offset_len) {
            LONGLONG offsetDiff = 0;
            for (int i = 0; i < offset_len; i++) {
                offsetDiff |= (LONGLONG)(runList++[0]) << (i * 8);
            }

            if (offsetDiff >= (1LL << ((offset_len * 8) - 1)))
                offsetDiff -= 1LL << (offset_len * 8);

            offset += offsetDiff;
        }

        result.push_back({ offset, length });
    }
    if (result.empty()) {
        return std::unexpected(win_error("No dataruns found", ERROR_LOC));
    }

    return result;
}

void MFTRecord::apply_fixups(PVOID buffer, DWORD buffer_size, WORD updateOffset, WORD updateSize) {
    PWORD usarray = POINTER_ADD(PWORD, buffer, updateOffset);
    PWORD sector = (PWORD)buffer;

    DWORD offset = _reader->sizes.sector_size;
    for (DWORD i = 1; i < updateSize; i++) {
        if (offset <= buffer_size) {
            sector[(offset - 2) / sizeof(WORD)] = usarray[i];
            offset += _reader->sizes.sector_size;
        } else {
            break;
        }
    }
}

std::expected<PMFT_RECORD_ATTRIBUTE_HEADER, win_error> MFTRecord::attribute_header(DWORD type, std::string name, int index) {
    PMFT_RECORD_ATTRIBUTE_HEADER pAttribute = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, _record->data(), _record->data()->attributeOffset);

    while ((pAttribute->TypeCode != $END) && (pAttribute->RecordLength > 0)) {
        if (pAttribute->TypeCode == type) {
            std::string attr_name = utils::strings::to_utf8(std::wstring(POINTER_ADD(PWCHAR, pAttribute, pAttribute->NameOffset), pAttribute->NameLength));
            if (attr_name == name) {
                if (index == 0) {
                    return pAttribute;
                } else {
                    index--;
                }
            }
        }
        pAttribute = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, pAttribute, pAttribute->RecordLength);
    }
    return std::unexpected(win_error("Attribute Not Found",ERROR_LOC));
}

std::expected<std::pair<PMFT_RECORD_ATTRIBUTE_HEADER, uint32_t>, win_error> MFTRecord::attribute_header_with_offset(DWORD type, std::string name, int index) {
    uint32_t ret_offset = _record->data()->attributeOffset;
    PMFT_RECORD_ATTRIBUTE_HEADER pAttribute = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, _record->data(), _record->data()->attributeOffset);

    while ((pAttribute->TypeCode != $END) && (pAttribute->RecordLength > 0)) {
        if (pAttribute->TypeCode == type) {
            std::string attr_name = utils::strings::to_utf8(std::wstring(POINTER_ADD(PWCHAR, pAttribute, pAttribute->NameOffset), pAttribute->NameLength));
            if (attr_name == name) {
                if (index == 0) {
                    return std::pair<PMFT_RECORD_ATTRIBUTE_HEADER, uint32_t>(pAttribute,ret_offset);
                } else {
                    index--;
                }
            }
        }

        ret_offset+=pAttribute->RecordLength;
        pAttribute = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, pAttribute, pAttribute->RecordLength);
    }
    return std::unexpected(win_error("Attribute Not Found",ERROR_LOC));
}

// flags MFTRecord::get_flags(PMFT_RECORD_ATTRIBUTE_HEADER data_attr) {
//     flags ret_flags;

//     if (data_attr->FormCode == RESIDENT_FORM) {
//         ret_flags.is_Resident = true;
//     } else if (data_attr->FormCode == NON_RESIDENT_FORM) {
//         ret_flags.is_NonResident = true;
//         if (data_attr->Flags & ATTRIBUTE_FLAG_COMPRESSED) {
//             ret_flags.is_Compressed = true;
//         } else {
//             ret_flags.is_Compressed = false;
//         }
//     }

//     return ret_flags;
// }

// std::string MFTRecord::print_flags(flags f) {
//     std::string ret;
//     ret += (f.is_Resident ? std::string("is Resident\n") : std::string("is Not Resident\n")) 
//         +  (f.is_NonResident ? "is Non Resident\n" : "is Not Non Resident\n")
//         +  (f.is_Compressed ? "is Compressed\n" : "is Not Compressed\n")
//         +  (f.is_InAttributeList ? "is In Attribute List\n" : "is Not In Attribute List");

//     return ret;
// }

// std::expected<ULONG64, win_error> MFTRecord::data_to_file(std::wstring dest_filename, std::string stream_name, bool skip_sparse) {
//     ULONG64 written_bytes = 0ULL;

//     HANDLE output = CreateFileW(
//         dest_filename.c_str(), 
//         GENERIC_WRITE, 
//         0, 
//         NULL, 
//         CREATE_ALWAYS, 
//         0, 
//         NULL
//     );
//     if (output == INVALID_HANDLE_VALUE) {
//         return std::unexpected(win_error(GetLastError(),ERROR_LOC));
//     }
    
//     for (auto& data_block : process_data(stream_name, 1024 * 1024, skip_sparse)) {
//         DWORD written_block;
//         if (!WriteFile(output, data_block.first, data_block.second, &written_block, NULL)) {
//             return std::unexpected(win_error(GetLastError(),ERROR_LOC));
//         } else {
//             written_bytes += written_block;
//         }
//     }
//     CloseHandle(output);

//     return written_bytes;
// }

// cppcoro::generator<std::variant<std::pair<uint64_t, uint64_t>, flags>> MFTRecord::_process_raw_pointers(std::string stream_name) {
//     auto temp_data = attribute_header($DATA, stream_name);
//     if (temp_data.has_value()) {
//         PMFT_RECORD_ATTRIBUTE_HEADER pAttributeData = temp_data.value();

//         co_yield get_flags(pAttributeData);
        
//         if (pAttributeData->FormCode == RESIDENT_FORM) {
//             if (pAttributeData->Form.Resident.ValueOffset + pAttributeData->Form.Resident.ValueLength <= pAttributeData->RecordLength) {
//                 co_yield std::tuple<uint64_t, uint64_t>(get_offset(), _reader->sizes.record_size);
//             } else {
//                 win_error::print(win_error("Invalid size of resident data",ERROR_LOC));
//             }
//         } else if (pAttributeData->FormCode == NON_RESIDENT_FORM) {
//             auto temp_dataruns = read_dataruns(pAttributeData);
//             if (!temp_dataruns.has_value()) {
//                 win_error::print(temp_dataruns.error().add_to_error_stack("Caller: Dataruns Error", ERROR_LOC));
//             }
//             std::vector<MFT_DATARUN> data_runs = temp_dataruns.value();
//             for (const MFT_DATARUN& run : data_runs) {
//                 co_yield std::tuple<uint64_t, uint64_t>(run.offset, run.length);
//             }
//         }
//     } else {
//         win_error::print(temp_data.error().add_to_error_stack("Caller: No Data Attribute", ERROR_LOC));
//         bool data_attribute_found = false;
//         auto temp_attribute_list = attribute_header($ATTRIBUTE_LIST);
//         if (temp_attribute_list.has_value()) {
//             PMFT_RECORD_ATTRIBUTE_HEADER pAttributeList = temp_attribute_list.value();
//             std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE>> attribute_list_data = attribute_data<PMFT_RECORD_ATTRIBUTE>(pAttributeList);
//             if (attribute_list_data != nullptr) {
//                 DWORD offset = 0;
//                 while (offset + sizeof(MFT_RECORD_ATTRIBUTE) <= attribute_list_data->size()) {
//                     PMFT_RECORD_ATTRIBUTE pAttrListI = POINTER_ADD(PMFT_RECORD_ATTRIBUTE, attribute_list_data->data(), offset);
//                     if (pAttrListI->typeID == $DATA) {
//                         data_attribute_found = true;
//                         if ((pAttrListI->recordNumber & 0xffffffffffff) != (_record->data()->MFTRecordIndex & 0xffffffffffff)) {
//                             auto temp_record = _mft->record_from_number(pAttrListI->recordNumber & 0xffffffffffff);
//                             if (!temp_record.has_value()) {
//                                 win_error::print(temp_record.error().add_to_error_stack("Caller: Record error", ERROR_LOC));
//                             }
//                             std::shared_ptr<MFTRecord> extRecordHeader = temp_record.value();
//                             for (std::variant<std::pair<uint64_t, uint64_t>, flags> b : extRecordHeader->_process_raw_pointers(stream_name)) {
//                                 co_yield b;
//                             }
//                         }
//                     }
//                     offset += pAttrListI->recordLength;
//                     if (pAttrListI->recordLength == 0) {
//                         break;
//                     }
//                 }
//             }
//         } else {
//             win_error::print(temp_attribute_list.error().add_to_error_stack("Caller: No Attribute list found", ERROR_LOC));
//         }
//         if (!data_attribute_found) {
//             win_error::print(win_error("Unable to find corresponding $DATA attribute", ERROR_LOC));
//         }
//     }
// }

// cppcoro::generator<std::expected<std::variant<std::tuple<uint64_t, uint64_t, std::array<uint8_t, 32>>, flags>, win_error>> MFTRecord::_process_raw_pointers_with_hash(blake3_hasher& hash, std::string stream_name) {
//     auto temp_data = attribute_header($DATA, stream_name);
//     if (temp_data.has_value()) {
//         PMFT_RECORD_ATTRIBUTE_HEADER pAttributeData = temp_data.value();
        
//         co_yield get_flags(pAttributeData);
        
//         if (pAttributeData->FormCode == RESIDENT_FORM) {
//             if (pAttributeData->Form.Resident.ValueOffset + pAttributeData->Form.Resident.ValueLength <= pAttributeData->RecordLength) {

//                 std::array<uint8_t, BLAKE3_OUT_LEN> hash_out{};
//                 std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> buffer = std::make_shared<Buffer<PMFT_RECORD_HEADER>>(_reader->sizes.record_size);

//                 auto temp_seek = _reader->seek(get_offset());
//                 if (!temp_seek.has_value()) {
//                     co_yield std::unexpected(temp_seek.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
//                     co_return;
//                 }

//                 auto temp_read = _reader->read(buffer->address(), _reader->sizes.record_size);
//                 if (!temp_read.has_value()) {
//                     co_yield std::unexpected(temp_read.error().add_to_error_stack("Caller: Read Error", ERROR_LOC));
//                     co_return;
//                 }

//                 blake3_hasher_update(&hash, buffer->data(), buffer->size());
//                 blake3_hasher temp_hash = hash;
//                 blake3_hasher_finalize(&temp_hash, hash_out.data(), hash_out.size());

//                 co_yield std::tuple(get_offset(), _reader->sizes.record_size, hash_out);

//             } else {
//                 win_error::print(win_error("Invalid size of resident data",ERROR_LOC));
//             }
//         } else if (pAttributeData->FormCode == NON_RESIDENT_FORM) {
//             auto temp_dataruns = read_dataruns(pAttributeData);
//             if (!temp_dataruns.has_value()) {
//                 win_error::print(temp_dataruns.error().add_to_error_stack("Caller: Dataruns Error", ERROR_LOC));
//             }
//             std::vector<MFT_DATARUN> data_runs = temp_dataruns.value();

//             for (const MFT_DATARUN& run : data_runs) {

//                 std::array<uint8_t, BLAKE3_OUT_LEN> hash_out{};
//                 std::shared_ptr<Buffer<PBYTE>> buffer = std::make_shared<Buffer<PBYTE>>(static_cast<DWORD>(run.length * _reader->sizes.cluster_size));

//                 auto temp_seek = _reader->seek(run.offset * _reader->sizes.cluster_size);
//                 if (!temp_seek.has_value()) {
//                     co_yield std::unexpected(temp_seek.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
//                     co_return;
//                 }

//                 auto temp_read = _reader->read(buffer->address(), run.length * _reader->sizes.cluster_size);
//                 if (!temp_read.has_value()) {
//                     co_yield std::unexpected(temp_read.error().add_to_error_stack("Caller: Read Error", ERROR_LOC));
//                     co_return;
//                 }

//                 blake3_hasher_update(&hash, buffer->data(), buffer->size());
//                 blake3_hasher temp_hash = hash;
//                 blake3_hasher_finalize(&temp_hash, hash_out.data(), hash_out.size());
//                 co_yield std::tuple(run.offset, run.length, hash_out);
//             }
//         }
//     } else {
//         win_error::print(temp_data.error().add_to_error_stack("Caller: No Data Attribute", ERROR_LOC));
//         bool data_attribute_found = false;
//         auto temp_attribute_list = attribute_header($ATTRIBUTE_LIST);
//         if (temp_attribute_list.has_value()) {
//             PMFT_RECORD_ATTRIBUTE_HEADER pAttributeList = temp_attribute_list.value();
//             std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE>> attribute_list_data = attribute_data<PMFT_RECORD_ATTRIBUTE>(pAttributeList);
//             if (attribute_list_data != nullptr) {
//                 DWORD offset = 0;
//                 while (offset + sizeof(MFT_RECORD_ATTRIBUTE) <= attribute_list_data->size()) {
//                     PMFT_RECORD_ATTRIBUTE pAttrListI = POINTER_ADD(PMFT_RECORD_ATTRIBUTE, attribute_list_data->data(), offset);
//                     if (pAttrListI->typeID == $DATA) {
//                         data_attribute_found = true;
//                         if ((pAttrListI->recordNumber & 0xffffffffffff) != (_record->data()->MFTRecordIndex & 0xffffffffffff)) {
//                             auto temp_record = _mft->record_from_number(pAttrListI->recordNumber & 0xffffffffffff);
//                             if (!temp_record.has_value()) {
//                                 win_error::print(temp_record.error().add_to_error_stack("Caller: Record error", ERROR_LOC));
//                             }
//                             std::shared_ptr<MFTRecord> extRecordHeader = temp_record.value();
//                             for (auto b : extRecordHeader->_process_raw_pointers_with_hash(hash, stream_name)) {
//                                 if (!b.has_value()) {
//                                     co_yield std::unexpected(b.error().add_to_error_stack("Caller: _process_raw_pointers_with_hash error", ERROR_LOC));
//                                     co_return;
//                                 }
//                                 co_yield b;
//                             }
//                         }
//                     }
//                     offset += pAttrListI->recordLength;
//                     if (pAttrListI->recordLength == 0) {
//                         break;
//                     }
//                 }
//             }
//         } else {
//             co_yield std::unexpected(temp_attribute_list.error().add_to_error_stack("Caller: No Attribute list found", ERROR_LOC));
//         }
//         if (!data_attribute_found) {
//             co_yield std::unexpected(win_error("Unable to find corresponding $DATA attribute", ERROR_LOC));
//         }
//     }
// }

cppcoro::generator<std::pair<PBYTE, DWORD>> MFTRecord::_process_data_raw(std::string stream_name, DWORD block_size, bool skip_sparse) {
    auto temp_data = attribute_header($DATA, stream_name);
    if (temp_data.has_value()) {
        PMFT_RECORD_ATTRIBUTE_HEADER pAttributeData = temp_data.value();
        DWORD64 writeSize = 0;
        DWORD64 fixed_blocksize;

        if (pAttributeData->FormCode == RESIDENT_FORM) {
            if (pAttributeData->Form.Resident.ValueOffset + pAttributeData->Form.Resident.ValueLength <= pAttributeData->RecordLength) {
                PBYTE data = POINTER_ADD(PBYTE, pAttributeData, pAttributeData->Form.Resident.ValueOffset);
                for (DWORD offset = 0; offset < pAttributeData->Form.Resident.ValueLength; offset += block_size) {
                    co_yield std::pair<PBYTE, DWORD>(data + offset, std::min<uint32_t>(block_size, pAttributeData->Form.Resident.ValueLength - offset));
                }
            } else {
                win_error::print(win_error("Invalid size of resident data",ERROR_LOC));
            }
        } else if (pAttributeData->FormCode == NON_RESIDENT_FORM) {
            bool err = false;
            auto temp_dataruns = read_dataruns(pAttributeData);
            if (!temp_dataruns.has_value()) {
                win_error::print(temp_dataruns.error().add_to_error_stack("Caller: No Dataruns found", ERROR_LOC));
            }
            std::vector<MFT_DATARUN> data_runs = temp_dataruns.value();

            if (pAttributeData->Flags & ATTRIBUTE_FLAG_COMPRESSED) {
                auto expansion_factor = 0x10ULL;

                LONGLONG last_offset = 0;

                for (const MFT_DATARUN& run : data_runs) {
                    if (err) break; //-V547

                    if (last_offset == run.offset) { // Padding run
                        continue;
                    }
                    last_offset = run.offset;

                    if (run.offset == 0) {
                        Buffer<PBYTE> buffer_decompressed(static_cast<DWORD>(block_size));

                        RtlZeroMemory(buffer_decompressed.data(), block_size);
                        DWORD64 total_size = run.length * _reader->sizes.cluster_size;
                        for (DWORD64 i = 0; i < total_size; i += block_size) {
                            fixed_blocksize = DWORD(std::min<uint32_t>(pAttributeData->Form.Nonresident.FileSize - writeSize, block_size));
                            co_yield std::pair<PBYTE, DWORD>(buffer_decompressed.data(), static_cast<DWORD>(fixed_blocksize));
                            writeSize += fixed_blocksize;
                        }
                    } else {
                        auto temp_pos = _reader->seek(run.offset * _reader->sizes.cluster_size);
                        if (!temp_pos.has_value()) {
                            win_error::print(temp_pos.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
                        }
                        DWORD64 total_size = run.length * _reader->sizes.cluster_size;

                        std::shared_ptr<Buffer<PBYTE>> buffer_compressed = std::make_shared<Buffer<PBYTE>>(static_cast<DWORD>(total_size));
                        auto temp_read = _reader->read(buffer_compressed->data(), static_cast<DWORD>(total_size));
                        if (!temp_read.has_value()) {
                            win_error::print(temp_read.error().add_to_error_stack("Caller: ReadFile compressed failed", ERROR_LOC));
                            err = true;
                            break;
                        }

                        if (run.length > 0x10) { // Uncompressed
                            co_yield std::pair<PBYTE, DWORD>(buffer_compressed->data(), buffer_compressed->size());
                            writeSize += buffer_compressed->size();
                        } else {
                            std::shared_ptr<Buffer<PBYTE>> buffer_decompressed = std::make_shared<Buffer<PBYTE>>(static_cast<DWORD>(total_size * expansion_factor));

                            DWORD final_size = 0;
                            int dec_status = decompress_lznt1(buffer_compressed, buffer_decompressed, &final_size);

                            if (!dec_status) {
                                co_yield std::pair<PBYTE, DWORD>(buffer_decompressed->data(), final_size);
                                writeSize += final_size;
                            } else {
                                break;
                            }
                        }
                    }
                }
            } else if (stream_name == "WofCompressedData") {
                DWORD window_size = 0;
                DWORD is_xpress_compressed = true;

                auto temp_reparse_point = attribute_header($REPARSE_POINT);

                if (temp_reparse_point.has_value()) {
                    PMFT_RECORD_ATTRIBUTE_HEADER pAttributeHeaderRP = temp_reparse_point.value();
                    auto pAttributeRP = attribute_data<PMFT_RECORD_ATTRIBUTE_REPARSE_POINT>(pAttributeHeaderRP);
                    if (pAttributeRP->data()->ReparseTag == IO_REPARSE_TAG_WOF) {
                        switch (pAttributeRP->data()->WindowsOverlayFilterBuffer.CompressionAlgorithm) {
                            case 0: window_size = 4 * 1024; is_xpress_compressed = true; break;
                            case 1: window_size = 32 * 1024; is_xpress_compressed = false; break;
                            case 2: window_size = 8 * 1024; is_xpress_compressed = true; break;
                            case 3: window_size = 16 * 1024; is_xpress_compressed = true; break;
                            default:
                                window_size = 0;
                        }
                    }
                } else {
                    win_error::print(temp_reparse_point.error().add_to_error_stack("Caller: no Reparse of Point", ERROR_LOC));
                }

                if (window_size == 0) {
                    co_return;
                }

                std::shared_ptr<Buffer<PBYTE>> buffer_compressed = data(stream_name);
                auto temp_datasize1 = datasize("", false);
                if (!temp_datasize1.has_value()) {
                    win_error::print(temp_datasize1.error().add_to_error_stack("Caller: Datasize error", ERROR_LOC));
                }
                std::shared_ptr<Buffer<PBYTE>> buffer_decompressed = std::make_shared<Buffer<PBYTE>>(temp_datasize1.value());

                auto temp_datasize = datasize();
                if (!temp_datasize.has_value()) {
                    win_error::print(temp_datasize.error().add_to_error_stack("Datasize Error", ERROR_LOC));
                }
                DWORD final_size = static_cast<DWORD>(datasize().value());
                int dec_status = 0;

                if (is_xpress_compressed) {
                    decompress_xpress(buffer_compressed, buffer_decompressed, window_size, final_size);
                } else {
                    decompress_lzx(buffer_compressed, buffer_decompressed, window_size);
                }

                if (!dec_status) {
                    co_yield std::pair<PBYTE, DWORD>(buffer_decompressed->data(), buffer_decompressed->size());
                    writeSize += buffer_decompressed->size();
                }
            } else {
                Buffer<PBYTE> buffer(block_size);

                for (const MFT_DATARUN& run : data_runs) {
                    if (err) break;

                    // std::cout << "3[+] pointer: " << run.offset << "  length: " << run.length << std::endl;

                    if (run.offset == 0) {
                        // if (skip_sparse) {
                        //     std::cout << "Skip sparse hit" <<std::endl;
                        // }
                        if (!skip_sparse) {
                            RtlZeroMemory(buffer.data(), block_size);
                            DWORD64 total_size = run.length * _reader->sizes.cluster_size;
                            for (DWORD64 i = 0; i < total_size; i += block_size) {
                                fixed_blocksize = DWORD(std::min<uint32_t>(pAttributeData->Form.Nonresident.FileSize - writeSize, block_size));
                                co_yield std::pair<PBYTE, DWORD>(buffer.data(), static_cast<DWORD>(fixed_blocksize));
                                writeSize += fixed_blocksize;
                            }
                        }
                    } else {
                        auto temp_seek = _reader->seek(run.offset * _reader->sizes.cluster_size);
                        if (!temp_seek.has_value()) {
                            win_error::print(temp_seek.error().add_to_error_stack("Caller: Seek Error", ERROR_LOC));
                        }
                        DWORD64 total_size = run.length * _reader->sizes.cluster_size;
                        DWORD64 read_block_size = static_cast<DWORD>(std::min<uint32_t>(block_size, total_size));
                        for (DWORD64 i = 0; i < total_size; i += read_block_size) {
                            auto temp_read = _reader->read(buffer.data(), static_cast<DWORD>(read_block_size));
                            if (!temp_read.has_value()) {
                                win_error::print(temp_read.error().add_to_error_stack("Caller: Read Error", ERROR_LOC));
                                err = true;
                                break;
                            }
                            read_block_size = std::min<uint32_t>(read_block_size, total_size - i);
                            fixed_blocksize = read_block_size;
                            co_yield std::pair<PBYTE, DWORD>(buffer.data(), static_cast<DWORD>(fixed_blocksize));
                            writeSize += fixed_blocksize;
                        }
                    }
                }
            }
        }
    } else {
        win_error::print(temp_data.error().add_to_error_stack("Caller: No Data Attribute", ERROR_LOC));
        bool data_attribute_found = false;

        auto temp_attribute_list = attribute_header($ATTRIBUTE_LIST);
        if (temp_attribute_list.has_value()) {
            PMFT_RECORD_ATTRIBUTE_HEADER pAttributeList = temp_attribute_list.value();
            std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE>> attribute_list_data = attribute_data<PMFT_RECORD_ATTRIBUTE>(pAttributeList);
            if (attribute_list_data != nullptr) {
                DWORD offset = 0;

                while (offset + sizeof(MFT_RECORD_ATTRIBUTE) <= attribute_list_data->size()) {
                    PMFT_RECORD_ATTRIBUTE pAttrListI = POINTER_ADD(PMFT_RECORD_ATTRIBUTE, attribute_list_data->data(), offset);
                    if (pAttrListI->typeID == $DATA) {
                        data_attribute_found = true;

                        DWORD64 next_inode = pAttrListI->recordNumber & 0xffffffffffff;
                        if (next_inode != _record->data()->MFTRecordIndex) {
                            auto temp_record = _mft->record_from_number(pAttrListI->recordNumber & 0xffffffffffff);
                            if (!temp_record.has_value()) {
                                win_error::print(temp_record.error().add_to_error_stack("Caller: Record error", ERROR_LOC));
                            }
                            std::shared_ptr<MFTRecord> extRecordHeader = temp_record.value();
                            for (std::pair<PBYTE, DWORD> b : extRecordHeader->_process_data_raw(stream_name, block_size, skip_sparse)) {
                                co_yield b;
                            }
                        }
                    }

                    offset += pAttrListI->recordLength;

                    if (pAttrListI->recordLength == 0) {
                        break;
                    }
                }
            }
        } else {
            win_error::print(temp_attribute_list.error().add_to_error_stack("Caller: No Attribute list found", ERROR_LOC));
        }

        if (!data_attribute_found) {
            win_error::print(win_error("Unable to find corresponding $DATA attribute", ERROR_LOC));
        }
    }
}

cppcoro::generator<std::pair<PBYTE, DWORD>> MFTRecord::process_data(std::string stream_name, DWORD block_size, bool skip_sparse)
{
    auto temp_datasize = datasize(stream_name, true);
    if (!temp_datasize.has_value()) {
        win_error::print(temp_datasize.error().add_to_error_stack("Caller: Datasize error", ERROR_LOC));
    }
    ULONG64 final_datasize = temp_datasize.value();
    bool check_size = final_datasize != 0; // ex: no real size for usn

    for (auto& block : _process_data_raw(stream_name, block_size, skip_sparse)) {
        if (block.second > final_datasize && check_size) {
            block.second = static_cast<DWORD>(final_datasize);
        }

        co_yield block;

        if (check_size) {
            final_datasize -= block.second;
        }
    }
}

cppcoro::generator<std::pair<PBYTE, DWORD>> MFTRecord::process_virtual_data(std::string stream_name, DWORD block_size, bool skip_sparse)
{
    auto temp_datasize = datasize(stream_name, false);
    if (!temp_datasize.has_value()) {
        win_error::print(temp_datasize.error().add_to_error_stack("Caller: Datasize error", ERROR_LOC));
    }
    ULONG64 final_datasize = temp_datasize.value();
    bool check_size = final_datasize != 0; // ex: no real size for usn

    for (auto& block : _process_data_raw(stream_name, block_size, skip_sparse)) {
        if (block.second > final_datasize && check_size) {
            block.second = static_cast<DWORD>(final_datasize);
        }

        co_yield block;

        if (check_size) {
            final_datasize -= block.second;
        }
    }
}

std::shared_ptr<Buffer<PBYTE>> MFTRecord::data(std::string stream_name, bool real_size) {
    std::shared_ptr<Buffer<PBYTE>> ret = nullptr;

    auto temp_data = attribute_header($DATA, stream_name);

    if (temp_data.has_value()) {
        PMFT_RECORD_ATTRIBUTE_HEADER pAttributeData = temp_data.value();
        return attribute_data<PBYTE>(pAttributeData, real_size);
    } else {
        win_error::print(temp_data.error().add_to_error_stack("Caller: No Data Attribute", ERROR_LOC));
        auto temp_attribute_list = attribute_header($ATTRIBUTE_LIST);
        if (temp_attribute_list.has_value()) {
            PMFT_RECORD_ATTRIBUTE_HEADER pAttributeList = temp_attribute_list.value();
            std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE>> attribute_list_data = attribute_data<PMFT_RECORD_ATTRIBUTE>(pAttributeList, real_size);
            if (attribute_list_data != nullptr) {
                DWORD offset = 0;
                while (offset + sizeof(MFT_RECORD_ATTRIBUTE_HEADER) <= attribute_list_data->size()) {
                    PMFT_RECORD_ATTRIBUTE pAttrListI = POINTER_ADD(PMFT_RECORD_ATTRIBUTE, attribute_list_data->data(), offset);
                    if (pAttrListI->typeID == $DATA) {
                        auto temp_record = _mft->record_from_number(pAttrListI->recordNumber & 0xffffffffffff);
                        if (!temp_record.has_value()) {
                            win_error::print(temp_record.error().add_to_error_stack("Caller: Record error", ERROR_LOC));
                        }
                        std::shared_ptr<MFTRecord> extRecordHeader = temp_record.value();
                        return extRecordHeader->data(stream_name, real_size);
                    }

                    if (pAttrListI->recordLength > 0) {
                        offset += pAttrListI->recordLength;
                    } else {
                        break;
                    }
                }
            }
        } else {
            win_error::print(temp_attribute_list.error().add_to_error_stack("Caller: Unable to find $DATA attribute", ERROR_LOC));
        }
    }
    return ret;
}

std::vector<std::string> MFTRecord::ads_names() {
    std::vector<std::string> ret;

    PMFT_RECORD_ATTRIBUTE_HEADER pAttribute = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, _record->data(), _record->data()->attributeOffset);
    while (pAttribute->TypeCode != $END) {
        if (pAttribute->TypeCode == $DATA) {
            if (pAttribute->NameLength != 0) {
                std::wstring name = std::wstring(POINTER_ADD(PWCHAR, pAttribute, pAttribute->NameOffset));
                name.resize(pAttribute->NameLength);
                ret.push_back(utils::strings::to_utf8(name));
            }
        }

        if (pAttribute->RecordLength > 0) {
            pAttribute = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, pAttribute, pAttribute->RecordLength);
        } else {
            break;
        }

    }

    return ret;
}

std::shared_ptr<NTFSReader> MFTRecord::get_reader() {
    return _reader;
}

std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> MFTRecord::get_record() {
    return _record;
}

MFT* MFTRecord::get_mft() {
    return _mft;
}

uint64_t MFTRecord::get_offset() {
    return offset;
}