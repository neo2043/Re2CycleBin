#include <Windows.h>
#include <cstdint>
#include <expected>
#include <string>

#include "mt_reader.hpp"
#include "Utils/error.hpp"

mt_reader::mt_reader(std::wstring volume_name) {
    std::wstring valid_name = volume_name;
    if (valid_name.back() == '\\') {
        valid_name.pop_back();
    }

    _handle_disk = CreateFileW(
        valid_name.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        // FILE_FLAG_OVERLAPPED,
        FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
        nullptr
    );

    if (_handle_disk == INVALID_HANDLE_VALUE) {
        auto err = win_error(GetLastError(),ERROR_LOC);
        if (err.Get_First_Error_Code()!=0) {
            win_error::print(err);
        }
    } else {
        auto temp_read = read(0, _boot_record, 0x200);
        if (!temp_read.has_value()) {
            win_error::print(temp_read.error().add_to_error_stack("Caller: read error", ERROR_LOC));
        }
    }
}

mt_reader::~mt_reader() {
    std::cout << "mt_reader closed" << std::endl;
    if (_handle_disk != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(_handle_disk)) {
            auto err = win_error(GetLastError(),ERROR_LOC);
            if (err.Get_First_Error_Code()!=0) {
                win_error::print(err);
            }
        }
    } else {
        std::cout << win_error("Invalid Handle Value", ERROR_LOC);
    }
}

// mt_reader::mt_reader(const mt_reader& reader_copy) {
//     _handle_disk = INVALID_HANDLE_VALUE;

//     if (!DuplicateHandle(
//         GetCurrentProcess(),
//         reader_copy.handle(),
//         GetCurrentProcess(),
//         &_handle_disk,
//         DUPLICATE_SAME_ACCESS,
//         TRUE,
//         DUPLICATE_SAME_ACCESS
//     )) {
//         auto err = win_error(GetLastError(),ERROR_LOC);
//         if (err.Get_First_Error_Code()!=0) {
//             win_error::print(err);
//         }
//     }

//     memcpy(_boot_record, reader_copy._boot_record, 512);
// }

// mt_reader& mt_reader::operator=(const mt_reader& e) {
//     _handle_disk = e._handle_disk;
//     memcpy(_boot_record, e._boot_record, 512);
//     return *this;
// }

// std::expected<void, win_error> mt_reader::read(uint64_t offset, void *buf, uint32_t size, char id) {
// std::expected<void, win_error> mt_reader::read(uint64_t offset, void *buf, uint32_t size) {
//     // std::cout << "id: " << id << " offset one: " << offset ;
//     OVERLAPPED ov{};
//     ov.Offset     = DWORD(offset & 0xffffffff);
//     ov.OffsetHigh = DWORD(offset >> 32);

//     DWORD read = 0;
//     if (BOOL ok = ReadFile(
//         _handle_disk,
//         buf,
//         size,
//         nullptr,
//         &ov
//     ); !ok) {

//         DWORD err = GetLastError();
//         if (err != ERROR_IO_PENDING) {
//             return std::unexpected(win_error(err, ERROR_LOC));
//         }

//         ok = GetOverlappedResult(
//             _handle_disk,
//             &ov,
//             &read,
//             TRUE
//         );
        
//         if (!ok) {
//             return std::unexpected(win_error(GetLastError(), ERROR_LOC));
//         }
//     } else {
//         read = ov.InternalHigh;
//     }

//     std::cout << " offset two: " << offset << " read: " << read << " size: " << size << std::endl;

//     if (read != size) {
//         return std::unexpected(win_error(GetLastError(), ERROR_LOC));
//     }

//     return {};
// }

std::expected<void, win_error> mt_reader::read(uint64_t offset, void *buf, uint32_t size) {
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(
        nullptr,
        TRUE,
        FALSE,
        nullptr
    );
    if (!ov.hEvent) {
        return std::unexpected(win_error(GetLastError(), ERROR_LOC));
    }

    BYTE *pbuf = static_cast<BYTE*>(buf);
    DWORD read;

    while (size > 0) {
        ov.Offset     = static_cast<DWORD>(offset & 0xffffffff);
        ov.OffsetHigh = static_cast<DWORD>(offset >> 32);

        read = 0;
        if (BOOL ok = ReadFile(
            _handle_disk,
            pbuf,
            size,
            nullptr,
            &ov
        ); !ok) {

            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                CloseHandle(ov.hEvent);
                return std::unexpected(win_error(err, ERROR_LOC));
            }

            ok = GetOverlappedResult(
                _handle_disk,
                &ov,
                &read,
                TRUE
            );
        
            if (!ok) {
                err = GetLastError();
                CloseHandle(ov.hEvent);
                return std::unexpected(win_error(err, ERROR_LOC));
            }

        } else {
            read = ov.InternalHigh;
        }

        if (read == 0) {
            CloseHandle(ov.hEvent);
            return std::unexpected(win_error(ERROR_HANDLE_EOF, ERROR_LOC));
        }

        offset += read;
        size -= read;
        pbuf += read;
    }

    CloseHandle(ov.hEvent);
    return {};
}