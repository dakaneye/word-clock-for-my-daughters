#pragma once

#include <string>
#include <vector>

namespace wc::wifi_provision {

struct TzOption {
    std::string label; // human-readable dropdown display
    std::string posix; // POSIX TZ string for setenv("TZ", ...)
};

const std::vector<TzOption>& tz_options();
bool is_known_posix_tz(const std::string& posix);

} // namespace wc::wifi_provision
