#pragma once

#include <expected>
#include <optional>
#include <Intsafe.h>
#include <cstdint>
#include <exception>
#include <string>
#include <variant>
#include <vector>
#include <iostream>

#define ERROR_LOC (char*)__FILE__, (uint32_t)__LINE__

#define SQLITE_ERROR_CHECK(expr, DB)                                                                                                                           \
    do {                                                                                                                                                       \
        int status = expr;                                                                                                                                     \
        if (!((status == SQLITE_OK) || (status == SQLITE_DONE) || (status == SQLITE_ROW))) {                                                                   \
            return std::unexpected(win_error(#expr, status, (DB!=nullptr) ? sqlite3_errmsg(DB) : "Null DB Pointer", ERROR_LOC));                               \
        }                                                                                                                                                      \
    } while (0)

typedef struct err_instance_t {
    int32_t Error_code;
    std::string Error_String;
    uint32_t File_Line;
    std::string File_Path;
} err_instance;

typedef struct sqlite_err_instance_t {
    int32_t sqlite_return_code;
    std::string sqlite_string;
    std::string sqlite_error_string;
    uint32_t File_Line;
    std::string File_Path;
} sqlite_err_instance;

class win_error: public std::exception {
    private:
        std::vector<std::variant<err_instance, sqlite_err_instance>> error_stack;
        std::string Set_Error_Code_String(DWORD error_code);
    public:
        win_error(int32_t error_code, char* file_path, uint32_t file_line);
        win_error(std::string custom_error, char* file_path, uint32_t file_line);
        win_error(std::string sqlite_string, int32_t sqlite_return_code, std::string custom_error, char* file_path, uint32_t file_line);
        std::expected<uint32_t, win_error> Get_First_Error_Code();
        std::expected<uint32_t, win_error> Get_Last_Error_Code();
        std::expected<std::optional<std::string>, win_error> Get_First_Error_String();
        std::expected<std::optional<std::string>, win_error> Get_Last_Error_String();
        std::expected<std::optional<std::string>, win_error> Get_First_File_path();
        std::expected<std::optional<std::string>, win_error> Get_Last_File_path();
        std::expected<uint32_t, win_error> Get_First_File_Line();
        std::expected<uint32_t, win_error> Get_Last_File_Line();
        win_error& add_to_error_stack(std::string custom, char* file_path, uint32_t file_line);
        win_error& add_to_error_stack(std::wstring custom, char* file_path, uint32_t file_line);
        win_error& add_to_error_stack(win_error err);
        friend std::ostream& operator<<(std::ostream& os, const win_error& werror);
        static void print(win_error err);
};