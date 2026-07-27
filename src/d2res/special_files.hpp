#pragma once
#include "mqdb.hpp"

namespace d2res {

enum class SpecialFileType : uint8_t {
    NOT_SPECIAL,
    INDEX_OPT,
    IMAGES_OPT,
    ANIMS_OPT,
    UNKNOWN_OPT,
    UNKNOWN_DAT,
    UNKNOWN_SPECIAL,
};

SpecialFileType classify_record(const Record& rec);

} // namespace d2res
