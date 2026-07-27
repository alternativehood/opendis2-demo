#include "debug_command_handler.hpp"
#include "battle_tuning_state.hpp"
#include "../battle_view/debug_renderable_item.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace d2engine {

namespace {

// Parse cmd and arg from raw line, trimming all surrounding whitespace and
// collapsing interior whitespace between the two tokens.
// Returns {cmd_lowercase, arg_trimmed}
std::pair<std::string, std::string> parse_line(const std::string& raw) {
    const auto ws_start = raw.find_first_not_of(" \t\r\n");
    if (ws_start == std::string::npos) {
        return {};
    }

    const auto  cmd_end = raw.find_first_of(" \t", ws_start);
    std::string cmd = (cmd_end == std::string::npos) ? raw.substr(ws_start)
                                                     : raw.substr(ws_start, cmd_end - ws_start);
    std::ranges::transform(cmd, cmd.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::string arg;
    if (cmd_end != std::string::npos) {
        const auto arg_start = raw.find_first_not_of(" \t", cmd_end);
        if (arg_start != std::string::npos) {
            const auto arg_end = raw.find_last_not_of(" \t\r\n");
            arg = raw.substr(arg_start, arg_end - arg_start + 1);
        }
    }
    return {std::move(cmd), std::move(arg)};
}

std::string format_item(const DebugRenderableItem& item) {
    // Indent based on depth in tree_path (number of path segments)
    int depth = 0;
    for (char c : item.tree_path) {
        if (c == '/')
            ++depth;
    }
    std::string out;
    if (depth > 0) {
        out.append(static_cast<std::size_t>(depth - 1) * 2, ' ');
    }
    out += item.stable_id;
    out += "  kind=";
    out += item.kind;
    if (!item.tree_path.empty()) {
        out += "  path=";
        out += item.tree_path;
    }
    if (!item.label.empty() && item.label != item.stable_id) {
        out += "  label=";
        out += item.label;
    }
    if (item.binding.has_value() && !item.binding->display_path.empty()) {
        out += "  display=";
        out += item.binding->display_path;
    }
    out += '\n';
    return out;
}

} // namespace

DebugCommandResult DebugCommandHandler::execute(const std::string& raw_line) {
    auto [cmd, arg] = parse_line(raw_line);
    if (cmd.empty()) {
        return {};
    }

    const auto& items = state_.current_items;

    auto items_ready = [&](std::string& out) -> bool {
        if (items.empty()) {
            out += "debug items are not ready yet; wait for first render\n";
            return false;
        }
        return true;
    };

    DebugCommandResult result;
    auto&              out = result.output;

    if (cmd == "help") {
        out = "commands: tree  find <text>  select <id|path|alias>  selected  save  reset  help  "
              "quit\n";
    } else if (cmd == "tree") {
        if (!items_ready(out)) {
            return result;
        }
        std::vector<const DebugRenderableItem*> sel;
        for (const auto& item : items) {
            if (item.selectable)
                sel.push_back(&item);
        }
        std::ranges::sort(sel, std::less<>{}, [](const DebugRenderableItem* p) {
            return std::pair{p->tree_path.empty() ? p->stable_id : p->tree_path, p->stable_id};
        });
        for (const auto* p : sel) {
            out += format_item(*p);
        }
    } else if (cmd == "find") {
        if (!items_ready(out)) {
            return result;
        }
        if (arg.empty()) {
            out = "usage: find <text>\n";
            return result;
        }
        std::string needle = arg;
        std::ranges::transform(needle, needle.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        auto icontains = [&](const std::string& s) {
            std::string ls = s;
            std::ranges::transform(
                ls, ls.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return ls.find(needle) != std::string::npos;
        };
        int found = 0;
        for (const auto& item : items) {
            if (!item.selectable) {
                continue;
            }
            if (icontains(item.stable_id) || icontains(item.tree_path) || icontains(item.kind) ||
                icontains(item.label)) {
                out += format_item(item);
                ++found;
            }
        }
        if (found == 0) {
            out += "no items matching '" + arg + "'\n";
        }
    } else if (cmd == "select") {
        if (!items_ready(out)) {
            return result;
        }
        if (arg.empty()) {
            out = "usage: select <id|path|alias>\n";
            return result;
        }
        const auto it = std::ranges::find_if(items, [&](const DebugRenderableItem& item) {
            if (!item.selectable) {
                return false;
            }
            if (item.stable_id == arg) {
                return true;
            }
            if (item.tree_path == arg) {
                return true;
            }
            // alias: "A_CENTER_1" or "A_CENTER_1/unit" → "tree:A_CENTER_1[/unit]"
            if (item.stable_id == "tree:" + arg) {
                return true;
            }
            // alias: "ground" or "battle" → "backgrounds.ground"/"backgrounds.battle"
            if (item.stable_id == "backgrounds." + arg) {
                return true;
            }
            return false;
        });
        if (it == items.end()) {
            out += "no selectable item matching '" + arg + "' — try: find " + arg + "\n";
        } else {
            state_.selected_debug_id = it->stable_id;
            out += "selected: " + it->stable_id + "  path=" + it->tree_path + "\n";
        }
    } else if (cmd == "selected") {
        if (!items_ready(out)) {
            return result;
        }
        const auto* item = state_.selected_item();
        if (item == nullptr) {
            out = "nothing selected\n";
        } else {
            out = format_item(*item);
        }
    } else if (cmd == "save") {
        result.request_save = true;
        out = "saved\n";
    } else if (cmd == "reset") {
        result.request_reset = true;
        out = "reset to saved\n";
    } else if (cmd == "quit") {
        result.request_quit = true;
    } else {
        out += "unknown command '" + cmd + "' — type 'help'\n";
    }
    return result;
}

} // namespace d2engine
