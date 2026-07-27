#pragma once

#include <string>

int cmd_sg_inspect(const std::string& file_path, bool summary, const std::string& dump_json_path,
                   const std::string& dump_terrain_csv_path,
                   const std::string& dump_objects_csv_path, const std::string& globals_dir,
                   bool report_global_ids, bool annotate_ids,
                   const std::string& dump_terrain_debug_csv_path = {});