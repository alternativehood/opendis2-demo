#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

namespace test_dbf {

class DbfBuilder {
public:
    struct Field {
        std::string name;
        char        type = 'C'; // 'C' for character, 'N' for numeric
        int         length = 1;
    };

    DbfBuilder(std::initializer_list<Field> fields) : fields_(fields) {}

    void add_record(const std::vector<std::string>& values) { records_.push_back(values); }

    void write(const std::filesystem::path& path) const {
        std::vector<uint8_t> buf;

        auto write16 = [&](uint16_t v) {
            buf.push_back(static_cast<uint8_t>(v & 0xff));
            buf.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
        };
        auto write32 = [&](uint32_t v) {
            buf.push_back(static_cast<uint8_t>(v & 0xff));
            buf.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
            buf.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
            buf.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
        };

        int record_size = 1; // deletion marker
        for (const auto& f : fields_)
            record_size += f.length;

        int header_size = 32 + static_cast<int>(fields_.size()) * 32 + 1; // +1 for terminator

        buf.push_back(0x03); // version
        buf.push_back(24);   // YY
        buf.push_back(1);    // MM
        buf.push_back(1);    // DD
        write32(static_cast<uint32_t>(records_.size()));
        write16(static_cast<uint16_t>(header_size));
        write16(static_cast<uint16_t>(record_size));
        buf.resize(buf.size() + 20, 0); // reserved

        int disp = 0;
        for (const auto& f : fields_) {
            for (int i = 0; i < 11; ++i) {
                buf.push_back(static_cast<std::size_t>(i) < f.name.size()
                                  ? static_cast<uint8_t>(f.name[static_cast<std::size_t>(i)])
                                  : 0);
            }
            buf.push_back(static_cast<uint8_t>(f.type));
            write32(static_cast<uint32_t>(disp));
            buf.push_back(static_cast<uint8_t>(f.length));
            buf.push_back(0);
            buf.resize(buf.size() + 14, 0);
            disp += f.length;
        }

        buf.push_back(0x0D); // terminator

        for (const auto& rec : records_) {
            buf.push_back(0x20); // active record marker
            for (std::size_t fi = 0; fi < fields_.size(); ++fi) {
                const auto& f = fields_[fi];
                std::string val;
                if (fi < rec.size())
                    val = rec[fi];
                if (val.size() > static_cast<std::size_t>(f.length))
                    val.resize(static_cast<std::size_t>(f.length));
                val.resize(static_cast<std::size_t>(f.length), ' ');
                for (char c : val)
                    buf.push_back(static_cast<uint8_t>(c));
            }
        }

        buf.push_back(0x1A); // EOF marker

        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(buf.data()),
                  static_cast<std::streamsize>(buf.size()));
    }

private:
    std::vector<Field>                    fields_;
    std::vector<std::vector<std::string>> records_;
};

} // namespace test_dbf
