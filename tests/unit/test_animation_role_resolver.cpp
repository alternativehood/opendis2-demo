#include <gtest/gtest.h>

#include "d2engine/battle_view/animation_role_resolver.hpp"

namespace d2engine {

namespace {

AnimationRoleResolver make_resolver() {
    return AnimationRoleResolver(std::unordered_set<std::string>{
        "G000UU0001IDLEA1A00",
        "G000UU0001IDLEA1D00",
        "G000UU9999IDLEA1A00",
        "G000UU0008TUCHA1B00",
    });
}

} // namespace

TEST(AnimationRoleResolver, ResolveExistingD) {
    const auto r = make_resolver();
    EXPECT_EQ(r.resolve("UU0001", "IDLEA", 'D'), "G000UU0001IDLEA1D00");
}

TEST(AnimationRoleResolver, FallbackToAOnDMiss) {
    const auto r = make_resolver();
    EXPECT_EQ(r.resolve("UU9999", "IDLEA", 'D'), "G000UU9999IDLEA1A00");
}

TEST(AnimationRoleResolver, EmptyOnMiss) {
    const auto r = make_resolver();
    EXPECT_EQ(r.resolve("UXXXX", "IDLEA", 'A'), "");
}

TEST(AnimationRoleResolver, FallbackToBWhenAAndDMiss) {
    const auto r = make_resolver();
    EXPECT_EQ(r.resolve("UU0008", "TUCHA", 'A'), "G000UU0008TUCHA1B00");
}

// --- resolve_layers: A2 B-direction fallback ---

TEST(AnimationRoleResolver, ResolveLayers_A2BFallback_GolemStyle) {
    // Golem-like: only B-direction variants exist for both A1 and A2
    AnimationRoleResolver const r(std::unordered_set<std::string>{
        "G000UU0020HEFFA1B00",
        "G000UU0020HEFFA2B00",
    });
    const auto                  triple = r.resolve_layers("UU0020", "HEFFA", 'A');
    EXPECT_EQ(triple.a1, "G000UU0020HEFFA1B00");
    EXPECT_EQ(triple.a2, "G000UU0020HEFFA2B00");
    EXPECT_EQ(triple.s1, ""); // HEFF has no S suffix variant in this set
}

TEST(AnimationRoleResolver, ResolveLayers_A2BFallback_GargoyleStyle) {
    // Gargoyle-like: only B-direction variants exist for TUCH A1 and A2
    AnimationRoleResolver const r(std::unordered_set<std::string>{
        "G000UU0021TUCHA1B00",
        "G000UU0021TUCHA2B00",
    });
    const auto                  triple = r.resolve_layers("UU0021", "TUCHA", 'A');
    EXPECT_EQ(triple.a1, "G000UU0021TUCHA1B00");
    EXPECT_EQ(triple.a2, "G000UU0021TUCHA2B00");
}

TEST(AnimationRoleResolver, ResolveLayers_A2DirectionSpecificBeatsB) {
    // When A-direction A2 exists it wins over B fallback
    AnimationRoleResolver const r(std::unordered_set<std::string>{
        "G000UU0022TUCHA1A00",
        "G000UU0022TUCHA2A00",
        "G000UU0022TUCHA2B00", // should not be used
    });
    const auto                  triple = r.resolve_layers("UU0022", "TUCHA", 'A');
    EXPECT_EQ(triple.a2, "G000UU0022TUCHA2A00");
}

TEST(AnimationRoleResolver, ResolveLayers_A2DFallsToAThenB) {
    // For direction D: tries D, then A, then B
    AnimationRoleResolver const r(std::unordered_set<std::string>{
        "G000UU0023HEFFA1B00",
        "G000UU0023HEFFA2B00",
    });
    const auto                  triple = r.resolve_layers("UU0023", "HEFFA", 'D');
    EXPECT_EQ(triple.a2, "G000UU0023HEFFA2B00"); // D and A miss, B hits
}

} // namespace d2engine
