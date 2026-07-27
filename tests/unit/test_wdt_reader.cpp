#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "d2res/wdt_reader.hpp"

namespace fs = std::filesystem;

TEST(WdtReader, AbsentFileReturnsError) {
    auto result = d2res::WdtReader::open("/nonexistent/path/file.wdt", "/tmp/wdt_test_absent");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
}

TEST(WdtReader, NonMqdbFileHexDump) {
    const fs::path out = fs::temp_directory_path() / "wdt_test_nonmqdb";
    fs::remove_all(out);
    fs::create_directories(out);

    // Write a non-MQDB temp file
    const fs::path tmp_file = fs::temp_directory_path() / "test_nonmqdb.wdt";
    {
        std::ofstream f(tmp_file, std::ios::binary);
        const char    data[] = "ABCD_NOT_MQDB";
        f.write(data, sizeof(data) - 1);
    }

    auto result = d2res::WdtReader::open(tmp_file.string(), out.string());
    EXPECT_EQ(result.format, "unknown");
    EXPECT_TRUE(fs::exists(out / "test_nonmqdb.hexdump.txt"));
    EXPECT_TRUE(fs::exists(out / "test_nonmqdb.raw.bin"));

    fs::remove_all(out);
    fs::remove(tmp_file);
}
