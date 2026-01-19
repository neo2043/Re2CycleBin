#include <Windows.h>
#include <expected>
#include <iostream>
#include <errhandlingapi.h>
#include <minwindef.h>

#include "reader.hpp"
#include "Utils/error.hpp"

Reader::Reader(std::wstring volume_name, DWORD64 volume_offset) {
    std::wstring valid_name = volume_name;
    if (valid_name.back() == '\\') {
        valid_name.pop_back();
    }
    _handle_disk = CreateFileW(
        valid_name.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (_handle_disk == INVALID_HANDLE_VALUE) {
        auto err = win_error(GetLastError(),ERROR_LOC);
        if (err.Get_First_Error_Code()!=0) {
            win_error::print(err);
        }
    } else {
        _image_volume_offset = volume_offset;
        LARGE_INTEGER result;
        LARGE_INTEGER pos;
        pos.QuadPart = volume_offset;
        if (!SetFilePointerEx(
            _handle_disk,
            pos,
            &result,
            SEEK_SET
        )) {
            auto err = win_error(GetLastError(),ERROR_LOC);
            if (err.Get_First_Error_Code()!=0) {
                win_error::print(err);
            }
        }

        DWORD read = 0;
        auto res = seek(0);
        if (!res.has_value()) {
            win_error::print(res.error().add_to_error_stack("Caller: seek error", ERROR_LOC));
        }
        if (!ReadFile(
            _handle_disk,
            &_boot_record,
            0x200,
            &read,
            NULL
        )) {
            auto err = win_error(GetLastError(),ERROR_LOC);
            if (err.Get_First_Error_Code()!=0) {
                win_error::print(err);
            }
        }
    }
}

Reader::Reader(const Reader& reader_copy) {
    _handle_disk = INVALID_HANDLE_VALUE;

    if (!DuplicateHandle(
        GetCurrentProcess(),
        reader_copy.handle(),
        GetCurrentProcess(),
        &_handle_disk,
        DUPLICATE_SAME_ACCESS,
        TRUE,
        DUPLICATE_SAME_ACCESS
    )) {
        auto err = win_error(GetLastError(),ERROR_LOC);
        if (err.Get_First_Error_Code()!=0) {
            win_error::print(err);
        }
    }

    memcpy(_boot_record, reader_copy._boot_record, 512);
    _current_position = reader_copy._current_position;
}

Reader& Reader::operator=(const Reader& e) {
    _handle_disk = e._handle_disk;
    memcpy(_boot_record, e._boot_record, 512);
    _current_position = e._current_position;
    return *this;
}

Reader::~Reader() {
    if (_handle_disk != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(_handle_disk)) {
            auto err = win_error(GetLastError(),ERROR_LOC);
            if (err.Get_First_Error_Code()!=0) {
                std::cout << err;
            }
        }
    } else {
        std::cout << win_error("Invalid Handle Value", ERROR_LOC);
    }
}

std::expected<bool, win_error> Reader::seek(ULONG64 position) {
    if (_current_position != position) {
        if (_handle_disk != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER pos;
            pos.QuadPart = (LONGLONG)position + _image_volume_offset;
            LARGE_INTEGER result;
            BOOL res = SetFilePointerEx(
                _handle_disk,
                pos,
                &result,
                SEEK_SET
            );
            if (!res) {
              return std::unexpected(win_error(GetLastError(),ERROR_LOC));
            }
            _current_position = position;
            return res || pos.QuadPart != result.QuadPart;
        } else {
            return std::unexpected(win_error("Invalid Handle Value",ERROR_LOC));
        }
    }
    return true;
}

DWORD64 Reader::tell() const {
    return _current_position;
}

std::expected<bool, win_error> Reader::read(LPVOID lpBuffer, ULONG32 nNumberOfBytesToRead) {
    bool ret = false;
    DWORD readBytes = 0;
    if (_handle_disk != INVALID_HANDLE_VALUE) {
        ret = ReadFile(
            _handle_disk,
            lpBuffer,
            nNumberOfBytesToRead,
            &readBytes,
            NULL
        );
        if (!ret) {
            return std::unexpected(win_error(GetLastError(),ERROR_LOC));
        }
        _current_position += readBytes;
        return ret;
    }
    return std::unexpected(win_error("Invalid Handle Value",ERROR_LOC));
}