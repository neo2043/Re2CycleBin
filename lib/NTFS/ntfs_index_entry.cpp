#include <Windows.h>
#include <codecvt>
#include <winnt.h>
#include <string>

#include "ntfs_index_entry.hpp"
#include "Utils/utils.hpp"
#include "spdlog/spdlog.h"

IndexEntry::IndexEntry(PMFT_RECORD_ATTRIBUTE_INDEX_ENTRY e, std::string type) {
	_type = type;

	if (_type == MFT_ATTRIBUTE_INDEX_FILENAME) {
		_reference = e->FileReference;
		_parent_reference = e->FileName.ParentDirectory.FileRecordNumber;
		_name = std::wstring(e->FileName.Name);
		// _name = std::wstring(e->FileName.Name, e->FileName.NameLength);
		// std::wcout << "string: " << _name << " count: " << (int)e->FileName.NameLength << std::endl;
		// SPDLOG_TRACE("wstring: {}, count: {}", _name, (int)e->FileName.NameLength);
		// SPDLOG_TRACE("started");
		_name.resize(e->FileName.NameLength);
		// auto log = spdlog::get("app");
		// log->trace("wstring: {}, count: {}", std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(_name), (int)e->FileName.NameLength);
		_name_type = e->FileName.NameType;
		_flags = e->Flags;
	}
	if (_type == MFT_ATTRIBUTE_INDEX_REPARSE) {
		_reference = e->reparse.asKeys.FileReference;
		_tag = e->reparse.asKeys.ReparseTag;
	}
	_vcn = *POINTER_ADD(PLONGLONG, e, e->Length - 8);
}