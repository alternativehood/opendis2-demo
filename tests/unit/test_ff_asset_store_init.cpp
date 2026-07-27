#include <gtest/gtest.h>

#include "d2engine/assets/animation_asset_preloader.hpp"
#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/ff_asset_store.hpp"
#include "d2engine/assets/image_asset_key.hpp"
#include "d2res/opt_maps.hpp"
#include "d2res/rgba_buffer.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <lodepng.h>
#include <span>
#include <string>
#include <vector>

#include "tests/test_process.hpp"

namespace d2engine {
namespace {

namespace fs = std::filesystem;

// ── Minimal valid PNG (1×1) encoded at test time ───────────────────────
std::vector<uint8_t> make_1x1_png(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    const unsigned char        pixels[4] = {r, g, b, a};
    std::vector<unsigned char> out;
    unsigned                   err = lodepng::encode(out, pixels, 1, 1);
    if (err != 0) {
        throw std::runtime_error(std::string("lodepng_encode: ") + lodepng_error_text(err));
    }
    return {out.begin(), out.end()};
}

// ── Little-endian integer helpers ──────────────────────────────────────
void write_i32le(std::vector<uint8_t>& buf, int32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void write_u16le(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void write_bytes(std::vector<uint8_t>& buf, std::span<const uint8_t> data) {
    buf.insert(buf.end(), data.begin(), data.end());
}

void write_cstr(std::vector<uint8_t>& buf, std::string_view s) {
    buf.insert(buf.end(), s.begin(), s.end());
    buf.push_back(0);
}

void write_fixed_str(std::vector<uint8_t>& buf, std::string_view s, std::size_t width) {
    auto start = buf.size();
    buf.insert(buf.end(), s.begin(), s.end());
    if (buf.size() - start > width)
        buf.resize(start + width);
    while (buf.size() - start < width)
        buf.push_back(0);
}

// ── MQRC record assembler ──────────────────────────────────────────────
struct MqrcRecord {
    int32_t              id = 0;
    std::vector<uint8_t> payload;
};

std::vector<uint8_t> assemble_mqrc(const MqrcRecord& rec) {
    std::vector<uint8_t> buf;
    // Magic
    buf.push_back('M');
    buf.push_back('Q');
    buf.push_back('R');
    buf.push_back('C');
    // unknown
    write_i32le(buf, 0);
    write_i32le(buf, rec.id);
    auto sz = static_cast<int32_t>(rec.payload.size());
    write_i32le(buf, sz);
    write_i32le(buf, sz); // realFileSize = size
    write_i32le(buf, 1);  // isNotDeleted
    write_i32le(buf, 0);  // recordMagic
    write_bytes(buf, rec.payload);
    return buf;
}

// ── Synthetic FF container writer ──────────────────────────────────────
struct SyntheticOptSpec {
    std::string index_entry_name = "TESTSPRITE";
    std::string anim_name = "TESTANIM";
    std::string physical_record_name = "BASE.PNG";
    int32_t     png_record_id = 200;
    int32_t     index_opt_id = 100;
    int32_t     images_opt_id = 101;
    int32_t     anims_opt_id = 102;
    uint8_t     png_r = 0xFF;
    uint8_t     png_g = 0x00;
    uint8_t     png_b = 0x00;
    int32_t     frame_output_w = 1;
    int32_t     frame_output_h = 1;
    int32_t     frame_pieces_count = 1;
};

void write_synthetic_opt_ff(const fs::path& path, const SyntheticOptSpec& s) {
    // Build payloads first so we know their sizes

    // INDEX payload: 1 image entry + 1 animation entry
    std::vector<uint8_t> index_payload;
    write_i32le(index_payload, 2); // framesCount

    // Image entry
    write_i32le(index_payload, s.png_record_id);
    write_cstr(index_payload, s.index_entry_name);
    write_i32le(index_payload, 0); // relatedOffset within IMAGES
    // IMAGES block size: compute later by assembling temporary
    // (we use fixed sizes, compute now)

    // Animation entry: id=-1, relatedOffset=0 into ANIMS
    write_i32le(index_payload, -1);
    write_cstr(index_payload, s.anim_name);
    write_i32le(index_payload, 0);
    // ANIMS block size: compute below

    // IMAGES payload: 1 block with 1 frame, 1 piece
    std::vector<uint8_t> images_payload;
    images_payload.push_back(0);      // transparentColorIndex
    write_u16le(images_payload, 255); // opacityAlgorithm (int16 LE)
    write_i32le(images_payload, 1);   // sizeX
    write_i32le(images_payload, 1);   // sizeY
    // palette: 256 × BGRA = 1024 bytes of zeros
    for (int i = 0; i < 256; ++i) {
        images_payload.push_back(0); // B
        images_payload.push_back(0); // G
        images_payload.push_back(0); // R
        images_payload.push_back(0); // A
    }
    write_i32le(images_payload, 1); // framesCount
    write_cstr(images_payload, s.index_entry_name);
    write_i32le(images_payload, s.frame_pieces_count); // piecesCount
    write_i32le(images_payload, s.frame_output_w);
    write_i32le(images_payload, s.frame_output_h);
    for (int32_t i = 0; i < s.frame_pieces_count; ++i) {
        // piece: outputX/Y baseX/Y w h
        write_i32le(images_payload, 0);
        write_i32le(images_payload, 0);
        write_i32le(images_payload, 0);
        write_i32le(images_payload, 0);
        write_i32le(images_payload, s.frame_output_w);
        write_i32le(images_payload, s.frame_output_h);
    }

    // ANIMS payload: 1 block
    std::vector<uint8_t> anims_payload;
    write_i32le(anims_payload, 1); // framesCount
    write_cstr(anims_payload, s.index_entry_name);

    // NOW compute the image-relatedOffset/size in INDEX entries
    auto images_block_size = static_cast<int32_t>(images_payload.size());
    auto anims_block_size = static_cast<int32_t>(anims_payload.size());

    // Rebuild INDEX with correct sizes
    std::vector<uint8_t> index_final;
    write_i32le(index_final, 2);
    write_i32le(index_final, s.png_record_id);
    write_cstr(index_final, s.index_entry_name);
    write_i32le(index_final, 0);
    write_i32le(index_final, images_block_size);

    write_i32le(index_final, -1);
    write_cstr(index_final, s.anim_name);
    write_i32le(index_final, 0);
    write_i32le(index_final, anims_block_size);

    // PNG payload
    auto png = make_1x1_png(s.png_r, s.png_g, s.png_b, 255);

    // MFF dummy record
    std::vector<uint8_t> mff_payload = {'M', 'F', 'F', 0, 0, 0, 0, 0};

    // Build name table
    std::vector<uint8_t> name_table;
    write_i32le(name_table, 6); // 6 named records
    write_fixed_str(name_table, s.index_entry_name, 256);
    write_i32le(name_table, s.index_opt_id);
    write_fixed_str(name_table, s.anim_name, 256);
    write_i32le(name_table, s.anims_opt_id);
    write_fixed_str(name_table, s.physical_record_name, 256);
    write_i32le(name_table, s.png_record_id);
    write_fixed_str(name_table, "-INDEX.OPT", 256);
    write_i32le(name_table, s.index_opt_id);
    write_fixed_str(name_table, "-IMAGES.OPT", 256);
    write_i32le(name_table, s.images_opt_id);
    write_fixed_str(name_table, "-ANIMS.OPT", 256);
    write_i32le(name_table, s.anims_opt_id);

    // Assemble records
    auto rec1 = assemble_mqrc({.id = 1, .payload = mff_payload});
    auto rec_nt = assemble_mqrc({.id = 2, .payload = name_table});
    auto rec_idx = assemble_mqrc({.id = s.index_opt_id, .payload = index_final});
    auto rec_img = assemble_mqrc({.id = s.images_opt_id, .payload = images_payload});
    auto rec_anm = assemble_mqrc({.id = s.anims_opt_id, .payload = anims_payload});
    auto rec_png = assemble_mqrc({.id = s.png_record_id, .payload = png});

    int32_t record_count = 6;

    // Write file
    std::vector<uint8_t> file;
    // MQDB header
    file.push_back('M');
    file.push_back('Q');
    file.push_back('D');
    file.push_back('B');
    write_i32le(file, 0); // unknown_0
    write_i32le(file, record_count);
    write_i32le(file, 0); // unknown_1
    write_i32le(file, 0);
    write_i32le(file, 0);
    write_i32le(file, 0); // checksum
    // Records
    write_bytes(file, rec1);
    write_bytes(file, rec_nt);
    write_bytes(file, rec_idx);
    write_bytes(file, rec_img);
    write_bytes(file, rec_anm);
    write_bytes(file, rec_png);

    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(file.data()),
                  static_cast<std::streamsize>(file.size()));
    }
}

// ── Test fixture: creates a temp game root with a synthetic OPT FF ─────
class FfAssetStoreInitTest : public ::testing::Test {
protected:
    fs::path temp_root_;
    fs::path ff_path_;

    void SetUp() override {
        static std::atomic<int> seq{0};
        temp_root_ = fs::temp_directory_path() /
                     ("opendis2_ff_init_test_" + std::to_string(test_support::process_id()) + "_" +
                      std::to_string(seq.fetch_add(1)));
        fs::create_directories(temp_root_ / "Imgs");
        ff_path_ = temp_root_ / "Imgs" / "Testopt.ff";
        write_synthetic_opt_ff(ff_path_, {});
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(temp_root_, ec);
    }
};

TEST_F(FfAssetStoreInitTest, InitializeContainerCompletes) {
    FfAssetStore store(temp_root_);
    auto         names = store.record_names("Imgs/Testopt.ff");
    EXPECT_FALSE(names.empty());
}

TEST_F(FfAssetStoreInitTest, AnimationsInListsAnimation) {
    FfAssetStore store(temp_root_);
    auto         anims = store.animations_in("Imgs/Testopt.ff");
    ASSERT_EQ(anims.size(), 1u);
    EXPECT_EQ(anims[0], "TESTANIM");
}

TEST_F(FfAssetStoreInitTest, AnimationMetadataReturnsFrames) {
    FfAssetStore store(temp_root_);
    auto         seq = store.animation_metadata("Imgs/Testopt.ff", "TESTANIM");
    EXPECT_EQ(seq.name, "TESTANIM");
    ASSERT_EQ(seq.frames.size(), 1u);
    EXPECT_EQ(seq.frames[0].image_name, "TESTSPRITE");
}

TEST_F(FfAssetStoreInitTest, ResolveSpriteFastReturnsComposedDependency) {
    FfAssetStore store(temp_root_);
    auto         dep = store.resolve_sprite_fast("Imgs/Testopt.ff", "TESTSPRITE");
    ASSERT_TRUE(dep.has_value());
    EXPECT_FALSE(dep->is_raw_png);
    EXPECT_EQ(dep->physical_record_name, "BASE.PNG");
    EXPECT_NE(dep->frame, nullptr);
    EXPECT_NE(dep->block, nullptr);
}

TEST_F(FfAssetStoreInitTest, ResolveSpriteFastPointersStableAfterAnimInit) {
    FfAssetStore store(temp_root_);

    auto dep1 = store.resolve_sprite_fast("Imgs/Testopt.ff", "TESTSPRITE");
    ASSERT_TRUE(dep1.has_value());

    // Trigger animation-map (ensure_anim_maps) — must not invalidate
    (void)store.animation_metadata("Imgs/Testopt.ff", "TESTANIM");

    auto dep2 = store.resolve_sprite_fast("Imgs/Testopt.ff", "TESTSPRITE");
    ASSERT_TRUE(dep2.has_value());
    EXPECT_EQ(dep2->frame, dep1->frame);
    EXPECT_EQ(dep2->block, dep1->block);

    // Verify pointers are into container_maps
    const auto* maps = store.container_maps("Imgs/Testopt.ff");
    ASSERT_NE(maps, nullptr);
    EXPECT_EQ(dep2->block, &maps->image_map.blocks[0]);
    EXPECT_EQ(dep2->frame, &maps->image_map.blocks[0].frames[0]);
}

TEST_F(FfAssetStoreInitTest, AnimationMetadataPreservesAuthoredEmptyFrameBounds) {
    const fs::path   empty_path = temp_root_ / "Imgs" / "Empty.ff";
    SyntheticOptSpec spec;
    spec.index_entry_name = "EMPTYSPRITE";
    spec.anim_name = "EMPTYANIM";
    spec.physical_record_name = "EMPTY.PNG";
    spec.frame_output_w = 800;
    spec.frame_output_h = 600;
    spec.frame_pieces_count = 0;
    write_synthetic_opt_ff(empty_path, spec);

    FfAssetStore store(temp_root_);
    const auto   seq = store.animation_metadata("Imgs/Empty.ff", "EMPTYANIM");
    ASSERT_EQ(seq.frames.size(), 1u);
    EXPECT_EQ(seq.frames[0].image_name, "EMPTYSPRITE");
    EXPECT_FALSE(seq.frames[0].content_bounds.valid());
    EXPECT_EQ(seq.native_canvas_w, 800);
    EXPECT_EQ(seq.native_canvas_h, 600);
    EXPECT_THROW(static_cast<void>(store.sprite_metadata("Imgs/Empty.ff", "EMPTYSPRITE")),
                 std::runtime_error);
}

TEST_F(FfAssetStoreInitTest, DecodeAnimationPreservesAuthoredEmptyFrameBounds) {
    const fs::path   empty_path = temp_root_ / "Imgs" / "EmptyDecode.ff";
    SyntheticOptSpec spec;
    spec.index_entry_name = "EMPTYSPRITE";
    spec.anim_name = "EMPTYANIM";
    spec.physical_record_name = "EMPTY.PNG";
    spec.frame_output_w = 800;
    spec.frame_output_h = 600;
    spec.frame_pieces_count = 0;
    write_synthetic_opt_ff(empty_path, spec);

    FfAssetStore store(temp_root_);
    const auto   seq = store.decode_animation("Imgs/EmptyDecode.ff", "EMPTYANIM");
    ASSERT_EQ(seq.frames.size(), 1u);
    EXPECT_EQ(seq.frames[0].image_name, "EMPTYSPRITE");
    EXPECT_FALSE(seq.frames[0].content_bounds.valid());
    EXPECT_EQ(seq.native_canvas_w, 800);
    EXPECT_EQ(seq.native_canvas_h, 600);
}

TEST_F(FfAssetStoreInitTest, ContainerAwarePhysicalIdentityPreventsCollision) {
    // Build two containers with same record name but different PNG pixels.
    const fs::path alpha_path = temp_root_ / "Imgs" / "Alpha.ff";
    const fs::path beta_path = temp_root_ / "Imgs" / "Beta.ff";

    // Alpha: red pixels
    {
        SyntheticOptSpec spec;
        spec.index_entry_name = "SPRITE_A";
        spec.anim_name = "ANIM_A";
        spec.physical_record_name = "BASE.PNG";
        spec.png_record_id = 200;
        spec.index_opt_id = 100;
        spec.images_opt_id = 101;
        spec.anims_opt_id = 102;
        spec.png_r = 0xFF;
        spec.png_g = 0x00;
        spec.png_b = 0x00;
        write_synthetic_opt_ff(alpha_path, spec);
    }

    // Beta: green pixels, same record name "BASE.PNG"
    {
        SyntheticOptSpec spec;
        spec.index_entry_name = "SPRITE_B";
        spec.anim_name = "ANIM_B";
        spec.physical_record_name = "BASE.PNG";
        spec.png_record_id = 200;
        spec.index_opt_id = 100;
        spec.images_opt_id = 101;
        spec.anims_opt_id = 102;
        spec.png_r = 0x00;
        spec.png_g = 0xFF;
        spec.png_b = 0x00;
        write_synthetic_opt_ff(beta_path, spec);
    }

    FfAssetStore store(temp_root_);

    // Verify each resolve gives the right physical name and container
    auto dep_a = store.resolve_sprite_fast("Imgs/Alpha.ff", "SPRITE_A");
    ASSERT_TRUE(dep_a.has_value());
    EXPECT_EQ(dep_a->physical_record_name, "BASE.PNG");

    auto dep_b = store.resolve_sprite_fast("Imgs/Beta.ff", "SPRITE_B");
    ASSERT_TRUE(dep_b.has_value());
    EXPECT_EQ(dep_b->physical_record_name, "BASE.PNG");

    // Through preloader: composed results must differ
    AssetRuntime            runtime(temp_root_, 2);
    AnimationAssetPreloader preloader(runtime);

    std::vector<ImageAssetKey> keys;
    keys.push_back(ImageAssetKey{.container_path = "Imgs/Alpha.ff",
                                 .image_name = "SPRITE_A",
                                 .kind = ImageAssetKind::ComposedSprite});
    keys.push_back(ImageAssetKey{.container_path = "Imgs/Beta.ff",
                                 .image_name = "SPRITE_B",
                                 .kind = ImageAssetKind::ComposedSprite});

    auto results = preloader.prepare_sprites(keys);
    ASSERT_EQ(results.size(), 2u);

    // Parallel compose — results appear in completion order, not input order.
    const PreparedImageResult* result_a = nullptr;
    const PreparedImageResult* result_b = nullptr;
    for (const auto& r : results) {
        if (r.key.image_name == "SPRITE_A") {
            result_a = &r;
        } else if (r.key.image_name == "SPRITE_B") {
            result_b = &r;
        }
    }
    ASSERT_NE(result_a, nullptr);
    ASSERT_NE(result_b, nullptr);
    ASSERT_TRUE(result_a->success) << result_a->error;
    ASSERT_TRUE(result_b->success) << result_b->error;
    ASSERT_NE(result_a->image, nullptr);
    ASSERT_NE(result_b->image, nullptr);
    ASSERT_NE(result_a->image->pixels, nullptr);
    ASSERT_NE(result_b->image->pixels, nullptr);

    // Alpha must be red (R=255, G=0), Beta must be green (R=0, G=255)
    const auto& px_a = *result_a->image->pixels;
    const auto& px_b = *result_b->image->pixels;
    ASSERT_EQ(px_a.width, 1u);
    ASSERT_EQ(px_a.height, 1u);
    ASSERT_EQ(px_b.width, 1u);
    ASSERT_EQ(px_b.height, 1u);

    EXPECT_EQ(px_a.rgba[0], 255) << "Alpha R";
    EXPECT_EQ(px_a.rgba[1], 0) << "Alpha G";
}

} // namespace
} // namespace d2engine
