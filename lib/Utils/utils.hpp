#pragma once

#include <Windows.h>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>

#include "Compression/ntdll_defs.hpp"
#include "Utils/buffer.hpp"

#define POINTER_ADD(t, p, v)	(reinterpret_cast<t>(reinterpret_cast<uint64_t>(p) + v))

namespace utils {
	namespace convert
	{
		std::shared_ptr<Buffer<PBYTE>> from_hex(std::string s);

		std::string to_hex(PVOID buffer, unsigned long size);
	}
    namespace strings {
        void ltrim(std::string& s);

		void rtrim(std::string& s);

		void trim(std::string& s);

        std::string to_utf8(std::wstring s, DWORD encoding = CP_UTF8);

        std::wstring from_string(std::string s);

        DWORD utf8_string_size(const std::string& str);

        template <class T>
		std::string join_vec(std::vector<T> items, const std::string separator)
		{
			std::ostringstream out;
			if (items.size() > 0) out << std::string(items[0]);
			for (unsigned int i = 1; i < items.size(); i++) {
				out << separator << std::string(items[i]);
			}
			return out.str();
		}
    }
    namespace format {
        std::string size(DWORD64 size);

        std::string hex(BYTE value, bool suffix = false, bool swap = false);

		std::string hex(USHORT value, bool suffix = false, bool swap = false);

		std::string hex(ULONG32 value, bool suffix = false, bool swap = false);

		std::string hex(DWORD value, bool suffix = false, bool swap = false);

		std::string hex6(ULONG64 value, bool suffix = false, bool swap = false);

		std::string hex(ULONG64 value, bool suffix = false, bool swap = false);

		std::string hex(LONG64 value, bool suffix = false, bool swap = false);

		std::string hex(std::u16string value, bool suffix = false, bool swap = false);

		std::string hex(std::string value, bool suffix = false, bool swap = false);

		std::string hex(PBYTE value, size_t byte_size, bool suffix = false, bool swap = false);
    }
    namespace ui {
		std::string line(unsigned int length, char type = '-');

        void title(std::string s, std::ostream& out = std::cout);
    }
    namespace processes {
		BOOL elevated(HANDLE p);
	}
	namespace dll {
		namespace ntdll {
			int load_compression_functions(_RtlDecompressBuffer* RtlDecompressBuffer, _RtlDecompressBufferEx* RtlDecompressBufferEx, _RtlGetCompressionWorkSpaceSize* RtlGetCompressionWorkSpaceSize);
		}
	}
	namespace id {
		std::string guid_to_string(GUID id);

		std::string sid_to_string(PSID id);

		std::string username_from_sid(std::string sid);
	}
	namespace times {
		std::string display_systemtime(SYSTEMTIME st);

		BOOL filetime_to_systemtime(FILETIME ft, PSYSTEMTIME pST);

		BOOL ull_to_systemtime(ULONGLONG ull, PSYSTEMTIME pST);

		BOOL filetime_to_local_systemtime(FILETIME ft, PSYSTEMTIME pST);

		BOOL ull_to_local_systemtime(ULONGLONG ull, PSYSTEMTIME pST);
	}
	namespace files {
		std::pair<std::string, std::string> split_file_and_stream(std::string& str);
	}
}