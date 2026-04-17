#pragma once

#include <array>
#include <cstdint>

namespace wc::display {

struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr bool operator==(const Rgb& a, const Rgb& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

constexpr bool operator!=(const Rgb& a, const Rgb& b) {
    return !(a == b);
}

inline constexpr uint8_t LED_COUNT = 35;

using Frame = std::array<Rgb, LED_COUNT>;

} // namespace wc::display
