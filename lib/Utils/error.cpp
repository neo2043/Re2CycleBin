#include <Windows.h>
#include <cstdint>
#include <errhandlingapi.h>
#include <expected>
#include <optional>
#include <ostream>
#include <string>
#include <stringapiset.h>
#include <iostream>

#include "error.hpp"
#include "Utils/utils.hpp"

win_error::win_error(int32_t error_code, char* file_path, uint32_t file_line){
    err_instance err = {
        error_code,
        Set_Error_Code_String(error_code),
        file_line,
        file_path
    };

    error_stack.push_back(err);
}

win_error::win_error(std::string custom_error, char* file_path, uint32_t file_line) {
    err_instance err = {
        .Error_code = -1,
        .Error_String = custom_error,
        .File_Line = file_line,
        .File_Path = file_path
    };
    error_stack.push_back(err);
}

win_error::win_error(std::string sqlite_string, int32_t sqlite_return_code, std::string sqlite_error_string, char* file_path, uint32_t file_line) {
    sqlite_err_instance err = {
        .sqlite_return_code = sqlite_return_code,
        .sqlite_string = sqlite_string,
        .sqlite_error_string = sqlite_error_string,
        .File_Line = file_line,
        .File_Path = file_path
    };
    error_stack.push_back(err);
}

std::string WideToUTF8(const std::wstring& wide)
{
    if (wide.empty()) return {};

    int sizeNeeded = WideCharToMultiByte(
        CP_UTF8,                // convert to UTF-8
        0,                      // flags
        wide.c_str(),           // source
        (int)wide.size(),       // source length
        NULL, 0,                // get required size first
        NULL, NULL
    );

    std::string result(sizeNeeded, 0);

    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        (int)wide.size(),
        &result[0],
        sizeNeeded,
        NULL, NULL
    );

    result.pop_back();
    result.pop_back();
    
    return result;
}

std::string win_error::Set_Error_Code_String(DWORD error_code) {
    LPWSTR Error_Buffer = nullptr;
    std::string ret;
    int flags = FORMAT_MESSAGE_ALLOCATE_BUFFER 
                | FORMAT_MESSAGE_FROM_SYSTEM 
                | FORMAT_MESSAGE_IGNORE_INSERTS;

    const DWORD size = FormatMessageW(
        flags, 
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&Error_Buffer),
        0,
        nullptr
    );

    if (size == 0) {
        std::cout << "Unknown Error Code: " << GetLastError() << std::endl;
    } else {
        ret = WideToUTF8(std::wstring(Error_Buffer,size));
    }

    if (Error_Buffer) {
        LocalFree(Error_Buffer);
    }
    return ret;
}

std::expected<uint32_t, win_error> win_error::Get_Last_Error_Code() {
    if (std::holds_alternative<err_instance>(error_stack[error_stack.size()-1])) {
        err_instance i = get<err_instance>(error_stack[error_stack.size()-1]);
        return i.Error_code;
    } else {
        sqlite_err_instance i = get<sqlite_err_instance>(error_stack[error_stack.size()-1]);
        return i.sqlite_return_code;
    }
}

std::expected<uint32_t, win_error> win_error::Get_Last_File_Line() {
    if (std::holds_alternative<err_instance>(error_stack[error_stack.size()-1])) {
        err_instance i = get<err_instance>(error_stack[error_stack.size()-1]);
        return i.File_Line;
    } else {
        sqlite_err_instance i = get<sqlite_err_instance>(error_stack[error_stack.size()-1]);
        return i.File_Line;
    }
}

std::expected<std::optional<std::string>, win_error> win_error::Get_Last_Error_String() {
    if (std::holds_alternative<err_instance>(error_stack[error_stack.size()-1])) {
        err_instance i = get<err_instance>(error_stack[error_stack.size()-1]);
        if (i.Error_String.empty()) {
            return std::nullopt;
        }
        return i.Error_String;
    } else {
        sqlite_err_instance i = get<sqlite_err_instance>(error_stack[error_stack.size()-1]);
        if (i.sqlite_error_string.empty()) {
            return std::nullopt;
        }
        return i.sqlite_error_string;
    }
}

std::expected<std::optional<std::string>, win_error> win_error::Get_Last_File_path() {
    if (std::holds_alternative<err_instance>(error_stack[error_stack.size()-1])) {
        err_instance i = get<err_instance>(error_stack[error_stack.size()-1]);
        if (i.File_Path.empty()) {
            return std::nullopt;
        }
        return i.File_Path;
    } else {
        sqlite_err_instance i = get<sqlite_err_instance>(error_stack[error_stack.size()-1]);
        if (i.File_Path.empty()) {
            return std::nullopt;
        }
        return i.File_Path;
    }
}

std::expected<uint32_t, win_error> win_error::Get_First_Error_Code() {
    if (std::holds_alternative<err_instance>(error_stack[0])) {
        err_instance i = get<err_instance>(error_stack[0]);
        return i.Error_code;
    } else {
        sqlite_err_instance i = get<sqlite_err_instance>(error_stack[0]);
        return i.sqlite_return_code;
    }
}

std::expected<uint32_t, win_error> win_error::Get_First_File_Line() {
    if (std::holds_alternative<err_instance>(error_stack[0])) {
        err_instance i = get<err_instance>(error_stack[0]);
        return i.File_Line;
    } else {
        sqlite_err_instance i = get<sqlite_err_instance>(error_stack[0]);
        return i.File_Line;
    }
}

std::expected<std::optional<std::string>, win_error> win_error::Get_First_Error_String() {
    if (std::holds_alternative<err_instance>(error_stack[0])) {
        err_instance i = get<err_instance>(error_stack[0]);
        if (i.Error_String.empty()) {
            return std::nullopt;
        }
        return i.Error_String;
    } else {
        sqlite_err_instance i = get<sqlite_err_instance>(error_stack[0]);
        if (i.sqlite_error_string.empty()) {
            return std::nullopt;
        }
        return i.sqlite_error_string;
    }
}

std::expected<std::optional<std::string>, win_error> win_error::Get_First_File_path() {
    if (std::holds_alternative<err_instance>(error_stack[0])) {
        err_instance i = get<err_instance>(error_stack[0]);
        if (i.File_Path.empty()) {
            return std::nullopt;
        }
        return i.File_Path;
    } else {
        sqlite_err_instance i = get<sqlite_err_instance>(error_stack[0]);
        if (i.File_Path.empty()) {
            return std::nullopt;
        }
        return i.File_Path;
    }
    return std::unexpected(win_error("Error: Wrong Variant Found", ERROR_LOC));
}

std::ostream& operator<<(std::ostream& os, const win_error& werror) {
    for (const auto &err: werror.error_stack) {
        if (std::holds_alternative<err_instance>(err)) {
            err_instance error = get<err_instance>(err);
            os << error.Error_String << " : " << error.Error_code << std::endl << error.File_Path << " : " << error.File_Line << std::endl;
        } else if (std::holds_alternative<sqlite_err_instance>(err)) {
            sqlite_err_instance error = get<sqlite_err_instance>(err);
            os << error.File_Path << " : " << error.File_Line << ", SQLite Check: " << error.sqlite_string <<
            std::endl << error.sqlite_error_string << " : " << error.sqlite_return_code << std::endl ;
        }
    }
    return os;
}

win_error& win_error::add_to_error_stack(std::string custom, char* file_path, uint32_t file_line) {
    err_instance new_err = {
        -1,
        custom,
        file_line,
        file_path
    };
    error_stack.push_back(new_err);
    return *this;
}

win_error& win_error::add_to_error_stack(std::wstring wstr, char* file_path, uint32_t file_line) {
    err_instance new_err = {
        -1,
        utils::strings::to_utf8(wstr),
        file_line,
        file_path
    };
    error_stack.push_back(new_err);
    return *this;
}

win_error& win_error::add_to_error_stack(win_error err) {
    error_stack.insert(error_stack.end(), err.error_stack.begin(), err.error_stack.end());
    return *this;
}

void win_error::print(win_error err) {
    // #ifdef _DEBUG_MODE
    std::cout << err;
    // #endif
}