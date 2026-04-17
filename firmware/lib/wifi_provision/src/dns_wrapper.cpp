// firmware/lib/wifi_provision/src/dns_wrapper.cpp
// Guarded with #ifdef ARDUINO because PlatformIO's LDF pulls library
// sources into the native test build even with build_src_filter; the
// Arduino-only DNSServer / IPAddress headers would otherwise break the
// native suite. Same pattern as nvs_store.cpp in Task 8.
#ifdef ARDUINO

#include <DNSServer.h>
#include <IPAddress.h>

namespace wc::wifi_provision::dns_hijack {

static DNSServer& server() {
    static DNSServer s;
    return s;
}

// Responds to every DNS query with the ESP32's SoftAP IP (192.168.4.1),
// which is what triggers iOS captive-portal auto-popup.
void begin(const IPAddress& ap_ip) {
    server().setErrorReplyCode(DNSReplyCode::NoError);
    server().start(53, "*", ap_ip);
}

void loop() {
    server().processNextRequest();
}

void stop() {
    server().stop();
}

} // namespace wc::wifi_provision::dns_hijack

#endif // ARDUINO
