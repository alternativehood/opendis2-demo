#pragma once

#include "color.hpp"
#include "rect.hpp"

#include <map>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace d2engine {

struct RenderTreeNode {
    std::string                kind;
    float                      x = 0.0f;
    float                      y = 0.0f;
    float                      w = 0.0f;
    float                      h = 0.0f;
    float                      alpha = 1.0f;
    int                        level = 0;
    std::optional<std::string> asset;
    Color                      color{.r = 0, .g = 0, .b = 0, .a = 255};
    float                      font_size = 12.0f;

    [[nodiscard]] bool operator==(const RenderTreeNode&) const = default;
};

struct ComposedTransform {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float alpha = 1.0f;
};

class RenderTree {
public:
    void load(const nlohmann::json& j);
    void save(nlohmann::json& j) const;

    [[nodiscard]] std::optional<RenderTreeNode> node(const std::string& path) const;
    [[nodiscard]] bool                          has_node(const std::string& path) const;

    void set_node(const std::string& path, RenderTreeNode node);

    [[nodiscard]] Rect                     compose(const std::string& path) const;
    [[nodiscard]] std::vector<std::string> paths() const;
    [[nodiscard]] std::vector<std::string> diagnostics() const;
    [[nodiscard]] std::vector<std::pair<std::string, RenderTreeNode>> entries() const;

    void               erase(const std::string& path);
    void               clear() { nodes_.clear(); }
    [[nodiscard]] bool empty() const { return nodes_.empty(); }

private:
    std::map<std::string, RenderTreeNode> nodes_;
};

using TreeNode = RenderTreeNode;
using TreeLayout = RenderTree;

} // namespace d2engine
