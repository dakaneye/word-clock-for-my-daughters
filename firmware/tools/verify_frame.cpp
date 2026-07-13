// firmware/tools/verify_frame.cpp
// Host-side frame oracle: renders a frame with the SAME pure-logic sources
// the firmware ships (lib/core + lib/display) and prints the lit LED
// indices in the exact format the BENCH_SIM telemetry logs
// ("idx=0,1,38,..."). bench_acceptance.py compiles this on demand and
// diffs it against the live board's telemetry — a full-stack check that
// the assembled clock renders what the tested logic says it should.
//
// Usage:
//   verify_frame YEAR MONTH DAY HOUR MINUTE [BMONTH BDAY BHOUR BMIN]
//
// seconds_since_sync is fixed at 0 (fresh sync) — staleness changes only
// color, never which LEDs are lit, so the index oracle is tint-agnostic.
#include <cstdio>
#include <cstdlib>

#include "display/renderer.h"

int main(int argc, char** argv) {
    if (argc != 6 && argc != 10) {
        std::fprintf(stderr,
                     "usage: %s YEAR MONTH DAY HOUR MINUTE "
                     "[BMONTH BDAY BHOUR BMIN]\n", argv[0]);
        return 2;
    }
    wc::display::RenderInput in{};
    in.year   = static_cast<uint16_t>(std::atoi(argv[1]));
    in.month  = static_cast<uint8_t>(std::atoi(argv[2]));
    in.day    = static_cast<uint8_t>(std::atoi(argv[3]));
    in.hour   = static_cast<uint8_t>(std::atoi(argv[4]));
    in.minute = static_cast<uint8_t>(std::atoi(argv[5]));
    in.now_ms = 0;
    in.seconds_since_sync = 0;
    if (argc == 10) {
        in.birthday = {static_cast<uint8_t>(std::atoi(argv[6])),
                       static_cast<uint8_t>(std::atoi(argv[7])),
                       static_cast<uint8_t>(std::atoi(argv[8])),
                       static_cast<uint8_t>(std::atoi(argv[9]))};
    }

    const wc::display::Frame f = wc::display::render(in);
    std::printf("idx=");
    for (uint8_t i = 0; i < wc::display::LED_COUNT; ++i) {
        const auto& c = f[i];
        if (c.r || c.g || c.b) std::printf("%u,", i);
    }
    std::printf("\n");
    return 0;
}
