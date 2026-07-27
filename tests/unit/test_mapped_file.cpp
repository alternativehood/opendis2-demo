#include <gtest/gtest.h>
#include "d2res/mapped_file.hpp"
#include "d2res/mqdb.hpp"

#include "tests/test_helpers.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

TEST(MappedFileTest, EmptyFileOpenAndClose) {
    TempDir    temp("mapped_file_empty");
    const auto path = temp.path() / "empty.ff";
    {
        std::ofstream out(path, std::ios::binary);
    }
    auto mapped = d2res::MappedFile::open(path);
    EXPECT_FALSE(mapped);
    EXPECT_EQ(mapped.data(), nullptr);
    EXPECT_EQ(mapped.size(), 0u);
}

TEST(MappedFileTest, NonAsciiFilenameOpens) {
    TempDir                           temp("mapped_file_unicode");
    const auto                        path = temp.path() / u8"mapped_file_é漢字.ff";
    const std::array<std::uint8_t, 4> bytes = {0x41, 0x42, 0x43, 0x44};
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    auto mapped = d2res::MappedFile::open(path);
    ASSERT_TRUE(mapped);
    ASSERT_NE(mapped.data(), nullptr);
    ASSERT_EQ(mapped.size(), bytes.size());
    EXPECT_EQ(std::memcmp(mapped.data(), bytes.data(), bytes.size()), 0);
}

TEST(MappedFileTest, MoveConstructionClearsSource) {
    TempDir    temp("mapped_file_move_ctor");
    const auto path = temp.path() / "move_ctor.ff";
    {
        std::ofstream out(path, std::ios::binary);
        out << "abc";
    }
    auto source = d2res::MappedFile::open(path);
    auto moved = std::move(source);
    EXPECT_EQ(source.data(), nullptr);
    EXPECT_EQ(source.size(), 0u);
    ASSERT_TRUE(moved);
    ASSERT_EQ(moved.size(), 3u);
}

TEST(MappedFileTest, MoveAssignmentClearsSource) {
    TempDir    temp("mapped_file_move_assign");
    const auto path = temp.path() / "move_assign.ff";
    {
        std::ofstream out(path, std::ios::binary);
        out << "abcd";
    }
    auto              source = d2res::MappedFile::open(path);
    d2res::MappedFile destination;
    destination = std::move(source);
    EXPECT_EQ(source.data(), nullptr);
    EXPECT_EQ(source.size(), 0u);
    ASSERT_TRUE(destination);
    ASSERT_EQ(destination.size(), 4u);
}

struct MqdbFixture {
    std::vector<uint8_t> bytes;

    static void set_le(uint8_t* dst, int32_t v) {
        for (int i = 0; i < 4; ++i)
            dst[i] = static_cast<uint8_t>((static_cast<uint32_t>(v) >> (i * 8)) & 0xFF);
    }

    static std::vector<uint8_t>
    build_name_table(const std::vector<std::pair<int32_t, std::string>>& entries) {
        std::vector<uint8_t> table(4);
        set_le(table.data(), static_cast<int32_t>(entries.size()));
        for (const auto& [id, name] : entries) {
            std::size_t base = table.size();
            table.resize(base + 260);
            std::memset(table.data() + base, 0, 260);
            auto truncated = name.substr(0, 255);
            std::memcpy(table.data() + base, truncated.data(), truncated.size());
            set_le(table.data() + base + 256, id);
        }
        return table;
    }

    static std::vector<uint8_t> build_record(int32_t id, int32_t size, int32_t realFileSize,
                                             int32_t isNotDeleted, int32_t recordMagic,
                                             std::span<const uint8_t> payload) {
        std::vector<uint8_t> rec(28 + payload.size());
        std::memcpy(rec.data(), "MQRC", 4);
        set_le(rec.data() + 8, id);
        set_le(rec.data() + 12, size);
        set_le(rec.data() + 16, realFileSize);
        set_le(rec.data() + 20, isNotDeleted);
        set_le(rec.data() + 24, recordMagic);
        std::memcpy(rec.data() + 28, payload.data(), payload.size());
        return rec;
    }

    static MqdbFixture minimal() {
        std::array<uint8_t, 4>                       payload = {0x10, 0x20, 0x30, 0x40};
        std::vector<std::pair<int32_t, std::string>> names = {{1, "rec.png"}};

        MqdbFixture f;
        f.bytes.insert(f.bytes.end(), {'M', 'Q', 'D', 'B'});
        for (int i = 0; i < 24; ++i)
            f.bytes.push_back(0);

        auto psz = static_cast<int32_t>(payload.size());
        auto rec1 = build_record(1, psz, psz, 1, 0, payload);
        f.bytes.insert(f.bytes.end(), rec1.begin(), rec1.end());

        auto name_data = build_name_table(names);
        auto nsz = static_cast<int32_t>(name_data.size());
        auto rec2 = build_record(2, nsz, nsz, 1, 0, std::span<const uint8_t>(name_data));
        f.bytes.insert(f.bytes.end(), rec2.begin(), rec2.end());

        return f;
    }

    fs::path write_to_temp(const std::string& label) const {
        auto          p = fs::temp_directory_path() / ("mqdb_test_" + label + ".ff");
        std::ofstream f(p, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        return p;
    }
};

} // namespace

TEST(MappedFileTest, LifetimeAfterOpen) {
    auto fixture = MqdbFixture::minimal();
    auto path = fixture.write_to_temp("lifetime");
    {
        auto c = d2res::MqdbContainer::open(path);
        auto view = c.payload_view(0);
        ASSERT_EQ(view.size(), 4u);
        EXPECT_EQ(view[0], 0x10);
        EXPECT_EQ(view[3], 0x40);
        auto rec = c.find_by_name("rec.png");
        ASSERT_TRUE(rec.has_value());
    }
    fs::remove(path);
}

TEST(MappedFileTest, MoveConstructionPreservesMapping) {
    auto fixture = MqdbFixture::minimal();
    auto path = fixture.write_to_temp("move_ctor");
    {
        auto c1 = d2res::MqdbContainer::open(path);
        auto c2 = std::move(c1);
        auto view = c2.payload_view(0);
        EXPECT_EQ(view.size(), 4u);
        EXPECT_EQ(view[0], 0x10);
    }
    fs::remove(path);
}

TEST(MappedFileTest, MoveAssignmentPreservesMapping) {
    auto fixture = MqdbFixture::minimal();
    auto path = fixture.write_to_temp("move_assign");
    {
        auto                 c1 = d2res::MqdbContainer::open(path);
        d2res::MqdbContainer c2;
        c2 = std::move(c1);
        auto view = c2.payload_view(0);
        EXPECT_EQ(view.size(), 4u);
        EXPECT_EQ(view[0], 0x10);
        EXPECT_EQ(view[1], 0x20);
        EXPECT_EQ(view[2], 0x30);
        EXPECT_EQ(view[3], 0x40);
    }
    fs::remove(path);
}

TEST(MappedFileTest, TruncatedPayloadIsSafe) {
    auto                         fixture = MqdbFixture::minimal();
    static constexpr std::size_t kRecOff = 28;
    if (fixture.bytes.size() > kRecOff + 24) {
        MqdbFixture::set_le(fixture.bytes.data() + kRecOff + 16, 9999);
    }
    auto path = fixture.write_to_temp("trunc");
    {
        auto c = d2res::MqdbContainer::open(path);
        ASSERT_GE(c.records().size(), 1u);
        const auto& rec = c.records()[0];

        auto fsize = static_cast<std::size_t>(fs::file_size(path));
        EXPECT_LE(rec.payloadOffset, fsize);
        // Parser clamps realFileSize to remaining file bytes
        EXPECT_EQ(static_cast<std::size_t>(rec.realFileSize), fsize - rec.payloadOffset);

        auto view = c.payload_view(rec.index);
        EXPECT_EQ(view.size(), static_cast<std::size_t>(rec.realFileSize));
        EXPECT_LE(view.size(), fsize - rec.payloadOffset);

        bool found_warning = false;
        for (const auto& w : c.warnings()) {
            if (w.find("payload extends beyond file") != std::string::npos) {
                found_warning = true;
                break;
            }
        }
        EXPECT_TRUE(found_warning) << "truncation warning not produced";
    }
    fs::remove(path);
}

TEST(MappedFileTest, PayloadViewsRemainInBounds) {
    auto fixture = MqdbFixture::minimal();
    auto path = fixture.write_to_temp("bounds");
    {
        auto c = d2res::MqdbContainer::open(path);
        auto fsize = static_cast<std::size_t>(fs::file_size(path));

        ASSERT_GE(c.records().size(), 2u);
        for (const auto& rec : c.records()) {
            EXPECT_LE(rec.payloadOffset, fsize);
            EXPECT_GE(rec.realFileSize, 0);
            auto view = c.payload_view(rec.index);
            EXPECT_EQ(view.size(), static_cast<std::size_t>(rec.realFileSize));
            EXPECT_LE(view.size(), fsize - rec.payloadOffset);
        }
    }
    fs::remove(path);
}
