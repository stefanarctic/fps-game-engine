#include "Forge/Core/Log.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>

namespace Forge {

namespace {
constexpr size_t kMaxMessages = 4000;

struct LogState {
    std::mutex mutex;
    std::deque<LogMessage> messages;
    size_t counts[4] = {0, 0, 0, 0};
    uint64_t revision = 0;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
};

LogState& state() {
    static LogState s;
    return s;
}
} // namespace

const char* Log::levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "Trace";
    case LogLevel::Info: return "Info";
    case LogLevel::Warning: return "Warning";
    case LogLevel::Error: return "Error";
    }
    return "?";
}

void Log::write(LogLevel level, const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);

    LogMessage msg;
    msg.level = level;
    msg.text = buffer;
    msg.time = std::chrono::duration<double>(std::chrono::steady_clock::now() - s.start).count();

    const char* prefix = "[info] ";
    switch (level) {
    case LogLevel::Trace: prefix = "[trace] "; break;
    case LogLevel::Info: prefix = "[info] "; break;
    case LogLevel::Warning: prefix = "[warn] "; break;
    case LogLevel::Error: prefix = "[error] "; break;
    }
    std::fprintf(level >= LogLevel::Warning ? stderr : stdout, "%s%s\n", prefix, buffer);

    s.messages.push_back(std::move(msg));
    s.counts[(int)level]++;
    if (s.messages.size() > kMaxMessages) {
        s.counts[(int)s.messages.front().level]--;
        s.messages.pop_front();
    }
    s.revision++;
}

std::vector<LogMessage> Log::snapshot() {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return std::vector<LogMessage>(s.messages.begin(), s.messages.end());
}

size_t Log::count(LogLevel level) {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.counts[(int)level];
}

uint64_t Log::revision() {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.revision;
}

void Log::clear() {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.messages.clear();
    for (size_t& c : s.counts) c = 0;
    s.revision++;
}

} // namespace Forge
