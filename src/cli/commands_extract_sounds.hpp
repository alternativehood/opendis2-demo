#pragma once
#include <string>

int cmd_extract_sounds(const std::string& container_path, const std::string& out_dir,
                       const std::string& pattern, bool all);
