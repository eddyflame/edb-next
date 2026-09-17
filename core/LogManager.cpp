#include "LogManager.hpp"
#include <iomanip>
#include <sstream>
#include <iostream>

namespace edb_next {

std::string LogEntry::formatTime() const {
    auto in_time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;

    std::tm bt{};
    localtime_r(&in_time_t, &bt);

    return std::format("{:02d}:{:02d}:{:02d}.{:03d}",
                       bt.tm_hour, bt.tm_min, bt.tm_sec, static_cast<int>(ms.count()));
}

std::string LogEntry::levelString() const {
    switch (level) {
        case LogLevel::Info:       return "INFO";
        case LogLevel::Debug:      return "DEBUG";
        case LogLevel::Event:      return "EVENT";
        case LogLevel::Breakpoint: return "BP";
        case LogLevel::Trace:      return "TRACE";
        case LogLevel::Plugin:     return "PLUGIN";
        case LogLevel::Command:    return "CMD";
        case LogLevel::Warning:    return "WARN";
        case LogLevel::Error:      return "ERROR";
    }
    return "UNKNOWN";
}

LogManager& LogManager::instance() {
    static LogManager inst;
    return inst;
}

void LogManager::log(LogLevel level, const std::string& category, const std::string& message) {
    LogEntry entry{
        .timestamp = std::chrono::system_clock::now(),
        .level = level,
        .category = category,
        .message = message
    };

    LogCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back(entry);
        cb = callback_;
    }

    if (cb) {
        cb(entry);
    }
}

void LogManager::info(const std::string& category, const std::string& message) {
    log(LogLevel::Info, category, message);
}

void LogManager::event(const std::string& category, const std::string& message) {
    log(LogLevel::Event, category, message);
}

void LogManager::bp(const std::string& category, const std::string& message) {
    log(LogLevel::Breakpoint, category, message);
}

void LogManager::trace(const std::string& category, const std::string& message) {
    log(LogLevel::Trace, category, message);
}

void LogManager::plugin(const std::string& category, const std::string& message) {
    log(LogLevel::Plugin, category, message);
}

void LogManager::cmd(const std::string& category, const std::string& message) {
    log(LogLevel::Command, category, message);
}

void LogManager::error(const std::string& category, const std::string& message) {
    log(LogLevel::Error, category, message);
}

std::vector<LogEntry> LogManager::entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

void LogManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

void LogManager::setCallback(LogCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

} // namespace edb_next
