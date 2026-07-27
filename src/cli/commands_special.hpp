#pragma once
#include <cstddef>
#include <string>

int cmd_extract_special(const std::string& container_path, const std::string& out_dir,
                        std::size_t hexdump_limit);
