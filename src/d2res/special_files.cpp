#include "special_files.hpp"
#include <string_view>

namespace d2res {

SpecialFileType classify_record(const Record& rec) {
    const std::string& name = rec.name;
    if (name.empty() || name[0] != '-')
        return SpecialFileType::NOT_SPECIAL;

    if (name == "-INDEX.OPT")
        return SpecialFileType::INDEX_OPT;
    if (name == "-IMAGES.OPT")
        return SpecialFileType::IMAGES_OPT;
    if (name == "-ANIMS.OPT")
        return SpecialFileType::ANIMS_OPT;

    auto dot = name.rfind('.');
    if (dot != std::string::npos) {
        std::string_view const ext(name.data() + dot, name.size() - dot);
        if (ext == ".OPT")
            return SpecialFileType::UNKNOWN_OPT;
        if (ext == ".DAT")
            return SpecialFileType::UNKNOWN_DAT;
    }

    return SpecialFileType::UNKNOWN_SPECIAL;
}

} // namespace d2res
