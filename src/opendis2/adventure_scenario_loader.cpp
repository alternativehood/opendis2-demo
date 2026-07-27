#include "adventure_scenario_loader.hpp"

#include <d2log/log.hpp>
#include <d2scenario/SgParser.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace opendis2 {

namespace {

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return data;
}

} // namespace

std::unique_ptr<ScenarioLoadResult>
// cppcheck-suppress unusedFunction
AdventureScenarioLoader::load_semantic(
    const std::string&                          scenario_path,
    d2game::AdventureUnitMovementProfileCatalog movement_profiles) {
    // 1. Read SG file
    auto raw = read_file(scenario_path);
    if (raw.empty()) {
        d2log::get("d2.adventure")->error("cannot_read_scenario_file path={}", scenario_path);
        return nullptr;
    }

    // 2. Parse
    d2scenario::SgParser parser(raw);
    auto                 parse_result = parser.parse();

    for (const auto& w : parse_result.parse_warnings) {
        d2log::get("d2.app")->warn("scenario_parse_warning: {}", w);
    }

    // 3. Build world state
    d2runtime::AdventureWorldBuilder builder;
    auto                             build_result = builder.build(parse_result.scenario);

    for (const auto& diag : build_result.diagnostics) {
        d2log::get("d2.app")->info("build_diagnostic kind={} msg={} oid={} cls={}",
                                   static_cast<int>(diag.kind), diag.message, diag.object_id,
                                   diag.object_class);
    }

    const auto warn_count = build_result.warning_count();
    const auto err_count = build_result.error_count();

    // 4. Create GameSession — only when no build errors
    std::unique_ptr<d2game::GameSession> session;
    if (err_count == 0) {
        session = std::make_unique<d2game::GameSession>(std::move(build_result.world), warn_count,
                                                        err_count, std::move(movement_profiles));
    }

    // 5. Return result
    auto result = std::make_unique<ScenarioLoadResult>();
    result->session = std::move(session);
    result->build_diagnostics = std::move(build_result.diagnostics);
    result->parsed_stack_count = parse_result.scenario.stacks.size();
    result->warning_count = warn_count;
    result->error_count = err_count;
    result->success = err_count == 0;

    return result;
}

} // namespace opendis2
