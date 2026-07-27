#pragma once
#include <string>

int cmd_compare_images(const std::string& actual_dir, const std::string& expected_dir,
                       const std::string& report_path, int sample_n, bool all_mode,
                       const std::string& diff_dir, const std::string& game_dir = "");
