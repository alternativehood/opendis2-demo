#include <gtest/gtest.h>
#include <filesystem>
#include <vector>
#include <cstdint>

#include "d2res/image_comparer.hpp"
#include <lodepng.h>

namespace fs = std::filesystem;

static void write_png(const fs::path& path, const std::vector<uint8_t>& rgba, unsigned w,
                      unsigned h) {
    fs::create_directories(path.parent_path());
    lodepng::encode(path.string(), rgba, w, h);
}

static std::vector<uint8_t> solid_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a, unsigned w,
                                       unsigned h) {
    std::vector<uint8_t> px(static_cast<std::size_t>(w) * h * 4U);
    for (std::size_t i = 0; i < px.size(); i += 4) {
        px[i] = r;
        px[i + 1] = g;
        px[i + 2] = b;
        px[i + 3] = a;
    }
    return px;
}

class ImageComparerTest : public ::testing::Test {
protected:
    void SetUp() override {
        base_ = fs::temp_directory_path() / "d2_cmp_unit_test";
        fs::remove_all(base_);
        expected_dir_ = base_ / "expected";
        actual_dir_ = base_ / "actual";
        diff_dir_ = base_ / "diffs";
        fs::create_directories(expected_dir_);
        fs::create_directories(actual_dir_);
    }
    void TearDown() override { fs::remove_all(base_); }

    // Helper: create a reference pair under expected/Container.ff/<idx>/ImageMap#<idx>#0.png
    // and actual/Container/<idx>/frame_000.png
    void make_pair(const std::string& container, int idx, const std::vector<uint8_t>& exp_px,
                   const std::vector<uint8_t>& act_px, unsigned w, unsigned h) {
        const std::string idx_s = std::to_string(idx);
        const fs::path    exp_f =
            expected_dir_ / container / idx_s / ("ImageMap#" + idx_s + "#000.png");
        write_png(exp_f, exp_px, w, h);

        const std::string stem = fs::path(container).stem().string();
        const fs::path    act_f = actual_dir_ / stem / idx_s / "frame_000.png";
        write_png(act_f, act_px, w, h);
    }

    fs::path base_, expected_dir_, actual_dir_, diff_dir_;
};

TEST_F(ImageComparerTest, IdenticalImagesPass) {
    auto px = solid_rgba(255, 0, 0, 255, 2, 2);
    make_pair("Test.ff", 0, px, px, 2, 2);

    auto report =
        d2res::ImageComparer::run(actual_dir_.string(), expected_dir_.string(), 0, true, "");
    EXPECT_EQ(report.total_compared, 1);
    EXPECT_EQ(report.passed, 1);
    EXPECT_EQ(report.failed, 0);
}

TEST_F(ImageComparerTest, DifferentImagesFailWithDiff) {
    auto red = solid_rgba(255, 0, 0, 255, 2, 2);
    auto blue = solid_rgba(0, 0, 255, 255, 2, 2);
    make_pair("Test.ff", 0, red, blue, 2, 2);

    auto report = d2res::ImageComparer::run(actual_dir_.string(), expected_dir_.string(), 0, true,
                                            diff_dir_.string());
    EXPECT_EQ(report.failed, 1);
    EXPECT_GT(report.failures[0].total_diff_pixels, 0);
    // Diff file should exist
    EXPECT_FALSE(fs::is_empty(diff_dir_));
}

TEST_F(ImageComparerTest, SampleLimitRespected) {
    auto px = solid_rgba(100, 100, 100, 255, 2, 2);
    for (int i = 0; i < 5; ++i)
        make_pair("Test.ff", i, px, px, 2, 2);

    auto report =
        d2res::ImageComparer::run(actual_dir_.string(), expected_dir_.string(), 3, false, "");
    EXPECT_EQ(report.total_compared + report.skipped, 3);
}

TEST_F(ImageComparerTest, ReportJsonKeys) {
    auto report =
        d2res::ImageComparer::run(actual_dir_.string(), expected_dir_.string(), 0, true, "");
    auto j = report.to_json();
    EXPECT_TRUE(j.contains("total_compared"));
    EXPECT_TRUE(j.contains("passed"));
    EXPECT_TRUE(j.contains("failed"));
    EXPECT_TRUE(j.contains("skipped"));
    EXPECT_TRUE(j.contains("sample_list"));
    EXPECT_TRUE(j.contains("failures"));
}
