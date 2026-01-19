// // example_user_types.cpp - Logging custom types
// #include "spdlog/spdlog.h"
// #include "spdlog/fmt/ostr.h"  // For ostream operator support
// #include <vector>

// // Custom type with fmt formatter specialization
// struct Point {
//     double x, y;
//     Point(double x, double y) : x(x), y(y) {}
// };

// // fmt formatter for Point
// template<>
// struct fmt::formatter<Point> : fmt::formatter<std::string> {
//     auto format(const Point& p, format_context& ctx) const -> decltype(ctx.out()) {
//         return fmt::format_to(ctx.out(), "Point({:.2f}, {:.2f})", p.x, p.y);
//     }
// };

// // Another custom type
// struct User {
//     std::string name;
//     int age;
//     std::string email;
// };

// template<>
// struct fmt::formatter<User> : fmt::formatter<std::string> {
//     auto format(const User& u, format_context& ctx) const -> decltype(ctx.out()) {
//         return fmt::format_to(ctx.out(), "User{{name='{}', age={}, email='{}'}}", 
//                               u.name, u.age, u.email);
//     }
// };

// // Enum with formatter
// enum class Status { Pending, Active, Completed, Failed };

// template<>
// struct fmt::formatter<Status> : fmt::formatter<std::string_view> {
//     auto format(Status s, format_context& ctx) const -> decltype(ctx.out()) {
//         std::string_view name;
//         switch (s) {
//             case Status::Pending:   name = "PENDING"; break;
//             case Status::Active:    name = "ACTIVE"; break;
//             case Status::Completed: name = "COMPLETED"; break;
//             case Status::Failed:    name = "FAILED"; break;
//         }
//         return fmt::formatter<std::string_view>::format(name, ctx);
//     }
// };

// int main() {
//     // Log custom Point type
//     Point p1(3.14159, 2.71828);
//     Point p2(0.0, 0.0);
//     spdlog::info("Point 1: {}", p1);
//     spdlog::info("Point 2: {}", p2);
//     spdlog::info("Moving from {} to {}", p1, p2);
    
//     // Log custom User type
//     User user{"Alice", 30, "alice@example.com"};
//     spdlog::info("Current user: {}", user);
    
//     // Log enum
//     Status status = Status::Active;
//     spdlog::info("Task status: {}", status);
    
//     // Combined logging
//     spdlog::info("User {} at position {} has status {}", user, p1, status);
    
//     return 0;
// }



// example_complete_app.cpp - Complete application with proper logging setup
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/cfg/env.h"
#include "spdlog/stopwatch.h"

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace app {

    // Application configuration
    struct LogConfig {
        std::string log_dir = "logs";
        std::string app_name = "myapp";
        spdlog::level::level_enum console_level = spdlog::level::info;
        spdlog::level::level_enum file_level = spdlog::level::debug;
        size_t max_file_size = 5 * 1024 * 1024;  // 5 MB
        size_t max_files = 10;
        size_t async_queue_size = 8192;
        size_t async_threads = 1;
    };

    // Initialize logging system
    void init_logging(const LogConfig& config) {
        try {
            // Initialize async thread pool
            spdlog::init_thread_pool(config.async_queue_size, config.async_threads);
            
            // Console sink - colorized output
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(config.console_level);
            console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
            
            // Rotating file sink - all messages
            auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.log_dir + "/" + config.app_name + ".log",
                config.max_file_size,
                config.max_files
            );
            rotating_sink->set_level(config.file_level);
            rotating_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s:%#] %v");
            
            // Daily file sink - for archival
            auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                config.log_dir + "/" + config.app_name + "_daily.log",
                0,
                0  // Rotate at midnight
            );
            daily_sink->set_level(spdlog::level::info);
            
            // Create main application logger (async)
            std::vector<spdlog::sink_ptr> sinks{console_sink, rotating_sink, daily_sink};
            auto main_logger = std::make_shared<spdlog::async_logger>(
                "app",
                sinks.begin(), sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block
            );
            main_logger->set_level(spdlog::level::trace);
            
            // Register and set as default
            spdlog::register_logger(main_logger);
            spdlog::set_default_logger(main_logger);
            
            // Create component-specific loggers
            auto db_logger = main_logger->clone("db");
            auto net_logger = main_logger->clone("net");
            auto auth_logger = main_logger->clone("auth");
            
            spdlog::register_logger(db_logger);
            spdlog::register_logger(net_logger);
            spdlog::register_logger(auth_logger);
            
            // Allow environment override
            spdlog::cfg::load_env_levels();
            
            // Set up periodic flush
            spdlog::flush_every(std::chrono::seconds(3));
            
            spdlog::info("Logging system initialized");
            
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
            throw;
        }
    }

    // Shutdown logging system
    void shutdown_logging() {
        spdlog::info("Shutting down logging system");
        spdlog::shutdown();
    }

    // Example service classes
    class DatabaseService {
    public:
        void connect(const std::string& connection_string) {
            auto logger = spdlog::get("db");
            spdlog::stopwatch sw;
            
            logger->info("Connecting to database: {}", connection_string);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            logger->info("Database connected in {:.3f}s", sw.elapsed().count());
        }
        
        void query(const std::string& sql) {
            auto logger = spdlog::get("db");
            logger->debug("Executing query: {}", sql);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            logger->debug("Query completed");
        }
    };

    class NetworkService {
    public:
        void listen(int port) {
            auto logger = spdlog::get("net");
            logger->info("Starting server on port {}", port);
        }
        
        void handle_request(int request_id) {
            auto logger = spdlog::get("net");
            logger->debug("Handling request {}", request_id);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            logger->debug("Request {} completed", request_id);
        }
    };

    class AuthService {
    public:
        bool authenticate(const std::string& username) {
            auto logger = spdlog::get("auth");
            logger->info("Authenticating user: {}", username);
            
            // Simulate auth
            bool success = (username != "invalid");
            
            if (success) {
                logger->info("User {} authenticated successfully", username);
            } else {
                logger->warn("Authentication failed for user: {}", username);
            }
            
            return success;
        }
    };

}  // namespace app

int main() {
    // Initialize logging
    app::LogConfig config;
    config.console_level = spdlog::level::info;
    config.file_level = spdlog::level::debug;
    
    app::init_logging(config);
    
    spdlog::stopwatch app_sw;
    
    SPDLOG_INFO("Application starting...");
    
    // Create services
    app::DatabaseService db;
    app::NetworkService net;
    app::AuthService auth;
    
    // Simulate application flow
    db.connect("postgresql://localhost/mydb");
    net.listen(8080);
    
    // Simulate some requests
    std::vector<std::thread> workers;
    for (int i = 0; i < 5; i++) {
        workers.emplace_back([&, i]() {
            std::string user = (i == 3) ? "invalid" : "user_" + std::to_string(i);
            if (auth.authenticate(user)) {
                net.handle_request(i);
                db.query("SELECT * FROM users WHERE id = " + std::to_string(i));
            }
        });
    }
    
    for (auto& w : workers) {
        w.join();
    }
    
    SPDLOG_INFO("Application completed in {:.3f}s", app_sw.elapsed().count());
    
    // Shutdown
    app::shutdown_logging();
    
    return 0;
}