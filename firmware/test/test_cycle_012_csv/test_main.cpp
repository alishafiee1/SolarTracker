// Cycle 012 — CSV export formatting (rodo/suntracker/cycles/012-log-csv-export)
#include <string.h>
#include <unity.h>

#include "csv_format.h"
#include "event_format.h"

void setUp() {}
void tearDown() {}

// R4: BOM and CRLF so Excel reads UTF-8 and Persian text correctly.
static void header_has_a_bom_and_the_four_columns() {
    char buf[96];
    const size_t n = events::csvHeader(buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_HEX8(0xEF, static_cast<uint8_t>(buf[0]));
    TEST_ASSERT_EQUAL_HEX8(0xBB, static_cast<uint8_t>(buf[1]));
    TEST_ASSERT_EQUAL_HEX8(0xBF, static_cast<uint8_t>(buf[2]));
    TEST_ASSERT_EQUAL_STRING("local_time,utc_time,type,detail\r\n", buf + 3);
    TEST_ASSERT_EQUAL_size_t(0, events::csvHeader(buf, 10));
}

// R3: local and UTC time in ISO 8601.
static void times_are_written_in_local_and_utc() {
    char buf[200];
    // 2026-10-29 14:07:23 UTC = 17:37:23 in Iran (+03:30)
    const size_t n = events::csvRow(buf, sizeof(buf), 1793282843LL, 210, "MoveEnd", "reason=stop angle=12.3");
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_STRING("2026-10-29T17:37:23+03:30,2026-10-29T14:07:23Z,MoveEnd,reason=stop angle=12.3\r\n", buf);
    TEST_ASSERT_EQUAL_size_t(strlen(buf), n);
}

static void negative_offsets_and_day_changes() {
    char buf[200];
    // 2027-01-01 01:00:00 UTC seen from UTC-05:00 is still 2026-12-31 20:00
    events::csvRow(buf, sizeof(buf), 1798765200LL, -300, "PowerUp", "power-on");
    TEST_ASSERT_EQUAL_STRING("2026-12-31T20:00:00-05:00,2027-01-01T01:00:00Z,PowerUp,power-on\r\n", buf);
}

static void an_unknown_time_gives_empty_time_cells() {
    char buf[120];
    events::csvRow(buf, sizeof(buf), 0, 210, "PowerUp", "software");
    TEST_ASSERT_EQUAL_STRING(",,PowerUp,software\r\n", buf);
}

// Persian text in the detail passes through unchanged (UTF-8).
static void persian_text_survives() {
    char buf[200];
    events::csvRow(buf, sizeof(buf), 1793282843LL, 210, "Fault", "گیرکردن بازو");
    TEST_ASSERT_NOT_NULL(strstr(buf, "گیرکردن بازو"));
}

static void fields_with_commas_quotes_or_leading_spaces_are_quoted() {
    char buf[200];
    events::csvRow(buf, sizeof(buf), 1793282843LL, 210, "Fault", "a, b");
    TEST_ASSERT_NOT_NULL(strstr(buf, ",\"a, b\"\r\n"));
    events::csvRow(buf, sizeof(buf), 1793282843LL, 210, "Fault", "say \"stop\"");
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"say \"\"stop\"\"\"\r\n"));
    events::csvRow(buf, sizeof(buf), 1793282843LL, 210, "Fault", " leading");
    TEST_ASSERT_NOT_NULL(strstr(buf, ",\" leading\"\r\n"));
}

static void a_too_small_buffer_is_reported_not_overrun() {
    char buf[20];
    memset(buf, 'x', sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(0, events::csvRow(buf, sizeof(buf), 1793282843LL, 210, "MoveEnd", "reason=stop angle=12.3"));
}

// The CSV is built from the stored lines: format -> parse -> row reproduces the event.
static void stored_log_lines_become_rows() {
    char line[events::kMaxLine];
    events::formatLine(line, sizeof(line), 1793282843LL, hal::EventType::MoveStart, "dir=west angle=3.0");
    int64_t t;
    char type[24], detail[96], row[200];
    TEST_ASSERT_TRUE(events::parseLine(line, t, type, sizeof(type), detail, sizeof(detail)));
    TEST_ASSERT_TRUE(events::csvRow(row, sizeof(row), t, 210, type, detail) > 0);
    TEST_ASSERT_EQUAL_STRING("2026-10-29T17:37:23+03:30,2026-10-29T14:07:23Z,MoveStart,dir=west angle=3.0\r\n", row);
}

// A whole 64 KB file converts without a buffer larger than one row: every possible stored line fits a row buffer.
static void the_longest_stored_line_fits_one_row_buffer() {
    char detail[events::kMaxDetail + 1];
    memset(detail, 'z', events::kMaxDetail);
    detail[events::kMaxDetail] = '\0';
    char row[events::kMaxLine * 2];
    TEST_ASSERT_TRUE(events::csvRow(row, sizeof(row), 1793282843LL, 210, "SettingsMigrated", detail) > 0);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(header_has_a_bom_and_the_four_columns);
    RUN_TEST(times_are_written_in_local_and_utc);
    RUN_TEST(negative_offsets_and_day_changes);
    RUN_TEST(an_unknown_time_gives_empty_time_cells);
    RUN_TEST(persian_text_survives);
    RUN_TEST(fields_with_commas_quotes_or_leading_spaces_are_quoted);
    RUN_TEST(a_too_small_buffer_is_reported_not_overrun);
    RUN_TEST(stored_log_lines_become_rows);
    RUN_TEST(the_longest_stored_line_fits_one_row_buffer);
    return UNITY_END();
}
