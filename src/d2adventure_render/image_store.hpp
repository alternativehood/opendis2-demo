#pragma once

#include <d2res/rgba_buffer.hpp>

#include <memory>
#include <optional>
#include <string_view>

namespace d2engine {

// ── IImageStore: abstract interface for image asset access ───────────────────
//
// Minimal interface covering the needs of AdventureTerrainSurfaceComposer
// and other consumer classes. Enables test fakes without real .ff files.
class IImageStore {
public:
    virtual ~IImageStore() = default;

    // Decode a raw PNG record. Returns immutable shared decoded buffer.
    // Returns nullptr on missing or decode failure.
    [[nodiscard]] virtual std::shared_ptr<const d2res::RgbaBuffer>
    raw_png(std::string_view container, std::string_view record) const = 0;

    // Explicit mutable copy (rarely needed; prefer raw_png for immutable access).
    [[nodiscard]] virtual std::optional<d2res::RgbaBuffer>
    copy_raw_png(std::string_view container, std::string_view record) const = 0;

protected:
    IImageStore() = default;
    IImageStore(const IImageStore&) = default;
    IImageStore(IImageStore&&) = default;
    IImageStore& operator=(const IImageStore&) = default;
    IImageStore& operator=(IImageStore&&) = default;
};

} // namespace d2engine
