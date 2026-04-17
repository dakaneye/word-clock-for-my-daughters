#include "wifi_provision/credential_validator.h"
#include "wifi_provision/tz_options.h"

namespace wc::wifi_provision {

static constexpr size_t MAX_SSID_LEN = 32;
static constexpr size_t MAX_PW_LEN = 64;

ValidationResult validate(const FormBody& body) {
    if (body.ssid.empty()) {
        return {false, "Pick a WiFi network first."};
    }
    if (body.ssid.size() > MAX_SSID_LEN) {
        return {false, "That network name is too long — max 32 characters."};
    }
    if (body.pw.size() > MAX_PW_LEN) {
        return {false, "That password is too long — max 64 characters."};
    }
    if (!is_known_posix_tz(body.tz)) {
        return {false, "Pick a timezone from the list."};
    }
    return {true, ""};
}

} // namespace wc::wifi_provision
