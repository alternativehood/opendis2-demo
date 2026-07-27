#include <gtest/gtest.h>
#include "d2res/game_scanner.hpp"

// SHA-256("abc") = ba7816bf8f01cfea414140de5dae2ec73b00361bbef0469132d0eb4045e0f3f
// We test via ScanResult::to_json() keys and likely_content logic directly.

TEST(GameScanner, ManifestJsonKeys) {
    d2res::ScanResult r;
    r.game_root = "/test/root";
    r.scan_timestamp = "2026-06-07T00:00:00Z";
    r.total_files = 2;

    d2res::ContainerEntry c;
    c.path = "Imgs/BatUnits.ff";
    c.size = 1000;
    c.sha256 = "aabbcc";
    c.record_count = 42;
    c.special_files = {"-INDEX.OPT"};
    c.likely_content = {d2res::ContentKind::Images};
    r.containers.push_back(c);

    d2res::OtherFileEntry o;
    o.path = "Video/Intro.bik";
    o.size = 500;
    o.sha256 = "ddeeff";
    o.type = "bik";
    r.other_files.push_back(o);

    const auto j = r.to_json();
    EXPECT_TRUE(j.contains("game_root"));
    EXPECT_TRUE(j.contains("scan_timestamp"));
    EXPECT_TRUE(j.contains("total_files"));
    EXPECT_TRUE(j.contains("mqdb_containers"));
    EXPECT_TRUE(j.contains("other_files"));
    EXPECT_TRUE(j.contains("warnings"));

    EXPECT_EQ(j["mqdb_containers"].size(), 1u);
    EXPECT_EQ(j["other_files"].size(), 1u);
    EXPECT_EQ(j["mqdb_containers"][0]["record_count"].get<int>(), 42);
    EXPECT_EQ(j["other_files"][0]["type"].get<std::string>(), "bik");
}

TEST(GameScanner, LikelyContentImages) {
    d2res::ScanResult r;
    r.game_root = "/g";
    r.scan_timestamp = "x";
    d2res::ContainerEntry c;
    c.path = "Imgs/Capital.ff";
    c.special_files = {"-INDEX.OPT", "-IMAGES.OPT", "-ANIMS.OPT"};
    c.likely_content = {d2res::ContentKind::Images, d2res::ContentKind::Animations};
    r.containers.push_back(c);

    const auto j = r.to_json();
    const auto lc = j["mqdb_containers"][0]["likely_content"];
    EXPECT_TRUE(lc.contains("images") || [&] {
        for (const auto& v : lc) {
            if (v.get<std::string>() == "images")
                return true;
        }
        return false;
    }());
}

TEST(GameScanner, ContentKindAnimationsSerializesToAnimationsNotAnims) {
    d2res::ScanResult r;
    r.game_root = "/g";
    r.scan_timestamp = "x";
    d2res::ContainerEntry c;
    c.path = "Imgs/IsoAnim.ff";
    c.likely_content = {d2res::ContentKind::Animations};
    r.containers.push_back(c);

    const auto j = r.to_json();
    const auto lc = j["mqdb_containers"][0]["likely_content"];
    ASSERT_EQ(lc.size(), 1u);
    EXPECT_EQ(lc[0].get<std::string>(), "animations");
}

TEST(GameScanner, ContainersSortedByPath) {
    d2res::ScanResult r;
    r.game_root = "/g";
    r.scan_timestamp = "x";
    for (const char* p : {"Imgs/Z.ff", "Imgs/A.ff", "Imgs/M.ff"}) {
        d2res::ContainerEntry ce;
        ce.path = p;
        r.containers.push_back(ce);
    }
    const auto j = r.to_json();
    EXPECT_EQ(j["mqdb_containers"][0]["path"].get<std::string>(), "Imgs/A.ff");
    EXPECT_EQ(j["mqdb_containers"][1]["path"].get<std::string>(), "Imgs/M.ff");
    EXPECT_EQ(j["mqdb_containers"][2]["path"].get<std::string>(), "Imgs/Z.ff");
}

TEST(GameScanner, DataTablesContainerClassified) {
    d2res::ScanResult r;
    r.game_root = "/g";
    r.scan_timestamp = "x";
    d2res::ContainerEntry c;
    c.path = "Tglobal.dbf";
    c.likely_content = {d2res::ContentKind::DataTables};
    r.containers.push_back(c);

    const auto j = r.to_json();
    const auto lc = j["mqdb_containers"][0]["likely_content"];
    ASSERT_EQ(lc.size(), 1u);
    EXPECT_EQ(lc[0].get<std::string>(), "data_tables");
}

TEST(GameScanner, DataTablesOtherFileClassified) {
    d2res::ScanResult r;
    r.game_root = "/g";
    r.scan_timestamp = "x";
    d2res::OtherFileEntry o;
    o.path = "Tglobal.dbf";
    o.type = "dbf";
    o.likely_content = {d2res::ContentKind::DataTables};
    r.other_files.push_back(o);

    const auto j = r.to_json();
    const auto lc = j["other_files"][0]["likely_content"];
    ASSERT_EQ(lc.size(), 1u);
    EXPECT_EQ(lc[0].get<std::string>(), "data_tables");
}
