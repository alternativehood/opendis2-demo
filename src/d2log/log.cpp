#include "log.hpp"

#include <spdlog/common.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <vector>

namespace d2log {

namespace {

// %z = UTC offset, %e = milliseconds
const char* kPattern = "%Y-%m-%dT%H:%M:%S.%e%z %-5l %-16n %v";

std::vector<spdlog::sink_ptr> g_sinks; // NOLINT(concurrency-mt-unsafe)
spdlog::level::level_enum     g_level = spdlog::level::info;
std::mutex                    g_mutex;

spdlog::level::level_enum parse_level(const std::string& s) {
    if (s == "trace")
        return spdlog::level::trace;
    if (s == "debug")
        return spdlog::level::debug;
    if (s == "warn")
        return spdlog::level::warn;
    if (s == "error")
        return spdlog::level::err;
    if (s == "off")
        return spdlog::level::off;
    return spdlog::level::info;
}

void reconfigure_all_loggers() {
    // Reconfigure every registered spdlog logger with the current sinks and level.
    spdlog::apply_all([&](const auto& logger) {
        logger->sinks().clear();
        for (const auto& sink : g_sinks) {
            logger->sinks().push_back(sink);
        }
        logger->set_level(g_level);
    });
}

} // namespace

void init(const LogConfig& config) {
    std::string level = config.level;
    if (level.empty()) {
        // Logger initialization reads process configuration before worker threads start.
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        if (const char* env = std::getenv("D2_LOG_LEVEL"); (env != nullptr) && env[0] != '\0') {
            level = env;
        } else {
            level = "info";
        }
    }
    std::string file = config.file;
    if (file.empty()) {
        // Logger initialization reads process configuration before worker threads start.
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        if (const char* env = std::getenv("D2_LOG_FILE"); (env != nullptr) && env[0] != '\0')
            file = env;
    }

    g_level = parse_level(level);

    std::scoped_lock const lock(g_mutex);
    g_sinks.clear();

    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    stderr_sink->set_pattern(kPattern);
    stderr_sink->set_level(g_level);
    g_sinks.push_back(std::move(stderr_sink));

    if (!file.empty()) {
        const auto parent = std::filesystem::path(file).parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file, true);
        file_sink->set_pattern(kPattern);
        file_sink->set_level(g_level);
        g_sinks.push_back(std::move(file_sink));
    }

    spdlog::set_level(g_level);
    reconfigure_all_loggers();
}

void shutdown() {
    spdlog::shutdown();
    std::scoped_lock const lock(g_mutex);
    g_sinks.clear();
}

void write_fatal_stderr(const char* message) noexcept {
    static_cast<void>(std::fputs("fatal: ", stderr));
    if (message != nullptr) {
        static_cast<void>(std::fputs(message, stderr));
    }
    static_cast<void>(std::fputc('\n', stderr));
    static_cast<void>(std::fflush(stderr));
}

std::shared_ptr<spdlog::logger> get(const std::string& component) {
    // Fast path: logger already registered.
    if (auto existing = spdlog::get(component))
        return existing;

    std::scoped_lock const lock(g_mutex);
    if (auto existing = spdlog::get(component))
        return existing;

    std::vector<spdlog::sink_ptr> sinks = g_sinks;
    if (sinks.empty()) {
        // Fallback when init() was not called (e.g. unit tests).
        auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        sink->set_pattern(kPattern);
        sinks.push_back(std::move(sink));
    }

    auto logger = std::make_shared<spdlog::logger>(component, sinks.begin(), sinks.end());
    logger->set_level(g_level);
    spdlog::register_logger(logger);
    return logger;
}

} // namespace d2log
