#include "BS_thread_pool.hpp"
#include "Utils/error.hpp"
#include "Utils/utils.hpp"
#include "commands/backup/container.hpp"

#include <array>
#include <utility>

// thread_local sqlite_filetable* sqlite_ft = nullptr;
// thread_local sqlite_dataruntable* sqlite_dt = nullptr;

int main() {
    // auto i = sqlite_commander::open(":memory:");
    // std::cout << "1" << std::endl;
    auto i = sqlite_commander::open("./db/db.sqlite");
    // std::cout << "2" << std::endl;
    if (!i.has_value()) {
        win_error::print(i.error().add_to_error_stack("Caller: sqlite open error", ERROR_LOC));
    }
    // std::cout << "3" << std::endl;
    auto commander = std::move(i.value());
    // std::cout << "4" << std::endl;
    
    auto j = commander->init(Init_State::backup);
    // std::cout << "5" << std::endl;
    if (!j.has_value()) {
        win_error::print(j.error().add_to_error_stack("Caller: sqlite init error", ERROR_LOC));
    }
    
    // std::cout << "6" << std::endl;
    auto temp_sqlite_ft = commander->filetable_push_back_return();
    if (!temp_sqlite_ft.has_value()) {
        win_error::print(temp_sqlite_ft.error().add_to_error_stack("Caller: sqlite_ft error", ERROR_LOC));
        return 1;
    }
    sqlite_filetable* sqlite_ft = temp_sqlite_ft.value();

    // std::cout << "7" << std::endl;

    // BS::priority_thread_pool pool(12, [&](){
    //     // auto temp_sqlite_ft = commander->filetable_push_back_return();
    //     // if (!temp_sqlite_ft.has_value()) {
    //     //     win_error::print(temp_sqlite_ft.error().add_to_error_stack("Caller: sqlite ft error", ERROR_LOC));
    //     //     exit(1);
    //     // }
    //     // sqlite_ft = temp_sqlite_ft.value();
        
    //     auto temp_sqlite_dt = commander->dataruntable_push_back_return();
    //     if (!temp_sqlite_dt.has_value()) {
    //         win_error::print(temp_sqlite_dt.error().add_to_error_stack("Caller: sqlite dt error", ERROR_LOC));
    //         exit(1);
    //     }
    //     sqlite_dt = temp_sqlite_dt.value();
    // });

    // pool.detach_task([](){
        auto k = sqlite_ft->sqlite_filetable_insert(
            1, utils::strings::to_utf8(L""), -1, 0
        );
        if (!k.has_value()) {
            win_error::print(k.error().add_to_error_stack("Caller: filetable insert error, 1", ERROR_LOC));
        }
        std::cout << "return id: " << k.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
        // });
        
        // pool.detach_task([](){
        auto l = sqlite_ft->sqlite_filetable_insert(
            2, "2name", 1, 0
        );
        if (!l.has_value()) {
            win_error::print(l.error().add_to_error_stack("Caller: filetable insert error, 2", ERROR_LOC));
        }
        std::cout << "return id: " << l.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        auto m = sqlite_ft->sqlite_filetable_insert(
            3, "3name", 1, 0
        );
        if (!m.has_value()) {
            win_error::print(m.error().add_to_error_stack("Caller: filetable insert error, 3", ERROR_LOC));
        }
        std::cout << "return id: " << m.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        auto n = sqlite_ft->sqlite_filetable_insert(
            4, "4name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 4", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            5, "5name", 3, 1
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 5", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            6, "6name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 6", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            7, "7name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 7", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            8, "8name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 8", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            9, "9name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 9", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            10, "10name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 10", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            11, "11name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 11", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            12, "12name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 12", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            13, "13name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 13", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            14, "14name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 14", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // pool.detach_task([](){
        n = sqlite_ft->sqlite_filetable_insert(
            15, "15name", 3, 0
        );
        if (!n.has_value()) {
            win_error::print(n.error().add_to_error_stack("Caller: filetable insert error, 15", ERROR_LOC));
        }
        std::cout << "return id: " << n.value() << std::endl;
        // std::cout << "thread no: " << BS::this_thread::get_index().value() << std::endl;
    // });
    
    // sqlite_dataruntable* sqlite_dt = commander->dataruntable_push_back_return().value();
    
    // uint8_t a[32] = {
    //     0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1,
    //     1,1,1,1,1,1,1,1,1,
    //     1,1,1,1,1,1,1,1,1,
    //     1,1,1
    // };

    // pool.detach_task([&](){
    //     auto k1 = sqlite_dt->sqlite_dataruntable_insert(
    //         1, 
    //         1, 
    //         1, 
    //         1, 
    //         std::to_array(a), 
    //         1
    //     );
    //     if (!k1.has_value()) {
    //         win_error::print(k1.error().add_to_error_stack("Caller: dataruntable insert error", ERROR_LOC));
    //     }
    //     // std::cout << "thread no: " << BS::this_thread::get_index().value() << ", datarun table insert return: " << k1.value() << std::endl;
    // });

    // pool.detach_task([&](){
    //     auto k1 = sqlite_dt->sqlite_dataruntable_insert(
    //         2,
    //         1,
    //         1,
    //         1,
    //         std::to_array(a),
    //         1
    //     );
    //     if (!k1.has_value()) {
    //         win_error::print(k1.error().add_to_error_stack("Caller: dataruntable insert error", ERROR_LOC));
    //     }
    //     // std::cout << "thread no: " << BS::this_thread::get_index().value() << ", datarun table insert return: " << k1.value() << std::endl;
    // });

    // pool.detach_task([&](){
    //     auto k1 = sqlite_dt->sqlite_dataruntable_insert(
    //         3, 
    //         1, 
    //         1, 
    //         1, 
    //         std::to_array(a), 
    //         1
    //     );
    //     if (!k1.has_value()) {
    //         win_error::print(k1.error().add_to_error_stack("Caller: dataruntable insert error", ERROR_LOC));
    //     }
    //     // std::cout << "thread no: " << BS::this_thread::get_index().value() << ", datarun table insert return: " << k1.value() << std::endl;
    // });

    // pool.detach_task([&](){
    //     auto k1 = sqlite_dt->sqlite_dataruntable_insert(
    //         4, 
    //         1, 
    //         1, 
    //         1, 
    //         std::to_array(a), 
    //         1
    //     );
    //     if (!k1.has_value()) {
    //         win_error::print(k1.error().add_to_error_stack("Caller: dataruntable insert error", ERROR_LOC));
    //     }
    //     // std::cout << "thread no: " << BS::this_thread::get_index().value() << ", datarun table insert return: " << k1.value() << std::endl;
    // });

    // pool.detach_task([&](){
    //     auto k1 = sqlite_dt->sqlite_dataruntable_insert(
    //         5, 
    //         1, 
    //         1, 
    //         1, 
    //         std::to_array(a), 
    //         1
    //     );
    //     if (!k1.has_value()) {
    //         win_error::print(k1.error().add_to_error_stack("Caller: dataruntable insert error", ERROR_LOC));
    //     }
    //     // std::cout << "thread no: " << BS::this_thread::get_index().value() << ", datarun table insert return: " << k1.value() << std::endl;
    // });

    // pool.detach_task([&](){
    //     auto k1 = sqlite_dt->sqlite_dataruntable_insert(
    //         6, 
    //         1, 
    //         1, 
    //         1, 
    //         std::to_array(a), 
    //         1
    //     );
    //     if (!k1.has_value()) {
    //         win_error::print(k1.error().add_to_error_stack("Caller: dataruntable insert error", ERROR_LOC));
    //     }
    //     // std::cout << "thread no: " << BS::this_thread::get_index().value() << ", datarun table insert return: " << k1.value() << std::endl;
    // });

    // pool.detach_task([&](){
    //     auto k1 = sqlite_dt->sqlite_dataruntable_insert(
    //         7, 
    //         1, 
    //         1, 
    //         1, 
    //         std::to_array(a), 
    //         1
    //     );
    //     if (!k1.has_value()) {
    //         win_error::print(k1.error().add_to_error_stack("Caller: dataruntable insert error", ERROR_LOC));
    //     }
    //     // std::cout << "thread no: " << BS::this_thread::get_index().value() << ", datarun table insert return: " << k1.value() << std::endl;
    // });

    // pool.detach_task([&](){
    //     auto k1 = sqlite_dt->sqlite_dataruntable_insert(
    //         8, 
    //         1, 
    //         1, 
    //         1, 
    //         std::to_array(a), 
    //         1
    //     );
    //     if (!k1.has_value()) {
    //         win_error::print(k1.error().add_to_error_stack("Caller: dataruntable insert error", ERROR_LOC));
    //     }
    //     // std::cout << "thread no: " << BS::this_thread::get_index().value() << ", datarun table insert return: " << k1.value() << std::endl;
    // });

    // pool.detach_task([&](){
    //     auto k1 = sqlite_dt->sqlite_dataruntable_insert(
    //         9, 
    //         1, 
    //         1, 
    //         1, 
    //         std::to_array(a), 
    //         1
    //     );
    //     if (!k1.has_value()) {
    //         win_error::print(k1.error().add_to_error_stack("Caller: dataruntable insert error", ERROR_LOC));
    //     }
    //     // std::cout << "thread no: " << BS::this_thread::get_index().value() << ", datarun table insert return: " << k1.value() << std::endl;
    // });

    // pool.detach_task([&](){
    //     auto k1 = sqlite_dt->sqlite_dataruntable_insert(
    //         10, 
    //         1, 
    //         1, 
    //         1, 
    //         std::to_array(a), 
    //         1
    //     );
    //     if (!k1.has_value()) {
    //         win_error::print(k1.error().add_to_error_stack("Caller: dataruntable insert error", ERROR_LOC));
    //     }
    //     // std::cout << "thread no: " << BS::this_thread::get_index().value() << ", datarun table insert return: " << k1.value() << std::endl;
    // });
    
    sqlite_systemcompatibilitytable* u2 = commander->get_sqlite_systemcompatibilitytable();
    auto k3 = u2->sqlite_systemcompatibilitytable_insert(
        1, 
        1, 
        1, 
        1, 
        "product_id", 
        "serial_id", 
        "disk_version"
    );
    if (!k3.has_value()) {
        win_error::print(k3.error().add_to_error_stack("Caller: systemcompat insert error", ERROR_LOC));
    }

    // auto li = commander.close();
    // if (!li.has_value()) {
    //     win_error::print(li.error().add_to_error_stack("Caller: commnader close error", ERROR_LOC));
    // }
}