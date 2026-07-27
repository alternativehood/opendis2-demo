#include <gtest/gtest.h>

#include "d2engine/render/prepared_texture_upload_queue.hpp"

#include <string>
#include <utility>
#include <vector>

namespace d2engine {
namespace {

PreparedTextureFrame frame(std::string name) {
    return {.container_path = "Imgs/Test.ff",
            .image_name = std::move(name),
            .rgba = {1, 2, 3, 4},
            .width = 1,
            .height = 1};
}

} // namespace

TEST(PreparedTextureUploadQueue, MaxTextureBudgetLeavesRemainderQueued) {
    PreparedTextureUploadQueue queue;
    queue.push(frame("A"));
    queue.push(frame("B"));
    queue.push(frame("C"));

    std::vector<std::string> uploaded;
    const auto               stats = queue.process(
        {.max_textures = 2, .max_ms = 1000.0},
        [&](const PreparedTextureFrame& prepared) { uploaded.push_back(prepared.image_name); });

    EXPECT_EQ(stats.processed, 2u);
    EXPECT_EQ(stats.remaining, 1u);
    EXPECT_EQ(queue.size(), 1u);
    EXPECT_EQ(uploaded, (std::vector<std::string>{"A", "B"}));
}

} // namespace d2engine
