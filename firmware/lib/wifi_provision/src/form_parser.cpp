#include "wifi_provision/form_parser.h"

namespace wc::wifi_provision {

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static std::string url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '+') {
            out.push_back(' ');
        } else if (c == '%' && i + 2 < s.size()) {
            int hi = hex_digit(s[i+1]);
            int lo = hex_digit(s[i+2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(c);
            }
        } else {
            out.push_back(c);
        }
    }
    return out;
}

FormBody parse_form_body(const std::string& body) {
    FormBody out;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t amp = body.find('&', pos);
        if (amp == std::string::npos) amp = body.size();
        size_t eq = body.find('=', pos);
        if (eq == std::string::npos || eq > amp) {
            pos = amp + 1;
            continue;
        }
        std::string key = body.substr(pos, eq - pos);
        std::string val = url_decode(body.substr(eq + 1, amp - (eq + 1)));
        if (key == "ssid") out.ssid = val;
        else if (key == "pw") out.pw = val;
        else if (key == "tz") out.tz = val;
        else if (key == "csrf") out.csrf = val;
        pos = amp + 1;
    }
    return out;
}

} // namespace wc::wifi_provision
