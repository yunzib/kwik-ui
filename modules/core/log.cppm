module;
#include <source_location>
#include <string_view>
#include <format>
#include <atomic>
#include <tuple>
#include <utility> // for index_sequence

export module kwik.core.log;

import std;

export enum class LogLevel { Debug, Info, Warning, Error };

export class Log {
public:
    // 全局控制
    static void set_level(LogLevel level) noexcept;
    static bool should_log(LogLevel level) noexcept;
    static void set_color_enabled(bool enabled) noexcept;

    // 统一接口：自动检测是否包含 source_location（必须是最后一个参数）
    template <typename... Args>
    static void debug(std::string_view fmt, Args &&...args) {
        if (should_log(LogLevel::Debug)) { log_dispatch(LogLevel::Debug, fmt, std::forward<Args>(args)...); }
    }
    template <typename... Args>
    static void info(std::string_view fmt, Args &&...args) {
        if (should_log(LogLevel::Info)) { log_dispatch(LogLevel::Info, fmt, std::forward<Args>(args)...); }
    }
    template <typename... Args>
    static void warn(std::string_view fmt, Args &&...args) {
        if (should_log(LogLevel::Warning)) { log_dispatch(LogLevel::Warning, fmt, std::forward<Args>(args)...); }
    }
    template <typename... Args>
    static void error(std::string_view fmt, Args &&...args) {
        if (should_log(LogLevel::Error)) { log_dispatch(LogLevel::Error, fmt, std::forward<Args>(args)...); }
    }

private:
    // 内部分派：检测最后一个参数是否为 source_location
    template <typename... Args>
    static void log_dispatch(LogLevel level, std::string_view fmt, Args &&...args) {
        // ===== 安全判断：空参数包直接返回 false =====
        constexpr bool has_loc = []() {
            if constexpr (sizeof...(Args) == 0) {
                return false;
            } else {
                using LastType = std::decay_t<std::tuple_element_t<sizeof...(Args) - 1, std::tuple<Args...>>>;
                return std::is_same_v<LastType, std::source_location>;
            }
        }();

        auto t = std::forward_as_tuple(std::forward<Args>(args)...);

        if constexpr (has_loc) {
            constexpr size_t N = sizeof...(Args) - 1;
            auto format_args = [&]<size_t... I>(std::index_sequence<I...>) {
                return std::make_format_args(std::get<I>(t)...);
            }(std::make_index_sequence<N>{});
            const auto &loc = std::get<N>(t);
            log_impl(level, std::vformat(fmt, format_args), &loc);
        } else {
            auto format_args = [&]<size_t... I>(std::index_sequence<I...>) {
                return std::make_format_args(std::get<I>(t)...);
            }(std::make_index_sequence<sizeof...(Args)>{});
            log_impl(level, std::vformat(fmt, format_args), nullptr);
        }
    }

    static void log_impl(LogLevel level, std::string_view msg, const std::source_location *loc);

    static std::atomic<LogLevel> current_level_;
    static bool color_enabled_;
};

// import kwik.core.log;

// int main() {
//     Log::set_level(LogLevel::Debug);
//     Log::set_color_enabled(true);

//     // 不带位置信息
//     Log::debug("This is a debug message");          // 无源位置
//     Log::info("Info without location");

//     // 带位置信息（显式传入，必须是最后一个参数）
//     Log::warn("Warning with location", std::source_location::current());
//     Log::error("Fatal error", std::source_location::current());
// Log::error("Eval JS Module Error:{}: {}", name, err ? err : "unknown", std::source_location::current());

//     return 0;
// }