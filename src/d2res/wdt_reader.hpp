#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace d2res {

struct WdtRecord {
    std::string name;
    std::size_t size{};
    std::string output_file;
};

struct WdtResult {
    bool                   ok{false};
    std::string            error;
    std::string            format; // "mqdb" or "unknown"
    int                    record_count{};
    std::vector<WdtRecord> records;
};

class WdtReader {
public:
    [[nodiscard]] static WdtResult open(const std::string& path, const std::string& out_dir);
};

} // namespace d2res
