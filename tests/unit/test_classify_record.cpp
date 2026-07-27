#include <gtest/gtest.h>
#include "d2res/special_files.hpp"

using d2res::classify_record;
using d2res::SpecialFileType;

static d2res::Record make_rec(const std::string& name) {
    d2res::Record r;
    r.name = name;
    return r;
}

TEST(ClassifyRecord, NotSpecial_Normal) {
    EXPECT_EQ(classify_record(make_rec("G000UU0001.PNG")), SpecialFileType::NOT_SPECIAL);
}

TEST(ClassifyRecord, NotSpecial_Empty) {
    EXPECT_EQ(classify_record(make_rec("")), SpecialFileType::NOT_SPECIAL);
}

TEST(ClassifyRecord, IndexOpt) {
    EXPECT_EQ(classify_record(make_rec("-INDEX.OPT")), SpecialFileType::INDEX_OPT);
}

TEST(ClassifyRecord, ImagesOpt) {
    EXPECT_EQ(classify_record(make_rec("-IMAGES.OPT")), SpecialFileType::IMAGES_OPT);
}

TEST(ClassifyRecord, AnimsOpt) {
    EXPECT_EQ(classify_record(make_rec("-ANIMS.OPT")), SpecialFileType::ANIMS_OPT);
}

TEST(ClassifyRecord, UnknownOpt) {
    EXPECT_EQ(classify_record(make_rec("-SOMETHING.OPT")), SpecialFileType::UNKNOWN_OPT);
}

TEST(ClassifyRecord, UnknownDat) {
    EXPECT_EQ(classify_record(make_rec("-FOO.DAT")), SpecialFileType::UNKNOWN_DAT);
}

TEST(ClassifyRecord, UnknownSpecial_NoDot) {
    EXPECT_EQ(classify_record(make_rec("-MYSTERY")), SpecialFileType::UNKNOWN_SPECIAL);
}

TEST(ClassifyRecord, UnknownSpecial_OtherExt) {
    EXPECT_EQ(classify_record(make_rec("-THING.BIN")), SpecialFileType::UNKNOWN_SPECIAL);
}
