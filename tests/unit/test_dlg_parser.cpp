#include <gtest/gtest.h>

#include "d2res/dlg_parser.hpp"

using namespace d2res;

TEST(DlgParser, SingleDialog) {
    const char* input = "DIALOG\tMY_DLG,0,0,100,100\nBEGIN\nEND\n";
    DlgParser   p;
    p.parse(input);
    ASSERT_EQ(p.dialogs().size(), 1u);
    EXPECT_EQ(p.dialogs()[0].id, "MY_DLG");
    EXPECT_EQ(p.dialogs()[0].bounds[0], 0);
    EXPECT_EQ(p.dialogs()[0].bounds[1], 0);
    EXPECT_EQ(p.dialogs()[0].bounds[2], 100);
    EXPECT_EQ(p.dialogs()[0].bounds[3], 100);
}

TEST(DlgParser, ImageElement) {
    const char* input = "DIALOG\tDLG,0,0,800,600\nBEGIN\nIMAGE\tIMG,10,20,30,40,MYIMG,\"\"\nEND\n";
    DlgParser   p;
    p.parse(input);
    ASSERT_EQ(p.dialogs().size(), 1u);
    ASSERT_EQ(p.dialogs()[0].elements.size(), 1u);
    const auto& el = p.dialogs()[0].elements[0];
    EXPECT_EQ(el.type, "IMAGE");
    EXPECT_EQ(el.id, "IMG");
    EXPECT_EQ(el.bounds[0], 10);
    EXPECT_EQ(el.extra["image"].get<std::string>(), "MYIMG");
}

TEST(DlgParser, ButtonElement) {
    const char* input =
        "DIALOG\tDLG,0,0,800,600\nBEGIN\nBUTTON\tBTN,0,0,10,10,N,H,C,D,\"MY_KEY\",0,1\nEND\n";
    DlgParser p;
    p.parse(input);
    ASSERT_EQ(p.dialogs().size(), 1u);
    ASSERT_EQ(p.dialogs()[0].elements.size(), 1u);
    const auto& el = p.dialogs()[0].elements[0];
    EXPECT_EQ(el.type, "BUTTON");
    EXPECT_EQ(el.extra["text_key"].get<std::string>(), "MY_KEY");
}

TEST(DlgParser, UnknownElement) {
    const char* input = "DIALOG\tDLG,0,0,800,600\nBEGIN\nFOOBAR\tSOMETHING,1,2,3,4\nEND\n";
    DlgParser   p;
    EXPECT_NO_THROW(p.parse(input));
    ASSERT_EQ(p.dialogs()[0].elements.size(), 1u);
    EXPECT_EQ(p.dialogs()[0].elements[0].type, "UNKNOWN");
}

TEST(DlgParser, ToJsonStructure) {
    const char* input = "DIALOG\tDLG,0,0,800,600\nBEGIN\nEND\n";
    DlgParser   p;
    p.parse(input);
    auto j = p.to_json();
    EXPECT_TRUE(j.is_array());
    ASSERT_FALSE(j.empty());
    EXPECT_TRUE(j[0].contains("id"));
    EXPECT_TRUE(j[0].contains("bounds"));
    EXPECT_TRUE(j[0].contains("elements"));
}
