#pragma once

#include <cstdint>
#include <functional>
#include <optional>

namespace d2engine {

// Strongly-typed identifier wrappers to prevent accidental mixing of different ID kinds.
// Each wraps a uint32_t with explicit construction and no implicit conversion.

struct UnitInstanceId {
    uint32_t value = 0;

    constexpr explicit UnitInstanceId(uint32_t v) noexcept : value(v) {}
    constexpr UnitInstanceId() noexcept = default;

    [[nodiscard]] constexpr bool operator==(UnitInstanceId other) const noexcept {
        return value == other.value;
    }
    [[nodiscard]] constexpr bool operator!=(UnitInstanceId other) const noexcept {
        return value != other.value;
    }
};

struct VisualEntityId {
    uint32_t value = 0;

    constexpr explicit VisualEntityId(uint32_t v) noexcept : value(v) {}
    constexpr VisualEntityId() noexcept = default;

    [[nodiscard]] constexpr bool operator==(VisualEntityId other) const noexcept {
        return value == other.value;
    }
    [[nodiscard]] constexpr bool operator!=(VisualEntityId other) const noexcept {
        return value != other.value;
    }
};

struct TrackId {
    uint32_t value = 0;

    constexpr explicit TrackId(uint32_t v) noexcept : value(v) {}
    constexpr TrackId() noexcept = default;

    [[nodiscard]] constexpr bool operator==(TrackId other) const noexcept {
        return value == other.value;
    }
    [[nodiscard]] constexpr bool operator!=(TrackId other) const noexcept {
        return value != other.value;
    }
};

struct AttackInstanceId {
    uint32_t value = 0;

    constexpr explicit AttackInstanceId(uint32_t v) noexcept : value(v) {}
    constexpr AttackInstanceId() noexcept = default;

    [[nodiscard]] constexpr bool operator==(AttackInstanceId other) const noexcept {
        return value == other.value;
    }
    [[nodiscard]] constexpr bool operator!=(AttackInstanceId other) const noexcept {
        return value != other.value;
    }
};

struct EffectInstanceId {
    uint32_t value = 0;

    constexpr explicit EffectInstanceId(uint32_t v) noexcept : value(v) {}
    constexpr EffectInstanceId() noexcept = default;

    [[nodiscard]] constexpr bool operator==(EffectInstanceId other) const noexcept {
        return value == other.value;
    }
    [[nodiscard]] constexpr bool operator!=(EffectInstanceId other) const noexcept {
        return value != other.value;
    }
};

struct UnitVisualProfileId {
    uint32_t value = 0;

    constexpr explicit UnitVisualProfileId(uint32_t v) noexcept : value(v) {}
    constexpr UnitVisualProfileId() noexcept = default;

    [[nodiscard]] constexpr bool operator==(UnitVisualProfileId other) const noexcept {
        return value == other.value;
    }
    [[nodiscard]] constexpr bool operator!=(UnitVisualProfileId other) const noexcept {
        return value != other.value;
    }
};

struct UnitLifecycleVisualProfileId {
    uint32_t value = 0;

    constexpr explicit UnitLifecycleVisualProfileId(uint32_t v) noexcept : value(v) {}
    constexpr UnitLifecycleVisualProfileId() noexcept = default;

    [[nodiscard]] constexpr bool operator==(UnitLifecycleVisualProfileId other) const noexcept {
        return value == other.value;
    }
    [[nodiscard]] constexpr bool operator!=(UnitLifecycleVisualProfileId other) const noexcept {
        return value != other.value;
    }
};

} // namespace d2engine

template <> struct std::hash<d2engine::UnitInstanceId> {
    std::size_t operator()(d2engine::UnitInstanceId id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};
