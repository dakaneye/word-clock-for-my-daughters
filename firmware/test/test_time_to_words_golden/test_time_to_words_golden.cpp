#include <unity.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "time_to_words.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

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

static void format_line(uint8_t h, uint8_t m, const WordSet& ws, char* buf, size_t buflen) {
    int n = snprintf(buf, buflen, "%02u:%02u", h, m);
    for (uint8_t i = 0; i < ws.count && n > 0 && (size_t)n < buflen - 1; ++i) {
        n += snprintf(buf + n, buflen - (size_t)n, " %s", word_name(ws.words[i]));
    }
}

static void run_golden_check(const char* golden_rel) {
    char path[1024];
    const char* slash = strrchr(__FILE__, '/');
    size_t dir_len = slash ? (size_t)(slash - __FILE__ + 1) : 0;
    snprintf(path, sizeof(path), "%.*s%s", (int)dir_len, __FILE__, golden_rel);

    FILE* f = fopen(path, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);

    char line[512];
    for (int mins = 0; mins < 1440; ++mins) {
        uint8_t h = mins / 60;
        uint8_t m = mins % 60;
        WordSet ws = time_to_words(h, m);
        char actual[512];
        format_line(h, m, ws, actual, sizeof(actual));

        if (!fgets(line, sizeof(line), f)) {
            fclose(f);
            char msg[128];
            snprintf(msg, sizeof(msg), "golden file truncated at line %d", mins + 1);
            TEST_FAIL_MESSAGE(msg);
        }
        size_t L = strlen(line);
        while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = 0;

        if (strcmp(actual, line) != 0) {
            fclose(f);
            char msg[1200];
            snprintf(msg, sizeof(msg), "mismatch at line %d:\n  expected: %s\n  actual:   %s",
                     mins + 1, line, actual);
            TEST_FAIL_MESSAGE(msg);
        }
    }
    fclose(f);
}

void test_emory_golden(void) { run_golden_check("emory_golden.txt"); }
void test_nora_golden(void)  { run_golden_check("nora_golden.txt"); }

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_emory_golden);
    RUN_TEST(test_nora_golden);
    return UNITY_END();
}
