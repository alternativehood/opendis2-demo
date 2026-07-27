#include <gtest/gtest.h>
#include <filesystem>

#include "cli/commands_extract_all.hpp"

namespace fs = std::filesystem;

TEST(ExtractAll, NonexistentGameDirReturnsError) {
    const int r = cmd_extract_all("/nonexistent/game/dir",
                                  (fs::temp_directory_path() / "d2_extract_all_unit").string());
    EXPECT_NE(r, 0);
}
