#pragma once

#include "../render/render_tree.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace d2engine {

struct ScreenConfig {
    std::string           screen_name;
    std::filesystem::path config_path;
    nlohmann::json        document;
    TreeLayout            tree_layout;
};

class ScreenConfigStore {
public:
    explicit ScreenConfigStore(std::filesystem::path configs_dir);

    [[nodiscard]] ScreenConfig load(std::string_view screen_name) const;
    [[nodiscard]] ScreenConfig load_validated(std::string_view                screen_name,
                                              const std::vector<std::string>& required_paths) const;

    void validate_required_nodes(std::string_view screen_name, const TreeLayout& tree,
                                 const std::vector<std::string>& required_paths) const;
    [[nodiscard]] TreeLayout load_render_tree(const std::filesystem::path& config_path) const;
    void save_render_tree(const std::filesystem::path& config_path, const TreeLayout& tree) const;

    [[nodiscard]] nlohmann::json load_document(const std::filesystem::path& path) const;
    void                         save_document_atomic(const std::filesystem::path& path,
                                                      const nlohmann::json&        document) const;

private:
    std::filesystem::path configs_dir_;
};

[[nodiscard]] std::filesystem::path resolve_runtime_config_root();

// Resolve the application config root: use override if non-empty and valid,
// otherwise fall back to resolve_runtime_config_root().
// Throws std::runtime_error if override is non-empty but invalid
// (not a directory, or missing screens/ subdirectory).
std::filesystem::path resolve_application_config_root(const std::filesystem::path& override_path);

} // namespace d2engine
