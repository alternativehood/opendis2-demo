#pragma once
#include <map>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace d2res {

class DatParser {
public:
    void                         parse(std::string_view text);
    [[nodiscard]] nlohmann::json to_json() const;

    [[nodiscard]] const std::map<std::string, std::string>& entries() const { return entries_; }

private:
    std::map<std::string, std::string> entries_;
};

} // namespace d2res
