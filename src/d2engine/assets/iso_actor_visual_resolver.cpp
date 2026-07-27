#include "iso_actor_visual_resolver.hpp"

#include <d2runtime/AdventureActorAnimationResolver.hpp>
#include <d2runtime/AdventureStackPresentationResolver.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace d2engine {
namespace {

constexpr std::string_view kIsoUnitContainer = "Imgs/Isounit.ff";

std::string lower(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string dir_suffix(d2runtime::AdventureIsoDirection d) {
    return std::to_string(d2runtime::direction_index(d));
}

} // namespace

std::string IsoActorVisualResolver::cache_key_unit(std::string_view normalized_unit_id,
                                                   d2runtime::AdventureIsoDirection       direction,
                                                   d2runtime::AdventureActorAnimationRole role) {
    std::string key = "U/";
    key += normalized_unit_id;
    key += '/';
    key += dir_suffix(direction);
    key += '/';
    key += std::to_string(static_cast<int>(role));
    return key;
}

std::string IsoActorVisualResolver::cache_key_boat(std::string_view                       race_id,
                                                   d2runtime::AdventureIsoDirection       direction,
                                                   d2runtime::AdventureActorAnimationRole role) {
    std::string key = "B/";
    key += race_id;
    key += '/';
    key += dir_suffix(direction);
    key += '/';
    key += std::to_string(static_cast<int>(role));
    return key;
}

ExactLayerResolution IsoActorVisualResolver::resolve_exact_layer(
    const d2runtime::AdventureActorPresentation& presentation, std::string_view owner_id,
    d2runtime::AdventureIsoDirection direction, d2runtime::AdventureActorAnimationLayer layer,
    d2runtime::AdventureActorAnimationRole role) const {
    d2runtime::AdventureActorAnimationResolver anim_resolver;
    const auto identity = anim_resolver.resolve(presentation, role, layer, std::string(owner_id),
                                                std::string(owner_id), direction);
    if (!identity.has_value())
        return {.presence = ExactLayerPresence::Missing, .visual = std::nullopt};

    const auto& anim_name = identity->logical_animation_name;

    if (!animations_.contains(anim_name))
        return {.presence = ExactLayerPresence::Missing, .visual = std::nullopt};

    const auto seq = catalog_->animation_sequence(kIsoUnitContainer, anim_name);
    const bool is_shadow = layer == d2runtime::AdventureActorAnimationLayer::Shadow;

    if (seq.frames.empty()) {
        std::string msg = is_shadow ? "malformed_shadow_sequence" : "malformed_body_sequence";
        msg += " identity=";
        msg += anim_name;
        msg += " reason=empty_sequence";
        throw std::runtime_error(msg);
    }

    std::size_t       visible_count = 0;
    std::size_t       empty_count = 0;
    std::vector<bool> frame_has_visible;

    for (std::size_t fi = 0; fi < seq.frames.size(); ++fi) {
        const auto& frame = seq.frames[fi];
        if (frame.image_name.empty()) {
            std::string msg = (is_shadow ? "malformed_shadow_sequence" : "malformed_body_sequence");
            msg += " identity=";
            msg += anim_name;
            msg += " reason=empty_frame_record";
            msg += " frame=" + std::to_string(fi);
            throw std::runtime_error(msg);
        }
        const auto sm = catalog_->sprite_metadata(kIsoUnitContainer, frame.image_name);

        if (sm.canvas_width <= 0 || sm.canvas_height <= 0) {
            std::string msg = (is_shadow ? "malformed_shadow_sequence" : "malformed_body_sequence");
            msg += " identity=";
            msg += anim_name;
            msg += " reason=zero_frame_dimensions";
            msg += " frame=" + std::to_string(fi);
            msg +=
                " dims=" + std::to_string(sm.canvas_width) + "x" + std::to_string(sm.canvas_height);
            msg += " record=" + frame.image_name;
            throw std::runtime_error(msg);
        }

        if (sm.has_visible_pieces) {
            if (!sm.content_bounds.valid()) {
                std::string msg =
                    is_shadow ? "malformed_shadow_sequence" : "malformed_body_sequence";
                msg += " identity=";
                msg += anim_name;
                msg += " reason=invalid_content_bounds";
                msg += " frame=" + std::to_string(fi);
                msg += " record=" + frame.image_name;
                throw std::runtime_error(msg);
            }
            ++visible_count;
            frame_has_visible.push_back(true);
        } else {
            ++empty_count;
            frame_has_visible.push_back(false);
        }
    }

    if (is_shadow) {
        if (empty_count == seq.frames.size()) {
            return {.presence = ExactLayerPresence::AuthoredEmpty, .visual = std::nullopt};
        }

        if (visible_count > 0 && empty_count > 0) {
            std::string msg = "mixed_visibility_shadow identity=";
            msg += anim_name;
            msg += " owner=" + std::string(owner_id);
            msg += " visible_frames=" + std::to_string(visible_count);
            msg += " empty_frames=" + std::to_string(empty_count);
            for (std::size_t fi = 0; fi < frame_has_visible.size(); ++fi) {
                if (!frame_has_visible[fi]) {
                    msg += " empty_frame_index=" + std::to_string(fi);
                    msg += " empty_frame_record=" + seq.frames[fi].image_name;
                }
            }
            throw std::runtime_error(msg);
        }
    } else {
        if (empty_count > 0) {
            std::string msg = "malformed_body_sequence identity=";
            msg += anim_name;
            msg += " owner=" + std::string(owner_id);
            msg += " reason=empty_body_frame";
            for (std::size_t fi = 0; fi < frame_has_visible.size(); ++fi) {
                if (!frame_has_visible[fi]) {
                    msg += " empty_frame_index=" + std::to_string(fi);
                    msg += " empty_frame_record=" + seq.frames[fi].image_name;
                    break;
                }
            }
            throw std::runtime_error(msg);
        }
    }

    if (seq.native_canvas_w <= 0 || seq.native_canvas_h <= 0) {
        std::string msg = is_shadow ? "malformed_shadow_sequence" : "malformed_body_sequence";
        msg += " identity=";
        msg += anim_name;
        msg += " reason=invalid_native_canvas ";
        msg += std::to_string(seq.native_canvas_w) + "x" + std::to_string(seq.native_canvas_h);
        throw std::runtime_error(msg);
    }

    IsoActorVisualLayer visual;
    visual.container_path = std::string(kIsoUnitContainer);
    visual.animation_name = anim_name;
    visual.native_canvas_w = seq.native_canvas_w;
    visual.native_canvas_h = seq.native_canvas_h;
    visual.canvas_foot_x = seq.canvas_foot_x;
    visual.canvas_foot_y = seq.canvas_foot_y;
    visual.content_bounds = {};
    visual.frames.reserve(seq.frames.size());
    for (const auto& frame : seq.frames) {
        const auto          sm = catalog_->sprite_metadata(kIsoUnitContainer, frame.image_name);
        IsoActorVisualFrame vf;
        vf.record_name = frame.image_name;
        vf.canvas_width = sm.canvas_width;
        vf.canvas_height = sm.canvas_height;
        if (!sm.has_visible_pieces) {
            std::string msg = is_shadow ? "malformed_shadow_sequence" : "malformed_body_sequence";
            msg += " identity=" + std::string(anim_name);
            msg += " reason=empty_body_frame";
            msg += " frame=" + std::to_string(visual.frames.size());
            msg += " record=" + frame.image_name;
            if (!is_shadow) {
                throw std::runtime_error(msg);
            }
            vf.content_bounds = {};
        } else {
            if (!sm.content_bounds.valid()) {
                std::string msg =
                    is_shadow ? "malformed_shadow_sequence" : "malformed_body_sequence";
                msg += " identity=" + std::string(anim_name);
                msg += " reason=invalid_content_bounds";
                msg += " frame=" + std::to_string(visual.frames.size());
                msg += " record=" + frame.image_name;
                throw std::runtime_error(msg);
            }
            vf.content_bounds = sm.content_bounds;
        }
        visual.content_bounds =
            adventure_render::union_canvas_content_bounds(visual.content_bounds, vf.content_bounds);
        visual.frames.push_back(std::move(vf));
    }
    if (visible_count > 0 && !visual.content_bounds.valid()) {
        std::string msg = is_shadow ? "malformed_shadow_sequence" : "malformed_body_sequence";
        msg += " identity=" + std::string(anim_name);
        msg += " reason=invalid_content_bounds";
        throw std::runtime_error(msg);
    }
    return {
        .presence = ExactLayerPresence::Visible,
        .visual = std::move(visual),
    };
}

IsoActorVisualResolver::IsoActorVisualResolver(const ISpriteAnimationCatalog& catalog,
                                               const GameDataRegistry&        game_data)
    : catalog_(&catalog), game_data_(&game_data) {
    for (auto name : catalog_->animations_in(kIsoUnitContainer)) {
        animations_.insert(std::move(name));
    }
}

std::optional<IsoActorVisual>
IsoActorVisualResolver::resolve(const AdventureStackActorVisualRequest& request) {
    const std::string dir_str = dir_suffix(request.direction);
    const auto        unit_pres =
        d2runtime::AdventureActorPresentation{d2runtime::AdventureActorPresentationKind::Unit};
    const auto boat_pres =
        d2runtime::AdventureActorPresentation{d2runtime::AdventureActorPresentationKind::Boat};

    if (request.presentation.kind == d2runtime::AdventureActorPresentationKind::Boat) {
        // ── Boat resolution ──────────────────────────────────────────────
        const std::string key = cache_key_boat(request.race_id, request.direction, request.role);
        auto              cache_it = cache_.find(key);
        if (cache_it != cache_.end())
            return cache_it->second;

        // Boat body: exact race BOATn, no fallback
        auto body =
            resolve_exact_layer(boat_pres, request.race_id, request.direction,
                                d2runtime::AdventureActorAnimationLayer::Main, request.role);

        if (body.presence == ExactLayerPresence::Missing) {
            d2runtime::AdventureActorAnimationResolver anim_resolver;
            const auto                                 expected_identity = anim_resolver.resolve(
                boat_pres, request.role, d2runtime::AdventureActorAnimationLayer::Main,
                request.race_id, request.race_id, request.direction);
            std::string expected_name = expected_identity.has_value()
                                            ? expected_identity->logical_animation_name
                                            : "unknown";
            std::string msg = "missing_boat_body presentation=Boat";
            msg += " race=" + request.race_id;
            msg += " direction=" + dir_str;
            msg += " expected=" + expected_name;
            msg += " reason=missing_boat_body";
            throw std::runtime_error(msg);
        }

        IsoActorVisual visual;
        visual.presentation_kind = d2runtime::AdventureActorPresentationKind::Boat;
        visual.resolved_owner_id = request.race_id;
        visual.body = std::move(*body.visual);

        // Boat shadow: exact same race SBOAn, optional
        auto shadow =
            resolve_exact_layer(boat_pres, request.race_id, request.direction,
                                d2runtime::AdventureActorAnimationLayer::Shadow, request.role);

        visual.shadow_presence = shadow.presence;
        if (shadow.visual.has_value())
            visual.shadow = std::move(shadow.visual);

        cache_[key] = visual;
        return visual;
    }

    // ── Unit resolution ────────────────────────────────────────────────
    const std::string normalized = lower(request.leader_unit_type_id);
    const std::string key = cache_key_unit(normalized, request.direction, request.role);
    auto              cache_it = cache_.find(key);
    if (cache_it != cache_.end())
        return cache_it->second;

    std::string actual_owner_id;
    auto body = resolve_exact_layer(unit_pres, normalized, request.direction,
                                    d2runtime::AdventureActorAnimationLayer::Main, request.role);

    if (body.presence == ExactLayerPresence::Missing) {
        const UnitDef* def = game_data_->find_unit(normalized);
        if (def != nullptr && !def->base_unit_id.empty() && def->base_unit_id != normalized) {
            body = resolve_exact_layer(unit_pres, def->base_unit_id, request.direction,
                                       d2runtime::AdventureActorAnimationLayer::Main, request.role);
            if (body.presence != ExactLayerPresence::Missing)
                actual_owner_id = def->base_unit_id;
        }
    } else {
        actual_owner_id = normalized;
    }

    IsoActorVisual visual;
    visual.presentation_kind = d2runtime::AdventureActorPresentationKind::Unit;
    visual.resolved_owner_id = actual_owner_id;

    if (body.presence == ExactLayerPresence::Missing || !body.visual.has_value()) {
        cache_[key] = std::nullopt;
        return std::nullopt;
    }

    visual.body = std::move(*body.visual);

    if (!actual_owner_id.empty()) {
        auto shadow =
            resolve_exact_layer(unit_pres, actual_owner_id, request.direction,
                                d2runtime::AdventureActorAnimationLayer::Shadow, request.role);
        visual.shadow_presence = shadow.presence;
        if (shadow.visual.has_value())
            visual.shadow = std::move(*shadow.visual);
    }

    cache_[key] = visual;
    return visual;
}

ExactLayerPresence
IsoActorVisualResolver::shadow_presence(std::string_view                 resolved_owner_id,
                                        d2runtime::AdventureIsoDirection direction) const {
    auto shadow_res =
        resolve_exact_layer({d2runtime::AdventureActorPresentationKind::Unit}, resolved_owner_id,
                            direction, d2runtime::AdventureActorAnimationLayer::Shadow,
                            d2runtime::AdventureActorAnimationRole::Idle);
    return shadow_res.presence;
}

} // namespace d2engine
