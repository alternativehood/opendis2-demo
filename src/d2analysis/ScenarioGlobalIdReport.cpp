#include "ScenarioGlobalIdReport.hpp"

#include <algorithm>
#include <sstream>

namespace d2analysis {

ScenarioGlobalIdReport::ScenarioGlobalIdReport(const d2gamedata::GlobalIdResolver& resolver,
                                               const d2scenario::SgParseResult&    parse_result)
    : resolver_(resolver), parse_result_(parse_result) {}

GlobalIdSummary ScenarioGlobalIdReport::summarize() const {
    GlobalIdSummary summary;

    // Fill registry stats
    auto sreport = resolver_.registry().scan_report();
    summary.dbf_files_loaded = sreport.dbf_files_loaded;
    summary.dbf_files_failed = sreport.dbf_files_failed;
    summary.dbf_rows_scanned = sreport.dbf_rows_scanned;
    summary.dbf_field_values_scanned = sreport.dbf_field_values_scanned;
    summary.indexed_global_values = sreport.indexed_global_values;
    summary.tables_with_global_ids = sreport.tables_with_global_ids;
    summary.unique_global_ids = sreport.unique_global_ids;
    summary.text_rows_loaded = sreport.text_rows_loaded;
    summary.asset_fallback_configured = false;
    summary.asset_manifests_scanned = 0;

    summary.total_usages = static_cast<int>(parse_result_.global_id_usages.size());

    std::map<std::string, int> prefix_counts;
    std::set<std::string>      all_ids;
    std::set<std::string>      resolved_ids;
    std::set<std::string>      unresolved_ids;

    for (const auto& usage : parse_result_.global_id_usages) {
        all_ids.insert(usage.value.value);
        summary.by_field[usage.field_name]++;
        summary.by_class[usage.class_name]++;

        // Each usage increments exactly ONE by_source_kind entry (via resolve_raw).
        // Null refs are NOT manually incremented here to avoid double counting.
        auto resolved_result = resolver_.resolve_raw(usage.value.value);
        summary.by_source_kind[d2gamedata::to_string(resolved_result.source_kind)]++;

        if (usage.value.is_null()) {
            ++summary.null_ref_usages;
            summary.by_domain_category["null_reference"]++;
        } else if (usage.value.value.size() >= 7) {
            std::string prefix = usage.value.prefix();
            prefix_counts[prefix]++;
            if (resolved_result.status == d2gamedata::ResolutionStatus::Resolved &&
                !resolved_result.category_kind.empty()) {
                summary.by_domain_category[resolved_result.category_kind]++;
            } else {
                summary.by_domain_category["unresolved"]++;
            }
        }

        if (usage.value.is_null()) {
            // counted already via null_ref_usages
        } else if (resolved_result.status == d2gamedata::ResolutionStatus::Resolved) {
            ++summary.resolved_usages;
            resolved_ids.insert(usage.value.value);
        } else {
            ++summary.unresolved_usages;
            unresolved_ids.insert(usage.value.value);
        }
    }

    summary.unique_ids = static_cast<int>(all_ids.size());
    summary.resolved_unique_ids = static_cast<int>(resolved_ids.size());
    summary.unresolved_unique_ids = static_cast<int>(unresolved_ids.size());
    summary.null_ref_unique_ids = all_ids.count(std::string(d2gamedata::kNullGlobalId)) ? 1 : 0;

    for (const auto& kv : prefix_counts) {
        summary.by_prefix[kv.first] = kv.second;
    }

    return summary;
}

UnresolvedGlobalIdReport ScenarioGlobalIdReport::build_unresolved_report() const {
    UnresolvedGlobalIdReport report;
    report.total_usages = static_cast<int>(parse_result_.global_id_usages.size());

    std::set<std::string> seen_unresolved;

    for (const auto& usage : parse_result_.global_id_usages) {
        if (usage.value.is_null()) {
            ++report.null_ref_usages;
            continue;
        }

        auto resolved = resolver_.resolve_raw(usage.value.value);
        if (resolved.status == d2gamedata::ResolutionStatus::Resolved) {
            ++report.resolved_usages;
        } else {
            ++report.unresolved_usages;
            if (seen_unresolved.insert(usage.value.value).second) {
                GlobalIdReportEntry entry;
                entry.id = d2gamedata::GlobalId{usage.value.value};
                entry.resolved = false;
                entry.reason = resolved.unresolved_reason;
                entry.source_kind_str = d2gamedata::to_string(resolved.source_kind);
                report.unresolved.push_back(entry);
            }
        }
    }

    report.null_ref_unique_ids = report.null_ref_usages > 0 ? 1 : 0;

    // Fill usage counts and examples
    for (auto& entry : report.unresolved) {
        for (const auto& usage : parse_result_.global_id_usages) {
            if (usage.value.value == entry.id.value) {
                ++entry.usage_count;
                if (static_cast<int>(entry.usage_examples.size()) < 3) {
                    GlobalIdReportEntry::UsageExample ex;
                    ex.object_id = usage.object_id.value;
                    ex.class_name = usage.class_name;
                    ex.field_name = usage.field_name;
                    entry.usage_examples.push_back(ex);
                }
            }
        }
    }

    return report;
}

std::map<std::string, GlobalIdReportEntry> ScenarioGlobalIdReport::build_resolution_map() const {
    std::map<std::string, GlobalIdReportEntry> result;

    std::set<std::string> unique_ids;
    for (const auto& usage : parse_result_.global_id_usages) {
        unique_ids.insert(usage.value.value);
    }

    for (const auto& raw_id : unique_ids) {
        auto resolved = resolver_.resolve_raw(raw_id);

        GlobalIdReportEntry entry;
        entry.id = d2gamedata::GlobalId{raw_id};
        entry.resolved = (resolved.status == d2gamedata::ResolutionStatus::Resolved);
        entry.source_kind_str = d2gamedata::to_string(resolved.source_kind);
        entry.category = resolved.category_kind;
        entry.reason = resolved.unresolved_reason;
        entry.all_matches = resolved.all_matches;

        if (resolved.primary_match.has_value()) {
            entry.source_table = resolved.primary_match->table_name;
            entry.source_file_path = resolved.primary_match->file_path.string();
            entry.row_index = static_cast<int>(resolved.primary_match->row_index);
            entry.source_field = resolved.primary_match->field_name;
        }

        // Honest name text resolution: text id stored separately from resolved value.
        if (resolved.name.has_value()) {
            entry.name_text_id = resolved.name->id.value;
            entry.name_resolved = resolved.name->resolved;
            if (resolved.name->resolved) {
                entry.name_value = resolved.name->value;
            } else {
                entry.name_unresolved_reason = resolved.name->reason_unresolved;
            }
        }

        if (resolved.description.has_value()) {
            entry.description_text_id = resolved.description->id.value;
            entry.description_resolved = resolved.description->resolved;
            if (resolved.description->resolved) {
                entry.description_value = resolved.description->value;
            } else {
                entry.description_unresolved_reason = resolved.description->reason_unresolved;
            }
        }

        if (resolved.status == d2gamedata::ResolutionStatus::Resolved) {
            entry.confidence = "high";
        } else {
            entry.confidence = "none";
        }

        entry.asset_fallback_configured = false;

        // Count usages and add examples
        for (const auto& usage : parse_result_.global_id_usages) {
            if (usage.value.value == raw_id) {
                ++entry.usage_count;
                if (static_cast<int>(entry.usage_examples.size()) < 3) {
                    GlobalIdReportEntry::UsageExample ex;
                    ex.object_id = usage.object_id.value;
                    ex.class_name = usage.class_name;
                    ex.field_name = usage.field_name;
                    entry.usage_examples.push_back(ex);
                }
            }
        }

        result[raw_id] = entry;
    }

    return result;
}

std::string GlobalIdSummary::to_string() const {
    std::string out;
    out += "=== Global ID Summary ===\n";
    out += "  Total usages: " + std::to_string(total_usages) + "\n";
    out += "  Unique IDs: " + std::to_string(unique_ids) + "\n";
    out += "  Resolved usages: " + std::to_string(resolved_usages) + "\n";
    out += "  Resolved unique IDs: " + std::to_string(resolved_unique_ids) + "\n";
    out += "  Unresolved usages: " + std::to_string(unresolved_usages) + "\n";
    out += "  Unresolved unique IDs: " + std::to_string(unresolved_unique_ids) + "\n";
    out += "  Null ref usages: " + std::to_string(null_ref_usages) + "\n";
    out += "\n  DBF files loaded: " + std::to_string(dbf_files_loaded) + "\n";
    out += "  DBF files failed: " + std::to_string(dbf_files_failed) + "\n";
    out += "  DBF rows scanned: " + std::to_string(dbf_rows_scanned) + "\n";
    out += "  DBF field values scanned: " + std::to_string(dbf_field_values_scanned) + "\n";
    out += "  Indexed G* values: " + std::to_string(indexed_global_values) + "\n";
    out += "  Unique G* IDs: " + std::to_string(unique_global_ids) + "\n";
    out += "  Tables with G* IDs: " + std::to_string(tables_with_global_ids) + "\n";
    out += "  Text rows loaded: " + std::to_string(text_rows_loaded) + "\n";
    out += "  Asset fallback: " +
           std::string(asset_fallback_configured ? "configured" : "not configured") + "\n";
    out += "\n  By source kind:\n";
    for (const auto& kv : by_source_kind) {
        out += "    " + kv.first + ": " + std::to_string(kv.second) + "\n";
    }
    out += "\n  By domain category:\n";
    for (const auto& kv : by_domain_category) {
        out += "    " + kv.first + ": " + std::to_string(kv.second) + "\n";
    }
    out += "\n  By prefix:\n";
    for (const auto& kv : by_prefix) {
        out += "    " + kv.first + "*: " + std::to_string(kv.second) + "\n";
    }
    out += "\n  By class:\n";
    for (const auto& kv : by_class) {
        out += "    " + kv.first + ": " + std::to_string(kv.second) + "\n";
    }
    out += "\n  By field:\n";
    for (const auto& kv : by_field) {
        out += "    " + kv.first + ": " + std::to_string(kv.second) + "\n";
    }
    return out;
}

std::string UnresolvedGlobalIdReport::to_string() const {
    std::string out;
    out += "=== Unresolved Global ID Report ===\n";
    if (unresolved_usages == 0) {
        out += "  All " + std::to_string(total_usages) + " global ID usages resolved.\n";
        out += "  (null ref usages: " + std::to_string(null_ref_usages) + ")\n";
    } else {
        out += "  " + std::to_string(unresolved_usages) + "/" + std::to_string(total_usages) +
               " usages unresolved";
        out += " (null ref usages: " + std::to_string(null_ref_usages) + ")\n";
        out += "  Unique unresolved IDs: " + std::to_string(unresolved.size()) + "\n";
        for (const auto& r : unresolved) {
            out += "  " + r.id.value + ": " + r.reason + "\n";
        }
    }
    return out;
}

} // namespace d2analysis
