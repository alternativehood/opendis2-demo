#pragma once

#include <string_view>

namespace d2engine {
namespace schemas {

// Canonical JSON Schema for battle_screen.json
inline std::string_view battle_screen() noexcept {
    return R"({
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "BattleVisualConfig",
  "description": "Schema for configs/screens/battle_screen.json — structural validation only. Semantic validation (asset existence, role/profile resolution) stays in C++.",
  "type": "object",
  "properties": {
    "render_tree": {
      "type": "object",
      "description": "TreeLayout node definitions",
      "patternProperties": {
        "^/": {
          "type": "object",
          "properties": {
            "kind": { "type": "string" },
            "x": { "type": "integer" },
            "y": { "type": "integer" },
            "w": { "type": "integer" },
            "h": { "type": "integer" },
            "alpha": { "type": "number" },
            "level": { "type": "integer" },
            "font_size": { "type": "integer" },
            "asset": { "type": "string" },
            "color": {
              "type": "object",
              "properties": {
                "r": { "type": "integer" },
                "g": { "type": "integer" },
                "b": { "type": "integer" },
                "a": { "type": "integer" }
              },
              "additionalProperties": false
            }
          },
          "additionalProperties": false
        }
      }
    },
    "scene_layout": {
      "type": "object",
      "description": "Per-role scene placement overrides"
    },
    "unit_visual_profiles": {
      "type": "object",
      "description": "Per-unit-type visual profile overrides"
    },
    "unit_visual_layer_profiles": {
      "type": "object",
      "description": "Per-layer unit visual profiles"
    },
    "unit_visual_layer_default_profiles": {
      "type": "object",
      "description": "Shared layer-default profiles"
    },
    "battle_effect_profiles": {
      "type": "object",
      "description": "Typed effect visual placements"
    },
    "battle_effect_default_profiles": {
      "type": "object",
      "description": "Shared effect-default profiles"
    },
    "unit_lifecycle_profiles": {
      "type": "object",
      "description": "Lifecycle visual profiles"
    },
    "sprite_profiles": {
      "type": "object",
      "description": "Shared sprite-level profiles"
    },
    "position_levels": {
      "type": "object",
      "description": "Named position draw levels"
    },
    "unit_attack_visual_intents": {
      "type": "object",
      "description": "Per-unit attack visual intent configuration"
    },
    "layout_metrics": {
      "type": "object",
      "description": "Reference canvas and slot dimensions",
      "properties": {
        "reference_size": { "type": "object" },
        "slot_hitbox": { "type": "object" },
        "battlefield_reference_rect": { "type": "object" }
      },
      "additionalProperties": false
    },
    "debug_overlay_style": {
      "type": "object",
      "description": "Debug overlay colors and radii"
    },
    "font": {
      "type": "object",
      "properties": {
        "face": { "type": "string" }
      },
      "additionalProperties": false
    }
  },
  "additionalProperties": false
}
)";
}

} // namespace schemas
} // namespace d2engine
