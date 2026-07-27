#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

class ManifestBuilder {
public:
    void set_game_root(std::string game_root);

    void add_container(const std::string&              relative_path,
                       const std::vector<std::string>& content_kinds);

    void add_asset(const std::string& type, const std::string& logical_name,
                   const std::string& container_relative_path,
                   const std::string& asset_relative_output_path);

    void add_warning(std::string message);

    [[nodiscard]] nlohmann::json to_json() const;

    void write(const std::filesystem::path& out_dir) const;

    [[nodiscard]] static std::string make_id(std::string_view s);

private:
    struct ContainerEntry {
        std::string              container_id;
        std::string              path;
        std::vector<std::string> content_kinds;
    };
    struct AssetEntry {
        std::string asset_id;
        std::string logical_name;
        std::string type;
        std::string container_id;
        std::string path;
    };

    std::string                 game_root_;
    std::vector<ContainerEntry> containers_;
    std::vector<AssetEntry>     assets_;
    std::vector<std::string>    warnings_;
};
