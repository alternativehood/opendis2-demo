#include "render_tree.hpp"

#include <cmath>
#include <nlohmann/json.hpp>
#include <utility>

namespace d2engine {
namespace {

[[nodiscard]] Color load_color(const nlohmann::json& val, Color fallback) {
    if (!val.is_object()) {
        return fallback;
    }
    return Color{.r = val.value("r", fallback.r),
                 .g = val.value("g", fallback.g),
                 .b = val.value("b", fallback.b),
                 .a = val.value("a", fallback.a)};
}

[[nodiscard]] nlohmann::json save_color(Color c) {
    return nlohmann::json{{"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a}};
}

} // namespace

void RenderTree::load(const nlohmann::json& j) {
    nodes_.clear();
    if (!j.is_object()) {
        return;
    }
    for (const auto& [path, val] : j.items()) {
        if (!val.is_object()) {
            continue;
        }
        RenderTreeNode n;
        n.kind = val.value("kind", std::string{});
        n.x = val.value("x", 0.0f);
        n.y = val.value("y", 0.0f);
        n.w = val.value("w", 0.0f);
        n.h = val.value("h", 0.0f);
        n.alpha = val.value("alpha", 1.0f);
        n.level = val.value("level", 0);
        if (val.contains("asset") && val["asset"].is_string()) {
            n.asset = val["asset"].get<std::string>();
        }
        if (val.contains("color")) {
            n.color = load_color(val["color"], n.color);
        }
        n.font_size = val.value("font_size", n.font_size);
        nodes_[path] = std::move(n);
    }
}

void RenderTree::save(nlohmann::json& j) const {
    j = nlohmann::json::object();
    for (const auto& [path, n] : nodes_) {
        nlohmann::json obj;
        obj["kind"] = n.kind;
        obj["x"] = static_cast<int>(std::lround(n.x));
        obj["y"] = static_cast<int>(std::lround(n.y));
        obj["w"] = static_cast<int>(std::lround(n.w));
        obj["h"] = static_cast<int>(std::lround(n.h));
        obj["alpha"] = n.alpha;
        obj["level"] = n.level;
        if (n.asset.has_value()) {
            obj["asset"] = *n.asset;
        }
        if (n.kind == "text") {
            obj["color"] = save_color(n.color);
            obj["font_size"] = static_cast<int>(n.font_size);
        }
        j[path] = std::move(obj);
    }
}

std::optional<RenderTreeNode> RenderTree::node(const std::string& path) const {
    const auto it = nodes_.find(path);
    if (it != nodes_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool RenderTree::has_node(const std::string& path) const {
    return nodes_.contains(path);
}

void RenderTree::set_node(const std::string& path, RenderTreeNode node) {
    nodes_[path] = std::move(node);
}

Rect RenderTree::compose(const std::string& path) const {
    if (path.empty() || path[0] != '/') {
        return {};
    }
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool  found = false;

    std::string prefix;
    size_t      pos = 1;
    while (pos < path.size()) {
        const size_t      slash = path.find('/', pos);
        const std::string seg = path.substr(pos, slash - pos);
        prefix += '/';
        prefix += seg;

        const auto it = nodes_.find(prefix);
        if (it != nodes_.end()) {
            x += it->second.x;
            y += it->second.y;
            w = it->second.w;
            h = it->second.h;
            found = true;
        }
        pos = (slash == std::string::npos) ? path.size() : slash + 1;
    }

    if (!found) {
        return {};
    }
    return {.x = x, .y = y, .w = w, .h = h};
}

std::vector<std::string> RenderTree::paths() const {
    std::vector<std::string> result;
    result.reserve(nodes_.size());
    for (const auto& [path, _] : nodes_) {
        result.push_back(path);
    }
    return result;
}

std::vector<std::pair<std::string, RenderTreeNode>> RenderTree::entries() const {
    return {nodes_.begin(), nodes_.end()};
}

void RenderTree::erase(const std::string& path) {
    nodes_.erase(path);
}

std::vector<std::string> RenderTree::diagnostics() const {
    std::vector<std::string> result;
    for (const auto& [path, _] : nodes_) {
        const auto slash = path.find_last_of('/');
        if (slash != std::string::npos && slash > 0) {
            const std::string parent = path.substr(0, slash);
            if (!nodes_.contains(parent)) {
                std::string message = "render_tree missing parent ";
                message.append(parent).append(" for ").append(path);
                result.push_back(std::move(message));
            }
        }
    }
    return result;
}

} // namespace d2engine
