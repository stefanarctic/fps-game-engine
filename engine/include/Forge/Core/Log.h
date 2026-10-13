#pragma once
#include "Forge/Core/Base.h"

namespace Forge {

enum class LogLevel { Trace = 0, Info, Warning, Error };

struct LogMessage {
    LogLevel level = LogLevel::Info;
    std::string text;
    double time = 0.0; // seconds since application start
};

// Thread-safe global logger. Messages are printed to stdout and kept in a
// ring buffer so the editor Console panel can display them.
class Log {
public:
    static void write(LogLevel level, const char* fmt, ...);

    // Copies the buffered messages (thread safe).
    static std::vector<LogMessage> snapshot();
    static size_t count(LogLevel level);
    static uint64_t revision(); // increments on every new message / clear
    static void clear();

    static const char* levelName(LogLevel level);
};

} // namespace Forge

#define LOG_TRACE(...) ::Forge::Log::write(::Forge::LogLevel::Trace, __VA_ARGS__)
#define LOG_INFO(...)  ::Forge::Log::write(::Forge::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...)  ::Forge::Log::write(::Forge::LogLevel::Warning, __VA_ARGS__)
#define LOG_ERROR(...) ::Forge::Log::write(::Forge::LogLevel::Error, __VA_ARGS__)
