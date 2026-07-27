#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace d2res {

struct DlgElement {
    std::string        type;
    std::string        id;
    std::array<int, 4> bounds{};
    nlohmann::json     extra; // type-specific fields
};

struct DlgDialog {
    std::string             id;
    std::array<int, 4>      bounds{};
    std::vector<DlgElement> elements;
};

class DlgParser {
public:
    void                         parse(std::string_view text);
    [[nodiscard]] nlohmann::json to_json() const;

    [[nodiscard]] const std::vector<DlgDialog>& dialogs() const { return dialogs_; }

private:
    std::vector<DlgDialog> dialogs_;
};

} // namespace d2res
