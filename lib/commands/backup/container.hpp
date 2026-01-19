#pragma once

#include "Utils/error.hpp"
#include "blake3.h"
#include "sqlite3.h"
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <array>
#include <vector>

using psqlite3 = sqlite3 *;
using psqlite3_stmt = sqlite3_stmt *;

enum class Init_State {
    backup,
    recover
};

class sqlite_filetable {
    private:
        // std::string sqlite_insert_filetable = "INSERT INTO RECORD_TABLE(RECORD_ID, NAME, PARENT_ID, IS_DIR) VALUES (?,?,?,?) RETURNING (RECORD_TABLE_ID, RECORD_ID);";
        std::string sqlite_insert_filetable = "INSERT INTO RECORD_TABLE(RECORD_ID, NAME, PARENT_ID, IS_DIR) VALUES (?,?,?,?) RETURNING (RECORD_TABLE_ID);";
        // std::string sqlite_insert_filetable = "INSERT INTO RECORD_TABLE(RECORD_ID, NAME, PARENT_ID, IS_DIR) VALUES (?,?,?,?);";
        // retrieve string here

        std::string sqlite_retrieve_filetable;

        psqlite3_stmt sqlite_stmt_filetable = nullptr;

        psqlite3 DB;
        sqlite_filetable(psqlite3 DB):DB(DB){} 
    
    public:
        // static std::expected<sqlite_filetable, win_error> init(psqlite3 DB, uint8_t state) {
        static std::expected<std::unique_ptr<sqlite_filetable>, win_error> init(psqlite3 DB, Init_State state) {
            auto ft = std::unique_ptr<sqlite_filetable>(new sqlite_filetable(DB));
            // sqlite_filetable ft(DB);

            if (state == Init_State::backup) {
                auto i = ft->init_insert();
                if (!i.has_value()) {
                    return std::unexpected(i.error().add_to_error_stack("Caller: filetable insert init failed", ERROR_LOC));
                }
            } else {
                // retrieve will be here
            }

            return ft;
            // return std::make_unique<sqlite_filetable>(ft);
        }

        std::expected<void, win_error> init_insert() {
            SQLITE_ERROR_CHECK(sqlite3_prepare_v2(
                DB,
                sqlite_insert_filetable.c_str(),
                -1,
                &sqlite_stmt_filetable,
                NULL
            ), DB);
            return {};
        }

        std::expected<int64_t, win_error> sqlite_filetable_insert(int64_t record_id, const std::string& name, int64_t parent_id, uint8_t is_dir) {
            if (sqlite_stmt_filetable == nullptr) {
                return std::unexpected(win_error("filetable insert not initialized or closed",ERROR_LOC));
            }
            SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_filetable, 1, record_id), DB);
            if (name.length() == 0) {
                SQLITE_ERROR_CHECK(sqlite3_bind_null(sqlite_stmt_filetable, 2), DB);
            } else {
                SQLITE_ERROR_CHECK(sqlite3_bind_text(sqlite_stmt_filetable, 2, name.c_str(), -1, NULL), DB);
            }
            // SQLITE_ERROR_CHECK(sqlite3_bind_text(sqlite_stmt_filetable, 2, name.c_str(), -1, NULL), DB);
            if (parent_id == -1) {
                SQLITE_ERROR_CHECK(sqlite3_bind_null(sqlite_stmt_filetable, 3), DB);
            } else {
                SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_filetable, 3, parent_id), DB);
            }
            SQLITE_ERROR_CHECK(sqlite3_bind_int(sqlite_stmt_filetable, 4, is_dir), DB);
            SQLITE_ERROR_CHECK(sqlite3_step(sqlite_stmt_filetable), DB);
            int64_t ret_record_table_id = sqlite3_column_int64(sqlite_stmt_filetable, 0);
            SQLITE_ERROR_CHECK(sqlite3_reset(sqlite_stmt_filetable), DB);
            SQLITE_ERROR_CHECK(sqlite3_clear_bindings(sqlite_stmt_filetable), DB);
            return ret_record_table_id;
        }

        std::expected<void, win_error> close() {
            if (sqlite_stmt_filetable == nullptr) {
                return std::unexpected(win_error("filetable insert not initialized or closed", ERROR_LOC));
            }
            SQLITE_ERROR_CHECK(sqlite3_finalize(sqlite_stmt_filetable), DB);
            return {};
        }

        ~sqlite_filetable() {
            if (sqlite_stmt_filetable != nullptr) {
                sqlite3_finalize(sqlite_stmt_filetable);
            }
        }
};

class sqlite_dataruntable {
    private:
        psqlite3_stmt sqlite_stmt_dataruntable = nullptr;
        psqlite3 DB;
        // std::string sqlite_insert_dataruntable = "INSERT INTO DATARUN_TABLE(RECORD_ID, RECORD_DATARUN_INDEX, DATARUN_OFFSET, DATARUN_LENGTH,"
        //                                               " DATARUN_HASH, DATARUN_FLAG) VALUES (?,?,?,?,?,?,?) RETURNING DATARUN_ID, RECORD_ID;";
        // std::string sqlite_insert_dataruntable = "INSERT INTO DATARUN_TABLE(RECORD_ID, RECORD_DATARUN_INDEX, DATARUN_OFFSET, DATARUN_LENGTH,"
        //                                               " DATARUN_HASH, DATARUN_FLAG) VALUES (?,?,?,?,?,?) RETURNING DATARUN_ID;";
        std::string sqlite_insert_dataruntable = "INSERT INTO DATARUN_TABLE(RECORD_ID, RECORD_DATARUN_INDEX, DATARUN_OFFSET, DATARUN_LENGTH,"
                                                      " DATARUN_HASH, DATARUN_FLAG) VALUES (?,?,?,?,?,?);";
        sqlite_dataruntable(psqlite3 DB): DB(DB){}

    public:
        // static std::expected<sqlite_dataruntable, win_error> init(psqlite3 DB, uint8_t state) {
        static std::expected<std::unique_ptr<sqlite_dataruntable>, win_error> init(psqlite3 DB, Init_State state) {
            auto dt = std::unique_ptr<sqlite_dataruntable>(new sqlite_dataruntable(DB));
            // sqlite_dataruntable dt(DB);
            if (state == Init_State::backup) {
                auto i = dt->init_insert();
                if (!i.has_value()) {
                    return std::unexpected(i.error().add_to_error_stack("Caller: dataruntable insert init failed", ERROR_LOC));
                }
            } else {
                // retrieve will be here
            }

            return dt;
            // return std::make_unique<sqlite_dataruntable>(dt);
        }

        std::expected<void, win_error> init_insert() {
            SQLITE_ERROR_CHECK(sqlite3_prepare_v2(
                DB,
                sqlite_insert_dataruntable.c_str(),
                -1,
                &sqlite_stmt_dataruntable,
                NULL
            ), DB);
            return {};
        }

        std::expected<void, win_error> sqlite_dataruntable_insert(
            int64_t record_id,
            int32_t record_datrun_index,
            int64_t datarun_offset,
            int64_t datarun_length,
            std::array<uint8_t, 32> datarun_hash,
            int32_t datarun_flag
        ) {
            if (sqlite_stmt_dataruntable == nullptr) {
                return std::unexpected(win_error("dataruntable insert not initialized or closed", ERROR_LOC));
            }
            SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_dataruntable, 1, record_id), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_int(sqlite_stmt_dataruntable, 2, record_datrun_index), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_dataruntable, 3, datarun_offset), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_dataruntable, 4, datarun_length), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_blob(sqlite_stmt_dataruntable, 5, datarun_hash.data(), BLAKE3_OUT_LEN, NULL), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_int(sqlite_stmt_dataruntable, 6, datarun_flag), DB);
            SQLITE_ERROR_CHECK(sqlite3_step(sqlite_stmt_dataruntable), DB);
            SQLITE_ERROR_CHECK(sqlite3_reset(sqlite_stmt_dataruntable), DB);
            SQLITE_ERROR_CHECK(sqlite3_clear_bindings(sqlite_stmt_dataruntable), DB);
            return {};
        }

        // std::expected<int64_t, win_error> sqlite_dataruntable_insert(
        //     int64_t record_id,
        //     int32_t record_datrun_index,
        //     int64_t datarun_offset,
        //     int64_t datarun_length,
        //     std::array<uint8_t, 32> datarun_hash,
        //     int32_t datarun_flag
        // ) {
        //     if (sqlite_stmt_dataruntable == nullptr) {
        //         return std::unexpected(win_error("dataruntable insert not initialized or closed", ERROR_LOC));
        //     }
        //     SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_dataruntable, 1, record_id), DB);
        //     SQLITE_ERROR_CHECK(sqlite3_bind_int(sqlite_stmt_dataruntable, 2, record_datrun_index), DB);
        //     SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_dataruntable, 3, datarun_offset), DB);
        //     SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_dataruntable, 4, datarun_length), DB);
        //     SQLITE_ERROR_CHECK(sqlite3_bind_blob(sqlite_stmt_dataruntable, 5, datarun_hash.data(), BLAKE3_OUT_LEN, NULL), DB);
        //     SQLITE_ERROR_CHECK(sqlite3_bind_int(sqlite_stmt_dataruntable, 6, datarun_flag), DB);
        //     SQLITE_ERROR_CHECK(sqlite3_step(sqlite_stmt_dataruntable), DB);
        //     int64_t ret_record_id = sqlite3_column_int64(sqlite_stmt_dataruntable, 0);
        //     SQLITE_ERROR_CHECK(sqlite3_reset(sqlite_stmt_dataruntable), DB);
        //     SQLITE_ERROR_CHECK(sqlite3_clear_bindings(sqlite_stmt_dataruntable), DB);
        //     return ret_record_id;
        // }

        std::expected<void, win_error> close(){
            if (sqlite_stmt_dataruntable == nullptr) {
                return std::unexpected(win_error("datarun table insert not initialized or closed", ERROR_LOC));
            }
            SQLITE_ERROR_CHECK(sqlite3_finalize(sqlite_stmt_dataruntable), DB);
            return {};
        }

        ~sqlite_dataruntable() {
            if (sqlite_stmt_dataruntable != nullptr) {
                sqlite3_finalize(sqlite_stmt_dataruntable);
            }
        }
};

class sqlite_systemcompatibilitytable {
    private:
        psqlite3 DB;
        psqlite3_stmt sqlite_stmt_systemcompatibilitytable = nullptr;
        std::string sqlite_insert_systemcompatibilitytable = "INSERT INTO SYSTEM_COMPATIBILITY_TABLE(GEOMETRY_CLUSTER_SIZE, GEOMETRY_RECORD_SIZE, "
                                                             "GEOMETRY_BLOCK_SIZE, GEOMETRY_SECTOR_SIZE, DISK_PRODUCT_ID, DISK_SERIAL_ID, DISK_VERSION) "
                                                             "VALUES (?,?,?,?,?,?,?);";
    
        sqlite_systemcompatibilitytable(psqlite3 DB):DB(DB){}

    public:
        // static std::expected<sqlite_systemcompatibilitytable, win_error> init(psqlite3 DB, uint8_t state) {
        static std::expected<std::unique_ptr<sqlite_systemcompatibilitytable>, win_error> init(psqlite3 DB, Init_State state) {
            auto sct = std::unique_ptr<sqlite_systemcompatibilitytable>(new sqlite_systemcompatibilitytable(DB));
            // sqlite_systemcompatibilitytable sct(DB);
            if (state == Init_State::backup) {
                auto i = sct->init_insert();
                if (!i.has_value()) {
                    return std::unexpected(i.error().add_to_error_stack("Caller: system compatibility table insert init failed", ERROR_LOC));
                }
            } else {
                // retrieve will be here
            }
            
            // return sct;
            return sct;
        }

        std::expected<void, win_error> init_insert() {
            SQLITE_ERROR_CHECK(sqlite3_prepare_v2(
                DB,
                sqlite_insert_systemcompatibilitytable.c_str(),
                -1,
                &sqlite_stmt_systemcompatibilitytable,
                NULL
            ), DB);
            return {};
        }

        std::expected<void, win_error> sqlite_systemcompatibilitytable_insert(
            int64_t geometry_cluster_size,
            int64_t geometry_record_size,
            int64_t geometry_block_size,
            int64_t geometry_sector_size,
            const std::string& disk_product_id,
            const std::string& disk_serial_id,
            const std::string& disk_version
        ) {
            if (sqlite_stmt_systemcompatibilitytable == nullptr) {
                return std::unexpected(win_error("dataruntable insert not initialized", ERROR_LOC));
            }
            SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_systemcompatibilitytable, 1, geometry_cluster_size), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_systemcompatibilitytable, 2, geometry_record_size), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_systemcompatibilitytable, 3, geometry_block_size), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_int64(sqlite_stmt_systemcompatibilitytable, 4, geometry_sector_size), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_text(sqlite_stmt_systemcompatibilitytable, 5, disk_product_id.c_str(), -1, NULL), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_text(sqlite_stmt_systemcompatibilitytable, 6, disk_serial_id.c_str(), -1, NULL), DB);
            SQLITE_ERROR_CHECK(sqlite3_bind_text(sqlite_stmt_systemcompatibilitytable, 7, disk_version.c_str(), -1, NULL), DB);
            SQLITE_ERROR_CHECK(sqlite3_step(sqlite_stmt_systemcompatibilitytable), DB);
            SQLITE_ERROR_CHECK(sqlite3_reset(sqlite_stmt_systemcompatibilitytable), DB);
            SQLITE_ERROR_CHECK(sqlite3_clear_bindings(sqlite_stmt_systemcompatibilitytable), DB);
            return {};
        }

        std::expected<void, win_error> close() {
            if (sqlite_stmt_systemcompatibilitytable == nullptr) {
                return std::unexpected(win_error("datarun table insert not initialized or closed",ERROR_LOC));
            }
            SQLITE_ERROR_CHECK(sqlite3_finalize(sqlite_stmt_systemcompatibilitytable), DB);
            return {};
        }

        ~sqlite_systemcompatibilitytable() {
            if (sqlite_stmt_systemcompatibilitytable != nullptr) {
                sqlite3_finalize(sqlite_stmt_systemcompatibilitytable);
            }
        }
};

class sqlite_commander {
    private:
        Init_State state;
        psqlite3 DB = nullptr;
        // std::unique_ptr<sqlite_filetable>    filetable_ptr = nullptr;
        std::vector<std::unique_ptr<sqlite_filetable>> filetable_ptr_arr;
        // std::unique_ptr<sqlite_dataruntable> dataruntable_ptr = nullptr;
        std::vector<std::unique_ptr<sqlite_dataruntable>> dataruntable_ptr_arr;
        std::unique_ptr<sqlite_systemcompatibilitytable> systemcompatibilitytable_ptr = nullptr;
        // std::vector<std::unique_ptr<sqlite_systemcompatibilitytable>> systemcompatibilitytable_ptr_arr;

        std::string sqlite_setupdb = "PRAGMA foreign_keys = ON;"
                                     "PRAGMA journal_mode = WAL;";

        std::string sqlite_create_filetable = "CREATE TABLE IF NOT EXISTS RECORD_TABLE("
                                              "RECORD_TABLE_ID    INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
                                              "RECORD_ID          INTEGER NOT NULL,"
                                              "NAME               TEXT,"
                                              "PARENT_ID          INTEGER,"
                                              "IS_DIR             INTEGER NOT NULL,"
                                              "FOREIGN KEY(PARENT_ID) REFERENCES RECORD_TABLE(RECORD_TABLE_ID) ON DELETE CASCADE,"
                                              "UNIQUE(PARENT_ID, NAME));";
        
        std::string sqlite_create_dataruntable = "CREATE TABLE IF NOT EXISTS DATARUN_TABLE("
                                                 "DATARUN_ID           INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
                                                 "RECORD_ID            INTEGER NOT NULL,"
                                                 "RECORD_DATARUN_INDEX INTEGER NOT NULL,"
                                                 "DATARUN_OFFSET       INTEGER NOT NULL,"
                                                 "DATARUN_LENGTH       INTEGER NOT NULL,"
                                                 "DATARUN_HASH         BLOB    NOT NULL,"
                                                //  "DATARUN_HASH         BLOB    NOT NULL UNIQUE,"
                                                 "DATARUN_FLAG         INTEGER NOT NULL,"
                                                 "FOREIGN KEY(RECORD_ID) REFERENCES RECORD_TABLE(RECORD_ID) ON DELETE CASCADE);";
        
        std::string sqlite_create_systemcompatibilitytable = "CREATE TABLE IF NOT EXISTS SYSTEM_COMPATIBILITY_TABLE("
                                                             "GEOMETRY_CLUSTER_SIZE INTEGER NOT NULL,"
                                                             "GEOMETRY_RECORD_SIZE  INTEGER NOT NULL,"
                                                             "GEOMETRY_BLOCK_SIZE   INTEGER NOT NULL,"
                                                             "GEOMETRY_SECTOR_SIZE  INTEGER NOT NULL,"
                                                             "DISK_PRODUCT_ID       TEXT    NOT NULL,"
                                                             "DISK_SERIAL_ID        TEXT    NOT NULL,"
                                                             "DISK_VERSION          TEXT    NOT NULL);";

        std::string sqlite_parentid_index = "CREATE INDEX IF NOT EXISTS IDX_RECORD_TABLE_PARENT ON RECORD_TABLE(PARENT_ID);";

        std::string sqlite_uniquechild_index = "CREATE UNIQUE INDEX IDX_RECORD_TABLE_UNIQUE_CHILD ON RECORD_TABLE(PARENT_ID, NAME);";

        explicit sqlite_commander(psqlite3 DB): DB(DB) {}

    public:

        ~sqlite_commander() {
            if (this->DB) {
                // filetable_ptr.reset();
                for (int i=0; i< filetable_ptr_arr.size(); i++) { filetable_ptr_arr[i].reset(); }
                // dataruntable_ptr.reset();
                for (int i=0; i< dataruntable_ptr_arr.size(); i++) { dataruntable_ptr_arr[i].reset(); }
                systemcompatibilitytable_ptr.reset();
                sqlite3_close(this->DB);
            }
        }

        sqlite_commander(const sqlite_commander&) = delete;
        sqlite_commander& operator=(const sqlite_commander&) = delete;

        sqlite_commander(sqlite_commander&& other) noexcept {
            this->DB = other.DB;
            other.DB = nullptr;
        }

        sqlite_commander& operator=(sqlite_commander&& other) noexcept {
            if (this == &other) {
                return *this;
            }

            if (this->DB) {
                sqlite3_close(this->DB);
            }

            this->DB = other.DB;
            other.DB = nullptr;

            return *this;
        }

        static std::expected<std::unique_ptr<sqlite_commander>, win_error> open(std::string db_path) {
            psqlite3 db_handle = nullptr;
            SQLITE_ERROR_CHECK(sqlite3_open(db_path.c_str(), &db_handle), db_handle);
            return std::unique_ptr<sqlite_commander>(new sqlite_commander(db_handle));
        }

        psqlite3 get_raw_dbp() {
            return this->DB;
        }

        std::expected<void, win_error> init(Init_State state) {
            SQLITE_ERROR_CHECK(sqlite3_exec(DB, sqlite_setupdb.c_str(),                         NULL, 0, NULL), DB);
            SQLITE_ERROR_CHECK(sqlite3_exec(DB, sqlite_create_filetable.c_str(),                NULL, 0, NULL), DB);
            SQLITE_ERROR_CHECK(sqlite3_exec(DB, sqlite_create_dataruntable.c_str(),             NULL, 0, NULL), DB);
            SQLITE_ERROR_CHECK(sqlite3_exec(DB, sqlite_create_systemcompatibilitytable.c_str(), NULL, 0, NULL), DB);
            SQLITE_ERROR_CHECK(sqlite3_exec(DB, sqlite_parentid_index.c_str(),                  NULL, 0, NULL), DB);
            SQLITE_ERROR_CHECK(sqlite3_exec(DB, sqlite_uniquechild_index.c_str(),               NULL, 0, NULL), DB);

            // auto i = sqlite_filetable::init(DB, state);
            // if (!i.has_value()) {
            //     return std::unexpected(i.error().add_to_error_stack("Caller: sqlite filetable init failed", ERROR_LOC));
            // }
            // filetable_ptr = std::move(i.value());
            
            // auto j = sqlite_dataruntable::init(DB, state);
            // if (!j.has_value()) {
            //     return std::unexpected(j.error().add_to_error_stack("Caller: sqlite dataruntable init failed", ERROR_LOC));
            // }
            // dataruntable_ptr = std::move(j.value());

            this->state = state;
            
            auto k = sqlite_systemcompatibilitytable::init(DB, state);
            if (!k.has_value()) {
                return std::unexpected(k.error().add_to_error_stack("Caller: sqlite systemcomaptibilitytable init failed", ERROR_LOC));
            }
            systemcompatibilitytable_ptr = std::move(k.value());

            return {};
        }

        // static void push_back_return() {

        // }

        std::expected<sqlite_filetable*, win_error> filetable_push_back_return() {
            auto i = sqlite_filetable::init(DB, state);
            if (!i.has_value()) {
                return std::unexpected(i.error().add_to_error_stack("Caller: sqlite filetable init failed", ERROR_LOC));
            }
            filetable_ptr_arr.push_back(std::move(i.value()));
            return filetable_ptr_arr.back().get();
            // return filetable_ptr_arr[filetable_ptr_arr.size()-1].get();
        }
        
        std::expected<sqlite_dataruntable*, win_error> dataruntable_push_back_return() {
            auto i = sqlite_dataruntable::init(DB, state);
            if (!i.has_value()) {
                return std::unexpected(i.error().add_to_error_stack("Caller: sqlite filetable init failed", ERROR_LOC));
            }
            dataruntable_ptr_arr.push_back(std::move(i.value()));
            return dataruntable_ptr_arr.back().get();
            // return dataruntable_ptr_arr[dataruntable_ptr_arr.size()-1].get();
        }

        std::optional<sqlite_filetable*> get_sqlite_filetable(int index) {
            if (index>=filetable_ptr_arr.size()) { return std::nullopt; }
            return filetable_ptr_arr[index].get();
        }

        std::optional<sqlite_dataruntable*> get_sqlite_dataruntable(int index) {
            if (index>=dataruntable_ptr_arr.size()) { return std::nullopt; }
            return dataruntable_ptr_arr[index].get();
        }

        sqlite_systemcompatibilitytable* get_sqlite_systemcompatibilitytable() {
            return systemcompatibilitytable_ptr.get();
        }
};