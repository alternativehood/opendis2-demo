#include "terrain_calibration_exporter.hpp"

#include <opendis2_terrain_preview/terrain_preview_image.hpp>

#include <d2buildinfo/build_info.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2adventure_render/terrain/terrain_asset_catalog.hpp>
#include <d2res/platform/hash.hpp>
#include <d2res/rgba_buffer.hpp>
#include <d2runtime/AdventureTerrainDecoder.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2scenario/SgParser.hpp>

#include <lodepng.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace d2terrain_calibration {
namespace {

using json = nlohmann::json;

constexpr int                             kMapSize = 48;
constexpr std::array<std::string_view, 4> kApexOrder = {"apex_top", "apex_right", "apex_bottom",
                                                        "apex_left"};

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("cannot read file: " + path.string());
    }
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return data;
}

void copy_file_exact(const std::filesystem::path& src, const std::filesystem::path& dst) {
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
}

std::string hex_u32(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

std::string bool_text(bool value) {
    return value ? "true" : "false";
}

std::string border_kind_name(const std::optional<d2runtime::AdventureTerrainBorderDescriptor>& b) {
    return d2runtime::terrain_border_kind_name(b ? b->kind
                                                 : d2runtime::AdventureTerrainBorderKind::None);
}

bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

std::string material(const d2runtime::AdventureTerrainTileDescriptor& tile) {
    return tile.terrain_code;
}

const d2runtime::AdventureTerrainTileDescriptor&
display_tile(const d2engine::AdventureTerrainSurfaceInput& input, int display_x, int display_y) {
    return input.descriptors[static_cast<std::size_t>(display_y) *
                                 static_cast<std::size_t>(input.map_width) +
                             static_cast<std::size_t>(display_x)];
}

void write_png(const std::filesystem::path& path, const std::vector<std::uint8_t>& rgba, int width,
               int height) {
    std::vector<std::uint8_t> png;
    const auto                err =
        lodepng::encode(png, rgba, static_cast<unsigned>(width), static_cast<unsigned>(height));
    if (err != 0) {
        throw std::runtime_error(std::string("PNG encode failed: ") + lodepng_error_text(err));
    }
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
}

void write_preview_png(const std::filesystem::path&           path,
                       const d2terrain_preview::PreviewImage& image) {
    write_png(path, image.rgba, image.output_width, image.output_height);
}

void write_candidate_sheet(const std::filesystem::path& path, const d2engine::FfAssetStore& store,
                           const std::string& family, const std::string& current_record) {
    constexpr int             width = 64 * 4;
    constexpr int             height = 32 * 30;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4U, 245);
    int                       row = 0;
    for (int shape : candidate_shapes()) {
        char record[32] = {};
        std::snprintf(record, sizeof(record), "%s_%02d_00.PNG", family.c_str(), shape);
        const auto mask_buf = store.copy_raw_png("Imgs/GrBorder.ff", record);
        const auto mask = mask_buf.has_value() ? *mask_buf : d2res::RgbaBuffer{};
        for (int t = 0; t < 4; ++t) {
            const bool current = current_record == record;
            for (int y = 0; y < 32; ++y) {
                for (int x = 0; x < 64; ++x) {
                    int mx = x;
                    int my = y;
                    if (t == 1 || t == 3) {
                        mx = 63 - x;
                    }
                    if (t == 2 || t == 3) {
                        my = 31 - y;
                    }
                    const auto i = (static_cast<std::size_t>(row * 32 + y) * width +
                                    static_cast<std::size_t>(t * 64 + x)) *
                                   4U;
                    const auto src = mask.rgba.empty()
                                         ? 0U
                                         : (static_cast<std::size_t>(my) * mask.width +
                                            static_cast<std::size_t>(mx)) *
                                               4U;
                    const auto value = mask.rgba.empty() ? std::uint8_t{220} : mask.rgba[src];
                    rgba[i] = current && x < 4 ? 255 : value;
                    rgba[i + 1U] = current && x < 4 ? 0 : value;
                    rgba[i + 2U] = current && x < 4 ? 0 : value;
                    rgba[i + 3U] = 255;
                }
            }
        }
        ++row;
    }
    write_png(path, rgba, width, height);
}

void blit_rgba(std::vector<std::uint8_t>& dst, int dst_width, int dst_height, int ox, int oy,
               const d2res::RgbaBuffer& src) {
    for (std::uint32_t y = 0; y < src.height; ++y) {
        for (std::uint32_t x = 0; x < src.width; ++x) {
            const int dx = ox + static_cast<int>(x);
            const int dy = oy + static_cast<int>(y);
            if (dx < 0 || dy < 0 || dx >= dst_width || dy >= dst_height) {
                continue;
            }
            const auto si = (static_cast<std::size_t>(y) * src.width + x) * 4U;
            const auto di = (static_cast<std::size_t>(dy) * static_cast<std::size_t>(dst_width) +
                             static_cast<std::size_t>(dx)) *
                            4U;
            std::copy_n(src.rgba.data() + si, 4, dst.data() + di);
        }
    }
}

void write_ground_materials_atlas(const std::filesystem::path&  path,
                                  const d2engine::FfAssetStore& store) {
    static constexpr std::array records = {"HU_00.PNG", "NE_00.PNG", "WA_00.PNG", "BL_00.PNG"};
    constexpr int               width = 64 * static_cast<int>(records.size());
    constexpr int               height = 32;
    std::vector<std::uint8_t>   rgba(static_cast<std::size_t>(width) * height * 4U, 255);
    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto image_buf = store.copy_raw_png("Imgs/Ground.ff", records[i]);
        const auto image = image_buf.has_value() ? *image_buf : d2res::RgbaBuffer{};
        if (!image.rgba.empty()) {
            blit_rgba(rgba, width, height, static_cast<int>(i) * 64, 0, image);
        }
    }
    write_png(path, rgba, width, height);
}

std::string current_build_id() {
    return d2buildinfo::build_timestamp();
}

void add_hash(json& hashes, const std::filesystem::path& base, const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) {
        return;
    }
    hashes[std::filesystem::relative(path, base).generic_string()] =
        d2res::platform::sha256_file(path.string());
}

void collect_hashes(json& hashes, const std::filesystem::path& base,
                    const std::filesystem::path& root) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        add_hash(hashes, base, entry.path());
    }
}

json apex_definition_json(ApexDisplayOrigin origin) {
    json result = json::object();
    for (const auto& coord : apex_coords(origin)) {
        result[coord.name] = {{"display", {coord.display_x, coord.display_y}},
                              {"source", {coord.source_x, coord.source_y}}};
    }
    return result;
}

json expected_patterns_json() {
    json result = json::array();
    for (const auto& pattern : pattern_manifest()) {
        json materials = json::object();
        for (std::size_t i = 0; i < kApexOrder.size(); ++i) {
            materials[std::string(kApexOrder[i])] = pattern.expected_materials[i];
        }
        result.push_back({{"stem", pattern.stem},
                          {"dataset_group", pattern.dataset_group},
                          {"pattern_index", pattern.pattern_index},
                          {"materials", materials}});
    }
    return result;
}

void write_json(const std::filesystem::path& path, const json& value) {
    std::ofstream out(path);
    out << value.dump(2) << '\n';
}

void write_apex_cells_csv(std::ostream& out, const PatternEntry& pattern,
                          const d2runtime::AdventureTerrainGrid&           terrain,
                          const d2engine::AdventureTerrainSurfaceInput&    input,
                          const d2engine::AdventureTerrainSurfaceComposer& composer,
                          ApexDisplayOrigin                                origin) {
    const auto coords = apex_coords(origin);
    for (std::size_t i = 0; i < coords.size(); ++i) {
        const auto& coord = coords[i];
        const auto& tile = display_tile(input, coord.display_x, coord.display_y);
        const auto  info = composer.describe_tile(input, coord.display_x, coord.display_y);
        out << pattern.stem << ',' << pattern.dataset_group << ',' << pattern.pattern_index << ','
            << coord.name << ',' << coord.display_x << ',' << coord.display_y << ','
            << coord.source_x << ',' << coord.source_y << ',' << pattern.expected_materials[i]
            << ',' << material(tile) << ',' << tile.terrain_code << ',' << tile.raw_value << ','
            << hex_u32(tile.raw_value) << ',' << static_cast<int>(tile.raw.low_byte) << ','
            << static_cast<int>(tile.raw.family_id) << ','
            << static_cast<int>(tile.raw.variant_bits) << ','
            << static_cast<int>(tile.raw.terrain_flags) << ','
            << static_cast<int>(tile.raw.border_shape) << ',' << border_kind_name(tile.border)
            << ',' << tile.expected_ground_asset.record_name << ','
            << bool_text(input
                             .resolved_tiles[static_cast<std::size_t>(coord.display_y) *
                                                 static_cast<std::size_t>(input.map_width) +
                                             static_cast<std::size_t>(coord.display_x)]
                             .ground_asset_found)
            << ',' << static_cast<int>(info.logical_border_shape) << ','
            << info.composer_border_family << ',' << static_cast<int>(info.resolved_record_shape)
            << ',' << info.resolved_border_record << ',' << info.border_shape_source << '\n';
        (void)terrain;
    }
}

struct EdgeSpec {
    std::string name;
    std::string a;
    std::string b;
    int         ax = 0;
    int         ay = 0;
    int         bx = 0;
    int         by = 0;
};

const std::array<EdgeSpec, 8>& edge_specs() {
    static const std::array<EdgeSpec, 8> specs = {
        EdgeSpec{"top-right", "apex_top", "apex_right", 0, 0, 1, 0},
        EdgeSpec{"right-bottom", "apex_right", "apex_bottom", 1, 0, 1, 1},
        EdgeSpec{"bottom-left", "apex_bottom", "apex_left", 1, 1, 0, 1},
        EdgeSpec{"left-top", "apex_left", "apex_top", 0, 1, 0, 0},
        EdgeSpec{"top-outside", "apex_top", "outside", 0, 0, 0, -1},
        EdgeSpec{"right-outside", "apex_right", "outside", 1, 0, 2, 0},
        EdgeSpec{"bottom-outside", "apex_bottom", "outside", 1, 1, 1, 2},
        EdgeSpec{"left-outside", "apex_left", "outside", 0, 1, -1, 1},
    };
    return specs;
}

void write_apex_edges_csv(std::ostream& out, const PatternEntry& pattern,
                          const d2runtime::AdventureTerrainGrid&           terrain,
                          const d2engine::AdventureTerrainSurfaceInput&    input,
                          const d2engine::AdventureTerrainSurfaceComposer& composer,
                          ApexDisplayOrigin                                origin) {
    for (const auto& edge : edge_specs()) {
        const int   ax = origin.x + edge.ax;
        const int   ay = origin.y + edge.ay;
        const int   bx = origin.x + edge.bx;
        const int   by = origin.y + edge.by;
        const bool  a_ok = ax >= 0 && ay >= 0 && ax < input.map_width && ay < input.map_height;
        const bool  b_ok = bx >= 0 && by >= 0 && bx < input.map_width && by < input.map_height;
        const auto& a = display_tile(input, ax, ay);
        const auto  info = composer.describe_tile(input, ax, ay);
        const auto* b = b_ok ? &display_tile(input, bx, by) : nullptr;
        const int   asx = ay;
        const int   asy = ax;
        const int   bsx = by;
        const int   bsy = bx;
        out << pattern.stem << ',' << pattern.dataset_group << ',' << pattern.pattern_index << ','
            << edge.name << ',' << edge.a << ',' << edge.b << ',' << ax << ',' << ay << ',' << bx
            << ',' << by << ',' << asx << ',' << asy << ',' << bsx << ',' << bsy << ','
            << material(a) << ',' << (b == nullptr ? "out_of_bounds" : material(*b)) << ','
            << bool_text(b != nullptr && material(a) != material(*b)) << ',' << a.raw_value << ','
            << (b == nullptr ? 0U : b->raw_value) << ',' << static_cast<int>(a.raw.low_byte) << ','
            << (b == nullptr ? 0 : static_cast<int>(b->raw.low_byte)) << ','
            << static_cast<int>(a.raw.border_shape) << ','
            << (b == nullptr ? 0 : static_cast<int>(b->raw.border_shape)) << ",composer,"
            << static_cast<int>(info.logical_border_shape) << ',' << info.composer_border_family
            << ',' << static_cast<int>(info.resolved_record_shape) << ','
            << info.resolved_border_record << ',' << "native" << ',' << info.border_shape_source
            << '\n';
        (void)a_ok;
        (void)terrain;
    }
}

void write_apex_context_csv(std::ostream& out, const PatternEntry& pattern,
                            const d2engine::AdventureTerrainSurfaceInput& input,
                            ApexDisplayOrigin                             origin) {
    static constexpr std::array dirs = {
        std::pair{"north", std::pair{0, -1}},      std::pair{"east", std::pair{1, 0}},
        std::pair{"south", std::pair{0, 1}},       std::pair{"west", std::pair{-1, 0}},
        std::pair{"north_east", std::pair{1, -1}}, std::pair{"south_east", std::pair{1, 1}},
        std::pair{"south_west", std::pair{-1, 1}}, std::pair{"north_west", std::pair{-1, -1}},
    };
    for (const auto& coord : apex_coords(origin)) {
        for (const auto& [name, delta] : dirs) {
            const int  nx = coord.display_x + delta.first;
            const int  ny = coord.display_y + delta.second;
            const bool ok = nx >= 0 && ny >= 0 && nx < input.map_width && ny < input.map_height;
            out << pattern.stem << ',' << coord.name << ',' << name << ',' << nx << ',' << ny << ','
                << ny << ',' << nx << ',';
            if (ok) {
                const auto& tile = display_tile(input, nx, ny);
                out << material(tile) << ',' << tile.raw_value << ','
                    << static_cast<int>(tile.raw.low_byte) << ",false\n";
            } else {
                out << "out_of_bounds,0,0,true\n";
            }
        }
    }
}

void write_csv_headers(std::ofstream& cells, std::ofstream& edges) {
    cells << "scenario,dataset_group,pattern_index,apex_position,display_x,display_y,source_x,"
             "source_y,expected_material,actual_material,terrain_code,raw_value_dec,"
             "raw_value_hex,low_byte,family_id,variant_bits,terrain_flags,raw_border_shape,"
             "raw_border_kind,expected_ground_record,ground_asset_found,current_logical_shape,"
             "current_record_family,current_record_shape,current_record_name,"
             "current_mapping_source\n";
    edges << "scenario,dataset_group,pattern_index,edge_name,position_a,position_b,display_x_a,"
             "display_y_a,display_x_b,display_y_b,source_x_a,source_y_a,source_x_b,source_y_b,"
             "material_a,material_b,materials_differ,raw_value_a,raw_value_b,low_byte_a,"
             "low_byte_b,raw_border_shape_a,raw_border_shape_b,current_composer_owner,"
             "current_logical_shape,current_record_family,current_record_shape,current_record_name,"
             "current_mapping_source\n";
}

void print_validation_failure(std::ostream& out, const ValidationFailure& f) {
    out << f.scenario_filename << '\n'
        << f.location << '\n'
        << "expected material: " << f.expected_material << '\n'
        << "actual material: " << f.actual_material << '\n'
        << "raw value: " << f.raw_value << '\n'
        << "low byte: " << static_cast<int>(f.low_byte) << '\n';
}

} // namespace

std::array<ApexCoord, 4> apex_coords(ApexDisplayOrigin origin) {
    const auto make = [](std::string name, int display_x, int display_y) {
        return ApexCoord{std::move(name), display_x, display_y, display_y, display_x};
    };
    return {
        make("apex_top", origin.x, origin.y),
        make("apex_right", origin.x + 1, origin.y),
        make("apex_bottom", origin.x + 1, origin.y + 1),
        make("apex_left", origin.x, origin.y + 1),
    };
}

const std::vector<PatternEntry>& pattern_manifest() {
    static const std::vector<PatternEntry> patterns = {
        {"control_apex_all_hu", "control", 0, {"HU", "HU", "HU", "HU"}},
        {"land_apex_01_top_ne", "land", 1, {"NE", "HU", "HU", "HU"}},
        {"land_apex_02_right_ne", "land", 2, {"HU", "NE", "HU", "HU"}},
        {"land_apex_03_top_right_ne", "land", 3, {"NE", "NE", "HU", "HU"}},
        {"land_apex_04_bottom_ne", "land", 4, {"HU", "HU", "NE", "HU"}},
        {"land_apex_05_top_bottom_ne", "land", 5, {"NE", "HU", "NE", "HU"}},
        {"land_apex_06_right_bottom_ne", "land", 6, {"HU", "NE", "NE", "HU"}},
        {"land_apex_07_top_right_bottom_ne", "land", 7, {"NE", "NE", "NE", "HU"}},
        {"land_apex_08_left_ne", "land", 8, {"HU", "HU", "HU", "NE"}},
        {"land_apex_09_top_left_ne", "land", 9, {"NE", "HU", "HU", "NE"}},
        {"land_apex_10_right_left_ne", "land", 10, {"HU", "NE", "HU", "NE"}},
        {"land_apex_11_top_right_left_ne", "land", 11, {"NE", "NE", "HU", "NE"}},
        {"land_apex_12_bottom_left_ne", "land", 12, {"HU", "HU", "NE", "NE"}},
        {"land_apex_13_top_bottom_left_ne", "land", 13, {"NE", "HU", "NE", "NE"}},
        {"land_apex_14_right_bottom_left_ne", "land", 14, {"HU", "NE", "NE", "NE"}},
        {"water_apex_01_top_wa", "water", 1, {"WA", "HU", "HU", "HU"}},
        {"water_apex_02_right_wa", "water", 2, {"HU", "WA", "HU", "HU"}},
        {"water_apex_03_top_right_wa", "water", 3, {"WA", "WA", "HU", "HU"}},
        {"water_apex_04_bottom_wa", "water", 4, {"HU", "HU", "WA", "HU"}},
        {"water_apex_05_top_bottom_wa", "water", 5, {"WA", "HU", "WA", "HU"}},
        {"water_apex_06_right_bottom_wa", "water", 6, {"HU", "WA", "WA", "HU"}},
        {"water_apex_07_top_right_bottom_wa", "water", 7, {"WA", "WA", "WA", "HU"}},
        {"water_apex_08_left_wa", "water", 8, {"HU", "HU", "HU", "WA"}},
        {"water_apex_09_top_left_wa", "water", 9, {"WA", "HU", "HU", "WA"}},
        {"water_apex_10_right_left_wa", "water", 10, {"HU", "WA", "HU", "WA"}},
        {"water_apex_11_top_right_left_wa", "water", 11, {"WA", "WA", "HU", "WA"}},
        {"water_apex_12_bottom_left_wa", "water", 12, {"HU", "HU", "WA", "WA"}},
        {"water_apex_13_top_bottom_left_wa", "water", 13, {"WA", "HU", "WA", "WA"}},
        {"water_apex_14_right_bottom_left_wa", "water", 14, {"HU", "WA", "WA", "WA"}},
    };
    return patterns;
}

std::vector<std::string> expected_sg_filenames() {
    std::vector<std::string> result;
    for (const auto& pattern : pattern_manifest()) {
        result.push_back(pattern.stem + ".sg");
    }
    return result;
}

std::vector<std::string> expected_editor_png_filenames() {
    std::vector<std::string> result;
    for (const auto& pattern : pattern_manifest()) {
        result.push_back(pattern.stem + "_editor.png");
    }
    return result;
}

std::vector<std::string> expected_dataset_filenames() {
    auto result = expected_sg_filenames();
    auto pngs = expected_editor_png_filenames();
    result.insert(result.end(), pngs.begin(), pngs.end());
    return result;
}

std::vector<int> candidate_shapes() {
    std::vector<int> result;
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape != 16) {
            result.push_back(shape);
        }
    }
    return result;
}

DatasetPreflightResult preflight_dataset(const std::filesystem::path& maps_dir) {
    DatasetPreflightResult      result;
    const auto                  expected = expected_dataset_filenames();
    const std::set<std::string> expected_set(expected.begin(), expected.end());
    for (const auto& filename : expected) {
        if (!std::filesystem::is_regular_file(maps_dir / filename)) {
            result.missing.push_back(filename);
        }
    }
    if (std::filesystem::is_directory(maps_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(maps_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto name = entry.path().filename().string();
            const bool dataset_kind =
                entry.path().extension() == ".sg" || ends_with(name, "_editor.png");
            if (dataset_kind && !expected_set.contains(name)) {
                result.unexpected.push_back(name);
            }
        }
    }
    std::ranges::sort(result.missing);
    std::ranges::sort(result.unexpected);
    result.ok = result.missing.empty() && result.unexpected.empty();
    return result;
}

ValidationResult
validate_apex_terrain(std::string_view scenario_filename, const PatternEntry& pattern,
                      const d2runtime::AdventureTerrainGrid&                        terrain,
                      const std::vector<d2runtime::AdventureTerrainTileDescriptor>& descriptors,
                      ApexDisplayOrigin                                             origin) {
    ValidationResult result;
    const auto       fail = [&](std::string location, std::string expected, std::string actual,
                                std::uint32_t raw, std::uint8_t low) {
        result.ok = false;
        result.failures.push_back({std::string(scenario_filename), std::move(location),
                                   std::move(expected), std::move(actual), raw, low});
    };
    if (terrain.width != kMapSize || terrain.height != kMapSize) {
        fail("terrain dimensions", "48x48",
             std::to_string(terrain.width) + "x" + std::to_string(terrain.height), 0, 0);
        return result;
    }
    if (descriptors.size() != static_cast<std::size_t>(kMapSize * kMapSize)) {
        fail("terrain descriptor count", std::to_string(kMapSize * kMapSize),
             std::to_string(descriptors.size()), 0, 0);
        return result;
    }
    std::set<std::pair<int, int>> apex_sources;
    const auto                    coords = apex_coords(origin);
    for (std::size_t i = 0; i < coords.size(); ++i) {
        const auto& coord = coords[i];
        if (coord.source_x < 0 || coord.source_y < 0 || coord.source_x >= terrain.width ||
            coord.source_y >= terrain.height) {
            fail(coord.name, "inside 48x48",
                 "source(" + std::to_string(coord.source_x) + "," + std::to_string(coord.source_y) +
                     ")",
                 0, 0);
            continue;
        }
        apex_sources.emplace(coord.source_x, coord.source_y);
        const auto index =
            static_cast<std::size_t>(coord.source_y) * static_cast<std::size_t>(terrain.width) +
            static_cast<std::size_t>(coord.source_x);
        const auto& tile = descriptors[index];
        if (material(tile) != pattern.expected_materials[i]) {
            fail(coord.name, pattern.expected_materials[i], material(tile), tile.raw_value,
                 tile.raw.low_byte);
        }
    }
    for (int y = 0; y < terrain.height; ++y) {
        for (int x = 0; x < terrain.width; ++x) {
            if (apex_sources.contains({x, y})) {
                continue;
            }
            const auto& tile =
                descriptors[static_cast<std::size_t>(y) * static_cast<std::size_t>(terrain.width) +
                            static_cast<std::size_t>(x)];
            if (material(tile) != "HU") {
                fail("source(" + std::to_string(x) + "," + std::to_string(y) + ")", "HU",
                     material(tile), tile.raw_value, tile.raw.low_byte);
            }
        }
    }
    return result;
}

int run_export(const ExportConfig& raw_config, std::ostream& diagnostics) {
    ExportConfig config = raw_config;
    if (config.out_dir.empty()) {
        config.out_dir = config.maps_dir / "research_export";
    }
    const auto preflight = preflight_dataset(config.maps_dir);
    if (!preflight.ok) {
        diagnostics << "terrain calibration dataset is incomplete\n";
        for (const auto& name : preflight.missing) {
            diagnostics << name << '\n';
        }
        for (const auto& name : preflight.unexpected) {
            diagnostics << name << '\n';
        }
        diagnostics << "complete expected filename list\n";
        for (const auto& name : expected_dataset_filenames()) {
            diagnostics << name << '\n';
        }
        return EXIT_FAILURE;
    }

    d2engine::FfAssetStore store(config.game_root.string());
    const auto             catalog = d2engine::TerrainAssetCatalogBuilder{}.build(store);
    const d2engine::AdventureTerrainAssetResolver   resolver(catalog);
    const d2engine::AdventureTerrainSurfaceComposer composer(store, catalog);
    const d2runtime::AdventureTerrainDecoder        tile_decoder;
    const d2runtime::AdventureTerrainMapDecoder     map_decoder(tile_decoder);

    struct ScenarioData {
        PatternEntry                                           pattern;
        d2runtime::AdventureTerrainGrid                        terrain;
        std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors;
        d2engine::AdventureTerrainSurfaceInput                 source_input;
    };
    std::vector<ScenarioData> scenarios;
    scenarios.reserve(pattern_manifest().size());

    for (const auto& pattern : pattern_manifest()) {
        const auto           sg_path = config.maps_dir / (pattern.stem + ".sg");
        const auto           bytes = read_file(sg_path);
        d2scenario::SgParser parser(bytes);
        auto                 parsed = parser.parse();
        auto                 built = d2runtime::AdventureWorldBuilder{}.build(parsed.scenario);
        auto                 descriptors = map_decoder.decode_grid(built.world.terrain);
        auto                 validation =
            validate_apex_terrain(sg_path.filename().string(), pattern, built.world.terrain,
                                  descriptors, config.apex_display_origin);
        if (!validation.ok) {
            for (const auto& failure : validation.failures) {
                print_validation_failure(diagnostics, failure);
            }
            return EXIT_FAILURE;
        }
        auto                                   resolved = resolver.resolve_all(descriptors);
        d2engine::AdventureTerrainSurfaceInput source_input{.map_width = built.world.map_width,
                                                            .map_height = built.world.map_height,
                                                            .descriptors = descriptors,
                                                            .resolved_tiles = resolved};
        scenarios.push_back(
            {pattern, built.world.terrain, std::move(descriptors), std::move(source_input)});
    }

    std::filesystem::create_directories(config.out_dir / "atlas");
    std::filesystem::create_directories(config.out_dir / "maps");
    write_ground_materials_atlas(config.out_dir / "atlas" / "ground_materials.png", store);
    write_candidate_sheet(config.out_dir / "atlas" / "grborder_ne_masks.png", store, "NE", "");
    write_candidate_sheet(config.out_dir / "atlas" / "grborder_wa_masks.png", store, "WA", "");

    std::ofstream combined_cells(config.out_dir / "combined_apex_cells.csv");
    std::ofstream combined_edges(config.out_dir / "combined_apex_edges.csv");
    write_csv_headers(combined_cells, combined_edges);

    json actual_patterns = json::array();
    json generated = json::array();
    for (const auto& scenario : scenarios) {
        const auto map_dir = config.out_dir / "maps" / scenario.pattern.stem;
        std::filesystem::create_directories(map_dir / "candidates");
        copy_file_exact(config.maps_dir / (scenario.pattern.stem + ".sg"), map_dir / "source.sg");
        copy_file_exact(config.maps_dir / (scenario.pattern.stem + "_editor.png"),
                        map_dir / "editor.png");

        std::ofstream cells(map_dir / "apex_cells.csv");
        std::ofstream edges(map_dir / "apex_edges.csv");
        write_csv_headers(cells, edges);
        write_apex_cells_csv(cells, scenario.pattern, scenario.terrain, scenario.source_input,
                             composer, config.apex_display_origin);
        write_apex_edges_csv(edges, scenario.pattern, scenario.terrain, scenario.source_input,
                             composer, config.apex_display_origin);
        write_apex_cells_csv(combined_cells, scenario.pattern, scenario.terrain,
                             scenario.source_input, composer, config.apex_display_origin);
        write_apex_edges_csv(combined_edges, scenario.pattern, scenario.terrain,
                             scenario.source_input, composer, config.apex_display_origin);

        std::ofstream context(map_dir / "apex_context.csv");
        context << "scenario,apex_position,neighbour_direction,neighbour_display_x,"
                   "neighbour_display_y,neighbour_source_x,neighbour_source_y,neighbour_material,"
                   "neighbour_raw_value,neighbour_low_byte,out_of_bounds\n";
        write_apex_context_csv(context, scenario.pattern, scenario.source_input,
                               config.apex_display_origin);

        const d2terrain_preview::PreviewRenderOptions preview_base{
            .scale = 1,
            .max_size = 1024,
        };
        write_preview_png(
            map_dir / "apex_preview_assets.png",
            d2terrain_preview::render_preview_image(scenario.terrain.width, scenario.terrain.height,
                                                    scenario.source_input, composer,
                                                    {.mode = d2terrain_preview::PreviewMode::Assets,
                                                     .scale = preview_base.scale,
                                                     .max_size = preview_base.max_size}));
        write_preview_png(map_dir / "apex_preview_family.png",
                          d2terrain_preview::render_preview_image(
                              scenario.terrain.width, scenario.terrain.height,
                              scenario.source_input, composer,
                              {.mode = d2terrain_preview::PreviewMode::FamilyId,
                               .scale = preview_base.scale,
                               .max_size = preview_base.max_size}));
        write_preview_png(map_dir / "apex_preview_low_byte.png",
                          d2terrain_preview::render_preview_image(
                              scenario.terrain.width, scenario.terrain.height,
                              scenario.source_input, composer,
                              {.mode = d2terrain_preview::PreviewMode::LowByte,
                               .scale = preview_base.scale,
                               .max_size = preview_base.max_size}));
        write_preview_png(map_dir / "apex_preview_border_shape.png",
                          d2terrain_preview::render_preview_image(
                              scenario.terrain.width, scenario.terrain.height,
                              scenario.source_input, composer,
                              {.mode = d2terrain_preview::PreviewMode::BorderShape,
                               .scale = preview_base.scale,
                               .max_size = preview_base.max_size}));

        json decoded = json::object();
        for (const auto& coord : apex_coords(config.apex_display_origin)) {
            decoded[coord.name] =
                material(display_tile(scenario.source_input, coord.display_x, coord.display_y));
        }
        actual_patterns.push_back({{"stem", scenario.pattern.stem}, {"materials", decoded}});
        write_json(map_dir / "validation.json",
                   {{"scenario", scenario.pattern.stem}, {"ok", true}, {"apex", decoded}});

        static constexpr std::array neighbour_deltas = {std::pair{0, -1}, std::pair{1, 0},
                                                        std::pair{0, 1}, std::pair{-1, 0}};
        for (const auto& coord : apex_coords(config.apex_display_origin)) {
            const auto& tile =
                display_tile(scenario.source_input, coord.display_x, coord.display_y);
            const auto info =
                composer.describe_tile(scenario.source_input, coord.display_x, coord.display_y);
            std::set<std::string> emitted;
            for (const auto& [dx, dy] : neighbour_deltas) {
                const int nx = coord.display_x + dx;
                const int ny = coord.display_y + dy;
                if (nx < 0 || ny < 0 || nx >= scenario.source_input.map_width ||
                    ny >= scenario.source_input.map_height) {
                    continue;
                }
                const auto& other = display_tile(scenario.source_input, nx, ny);
                if (tile.terrain_code == other.terrain_code) {
                    continue;
                }
                const auto family =
                    tile.terrain_code == "WA" || other.terrain_code == "WA" ? "WA" : "NE";
                const auto filename = coord.name + std::string("_") + tile.terrain_code + "_to_" +
                                      other.terrain_code + ".png";
                if (emitted.insert(filename).second) {
                    write_candidate_sheet(map_dir / "candidates" / filename, store, family,
                                          info.resolved_border_record);
                }
            }
        }
        generated.push_back(std::filesystem::relative(map_dir, config.out_dir).generic_string());
    }

    json hashes = json::object();
    for (const auto& name : expected_dataset_filenames()) {
        hashes["inputs/" + name] = d2res::platform::sha256_file((config.maps_dir / name).string());
    }
    collect_hashes(hashes, config.out_dir, config.out_dir);

    write_json(config.out_dir / "dataset_manifest.json",
               {{"schema_version", 1},
                {"canonical_orientation", "transpose"},
                {"source_dimensions", {{"width", kMapSize}, {"height", kMapSize}}},
                {"apex_display_origin",
                 {{"x", config.apex_display_origin.x}, {"y", config.apex_display_origin.y}}},
                {"apex_definition", apex_definition_json(config.apex_display_origin)},
                {"required_files", expected_dataset_filenames()},
                {"expected_patterns", expected_patterns_json()},
                {"actual_decoded_patterns", actual_patterns},
                {"validation_results", {{"ok", true}}},
                {"generated_relative_paths", generated},
                {"sha256", hashes},
                {"tool_version", current_build_id()}});
    return EXIT_SUCCESS;
}

} // namespace d2terrain_calibration
