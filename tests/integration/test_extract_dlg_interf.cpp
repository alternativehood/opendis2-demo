#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

#include "cli/commands_extract_dlg.hpp"
#include "tests/test_helpers.hpp"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

static const char* GAME_ROOT = DISCIPLES2_GAME_ROOT;

TEST(ExtractDlgInterf, ProducesNonEmptyJson) {
    if ((GAME_ROOT == nullptr) || std::string(GAME_ROOT).empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    const fs::path src = fs::path(GAME_ROOT) / "Interf" / "Interf.dlg";
    if (!fs::exists(src))
        GTEST_SKIP() << "Interf.dlg not found: " << src;

    TempDir out("dlg_interf");
    ASSERT_EQ(cmd_extract_dlg(src.string(), out.str()), 0);
    EXPECT_TRUE(fs::exists(out.path() / "Interf.json"));

    std::ifstream f(out.path() / "Interf.json");
    ASSERT_TRUE(f);
    nlohmann::json j;
    f >> j;
    EXPECT_TRUE(j.is_array());
    EXPECT_FALSE(j.empty());
}
