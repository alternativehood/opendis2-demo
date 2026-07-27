#pragma once

#include <d2game/GameSession.hpp>

#include <memory>

namespace opendis2 {

class HeadlessFrontend {
public:
    explicit HeadlessFrontend(d2game::GameSession& session);

    int run();

private:
    d2game::GameSession& session_;
};

} // namespace opendis2
