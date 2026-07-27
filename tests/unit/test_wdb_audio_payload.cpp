#include "d2res/wdb_audio_payload.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Chunk = std::pair<std::string, std::vector<std::uint8_t>>;

void append_u32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16U));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24U));
}

std::vector<std::uint8_t> make_wave(std::vector<Chunk> chunks) {
    std::vector<std::uint8_t> bytes{'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
    for (auto& [id, data] : chunks) {
        if (id.size() != 4U)
            throw std::invalid_argument("synthetic RIFF chunk id");
        bytes.insert(bytes.end(), id.begin(), id.end());
        append_u32(bytes, static_cast<std::uint32_t>(data.size()));
        bytes.insert(bytes.end(), data.begin(), data.end());
        if ((data.size() & 1U) != 0U)
            bytes.push_back(0);
    }
    return bytes;
}

std::vector<std::uint8_t> fmt_chunk(const std::uint16_t tag, const std::size_t size = 16) {
    std::vector<std::uint8_t> bytes(size, 0);
    if (size >= 2) {
        bytes[0] = static_cast<std::uint8_t>(tag);
        bytes[1] = static_cast<std::uint8_t>(tag >> 8U);
    }
    return bytes;
}

std::vector<std::uint8_t> make_mpeg_wave() {
    return make_wave({Chunk{"fact", {1, 2, 3, 4}}, Chunk{"data", {0xFF, 0xFB, 0x90, 0x64}},
                      Chunk{"fmt ", fmt_chunk(85)}});
}

TEST(WdbAudioPayload, PcmKeepsCompleteSourcePayload) {
    const auto source = make_wave({Chunk{"fmt ", fmt_chunk(1)}, Chunk{"data", {1, 2, 3}}});
    const auto result = d2res::make_wdb_playback_payload(source);
    EXPECT_EQ(result.encoding, d2res::WdbPlaybackEncoding::Wave);
    EXPECT_EQ(result.source_format_tag, 1);
    EXPECT_EQ(result.bytes, source);
}

TEST(WdbAudioPayload, MpegExtractsOnlyDeclaredData) {
    const auto source = make_mpeg_wave();
    const auto result = d2res::make_wdb_playback_payload(source);
    EXPECT_EQ(result.encoding, d2res::WdbPlaybackEncoding::Mp3);
    EXPECT_EQ(result.source_format_tag, 85);
    EXPECT_EQ(result.bytes, (std::vector<std::uint8_t>{0xFF, 0xFB, 0x90, 0x64}));
    EXPECT_NE(result.bytes[0], static_cast<std::uint8_t>('R'));
}

TEST(WdbAudioPayload, AcceptsOddChunksAndEitherChunkOrder) {
    const auto source = make_wave({Chunk{"JUNK", {9}}, Chunk{"data", {1}}, Chunk{"fact", {2, 3, 4}},
                                   Chunk{"fmt ", fmt_chunk(1)}});
    EXPECT_NO_THROW(static_cast<void>(d2res::make_wdb_playback_payload(source)));
}

TEST(WdbAudioPayload, RejectsUnsupportedTagWithNumericTag) {
    const auto source = make_wave({Chunk{"fmt ", fmt_chunk(3)}, Chunk{"data", {1}}});
    try {
        static_cast<void>(d2res::make_wdb_playback_payload(source));
        FAIL() << "expected unsupported format tag failure";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("3"), std::string::npos);
    }
}

TEST(WdbAudioPayload, RejectsMalformedRiffPayloads) {
    EXPECT_THROW(static_cast<void>(d2res::make_wdb_playback_payload(std::vector<std::uint8_t>{})),
                 std::runtime_error);
    EXPECT_THROW(static_cast<void>(d2res::make_wdb_playback_payload(std::vector<std::uint8_t>{
                     'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'X'})),
                 std::runtime_error);
    EXPECT_THROW(static_cast<void>(d2res::make_wdb_playback_payload(std::vector<std::uint8_t>{
                     'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 1})),
                 std::runtime_error);
    EXPECT_THROW(static_cast<void>(d2res::make_wdb_playback_payload(
                     make_wave({Chunk{"fmt ", fmt_chunk(1, 15)}, Chunk{"data", {1}}}))),
                 std::runtime_error);
    EXPECT_THROW(static_cast<void>(
                     d2res::make_wdb_playback_payload(make_wave({Chunk{"fmt ", fmt_chunk(1)}}))),
                 std::runtime_error);
    EXPECT_THROW(static_cast<void>(d2res::make_wdb_playback_payload(
                     make_wave({Chunk{"fmt ", fmt_chunk(1)}, Chunk{"data", {}}}))),
                 std::runtime_error);
}

TEST(WdbAudioPayload, RejectsTruncatedChunksAndPaddedOverflow) {
    auto truncated_header = make_wave({Chunk{"fmt ", fmt_chunk(1)}});
    truncated_header.push_back('x');
    EXPECT_THROW(static_cast<void>(d2res::make_wdb_playback_payload(truncated_header)),
                 std::runtime_error);

    auto truncated_payload = make_wave({Chunk{"fmt ", fmt_chunk(1)}});
    truncated_payload.insert(truncated_payload.end(), {'d', 'a', 't', 'a', 4, 0, 0, 0, 1});
    EXPECT_THROW(static_cast<void>(d2res::make_wdb_playback_payload(truncated_payload)),
                 std::runtime_error);

    std::vector<std::uint8_t> overflow{'R', 'I', 'F', 'F', 0,   0,   0,    0,    'W',  'A',
                                       'V', 'E', 'd', 'a', 't', 'a', 0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_THROW(static_cast<void>(d2res::make_wdb_playback_payload(overflow)), std::runtime_error);
}

} // namespace
