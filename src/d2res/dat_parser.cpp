#include "d2res/dat_parser.hpp"

#include <sstream>

namespace d2res {

namespace {

std::string trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(first, last - first + 1));
}

} // namespace

void DatParser::parse(std::string_view text) {
    entries_.clear();

    std::istringstream stream{std::string(text)};
    std::string        line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const auto trimmed = trim(line);
        if (trimmed.empty())
            continue;
        if (trimmed[0] == ';' || trimmed[0] == '#')
            continue;

        const auto eq = trimmed.find('=');
        if (eq == std::string::npos)
            continue;

        auto key = trim(trimmed.substr(0, eq));
        auto val = trim(trimmed.substr(eq + 1));
        if (!key.empty())
            entries_[key] = val;
    }
}

nlohmann::json DatParser::to_json() const {
    nlohmann::json obj = nlohmann::json::object();
    for (const auto& [k, v] : entries_)
        obj[k] = v;
    return obj;
}

} // namespace d2res
