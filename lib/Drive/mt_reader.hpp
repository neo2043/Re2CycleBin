#pragma once

#include <Windows.h>
#include <cstdint>
#include <expected>
#include <string>

#include "Utils/error.hpp"

class mt_reader {
    protected:
        HANDLE _handle_disk = INVALID_HANDLE_VALUE;
        uint8_t _boot_record[512] = { 0 };
        std::wstring _volume_name;
        
    public:
        explicit mt_reader(std::wstring volume_name);
        ~mt_reader();

        mt_reader(const mt_reader&) = delete;
        mt_reader& operator=(const mt_reader&) = delete;
        mt_reader(mt_reader&&) = delete;
        mt_reader& operator=(mt_reader&&) = delete;
        
        // mt_reader(const mt_reader& reader2);
        // mt_reader& operator=(const mt_reader& e);
        HANDLE handle() const { return _handle_disk; };
        std::expected<void, win_error> read(uint64_t offset, void *buf, uint32_t size);
};