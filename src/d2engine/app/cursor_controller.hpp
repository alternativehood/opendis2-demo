#pragma once

#include "screen.hpp"

#include <SDL3/SDL.h>

#include <optional>

namespace d2engine {

class CursorController {
public:
    CursorController() = default;
    ~CursorController();

    CursorController(const CursorController&) = delete;
    CursorController& operator=(const CursorController&) = delete;
    CursorController(CursorController&&) = delete;
    CursorController& operator=(CursorController&&) = delete;

    void set_cursors(SDL_Cursor* default_cursor, SDL_Cursor* select_unit);

    [[nodiscard]] bool activate();

    void set_kind(CursorKind kind);
    void deactivate();

    [[nodiscard]] std::optional<CursorKind> current_kind() const { return current_kind_; }

private:
    bool apply_kind(CursorKind kind, bool force);

    SDL_Cursor*               default_cursor_ = nullptr;
    SDL_Cursor*               select_unit_cursor_ = nullptr;
    std::optional<CursorKind> current_kind_;
    bool                      active_ = false;
};

} // namespace d2engine
