#include "unit_def.hpp"

#include <charconv>

namespace d2engine {

ResourceCost parse_resource_cost(std::string_view s) {
    ResourceCost cost;
    if (s.empty()) {
        return cost;
    }
    // Format: gNNN:rNNN:yNNN:eNNN:wNNN — colon-separated tokens, prefix letter then digits
    while (!s.empty()) {
        const char prefix = s.front();
        s.remove_prefix(1);

        // find end of token (next colon or end of string)
        const std::size_t      colon = s.find(':');
        const std::string_view num_part =
            (colon == std::string_view::npos) ? s : s.substr(0, colon);

        int value = 0;
        std::from_chars(num_part.data(), num_part.data() + num_part.size(), value);

        switch (prefix) {
        case 'g':
            cost.gold = value;
            break;
        case 'r':
            cost.runic = value;
            break;
        case 'y':
            cost.yellow = value;
            break;
        case 'e':
            cost.infernal = value;
            break;
        case 'w':
            cost.grove = value;
            break;
        default:
            break;
        }

        if (colon == std::string_view::npos) {
            break;
        }
        s.remove_prefix(colon + 1);
    }
    return cost;
}

} // namespace d2engine
