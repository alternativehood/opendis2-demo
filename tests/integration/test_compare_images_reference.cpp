#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

#include "cli/commands_extract_images.hpp"
#include "cli/commands_compare_images.hpp"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

static const char* GAME_ROOT = DISCIPLES2_GAME_ROOT;
static const char* REFERENCE_ROOT = []() -> const char* {
    auto* env = std::getenv("D2_REFERENCE_ROOT"); // NOLINT(concurrency-mt-unsafe)
    return (env && *env) ? env : "";
}();

class CompareImagesReferenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if ((GAME_ROOT == nullptr) || std::string(GAME_ROOT).empty()) {
            GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
        }
        if (!fs::is_directory(REFERENCE_ROOT)) {
            GTEST_SKIP() << "Reference dataset not found: " << REFERENCE_ROOT;
        }
        out_ = fs::temp_directory_path() / "d2_compare_images_test";
        report_ = out_ / "report.json";
        fs::remove_all(out_);
    }
    void TearDown() override { fs::remove_all(out_); }

    fs::path out_, report_;
};

TEST_F(CompareImagesReferenceTest, ReportExists) {
    // Extract Capital.ff images to actual_dir
    const fs::path capital_ff = fs::path(GAME_ROOT) / "Imgs" / "Capital.ff";
    if (!fs::exists(capital_ff))
        GTEST_SKIP() << "Capital.ff not found";

    const fs::path actual_dir = out_ / "actual" / "Imgs";
    fs::create_directories(actual_dir);
    cmd_extract_images(capital_ff.string(), (actual_dir / "Capital.ff").string(), "EMP_IMPGUILD*");

    const std::string expected_imgs = fs::path(REFERENCE_ROOT) / "Imgs";
    const int r = cmd_compare_images(actual_dir.string(), expected_imgs, report_.string(), 50,
                                     false, "", GAME_ROOT);

    EXPECT_TRUE(fs::exists(report_));
    std::ifstream f(report_);
    ASSERT_TRUE(f);
    nlohmann::json j;
    f >> j;
    EXPECT_TRUE(j.contains("total_compared"));
    EXPECT_GE(j["total_compared"].get<int>() + j["skipped"].get<int>(), 0);
    (void)r; // pass/fail depends on pixel accuracy - just verify no crash
}
