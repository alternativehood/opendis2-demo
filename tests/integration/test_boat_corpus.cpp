#include <gtest/gtest.h>
#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/asset_runtime_catalog_adapter.hpp"
#include "d2res/mqdb.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#ifndef DISCIPLES2_GAME_ROOT
#define DISCIPLES2_GAME_ROOT ""
#endif

namespace {

static std::filesystem::path game_root() {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT"); // NOLINT
    return (env != nullptr && env[0] != '\0') ? std::filesystem::path(env)
                                              : std::filesystem::path(DISCIPLES2_GAME_ROOT);
}

std::string extract_owner(std::string_view name, std::string_view suffix) {
    auto pos = name.find(suffix);
    if (pos == std::string_view::npos)
        return {};
    return std::string(name.substr(0, pos));
}

int extract_direction(std::string_view name, std::string_view suffix) {
    auto pos = name.find(suffix);
    if (pos == std::string_view::npos)
        return -1;
    auto dir_pos = pos + suffix.size();
    if (dir_pos >= name.size())
        return -1;
    char c = name[dir_pos];
    if (c >= '0' && c <= '7')
        return static_cast<int>(c - '0');
    return -1;
}

} // namespace

TEST(BoatCorpus, BoatsCompleteAndConsistent) {
    const auto root = game_root();
    const auto iso_path = root / "Imgs/Isounit.ff";
    if (root.empty() || !std::filesystem::exists(iso_path))
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set or Isounit.ff not found: " << iso_path;

    // Verify the MQDB container opens and has records
    const auto container = d2res::MqdbContainer::open(iso_path);
    const auto mqdb_names = container.names();
    EXPECT_FALSE(mqdb_names.empty());

    // Use the catalog to get all animation names (BOAT/SBOA are OPT animation names)
    d2engine::AssetRuntime               assets(root, 1);
    d2engine::AssetRuntimeCatalogAdapter catalog(assets);
    const auto                           anim_names = catalog.animations_in("Imgs/Isounit.ff");

    // Separate BOAT and SBOA names
    std::map<std::string, std::set<int>> boat_owners; // owner -> directions
    std::map<std::string, std::set<int>> sboa_owners; // owner -> directions

    for (const auto& name : anim_names) {
        if (name.find("BOAT") != std::string::npos && name.find("SBOA") == std::string::npos) {
            auto owner = extract_owner(name, "BOAT");
            int  dir = extract_direction(name, "BOAT");
            if (!owner.empty() && dir >= 0)
                boat_owners[owner].insert(dir);
        } else if (name.find("SBOA") != std::string::npos) {
            auto owner = extract_owner(name, "SBOA");
            int  dir = extract_direction(name, "SBOA");
            if (!owner.empty() && dir >= 0)
                sboa_owners[owner].insert(dir);
        }
    }

    // Exact real-game corpus counts
    EXPECT_EQ(boat_owners.size(), 6u) << "Exactly 6 races own BOAT sequences";
    EXPECT_EQ(sboa_owners.size(), 5u) << "Exactly 5 races own SBOA shadow sequences";

    // Every BOAT owner must have all 8 directions
    int complete_boat_owners = 0;
    int total_boat_sequences = 0;
    for (const auto& [owner, dirs] : boat_owners) {
        if (dirs.size() == 8)
            ++complete_boat_owners;
        total_boat_sequences += static_cast<int>(dirs.size());
    }
    EXPECT_EQ(complete_boat_owners, static_cast<int>(boat_owners.size()))
        << "Every BOAT owner must have all 8 directions";
    EXPECT_EQ(total_boat_sequences, 48) << "6 owners * 8 directions = 48 total BOAT sequences";

    // Verify each BOAT sequence has exactly 16 frames, positive native canvas,
    // non-empty frame records, valid sprite_metadata, positive dimensions, visible pieces
    for (const auto& [owner, dirs] : boat_owners) {
        for (int dir : dirs) {
            const auto anim_name = owner + "BOAT" + std::to_string(dir);
            const auto seq = catalog.animation_sequence("Imgs/Isounit.ff", anim_name);
            EXPECT_EQ(seq.frames.size(), 16u)
                << anim_name << " expected 16 frames, got " << seq.frames.size();
            EXPECT_GT(seq.native_canvas_w, 0) << anim_name;
            EXPECT_GT(seq.native_canvas_h, 0) << anim_name;
            for (std::size_t fi = 0; fi < seq.frames.size(); ++fi) {
                const auto& frame = seq.frames[fi];
                EXPECT_FALSE(frame.image_name.empty())
                    << anim_name << " frame " << fi << " has empty record";
                const auto sm = catalog.sprite_metadata("Imgs/Isounit.ff", frame.image_name);
                EXPECT_GT(sm.canvas_width, 0)
                    << anim_name << " frame " << fi << " record=" << frame.image_name
                    << " width=" << sm.canvas_width;
                EXPECT_GT(sm.canvas_height, 0)
                    << anim_name << " frame " << fi << " record=" << frame.image_name
                    << " height=" << sm.canvas_height;
                EXPECT_TRUE(sm.has_visible_pieces)
                    << anim_name << " frame " << fi << " record=" << frame.image_name
                    << " has_visible_pieces=false (all BOAT body frames must be visible)";
            }
        }
    }

    // Verify each SBOA sequence: 16 frames, positive canvas, non-empty records,
    // valid sprite_metadata, positive dimensions. Classify as all-visible,
    // all-authored-empty, or mixed. No mixed sequences allowed.
    int total_sboa_sequences = 0;
    int visible_sboa = 0;
    int authored_empty_sboa = 0;
    int mixed_sboa = 0;
    for (const auto& [owner, dirs] : sboa_owners) {
        for (int dir : dirs) {
            const auto anim_name = owner + "SBOA" + std::to_string(dir);
            const auto seq = catalog.animation_sequence("Imgs/Isounit.ff", anim_name);
            EXPECT_EQ(seq.frames.size(), 16u)
                << anim_name << " expected 16 frames, got " << seq.frames.size();
            EXPECT_GT(seq.native_canvas_w, 0) << anim_name;
            EXPECT_GT(seq.native_canvas_h, 0) << anim_name;
            bool found_visible = false;
            bool found_empty = false;
            for (std::size_t fi = 0; fi < seq.frames.size(); ++fi) {
                const auto& frame = seq.frames[fi];
                EXPECT_FALSE(frame.image_name.empty())
                    << anim_name << " frame " << fi << " has empty record";
                const auto sm = catalog.sprite_metadata("Imgs/Isounit.ff", frame.image_name);
                EXPECT_GT(sm.canvas_width, 0)
                    << anim_name << " frame " << fi << " record=" << frame.image_name
                    << " width=" << sm.canvas_width;
                EXPECT_GT(sm.canvas_height, 0)
                    << anim_name << " frame " << fi << " record=" << frame.image_name
                    << " height=" << sm.canvas_height;
                if (sm.has_visible_pieces)
                    found_visible = true;
                else
                    found_empty = true;
            }
            if (found_visible && !found_empty)
                ++visible_sboa;
            else if (!found_visible && found_empty)
                ++authored_empty_sboa;
            else
                ++mixed_sboa;
            ++total_sboa_sequences;
        }
    }
    EXPECT_EQ(total_sboa_sequences, 40) << "5 owners * 8 directions = 40 total SBOA sequences";
    EXPECT_EQ(mixed_sboa, 0) << "No mixed SBOA sequences allowed";
    GTEST_LOG_(INFO) << "SBOA classification: visible=" << visible_sboa
                     << " authored_empty=" << authored_empty_sboa << " mixed=" << mixed_sboa;
}

TEST(BoatCorpus, SixthOwnerMissingSboa) {
    const auto root = game_root();
    const auto iso_path = root / "Imgs/Isounit.ff";
    if (root.empty() || !std::filesystem::exists(iso_path))
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set or Isounit.ff not found: " << iso_path;

    d2engine::AssetRuntime               assets(root, 1);
    d2engine::AssetRuntimeCatalogAdapter catalog(assets);
    const auto                           anim_names = catalog.animations_in("Imgs/Isounit.ff");

    std::set<std::string> boat_owners;
    std::set<std::string> sboa_owners;

    for (const auto& name : anim_names) {
        if (name.find("BOAT") != std::string::npos && name.find("SBOA") == std::string::npos) {
            auto owner = extract_owner(name, "BOAT");
            if (!owner.empty())
                boat_owners.insert(owner);
        } else if (name.find("SBOA") != std::string::npos) {
            auto owner = extract_owner(name, "SBOA");
            if (!owner.empty())
                sboa_owners.insert(owner);
        }
    }

    EXPECT_EQ(boat_owners.size(), 6u) << "Exactly 6 BOAT owners expected in real game data";
    EXPECT_EQ(sboa_owners.size(), 5u) << "Exactly 5 SBOA owners expected in real game data";

    // Find owners that have BOAT but no SBOA
    std::vector<std::string> missing_sboa;
    for (const auto& owner : boat_owners) {
        if (!sboa_owners.contains(owner))
            missing_sboa.push_back(owner);
    }

    ASSERT_EQ(missing_sboa.size(), 1u) << "Exactly one BOAT owner should be missing SBOA shadows";
    GTEST_LOG_(INFO) << "Owner without SBOA shadow: " << missing_sboa[0];
}
