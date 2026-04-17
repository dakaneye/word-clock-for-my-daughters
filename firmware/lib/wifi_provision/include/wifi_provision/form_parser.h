#pragma once

#include <string>

namespace wc::wifi_provision {

struct FormBody {
    std::string ssid;
    std::string pw;
    std::string tz;
    std::string csrf;
};

// Parses an application/x-www-form-urlencoded body.
// Unknown fields are ignored. Missing known fields are returned as empty strings.
// Decodes '+' → ' ' and %XX hex escapes. Malformed %XX yields the literal chars.
FormBody parse_form_body(const std::string& body);

} // namespace wc::wifi_provision
