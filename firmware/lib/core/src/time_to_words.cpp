// firmware/lib/core/src/time_to_words.cpp
#include "time_to_words.h"

namespace wc {

namespace {

enum class TimeOfDay : uint8_t {
    MORNING,
    AFTERNOON,
    EVENING,
    NIGHT,
    NOON_EXACT,
    MIDNIGHT_EXACT,
};

WordId hour_word(uint8_t h12) {
    switch (h12) {
        case 1:  return WordId::ONE;
        case 2:  return WordId::TWO;
        case 3:  return WordId::THREE;
        case 4:  return WordId::FOUR;
        case 5:  return WordId::FIVE_HR;
        case 6:  return WordId::SIX;
        case 7:  return WordId::SEVEN;
        case 8:  return WordId::EIGHT;
        case 9:  return WordId::NINE;
        case 10: return WordId::TEN_HR;
        case 11: return WordId::ELEVEN;
        case 12: return WordId::TWELVE;
        default: return WordId::TWELVE;
    }
}

uint8_t to_12h(uint8_t h24) {
    uint8_t h = h24 % 12;
    return h == 0 ? 12 : h;
}

TimeOfDay classify(uint8_t h24, uint8_t m5) {
    if (h24 == 12 && m5 == 0) return TimeOfDay::NOON_EXACT;
    if (h24 == 0  && m5 == 0) return TimeOfDay::MIDNIGHT_EXACT;
    if (h24 >= 5  && h24 < 12) return TimeOfDay::MORNING;
    if (h24 >= 12 && h24 < 17) return TimeOfDay::AFTERNOON;
    if (h24 >= 17 && h24 < 21) return TimeOfDay::EVENING;
    return TimeOfDay::NIGHT; // 21..23 or 0..4
}

void push(WordSet& ws, WordId id) {
    ws.words[ws.count++] = id;
}

void push_suffix(WordSet& ws, TimeOfDay tod) {
    switch (tod) {
        case TimeOfDay::MORNING:
            push(ws, WordId::IN);
            push(ws, WordId::THE);
            push(ws, WordId::MORNING);
            break;
        case TimeOfDay::AFTERNOON:
            push(ws, WordId::IN);
            push(ws, WordId::THE);
            push(ws, WordId::AFTERNOON);
            break;
        case TimeOfDay::EVENING:
            push(ws, WordId::IN);
            push(ws, WordId::THE);
            push(ws, WordId::EVENING);
            break;
        case TimeOfDay::NIGHT:
            push(ws, WordId::AT);
            push(ws, WordId::NIGHT);
            break;
        case TimeOfDay::NOON_EXACT:
        case TimeOfDay::MIDNIGHT_EXACT:
            // Handled as special early-return cases in time_to_words.
            break;
    }
}

} // namespace

WordSet time_to_words(uint8_t hour24, uint8_t minute) {
    WordSet ws{};
    ws.count = 0;

    uint8_t m5 = static_cast<uint8_t>(minute - (minute % 5));
    uint8_t block = static_cast<uint8_t>(m5 / 5);

    push(ws, WordId::IT);
    push(ws, WordId::IS);

    TimeOfDay tod = classify(hour24, m5);

    // Special exact cases (only when block == 0).
    if (tod == TimeOfDay::NOON_EXACT) {
        push(ws, WordId::TWELVE);
        push(ws, WordId::NOON);
        return ws;
    }
    if (tod == TimeOfDay::MIDNIGHT_EXACT) {
        push(ws, WordId::TWELVE);
        push(ws, WordId::AT);
        push(ws, WordId::NIGHT);
        return ws;
    }

    // Determine hour to announce.
    uint8_t announced_h24;
    if (block <= 6) {
        announced_h24 = hour24;
    } else {
        announced_h24 = static_cast<uint8_t>((hour24 + 1) % 24);
    }
    WordId h_word = hour_word(to_12h(announced_h24));

    switch (block) {
        case 0: // :00
            push(ws, h_word);
            push(ws, WordId::OCLOCK);
            break;
        case 1: // :05
            push(ws, WordId::FIVE_MIN);
            push(ws, WordId::MINUTES);
            push(ws, WordId::PAST);
            push(ws, h_word);
            break;
        case 2: // :10
            push(ws, WordId::TEN_MIN);
            push(ws, WordId::MINUTES);
            push(ws, WordId::PAST);
            push(ws, h_word);
            break;
        case 3: // :15
            push(ws, WordId::QUARTER);
            push(ws, WordId::PAST);
            push(ws, h_word);
            break;
        case 4: // :20
            push(ws, WordId::TWENTY);
            push(ws, WordId::MINUTES);
            push(ws, WordId::PAST);
            push(ws, h_word);
            break;
        case 5: // :25
            push(ws, WordId::TWENTY);
            push(ws, WordId::FIVE_MIN);
            push(ws, WordId::MINUTES);
            push(ws, WordId::PAST);
            push(ws, h_word);
            break;
        case 6: // :30
            push(ws, WordId::HALF);
            push(ws, WordId::PAST);
            push(ws, h_word);
            break;
        case 7: // :35
            push(ws, WordId::TWENTY);
            push(ws, WordId::FIVE_MIN);
            push(ws, WordId::MINUTES);
            push(ws, WordId::TO);
            push(ws, h_word);
            break;
        case 8: // :40
            push(ws, WordId::TWENTY);
            push(ws, WordId::MINUTES);
            push(ws, WordId::TO);
            push(ws, h_word);
            break;
        case 9: // :45
            push(ws, WordId::QUARTER);
            push(ws, WordId::TO);
            push(ws, h_word);
            break;
        case 10: // :50
            push(ws, WordId::TEN_MIN);
            push(ws, WordId::MINUTES);
            push(ws, WordId::TO);
            push(ws, h_word);
            break;
        case 11: // :55
            push(ws, WordId::FIVE_MIN);
            push(ws, WordId::MINUTES);
            push(ws, WordId::TO);
            push(ws, h_word);
            break;
        default:
            break;
    }

    // Suffix classification reflects the current wall-clock period-of-day.
    // NOON_EXACT/MIDNIGHT_EXACT are already handled via early return above, so
    // here tod is always one of MORNING/AFTERNOON/EVENING/NIGHT.
    push_suffix(ws, tod);

    return ws;
}

} // namespace wc
