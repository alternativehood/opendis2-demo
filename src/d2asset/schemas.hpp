#pragma once

#include <string>
#include <string_view>

namespace d2asset {
namespace schemas {

// JSON Schema for game_manifest.json (runtime asset manifest)
inline std::string_view runtime_asset_manifest() noexcept {
    return R"({
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "RuntimeAssetManifest",
  "description": "Schema for game_manifest.json — structural validation. Semantic checks (path exists, asset ID uniqueness) stay in C++.",
  "type": "object",
  "properties": {
    "asset_schema_version": {
      "type": "integer",
      "description": "Schema version number"
    },
    "containers": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "container_id": { "type": "string", "minLength": 1 },
          "path": { "type": "string", "minLength": 1 },
          "content_kinds": {
            "type": "array",
            "items": { "type": "string" },
            "minItems": 0
          }
        },
        "required": ["container_id", "path", "content_kinds"],
        "additionalProperties": false
      },
      "minItems": 0
    },
    "assets": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "asset_id": { "type": "string", "minLength": 1 },
          "logical_name": { "type": "string", "minLength": 1 },
          "type": { "type": "string", "minLength": 1 },
          "container_id": { "type": "string", "minLength": 1 },
          "path": { "type": "string", "minLength": 1 }
        },
        "required": ["asset_id", "logical_name", "type", "container_id", "path"],
        "additionalProperties": false
      },
      "minItems": 0
    },
    "warnings": {
      "type": "array",
      "items": { "type": "string" }
    },
    "game_root": { "type": "string" }
  },
  "required": ["asset_schema_version", "containers", "assets", "warnings"],
  "additionalProperties": false
}
)";
}

// JSON Schema for atlas sidecar manifest
inline std::string_view atlas_manifest() noexcept {
    return R"({
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "AtlasManifest",
  "description": "Schema for atlas sidecar JSON — structural validation. Semantic checks (sheet file exists, rectangle bounds) stay in C++.",
  "type": "object",
  "properties": {
    "source_container": { "type": "string" },
    "max_sheet_size": { "type": "integer", "minimum": 1 },
    "sheet_count": { "type": "integer", "minimum": 0 },
    "total_sprites": { "type": "integer", "minimum": 0 },
    "skipped_sprites": { "type": "integer", "minimum": 0 },
    "skipped": { "type": "array", "items": { "type": "string" } },
    "entries": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "name": { "type": "string" },
          "sheet": { "type": "integer", "minimum": 0 },
          "x": { "type": "integer", "minimum": 0 },
          "y": { "type": "integer", "minimum": 0 },
          "w": { "type": "integer", "minimum": 1 },
          "h": { "type": "integer", "minimum": 1 }
        },
        "required": ["name", "sheet", "x", "y", "w", "h"],
        "additionalProperties": false
      }
    }
  },
  "required": ["max_sheet_size", "sheet_count", "total_sprites", "skipped_sprites", "entries"],
  "additionalProperties": false
}
)";
}

} // namespace schemas
} // namespace d2asset
