#include "battle_scenario.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace d2engine {
namespace {

using json = nlohmann::json;

void reject_raw_tokens(const std::string& text, const std::string& path) {
    const std::string lowered = [&]() {
        std::string out;
        out.reserve(text.size());
        std::ranges::transform(text, std::back_inserter(out),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }();
    for (const auto& token : {"heff", "tuch"}) {
        // Token-aware scan: match standalone identifiers, not substrings of longer words.
        // "sheff" or "detuch" should not trigger rejection.
        std::size_t       pos = 0;
        const std::size_t token_len = std::strlen(token);
        while ((pos = lowered.find(token, pos)) != std::string::npos) {
            const bool prev_not_word =
                (pos == 0u || (std::isalnum(static_cast<unsigned char>(lowered[pos - 1u])) == 0));
            const bool next_not_word =
                (pos + token_len >= lowered.size() ||
                 (std::isalnum(static_cast<unsigned char>(lowered[pos + token_len])) == 0));
            if (prev_not_word && next_not_word) {
                throw std::runtime_error(
                    std::string("scenario files must use semantic visual effect names; "
                                "raw asset family '") +
                    token + "' belongs in visual resolver/config, not events.json. File=" + path);
            }
            pos += token_len;
        }
    }
}

void require_object(const json& value, const std::string& path) {
    if (!value.is_object()) {
        throw std::runtime_error(path + " must be an object");
    }
}

void reject_unknown(const json& value, const std::unordered_set<std::string>& fields,
                    const std::string& path) {
    require_object(value, path);
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (!fields.contains(it.key())) {
            throw std::runtime_error(path + " has unknown field '" + it.key() + "'");
        }
    }
}

std::string required_string(const json& value, const char* field, const std::string& path) {
    if (!value.contains(field) || !value[field].is_string() ||
        value[field].get<std::string>().empty()) {
        throw std::runtime_error(path + "." + field + " must be a non-empty string");
    }
    return value[field].get<std::string>();
}

AttackInstanceId required_attack_id(const json& value, const std::string& path) {
    if (!value.contains("attack_id") || !value["attack_id"].is_number_unsigned()) {
        throw std::runtime_error(path + ".attack_id must be positive");
    }
    const auto id = value["attack_id"].get<std::uint32_t>();
    if (id == 0u) {
        throw std::runtime_error(path + ".attack_id must be positive");
    }
    return AttackInstanceId{id};
}

std::optional<int> optional_int(const json& value, const char* field, const std::string& path) {
    if (!value.contains(field)) {
        return std::nullopt;
    }
    if (!value[field].is_number_integer()) {
        throw std::runtime_error(path + "." + field + " must be an integer");
    }
    return value[field].get<int>();
}

std::vector<std::string> required_targets(const json& value, const std::string& path) {
    if (!value.contains("targets") || !value["targets"].is_array() || value["targets"].empty()) {
        throw std::runtime_error(path + ".targets must be a non-empty array");
    }
    std::vector<std::string> targets;
    for (const auto& i : value["targets"]) {
        if (!i.is_string() || i.get<std::string>().empty()) {
            throw std::runtime_error(path + ".targets contains an empty unit ref");
        }
        targets.push_back(i.get<std::string>());
    }
    return targets;
}

BattleEffectRole role_from_string(const std::string& role, const std::string& path) {
    // Semantic effect names (preferred for scenario authoring)
    if (role == "source_attack_effect" || role == "team_impact_effect" ||
        role == "post_effect_visual")
        return BattleEffectRole::Heff;
    if (role == "target_impact_effect" || role == "projectile_effect")
        return BattleEffectRole::Tuch;
    // Legacy raw asset-family names — rejected with a clear migration message
    if (role == "Heff" || role == "Tuch") {
        throw std::runtime_error(path + " uses legacy effect role '" + role +
                                 "'. Use semantic effect names: "
                                 "source_attack_effect, target_impact_effect, "
                                 "team_impact_effect, projectile_effect, "
                                 "or post_effect_visual.");
    }
    throw std::runtime_error(path + " has unknown effect role '" + role +
                             "'. Supported semantic names: "
                             "source_attack_effect, target_impact_effect, "
                             "team_impact_effect, projectile_effect, "
                             "post_effect_visual.");
}

BattleEffectVisualRole visual_role_from_string(const std::string& s, const std::string& path) {
    if (s == "source_cast_fx" || s == "source_unit")
        return BattleEffectVisualRole::SourceCastFx;
    if (s == "target_damage_fx")
        return BattleEffectVisualRole::TargetDamageFx;
    if (s == "team_overlay_fx" || s == "opponent_team_overlay")
        return BattleEffectVisualRole::TeamOverlayFx;
    if (s == "field_overlay_fx")
        return BattleEffectVisualRole::FieldOverlayFx;
    throw std::runtime_error(path + " has unknown effect visual_role '" + s + "'");
}

BattleVisualStepCompletion completion_from_string(const std::string& complete,
                                                  const std::string& path) {
    if (complete == "immediate") {
        return BattleVisualStepCompletion::Immediate;
    }
    if (complete == "all_required_tracks_finished") {
        return BattleVisualStepCompletion::AllRequiredTracksFinished;
    }
    throw std::runtime_error(path + " has unknown completion policy '" + complete + "'");
}

BattleScenarioEventSchedule parse_schedule(const json& value, const std::string& envelope_id,
                                           const std::string& path) {
    if (!value.contains("at")) {
        return {};
    }
    const json& at = value["at"];
    if (at.is_string()) {
        const std::string cue = at.get<std::string>();
        if (cue == "start") {
            return {};
        }
        throw std::runtime_error(path + " envelope '" + envelope_id + "' has unknown schedule '" +
                                 cue + "'");
    }
    if (at.is_object()) {
        reject_unknown(at, {"event_id", "cue"}, path + ".at");
        return {.event_id = required_string(at, "event_id", path + ".at"),
                .cue = required_string(at, "cue", path + ".at")};
    }
    throw std::runtime_error(path + " envelope '" + envelope_id + "' has unknown schedule shape");
}

BattleScenarioEvent parse_event(const json& value, const std::string& path) {
    const std::string type = required_string(value, "type", path);
    if (type == "ActorSelected") {
        reject_unknown(value, {"type", "previous", "selected"}, path);
        return ScenarioActorSelected{.previous = required_string(value, "previous", path),
                                     .selected = required_string(value, "selected", path)};
    }
    if (type == "TargetSelected") {
        reject_unknown(value, {"type", "previous", "selected"}, path);
        return ScenarioTargetSelected{.previous = required_string(value, "previous", path),
                                      .selected = required_string(value, "selected", path)};
    }
    if (type == "AttackStarted") {
        reject_unknown(value, {"type", "attack_id", "source", "targets"}, path);
        return ScenarioAttackStarted{.attack_id = required_attack_id(value, path),
                                     .source = required_string(value, "source", path),
                                     .targets = required_targets(value, path)};
    }
    if (type == "AttackImpactCue") {
        reject_unknown(value, {"type", "attack_id", "source", "targets"}, path);
        return ScenarioAttackImpactCue{.attack_id = required_attack_id(value, path),
                                       .source = required_string(value, "source", path),
                                       .targets = required_targets(value, path)};
    }
    if (type == "TargetDamaged") {
        reject_unknown(value, {"type", "attack_id", "source", "target", "current_hp"}, path);
        return ScenarioTargetDamaged{.attack_id = required_attack_id(value, path),
                                     .source = required_string(value, "source", path),
                                     .target = required_string(value, "target", path),
                                     .current_hp = optional_int(value, "current_hp", path)};
    }
    if (type == "TargetMissed") {
        reject_unknown(value, {"type", "attack_id", "target"}, path);
        return ScenarioTargetMissed{.attack_id = required_attack_id(value, path),
                                    .target = required_string(value, "target", path)};
    }
    if (type == "TargetResisted") {
        reject_unknown(value, {"type", "attack_id", "target"}, path);
        return ScenarioTargetResisted{.attack_id = required_attack_id(value, path),
                                      .target = required_string(value, "target", path)};
    }
    if (type == "TargetKilled") {
        reject_unknown(value, {"type", "attack_id", "target"}, path);
        return ScenarioTargetKilled{.attack_id = required_attack_id(value, path),
                                    .target = required_string(value, "target", path)};
    }
    if (type == "LifeDrained") {
        reject_unknown(
            value,
            {"type", "attack_id", "source", "target", "target_current_hp", "source_current_hp"},
            path);
        return ScenarioLifeDrained{
            .attack_id = required_attack_id(value, path),
            .source = required_string(value, "source", path),
            .target = required_string(value, "target", path),
            .target_current_hp = optional_int(value, "target_current_hp", path),
            .source_current_hp = optional_int(value, "source_current_hp", path)};
    }
    if (type == "SourceHealed") {
        reject_unknown(value, {"type", "attack_id", "source", "current_hp"}, path);
        return ScenarioSourceHealed{.attack_id = required_attack_id(value, path),
                                    .source = required_string(value, "source", path),
                                    .current_hp = optional_int(value, "current_hp", path)};
    }
    if (type == "UnitHitReceived") {
        reject_unknown(value, {"type", "target"}, path);
        return ScenarioUnitHitReceived{.target = required_string(value, "target", path)};
    }
    if (type == "UnitKilled") {
        reject_unknown(value, {"type", "target"}, path);
        return ScenarioUnitKilled{.target = required_string(value, "target", path)};
    }
    if (type == "UnitReviveStarted") {
        reject_unknown(value, {"type", "target"}, path);
        return ScenarioUnitReviveStarted{.target = required_string(value, "target", path)};
    }
    if (type == "UnitRevived") {
        reject_unknown(value, {"type", "target", "current_hp"}, path);
        return ScenarioUnitRevived{.target = required_string(value, "target", path),
                                   .current_hp = optional_int(value, "current_hp", path)};
    }
    if (type == "CastEffectStarted") {
        reject_unknown(value, {"type", "caster", "effect"}, path);
        return ScenarioCastEffectStarted{
            .caster = required_string(value, "caster", path),
            .effect = role_from_string(required_string(value, "effect", path), path + ".effect")};
    }
    if (type == "BattleEffectStarted") {
        reject_unknown(value, {"type", "source", "effect", "visual_role", "target", "targets"},
                       path);
        const std::string vr_str = required_string(value, "visual_role", path);
        const auto        visual_role = visual_role_from_string(vr_str, path + ".visual_role");
        const bool        needs_target = (visual_role == BattleEffectVisualRole::TargetDamageFx);
        const bool        needs_targets = (visual_role == BattleEffectVisualRole::TeamOverlayFx ||
                                           visual_role == BattleEffectVisualRole::FieldOverlayFx);
        std::string       target;
        if (needs_target && value.contains("target")) {
            target = required_string(value, "target", path);
        }
        std::vector<std::string> targets;
        if (needs_targets) {
            if (!value.contains("targets") || !value["targets"].is_array() ||
                value["targets"].empty()) {
                throw std::runtime_error(path + ".targets must be a non-empty array");
            }
            targets = required_targets(value, path);
        }
        return ScenarioBattleEffectStarted{
            .source = required_string(value, "source", path),
            .effect = role_from_string(required_string(value, "effect", path), path + ".effect"),
            .visual_role = visual_role,
            .target = std::move(target),
            .targets = std::move(targets)};
    }
    if (type == "UnitCreated") {
        reject_unknown(value, {"type", "unit", "unit_type", "slot", "hp", "max_hp", "status"},
                       path);
        std::optional<int> hp;
        if (value.contains("hp")) {
            if (!value["hp"].is_number_integer()) {
                throw std::runtime_error(path + ".hp must be an integer");
            }
            hp = value["hp"].get<int>();
        }
        std::optional<int> max_hp;
        if (value.contains("max_hp")) {
            if (!value["max_hp"].is_number_integer()) {
                throw std::runtime_error(path + ".max_hp must be an integer");
            }
            max_hp = value["max_hp"].get<int>();
        }
        std::vector<std::string> status;
        if (value.contains("status")) {
            if (!value["status"].is_array()) {
                throw std::runtime_error(path + ".status must be an array");
            }
            for (std::size_t i = 0; i < value["status"].size(); ++i) {
                if (!value["status"][i].is_string()) {
                    throw std::runtime_error(path + ".status[" + std::to_string(i) +
                                             "] must be a string");
                }
                status.push_back(value["status"][i].get<std::string>());
            }
        }
        return ScenarioUnitCreated{.alias = required_string(value, "unit", path),
                                   .unit_type = required_string(value, "unit_type", path),
                                   .slot = required_string(value, "slot", path),
                                   .hp = hp,
                                   .max_hp = max_hp,
                                   .status = std::move(status)};
    }
    if (type == "UnitRetreated") {
        reject_unknown(value, {"type", "unit", "reason"}, path);
        return ScenarioUnitRetreated{.unit = required_string(value, "unit", path),
                                     .reason = required_string(value, "reason", path)};
    }
    throw std::runtime_error(path + " has unknown event type '" + type + "'");
}

BattleScenarioEventEnvelope parse_envelope(const json& value, const std::string& path) {
    reject_unknown(value, {"id", "optional", "at", "event"}, path);
    BattleScenarioEventEnvelope envelope;
    envelope.id = required_string(value, "id", path);
    if (value.contains("optional")) {
        if (!value["optional"].is_boolean()) {
            throw std::runtime_error(path + ".optional must be a boolean");
        }
        envelope.optional = value["optional"].get<bool>();
    }
    envelope.at = parse_schedule(value, envelope.id, path);
    if (!value.contains("event") || !value["event"].is_object()) {
        throw std::runtime_error(path + ".event must be an object");
    }
    envelope.event = parse_event(value["event"], path + ".event");
    return envelope;
}

} // namespace

std::string strip_scenario_comment_header(const std::string& raw) {
    std::istringstream stream(raw);
    std::ostringstream result;
    std::string        line;
    bool               past_comments = false;
    while (std::getline(stream, line)) {
        if (!past_comments) {
            const auto first_non_space = line.find_first_not_of(" \t");
            if (first_non_space != std::string::npos && line.substr(first_non_space, 2) == "//") {
                continue;
            }
            if (first_non_space == std::string::npos) {
                continue;
            }
            past_comments = true;
        }
        result << line << '\n';
    }
    return result.str();
}

BattleScenario load_battle_scenario(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("could not open battle scenario '" + path.string() + "'");
    }

    const std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // Reject raw asset-family tokens (heff/tuch) before any further processing.
    // This catches tokens in event bodies, comments, and header examples alike.
    reject_raw_tokens(raw, path.string());
    const std::string stripped = strip_scenario_comment_header(raw);

    const json document = json::parse(stripped);
    reject_unknown(document, {"version", "id", "sequences", "autoplay", "terrain"}, "$");

    if (!document.contains("version") || !document["version"].is_number_integer() ||
        document["version"].get<int>() != 3) {
        throw std::runtime_error(
            "unsupported battle scenario version; only version 3 is supported");
    }

    const std::string id = required_string(document, "id", "$");

    if (!document.contains("sequences") || !document["sequences"].is_array() ||
        document["sequences"].empty()) {
        throw std::runtime_error("$.sequences must be a non-empty array");
    }

    BattleScenario scenario;
    scenario.version = 3;
    scenario.id = id;
    scenario.terrain =
        document.contains("terrain") ? required_string(document, "terrain", "$") : "HU";
    if (document.contains("autoplay")) {
        scenario.autoplay = required_string(document, "autoplay", "$");
    }

    std::unordered_set<std::string> sequence_ids;
    const auto&                     sequences = document["sequences"];
    for (std::size_t i = 0; i < sequences.size(); ++i) {
        const std::string seq_path = "$.sequences[" + std::to_string(i) + "]";
        const json&       seq = sequences[i];
        reject_unknown(seq, {"id", "steps"}, seq_path);

        BattleScenarioSequence parsed_seq;
        parsed_seq.id = required_string(seq, "id", seq_path);
        if (!sequence_ids.insert(parsed_seq.id).second) {
            throw std::runtime_error("duplicate sequence id '" + parsed_seq.id + "'");
        }

        if (!seq.contains("steps") || !seq["steps"].is_array() || seq["steps"].empty()) {
            throw std::runtime_error(seq_path + ".steps must be a non-empty array");
        }

        for (std::size_t si = 0; si < seq["steps"].size(); ++si) {
            const std::string step_path = seq_path + ".steps[" + std::to_string(si) + "]";
            const json&       step = seq["steps"][si];
            reject_unknown(step, {"id", "complete", "events"}, step_path);

            BattleScenarioStep parsed_step;
            parsed_step.id = required_string(step, "id", step_path);
            parsed_step.complete = completion_from_string(
                required_string(step, "complete", step_path), step_path + ".complete");

            if (!step.contains("events") || !step["events"].is_array() || step["events"].empty()) {
                throw std::runtime_error(step_path + ".events must be a non-empty array");
            }

            std::unordered_set<std::string> envelope_ids;
            for (std::size_t ei = 0; ei < step["events"].size(); ++ei) {
                auto envelope = parse_envelope(step["events"][ei],
                                               step_path + ".events[" + std::to_string(ei) + "]");
                if (!envelope_ids.insert(envelope.id).second) {
                    throw std::runtime_error("duplicate envelope id '" + envelope.id + "'");
                }
                parsed_step.envelopes.push_back(std::move(envelope));
            }

            for (const auto& envelope : parsed_step.envelopes) {
                if (envelope.at.event_id.has_value() &&
                    !envelope_ids.contains(*envelope.at.event_id)) {
                    throw std::runtime_error("cue schedule references missing event_id '" +
                                             *envelope.at.event_id + "'");
                }
            }

            parsed_seq.steps.push_back(std::move(parsed_step));
        }
        scenario.sequences.push_back(std::move(parsed_seq));
    }
    return scenario;
}

} // namespace d2engine
