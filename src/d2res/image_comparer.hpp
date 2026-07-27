#pragma once
#include <array>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace d2res {

struct CompareResult {
    std::string        name;
    std::string        container;
    std::string        expected_path;
    std::string        actual_path;
    std::array<int, 2> dims_expected{};
    std::array<int, 2> dims_actual{};
    std::array<int, 2> first_diff_pixel{-1, -1};
    int                total_diff_pixels{};
    double             pct_diff{};
};

struct SampleEntry {
    std::string container;
    int         record_index{};
    std::string name;
};

struct CompareReport {
    int                        total_compared{};
    int                        passed{};
    int                        failed{};
    int                        skipped{};
    std::vector<SampleEntry>   sample_list;
    std::vector<CompareResult> failures;

    [[nodiscard]] nlohmann::json to_json() const;
};

class ImageComparer {
public:
    // actual_dir: directory produced by our extract-images / extract-anim
    // expected_dir: reference dataset root (d2_data_extacted/)
    // sample_n: max pairs to compare; 0 = --all mode
    // diff_dir: where to write diff PNGs (may be empty = skip)
    // game_dir: optional path to game root, used to build idx→name maps
    [[nodiscard]] static CompareReport run(const std::string& actual_dir,
                                           const std::string& expected_dir, int sample_n,
                                           bool all_mode, const std::string& diff_dir,
                                           const std::string& game_dir = "");
};

} // namespace d2res
