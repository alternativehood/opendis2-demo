#include "manifest_builder.hpp"

#include <cctype>
#include <fstream>

namespace fs = std::filesystem;

std::string ManifestBuilder::make_id(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        if (c == '\\') {
            out += '/';
        } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                   c == '_' || c == '-' || c == '.' || c == '/') {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            out += '_';
        }
    }
    return out;
}

void ManifestBuilder::set_game_root(std::string game_root) {
    game_root_ = std::move(game_root);
}

void ManifestBuilder::add_container(const std::string&              relative_path,
                                    const std::vector<std::string>& content_kinds) {
    ContainerEntry e;
    e.container_id = make_id(relative_path);
    e.path = relative_path;
    e.content_kinds = content_kinds;
    containers_.push_back(std::move(e));
}

void ManifestBuilder::add_asset(const std::string& type, const std::string& logical_name,
                                const std::string& container_relative_path,
                                const std::string& asset_relative_output_path) {
    const std::string cid = make_id(container_relative_path);
    AssetEntry        e;
    e.asset_id = cid + '/' + make_id(logical_name);
    e.logical_name = logical_name;
    e.type = type;
    e.container_id = cid;
    e.path = asset_relative_output_path;
    assets_.push_back(std::move(e));
}

void ManifestBuilder::add_warning(std::string message) {
    warnings_.push_back(std::move(message));
}

nlohmann::json ManifestBuilder::to_json() const {
    nlohmann::json containers_arr = nlohmann::json::array();
    for (const auto& c : containers_) {
        nlohmann::json kinds_arr = nlohmann::json::array();
        for (const auto& k : c.content_kinds)
            kinds_arr.push_back(k);
        nlohmann::json entry;
        entry["container_id"] = c.container_id;
        entry["path"] = c.path;
        entry["content_kinds"] = std::move(kinds_arr);
        containers_arr.push_back(std::move(entry));
    }

    nlohmann::json assets_arr = nlohmann::json::array();
    for (const auto& a : assets_) {
        nlohmann::json entry;
        entry["asset_id"] = a.asset_id;
        entry["logical_name"] = a.logical_name;
        entry["type"] = a.type;
        entry["container_id"] = a.container_id;
        entry["path"] = a.path;
        assets_arr.push_back(std::move(entry));
    }

    nlohmann::json warnings_arr = nlohmann::json::array();
    for (const auto& w : warnings_)
        warnings_arr.push_back(w);

    nlohmann::json j;
    j["asset_schema_version"] = 1;
    j["game_root"] = game_root_;
    j["containers"] = std::move(containers_arr);
    j["assets"] = std::move(assets_arr);
    j["warnings"] = std::move(warnings_arr);
    return j;
}

void ManifestBuilder::write(const fs::path& out_dir) const {
    std::ofstream f(out_dir / "game_manifest.json");
    f << to_json().dump(2) << '\n';
}
