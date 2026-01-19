#include <Windows.h>
#include <cstdlib>
// #include <ios>
#include <ios>
#include <iostream>
#include <memory>
#include <set>
#include <string>

#include "mt_ntfs_mft_record.hpp"
#include "NTFS/ntfs.hpp"
#include "Utils/error.hpp"
#include "mt_ntfs_mft_walker.hpp"
#include "mt_ntfs_reader.hpp"

// marker for work (maybe make prh std::shared_ptr cuz,
// mft walker makes its own copy of the record and
// sends it here to get copied)

// mt_ntfs_mft_record::mt_ntfs_mft_record(
//     PMFT_RECORD_HEADER pRH,
//     std::shared_ptr<mt_ntfs_mft_walker> mft_walker,
//     std::shared_ptr<mt_ntfs_reader> reader,
//     uint64_t offset
// ) {
//     _reader = reader;
//     _mft_walker = mft_walker;
//     this->offset = offset;
    
//     if (pRH != NULL) {
//         _record = std::make_shared<Buffer<PMFT_RECORD_HEADER>>(_reader->sizes.record_size);
//         memcpy(_record->data(), pRH, _reader->sizes.record_size);
        
//         apply_fixups(_record->data(), _record->size(), _record->data()->updateOffset, _record->data()->updateNumber);
//     } else {
//         win_error::print(win_error("Error: empty mft record buffer", ERROR_LOC));
//     }
// }

mt_ntfs_mft_record::mt_ntfs_mft_record(
    PMFT_RECORD_HEADER pRH,
    mt_ntfs_mft_walker* mft_walker,
    std::shared_ptr<mt_ntfs_reader> reader,
    uint64_t offset
) {
    _reader = reader;
    _mft_walker = mft_walker;
    this->offset = offset;
    
    if (pRH != NULL) {
        _record = std::make_shared<Buffer<PMFT_RECORD_HEADER>>(_reader->sizes.record_size);
        memcpy(_record->data(), pRH, _reader->sizes.record_size);
        
        apply_fixups(_record->data(), _record->size(), _record->data()->updateOffset, _record->data()->updateNumber);
    } else {
        win_error::print(win_error("Error: empty mft record buffer", ERROR_LOC));
    }
}

mt_ntfs_mft_record::~mt_ntfs_mft_record() {
    _record = nullptr;
}

std::expected<std::string, win_error> attribute_str(uint32_t attr) {
    switch (attr) {
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
            return std::unexpected(win_error("NO VALID ATTRIBUTE",ERROR_LOC));
    }
}

void mt_ntfs_mft_record::apply_fixups(PVOID buffer, DWORD buffer_size, WORD updateOffset, WORD updateSize) {
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

std::expected<PMFT_RECORD_ATTRIBUTE_HEADER, win_error> mt_ntfs_mft_record::attribute_header(DWORD type, std::string name, int index) {
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
    return std::unexpected(win_error(std::string("Attribute Not Found: "+attribute_str(type).value_or("No supported attribute")),ERROR_LOC));
}

std::expected<std::wstring, win_error> mt_ntfs_mft_record::filename() {
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
        return std::unexpected(win_error("Error: Empty ret_filename", ERROR_LOC));
    }

    return ret_filename;
}

std::expected<ULONG64, win_error> mt_ntfs_mft_record::datasize(std::string stream_name, bool real_size) {
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
                    auto temp_attr_str = attribute_str(pAttr->typeID);
                    if (!temp_attr_str.has_value()) {
                        win_error::print(temp_attr_str.error().add_to_error_stack("Caller: No Attribute String", ERROR_LOC));
                    }
                    if (pAttr->typeID == $DATA) {
                        std::wstring attr_name = std::wstring(POINTER_ADD(PWCHAR, pAttr, pAttr->nameOffset));
                        attr_name.resize(pAttr->nameLength);
                        if (((pAttr->nameLength == 0) && (stream_name == "")) || ((pAttr->nameLength > 0) && (stream_name == utils::strings::to_utf8(attr_name)))) {
                            if ((pAttr->recordNumber & 0xffffffffffff) != (header()->MFTRecordIndex & 0xffffffffffff)) {
                                auto temp_record = _mft_walker->record_from_number(pAttr->recordNumber & 0xffffffffffff);
                                if (temp_record.has_value()) {
                                    std::shared_ptr<mt_ntfs_mft_record> extRecordHeader = temp_record.value();
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

std::shared_ptr<Buffer<PBYTE>> mt_ntfs_mft_record::data(std::string stream_name, bool real_size) {
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
                        auto temp_record = _mft_walker->record_from_number(pAttrListI->recordNumber & 0xffffffffffff);
                        if (!temp_record.has_value()) {
                            win_error::print(temp_record.error().add_to_error_stack("Caller: Record error", ERROR_LOC));
                        }
                        std::shared_ptr<mt_ntfs_mft_record> extRecordHeader = temp_record.value();
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

// marker for work make ibsize static or global for quick access
std::expected<std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>, win_error> mt_ntfs_mft_record::_parse_index_block(std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>> pIndexBlock, DWORD IBSize) {
    // if (pIndexBlock == nullptr) {
    //     return std::unexpected(win_error("pIndexBlock null", ERROR_LOC));
    // }
    std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK> mapVCNToIndexBlock;

    PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK pIndexSubBlockData = pIndexBlock->data();
    DWORD IndexSubBlockDataSize = pIndexBlock->size();
    DWORD blockPos = 0;

    const DWORD MIN_BLOCK_HEADER_SIZE = sizeof(MFT_RECORD_ATTRIBUTE_INDEX_BLOCK);
    
    while (blockPos + MIN_BLOCK_HEADER_SIZE < pIndexBlock->size()) {
    // while (blockPos < pIndexBlock->size()) {
        DWORD blockSize = 0;
        if (pIndexSubBlockData->Magic == MAGIC_INDX) {
            apply_fixups(pIndexSubBlockData, IndexSubBlockDataSize - blockPos, pIndexSubBlockData->OffsetOfUS, pIndexSubBlockData->SizeOfUS);
            mapVCNToIndexBlock[pIndexSubBlockData->VCN] = pIndexSubBlockData;
            blockSize = pIndexSubBlockData->AllocEntrySize + 0x18;
        } else {
            blockSize = IBSize;
        }
        // else {
        //     return std::unexpected(win_error("Found block without INDX magic", ERROR_LOC));
        // }

        // if (blockSize == 0) {
        //     return std::unexpected(win_error("Corrupt index block with zero size", ERROR_LOC));
        // }

        blockPos += blockSize;
        // blockPos += pIndexSubBlockData->AllocEntrySize + 0x18;
        pIndexSubBlockData = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK, pIndexSubBlockData, blockSize);
    }

    // if (mapVCNToIndexBlock.empty()) {
    //     win_error::print(win_error("VCN to Index block map empty, i.e., empty INDEX_ALLOCATION",ERROR_LOC));
    // }

    return mapVCNToIndexBlock;
}

std::expected<std::vector<std::shared_ptr<IndexEntry>>, win_error> parse_entries(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY pIndexEntry, std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK> vcnToBlock, std::string type) {
    std::vector<std::shared_ptr<IndexEntry>> ret;

    while (TRUE) {
        std::shared_ptr<IndexEntry> e = std::make_shared<IndexEntry>(pIndexEntry, type);

        if (pIndexEntry->Flags & MFT_ATTRIBUTE_INDEX_ENTRY_FLAG_SUBNODE) {
            auto it = vcnToBlock.find(e->vcn());
            if (it == vcnToBlock.end()) {
                return std::unexpected(win_error("Corrupt index: VCN not found in block map", ERROR_LOC));
            }
            PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK block = it->second;
            // PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK block = vcnToBlock[e->vcn()];
            if ((block != nullptr) && (block->Magic == MAGIC_INDX)) {
                PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY nextEntries = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY, block, block->EntryOffset + 0x18);
                auto i = parse_entries(nextEntries, vcnToBlock, type);
                if (!i.has_value()) {
                    return std::unexpected(i.error().add_to_error_stack("Caller", ERROR_LOC));
                }
                std::vector<std::shared_ptr<IndexEntry>> subentries = i.value();
                ret.insert(ret.end(), subentries.begin(), subentries.end());
            }
        }

        if (pIndexEntry->Flags & MFT_ATTRIBUTE_INDEX_ENTRY_FLAG_LAST) {
            if (pIndexEntry->FileReference != 0) ret.push_back(e);
            break;
        }

        ret.push_back(e);

        if (pIndexEntry->Length > 0) {
            pIndexEntry = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY, pIndexEntry, pIndexEntry->Length);
        } else {
            break;
        }
    }
    // if (ret.empty()) {
    //     win_error::print(win_error("Empty Return IndexEntry vector",ERROR_LOC));
    // }
    return ret;
}

std::expected<std::vector<std::shared_ptr<IndexEntry>>, win_error> mt_ntfs_mft_record::index() {
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
                        auto temp_record = _mft_walker->record_from_number(next_inode);
                        if (!temp_record.has_value()) {
                            win_error::print(temp_record.error().add_to_error_stack("Caller: record error", ERROR_LOC));
                        }
                        std::shared_ptr<mt_ntfs_mft_record> extRecordHeader = temp_record.value();
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

        DWORD IBSize = pAttrIndexRoot->IBSize;
    
        std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>> indexBlocks = nullptr;
        std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK> VCNToBlock;
    
        if (pAttrIndexRoot->Flags & MFT_ATTRIBUTE_INDEX_ROOT_FLAG_LARGE) {
            auto j = attribute_header($INDEX_ALLOCATION, type);
            if (!j.has_value()) {
                return std::unexpected(win_error("Attribute $INDEX_ALLOCATION not found", ERROR_LOC));
            }
            PMFT_RECORD_ATTRIBUTE_HEADER pAttrAllocation = j.value();

            indexBlocks = attribute_data<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>(pAttrAllocation);

            auto k = _parse_index_block(indexBlocks, IBSize);
            if (!k.has_value()) {
                return std::unexpected(k.error().add_to_error_stack("Caller: Parse Index block error", ERROR_LOC));
            }
            VCNToBlock = k.value();
        }
    
        auto j = parse_entries(POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY, pAttrIndexRoot, pAttrIndexRoot->EntryOffset + 0x10), VCNToBlock, type);
        if (!j.has_value()) {
            return std::unexpected(j.error().add_to_error_stack("Caller: Parse Entries error",ERROR_LOC));
        }
        ret = j.value();
    }
    
    // if (ret.size() == 0) {
    //     return std::unexpected(win_error("Error: return vector size: 0", ERROR_LOC));
    // }

    return ret;
}

std::expected<void, win_error> collect_record_ids(
    PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY pIndexEntry,
    std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>& vcnToBlock,
    std::string type,
    std::set<DWORD64>& out
) {
    if (pIndexEntry == nullptr) return std::unexpected(win_error("Error: empty pIndexEntry", ERROR_LOC));

    while (TRUE)
    {
        // Recurse into sub-nodes first (B-tree traversal)
        if (pIndexEntry->Flags & MFT_ATTRIBUTE_INDEX_ENTRY_FLAG_SUBNODE)
        {
            DWORD64 vcn = *POINTER_ADD(PLONGLONG, pIndexEntry, pIndexEntry->Length - 8);
            auto it = vcnToBlock.find(vcn);
            if (it == vcnToBlock.end()) {
                return std::unexpected(win_error("Corrupt index: VCN not found in block map", ERROR_LOC));
            }
            PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK block = it->second;
            // PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK block = vcnToBlock[vcn];
            if ((block != nullptr) && (block->Magic == MAGIC_INDX))
            {
                PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY nextEntries = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY, block, block->EntryOffset + 0x18);
                auto i = collect_record_ids(nextEntries, vcnToBlock, type, out);
                if (!i.has_value()) {
                    return std::unexpected(i.error().add_to_error_stack("Caller: collect record error", ERROR_LOC));
                }
            }
        }

        // Check for last entry
        if (pIndexEntry->Flags & MFT_ATTRIBUTE_INDEX_ENTRY_FLAG_LAST)
        {
            if (pIndexEntry->FileReference != 0)
            {
                // Extract record number based on index type
                if (type == MFT_ATTRIBUTE_INDEX_FILENAME)
                    out.insert(pIndexEntry->FileReference & 0xffffffffffff);
                else if (type == MFT_ATTRIBUTE_INDEX_REPARSE)
                    out.insert(pIndexEntry->reparse.asKeys.FileReference & 0xffffffffffff);
            }
            break;
        }

        // Add current entry's record number
        if (type == MFT_ATTRIBUTE_INDEX_FILENAME)
            out.insert(pIndexEntry->FileReference & 0xffffffffffff);
        else if (type == MFT_ATTRIBUTE_INDEX_REPARSE)
            out.insert(pIndexEntry->reparse.asKeys.FileReference & 0xffffffffffff);

        // Advance to next entry
        if (pIndexEntry->Length > 0)
            pIndexEntry = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY, pIndexEntry, pIndexEntry->Length);
        else
            break;
    }
    return {};
}

std::expected<std::set<DWORD64>, win_error> mt_ntfs_mft_record::index_records() {
    std::set<DWORD64> ret;

    std::string type = MFT_ATTRIBUTE_INDEX_FILENAME;
    auto temp_index_root = attribute_header($INDEX_ROOT, type);
    if (!temp_index_root.has_value()) {
        win_error::print(win_error("No INDEX_ROOT with name: "+type+", record id: "+std::to_string(header()->MFTRecordIndex), ERROR_LOC));
        type = MFT_ATTRIBUTE_INDEX_REPARSE;
        temp_index_root = attribute_header($INDEX_ROOT, type);
    }
    if (!temp_index_root.has_value()) {
        win_error::print(win_error("No INDEX_ROOT with name: "+type+", record id: "+std::to_string(header()->MFTRecordIndex), ERROR_LOC));
        auto j = attribute_header($ATTRIBUTE_LIST);
        if (!j.has_value()) {
            return std::unexpected(win_error("No Attribute List Found", ERROR_LOC));
        }
        PMFT_RECORD_ATTRIBUTE_HEADER pAttributeList = j.value();
        std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE>> attribute_list_data = attribute_data<PMFT_RECORD_ATTRIBUTE>(pAttributeList);
        if (attribute_list_data != nullptr) {
            DWORD offset = 0;
            while (offset <= attribute_list_data->size()) {
                PMFT_RECORD_ATTRIBUTE pAttrListI = POINTER_ADD(PMFT_RECORD_ATTRIBUTE, attribute_list_data->data(), offset);
                if (pAttrListI->typeID == $INDEX_ROOT) {
                    DWORD64 next_inode = pAttrListI->recordNumber & 0xffffffffffff;
                    if (next_inode != _record->data()->MFTRecordIndex) {
                        auto temp_record = _mft_walker->record_from_number(next_inode);
                        if (!temp_record.has_value()) {
                            win_error::print(temp_record.error().add_to_error_stack("Caller: record error", ERROR_LOC));
                        }
                        std::shared_ptr<mt_ntfs_mft_record> extRecordHeader = temp_record.value();
                        return extRecordHeader->index_records();
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
        PMFT_RECORD_ATTRIBUTE_HEADER pAttr = temp_index_root.value();
        PMFT_RECORD_ATTRIBUTE_INDEX_ROOT pAttrIndexRoot = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ROOT, pAttr, pAttr->Form.Resident.ValueOffset);

        DWORD IBSize = pAttrIndexRoot->IBSize;

        std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>> indexBlocks = nullptr;
        std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK> VCNToBlock;

        if (pAttrIndexRoot->Flags & MFT_ATTRIBUTE_INDEX_ROOT_FLAG_LARGE) {
            // PMFT_RECORD_ATTRIBUTE_HEADER pAttrAllocation;
            PMFT_RECORD_ATTRIBUTE_HEADER pAttrAllocation = nullptr;
            std::shared_ptr<mt_ntfs_mft_record> extRecordHeader = nullptr;
            auto temp_attr = attribute_header($INDEX_ALLOCATION, type);
            if (!temp_attr.has_value()) {
                win_error::print(temp_attr.error().add_to_error_stack("Attribute $INDEX_ALLOCATION not found", ERROR_LOC));
                auto temp_attrList = attribute_header($ATTRIBUTE_LIST);
                if (!temp_attrList.has_value()) {
                    return std::unexpected(temp_attrList.error().add_to_error_stack("No Attribute List Found", ERROR_LOC));
                }
                PMFT_RECORD_ATTRIBUTE_HEADER pAttributeList = temp_attrList.value();
                std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE>> attribute_list_data = attribute_data<PMFT_RECORD_ATTRIBUTE>(pAttributeList);
                if (attribute_list_data != nullptr) {
                    DWORD offset = 0;
                    while (offset <= attribute_list_data->size()) {
                        PMFT_RECORD_ATTRIBUTE pAttrListI = POINTER_ADD(PMFT_RECORD_ATTRIBUTE, attribute_list_data->data(), offset);
                        if (pAttrListI->typeID == $INDEX_ALLOCATION) {
                            std::string attr_name = utils::strings::to_utf8(std::wstring(POINTER_ADD(PWCHAR, pAttrListI, pAttrListI->nameOffset), pAttrListI->nameLength));
                            if (attr_name == type) {
                                DWORD64 next_inode = pAttrListI->recordNumber & 0xffffffffffff;
                                if (next_inode != _record->data()->MFTRecordIndex) {
                                    auto temp_record = _mft_walker->record_from_number(next_inode);
                                    if (!temp_record.has_value()) {
                                        return std::unexpected(temp_record.error().add_to_error_stack("Caller: record error", ERROR_LOC));
                                    }
                                    // std::shared_ptr<mt_ntfs_mft_record> extRecordHeader = temp_record.value();
                                    extRecordHeader = temp_record.value();
                                    auto temp_attr_IndexAllocation = extRecordHeader->attribute_header($INDEX_ALLOCATION, type);
                                    if (!temp_attr_IndexAllocation.has_value()) {
                                        return std::unexpected(temp_attr_IndexAllocation.error().add_to_error_stack("Attribute $INDEX_ALLOCATION not found", ERROR_LOC));
                                    }
                                    pAttrAllocation = temp_attr_IndexAllocation.value();
                                    break;
                                }
                            }
                        }
                        
                        if (pAttrListI->recordLength > 0) {
                            offset += pAttrListI->recordLength;
                        } else {
                            break;
                        }
                    }
                    if (pAttrAllocation == nullptr) {
                        return std::unexpected(win_error("No $INDEX_ALLOCATION Attribute found in Attribute List", ERROR_LOC));
                    }
                }
            } else {
                pAttrAllocation = temp_attr.value();
            }

            indexBlocks = attribute_data<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>(pAttrAllocation);
            auto temp_parse = _parse_index_block(indexBlocks, IBSize);
            if (!temp_parse.has_value()) {
                return std::unexpected(temp_parse.error().add_to_error_stack("Caller: Parse Index block error", ERROR_LOC));
            }
            VCNToBlock = temp_parse.value();
        }

        auto temp_collect_ids = collect_record_ids(
            POINTER_ADD(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY, pAttrIndexRoot, pAttrIndexRoot->EntryOffset + 0x10),
            VCNToBlock,
            type,
            ret);
        if (!temp_collect_ids.has_value()) {
            return std::unexpected(temp_collect_ids.error().add_to_error_stack("Caller: collect record ids error", ERROR_LOC));
        }
    }

    return ret;
}

bool mt_ntfs_mft_record::is_valid(PMFT_RECORD_HEADER pmfth) {
    return (
        (memcmp(pmfth->signature, "FILE", 4) == 0) &&
        (pmfth->attributeOffset > 0x30) &&
        (pmfth->attributeOffset < 0x400) &&
        (POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, pmfth, pmfth->attributeOffset)->TypeCode >= 10) &&
        (POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, pmfth, pmfth->attributeOffset)->TypeCode <= 100)
    );
}

bool mt_ntfs_mft_record::is_valid() {
    return (
        (memcmp(_record->data()->signature, "FILE", 4) == 0) &&
        (_record->data()->attributeOffset > 0x30) &&
        (_record->data()->attributeOffset < 0x400) &&
        (POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, _record->data(), _record->data()->attributeOffset)->TypeCode >= 10) &&
        (POINTER_ADD(PMFT_RECORD_ATTRIBUTE_HEADER, _record->data(), _record->data()->attributeOffset)->TypeCode <= 100)
    );
}

std::expected<std::vector<MFT_DATARUN>, win_error> mt_ntfs_mft_record::read_dataruns(PMFT_RECORD_ATTRIBUTE_HEADER pAttribute) {
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

std::shared_ptr<mt_ntfs_reader> mt_ntfs_mft_record::get_reader() {
    return _reader;
}

std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> mt_ntfs_mft_record::get_record() {
    return _record;
}

mt_ntfs_mft_walker* mt_ntfs_mft_record::get_mft_walker() {
    return _mft_walker;
}

// std::shared_ptr<mt_ntfs_mft_walker> mt_ntfs_mft_record::get_mft_walker() {
//     return _mft_walker;
// }

uint64_t mt_ntfs_mft_record::get_offset() {
    return offset;
}

flags mt_ntfs_mft_record::get_flags(PMFT_RECORD_ATTRIBUTE_HEADER data_attr) {
    flags ret_flags;

    if (data_attr->FormCode == RESIDENT_FORM) {
        ret_flags.is_Resident = true;
    } else if (data_attr->FormCode == NON_RESIDENT_FORM) {
        ret_flags.is_NonResident = true;
        if (data_attr->Flags & ATTRIBUTE_FLAG_COMPRESSED) {
            ret_flags.is_Compressed = true;
        } else {
            ret_flags.is_Compressed = false;
        }
    }

    return ret_flags;
}

std::string mt_ntfs_mft_record::print_flags(flags f) {
    std::string ret;
    ret += (f.is_Resident ? std::string("is Resident\n") : std::string("is Not Resident\n")) 
        +  (f.is_NonResident ? "is Non Resident\n" : "is Not Non Resident\n")
        +  (f.is_Compressed ? "is Compressed\n" : "is Not Compressed\n")
        +  (f.is_InAttributeList ? "is In Attribute List\n" : "is Not In Attribute List");

    return ret;
}