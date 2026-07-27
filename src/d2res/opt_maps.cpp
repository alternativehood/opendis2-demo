#include "opt_maps.hpp"
#include "byte_reader.hpp"
#include <cctype>
#include <string>
#include <unordered_set>

namespace d2res {

static std::vector<std::string> validate_opt_maps(const IndexMap&      index_map,
                                                  const ImageMap&      image_map,
                                                  const AnimMap&       anim_map,
                                                  const MqdbContainer& container) {
    std::vector<std::string> warnings;

    // Build set of valid MQRC record ids for fast lookup
    std::unordered_set<int32_t> valid_ids;
    for (const auto& rec : container.records())
        valid_ids.insert(rec.id);

    for (const auto& entry : index_map.entries) {
        if (entry.is_animation())
            continue;

        // Check that the base PNG record exists in the container
        if (!valid_ids.contains(entry.id)) {
            warnings.push_back("image '" + entry.name +
                               "' references missing MQRC id: " + std::to_string(entry.id));
        }

        // Check that the frame name exists in ImageMap
        if (!image_map.frame_name_to_block.contains(entry.name)) {
            warnings.push_back("image '" + entry.name + "' not found in ImageMap");
        }
    }

    // Build uppercase-keyed name set for fast AnimMap frame checks
    for (const auto& anim : anim_map.blocks) {
        for (const auto& frame_name : anim.frames) {
            std::string key = frame_name;
            for (char& c : key)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (!index_map.name_to_index.contains(key)) {
                warnings.push_back("anim '" + anim.name + "' references unknown frame: '" +
                                   frame_name + "'");
            }
        }
    }

    return warnings;
}

OptMaps parse_index_and_image_maps(const MqdbContainer& container) {
    auto index_rec = container.find_by_name("-INDEX.OPT");
    auto images_rec = container.find_by_name("-IMAGES.OPT");

    if (!index_rec.has_value())
        throw ParseError("container has no -INDEX.OPT record", 0);
    if (!images_rec.has_value())
        throw ParseError("container has no -IMAGES.OPT record", 0);

    auto index_view = container.payload_view(index_rec->index);
    auto images_view = container.payload_view(images_rec->index);

    OptMaps result;

    result.index_map = IndexParser::parse(index_view);
    for (auto& w : result.index_map.warnings)
        result.warnings.push_back(std::move(w));

    result.image_map = ImagesParser::parse(images_view);
    for (auto& w : result.image_map.warnings)
        result.warnings.push_back(std::move(w));

    return result;
}

OptMaps parse_image_opt_maps(const MqdbContainer& container) {
    OptMaps result = parse_index_and_image_maps(container);
    auto    val_warnings =
        validate_opt_maps(result.index_map, result.image_map, result.anim_map, container);
    for (auto& w : val_warnings)
        result.warnings.push_back(std::move(w));
    return result;
}

OptMaps parse_opt_maps(const MqdbContainer& container) {
    OptMaps result = parse_index_and_image_maps(container);
    auto    anims_rec = container.find_by_name("-ANIMS.OPT");
    if (!anims_rec.has_value())
        throw ParseError("container has no -ANIMS.OPT record", 0);

    auto anims_view = container.payload_view(anims_rec->index);

    std::vector<std::string> anim_names;
    for (const auto& entry : result.index_map.entries) {
        if (entry.is_animation())
            anim_names.push_back(entry.name);
    }

    result.anim_map = AnimsParser::parse(anims_view, anim_names);
    for (auto& w : result.anim_map.warnings)
        result.warnings.push_back(std::move(w));

    auto val_warnings =
        validate_opt_maps(result.index_map, result.image_map, result.anim_map, container);
    for (auto& w : val_warnings)
        result.warnings.push_back(std::move(w));

    return result;
}

} // namespace d2res
