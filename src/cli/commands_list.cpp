#include "commands_list.hpp"
#include "d2res/mqdb.hpp"
#include <d2log/log.hpp>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <iostream>
#include <vector>

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

using json = nlohmann::json;

static std::string record_name_or_unnamed(const d2res::Record& rec) {
    return rec.name.empty() ? "<unnamed>" : rec.name;
}

int cmd_list(const std::string& container_path, bool json_output, bool special_only) {
    d2res::MqdbContainer container;
    try {
        container = d2res::MqdbContainer::open(container_path);
    } catch (const std::exception& e) {
        if (json_output) {
            json err;
            err["error"] = e.what();
            std::cout << err.dump(2) << '\n';
        } else {
            kLog->error("open_container_failed error={}", e.what());
        }
        return 1;
    }

    const auto& all_records = container.records();
    const auto& warnings = container.warnings();
    auto        all_names = container.names();

    // Build the view to display — filtered or full
    std::vector<const d2res::Record*> view;
    view.reserve(all_records.size());
    for (const auto& rec : all_records) {
        if (!special_only || (!rec.name.empty() && rec.name[0] == '-'))
            view.push_back(&rec);
    }

    if (json_output) {
        json out;
        out["container"] = container_path;
        out["format"] = "MQDB";
        if (special_only)
            out["filtered"] = "special";
        json recs = json::array();
        for (const auto* rec : view) {
            json r;
            r["index"] = rec->index;
            r["name"] = record_name_or_unnamed(*rec);
            r["size"] = rec->realFileSize;
            r["id"] = rec->id;
            recs.push_back(r);
        }
        out["records"] = std::move(recs);
        out["warnings"] = warnings;
        std::cout << out.dump(2) << '\n';
        return 0;
    }

    // Human-readable output
    std::cout << "Container: " << container_path << '\n';
    std::cout << "Format: MQDB\n";
    std::cout << "Records: " << view.size() << '\n';
    if (!special_only)
        std::cout << "Names: " << all_names.size() << '\n';
    std::cout << "Warnings: " << warnings.size() << '\n';
    std::cout << '\n';

    for (const auto* rec : view) {
        std::printf("[%05zu] %s size=%d\n", rec->index, record_name_or_unnamed(*rec).c_str(),
                    rec->realFileSize);
    }

    if (!warnings.empty()) {
        std::cout << "\n-- Warnings --\n";
        for (const auto& w : warnings)
            std::cout << w << '\n';
    }

    return 0;
}
