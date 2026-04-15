#!/usr/bin/env bash
# Regenerate the golden files for time_to_words. Run manually when the canonical
# output is intentionally changed; review the diff before committing.
set -euo pipefail

cd "$(dirname "$0")/.."  # firmware/

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

cat > "$TMPDIR/main.cpp" <<'CPP'
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "time_to_words.h"

using namespace wc;

static const char* word_name(WordId w) {
    switch (w) {
        case WordId::IT: return "IT";
        case WordId::IS: return "IS";
        case WordId::TEN_MIN: return "TEN";
        case WordId::HALF: return "HALF";
        case WordId::QUARTER: return "QUARTER";
        case WordId::TWENTY: return "TWENTY";
        case WordId::FIVE_MIN: return "FIVE_MIN";
        case WordId::MINUTES: return "MINUTES";
        case WordId::PAST: return "PAST";
        case WordId::TO: return "TO";
        case WordId::ONE: return "ONE";
        case WordId::TWO: return "TWO";
        case WordId::THREE: return "THREE";
        case WordId::FOUR: return "FOUR";
        case WordId::FIVE_HR: return "FIVE_HR";
        case WordId::SIX: return "SIX";
        case WordId::SEVEN: return "SEVEN";
        case WordId::EIGHT: return "EIGHT";
        case WordId::NINE: return "NINE";
        case WordId::TEN_HR: return "TEN_HR";
        case WordId::ELEVEN: return "ELEVEN";
        case WordId::TWELVE: return "TWELVE";
        case WordId::OCLOCK: return "OCLOCK";
        case WordId::NOON: return "NOON";
        case WordId::IN: return "IN";
        case WordId::THE: return "THE";
        case WordId::AT: return "AT";
        case WordId::MORNING: return "MORNING";
        case WordId::AFTERNOON: return "AFTERNOON";
        case WordId::EVENING: return "EVENING";
        case WordId::NIGHT: return "NIGHT";
        case WordId::HAPPY: return "HAPPY";
        case WordId::BIRTH: return "BIRTH";
        case WordId::DAY: return "DAY";
        case WordId::NAME: return "NAME";
        case WordId::COUNT: return "?";
    }
    return "?";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: gen <output-file>\n");
        return 1;
    }
    FILE* f = fopen(argv[1], "w");
    if (!f) { perror("fopen"); return 1; }
    for (int mins = 0; mins < 1440; ++mins) {
        uint8_t h = mins / 60;
        uint8_t m = mins % 60;
        WordSet ws = time_to_words(h, m);
        fprintf(f, "%02u:%02u", h, m);
        for (uint8_t i = 0; i < ws.count; ++i) {
            fprintf(f, " %s", word_name(ws.words[i]));
        }
        fprintf(f, "\n");
    }
    fclose(f);
    return 0;
}
CPP

c++ -std=gnu++17 -Wall -Wextra -I lib/core/include \
    "$TMPDIR/main.cpp" lib/core/src/time_to_words.cpp \
    -o "$TMPDIR/gen"

mkdir -p test/test_time_to_words_golden
"$TMPDIR/gen" test/test_time_to_words_golden/emory_golden.txt
cp test/test_time_to_words_golden/emory_golden.txt test/test_time_to_words_golden/nora_golden.txt

echo "Wrote:"
wc -l test/test_time_to_words_golden/emory_golden.txt \
      test/test_time_to_words_golden/nora_golden.txt
