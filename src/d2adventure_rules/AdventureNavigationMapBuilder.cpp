#include "AdventureNavigationMapBuilder.hpp"

#include <d2runtime/AdventureGroundClassifier.hpp>
#include <d2runtime/AdventureTerrainDecoder.hpp>

#include <limits>
#include <utility>

namespace d2adventure {

namespace {

void add_diagnostic(AdventureNavigationMapBuildResult& result,
                    AdventureNavigationDiagnosticKind kind, d2runtime::MapCellCoord cell,
                    std::string object_id, std::string conflicting_object_id, std::string message) {
    result.diagnostics.push_back(
        {kind, cell, std::move(object_id), std::move(conflicting_object_id), std::move(message)});
}

} // namespace

bool is_adventure_navigation_error(AdventureNavigationDiagnosticKind kind) {
    switch (kind) {
    case AdventureNavigationDiagnosticKind::InvalidMapDimensions:
    case AdventureNavigationDiagnosticKind::TerrainDimensionsMismatch:
    case AdventureNavigationDiagnosticKind::TerrainTileCountMismatch:
    case AdventureNavigationDiagnosticKind::BlockingObjectMissingFootprint:
    case AdventureNavigationDiagnosticKind::BlockingObjectFootprintOutOfBounds:
    case AdventureNavigationDiagnosticKind::StackOutOfBounds:
    case AdventureNavigationDiagnosticKind::OverlappingStacks:
    case AdventureNavigationDiagnosticKind::StackOnBlockingObject:
        return true;
    case AdventureNavigationDiagnosticKind::RoadOutOfBounds:
    case AdventureNavigationDiagnosticKind::OverlappingBlockingObjects:
        return false;
    }
    return true;
}

std::size_t AdventureNavigationMapBuildResult::error_count() const {
    std::size_t count = 0;
    for (const auto& diagnostic : diagnostics)
        count += is_adventure_navigation_error(diagnostic.kind) ? 1u : 0u;
    return count;
}

std::size_t AdventureNavigationMapBuildResult::warning_count() const {
    return diagnostics.size() - error_count();
}

bool AdventureNavigationMapBuildResult::success() const {
    return map.has_value() && error_count() == 0;
}

AdventureNavigationMapBuildResult
AdventureNavigationMapBuilder::build(const d2runtime::AdventureWorldState& world) const {
    AdventureNavigationMapBuildResult result;
    if (world.map_width <= 0 || world.map_height <= 0) {
        add_diagnostic(result, AdventureNavigationDiagnosticKind::InvalidMapDimensions, {}, {}, {},
                       "invalid map dimensions: width=" + std::to_string(world.map_width) +
                           ", height=" + std::to_string(world.map_height) +
                           "; both must be positive");
        return result;
    }
    if (world.terrain.width != world.map_width || world.terrain.height != world.map_height) {
        add_diagnostic(result, AdventureNavigationDiagnosticKind::TerrainDimensionsMismatch, {}, {},
                       {},
                       "terrain dimensions mismatch: world=" + std::to_string(world.map_width) +
                           "x" + std::to_string(world.map_height) +
                           ", terrain=" + std::to_string(world.terrain.width) + "x" +
                           std::to_string(world.terrain.height));
        return result;
    }

    const auto width = static_cast<std::size_t>(world.map_width);
    const auto height = static_cast<std::size_t>(world.map_height);
    const auto actual_tile_count = world.terrain.tiles.size();
    if (height > std::numeric_limits<std::size_t>::max() / width) {
        add_diagnostic(result, AdventureNavigationDiagnosticKind::TerrainTileCountMismatch, {}, {},
                       {},
                       "terrain tile count mismatch: actual=" + std::to_string(actual_tile_count) +
                           ", expected size overflows for map " + std::to_string(world.map_width) +
                           "x" + std::to_string(world.map_height));
        return result;
    }
    const auto expected_tile_count = width * height;
    if (actual_tile_count != expected_tile_count) {
        add_diagnostic(
            result, AdventureNavigationDiagnosticKind::TerrainTileCountMismatch, {}, {}, {},
            "terrain tile count mismatch: actual=" + std::to_string(actual_tile_count) +
                ", expected=" + std::to_string(expected_tile_count) + " for map " +
                std::to_string(world.map_width) + "x" + std::to_string(world.map_height));
        return result;
    }

    std::vector<AdventureNavigationCell>     cells(width * height);
    const d2runtime::AdventureTerrainDecoder decoder;
    for (std::size_t index = 0; index < cells.size(); ++index) {
        const auto descriptor = decoder.decode_tile(world.terrain.tiles[index].raw_value);
        cells[index].ground = d2runtime::classify_adventure_ground(descriptor);
    }
    AdventureNavigationMap map(world.map_width, world.map_height, std::move(cells));

    for (const auto& road : world.roads) {
        if (!map.contains(road.position)) {
            add_diagnostic(result, AdventureNavigationDiagnosticKind::RoadOutOfBounds,
                           road.position, road.id, {},
                           "road " + road.id + " is outside the navigation map");
            continue;
        }
        map.cells_[static_cast<std::size_t>(road.position.y) * width +
                   static_cast<std::size_t>(road.position.x)]
            .has_road = true;
    }

    for (const auto& object : world.map_objects) {
        if (!object.blocking || object.kind == d2runtime::AdventureMapObjectKind::Stack)
            continue;
        if (object.footprint.empty()) {
            add_diagnostic(result,
                           AdventureNavigationDiagnosticKind::BlockingObjectMissingFootprint,
                           object.position, object.id, {},
                           "blocking object " + object.id + " has an empty footprint");
            continue;
        }
        for (const auto cell : object.footprint) {
            if (!map.contains(cell)) {
                add_diagnostic(
                    result, AdventureNavigationDiagnosticKind::BlockingObjectFootprintOutOfBounds,
                    cell, object.id, {},
                    "blocking object " + object.id + " footprint cell is outside the map");
                continue;
            }
            auto& navigation_cell = map.cells_[static_cast<std::size_t>(cell.y) * width +
                                               static_cast<std::size_t>(cell.x)];
            if (navigation_cell.blocking_object_id.has_value() &&
                navigation_cell.blocking_object_id.value() != object.id) {
                add_diagnostic(result,
                               AdventureNavigationDiagnosticKind::OverlappingBlockingObjects, cell,
                               object.id, navigation_cell.blocking_object_id.value(),
                               "blocking objects " + object.id + " and " +
                                   navigation_cell.blocking_object_id.value() + " overlap");
                continue;
            }
            navigation_cell.blocking_object_id = object.id;
        }
    }

    for (const auto& stack : world.stacks) {
        if (!d2runtime::is_stack_on_adventure_map(stack))
            continue;
        if (!map.contains(stack.position)) {
            add_diagnostic(result, AdventureNavigationDiagnosticKind::StackOutOfBounds,
                           stack.position, stack.id, {},
                           "stack " + stack.id + " is outside the navigation map");
            continue;
        }
        auto& navigation_cell = map.cells_[static_cast<std::size_t>(stack.position.y) * width +
                                           static_cast<std::size_t>(stack.position.x)];
        if (navigation_cell.occupying_stack_id.has_value() &&
            navigation_cell.occupying_stack_id.value() != stack.id) {
            add_diagnostic(result, AdventureNavigationDiagnosticKind::OverlappingStacks,
                           stack.position, stack.id, navigation_cell.occupying_stack_id.value(),
                           "stacks " + stack.id + " and " +
                               navigation_cell.occupying_stack_id.value() + " overlap");
        } else {
            navigation_cell.occupying_stack_id = stack.id;
        }
        if (navigation_cell.blocking_object_id.has_value()) {
            add_diagnostic(result, AdventureNavigationDiagnosticKind::StackOnBlockingObject,
                           stack.position, stack.id, navigation_cell.blocking_object_id.value(),
                           "stack " + stack.id + " is on blocking object " +
                               navigation_cell.blocking_object_id.value());
        }
    }

    if (result.error_count() == 0)
        result.map = std::move(map);
    return result;
}

} // namespace d2adventure
