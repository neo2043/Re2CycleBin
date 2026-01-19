#include <Windows.h>
#include <errhandlingapi.h>
#include <iostream>

#include "Utils/error.hpp"
#include "Utils/options.hpp"
#include "Drive/disk.hpp"
#include "Utils/commands.hpp"

int main() {
    SetConsoleOutputCP(CP_UTF8);

    if (!utils::processes::elevated(GetCurrentProcess()))
    {
        std::cerr << "Administrator rights are required to read physical drives" << std::endl;
        return 1;
    }

    auto i = get_disk(1);
	if (!i.has_value()) {
		std::cout << i.error();
    }
	std::shared_ptr<Disk> disk = i.value();
	std::shared_ptr<Volume> vol = disk->volumes(3);

    if (!commands::helpers::is_ntfs(vol)) return 1;

    
    utils::ui::title("Extract file for " + disk->name() + " > Volume:" + std::to_string(vol->index()));
    
    std::cout << "[+] Opening " << (vol->name().empty() ? reinterpret_cast<Disk*>(vol->parent())->name() : vol->name()) << std::endl;
    
    std::string from = "C:/Users/AUTO/Downloads/tlor/tlor.mp4";
    // std::string from = "C:/Users/AUTO/Desktop/out.bin";
    std::string output = "C:/Users/AUTO/Desktop/new_tlor.mp4";
    std::shared_ptr<NTFSExplorer> explorer = std::make_shared<NTFSExplorer>(vol);
	auto temp_record = commands::helpers::find_record(explorer, from, 0);
    if (!temp_record.has_value()) {
        win_error::print(temp_record.error().add_to_error_stack("Caller: Record error", ERROR_LOC));
    }
	std::shared_ptr<MFTRecord> record = temp_record.value();
    std::wcout << "[+] Filename: " << record->filename().value() << std::endl;
    
    auto [filepath, stream_name] = utils::files::split_file_and_stream(from);
    
	std::cout << "[-] Record Num  : " << record->header()->MFTRecordIndex << " (" << utils::format::hex(record->header()->MFTRecordIndex, true) << ")" << std::endl;
    
    if (stream_name != "") {
        std::cout << "[-] Stream      : " << stream_name << std::endl;
	}
    
    std::cout << "[-] Destination : " << output << std::endl;
    
    PMFT_RECORD_ATTRIBUTE_STANDARD_INFORMATION stdinfo = nullptr;
	auto temp_attribute = record->attribute_header($STANDARD_INFORMATION);
	if (temp_attribute.has_value()) {
        PMFT_RECORD_ATTRIBUTE_HEADER stdinfo_att = temp_attribute.value();
		stdinfo = POINTER_ADD(PMFT_RECORD_ATTRIBUTE_STANDARD_INFORMATION, stdinfo_att, stdinfo_att->Form.Resident.ValueOffset);
	}
	if (stdinfo) {
        if (stdinfo->u.Permission.encrypted) {
            std::cout << "[!] Extracting encrypted data (not readable)" << std::endl;
		}
	}
    
    std::cout << "[+] Extracting file..." << std::endl;
	std::wstring output_filename = utils::strings::from_string(output);
    
    auto temp_written = record->data_to_file(output_filename, stream_name, true);
    if (!temp_written.has_value()) {
        win_error::print(temp_written.error().add_to_error_stack("Caller: Data Write Error", ERROR_LOC));
    }
    ULONG64 written = temp_written.value();
	std::cout << "[+] " << written << " bytes (" + utils::format::size(written) << ") written" << std::endl;
    
    if (stdinfo)
	{
        HANDLE hFile = CreateFileW(output_filename.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
            if (!SetFileTime(hFile, (FILETIME*)&stdinfo->CreateTime, (FILETIME*)&stdinfo->ReadTime, (FILETIME*)&stdinfo->AlterTime))
			{
                std::cerr << "[!] Failed to set file time" << std::endl;
			}
			CloseHandle(hFile);
		}
        
		if (!SetFileAttributesW(output_filename.c_str(), stdinfo->u.dword_part))
		{
            std::cerr << "[!] Failed to set attributes" << std::endl;
		}
	}

    // for (auto i: record->_process_raw_pointers()) {
    //     if (std::holds_alternative<std::pair<uint64_t, uint64_t>>(i)) {
    //         auto j = get<std::pair<uint64_t, uint64_t>>(i);
    //         std::cout << "[+] pointer: " << j.first << "  length: " << j.second << std::endl;
    //     } else if (std::holds_alternative<flags>(i)) {
    //         auto j = get<flags>(i);
    //         std::cout << "[+] flags: " << record->print_flags(j) << std::endl;
    //     }
    // }
}