#include "d2gamedata/GlobalIdResolver.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace d2gamedata {

static std::string infer_category(const std::string& table_name) {
    std::string lower = table_name;
    for (auto& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lower.find("unit") != std::string::npos)
        return "unit";
    if (lower.find("attack") != std::string::npos)
        return "attack";
    if (lower.find("race") != std::string::npos)
        return "race";
    if (lower.find("lord") != std::string::npos)
        return "lord";
    if (lower.find("item") != std::string::npos)
        return "item";
    if (lower.find("spell") != std::string::npos)
        return "spell";
    if (lower.find("build") != std::string::npos)
        return "building";
    if (lower.find("landscap") != std::string::npos || lower.find("land") != std::string::npos)
        return "map_graphic";
    if (lower.find("mountain") != std::string::npos)
        return "mountain";
    if (lower.find("road") != std::string::npos)
        return "road";
    if (lower.find("terrain") != std::string::npos || lower.find("tile") != std::string::npos)
        return "terrain";
    if (lower.find("text") != std::string::npos || lower.find("tglobal") != std::string::npos)
        return "text";
    if (lower.find("global") != std::string::npos)
        return "global";
    if (lower.find("facet") != std::string::npos)
        return "facet";
    if (lower.find("leader") != std::string::npos)
        return "leader";
    if (lower.find("mercen") != std::string::npos)
        return "mercenary";
    if (lower.find("music") != std::string::npos)
        return "music";
    if (lower.find("sound") != std::string::npos)
        return "sound";
    if (lower.find("movie") != std::string::npos)
        return "movie";
    if (lower.find("effect") != std::string::npos)
        return "effect";
    if (lower.find("ai") != std::string::npos)
        return "ai";
    if (lower.find("scen") != std::string::npos)
        return "scenario";
    return "misc";
}

GlobalIdResolver::GlobalIdResolver() : registry_(std::make_unique<DbfGameDataIndex>()) {}

void GlobalIdResolver::load_game_data(const std::string& root_path) {
    if (!fs::is_directory(root_path))
        return;

    fs::path root(root_path);

    bool has_dbf_directly = false;
    for (const auto& entry : fs::directory_iterator(root_path)) {
        if (entry.path().extension() == ".dbf") {
            has_dbf_directly = true;
            break;
        }
    }

    if (has_dbf_directly) {
        registry_->load_directory(root_path);
        asset_fallback_configured_ = false;
        return;
    }

    fs::path dbf_path = root / "DBF";
    if (fs::is_directory(dbf_path)) {
        registry_->load_directory(dbf_path.string());
        asset_fallback_configured_ = false;
        return;
    }

    fs::path globals_path = root / "Globals";
    if (fs::is_directory(globals_path))
        registry_->load_directory(globals_path.string());

    if (fs::is_directory(dbf_path))
        registry_->load_directory(dbf_path.string());

    fs::path script_path = root / "Script";
    if (fs::is_directory(script_path))
        registry_->load_directory(script_path.string());

    asset_fallback_configured_ = false;
}

GlobalIdResolution GlobalIdResolver::resolve(const GlobalId& id) const {
    return resolve_raw(id.value);
}

GlobalIdResolution GlobalIdResolver::resolve_raw(const std::string& raw_id) const {
    GlobalIdResolution result;
    result.id = GlobalId{raw_id};

    if (raw_id == kNullGlobalId) {
        result.status = ResolutionStatus::NullRef;
        result.source_kind = SourceKind::Null;
        result.category_kind = "null_reference";
        return result;
    }

    auto refs = registry_->find_global_id(raw_id);
    if (!refs.empty()) {
        result.status = ResolutionStatus::Resolved;
        result.source_kind = SourceKind::Dbf;
        result.primary_match = refs[0];
        result.all_matches = refs;
        result.category_kind = infer_category(refs[0].table_name);

        if (!refs[0].name_txt_id.empty()) {
            TextResolution tr;
            tr.id = TextId{refs[0].name_txt_id};
            std::string txt = registry_->resolve_text(refs[0].name_txt_id);
            tr.value = txt;
            tr.resolved = !txt.empty();
            if (!tr.resolved)
                tr.reason_unresolved = "Text ID not found in Tglobal";
            result.name = tr;
        }

        if (!refs[0].desc_txt_id.empty()) {
            TextResolution tr;
            tr.id = TextId{refs[0].desc_txt_id};
            std::string txt = registry_->resolve_text(refs[0].desc_txt_id);
            tr.value = txt;
            tr.resolved = !txt.empty();
            if (!tr.resolved)
                tr.reason_unresolved = "Text ID not found in Tglobal";
            result.description = tr;
        }

        if (!result.name.has_value()) {
            auto txt = registry_->resolve_text(raw_id);
            if (!txt.empty()) {
                TextResolution tr;
                tr.id = TextId{raw_id};
                tr.value = txt;
                tr.resolved = true;
                result.name = tr;
            }
        }

        return result;
    }

    auto txt = registry_->resolve_text(raw_id);
    if (!txt.empty()) {
        result.status = ResolutionStatus::Resolved;
        result.source_kind = SourceKind::Text;
        result.category_kind = "text";
        TextResolution tr;
        tr.id = TextId{raw_id};
        tr.value = txt;
        tr.resolved = true;
        result.name = tr;
        return result;
    }

    result.status = ResolutionStatus::Unresolved;
    result.source_kind = SourceKind::Unknown;

    auto        report = registry_->scan_report();
    std::string reason = "Not found in any scanned DBF table";
    if (report.dbf_files_loaded > 0) {
        reason += ". Scanned " + std::to_string(report.dbf_files_loaded) + " files, " +
                  std::to_string(report.dbf_rows_scanned) + " rows, " +
                  std::to_string(report.dbf_field_values_scanned) + " field values, " +
                  std::to_string(report.indexed_global_values) + " indexed G* values. ";
        reason += "Unresolved under current DBF/text-only resolver; "
                  "likely mod/campaign-local or asset-backed based on id ranges/usages. "
                  "Asset/resource fallback not configured (future work).";
    } else {
        reason += ". No game data loaded. Use --globals with game root or DBF directory. "
                  "Asset/resource fallback not configured (future work).";
    }
    result.unresolved_reason = reason;

    return result;
}

TextResolution GlobalIdResolver::resolve_text(const TextId& id) const {
    TextResolution tr;
    tr.id = id;
    std::string txt = registry_->resolve_text(id.value);
    tr.value = txt;
    tr.resolved = !txt.empty();
    if (!tr.resolved)
        tr.reason_unresolved = "Text ID not found in text map";
    return tr;
}

} // namespace d2gamedata
