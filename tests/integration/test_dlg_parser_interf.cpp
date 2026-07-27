#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

#include "d2res/dlg_parser.hpp"

namespace fs = std::filesystem;

static const char* GAME_ROOT = DISCIPLES2_GAME_ROOT;

class DlgParserInterfTest : public ::testing::Test {
protected:
    void SetUp() override {
        if ((GAME_ROOT == nullptr) || std::string(GAME_ROOT).empty()) {
            GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
        }
        dlg_path_ = fs::path(GAME_ROOT) / "Interf" / "Interf.dlg";
        if (!fs::exists(dlg_path_)) {
            GTEST_SKIP() << "Interf.dlg not found: " << dlg_path_;
        }
        std::ifstream f(dlg_path_);
        text_.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    fs::path    dlg_path_;
    std::string text_;
};

TEST_F(DlgParserInterfTest, NonEmptyDialogs) {
    d2res::DlgParser p;
    p.parse(text_);
    EXPECT_FALSE(p.dialogs().empty());
}

TEST_F(DlgParserInterfTest, DialogHasDlgPrefix) {
    d2res::DlgParser p;
    p.parse(text_);
    bool found = false;
    for (const auto& d : p.dialogs()) {
        if (d.id.find("DLG_") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(DlgParserInterfTest, HasImageElement) {
    d2res::DlgParser p;
    p.parse(text_);
    bool found = false;
    for (const auto& d : p.dialogs()) {
        for (const auto& el : d.elements) {
            if (el.type == "IMAGE") {
                found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(DlgParserInterfTest, JsonArrayStructure) {
    d2res::DlgParser p;
    p.parse(text_);
    auto j = p.to_json();
    ASSERT_TRUE(j.is_array());
    ASSERT_FALSE(j.empty());
    EXPECT_TRUE(j[0].contains("id"));
    EXPECT_TRUE(j[0].contains("bounds"));
    EXPECT_TRUE(j[0].contains("elements"));
}
