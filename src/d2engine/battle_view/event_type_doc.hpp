#pragma once

#include "battle_scenario_data.hpp"
#include "battle_visual_event.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace d2engine {

// Marker embedded in generated/patched scenario headers.
// Marker embedded in generated/patched scenario headers.
inline constexpr std::string_view SCENARIO_DOC_HEADER_MARKER =
    "// Header schema: battle_scenario_doc_v2\n";

// ─────────────────────────────────────────────────────────────────────────────
// Metadata for scenario event types — each variant alternative in
// BattleScenarioEvent gets a doc entry. If you add a new type to the variant
// you MUST add an event_types_doc_for<T>() specialization below or the
// static_assert in build_scenario_event_types_doc() will fail at compile time.
// ─────────────────────────────────────────────────────────────────────────────

enum class EventDocCategory : std::uint8_t { Lifecycle, Visual };

struct EventDocEntry {
    const char*      type_name;
    EventDocCategory category;
    std::string_view json_example;
    std::string_view fields;
    std::string_view notes;
};

template <typename T> struct event_types_doc_for;

// ── Lifecycle ────────────────────────────────────────────────────────────────

template <> struct event_types_doc_for<ScenarioUnitCreated> {
    static constexpr EventDocEntry entry = {
        "UnitCreated", EventDocCategory::Lifecycle,
        R"({"type":"UnitCreated","unit":"summon","unit_type":"UU0001","slot":"A_FRONT_0"})"
        "\n"
        R"({"type":"UnitCreated","unit":"dragon","unit_type":"UU5021","slot":"D_CENTER_1"})"
        "\n"
        R"({"type":"UnitCreated","unit":"hero","unit_type":"UU0003","slot":"A_BACK_1","hp":150,"max_hp":200,"status":["bless"]})",
        "unit, unit_type, slot [, hp, max_hp, status[]]",
        "Must be in lifecycle-only steps; no at/cue scheduling. "
        "Small units use FRONT or BACK slots. "
        "Large units use CENTER slots and occupy FRONT+BACK of the same lane."};
};

template <> struct event_types_doc_for<ScenarioUnitRetreated> {
    static constexpr EventDocEntry entry = {
        "UnitRetreated", EventDocCategory::Lifecycle,
        R"({"type":"UnitRetreated","unit":"summon","reason":"retreat_test"})", "unit, reason",
        "Removes a unit. Must be in lifecycle-only step. "
        "Requires no active visual step or busy animation engine."};
};

// ── Visual ───────────────────────────────────────────────────────────────────

template <> struct event_types_doc_for<ScenarioActorSelected> {
    static constexpr EventDocEntry entry = {
        "ActorSelected", EventDocCategory::Visual,
        R"({"type":"ActorSelected","previous":"attacker1","selected":"attacker2"})",
        "previous, selected",
        "Highlights a unit as the active actor. previous/selected are string aliases."};
};

template <> struct event_types_doc_for<ScenarioTargetSelected> {
    static constexpr EventDocEntry entry = {
        "TargetSelected", EventDocCategory::Visual,
        R"({"type":"TargetSelected","previous":"defender1","selected":"defender2"})",
        "previous, selected", "Highlights a unit as the current target."};
};

template <> struct event_types_doc_for<ScenarioAttackStarted> {
    static constexpr EventDocEntry entry = {
        "AttackStarted", EventDocCategory::Visual,
        R"({"type":"AttackStarted","attack_id":1,"source":"attacker1","targets":["defender1"]})"
        "\n"
        R"({"type":"AttackStarted","attack_id":2,"source":"attacker1","targets":["defender1","defender2"]})",
        "attack_id (int), source, targets[]",
        "Starts an attack animation from source to targets. attack_id must be unique per step. "
        "Targets count should match attack type (single_target=1, aoe=all enemies)."};
};

template <> struct event_types_doc_for<ScenarioAttackImpactCue> {
    static constexpr EventDocEntry entry = {
        "AttackImpactCue", EventDocCategory::Visual,
        R"({"type":"AttackImpactCue","attack_id":1,"source":"attacker1","targets":["defender1"]})",
        "attack_id (int), source, targets[]",
        "Triggers impact visual effects for an attack. Must reference a matching AttackStarted."};
};

template <> struct event_types_doc_for<ScenarioTargetDamaged> {
    static constexpr EventDocEntry entry = {
        "TargetDamaged", EventDocCategory::Visual,
        R"({"type":"TargetDamaged","attack_id":1,"source":"attacker1","target":"defender1","current_hp":177})",
        "attack_id (int), source, target, current_hp? (int)",
        "Applies damage visual to a single target and may update runtime target HP."};
};

template <> struct event_types_doc_for<ScenarioTargetMissed> {
    static constexpr EventDocEntry entry = {
        "TargetMissed", EventDocCategory::Visual,
        R"({"type":"TargetMissed","attack_id":1,"target":"defender1"})", "attack_id (int), target",
        "Shows a miss visual on target."};
};

template <> struct event_types_doc_for<ScenarioTargetResisted> {
    static constexpr EventDocEntry entry = {
        "TargetResisted", EventDocCategory::Visual,
        R"({"type":"TargetResisted","attack_id":1,"target":"defender1"})",
        "attack_id (int), target", "Shows a resist visual on target."};
};

template <> struct event_types_doc_for<ScenarioTargetKilled> {
    static constexpr EventDocEntry entry = {
        "TargetKilled", EventDocCategory::Visual,
        R"({"type":"TargetKilled","attack_id":1,"target":"defender1"})", "attack_id (int), target",
        "Shows a kill visual on target (death from this specific attack) and sets runtime HP to "
        "0."};
};

template <> struct event_types_doc_for<ScenarioLifeDrained> {
    static constexpr EventDocEntry entry = {
        "LifeDrained", EventDocCategory::Visual,
        R"({"type":"LifeDrained","attack_id":1,"source":"attacker1","target":"defender1","target_current_hp":120,"source_current_hp":40})",
        "attack_id (int), source, target, target_current_hp? (int), source_current_hp? (int)",
        "Shows life drain effect flowing from target back to source and may update runtime HP."};
};

template <> struct event_types_doc_for<ScenarioSourceHealed> {
    static constexpr EventDocEntry entry = {
        "SourceHealed", EventDocCategory::Visual,
        R"({"type":"SourceHealed","attack_id":1,"source":"attacker1","current_hp":40})",
        "attack_id (int), source, current_hp? (int)",
        "Shows heal effect on the source unit and may update runtime source HP."};
};

template <> struct event_types_doc_for<ScenarioUnitHitReceived> {
    static constexpr EventDocEntry entry = {
        "UnitHitReceived", EventDocCategory::Visual,
        R"({"type":"UnitHitReceived","target":"defender1"})", "target",
        "Shows a hit reaction on target (independent of any specific attack)."};
};

template <> struct event_types_doc_for<ScenarioUnitKilled> {
    static constexpr EventDocEntry entry = {
        "UnitKilled", EventDocCategory::Visual, R"({"type":"UnitKilled","target":"defender1"})",
        "target",
        "Plays death animation for target unit and sets runtime "
        "HP to 0."};
};

template <> struct event_types_doc_for<ScenarioUnitReviveStarted> {
    static constexpr EventDocEntry entry = {"UnitReviveStarted", EventDocCategory::Visual,
                                            R"({"type":"UnitReviveStarted","target":"defender1"})",
                                            "target",
                                            "Begins a revive animation sequence for target."};
};

template <> struct event_types_doc_for<ScenarioUnitRevived> {
    static constexpr EventDocEntry entry = {
        "UnitRevived", EventDocCategory::Visual,
        R"({"type":"UnitRevived","target":"defender1","current_hp":40})",
        "target [, current_hp? (int)]",
        "Completes revive animation; current_hp may update runtime HP when supplied."};
};

template <> struct event_types_doc_for<ScenarioCastEffectStarted> {
    static constexpr EventDocEntry entry = {
        "CastEffectStarted", EventDocCategory::Visual,
        R"({"type":"CastEffectStarted","caster":"attacker1","effect":"source_attack_effect"})"
        "\n"
        R"({"type":"CastEffectStarted","caster":"attacker1","effect":"target_impact_effect"})",
        "caster, effect",
        "Plays a spell-casting effect on the caster unit. Effect describes visual intent: "
        "source_attack_effect (caster casts an attack), target_impact_effect (prep target "
        "impact)."};
};

template <> struct event_types_doc_for<ScenarioBattleEffectStarted> {
    static constexpr EventDocEntry entry = {
        "BattleEffectStarted", EventDocCategory::Visual,
        R"({"type":"BattleEffectStarted","source":"attacker1","effect":"source_attack_effect","visual_role":"source_cast_fx"})"
        "\n"
        R"({"type":"BattleEffectStarted","source":"attacker1","effect":"target_impact_effect","visual_role":"target_damage_fx","target":"defender1"})"
        "\n"
        R"({"type":"BattleEffectStarted","source":"attacker1","effect":"team_impact_effect","visual_role":"team_overlay_fx","targets":["defender1","defender2"]})"
        "\n"
        R"({"type":"BattleEffectStarted","source":"attacker1","effect":"source_attack_effect","visual_role":"field_overlay_fx","targets":["defender1"]})",
        "source, effect, visual_role [, target] [, targets[]]",
        "Plays a generic battle effect. Semantic effect names describe the visual intent: "
        "source_attack_effect | target_impact_effect | team_impact_effect | "
        "projectile_effect | post_effect_visual. "
        "visual_role determines placement: "
        "source_cast_fx | target_damage_fx | team_overlay_fx | field_overlay_fx."};
};

// ── Gather all docs ──────────────────────────────────────────────────────────

template <std::size_t... Is>
[[nodiscard]] inline std::vector<EventDocEntry>
collect_event_docs_impl(std::index_sequence<Is...>) {
    return {event_types_doc_for<std::variant_alternative_t<Is, BattleScenarioEvent>>::entry...};
}

[[nodiscard]] inline std::vector<EventDocEntry> collect_event_docs() {
    constexpr std::size_t N = std::variant_size_v<BattleScenarioEvent>;
    return collect_event_docs_impl(std::make_index_sequence<N>());
}

// ── Build comment text ───────────────────────────────────────────────────────

[[nodiscard]] inline std::string build_scenario_event_types_doc() {
    constexpr std::size_t N = std::variant_size_v<BattleScenarioEvent>;

    std::string out;
    out += "// Available event types (struct definitions in battle_scenario_data.hpp):\n";
    out += "//\n";

    auto docs = collect_event_docs();

    auto emit_entry = [&](const EventDocEntry& d) {
        out += "//     " + std::string(d.type_name) + ":\n";
        // Examples (newline-separated within json_example)
        std::string_view ex = d.json_example;
        while (!ex.empty()) {
            auto end = ex.find('\n');
            if (end == std::string_view::npos) {
                out += "//       " + std::string(ex) + "\n";
                break;
            }
            out += "//       " + std::string(ex.substr(0, end)) + "\n";
            ex.remove_prefix(end + 1);
        }
        if (!d.fields.empty())
            out += "//     fields: " + std::string(d.fields) + "\n";
        if (!d.notes.empty())
            out += "//     notes: " + std::string(d.notes) + "\n";
    };

    // Lifecycle section
    out += "//   Lifecycle (must be in lifecycle-only steps, never mixed with visual):\n";
    out += "//\n";
    for (const auto& d : docs) {
        if (d.category != EventDocCategory::Lifecycle)
            continue;
        emit_entry(d);
    }

    // Visual section
    out += "//\n";
    out += "//   Visual (must be in visual-only steps):\n";
    out += "//\n";
    for (const auto& d : docs) {
        if (d.category != EventDocCategory::Visual)
            continue;
        emit_entry(d);
    }

    out += "//\n";
    out += "//   Scheduling (at/cue on any event envelope):\n"
           "//     No at:    {\"id\":\"evt1\",\"event\":{...}}  \xe2\x80\x94 starts immediately\n"
           "//     Wait for cue:  "
           "{\"id\":\"evt2\",\"at\":{\"event_id\":\"evt1\",\"cue\":\"impact\"},\"event\":{...}}\n"
           "//     Cues: start|complete|impact\n";
    out += "//\n";
    out += "// Step constraints (enforced by BattleScenarioPlayer::update):\n"
           "//   - setup_units:       only UnitCreated allowed\n"
           "//   - lifecycle steps:   only UnitCreated / UnitRetreated, no visual events\n"
           "//   - visual steps:      any visual events, no lifecycle events\n"
           "//   - Lifecycle events cannot use at/cue scheduling (yet)\n"
           "//   - UnitRetreated requires no active visual step or busy animation engine\n";
    out += "//\n";

    // Compile-time assertion: all variant alternatives must have doc entries.
    // If N changes (new type added to variant), the static_assert below will fail
    // because event_types_doc_for won't be defined for the new type.
    // The collect_event_docs_impl call above ensures all types are instantiated.
    static_assert(N == 18, "BattleScenarioEvent variant size changed. "
                           "Add a new event_types_doc_for<T> specialization for the new type.");

    return out;
}

} // namespace d2engine
