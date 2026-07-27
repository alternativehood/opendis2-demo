#include "d2res/wdt_reader.hpp"
#include "d2res/hexdump.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/rgba_buffer.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace d2res {

WdtResult WdtReader::open(const std::string& path, const std::string& out_dir) {
    WdtResult result;

    if (!fs::exists(path)) {
        result.ok = false;
        result.error = "not found: " + path;
        return result;
    }

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        result.ok = false;
        result.error = "cannot create output dir: " + ec.message();
        return result;
    }

    // Read first 4 bytes to detect magic
    std::ifstream f(path, std::ios::binary);
    char          magic[4]{};
    f.read(magic, 4);
    f.close();

    const std::string stem = fs::path(path).stem().string();

    if (std::string(magic, 4) != "MQDB") {
        // Non-MQDB fallback: copy raw + hexdump
        result.ok = false;
        result.format = "unknown";
        result.error = "not MQDB";

        // Copy raw
        const fs::path raw_out = fs::path(out_dir) / (stem + ".raw.bin");
        fs::copy_file(path, raw_out, fs::copy_options::overwrite_existing, ec);

        // Write hexdump
        std::ifstream        fdata(path, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(fdata)),
                                  std::istreambuf_iterator<char>());
        std::ofstream        fhex(fs::path(out_dir) / (stem + ".hexdump.txt"));
        write_hexdump(fhex, std::span<const uint8_t>(data), HexdumpOptions{.limit_bytes = 512});

        // Write manifest
        nlohmann::json mj;
        mj["source"] = path;
        mj["format"] = "unknown";
        mj["record_count"] = 0;
        mj["records"] = nlohmann::json::array();
        std::ofstream fm(fs::path(out_dir) / (stem + ".manifest.json"));
        fm << mj.dump(2) << '\n';

        return result;
    }

    // MQDB path
    try {
        MqdbContainer const container = MqdbContainer::open(path);
        result.ok = true;
        result.format = "mqdb";
        result.record_count = static_cast<int>(container.records().size());

        for (std::size_t i = 0; i < container.records().size(); ++i) {
            const auto&       rec = container.records()[i];
            const std::string safe_name =
                rec.name.empty() ? ("record_" + std::to_string(i)) : rec.name;

            const std::string fname = sanitize_filename(safe_name) + ".bin";

            const fs::path out_file = fs::path(out_dir) / fname;
            Entry          entry = container.read_record(i);
            std::ofstream  fo(out_file, std::ios::binary);
            fo.write(reinterpret_cast<const char*>(entry.payload.data()),
                     static_cast<std::streamsize>(entry.payload.size()));

            WdtRecord wr;
            wr.name = rec.name;
            wr.size = static_cast<std::size_t>(rec.size);
            wr.output_file = out_file.filename().string();
            result.records.push_back(std::move(wr));
        }

        // Write manifest
        nlohmann::json mj;
        mj["source"] = path;
        mj["format"] = "mqdb";
        mj["record_count"] = result.record_count;
        auto& recs_arr = mj["records"] = nlohmann::json::array();
        for (const auto& r : result.records) {
            nlohmann::json rj;
            rj["name"] = r.name;
            rj["size"] = r.size;
            rj["output_file"] = r.output_file;
            recs_arr.push_back(std::move(rj));
        }
        std::ofstream fm(fs::path(out_dir) / (stem + ".manifest.json"));
        fm << mj.dump(2) << '\n';

    } catch (const std::exception& ex) {
        result.ok = false;
        result.error = ex.what();
    }

    return result;
}

} // namespace d2res
