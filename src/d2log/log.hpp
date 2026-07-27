#pragma once

// Enable TRACE/DEBUG macros in all builds; runtime level filters at init time.
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace d2log {

struct LogConfig {
    std::string level; // empty → D2_LOG_LEVEL env → "info"
    std::string file;  // empty → D2_LOG_FILE env → stderr only
};

// Call once at process start. Reads D2_LOG_LEVEL / D2_LOG_FILE as fallbacks.
void init(const LogConfig& config);
void shutdown();
void write_fatal_stderr(const char* message) noexcept;

// Returns a named component logger (created on first call, cached).
// Safe to call before init() — falls back to stderr with default format.
std::shared_ptr<spdlog::logger> get(const std::string& component);

} // namespace d2log

// Convenience macros: compile-time level guard via SPDLOG_ACTIVE_LEVEL.
// Usage: D2_LOG_INFO(kLog, "loaded {} units", count);
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define D2_LOG_TRACE(logger, ...) SPDLOG_LOGGER_TRACE(logger, __VA_ARGS__)
#define D2_LOG_DEBUG(logger, ...) SPDLOG_LOGGER_DEBUG(logger, __VA_ARGS__)
#define D2_LOG_INFO(logger, ...) SPDLOG_LOGGER_INFO(logger, __VA_ARGS__)
#define D2_LOG_WARN(logger, ...) SPDLOG_LOGGER_WARN(logger, __VA_ARGS__)
#define D2_LOG_ERROR(logger, ...) SPDLOG_LOGGER_ERROR(logger, __VA_ARGS__)
// NOLINTEND(cppcoreguidelines-macro-usage)
