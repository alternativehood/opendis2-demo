#include "terrain_calibration_exporter.hpp"

#include <d2buildinfo/build_info.hpp>
#include <d2log/log.hpp>

#include <CLI/CLI.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        d2terrain_calibration::ExportConfig config;
        CLI::App                            app{"opendis2-dev-terrain-calibration-export"};
        app.set_version_flag("--version", d2buildinfo::format_build_version());
        app.add_option("--maps-dir", config.maps_dir)->required();
        app.add_option("--game-root", config.game_root)->required();
        app.add_option("--out-dir", config.out_dir);
        app.add_option("--apex-display-x", config.apex_display_origin.x);
        app.add_option("--apex-display-y", config.apex_display_origin.y);
        app.parse(argc, argv);
        d2log::init({});
        return d2terrain_calibration::run_export(config, std::cout);
    } catch (const std::exception& e) {
        std::cout << "fatal: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
