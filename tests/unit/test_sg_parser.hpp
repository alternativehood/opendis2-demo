#pragma once

#include <d2scenario/SgParser.hpp>
#include <d2scenario/SgTypes.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct SgTestFixture : public ::testing::Test {
    std::string sg_data_dir() const {
        const char* env = std::getenv("OPENDIS2_SG_TEST_DIR"); // NOLINT(concurrency-mt-unsafe)
        if (env)
            return env;
        const char* source = std::getenv("OPENDIS2_SOURCE_DIR"); // NOLINT(concurrency-mt-unsafe)
        if (source) {
            fs::path p = fs::path(source) / "Downloads";
            if (fs::exists(p))
                return p.string();
        }
        return (fs::path(std::string(__FILE__)).parent_path().parent_path().parent_path() /
                "Downloads")
            .string();
    }

    std::vector<uint8_t> read_file(const std::string& filename) {
        fs::path full = fs::path(sg_data_dir()) / filename;
        if (!fs::exists(full))
            return {};
        std::size_t          sz = static_cast<std::size_t>(fs::file_size(full));
        std::vector<uint8_t> data(sz);
        std::ifstream        ifs(full, std::ios::binary);
        if (!ifs.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(sz)))
            return {};
        return data;
    }

    bool has_dosgen() const {
        return fs::exists(fs::path(sg_data_dir()) / "Кошмар Сэра Доргенвилля.sg");
    }

    std::string dosgen_filename() const { return "Кошмар Сэра Доргенвилля.sg"; }

    std::string defeated_filename() const { return "The_DEFEATED_III_Alt_Mod_v1.2.1.1.sg"; }
};