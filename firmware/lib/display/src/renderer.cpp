// firmware/lib/display/src/renderer.cpp
#include "display/renderer.h"

#include <cstdint>

#include "birthday.h"
#include "dim_schedule.h"
#include "display/led_map.h"
#include "display/palette.h"
#include "display/rainbow.h"
#include "holidays.h"
#include "time_to_words.h"
#include "word_id.h"

namespace wc::display {

namespace {

bool word_in_set(WordId w, const WordSet& s) {
    for (uint8_t i = 0; i < s.count; ++i) {
        if (s.words[i] == w) return true;
    }
    return false;
}

bool is_decor_word(WordId w) {
    return w == WordId::HAPPY || w == WordId::BIRTH ||
           w == WordId::DAY   || w == WordId::NAME;
}

// (r * bright_u8 + 127) / 255 — round-to-nearest integer division.
uint8_t scale_channel(uint8_t v, uint8_t bright_u8) {
    const uint16_t num =
        static_cast<uint16_t>(v) * static_cast<uint16_t>(bright_u8) + 127U;
    return static_cast<uint8_t>(num / 255U);
}

Rgb apply_dim(Rgb c, uint8_t bright_u8) {
    return Rgb{
        scale_channel(c.r, bright_u8),
        scale_channel(c.g, bright_u8),
        scale_channel(c.b, bright_u8),
    };
}

} // namespace

Frame render(const RenderInput& in) {
    const WordSet lit = time_to_words(in.hour, in.minute);
    const BirthdayState bs = birthday_state(
        in.month, in.day, in.hour, in.minute, in.birthday);
    const Palette palette = palette_for_date(in.year, in.month, in.day);
    const bool stale = in.seconds_since_sync > STALE_SYNC_THRESHOLD_S;
    const uint8_t bright_u8 = static_cast<uint8_t>(
        wc::brightness(in.hour, in.minute) * 255.0f + 0.5f);

    Frame frame{};  // zero-initialized (all black)
    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const WordId w = static_cast<WordId>(i);
        const bool decor = is_decor_word(w);

        // Time words come from time_to_words; decor is lit only on
        // birthdays.
        const bool lit_this_tick =
            word_in_set(w, lit) || (bs.is_birthday && decor);
        if (!lit_this_tick) continue;

        // Priority: birthday rainbow > holiday > amber > warm white.
        Rgb color;
        if (bs.is_birthday && decor) {
            color = rainbow(w, in.now_ms);
        } else if (palette != Palette::WARM_WHITE) {
            color = color_for(palette, w);
        } else if (stale) {
            color = amber();
        } else {
            color = warm_white();
        }

        frame[index_of(w)] = apply_dim(color, bright_u8);
    }
    return frame;
}

} // namespace wc::display
