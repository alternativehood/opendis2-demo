#include "glob.hpp"
#include <cctype>
#include <string>

#ifndef _WIN32
#include <fnmatch.h>
#endif

namespace d2res::platform {

#ifdef _WIN32
namespace {
bool glob_match_impl(std::string_view pat, std::string_view str) {
    // Simple recursive * / ? matcher with case-folding.
    if (pat.empty()) {
        return str.empty();
    }
    if (pat[0] == '*') {
        // Skip consecutive stars.
        while (pat.size() > 1 && pat[1] == '*') {
            pat.remove_prefix(1);
        }
        pat.remove_prefix(1);
        for (std::size_t i = 0; i <= str.size(); ++i) {
            if (glob_match_impl(pat, str.substr(i))) {
                return true;
            }
        }
        return false;
    }
    if (str.empty()) {
        return false;
    }
    bool const char_match = (pat[0] == '?') || (std::tolower(static_cast<unsigned char>(pat[0])) ==
                                                std::tolower(static_cast<unsigned char>(str[0])));
    return char_match && glob_match_impl(pat.substr(1), str.substr(1));
}
} // namespace
#endif

bool glob_match(std::string_view pattern, std::string_view name) {
    if (pattern.empty()) {
        return name.empty();
    }
#ifdef _WIN32
    return glob_match_impl(pattern, name);
#else
    std::string const pat(pattern);
    std::string const nam(name);
    return fnmatch(pat.c_str(), nam.c_str(), FNM_CASEFOLD) == 0;
#endif
}

} // namespace d2res::platform
