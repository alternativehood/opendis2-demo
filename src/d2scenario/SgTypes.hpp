#pragma once

// ── Full SG parse types (semantic + diagnostics) ─────────────────────────
//
// Includes ScenarioTemplate.hpp for the runtime semantic model, plus all
// parser/debug-only types: object index, raw objects, global-id provenance
// records, verified-empty hulls, and the SgParseResult wrapper.
//
// Code that only needs the semantic model should include ScenarioTemplate.hpp
// instead of this file.

#include "ScenarioTemplate.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace d2scenario {

constexpr std::string_view kSignature = "D2EESFISIG";

// ── Strong ID types for the scenario/diagnostic boundary ─────────────────

struct SgGlobalId {
    std::string value;

    bool        is_null() const { return value == "G000000000"; }
    bool        is_valid() const { return !value.empty(); }
    std::string prefix() const {
        if (value.size() >= 7)
            return value.substr(0, 7);
        return value;
    }

    bool operator==(const SgGlobalId& o) const { return value == o.value; }
    bool operator<(const SgGlobalId& o) const { return value < o.value; }
};

struct SgScenarioObjectId {
    std::string value;

    bool is_valid() const { return !value.empty(); }
    bool operator==(const SgScenarioObjectId& o) const { return value == o.value; }
    bool operator<(const SgScenarioObjectId& o) const { return value < o.value; }
};

// ── Field provenance (parser debugging) ──────────────────────────────────

struct SgFieldProvenance {
    std::string field_name;
    std::size_t offset_in_object = 0;
    std::size_t offset_in_file = 0;
};

// ── Global ID provenance record ──────────────────────────────────────────
//
// Records a single reference to a global ID found in a parsed scenario object.
// Used by d2analysis to build reports and by the CLI for JSON/CSV output.
// Fields use strong ID types at the boundary — d2analysis converts these to
// d2gamedata::GlobalId / d2gamedata::ScenarioObjectId when crossing layers.
//
struct SgGlobalIdUsage {
    SgGlobalId         value;
    SgScenarioObjectId object_id;
    std::string        class_name;
    std::string        field_name;
    int                pos_x = -1;
    int                pos_y = -1;
};

enum class SgObjectClassification : uint8_t { Parsed, VerifiedEmptyInitialState, Unknown };

struct SgObjectIndexEntry {
    std::size_t            offset = 0;
    std::size_t            length = 0;
    std::string            class_name;
    SgObjectId             obj_id;
    SgObjectClassification classification = SgObjectClassification::Unknown;
};

struct SgRawObject {
    std::size_t          offset = 0;
    std::size_t          length = 0;
    std::string          class_name;
    SgObjectId           obj_id;
    std::vector<uint8_t> raw_bytes;
};

struct SgMidUnitWire {
    SgObjectId object_id;

    SgObjectId   unit_id;
    SgObjectId   type_id;
    std::int32_t level = 0;

    std::array<char, 10> inner_unit_id{};

    std::vector<SgObjectId> modifier_ids;

    std::int32_t              creation = 0;
    std::vector<std::uint8_t> name_text_raw;
    std::string               name_text;

    std::uint8_t transformed = 0;

    bool         has_dynamic_level = false;
    std::uint8_t dynamic_level = 0;

    std::vector<std::uint8_t> pre_hp_padding;

    std::int32_t hp = 0;
    std::int32_t xp = 0;
};

// ── Verified-empty hull types (no semantic payload) ──────────────────────

struct SgSpellCast {
    SgObjectId           id;
    std::vector<uint8_t> raw_data;
};

struct SgSpellEffects {
    SgObjectId           id;
    std::vector<uint8_t> raw_data;
};

struct SgStackDestroyed {
    SgObjectId           id;
    std::vector<uint8_t> raw_data;
};

struct SgQuestLog {
    SgObjectId           id;
    std::vector<uint8_t> raw_data;
};

// ── Full parse result with diagnostics ────────────────────────────────────
//
// Contains a ScenarioTemplate for semantic data, plus all diagnostic/debug
// data: object_index, raw_objects, classification maps, global_id_usages,
// file_path, file_size, parse_warnings, and empty hull vectors.
//
struct SgParseResult {
    ScenarioTemplate scenario;

    std::vector<SgObjectIndexEntry> object_index;
    std::vector<SgRawObject>        raw_objects;
    std::vector<SgMidUnitWire>      unit_wires;

    std::map<std::string, std::vector<SgObjectIndexEntry>> parsed_objects;
    std::map<std::string, std::vector<SgObjectIndexEntry>> verified_empty_objects;
    std::map<std::string, std::vector<SgObjectIndexEntry>> unknown_objects;

    // Verified-empty hulls (no semantic payload, stored for completeness).
    std::vector<SgSpellCast>      spell_casts;
    std::vector<SgSpellEffects>   spell_effects;
    std::vector<SgStackDestroyed> stacks_destroyed;
    std::vector<SgQuestLog>       quest_logs;

    std::vector<SgGlobalIdUsage> global_id_usages;

    std::string              file_path;
    std::size_t              file_size = 0;
    std::vector<std::string> parse_warnings;
};

} // namespace d2scenario
