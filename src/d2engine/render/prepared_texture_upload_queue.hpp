#pragma once

#include "prepared_texture_frame.hpp"

#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <utility>

namespace d2engine {

struct PreparedTextureUploadBudget {
    std::size_t max_textures = 4;
    double      max_ms = 2.0;
};

struct PreparedTextureUploadQueueStats {
    std::size_t processed = 0;
    std::size_t remaining = 0;
};

class PreparedTextureUploadQueue {
public:
    void push(PreparedTextureFrame frame) { queue_.push_back(std::move(frame)); }

    [[nodiscard]] std::size_t size() const noexcept { return queue_.size(); }
    [[nodiscard]] bool        empty() const noexcept { return queue_.empty(); }

    template <typename Upload>
    PreparedTextureUploadQueueStats process(PreparedTextureUploadBudget budget, Upload&& upload) {
        const auto  started = std::chrono::steady_clock::now();
        std::size_t processed = 0;
        while (!queue_.empty() && processed < budget.max_textures) {
            upload(queue_.front());
            queue_.pop_front();
            ++processed;
            const double elapsed_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - started)
                                          .count();
            if (elapsed_ms >= budget.max_ms) {
                break;
            }
        }
        return {.processed = processed, .remaining = queue_.size()};
    }

private:
    std::deque<PreparedTextureFrame> queue_;
};

} // namespace d2engine
