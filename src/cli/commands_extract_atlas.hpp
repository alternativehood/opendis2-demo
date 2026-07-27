#pragma once
#include <string>

namespace d2res {
class MqdbContainer;
struct OptMaps;
} // namespace d2res

int cmd_extract_atlas(const std::string& container_path, const std::string& out_dir,
                      const std::string& pattern, bool all, int max_size);

int cmd_extract_atlas_from_container(const d2res::MqdbContainer& container,
                                     const d2res::OptMaps& maps, const std::string& out_dir,
                                     const std::string& pattern, bool all, int max_size);
