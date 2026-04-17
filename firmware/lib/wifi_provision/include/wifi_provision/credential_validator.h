#pragma once

#include <string>
#include "wifi_provision/form_parser.h"

namespace wc::wifi_provision {

struct ValidationResult {
    bool ok;
    std::string message; // empty on success; human-readable reason on failure
};

ValidationResult validate(const FormBody& body);

} // namespace wc::wifi_provision
