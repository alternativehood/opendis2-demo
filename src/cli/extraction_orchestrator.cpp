#include "extraction_orchestrator.hpp"

#include "commands_extract_anim.hpp"
#include "commands_extract_dbf.hpp"
#include "commands_extract_dat.hpp"
#include "commands_extract_dlg.hpp"
#include "commands_extract_images.hpp"
#include "commands_extract_sounds.hpp"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

bool has_kind(const d2res::ContainerEntry& container, d2res::ContentKind kind) {
    return std::ranges::find(container.likely_content, kind) != container.likely_content.end();
}

std::vector<ExtractedAsset> collect_sidecars(const fs::path& output_path, ExtractedAssetKind kind) {
    std::vector<ExtractedAsset> assets;
    std::error_code             ec;

    if (kind == ExtractedAssetKind::Animation) {
        for (const auto& entry : fs::directory_iterator(output_path, ec)) {
            if (!entry.is_directory())
                continue;
            const fs::path sidecar = entry.path() / "anim.json";
            if (!fs::is_regular_file(sidecar))
                continue;

            std::ifstream  input(sidecar);
            nlohmann::json json;
            try {
                input >> json;
                if (json.contains("name") && json["name"].is_string()) {
                    assets.push_back({.kind = kind,
                                      .logical_name = json["name"].get<std::string>(),
                                      .sidecar_path = sidecar});
                }
            } catch (const nlohmann::json::exception&) {
                continue;
            }
        }
        return assets;
    }

    for (const auto& entry : fs::directory_iterator(output_path, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        const auto filename = entry.path().filename().string();
        if (filename == "images_manifest.json" || filename == "anim_manifest.json" ||
            filename == "manifest.json") {
            continue;
        }

        if (kind == ExtractedAssetKind::DataTable) {
            const std::string logical_name = entry.path().stem().string();
            assets.push_back(
                {.kind = kind, .logical_name = logical_name, .sidecar_path = entry.path()});
            continue;
        }

        std::ifstream  input(entry.path());
        nlohmann::json json;
        try {
            input >> json;
            if (json.contains("logical_name") && json["logical_name"].is_string()) {
                assets.push_back({.kind = kind,
                                  .logical_name = json["logical_name"].get<std::string>(),
                                  .sidecar_path = entry.path()});
            }
        } catch (const nlohmann::json::exception&) {
            continue;
        }
    }
    return assets;
}

void append_assets(std::vector<ExtractedAsset>& destination, std::vector<ExtractedAsset> source) {
    destination.insert(destination.end(), std::make_move_iterator(source.begin()),
                       std::make_move_iterator(source.end()));
}

} // namespace

bool has_supported_runtime_content(const d2res::ContainerEntry& container) {
    return has_kind(container, d2res::ContentKind::Images) ||
           has_kind(container, d2res::ContentKind::Animations) ||
           has_kind(container, d2res::ContentKind::Sounds) ||
           has_kind(container, d2res::ContentKind::DataTables);
}

ContainerExtraction extract_container_assets(const fs::path&              game_root,
                                             const d2res::ContainerEntry& container,
                                             const fs::path&              output_path,
                                             const ExtractionOptions&     options) {
    ContainerExtraction result;
    result.container_path = container.path;
    result.output_path = output_path;
    result.supported = has_supported_runtime_content(container);
    for (const auto kind : container.likely_content)
        result.content_kinds.emplace_back(d2res::to_string(kind));

    if (!result.supported)
        return result;

    std::error_code ec;
    fs::create_directories(output_path, ec);
    if (ec) {
        result.error = "cannot create output directory: " + ec.message();
        return result;
    }

    const std::string source_path = (game_root / container.path).string();
    if (has_kind(container, d2res::ContentKind::Images)) {
        if (cmd_extract_images(source_path, output_path.string(), "") != 0) {
            result.error = "image extractor returned non-zero";
            return result;
        }
        append_assets(result.assets, collect_sidecars(output_path, ExtractedAssetKind::Image));
    }

    if (has_kind(container, d2res::ContentKind::Animations)) {
        if (cmd_extract_anim(source_path, output_path.string(), "", "", true, false, 100,
                             options.animation_frame_atlases, options.atlas_max_size) != 0) {
            result.error = "animation extractor returned non-zero";
            return result;
        }
        append_assets(result.assets, collect_sidecars(output_path, ExtractedAssetKind::Animation));
    }

    if (has_kind(container, d2res::ContentKind::Sounds)) {
        if (cmd_extract_sounds(source_path, output_path.string(), "", true) != 0) {
            result.error = "sound extractor returned non-zero";
            return result;
        }
        append_assets(result.assets, collect_sidecars(output_path, ExtractedAssetKind::Sound));
    }

    if (has_kind(container, d2res::ContentKind::DataTables)) {
        const fs::path    p = container.path;
        const std::string ext = p.extension().string();
        const std::string lext = [&] {
            std::string e = ext;
            for (char& c : e)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return e;
        }();
        if (lext == ".dbf") {
            if (cmd_extract_dbf(source_path, output_path.string()) != 0) {
                result.error = "dbf extractor returned non-zero";
                return result;
            }
        } else if (lext == ".dat") {
            if (cmd_extract_dat(source_path, output_path.string()) != 0) {
                result.error = "dat extractor returned non-zero";
                return result;
            }
        } else if (lext == ".dlg") {
            if (cmd_extract_dlg(source_path, output_path.string()) != 0) {
                result.error = "dlg extractor returned non-zero";
                return result;
            }
        }
        append_assets(result.assets, collect_sidecars(output_path, ExtractedAssetKind::DataTable));
    }

    const fs::path warnings_path = output_path / "warnings.txt";
    if (fs::is_regular_file(warnings_path)) {
        std::ifstream input(warnings_path);
        for (std::string line; std::getline(input, line);) {
            if (!line.empty())
                result.warnings.push_back(std::move(line));
        }
    }

    result.succeeded = true;
    return result;
}

const char* to_string(ExtractedAssetKind kind) noexcept {
    switch (kind) {
    case ExtractedAssetKind::Image:
        return "image";
    case ExtractedAssetKind::Animation:
        return "animation";
    case ExtractedAssetKind::Sound:
        return "sound";
    case ExtractedAssetKind::DataTable:
        return "data_table";
    }
    return "unknown";
}
