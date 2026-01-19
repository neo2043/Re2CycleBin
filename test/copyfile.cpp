#include "Utils/error.hpp"
#include <Windows.h>

int main() {
    if (!DeleteFileW(
        L"\\\\?\\D:\\1. Binary search - copied.txt"
    )) {
        win_error::print(win_error(GetLastError(), ERROR_LOC));
    }

    // if (!CreateHardLinkW(
    //     L"\\\\?\\D:\\1. Binary search - copied.txt",
    //     L"\\\\?\\D:\\1. Binary search.txt",
    //     nullptr
    // )) {
    //     win_error::print(win_error(GetLastError(), ERROR_LOC));
    // }
}