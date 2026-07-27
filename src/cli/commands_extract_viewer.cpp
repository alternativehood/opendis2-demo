#include "commands_extract_viewer.hpp"
#include <d2log/log.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

// ---- base64 encoder (RFC 4648, no line-wrapping) ----------------------

static const char B64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = (i + 1 < data.size()) ? data[i + 1] : 0u;
        const uint32_t b2 = (i + 2 < data.size()) ? data[i + 2] : 0u;
        const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
        out += B64_CHARS[(triple >> 18) & 0x3F];
        out += B64_CHARS[(triple >> 12) & 0x3F];
        out += (i + 1 < data.size()) ? B64_CHARS[(triple >> 6) & 0x3F] : '=';
        out += (i + 2 < data.size()) ? B64_CHARS[(triple >> 0) & 0x3F] : '=';
    }
    return out;
}

static std::vector<uint8_t> read_file_bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

// ---- HTML generation --------------------------------------------------

static std::string html_escape(const std::string& s) {
    std::string out;
    for (char const c : s) {
        switch (c) {
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '&':
            out += "&amp;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

int cmd_extract_viewer(const std::string& src_dir, const std::string& out_html) {
    if (!fs::is_directory(src_dir)) {
        kLog->error("source_directory_not_found path={}", src_dir);
        return 1;
    }

    // Collect assets recursively
    struct Asset {
        fs::path    path;
        std::string rel;
    };
    std::vector<Asset> images;
    std::vector<Asset> gifs;
    std::vector<Asset> wavs;
    std::vector<Asset> atlases;

    for (auto it = fs::recursive_directory_iterator(src_dir); it != fs::end(it); ++it) {
        if (!it->is_regular_file())
            continue;
        const fs::path&   p = it->path();
        const std::string ext = p.extension().string();
        const std::string rel = fs::relative(p, src_dir).string();
        const std::string stem = p.stem().string();

        // atlas PNGs (atlas_NNN.png)
        if (ext == ".png" && stem.starts_with("atlas_")) {
            atlases.push_back({.path = p, .rel = rel});
        } else if (ext == ".png") {
            images.push_back({.path = p, .rel = rel});
        } else if (ext == ".gif") {
            gifs.push_back({.path = p, .rel = rel});
        } else if (ext == ".wav") {
            wavs.push_back({.path = p, .rel = rel});
        }
    }

    std::ranges::sort(images, [](const Asset& a, const Asset& b) { return a.rel < b.rel; });
    std::ranges::sort(gifs, [](const Asset& a, const Asset& b) { return a.rel < b.rel; });
    std::ranges::sort(wavs, [](const Asset& a, const Asset& b) { return a.rel < b.rel; });
    std::ranges::sort(atlases, [](const Asset& a, const Asset& b) { return a.rel < b.rel; });

    // Open output file early — write directly, no full-buffer accumulation
    std::error_code ec;
    fs::create_directories(fs::path(out_html).parent_path(), ec);
    std::ofstream f(out_html);
    if (!f) {
        kLog->error("cannot_write_output path={}", out_html);
        return 1;
    }

    f << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>D2 Asset Viewer</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: monospace; background: #1a1a1a; color: #ccc; padding: 16px; }
h1 { color: #eee; margin-bottom: 12px; font-size: 1.2em; }
h2 { color: #aaa; margin: 20px 0 8px; font-size: 1em; border-bottom: 1px solid #333; padding-bottom: 4px; }
.grid { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 20px; }
.card { background: #2a2a2a; border: 1px solid #333; border-radius: 4px; padding: 6px; max-width: 200px; text-align: center; }
.card img { max-width: 180px; max-height: 180px; display: block; margin: 0 auto 4px; image-rendering: pixelated; background: #111; }
.card .name { font-size: 0.65em; color: #888; word-break: break-all; }
.audio-row { display: flex; align-items: center; gap: 8px; margin-bottom: 6px; }
.audio-row .name { font-size: 0.75em; color: #888; min-width: 200px; }
audio { height: 28px; }
.atlas-card { background: #2a2a2a; border: 1px solid #444; border-radius: 4px; padding: 8px; margin-bottom: 12px; }
.atlas-card img { max-width: 100%; display: block; }
.atlas-card .name { font-size: 0.7em; color: #888; margin-bottom: 4px; }
.stats { color: #666; font-size: 0.75em; margin-bottom: 16px; }
</style>
</head>
<body>
<h1>D2 Asset Viewer</h1>
<div class="stats">)";

    f << images.size() << " images &nbsp;|&nbsp; " << gifs.size() << " animations &nbsp;|&nbsp; "
      << atlases.size() << " atlases &nbsp;|&nbsp; " << wavs.size() << " sounds</div>\n";

    // Images section
    if (!images.empty()) {
        f << "<h2>Images (" << images.size() << ")</h2>\n<div class=\"grid\">\n";
        for (const auto& a : images) {
            const auto bytes = read_file_bytes(a.path);
            if (bytes.empty())
                continue;
            f << R"(<div class="card"><img src="data:image/png;base64,)" << base64_encode(bytes)
              << R"(" loading="lazy"><div class="name">)" << html_escape(a.rel) << "</div></div>\n";
        }
        f << "</div>\n";
    }

    // Animations section
    if (!gifs.empty()) {
        f << "<h2>Animations (" << gifs.size() << ")</h2>\n<div class=\"grid\">\n";
        for (const auto& a : gifs) {
            const auto bytes = read_file_bytes(a.path);
            if (bytes.empty())
                continue;
            f << R"(<div class="card"><img src="data:image/gif;base64,)" << base64_encode(bytes)
              << R"(" loading="lazy"><div class="name">)" << html_escape(a.rel) << "</div></div>\n";
        }
        f << "</div>\n";
    }

    // Atlas section
    if (!atlases.empty()) {
        f << "<h2>Atlas Sheets (" << atlases.size() << ")</h2>\n";
        for (const auto& a : atlases) {
            const auto bytes = read_file_bytes(a.path);
            if (bytes.empty())
                continue;
            f << R"(<div class="atlas-card"><div class="name">)" << html_escape(a.rel)
              << "</div><img src=\"data:image/png;base64," << base64_encode(bytes) << "\"></div>\n";
        }
    }

    // Audio section
    if (!wavs.empty()) {
        f << "<h2>Sounds (" << wavs.size() << ")</h2>\n";
        for (const auto& a : wavs) {
            const auto bytes = read_file_bytes(a.path);
            if (bytes.empty())
                continue;
            f << R"(<div class="audio-row"><span class="name">)" << html_escape(a.rel) << "</span>"
              << "<audio controls src=\"data:audio/wav;base64," << base64_encode(bytes)
              << "\"></audio></div>\n";
        }
    }

    f << "</body>\n</html>\n";

    const auto file_size = static_cast<std::size_t>(f.tellp());
    kLog->info("wrote_viewer out={} size_kb={}", out_html, file_size / 1024);
    return 0;
}
