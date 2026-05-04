module;
#include <print>
#include <chrono>
#include <ctime>
#include <string>

module kwik.core.log;

import std;

std::atomic<LogLevel> Log::current_level_{LogLevel::Debug};
bool Log::color_enabled_ = true;

void Log::set_level(LogLevel level) noexcept {
    current_level_.store(level, std::memory_order_relaxed);
}

bool Log::should_log(LogLevel level) noexcept {
    return level >= current_level_.load(std::memory_order_relaxed);
}

void Log::set_color_enabled(bool enabled) noexcept {
    color_enabled_ = enabled;
}

namespace {
constexpr const char* level_to_string(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Debug:   return "Debug";
    case LogLevel::Info:    return "Info";
    case LogLevel::Warning: return "Warning";
    case LogLevel::Error:   return "Error";
    }
    return "unknown";
}

constexpr const char* level_color(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Debug:   return "\033[36m";
    case LogLevel::Info:    return "\033[32m";
    case LogLevel::Warning: return "\033[33m";
    case LogLevel::Error:   return "\033[31m";
    }
    return "\033[0m";
}

constexpr const char* color_reset = "\033[0m";

struct CachedTime {
    std::string str;
    std::chrono::seconds epoch{0};
};
thread_local CachedTime t_cached_time;

std::string_view get_cached_time() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto sec = duration_cast<seconds>(now.time_since_epoch());
    if (sec != t_cached_time.epoch) {
        t_cached_time.epoch = sec;
        time_t tt = system_clock::to_time_t(now);
        tm tm_buf;
#if defined(_WIN32)
        localtime_s(&tm_buf, &tt);
#else
        localtime_r(&tt, &tm_buf);
#endif
        t_cached_time.str =
            std::format("[{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}]",
                        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1,
                        tm_buf.tm_mday, tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
    }
    return t_cached_time.str;
}

thread_local std::string t_buffer;

void format_log_message(LogLevel level, std::string_view msg,
                        const std::source_location* loc, bool color_enabled) {
    t_buffer.clear();
    // 预分配常见日志长度，减少扩容
    t_buffer.reserve(256);

    // 1. 日期时间
    t_buffer.append(get_cached_time());

    // 2. 级别（带颜色）
    t_buffer.append(" [");
    if (color_enabled) {
        t_buffer.append(level_color(level));
        t_buffer.append(level_to_string(level));
        t_buffer.append(color_reset);
    } else {
        t_buffer.append(level_to_string(level));
    }
    t_buffer.append("]");

    // 3. 位置信息（如果有）：文件名、行号、函数名
    if (loc != nullptr) {
        // 文件名（仅基名）
        std::string_view file = loc->file_name();
        if (auto pos = file.find_last_of("/\\"); pos != std::string_view::npos)
            file = file.substr(pos + 1);
        t_buffer.append(" [");
        t_buffer.append(file);
        t_buffer.append("]");

        // 行号和函数名：使用 format_to 直接追加，避免临时字符串
        std::format_to(std::back_inserter(t_buffer), " [{}] [{}] ", loc->line(), loc->function_name());
    } else {
        t_buffer.append(" ");
    }

    // 4. 日志信息
    t_buffer.append(msg);
}
} // anonymous namespace

void Log::log_impl(LogLevel level, std::string_view msg, const std::source_location* loc) {
    format_log_message(level, msg, loc, color_enabled_);
    // std::println 是线程安全的，无需额外加锁
    std::println("{}", t_buffer);
}