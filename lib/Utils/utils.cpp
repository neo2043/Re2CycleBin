#include "utils.hpp"

#include <Windows.h>
#include <filesystem>
#include <sddl.h>
#include <Intsafe.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <openssl/bn.h>
#include <tchar.h>

namespace utils {
    namespace strings {
        void ltrim(std::string& s) {
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
				return !std::isspace(ch & 0xff) && ((ch & 0xff) != 0);
				}));
		}

		void rtrim(std::string& s) {
			s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned int ch) {
				return !std::isspace(ch & 0xff) && ((ch & 0xff) != 0);
				}).base(), s.end());
		}

		void trim(std::string& s) {
			ltrim(s);
			rtrim(s);
		}

		std::string to_utf8(std::wstring ws, DWORD encoding) {
			if (ws.empty()) return "";
			int utf16len = 0;
			if (!FAILED(SizeTToInt(ws.length(), &utf16len))) {
				int utf8len = WideCharToMultiByte(
					encoding,
					0,
					ws.c_str(),
					utf16len,
					NULL,
					0,
					NULL,
					NULL
				);
				std::string ret(utf8len, 0);
				WideCharToMultiByte(
					encoding,
					0,
					ws.c_str(),
					utf16len,
					&ret[0],
					utf8len,
					0,
					0
				);
				return ret;
			}
			return "";
		}

		std::wstring from_string(std::string s) {
			std::wstring ws;
			ws.assign(s.begin(), s.end());
			return ws;
		}

		DWORD utf8_string_size(const std::string& str) {
			int q = 0;
			size_t i = 0, ix = 0;
			for (q = 0, i = 0, ix = str.length(); i < ix; i++, q++)
			{
				int c = (unsigned char)str[i];
				if (c >= 0 && c <= 127) i += 0; //-V560
				else if ((c & 0xE0) == 0xC0) i += 1;
				else if ((c & 0xF0) == 0xE0) i += 2;
				else if ((c & 0xF8) == 0xF0) i += 3;
				else return 0;
			}
			return q;
		}
    }
	namespace format  {
		std::string size(DWORD64 size) {
			double s = static_cast<double>(size);
			std::stringstream stream;
			stream << std::fixed << std::setprecision(2);
			if (s < 1024) {
				stream << s << TEXT(" byte") << (s < 2 ? TEXT("") : TEXT("s"));
				return stream.str();
			}
			s /= 1024;
			if (s < 1024) {
				stream << s << TEXT(" KiB") << (s < 2 ? TEXT("") : TEXT("s"));
				return stream.str();
			}
			s /= 1024;
			if (s < 1024) {
				stream << s << TEXT(" MiB") << (s < 2 ? TEXT("") : TEXT("s"));
				return stream.str();
			}
			s /= 1024;
			if (s < 1024) {
				stream << s << TEXT(" GiB") << (s < 2 ? TEXT("") : TEXT("s"));
				return stream.str();
			}
			s /= 1024;
			if (s < 1024) {
				stream << s << TEXT(" TiB") << (s < 2 ? TEXT("") : TEXT("s"));
				return stream.str();
			}
			s /= 1024;
			stream << s << TEXT(" PiB") << (s < 2 ? TEXT("") : TEXT("s"));
			return stream.str();
		}

		std::string hex(BYTE value, bool suffix, bool swap) {
			std::ostringstream os;
			os << std::hex << std::setw(2) << std::setfill('0') << (ULONG32)value << std::dec;
			if (suffix) {
				os << "h";
			}
			return os.str();
		}

		std::string hex(USHORT value, bool suffix, bool swap) {
			if (swap) {
				value = _byteswap_ushort(value);
			}
			std::ostringstream os;
			os << std::hex << std::setw(4) << std::setfill('0') << value << std::dec;
			if (suffix) {
				os << "h";
			}
			return os.str();
		}

		std::string hex(ULONG32 value, bool suffix, bool swap) {
			if (swap) {
				value = _byteswap_ulong(value);
			}
			std::ostringstream os;
			os << std::hex << std::setw(8) << std::setfill('0') << value << std::dec;
			if (suffix) {
				os << "h";
			}
			return os.str();
		}

		std::string hex(DWORD value, bool suffix, bool swap) {
			return  hex((ULONG32)value, suffix, swap);
		}

		std::string hex6(ULONG64 value, bool suffix, bool swap) {
			if (swap) {
				value = _byteswap_uint64(value);
			}
			std::ostringstream os;
			os << std::hex << std::setw(12) << std::setfill('0') << value << std::dec;
			if (suffix) {
				os << "h";
			}
			return os.str();
		}

		std::string hex(ULONG64 value, bool suffix, bool swap) {
			if (swap) {
				value = _byteswap_uint64(value);
			}
			std::ostringstream os;
			os << std::hex << std::setw(16) << std::setfill('0') << value << std::dec;
			if (suffix) {
				os << "h";
			}
			return os.str();
		}

		std::string hex(LONG64 value, bool suffix, bool swap) {
			return hex((ULONG64)value, suffix, swap);
		}

		std::string hex(std::u16string value, bool suffix, bool swap) {
			return hex(((PBYTE)value.c_str()), value.size() * sizeof(char16_t), suffix, swap);
		}

		std::string hex(std::string value, bool suffix, bool swap) {
			return hex(((PBYTE)value.c_str()), value.size(), suffix, swap);
		}

		std::string hex(PBYTE value, size_t byte_size, bool suffix, bool swap) {
			std::ostringstream os;
			if (swap) {
				for (size_t i = 0; i < byte_size; i++) {
					os << std::setfill('0') << std::setw(2) << std::hex << (ULONG)value[byte_size - i - 1];
				}
			}
			else {
				for (size_t i = 0; i < byte_size; i++) {
					os << std::setfill('0') << std::setw(2) << std::hex << (ULONG)value[i];
				}
			}

			os << std::dec;
			if (suffix) {
				os << "h";
			}
			return os.str();
		}
	}
	namespace ui {
		std::string line(unsigned int length, char type) {
			std::string s;
			for (unsigned int i = 0; i < length; i++) s += type;
			return s;
		}
		void title(std::string s, std::ostream& out) {
			std::cout << std::setfill('0');
			out << s << std::endl;
			out << line(utils::strings::utf8_string_size(s));
			out << std::endl;
			out << std::endl;
		}
	}
	namespace processes {
		BOOL elevated(HANDLE p) {
			DWORD dwSize = 0;
			HANDLE hToken = NULL;
			TOKEN_ELEVATION tokenInformation;
			BOOL elevated = FALSE;

			if (OpenProcessToken(p, TOKEN_QUERY, &hToken)) {
				if (GetTokenInformation(hToken, TokenElevation, &tokenInformation, sizeof(TOKEN_ELEVATION), &dwSize))
					elevated = tokenInformation.TokenIsElevated;
				CloseHandle(hToken);
			}

			return elevated;
		}
	}
	namespace convert
    {
    	std::string to_base64(const char* in, size_t in_len)
    	{
    		std::string ret;
    		int i = 0;
    		unsigned char char_array_3[3];
    		unsigned char char_array_4[4];
    		const std::string base64_chars =
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"abcdefghijklmnopqrstuvwxyz"
				"0123456789+/";

    		while (in_len--)
    		{
    			char_array_3[i++] = *(in++);
    			if (i == 3)
    			{
    				char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    				char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    				char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    				char_array_4[3] = char_array_3[2] & 0x3f;

    				for (i = 0; (i < 4); i++)
    					ret += base64_chars[char_array_4[i]];
    				i = 0;
    			}
    		}

    		if (i)
    		{
    			for (int j = i; j < 3; j++)
    				char_array_3[j] = '\0';

    			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    			char_array_4[3] = char_array_3[2] & 0x3f;

    			for (int j = 0; (j < i + 1); j++)
    				ret += base64_chars[char_array_4[j]];

    			while ((i++ < 3))
    				ret += '=';
    		}

    		return ret;
    	}

    	std::string to_base64(std::string s)
    	{
    		return to_base64(s.c_str(), s.length());
    	}

    	std::shared_ptr<Buffer<PBYTE>> from_hex(std::string s)
    	{
    		BIGNUM* input = BN_new();
    		int input_length = BN_hex2bn(&input, s.c_str());
    		std::shared_ptr<Buffer<PBYTE>> ret = std::make_shared<Buffer<PBYTE>>((input_length + 1) / 2);
    		BN_bn2bin(input, ret->data());
    		return ret;
    	}

    	std::string to_hex(PVOID buffer, unsigned long size)
    	{
    		std::ostringstream ret;
    		PBYTE buf = reinterpret_cast<PBYTE>(buffer);
    		if (buffer != nullptr)
    		{
    			for (unsigned int i = 0; i < size; i++)
    			{
    				ret << "0123456789ABCDEF"[buf[i] >> 4] << "0123456789ABCDEF"[buf[i] & 0x0F];
    			}
    		}
    		return ret.str();
    	}
    }
	namespace id
    {
    	std::string guid_to_string(GUID guid)
    	{
    		char guid_cstr[64];
    		snprintf(guid_cstr, sizeof(guid_cstr),
				"{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
				guid.Data1, guid.Data2, guid.Data3,
				guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
				guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

    		return std::string(guid_cstr);
    	}
    	std::string sid_to_string(PSID pid)
    	{
    		std::string ret;
    		LPSTR psSid = NULL;
    		if (ConvertSidToStringSidA(pid, &psSid))
    		{
    			ret = std::string(psSid);
    			LocalFree(psSid);
    			return ret;
    		}
    		else
    		{
    			return "Converting SID failed";
    		}
    	}

    	std::string username_from_sid(std::string sid)
    	{
    		char oname[512] = { 0 };
    		char doname[512] = { 0 };
    		DWORD namelen = 512;
    		DWORD domainnamelen = 512;

    		SID_NAME_USE peUse;

    		PSID psid = nullptr;
    		std::string username = "";
    		if (ConvertStringSidToSidA(sid.c_str(), &psid))
    		{
    			if (LookupAccountSidA(NULL, psid, oname, &namelen, doname, &domainnamelen, &peUse))
    			{
    				if (strnlen_s(oname, 512) > 0 && strnlen_s(doname, 512) > 0)
    				{
    					username = std::string(doname, domainnamelen) + "/" + std::string(oname, namelen);
    				}
    			}
    			FreeSid(psid);
    		}
    		return username;
    	}
    }
	namespace times {


    	std::string display_systemtime(SYSTEMTIME st)
    	{
    		TCHAR buf[64] = { 0 };
    		_stprintf_s(buf, TEXT("%04u-%02u-%02u %02u:%02u:%02u"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    		return std::string(buf);
    	}

    	BOOL filetime_to_systemtime(FILETIME ft, PSYSTEMTIME pST)
    	{
    		return FileTimeToSystemTime(&ft, pST);
    	}

    	BOOL ull_to_systemtime(ULONGLONG ull, PSYSTEMTIME pST)
    	{
    		FILETIME ft;
    		ft.dwLowDateTime = (DWORD)(ull & 0xFFFFFFFF);
    		ft.dwHighDateTime = (DWORD)(ull >> 32);
    		return filetime_to_systemtime(ft, pST);
    	}

    	BOOL filetime_to_local_systemtime(FILETIME ft, PSYSTEMTIME pST)
    	{
    		FILETIME local;
    		if (FileTimeToLocalFileTime(&ft, &local))
    		{
    			if (FileTimeToSystemTime(&local, pST))
    			{
    				return TRUE;
    			}
    		}
    		return FALSE;
    	}

    	BOOL ull_to_local_systemtime(ULONGLONG ull, PSYSTEMTIME pST)
    	{
    		FILETIME ft;
    		ft.dwLowDateTime = (DWORD)(ull & 0xFFFFFFFF);
    		ft.dwHighDateTime = (DWORD)(ull >> 32);
    		return filetime_to_local_systemtime(ft, pST);
    	}
    }
	namespace files {
		std::pair<std::string, std::string> split_file_and_stream(std::string& str) {
			std::filesystem::path p(str);
			std::string fname = str;

			size_t ads_sep = p.filename().string().find(':');
			std::string stream_name = "";
			if (ads_sep != std::string::npos)
			{
				stream_name = p.filename().string().substr(ads_sep + 1);
				size_t last_sep = fname.find_last_of(':');
				fname = fname.substr(0, last_sep);
			}

			return std::make_pair(fname, stream_name);
		}
	}
}

int utils::dll::ntdll::load_compression_functions(_RtlDecompressBuffer* RtlDecompressBuffer, _RtlDecompressBufferEx* RtlDecompressBufferEx, _RtlGetCompressionWorkSpaceSize* RtlGetCompressionWorkSpaceSize) {
	auto ntdll = GetModuleHandle("ntdll.dll");
	if (ntdll != nullptr) {
		if (RtlDecompressBuffer) {
			*RtlDecompressBuffer = (_RtlDecompressBuffer)GetProcAddress(ntdll, "RtlDecompressBuffer");
			if (*RtlDecompressBuffer == nullptr) {
				return 4;
			}
		}

		if (RtlGetCompressionWorkSpaceSize) {
			*RtlGetCompressionWorkSpaceSize = (_RtlGetCompressionWorkSpaceSize)GetProcAddress(ntdll, "RtlGetCompressionWorkSpaceSize");
			if (*RtlGetCompressionWorkSpaceSize == nullptr) {
				return 3;
			}
		}

		if (RtlDecompressBufferEx) {
			*RtlDecompressBufferEx = (_RtlDecompressBufferEx)GetProcAddress(ntdll, "RtlDecompressBufferEx");
			if (*RtlDecompressBufferEx == nullptr) {
				return 2;
			}
		}
	}
	else {
		return 1;
	}
	return 0;
}