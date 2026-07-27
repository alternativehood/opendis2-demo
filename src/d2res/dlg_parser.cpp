#include "d2res/dlg_parser.hpp"

#include <sstream>
#include <vector>

namespace d2res {

namespace {

std::vector<std::string> split_comma(const std::string& s) {
    std::vector<std::string> result;
    std::string              token;
    bool                     in_quotes = false;
    for (char const c : s) {
        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (c == ',' && !in_quotes) {
            result.push_back(token);
            token.clear();
            continue;
        }
        token += c;
    }
    result.push_back(token);
    return result;
}

std::array<int, 4> parse_bounds(const std::vector<std::string>& parts, size_t start) {
    std::array<int, 4> b{};
    for (int i = 0; i < 4; ++i) {
        size_t const idx = start + static_cast<size_t>(i);
        if (idx < parts.size()) {
            try {
                b[static_cast<size_t>(i)] = std::stoi(parts[idx]);
            } catch (...) {
            }
        }
    }
    return b;
}

std::string get_field(const std::vector<std::string>& parts, size_t idx) {
    if (idx < parts.size())
        return parts[idx];
    return {};
}

} // namespace

void DlgParser::parse(std::string_view text) {
    dialogs_.clear();

    std::istringstream stream{std::string(text)};
    std::string        line;

    DlgDialog* current = nullptr;
    bool       in_block = false;

    while (std::getline(stream, line)) {
        // Strip \r and leading whitespace
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const auto first_nonws = line.find_first_not_of(" \t");
        if (first_nonws == std::string::npos)
            continue;
        line = line.substr(first_nonws);

        // Split on first tab
        const auto        tab_pos = line.find('\t');
        const std::string token = (tab_pos != std::string::npos) ? line.substr(0, tab_pos) : line;
        const std::string rest =
            (tab_pos != std::string::npos) ? line.substr(tab_pos + 1) : std::string{};

        if (token == "DIALOG") {
            dialogs_.emplace_back();
            current = &dialogs_.back();
            in_block = false;
            auto parts = split_comma(rest);
            current->id = get_field(parts, 0);
            current->bounds = parse_bounds(parts, 1);
        } else if (token == "BEGIN") {
            in_block = true;
        } else if (token == "END") {
            in_block = false;
            current = nullptr;
        } else if (in_block && (current != nullptr)) {
            DlgElement el;
            auto       parts = split_comma(rest);
            el.id = get_field(parts, 0);
            el.bounds = parse_bounds(parts, 1);

            if (token == "IMAGE") {
                el.type = "IMAGE";
                el.extra["image"] = get_field(parts, 5);
            } else if (token == "BUTTON") {
                el.type = "BUTTON";
                nlohmann::json images = nlohmann::json::array();
                for (size_t i = 5; i <= 8 && i < parts.size(); ++i)
                    images.push_back(parts[i]);
                el.extra["images"] = std::move(images);
                el.extra["text_key"] = get_field(parts, 9);
            } else if (token == "TEXT") {
                el.type = "TEXT";
                el.extra["text_key"] = get_field(parts, 6);
            } else {
                el.type = "UNKNOWN";
                el.extra["raw"] = line;
            }
            current->elements.push_back(std::move(el));
        }
    }
}

nlohmann::json DlgParser::to_json() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& dlg : dialogs_) {
        nlohmann::json dj;
        dj["id"] = dlg.id;
        dj["bounds"] = dlg.bounds;
        auto& elems = dj["elements"] = nlohmann::json::array();
        for (const auto& el : dlg.elements) {
            nlohmann::json ej;
            ej["type"] = el.type;
            ej["id"] = el.id;
            ej["bounds"] = el.bounds;
            for (const auto& [k, v] : el.extra.items())
                ej[k] = v;
            elems.push_back(std::move(ej));
        }
        arr.push_back(std::move(dj));
    }
    return arr;
}

} // namespace d2res
