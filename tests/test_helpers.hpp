#pragma once

#include "tests/test_process.hpp"

#include <filesystem>
#include <string>
#include <system_error>

// RAII temporary directory with a unique path per instance.
// Uses test name + process-unique counter for collision-free parallel execution.
class TempDir {
public:
    explicit TempDir(const std::string& label) {
        static std::size_t counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("opendis2_" + label + "_" + std::to_string(test_support::process_id()) + "_" +
                 std::to_string(++counter));
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_, ec);
    }

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }
    [[nodiscard]] std::string                  str() const { return path_.string(); }

private:
    std::filesystem::path path_;
};
