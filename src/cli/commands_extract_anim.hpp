#pragma once
#include <string>

namespace d2res {
class MqdbContainer;
struct OptMaps;
} // namespace d2res

int cmd_extract_anim(const std::string& container_path, const std::string& out_dir,
                     const std::string& name, const std::string& pattern, bool all, bool gif,
                     int frame_delay_ms, bool atlas = false, int atlas_max_size = 4096);

int cmd_extract_anim_from_container(const d2res::MqdbContainer& container,
                                    const d2res::OptMaps& maps, const std::string& out_dir,
                                    const std::string& name, const std::string& pattern, bool all,
                                    bool gif, int frame_delay_ms, bool atlas = false,
                                    int atlas_max_size = 4096);
