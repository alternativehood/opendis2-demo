#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

int cmd_mqrc_inventory(const std::string& container_path);

int cmd_battle_fx_report(const std::string& container_path, const std::string& game_root,
                         const std::vector<std::string>& units, const std::string& out_json,
                         const std::string& out_markdown, const std::string& contact_sheet);

namespace d2cli_research {

nlohmann::json summarize_role_morphology(const nlohmann::json& frames, std::size_t variant_count);

nlohmann::json build_gameplay_linkage(
    std::string_view                                                 unit_type,
    const std::map<std::string, std::map<std::string, std::string>>& units_by_id,
    const std::map<std::string, std::map<std::string, std::string>>& attacks_by_id);

} // namespace d2cli_research
