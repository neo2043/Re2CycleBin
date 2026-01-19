#pragma once

#include <cstdint>
#include <string>
#include <Windows.h>

namespace constants {
    namespace disk {
        std::string mbr_type(uint8_t type);

        std::string gpt_type(GUID type);

        std::string media_type(MEDIA_TYPE t);

        std::string partition_type(DWORD t);

        std::string drive_type(DWORD t);

        namespace mft
        {
            std::string file_record_flags(ULONG32 f);

            std::string file_record_attribute_type(ULONG32 a);

            std::string file_record_index_root_attribute_type(ULONG32 a);

            std::string file_record_index_root_attribute_flag(ULONG32 f);

            std::string file_record_reparse_point_type(ULONG32 t);

            std::string file_record_filename_name_type(UCHAR t);

            std::string efs_type(ULONG32);

            std::string wof_compression(DWORD);
        }
    }
}