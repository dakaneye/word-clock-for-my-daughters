// firmware/lib/core/src/holidays.cpp
#include "holidays.h"

namespace wc {

namespace {

// Zeller-based day-of-week. Returns 0=Sun..6=Sat.
int day_of_week(uint16_t y, uint8_t m, uint8_t d) {
    int month = m, year = y;
    if (month < 3) { month += 12; year -= 1; }
    int K = year % 100;
    int J = year / 100;
    int h = (d + (13 * (month + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
    // Zeller: 0=Saturday..6=Friday. Remap to 0=Sunday..6=Saturday.
    return (h + 6) % 7;
}

// Nth occurrence of a given weekday in (year, month). dow: 0=Sun..6=Sat. n: 1-based.
uint8_t nth_weekday_of_month(uint16_t year, uint8_t month, int dow, int n) {
    int first_dow = day_of_week(year, month, 1);
    int offset = (dow - first_dow + 7) % 7;
    return (uint8_t)(1 + offset + (n - 1) * 7);
}

// Meeus/Jones/Butcher Gregorian Easter algorithm.
void gregorian_easter(uint16_t year, uint8_t& month, uint8_t& day) {
    int a = year % 19;
    int b = year / 100;
    int c = year % 100;
    int d = b / 4;
    int e = b % 4;
    int f = (b + 8) / 25;
    int g = (b - f + 1) / 3;
    int h = (19 * a + b - d - g + 15) % 30;
    int i = c / 4;
    int k = c % 4;
    int L = (32 + 2 * e + 2 * i - h - k) % 7;
    int m = (a + 11 * h + 22 * L) / 451;
    int month_i = (h + L - 7 * m + 114) / 31;
    int day_i = ((h + L - 7 * m + 114) % 31) + 1;
    month = (uint8_t)month_i;
    day = (uint8_t)day_i;
}

} // namespace

Palette palette_for_date(uint16_t year, uint8_t month, uint8_t day) {
    // Fixed-date holidays (highest priority among fixed).
    if (month == 2  && day == 14) return Palette::VALENTINES;
    if (month == 3  && day == 8)  return Palette::WOMEN_PURPLE;
    if (month == 4  && day == 22) return Palette::EARTH_DAY;
    if (month == 6  && day == 19) return Palette::JUNETEENTH;
    if (month == 10 && day == 31) return Palette::HALLOWEEN;
    if (month == 12 && day == 25) return Palette::CHRISTMAS;

    // MLK Day: 3rd Monday of January.
    if (month == 1) {
        uint8_t mlk = nth_weekday_of_month(year, 1, 1, 3);
        if (day == mlk) return Palette::MLK_PURPLE;
    }

    // Indigenous Peoples' Day: 2nd Monday of October.
    if (month == 10) {
        uint8_t ipd = nth_weekday_of_month(year, 10, 1, 2);
        if (day == ipd) return Palette::INDIGENOUS;
    }

    // Easter: computed.
    uint8_t em, ed;
    gregorian_easter(year, em, ed);
    if (month == em && day == ed) return Palette::EASTER_PASTEL;

    return Palette::WARM_WHITE;
}

} // namespace wc
