#include "screen_config_store.hpp"
#include "d2engine/platform/executable_path.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall MoveFileExW(const wchar_t*, const wchar_t*,
                                                           unsigned long);
inline constexpr unsigned long                 kMoveFileReplaceExisting = 0x00000001u;
inline constexpr unsigned long                 kMoveFileWriteThrough = 0x00000008u;
#endif

namespace d2engine {

std::filesystem::path resolve_runtime_config_root() {
    const auto exe_dir = d2engine::platform::executable_directory();

#if defined(__APPLE__)
    auto resources_configs = exe_dir.parent_path() / "Resources" / "configs";
    if (std::filesystem::is_directory(resources_configs)) {
        return resources_configs;
    }
#endif

    auto configs = exe_dir / "configs";
    if (std::filesystem::is_directory(configs)) {
        return configs;
    }

#if !defined(__APPLE__)
    auto resources_configs = exe_dir.parent_path() / "Resources" / "configs";
    if (std::filesystem::is_directory(resources_configs)) {
        return resources_configs;
    }
#endif

    throw std::runtime_error("configs/ directory not found relative to executable: " +
                             exe_dir.string());
}

std::filesystem::path resolve_application_config_root(const std::filesystem::path& override_path) {
    if (override_path.empty())
        return resolve_runtime_config_root();
    if (!std::filesystem::is_directory(override_path)) {
        throw std::runtime_error("config root override is not a directory: " +
                                 override_path.string());
    }
    const auto canonical = std::filesystem::canonical(override_path);
    if (!std::filesystem::is_directory(canonical / "screens")) {
        throw std::runtime_error("config root override missing screens/ subdirectory: " +
                                 canonical.string());
    }
    return canonical;
}

ScreenConfigStore::ScreenConfigStore(std::filesystem::path configs_dir)
    : configs_dir_(std::move(configs_dir)) {}

ScreenConfig ScreenConfigStore::load(std::string_view screen_name) const {
    const auto config_path = configs_dir_ / "screens" / std::string{screen_name}.append(".json");
    auto       document = load_document(config_path);
    if (!document.contains("render_tree") || !document["render_tree"].is_object()) {
        throw std::runtime_error("Missing mandatory \"render_tree\" in: " + config_path.string());
    }
    ScreenConfig config;
    config.screen_name = std::string{screen_name};
    config.config_path = config_path;
    config.document = std::move(document);
    config.tree_layout.load(config.document["render_tree"]);
    return config;
}

ScreenConfig
ScreenConfigStore::load_validated(std::string_view                screen_name,
                                  const std::vector<std::string>& required_paths) const {
    auto config = load(screen_name);
    validate_required_nodes(screen_name, config.tree_layout, required_paths);
    return config;
}

void ScreenConfigStore::validate_required_nodes(
    std::string_view screen_name, const TreeLayout& tree,
    const std::vector<std::string>& required_paths) const {
    std::vector<std::string> missing;
    for (const auto& path : required_paths) {
        if (!tree.has_node(path))
            missing.push_back(path);
    }
    if (!missing.empty()) {
        std::ostringstream msg;
        msg << "Screen '" << screen_name << "' missing required layout nodes:";
        for (const auto& p : missing)
            msg << "\n  " << p;
        throw std::runtime_error(msg.str());
    }
}

TreeLayout ScreenConfigStore::load_render_tree(const std::filesystem::path& config_path) const {
    auto document = load_document(config_path);
    if (!document.contains("render_tree") || !document["render_tree"].is_object()) {
        throw std::runtime_error("Missing render_tree in: " + config_path.string());
    }
    TreeLayout tree;
    tree.load(document["render_tree"]);
    return tree;
}

void ScreenConfigStore::save_render_tree(const std::filesystem::path& config_path,
                                         const TreeLayout&            tree) const {
    auto doc = load_document(config_path);
    if (!doc.contains("render_tree") || !doc["render_tree"].is_object())
        throw std::runtime_error("Missing render_tree in: " + config_path.string());

    nlohmann::json rt;
    tree.save(rt);
    doc["render_tree"] = std::move(rt);
    save_document_atomic(config_path, doc);
}

nlohmann::json ScreenConfigStore::load_document(const std::filesystem::path& path) const {
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("Config file not found: " + path.string());
    }
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("Cannot open: " + path.string());
    auto doc = nlohmann::json::parse(in, nullptr, false);
    if (doc.is_discarded())
        throw std::runtime_error("Invalid JSON: " + path.string());
    if (!doc.is_object())
        throw std::runtime_error("Root not an object: " + path.string());
    return doc;
}

void ScreenConfigStore::save_document_atomic(const std::filesystem::path& path,
                                             const nlohmann::json&        document) const {
    auto tmp_path = path;
    tmp_path += ".tmp";
    {
        std::ofstream out(tmp_path);
        if (!out)
            throw std::runtime_error("Cannot create: " + tmp_path.string());
        out << document.dump(2) << '\n';
        if (!out) {
            std::filesystem::remove(tmp_path);
            throw std::runtime_error("Write failed: " + tmp_path.string());
        }
        out.flush();
        if (!out) {
            std::filesystem::remove(tmp_path);
            throw std::runtime_error("Flush failed: " + tmp_path.string());
        }
    }
#if defined(_WIN32)
    if (!MoveFileExW(tmp_path.c_str(), path.c_str(),
                     kMoveFileReplaceExisting | kMoveFileWriteThrough)) {
        std::filesystem::remove(tmp_path);
        throw std::runtime_error("MoveFileExW failed: " + tmp_path.string());
    }
#else
    if (::rename(tmp_path.c_str(), path.c_str()) != 0) {
        std::filesystem::remove(tmp_path);
        throw std::runtime_error("rename failed: " + tmp_path.string() + " -> " + path.string());
    }
#endif
}

} // namespace d2engine
