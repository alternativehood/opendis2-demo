#pragma once

#include <d2gamedata/GlobalIdResolver.hpp>
#include <d2gamedata/DbfGameDataIndex.hpp>
#include <d2gamedata/RefTypes.hpp>
#include <d2gamedata/SourceKind.hpp>
#include <d2scenario/SgTypes.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace d2analysis {

// ── Report entry for JSON output ──────────────────────────────────────────

struct GlobalIdReportEntry {
    d2gamedata::GlobalId                 id;
    bool                                 resolved = false;
    std::string                          source_kind_str;
    std::string                          source_table;
    std::string                          source_file_path;
    int                                  row_index = -1;
    std::string                          source_field;
    std::string                          category;
    std::string                          confidence;
    std::string                          reason;
    int                                  usage_count = 0;
    std::vector<d2gamedata::DbfValueRef> all_matches;
    bool                                 asset_fallback_configured = false;

    // Honest name text fields: text id stored separately from resolved value.
    // display_name is intentionally absent — use name_* fields and only
    // produce a JSON display_name when name_resolved is true.
    std::string name_text_id;
    bool        name_resolved = false;
    std::string name_value;
    std::string name_unresolved_reason;

    // Honest description text fields (same pattern).
    std::string description_text_id;
    bool        description_resolved = false;
    std::string description_value;
    std::string description_unresolved_reason;

    struct UsageExample {
        std::string object_id;
        std::string class_name;
        std::string field_name;
    };
    std::vector<UsageExample> usage_examples;
};

// ── Summary ───────────────────────────────────────────────────────────────

struct GlobalIdSummary {
    int total_usages = 0;
    int unique_ids = 0;

    int resolved_usages = 0;
    int unresolved_usages = 0;
    int null_ref_usages = 0;

    int resolved_unique_ids = 0;
    int unresolved_unique_ids = 0;
    int null_ref_unique_ids = 0;

    int  dbf_files_loaded = 0;
    int  dbf_files_failed = 0;
    int  dbf_rows_scanned = 0;
    int  dbf_field_values_scanned = 0;
    int  indexed_global_values = 0;
    int  tables_with_global_ids = 0;
    int  unique_global_ids = 0;
    int  text_rows_loaded = 0;
    bool asset_fallback_configured = false;
    int  asset_manifests_scanned = 0;

    // Separated summary maps
    std::map<std::string, int> by_source_kind;
    std::map<std::string, int> by_domain_category;
    std::map<std::string, int> by_prefix;
    std::map<std::string, int> by_class;
    std::map<std::string, int> by_field;

    std::string to_string() const;
};

// ── Unresolved report ─────────────────────────────────────────────────────

struct UnresolvedGlobalIdReport {
    std::vector<GlobalIdReportEntry> unresolved;
    int                              total_usages = 0;
    int                              resolved_usages = 0;
    int                              unresolved_usages = 0;
    int                              null_ref_usages = 0;
    int                              null_ref_unique_ids = 0;

    std::string to_string() const;
};

// ── Builder ───────────────────────────────────────────────────────────────

struct SearchEvidence {
    std::string id;
    std::string object_id;
    std::string class_name;
    std::string field_name;
    int         pos_x = -1;
    int         pos_y = -1;
    bool        is_null_ref = false;
    std::size_t file_offset = 0;
    std::size_t object_offset = 0;
};

class ScenarioGlobalIdReport {
public:
    ScenarioGlobalIdReport(const d2gamedata::GlobalIdResolver& resolver,
                           const d2scenario::SgParseResult&    parse_result);

    GlobalIdSummary                            summarize() const;
    UnresolvedGlobalIdReport                   build_unresolved_report() const;
    std::map<std::string, GlobalIdReportEntry> build_resolution_map() const;

private:
    const d2gamedata::GlobalIdResolver& resolver_;
    const d2scenario::SgParseResult&    parse_result_;
};

} // namespace d2analysis
