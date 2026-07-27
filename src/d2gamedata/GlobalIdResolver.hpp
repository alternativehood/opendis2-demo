#pragma once

#include "DbfGameDataIndex.hpp"
#include "GlobalIdResolution.hpp"
#include "RefTypes.hpp"
#include "SourceKind.hpp"
#include "TextResolution.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace d2gamedata {

class GlobalIdResolver {
public:
    GlobalIdResolver();

    void load_game_data(const std::string& root_path);

    GlobalIdResolution resolve(const GlobalId& id) const;
    GlobalIdResolution resolve_raw(const std::string& raw_id) const;

    TextResolution resolve_text(const TextId& id) const;

    int scanned_table_count() const { return registry_->scan_report().dbf_files_loaded; }
    int text_count() const { return registry_->text_count(); }

    const DbfGameDataIndex& registry() const { return *registry_; }

private:
    std::unique_ptr<DbfGameDataIndex> registry_;
    bool                              asset_fallback_configured_ = false;
};

} // namespace d2gamedata
