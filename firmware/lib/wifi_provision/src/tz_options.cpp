#include "wifi_provision/tz_options.h"

namespace wc::wifi_provision {

const std::vector<TzOption>& tz_options() {
    static const std::vector<TzOption> opts = {
        {"Pacific (Los Angeles, Seattle)",       "PST8PDT,M3.2.0,M11.1.0"},
        {"Mountain (Denver, Salt Lake City)",    "MST7MDT,M3.2.0,M11.1.0"},
        {"Arizona (Phoenix, no DST)",            "MST7"},
        {"Central (Chicago, Dallas)",            "CST6CDT,M3.2.0,M11.1.0"},
        {"Eastern (New York, Miami)",            "EST5EDT,M3.2.0,M11.1.0"},
        {"Alaska (Anchorage)",                   "AKST9AKDT,M3.2.0,M11.1.0"},
        {"Hawaii (Honolulu)",                    "HST10"},
        {"UTC",                                  "UTC0"},
    };
    return opts;
}

bool is_known_posix_tz(const std::string& posix) {
    for (const auto& opt : tz_options()) {
        if (opt.posix == posix) return true;
    }
    return false;
}

} // namespace wc::wifi_provision
