#pragma once

#include <Windows.h>
#include <cstdint>
#include <variant>
#include <winternl.h>
#include <vector>
#include <memory>
#include <map>
#include <expected>
#include <algorithm>

class MFT;
class NTFSReader;
#include "ntfs.hpp"
#include "ntfs_mft.hpp"
#include "Utils/utils.hpp"
#include "Utils/error.hpp"
#include "ntfs_reader.hpp"
#include "Utils/buffer.hpp"
#include "ntfs_index_entry.hpp"
#include "cppcoro/generator.hpp"
#include "blake3.h"

#define MAGIC_FILE 0x454C4946
#define MAGIC_INDX 0x58444E49

// typedef struct flags_t{
// 	bool is_Resident = false;
// 	bool is_NonResident = false;
// 	bool is_Compressed = false;
// 	bool is_InAttributeList = false;
// } flags;

class MFTRecord {
private:
	std::shared_ptr<NTFSReader> _reader;
	std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> _record = nullptr;
	MFT* _mft = nullptr;
	std::expected<std::map<DWORD64, PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>, win_error> _parse_index_block(std::shared_ptr<Buffer<PMFT_RECORD_ATTRIBUTE_INDEX_BLOCK>> pIndexBlock);
	cppcoro::generator<std::pair<PBYTE, DWORD>> _process_data_raw(std::string stream_name = "", DWORD blocksize = 1024 * 1024, bool skip_sparse = false);
	uint64_t offset;
public:

	MFTRecord(PMFT_RECORD_HEADER pRH, MFT* mft, std::shared_ptr<NTFSReader> reader, uint64_t offset);
	~MFTRecord();

	uint64_t raw_address();

	std::expected<uint64_t, win_error> raw_address(PMFT_RECORD_ATTRIBUTE_HEADER pAttr, uint64_t offset);

	PMFT_RECORD_HEADER header() { return _record->data(); }

	void apply_fixups(PVOID buffer, DWORD buffer_size, WORD updateOffset, WORD updateSize);

	std::expected<PMFT_RECORD_ATTRIBUTE_HEADER, win_error> attribute_header(DWORD type, std::string name = "", int index = 0);

	template<typename T>
	std::shared_ptr<Buffer<T>> attribute_data(PMFT_RECORD_ATTRIBUTE_HEADER pAttributeData, bool real_size = true) {
		std::shared_ptr<Buffer<T>> ret = nullptr;

		if (pAttributeData->FormCode == RESIDENT_FORM) {
			ret = std::make_shared<Buffer<T>>(pAttributeData->Form.Resident.ValueLength);
			memcpy_s(ret->data(), ret->size(), POINTER_ADD(LPBYTE, pAttributeData, pAttributeData->Form.Resident.ValueOffset), pAttributeData->Form.Resident.ValueLength);
		} else if (pAttributeData->FormCode == NON_RESIDENT_FORM) {
			ULONGLONG readSize = 0;
			ULONGLONG filesize = pAttributeData->Form.Nonresident.FileSize;

			ret = std::make_shared<Buffer<T>>(pAttributeData->Form.Nonresident.AllocatedLength);

			bool err = false;

			auto temp_dataruns = read_dataruns(pAttributeData);
			if (!temp_dataruns.has_value()) {
				win_error::print(temp_dataruns.error().add_to_error_stack("Caller: Datarun Error", ERROR_LOC));
			}
			std::vector<MFT_DATARUN> runList = temp_dataruns.value();
			for (const MFT_DATARUN& run : runList) {
				if (err) break; //-V547

				if (run.offset == 0) {
					for (ULONGLONG i = 0; i < run.length; i++) {
						readSize += std::min<uint64_t>(filesize - readSize, _reader->sizes.cluster_size);
					}
				} else {
					// marker for work
					auto temp_seek = _reader->seek(run.offset * _reader->sizes.cluster_size);
					if (!temp_seek.has_value()) {
						win_error::print(temp_seek.error().add_to_error_stack("Caller: Seek error", ERROR_LOC));
					}
					auto temp_read = _reader->read(POINTER_ADD(PBYTE, ret->data(), DWORD(readSize)), static_cast<DWORD>(run.length) * _reader->sizes.cluster_size);
					if (!temp_read.has_value()) {
						win_error::print(temp_read.error().add_to_error_stack("Caller: Read error", ERROR_LOC));
						err=true;
						break;
					} else {
						readSize += std::min<uint64_t>(filesize - readSize, static_cast<DWORD>(run.length) * _reader->sizes.cluster_size);
					}
				}
			}
			if (readSize != filesize) {
				std::cout << "[!] Invalid read file size" << std::endl;
				ret = nullptr;
			}
			if (real_size) {
				ret->shrink(static_cast<DWORD>(filesize));
			}
		}
		return ret;
	}

	std::expected<std::wstring, win_error> filename();

	std::expected<ULONG64, win_error> datasize(std::string stream_name = "", bool real_size = true);

	std::shared_ptr<Buffer<PBYTE>> data(std::string stream_name = "", bool real_size = true);

	std::expected<ULONG64, win_error> data_to_file(std::wstring dest_filename, std::string stream_name = "", bool skip_sparse = false);

	cppcoro::generator<std::pair<PBYTE, DWORD>> process_data(std::string stream_name = "", DWORD blocksize = 1024 * 1024, bool skip_sparse = false);

	cppcoro::generator<std::pair<PBYTE, DWORD>> process_virtual_data(std::string stream_name = "", DWORD blocksize = 1024 * 1024, bool skip_sparse = false);

	std::vector<std::string> ads_names();

	std::expected<std::vector<std::shared_ptr<IndexEntry>>, win_error> index();

	static bool is_valid(PMFT_RECORD_HEADER pmfth);

	static std::expected<std::vector<MFT_DATARUN>, win_error> read_dataruns(PMFT_RECORD_ATTRIBUTE_HEADER pAttribute);

	std::shared_ptr<NTFSReader> get_reader();

	std::shared_ptr<Buffer<PMFT_RECORD_HEADER>> get_record();

	MFT* get_mft();

	bool is_valid();

	std::expected<std::pair<PMFT_RECORD_ATTRIBUTE_HEADER, uint32_t>, win_error> attribute_header_with_offset(DWORD type, std::string name = "", int index = 0);

	uint64_t get_offset();

	// cppcoro::generator<std::expected<std::variant<std::tuple<uint64_t, uint64_t, std::array<uint8_t, 32>>, flags>, win_error>> _process_raw_pointers_with_hash(blake3_hasher& hash, std::string stream_name = "");

	// cppcoro::generator<std::variant<std::pair<uint64_t, uint64_t>, flags>> _process_raw_pointers(std::string stream_name = "");

	// flags get_flags(PMFT_RECORD_ATTRIBUTE_HEADER data_attr);

	// std::string print_flags(flags f);
};