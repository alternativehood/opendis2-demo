#include "cursor_controller.hpp"

#include <d2log/log.hpp>

namespace d2engine {

namespace {
auto kLog = d2log::get("d2.csr"); // NOLINT
} // namespace

CursorController::~CursorController() {
    deactivate();
    if (select_unit_cursor_)
        SDL_DestroyCursor(select_unit_cursor_);
    if (default_cursor_)
        SDL_DestroyCursor(default_cursor_);
}

void CursorController::set_cursors(SDL_Cursor* default_cursor, SDL_Cursor* select_unit) {
    default_cursor_ = default_cursor;
    select_unit_cursor_ = select_unit;
}

bool CursorController::activate() {
    if (active_)
        return true;
    if (!default_cursor_) {
        kLog->error("cursor_activate_failed reason=no_default_cursor");
        return false;
    }

    active_ = true;
    const bool applied = apply_kind(CursorKind::Default, /*force=*/true);
    if (!applied) {
        active_ = false;
        return false;
    }

    if (!SDL_ShowCursor()) {
        kLog->warn("cursor_show_failed error={}", SDL_GetError());
    }

    kLog->info("cursor_activated");
    return true;
}

void CursorController::set_kind(CursorKind kind) {
    if (!active_)
        return;
    apply_kind(kind, /*force=*/false);
}

bool CursorController::apply_kind(CursorKind kind, bool force) {
    if (!force && current_kind_ == kind)
        return true;

    SDL_Cursor* target = nullptr;
    switch (kind) {
    case CursorKind::SelectUnit:
        target = select_unit_cursor_;
        break;
    case CursorKind::Default:
        target = default_cursor_;
        break;
    }
    if (!target)
        return false;

    if (!SDL_SetCursor(target)) {
        kLog->warn("cursor_set_kind_failed kind={} error={}", static_cast<int>(kind),
                   SDL_GetError());
        return false;
    }
    current_kind_ = kind;
    return true;
}

void CursorController::deactivate() {
    if (!active_)
        return;
    // Restore system default before destroying our cursors
    SDL_SetCursor(nullptr);
    SDL_ShowCursor();
    current_kind_.reset();
    active_ = false;
}

} // namespace d2engine
