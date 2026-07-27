#include "d2res/image_comparer.hpp"
#include "d2res/mqdb.hpp"
#include <algorithm>
#include <filesystem>
#include <map>
#include <regex>
#include <set>

#include <lodepng.h>

namespace fs = std::filesystem;

namespace d2res {

// ---- helpers ----------------------------------------------------------------

namespace {

struct CandidatePair {
    std::string container_name; // e.g. "BatUnits.ff"
    int         record_index{};
    int         frame_index{};
    std::string record_name;
    std::string expected_path;
};

// Decode PNG file to RGBA8. Returns empty vector on error.
std::vector<uint8_t> decode_png(const std::string& path, unsigned& w, unsigned& h) {
    std::vector<uint8_t> out;
    const unsigned       err = lodepng::decode(out, w, h, path);
    if (err != 0u) {
        w = 0;
        h = 0;
        return {};
    }
    return out;
}

// Write a diff PNG: red channel = 255 where pixels differ, black elsewhere.
void write_diff_png(const std::string& path, const std::vector<uint8_t>& a,
                    const std::vector<uint8_t>& b, unsigned w, unsigned h) {
    std::vector<uint8_t> diff(static_cast<std::size_t>(w) * h * 4U, 0);
    const std::size_t    n = static_cast<std::size_t>(w) * h * 4U;
    for (std::size_t i = 0; i < n; i += 4) {
        bool const differ =
            (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2] || a[i + 3] != b[i + 3]);
        if (differ)
            diff[i] = 255; // red
    }
    lodepng::encode(path, diff, w, h);
}

std::string lowercase(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

// ---- CompareReport ----------------------------------------------------------

nlohmann::json CompareReport::to_json() const {
    nlohmann::json j;
    j["total_compared"] = total_compared;
    j["passed"] = passed;
    j["failed"] = failed;
    j["skipped"] = skipped;

    auto& sl = j["sample_list"] = nlohmann::json::array();
    for (const auto& e : sample_list) {
        nlohmann::json ej;
        ej["container"] = e.container;
        ej["record_index"] = e.record_index;
        ej["name"] = e.name;
        sl.push_back(std::move(ej));
    }

    auto& fa = j["failures"] = nlohmann::json::array();
    for (const auto& f : failures) {
        nlohmann::json fj;
        fj["name"] = f.name;
        fj["container"] = f.container;
        fj["expected_path"] = f.expected_path;
        fj["actual_path"] = f.actual_path;
        fj["dims_expected"] = f.dims_expected;
        fj["dims_actual"] = f.dims_actual;
        fj["first_diff_pixel"] = f.first_diff_pixel;
        fj["total_diff_pixels"] = f.total_diff_pixels;
        fj["pct_diff"] = f.pct_diff;
        fa.push_back(std::move(fj));
    }
    return j;
}

// ---- ImageComparer::run -----------------------------------------------------

CompareReport ImageComparer::run(const std::string& actual_dir, const std::string& expected_dir,
                                 int sample_n, bool all_mode, const std::string& diff_dir,
                                 const std::string& game_dir) {
    CompareReport report;

    if (!fs::is_directory(expected_dir) || !fs::is_directory(actual_dir)) {
        return report;
    }

    // Create diff dir if specified
    if (!diff_dir.empty()) {
        std::error_code ec;
        fs::create_directories(diff_dir, ec);
    }

    // Collect all candidate pairs from the expected dataset.
    // Structure: <expected_dir>/<ContainerSubdir>/<record_idx>/ImageMap#<idx>#<frame>.png
    // OR: <expected_dir>/Imgs/<Container.ff>/<idx>/ImageMap#<idx>#<frame>.png
    std::vector<CandidatePair> candidates;

    // Walk expected_dir recursively looking for ImageMap#...#....png files
    const std::regex imgmap_re(R"(ImageMap#(\d+)#(\d+)\.png)", std::regex::icase);
    for (const auto& entry : fs::recursive_directory_iterator(expected_dir)) {
        if (!entry.is_regular_file())
            continue;
        const std::string fname = entry.path().filename().string();
        std::smatch       m;
        if (!std::regex_match(fname, m, imgmap_re))
            continue;

        const int idx = std::stoi(m[1].str());
        const int frame = std::stoi(m[2].str());

        // Container name is two levels up: .../Imgs/BatUnits.ff/<idx>/ImageMap...
        // Or one level up if no intermediate: .../BatUnits.ff/<idx>/ImageMap...
        // Use parent's parent directory name as container
        const fs::path    parent = entry.path().parent_path();  // <idx> dir
        const fs::path    container_dir = parent.parent_path(); // ContainerX.ff
        const std::string container_name = container_dir.filename().string();

        CandidatePair cp;
        cp.container_name = container_name;
        cp.record_index = idx;
        cp.frame_index = frame;
        cp.expected_path = entry.path().string();
        candidates.push_back(std::move(cp));
    }

    if (candidates.empty())
        return report;

    // Sort: containers present in actual_dir first (for sane sample_n behaviour),
    // then by container/record_index/frame for determinism within each group.
    const auto actual_base_exists = [&](const std::string& cn) {
        if (fs::is_directory(fs::path(actual_dir) / cn))
            return true;
        return fs::is_directory(fs::path(actual_dir) / fs::path(cn).stem().string());
    };
    std::ranges::stable_sort(candidates, [&](const CandidatePair& a, const CandidatePair& b) {
        const int ea = actual_base_exists(a.container_name) ? 1 : 0;
        const int eb = actual_base_exists(b.container_name) ? 1 : 0;
        if (ea != eb)
            return ea > eb;
        if (a.container_name != b.container_name)
            return lowercase(a.container_name) < lowercase(b.container_name);
        if (a.record_index != b.record_index)
            return a.record_index < b.record_index;
        return a.frame_index < b.frame_index;
    });

    // Build idx→name maps per container using MQDB if game_dir is provided.
    // Map: lowercase(container_name) → { record_index → record_name }
    std::map<std::string, std::map<int, std::string>> idx_to_name;
    if (!game_dir.empty()) {
        // Walk expected_dir structure to find which containers are referenced
        std::set<std::string> container_names_seen;
        for (const auto& cp : candidates)
            container_names_seen.insert(lowercase(cp.container_name));

        // Search for matching .ff/.wdb files under game_dir
        for (const auto& cn : container_names_seen) {
            for (const auto& entry : fs::recursive_directory_iterator(game_dir)) {
                if (!entry.is_regular_file())
                    continue;
                if (lowercase(entry.path().filename().string()) == cn) {
                    try {
                        MqdbContainer const c = MqdbContainer::open(entry.path());
                        auto&               m = idx_to_name[cn];
                        for (std::size_t i = 0; i < c.records().size(); ++i)
                            m[static_cast<int>(i)] = c.records()[i].name;
                    } catch (...) {
                    }
                    break;
                }
            }
        }
    }

    // Apply sample limit
    const std::size_t limit = (!all_mode && sample_n > 0)
                                  ? std::min(static_cast<std::size_t>(sample_n), candidates.size())
                                  : candidates.size();

    for (std::size_t ci = 0; ci < limit; ++ci) {
        const auto& cp = candidates[ci];

        SampleEntry se;
        se.container = cp.container_name;
        se.record_index = cp.record_index;
        se.name = cp.record_name.empty()
                      ? (cp.container_name + "/" + std::to_string(cp.record_index))
                      : cp.record_name;
        report.sample_list.push_back(se);

        // Find actual PNG.
        // actual_dir/<container_name>/<name>.png (extract-images output)
        const std::string container_stem = fs::path(cp.container_name).stem().string();
        // Try both with and without .ff extension in actual dir
        fs::path actual_base = fs::path(actual_dir) / cp.container_name;
        if (!fs::is_directory(actual_base))
            actual_base = fs::path(actual_dir) / container_stem;

        const std::string idx_str = std::to_string(cp.record_index);
        const std::string frame_str = [&] {
            std::string s = std::to_string(cp.frame_index);
            while (s.size() < 3)
                s.insert(0U, 1U, '0');
            return s;
        }();

        fs::path actual_path;
        // Try name-based lookup if we have the idx→name map
        const auto cn_key = lowercase(cp.container_name);
        if (static_cast<unsigned int>(idx_to_name.contains(cn_key)) != 0u) {
            const auto& name_map = idx_to_name.at(cn_key);
            auto        it = name_map.find(cp.record_index);
            if (it != name_map.end() && !it->second.empty()) {
                const std::string& rec_name = it->second;
                // Try: <base>/<name>/frame_NNN.png (animation frames)
                actual_path = actual_base / rec_name / ("frame_" + frame_str + ".png");
                if (!fs::exists(actual_path)) {
                    // Record names may already include .PNG extension
                    const std::string lc_name = lowercase(rec_name);
                    const bool        has_img_ext =
                        (lc_name.size() > 4 && lc_name.substr(lc_name.size() - 4) == ".png");
                    if (has_img_ext) {
                        actual_path = actual_base / rec_name;
                    } else {
                        actual_path = actual_base / (rec_name + ".png");
                    }
                }
            }
        }
        // Fallback: indexed naming (same as reference)
        if (actual_path.empty() || !fs::exists(actual_path))
            actual_path = actual_base / idx_str / ("frame_" + frame_str + ".png");
        if (!fs::exists(actual_path))
            actual_path = actual_base / (idx_str + ".png");
        if (!fs::exists(actual_path)) {
            ++report.skipped;
            continue;
        }

        unsigned   ew{};
        unsigned   eh{};
        unsigned   aw{};
        unsigned   ah{};
        const auto exp_px = decode_png(cp.expected_path, ew, eh);
        const auto act_px = decode_png(actual_path.string(), aw, ah);

        if (exp_px.empty() || act_px.empty()) {
            ++report.skipped;
            continue;
        }

        CompareResult cr;
        cr.name = se.name;
        cr.container = cp.container_name;
        cr.expected_path = cp.expected_path;
        cr.actual_path = actual_path.string();
        cr.dims_expected = {static_cast<int>(ew), static_cast<int>(eh)};
        cr.dims_actual = {static_cast<int>(aw), static_cast<int>(ah)};

        ++report.total_compared;

        if (ew != aw || eh != ah) {
            cr.total_diff_pixels = static_cast<int>(ew * eh);
            cr.pct_diff = 100.0;
            ++report.failed;
            report.failures.push_back(cr);
            continue;
        }

        int               diff_count = 0;
        bool              found_first = false;
        const std::size_t npx = static_cast<std::size_t>(ew) * static_cast<std::size_t>(eh) * 4U;
        for (std::size_t pi = 0; pi < npx; pi += 4) {
            if (exp_px[pi] != act_px[pi] || exp_px[pi + 1] != act_px[pi + 1] ||
                exp_px[pi + 2] != act_px[pi + 2] || exp_px[pi + 3] != act_px[pi + 3]) {
                ++diff_count;
                if (!found_first) {
                    const int px_idx = static_cast<int>(pi / 4);
                    cr.first_diff_pixel = {px_idx % static_cast<int>(ew),
                                           px_idx / static_cast<int>(ew)};
                    found_first = true;
                }
            }
        }

        if (diff_count == 0) {
            ++report.passed;
        } else {
            cr.total_diff_pixels = diff_count;
            cr.pct_diff = 100.0 * diff_count / static_cast<double>(ew * eh);
            ++report.failed;
            report.failures.push_back(cr);

            if (!diff_dir.empty()) {
                std::string diff_name = container_stem;
                diff_name += '_';
                diff_name += idx_str;
                diff_name += '_';
                diff_name += frame_str;
                diff_name += ".diff.png";
                write_diff_png((fs::path(diff_dir) / diff_name).string(), exp_px, act_px, ew, eh);
            }
        }
    }

    return report;
}

} // namespace d2res
