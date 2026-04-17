// firmware/lib/display/src/palette.cpp
#include "display/palette.h"

#include <array>
#include <cstddef>

namespace wc::display {

namespace {

// Cycle a WordId through a fixed-size color array by enum index.
// Decoupled from PCB LED chain order — palette behavior survives
// any future PCB LED reorder without test changes.
template <std::size_t N>
Rgb pick(const std::array<Rgb, N>& colors, WordId w) {
    return colors[static_cast<std::size_t>(w) % N];
}

} // namespace

Rgb warm_white() { return Rgb{255, 170, 100}; }
Rgb amber()      { return Rgb{255, 120,  30}; }

Rgb color_for(Palette p, WordId w) {
    switch (p) {
        case Palette::WARM_WHITE:
            return warm_white();
        case Palette::MLK_PURPLE:
            return Rgb{128, 0, 180};
        case Palette::VALENTINES: {
            static constexpr std::array<Rgb, 2> cs = {{
                {255,  30,  60},
                {255, 120, 160},
            }};
            return pick(cs, w);
        }
        case Palette::WOMEN_PURPLE:
            return Rgb{150, 50, 180};
        case Palette::EARTH_DAY: {
            static constexpr std::array<Rgb, 2> cs = {{
                { 30, 140,  60},
                { 30,  80, 200},
            }};
            return pick(cs, w);
        }
        case Palette::EASTER_PASTEL: {
            static constexpr std::array<Rgb, 4> cs = {{
                {255, 180, 200},
                {180, 220, 255},
                {220, 255, 180},
                {255, 230, 180},
            }};
            return pick(cs, w);
        }
        case Palette::JUNETEENTH: {
            static constexpr std::array<Rgb, 3> cs = {{
                {200,  30,  30},
                { 10,  10,  10},
                { 30, 160,  60},
            }};
            return pick(cs, w);
        }
        case Palette::INDIGENOUS: {
            static constexpr std::array<Rgb, 3> cs = {{
                {160,  80,  40},
                {200, 140,  80},
                {120,  60,  30},
            }};
            return pick(cs, w);
        }
        case Palette::HALLOWEEN: {
            static constexpr std::array<Rgb, 2> cs = {{
                {255, 100,   0},
                {140,   0, 180},
            }};
            return pick(cs, w);
        }
        case Palette::CHRISTMAS: {
            static constexpr std::array<Rgb, 2> cs = {{
                {220,  30,  30},
                { 30, 160,  60},
            }};
            return pick(cs, w);
        }
    }
    // -Wswitch-enum + complete handling makes this unreachable.
    return warm_white();
}

} // namespace wc::display
