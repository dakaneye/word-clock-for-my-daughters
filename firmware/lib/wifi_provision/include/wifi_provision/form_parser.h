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
//
// Status: the production web_server reads form fields via WebServer::arg()
// directly (the Arduino-ESP32 core URL-decodes them). This function is not
// called from production code today. It remains because its 12 Unity tests
// document correct URL-decoder semantics — useful reference if a future
// code path ever parses a raw body (REST-ish endpoint, captive-portal
// variant with a different content type, etc.).
FormBody parse_form_body(const std::string& body);

} // namespace wc::wifi_provision
