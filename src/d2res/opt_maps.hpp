#pragma once
#include "opt_anims.hpp"
#include "opt_images.hpp"
#include "opt_index.hpp"
#include "mqdb.hpp"
#include <string>
#include <vector>

namespace d2res {

struct OptMaps {
    IndexMap                 index_map;
    ImageMap                 image_map;
    AnimMap                  anim_map;
    std::vector<std::string> warnings;
};

OptMaps parse_image_opt_maps(const MqdbContainer& container);
OptMaps parse_opt_maps(const MqdbContainer& container);

} // namespace d2res
