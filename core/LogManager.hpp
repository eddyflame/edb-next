#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <chrono>

namespace edb_next {

enum class LogLevel {
    Info,
    Debug,
    Event,
    Breakpoint,
    Trace,
    Plugin,
    Command,
    Warning,
    Error
};

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string category;
    std::string message;

    [[nodiscard]] std::string formatTime() const;
    [[nodiscard]] std::string levelString() const;
};

class LogManager {
public:
    using LogCallback = std::function<void(const LogEntry&)>;

    static LogManager& instance();

    void log(LogLevel level, const std::string& category, const std::string& message);
    void info(const std::string& category, const std::string& message);
    void event(const std::string& category, const std::string& message);
    void bp(const std::string& category, const std::string& message);
    void trace(const std::string& category, const std::string& message);
    void plugin(const std::string& category, const std::string& message);
    void cmd(const std::string& category, const std::string& message);
    void error(const std::string& category, const std::string& message);

    [[nodiscard]] std::vector<LogEntry> entries() const;
    void clear();

    void setCallback(LogCallback callback);

private:
    LogManager() = default;
    ~LogManager() = default;
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    mutable std::mutex mutex_;
    std::vector<LogEntry> entries_;
    LogCallback callback_;
};

} // namespace edb_next
