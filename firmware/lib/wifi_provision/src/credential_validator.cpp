#include "wifi_provision/credential_validator.h"
#include "wifi_provision/tz_options.h"

namespace wc::wifi_provision {

static constexpr size_t MAX_SSID_LEN = 32;
static constexpr size_t MAX_PW_LEN = 64;

ValidationResult validate(const FormBody& body) {
    if (body.ssid.empty()) {
        return {false, "SSID is required."};
    }
    if (body.ssid.size() > MAX_SSID_LEN) {
        return {false, "SSID must be 32 characters or fewer."};
    }
    if (body.pw.size() > MAX_PW_LEN) {
        return {false, "Password must be 64 characters or fewer."};
    }
    if (!is_known_posix_tz(body.tz)) {
        return {false, "Please pick a timezone from the list."};
    }
    return {true, ""};
}

} // namespace wc::wifi_provision
