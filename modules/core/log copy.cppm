module;
#include <source_location>
#include <string_view>
#include <format>
#include <atomic>

export module kwik.core.log;

import std;

export enum class LogLevel { Debug, Info, Warning, Error };

export class Log {
public:
    // 全局控制
    static void set_level(LogLevel level) noexcept;
    static bool should_log(LogLevel level) noexcept;
    static void set_color_enabled(bool enabled) noexcept;

    // 不带位置信息的重载
    template <typename... Args>
    static void debug(std::string_view fmt, Args&&... args) {
        if (should_log(LogLevel::Debug)) {
            log_impl(LogLevel::Debug, std::vformat(fmt, std::make_format_args(args...)), nullptr);
        }
    }
    template <typename... Args>
    static void info(std::string_view fmt, Args&&... args) {
        if (should_log(LogLevel::Info)) {
            log_impl(LogLevel::Info, std::vformat(fmt, std::make_format_args(args...)), nullptr);
        }
    }
    template <typename... Args>
    static void warn(std::string_view fmt, Args&&... args) {
        if (should_log(LogLevel::Warning)) {
            log_impl(LogLevel::Warning, std::vformat(fmt, std::make_format_args(args...)), nullptr);
        }
    }
    template <typename... Args>
    static void error(std::string_view fmt, Args&&... args) {
        if (should_log(LogLevel::Error)) {
            log_impl(LogLevel::Error, std::vformat(fmt, std::make_format_args(args...)), nullptr);
        }
    }

    // 带位置信息的重载（最后一个参数为显式传入的 source_location）
    template <typename... Args>
    static void debug(std::string_view fmt, Args&&... args, const std::source_location& loc) {
        if (should_log(LogLevel::Debug)) {
            log_impl(LogLevel::Debug, std::vformat(fmt, std::make_format_args(args...)), &loc);
        }
    }
    template <typename... Args>
    static void info(std::string_view fmt, Args&&... args, const std::source_location& loc) {
        if (should_log(LogLevel::Info)) {
            log_impl(LogLevel::Info, std::vformat(fmt, std::make_format_args(args...)), &loc);
        }
    }
    template <typename... Args>
    static void warn(std::string_view fmt, Args&&... args, const std::source_location& loc) {
        if (should_log(LogLevel::Warning)) {
            log_impl(LogLevel::Warning, std::vformat(fmt, std::make_format_args(args...)), &loc);
        }
    }
    template <typename... Args>
    static void error(std::string_view fmt, Args&&... args, const std::source_location& loc) {
        if (should_log(LogLevel::Error)) {
            log_impl(LogLevel::Error, std::vformat(fmt, std::make_format_args(args...)), &loc);
        }
    }

private:
    static void log_impl(LogLevel level, std::string_view msg, const std::source_location* loc);

    static std::atomic<LogLevel> current_level_;
    static bool color_enabled_;
};


// 使用示例
// import kwik.core.log;

// int main() {
//     Log::set_level(LogLevel::Debug);
//     Log::set_color_enabled(true);

//     // 不带位置信息
//     Log::debug("This is a debug message");          // 无源位置
//     Log::info("Info without location");

//     // 带位置信息（显式传入）
//     Log::warn("Warning with location", std::source_location::current());
//     Log::error("Fatal error", std::source_location::current());

//     return 0;
// }