#pragma once

#include "rect.hpp"
#include "vec2.hpp"

#include <optional>
#include <string>
#include <vector>

namespace d2engine {

struct TunablePropertyValue {
    float x = 0.0f;
    float y = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float alpha = 1.0f;
    int   level = 0;
    int   frame_delay = 0;

    [[nodiscard]] bool operator==(const TunablePropertyValue&) const = default;
};

struct TunableProperty {
    std::string          name;
    TunablePropertyValue value;
};

struct TuningBinding {
    std::string domain;
    std::string owner_kind;
    std::string owner_id;
    std::string role;
    std::string property;
    std::string config_path;
    std::string profile_id;
    std::string layer_id;
    std::string tree_path;
    std::string side;
    std::string display_path;

    [[nodiscard]] bool writable() const {
        return !config_path.empty() && (!owner_id.empty() || !profile_id.empty());
    }
};

struct TunableRenderItem {
    std::string                  stable_id;
    std::string                  label;
    std::string                  tree_path;
    Rect                         bounds;
    std::string                  kind;
    int                          layer = 0;
    std::string                  resource_key;
    std::string                  current_frame_name;
    Rect                         logical_rect;
    Rect                         screen_rect;
    Vec2                         anchor;
    bool                         selectable = true;
    bool                         visible = true;
    std::vector<TunableProperty> properties;
    std::optional<TuningBinding> binding;
    std::optional<TuningBinding> default_binding;
    std::string                  visual_category;
    std::string                  render_side;
    std::string                  asset_direction;
};

class ITuningBindingResolver {
public:
    ITuningBindingResolver() = default;
    virtual ~ITuningBindingResolver() = default;

    ITuningBindingResolver(const ITuningBindingResolver&) = delete;
    ITuningBindingResolver& operator=(const ITuningBindingResolver&) = delete;
    ITuningBindingResolver(ITuningBindingResolver&&) = delete;
    ITuningBindingResolver& operator=(ITuningBindingResolver&&) = delete;

    [[nodiscard]] virtual bool can_resolve(const TuningBinding& binding) const = 0;
    [[nodiscard]] virtual std::optional<TunablePropertyValue>
                 read_property(const TuningBinding& binding) const = 0;
    virtual bool write_property(const TuningBinding&        binding,
                                const TunablePropertyValue& value) = 0;
};

} // namespace d2engine
