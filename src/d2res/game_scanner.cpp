#include "game_scanner.hpp"
#include "mqdb.hpp"
#include "platform/hash.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <sstream>

namespace fs = std::filesystem;

namespace d2res {

namespace {

void fill_utc_tm(std::tm& result, const std::time_t& time_value) {
#if defined(_WIN32)
    if (gmtime_s(&result, &time_value) != 0) {
        throw std::runtime_error("failed to convert timestamp to UTC");
    }
#else
    if (gmtime_r(&time_value, &result) == nullptr) {
        throw std::runtime_error("failed to convert timestamp to UTC");
    }
#endif
}

std::string infer_type(const fs::path& p) {
    const std::string ext = p.extension().string();
    if (ext == ".bik" || ext == ".BIK")
        return "bik";
    if (ext == ".wav" || ext == ".WAV")
        return "wav";
    if (ext == ".dbf" || ext == ".DBF")
        return "dbf";
    if (ext == ".dlg" || ext == ".DLG")
        return "dlg";
    if (ext == ".mft" || ext == ".MFT")
        return "mft";
    if (ext == ".dat" || ext == ".DAT")
        return "dat";
    if (ext == ".sg" || ext == ".SG")
        return "sg";
    if (ext == ".csg" || ext == ".CSG")
        return "csg";
    return "unknown";
}

std::string iso8601_now() {
    const auto        now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm           tm{};
    fill_utc_tm(tm, t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::vector<ContentKind> infer_content(const std::vector<std::string>& special_files,
                                       const std::string&              rel_path) {
    std::vector<ContentKind> result;
    bool                     has_images = false;
    bool                     has_anims = false;
    for (const auto& s : special_files) {
        if (s == "-IMAGES.OPT")
            has_images = true;
        if (s == "-ANIMS.OPT")
            has_anims = true;
    }
    if (has_images)
        result.push_back(ContentKind::Images);
    if (has_anims)
        result.push_back(ContentKind::Animations);

    const std::string lower = [&] {
        std::string lp = rel_path;
        for (char& c : lp)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return lp;
    }();

    const std::string ext = fs::path(rel_path).extension().string();
    const std::string lext = [&] {
        std::string e = ext;
        for (char& c : e)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return e;
    }();

    if (!has_images && !has_anims) {
        const bool in_sounds = lower.find("sounds/") != std::string::npos ||
                               lower.find("sounds\\") != std::string::npos;

        if (in_sounds && lext == ".wdb") {
            result.push_back(ContentKind::Sounds);
        } else if (lext == ".wdt") {
            result.push_back(ContentKind::SoundMapping);
        }
    }

    if (result.empty()) {
        if (lext == ".dbf" || lext == ".dat" || lext == ".dlg") {
            result.push_back(ContentKind::DataTables);
        } else {
            result.push_back(ContentKind::Unknown);
        }
    }
    return result;
}

} // namespace

ScanResult GameScanner::scan(const std::string& game_root) {
    ScanResult result;
    result.game_root = game_root;
    result.scan_timestamp = iso8601_now();

    std::error_code ec;
    const fs::path  root(game_root);

    for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::end(it);
         it.increment(ec)) {
        if (ec) {
            result.warnings.push_back("directory iteration error: " + ec.message());
            ec.clear();
            continue;
        }

        if (!it->is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        ++result.total_files;

        const fs::path&   abs_path = it->path();
        const std::string rel = fs::relative(abs_path, root, ec).string();
        if (ec) {
            result.warnings.push_back("cannot compute relative path for " + abs_path.string() +
                                      ": " + ec.message());
            ec.clear();
            continue;
        }
        // Validate relative path has no parent-directory traversal
        {
            const fs::path norm = fs::path(rel).lexically_normal();
            bool           safe = !norm.empty() && norm != ".";
            for (const auto& comp : norm) {
                if (comp == "..") {
                    safe = false;
                    break;
                }
            }
            if (!safe) {
                result.warnings.push_back("skipping unsafe relative path: " + rel);
                continue;
            }
        }

        // Read first 4 bytes to check MQDB magic
        bool is_mqdb = false;
        try {
            std::ifstream f(abs_path, std::ios::binary);
            if (f) {
                char magic[4] = {};
                f.read(magic, 4);
                if (f.gcount() == 4 && magic[0] == 'M' && magic[1] == 'Q' && magic[2] == 'D' &&
                    magic[3] == 'B')
                    is_mqdb = true;
            }
        } catch (...) {
            result.warnings.push_back("exception while reading magic from " + abs_path.string());
        }

        if (is_mqdb) {
            ContainerEntry entry;
            entry.path = rel;
            entry.size = it->file_size(ec);
            ec.clear();
            entry.sha256 = platform::sha256_file(abs_path.string());

            try {
                auto container = MqdbContainer::open(abs_path);
                entry.record_count = static_cast<int>(container.records().size());
                for (const auto& rec : container.records()) {
                    if (!rec.name.empty() && rec.name[0] == '-')
                        entry.special_files.push_back(rec.name);
                }
                // deduplicate special_files
                std::ranges::sort(entry.special_files);
                auto surplus = std::ranges::unique(entry.special_files);
                entry.special_files.erase(surplus.begin(), surplus.end());
            } catch (const std::exception& e) {
                result.warnings.push_back("failed to open MQDB " + rel + ": " + e.what());
            }

            entry.likely_content = infer_content(entry.special_files, rel);
            result.containers.push_back(std::move(entry));
        } else {
            OtherFileEntry entry;
            entry.path = rel;
            entry.size = it->file_size(ec);
            ec.clear();
            entry.sha256 = platform::sha256_file(abs_path.string());
            entry.type = infer_type(abs_path);
            if (entry.type == "dbf" || entry.type == "dat" || entry.type == "dlg") {
                entry.likely_content = {ContentKind::DataTables};
            } else {
                entry.likely_content = {ContentKind::Unknown};
            }
            result.other_files.push_back(std::move(entry));
        }
    }

    std::ranges::sort(result.containers, [](const ContainerEntry& a, const ContainerEntry& b) {
        return a.path < b.path;
    });
    std::ranges::sort(result.other_files, [](const OtherFileEntry& a, const OtherFileEntry& b) {
        return a.path < b.path;
    });

    return result;
}

nlohmann::json ScanResult::to_json() const {
    // Sort copies so output is deterministic regardless of insertion order
    auto sorted_containers = containers;
    auto sorted_other = other_files;
    std::ranges::sort(sorted_containers, [](const ContainerEntry& a, const ContainerEntry& b) {
        return a.path < b.path;
    });
    std::ranges::sort(sorted_other, [](const OtherFileEntry& a, const OtherFileEntry& b) {
        return a.path < b.path;
    });

    nlohmann::json containers_j = nlohmann::json::array();
    for (const auto& c : sorted_containers) {
        nlohmann::json sf = nlohmann::json::array();
        for (const auto& s : c.special_files)
            sf.push_back(s);
        nlohmann::json lc = nlohmann::json::array();
        for (ContentKind const k : c.likely_content)
            lc.push_back(to_string(k));
        containers_j.push_back({{"path", c.path},
                                {"size", c.size},
                                {"sha256", c.sha256},
                                {"record_count", c.record_count},
                                {"special_files", sf},
                                {"likely_content", lc}});
    }

    nlohmann::json other_j = nlohmann::json::array();
    for (const auto& o : sorted_other) {
        nlohmann::json lc = nlohmann::json::array();
        for (ContentKind const k : o.likely_content)
            lc.push_back(to_string(k));
        other_j.push_back({{"path", o.path},
                           {"size", o.size},
                           {"sha256", o.sha256},
                           {"type", o.type},
                           {"likely_content", lc}});
    }

    nlohmann::json warnings_j = nlohmann::json::array();
    for (const auto& w : warnings)
        warnings_j.push_back(w);

    return {{"game_root", game_root},     {"scan_timestamp", scan_timestamp},
            {"total_files", total_files}, {"mqdb_containers", containers_j},
            {"other_files", other_j},     {"warnings", warnings_j}};
}

} // namespace d2res
