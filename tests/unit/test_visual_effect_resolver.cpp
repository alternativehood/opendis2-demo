#include <gtest/gtest.h>

#include "d2engine/battle_view/battle_effect_clip.hpp"
#include "d2engine/battle_view/unit_state_clip.hpp"
#include "d2engine/battle_view/visual_effect_resolver.hpp"

#include <variant>

namespace d2engine {
namespace {

AnimationSequence make_seq(std::string name) {
    AnimationSequence s;
    s.name = std::move(name);
    s.frames.push_back({.image_name = "f", .index = 0, .duration_ms = 100});
    return s;
}

struct ResolverFixture {
    BattleScene               scene;
    UnitVisualProfileRegistry profiles;

    // unit 1: has heff + tuch
    // unit 2: no heff, no tuch
    ResolverFixture() {
        auto add_unit = [&](std::uint32_t id) {
            BattleUnit u;
            u.unit_instance_id = UnitInstanceId{id};
            u.visual_profile_id = UnitVisualProfileId{id};
            VisualTrack base;
            base.kind = TrackKind::Base;
            base.player.load(make_seq("idle"));
            base.player.play();
            u.tracks.push_back(std::move(base));
            scene.add_unit(std::move(u));
        };
        add_unit(1);
        add_unit(2);

        // profile 1: has heff + tuch
        UnitAnimationRoleSet r1;
        r1.idle = UnitStateClip::from_a1(make_seq("idle_1"));
        r1.attack = UnitStateClip::from_a1(make_seq("attack_1"));
        r1.heff = BattleEffectClip{.clip = {.a1 = make_seq("heff_1")}};
        r1.tuch = BattleEffectClip{.clip = {.a1 = make_seq("tuch_1")}};
        static_cast<void>(profiles.add(std::move(r1)));

        // profile 2: no heff, no tuch
        UnitAnimationRoleSet r2;
        r2.idle = UnitStateClip::from_a1(make_seq("idle_2"));
        r2.attack = UnitStateClip::from_a1(make_seq("attack_2"));
        static_cast<void>(profiles.add(std::move(r2)));
    }

    [[nodiscard]] VisualEffectResolver resolver() const {
        return VisualEffectResolver{scene, &profiles};
    }
};

// AttackStarted with TUCH → [AttackStarted, BattleEffectStarted{Tuch, SourceCastFx}]
TEST(VisualEffectResolver, AttackStartedWithTuchInjectsSourceCastFx) {
    ResolverFixture const fx;
    auto                  res = fx.resolver();

    AttackStarted event{.attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{1u}};
    auto          expanded = res.expand(event);

    ASSERT_EQ(expanded.size(), 2u);
    ASSERT_TRUE(std::holds_alternative<AttackStarted>(expanded[0]));
    ASSERT_TRUE(std::holds_alternative<BattleEffectStarted>(expanded[1]));

    const auto& bes = std::get<BattleEffectStarted>(expanded[1]);
    EXPECT_EQ(bes.source, UnitInstanceId{1u});
    EXPECT_EQ(bes.role, BattleEffectRole::Tuch);
    EXPECT_EQ(bes.visual_role, BattleEffectVisualRole::SourceCastFx);
}

// AttackStarted without TUCH → [AttackStarted] only
TEST(VisualEffectResolver, AttackStartedWithoutTuchPassesThrough) {
    ResolverFixture const fx;
    auto                  res = fx.resolver();

    AttackStarted event{.attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{2u}};
    auto          expanded = res.expand(event);

    ASSERT_EQ(expanded.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<AttackStarted>(expanded[0]));
}

// AttackStarted with TUCH does NOT inject HEFF as source-side effect
TEST(VisualEffectResolver, AttackStartedDoesNotInjectHeffAsSourceSideFx) {
    ResolverFixture const fx;
    auto                  res = fx.resolver();

    AttackStarted event{.attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{1u}};
    auto          expanded = res.expand(event);

    // If a BattleEffectStarted is injected, it must be TUCH not HEFF
    for (const auto& ev : expanded) {
        if (const auto* bes = std::get_if<BattleEffectStarted>(&ev)) {
            EXPECT_NE(bes->role, BattleEffectRole::Heff)
                << "HEFF must not be injected from AttackStarted";
        }
    }
}

// AttackImpactCue always passes through — never injects TUCH or HEFF
TEST(VisualEffectResolver, AttackImpactCuePassesThrough) {
    ResolverFixture const fx;
    auto                  res = fx.resolver();

    // Unit 1 has both TUCH and HEFF — neither should be injected from impact cue
    TargetSet const targets = TargetSet::unit_list({UnitInstanceId{3u}, UnitInstanceId{4u}});
    AttackImpactCue event{
        .attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{1u}, .targets = targets};
    auto expanded = res.expand(event);

    ASSERT_EQ(expanded.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<AttackImpactCue>(expanded[0]));
}

// TargetDamaged with source HEFF → [TargetDamaged, BattleEffectStarted{Heff, TargetDamageFx}]
TEST(VisualEffectResolver, TargetDamagedWithHeffInjectsTargetDamageFx) {
    ResolverFixture const fx;
    auto                  res = fx.resolver();

    TargetDamaged event{.attack_id = AttackInstanceId{1u},
                        .source = UnitInstanceId{1u},
                        .target = UnitInstanceId{2u}};
    auto          expanded = res.expand(event);

    ASSERT_EQ(expanded.size(), 2u);
    ASSERT_TRUE(std::holds_alternative<TargetDamaged>(expanded[0]));
    ASSERT_TRUE(std::holds_alternative<BattleEffectStarted>(expanded[1]));

    const auto& bes = std::get<BattleEffectStarted>(expanded[1]);
    EXPECT_EQ(bes.source, UnitInstanceId{1u});
    EXPECT_EQ(bes.role, BattleEffectRole::Heff);
    EXPECT_EQ(bes.visual_role, BattleEffectVisualRole::TargetDamageFx);
    EXPECT_EQ(bes.target, UnitInstanceId{2u});
}

// TargetDamaged when source has no HEFF → passes through only
TEST(VisualEffectResolver, TargetDamagedWithoutHeffPassesThrough) {
    ResolverFixture const fx;
    auto                  res = fx.resolver();

    TargetDamaged event{.attack_id = AttackInstanceId{1u},
                        .source = UnitInstanceId{2u}, // unit 2 has no heff
                        .target = UnitInstanceId{1u}};
    auto          expanded = res.expand(event);

    ASSERT_EQ(expanded.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<TargetDamaged>(expanded[0]));
}

// TargetDamaged does NOT inject TUCH as target damage FX
TEST(VisualEffectResolver, TargetDamagedDoesNotInjectTuchAsTargetFx) {
    ResolverFixture const fx;
    auto                  res = fx.resolver();

    TargetDamaged event{.attack_id = AttackInstanceId{1u},
                        .source = UnitInstanceId{1u},
                        .target = UnitInstanceId{2u}};
    auto          expanded = res.expand(event);

    for (const auto& ev : expanded) {
        if (const auto* bes = std::get_if<BattleEffectStarted>(&ev)) {
            EXPECT_NE(bes->role, BattleEffectRole::Tuch)
                << "TUCH must not be injected from TargetDamaged";
        }
    }
}

// Multiple TargetDamaged events → distinct target fields per injection
TEST(VisualEffectResolver, MultipleTargetDamagedProducePerTargetInjections) {
    ResolverFixture const fx;
    auto                  res = fx.resolver();

    const std::vector<UnitInstanceId> targets = {UnitInstanceId{10u}, UnitInstanceId{11u},
                                                 UnitInstanceId{12u}, UnitInstanceId{13u}};

    std::vector<UnitInstanceId> fx_targets;
    for (const auto& tgt : targets) {
        TargetDamaged event{
            .attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{1u}, .target = tgt};
        auto expanded = res.expand(event);
        ASSERT_EQ(expanded.size(), 2u);
        const auto& bes = std::get<BattleEffectStarted>(expanded[1]);
        fx_targets.push_back(bes.target);
    }

    // Each injection must carry the correct distinct target
    for (std::size_t i = 0; i < targets.size(); ++i) {
        EXPECT_EQ(fx_targets[i], targets[i])
            << "FX target " << i << " must match TargetDamaged.target";
    }
}

// Non-FX events pass through unchanged
TEST(VisualEffectResolver, NonFxEventsPassThrough) {
    ResolverFixture const fx;
    auto                  res = fx.resolver();

    {
        // TargetDamaged with source that has NO heff passes through
        TargetDamaged ev{.attack_id = AttackInstanceId{1u},
                         .source = UnitInstanceId{2u},
                         .target = UnitInstanceId{1u}};
        auto          expanded = res.expand(ev);
        ASSERT_EQ(expanded.size(), 1u);
        EXPECT_TRUE(std::holds_alternative<TargetDamaged>(expanded[0]));
    }
    {
        UnitKilled ev{.target = UnitInstanceId{2u}};
        auto       expanded = res.expand(ev);
        ASSERT_EQ(expanded.size(), 1u);
        EXPECT_TRUE(std::holds_alternative<UnitKilled>(expanded[0]));
    }
    {
        ActorSelected ev{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}};
        auto          expanded = res.expand(ev);
        ASSERT_EQ(expanded.size(), 1u);
        EXPECT_TRUE(std::holds_alternative<ActorSelected>(expanded[0]));
    }
}

// Unknown UnitInstanceId → [original_event] without throwing
TEST(VisualEffectResolver, UnknownUnitIdPassesThrough) {
    ResolverFixture const fx;
    auto                  res = fx.resolver();

    AttackStarted event{.attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{99u}};
    EXPECT_NO_THROW({
        auto expanded = res.expand(event);
        ASSERT_EQ(expanded.size(), 1u);
        EXPECT_TRUE(std::holds_alternative<AttackStarted>(expanded[0]));
    });
}

// ──────────────────────────────────────────────────────────────────────────────
// Nightmare-style per-unit attack_fx_override tests
// Unit 3: has attack_fx_override (source_overlay=heffa1991, team_overlay=heffd1992,
// target_damage=tuchb2011) Unit 4: has heff+tuch but NO override (generic fallback)
// ──────────────────────────────────────────────────────────────────────────────

struct NightmareFixture {
    BattleScene               scene;
    UnitVisualProfileRegistry profiles;

    static constexpr std::uint32_t kNightmare = 3u;
    static constexpr std::uint32_t kGeneric = 4u;
    static constexpr std::uint32_t kTarget = 5u;

    NightmareFixture() {
        // Build profiles first, capture IDs, then assign to units.
        UnitAnimationRoleSet r_nightmare;
        r_nightmare.idle = UnitStateClip::from_a1(make_seq("idle_nm"));
        r_nightmare.attack = UnitStateClip::from_a1(make_seq("attack_nm"));
        r_nightmare.heff =
            BattleEffectClip{.clip = LayeredAnimationClip{.a1 = make_seq("heff_fallback")},
                             .family = "HEFF",
                             .direction_or_variant = 'A',
                             .requested_direction = 'A'};
        r_nightmare.tuch =
            BattleEffectClip{.clip = LayeredAnimationClip{.a1 = make_seq("tuch_fallback")},
                             .family = "TUCH",
                             .direction_or_variant = 'A',
                             .requested_direction = 'A'};
        r_nightmare.attack_fx_override = UnitAttackFxOverride{
            .source_attack_overlay =
                BattleEffectClip{.clip = LayeredAnimationClip{.a1 = make_seq("heffa1991")},
                                 .family = "HEFF",
                                 .direction_or_variant = 'A',
                                 .requested_direction = 'A'},
            .team_attack_overlay =
                BattleEffectClip{.clip = LayeredAnimationClip{.a1 = make_seq("heffd1992")},
                                 .family = "HEFF",
                                 .direction_or_variant = 'D',
                                 .requested_direction = 'D'},
            .target_damage_fx =
                BattleEffectClip{.clip = LayeredAnimationClip{.a1 = make_seq("tuchb2011")},
                                 .family = "TUCH",
                                 .direction_or_variant = 'B',
                                 .requested_direction = 'B'},
        };
        const auto nightmare_profile = profiles.add(std::move(r_nightmare));

        UnitAnimationRoleSet r_generic;
        r_generic.idle = UnitStateClip::from_a1(make_seq("idle_gen"));
        r_generic.attack = UnitStateClip::from_a1(make_seq("attack_gen"));
        r_generic.heff = BattleEffectClip{.clip = LayeredAnimationClip{.a1 = make_seq("heff_gen")},
                                          .family = "HEFF",
                                          .direction_or_variant = 'A',
                                          .requested_direction = 'A'};
        r_generic.tuch = BattleEffectClip{.clip = LayeredAnimationClip{.a1 = make_seq("tuch_gen")},
                                          .family = "TUCH",
                                          .direction_or_variant = 'A',
                                          .requested_direction = 'A'};
        const auto generic_profile = profiles.add(std::move(r_generic));

        UnitAnimationRoleSet r_target;
        r_target.idle = UnitStateClip::from_a1(make_seq("idle_tgt"));
        const auto target_profile = profiles.add(std::move(r_target));

        auto add_unit = [&](std::uint32_t unit_id, UnitVisualProfileId profile_id) {
            BattleUnit u;
            u.unit_instance_id = UnitInstanceId{unit_id};
            u.visual_profile_id = profile_id;
            VisualTrack base;
            base.kind = TrackKind::Base;
            base.player.load(make_seq("idle"));
            base.player.play();
            u.tracks.push_back(std::move(base));
            scene.add_unit(std::move(u));
        };
        add_unit(kNightmare, nightmare_profile);
        add_unit(kGeneric, generic_profile);
        add_unit(kTarget, target_profile);
    }

    [[nodiscard]] VisualEffectResolver resolver() const {
        return VisualEffectResolver{scene, &profiles};
    }
};

// AttackStarted for override unit → injects SourceAttackOverlayFx (heffa1991) and
// TeamAttackOverlayFx (heffd1992) NOT SourceCastFx
TEST(VisualEffectResolver, NightmareAttackStartedInjectsSourceOverlayAndTeamOverlay) {
    NightmareFixture const fx;
    auto                   res = fx.resolver();

    AttackStarted event{.attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{3u}};
    auto          expanded = res.expand(event);

    ASSERT_EQ(expanded.size(), 3u);
    ASSERT_TRUE(std::holds_alternative<AttackStarted>(expanded[0]));

    const auto& s = std::get<BattleEffectStarted>(expanded[1]);
    EXPECT_EQ(s.visual_role, BattleEffectVisualRole::SourceAttackOverlayFx);
    EXPECT_EQ(s.source, UnitInstanceId{3u});

    const auto& t = std::get<BattleEffectStarted>(expanded[2]);
    EXPECT_EQ(t.visual_role, BattleEffectVisualRole::TeamAttackOverlayFx);
    EXPECT_EQ(t.source, UnitInstanceId{3u});
}

// Override unit: AttackStarted does NOT inject SourceCastFx (TUCH)
TEST(VisualEffectResolver, NightmareAttackStartedDoesNotInjectSourceCastFx) {
    NightmareFixture const fx;
    auto                   res = fx.resolver();

    AttackStarted event{.attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{3u}};
    auto          expanded = res.expand(event);

    for (const auto& ev : expanded) {
        if (const auto* bes = std::get_if<BattleEffectStarted>(&ev)) {
            EXPECT_NE(bes->visual_role, BattleEffectVisualRole::SourceCastFx)
                << "SourceCastFx must not be injected for override unit";
        }
    }
}

// Override unit: TargetDamaged injects TargetDamageFx with TUCH override clip (tuchb2011)
TEST(VisualEffectResolver, NightmareTargetDamagedInjectsTuchBasedTargetDamageFx) {
    NightmareFixture const fx;
    auto                   res = fx.resolver();

    TargetDamaged event{.attack_id = AttackInstanceId{1u},
                        .source = UnitInstanceId{3u},
                        .target = UnitInstanceId{5u}};
    auto          expanded = res.expand(event);

    ASSERT_EQ(expanded.size(), 2u);
    ASSERT_TRUE(std::holds_alternative<TargetDamaged>(expanded[0]));
    const auto& bes = std::get<BattleEffectStarted>(expanded[1]);
    EXPECT_EQ(bes.visual_role, BattleEffectVisualRole::TargetDamageFx);
    EXPECT_EQ(bes.source, UnitInstanceId{3u});
    EXPECT_EQ(bes.target, UnitInstanceId{5u});
}

// Four TargetDamaged events from override unit → four distinct TargetDamageFx with correct targets
TEST(VisualEffectResolver, NightmareFourTargetDamagedProduceFourTuchTargetFx) {
    NightmareFixture const fx;
    auto                   res = fx.resolver();

    const std::vector<UnitInstanceId> targets = {UnitInstanceId{10u}, UnitInstanceId{11u},
                                                 UnitInstanceId{12u}, UnitInstanceId{13u}};

    std::vector<UnitInstanceId> fx_targets;
    for (const auto& tgt : targets) {
        TargetDamaged ev{
            .attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{3u}, .target = tgt};
        auto expanded = res.expand(ev);
        ASSERT_EQ(expanded.size(), 2u);
        fx_targets.push_back(std::get<BattleEffectStarted>(expanded[1]).target);
    }
    for (std::size_t i = 0; i < targets.size(); ++i) {
        EXPECT_EQ(fx_targets[i], targets[i]);
    }
}

// Override unit TargetDamaged: injects TargetDamageFx (not TeamAttackOverlayFx or
// SourceAttackOverlayFx)
TEST(VisualEffectResolver, NightmareTargetDamagedInjectsOnlyTargetDamageFx) {
    NightmareFixture const fx;
    auto                   res = fx.resolver();

    TargetDamaged event{.attack_id = AttackInstanceId{1u},
                        .source = UnitInstanceId{3u},
                        .target = UnitInstanceId{5u}};
    auto          expanded = res.expand(event);

    for (const auto& ev : expanded) {
        if (const auto* bes = std::get_if<BattleEffectStarted>(&ev)) {
            EXPECT_EQ(bes->visual_role, BattleEffectVisualRole::TargetDamageFx)
                << "Only TargetDamageFx must be injected from TargetDamaged";
        }
    }
}

// HEFFA1A/HEFFD are NOT spawned as per-target hit FX (only TeamAttackOverlayFx is single)
TEST(VisualEffectResolver, NightmareSourceOverlayIsNotSpawnedPerTarget) {
    NightmareFixture const fx;
    auto                   res = fx.resolver();

    // TargetDamaged should NOT produce SourceAttackOverlayFx or TeamAttackOverlayFx
    TargetDamaged event{.attack_id = AttackInstanceId{1u},
                        .source = UnitInstanceId{3u},
                        .target = UnitInstanceId{5u}};
    auto          expanded = res.expand(event);

    for (const auto& ev : expanded) {
        if (const auto* bes = std::get_if<BattleEffectStarted>(&ev)) {
            EXPECT_NE(bes->visual_role, BattleEffectVisualRole::SourceAttackOverlayFx)
                << "SourceAttackOverlayFx must not appear in TargetDamaged expansion";
            EXPECT_NE(bes->visual_role, BattleEffectVisualRole::TeamAttackOverlayFx)
                << "TeamAttackOverlayFx must not appear in TargetDamaged expansion";
        }
    }
}

// Generic unit (unit 4) still uses fallback: TUCH→SourceCastFx, HEFF→TargetDamageFx
TEST(VisualEffectResolver, GenericUnitFallbackUnaffectedByNightmareOverride) {
    NightmareFixture const fx;
    auto                   res = fx.resolver();

    {
        AttackStarted ev{.attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{4u}};
        auto          expanded = res.expand(ev);
        ASSERT_EQ(expanded.size(), 2u);
        const auto& bes = std::get<BattleEffectStarted>(expanded[1]);
        EXPECT_EQ(bes.visual_role, BattleEffectVisualRole::SourceCastFx);
        EXPECT_EQ(bes.role, BattleEffectRole::Tuch);
    }
    {
        TargetDamaged ev{.attack_id = AttackInstanceId{1u},
                         .source = UnitInstanceId{4u},
                         .target = UnitInstanceId{5u}};
        auto          expanded = res.expand(ev);
        ASSERT_EQ(expanded.size(), 2u);
        const auto& bes = std::get<BattleEffectStarted>(expanded[1]);
        EXPECT_EQ(bes.visual_role, BattleEffectVisualRole::TargetDamageFx);
        EXPECT_EQ(bes.role, BattleEffectRole::Heff);
    }
}

// TeamAttackOverlayFx is injected once from AttackStarted, not from TargetDamaged
TEST(VisualEffectResolver, NightmareTeamAttackOverlayInjectedFromAttackStartedNotTargetDamaged) {
    NightmareFixture const fx;
    auto                   res = fx.resolver();

    // Confirm present from AttackStarted
    {
        AttackStarted ev{.attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{3u}};
        auto          expanded = res.expand(ev);
        int           count = 0;
        for (const auto& e : expanded) {
            if (const auto* bes = std::get_if<BattleEffectStarted>(&e)) {
                if (bes->visual_role == BattleEffectVisualRole::TeamAttackOverlayFx)
                    ++count;
            }
        }
        EXPECT_EQ(count, 1) << "Exactly one TeamAttackOverlayFx from AttackStarted";
    }
    // Confirm absent from TargetDamaged
    {
        TargetDamaged ev{.attack_id = AttackInstanceId{1u},
                         .source = UnitInstanceId{3u},
                         .target = UnitInstanceId{5u}};
        auto          expanded = res.expand(ev);
        for (const auto& e : expanded) {
            if (const auto* bes = std::get_if<BattleEffectStarted>(&e)) {
                EXPECT_NE(bes->visual_role, BattleEffectVisualRole::TeamAttackOverlayFx)
                    << "TeamAttackOverlayFx must not appear in TargetDamaged expansion";
            }
        }
    }
}

} // namespace
} // namespace d2engine
