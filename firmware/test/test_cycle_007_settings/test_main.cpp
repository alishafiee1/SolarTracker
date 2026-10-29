// Cycle 007 — settings, clock maths, event format, cities, supply watch
// (rodo/suntracker/cycles/007-settings-clock-log)
#include <fakes.h>
#include <stddef.h>
#include <string.h>
#include <unity.h>

#include "event_format.h"
#include "location.h"
#include "rtc_time.h"
#include "settings.h"
#include "supply_watch.h"

using settings::LoadResult;
using settings::Settings;
using settings::SettingsStore;

void setUp() {}
void tearDown() {}

// ------------------------------------------------------------ R1
static void defaults_are_valid_and_match_the_proposal() {
    Settings s;
    TEST_ASSERT_TRUE(settings::isValid(s));
    TEST_ASSERT_EQUAL_STRING("kerman", s.cityId);
    TEST_ASSERT_EQUAL_FLOAT(30.283f, s.latDeg);
    TEST_ASSERT_EQUAL_INT(210, s.tzOffsetMin);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, s.moveThresholdDeg);
    TEST_ASSERT_EQUAL_FLOAT(-45.0f, s.minDeg);
    TEST_ASSERT_EQUAL_FLOAT(45.0f, s.maxDeg);
    TEST_ASSERT_EQUAL_UINT32(3000, s.stopSettleMs);
    TEST_ASSERT_EQUAL_UINT32(10000, s.minMoveGapMs);
    TEST_ASSERT_EQUAL_UINT32(120000, s.maxMoveMs);
    TEST_ASSERT_EQUAL_FLOAT(40.0f, s.windStowKmh);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, s.windReturnKmh);
    TEST_ASSERT_EQUAL_UINT32(15, s.windReturnMin);
    TEST_ASSERT_EQUAL_FLOAT(4.5f, s.supplyLowV);
}

static const char* firstInvalid(const Settings& s) {
    const char* names[4] = {nullptr, nullptr, nullptr, nullptr};
    settings::validate(s, names, 4);
    return names[0];
}

static void validation_names_each_bad_field() {
    Settings s;
    s.latDeg = 91.0f;
    TEST_ASSERT_EQUAL_STRING("latDeg", firstInvalid(s));
    s = Settings();
    s.lonDeg = -181.0f;
    TEST_ASSERT_EQUAL_STRING("lonDeg", firstInvalid(s));
    s = Settings();
    s.moveThresholdDeg = 1.0f;
    TEST_ASSERT_EQUAL_STRING("moveThresholdDeg", firstInvalid(s));
    s = Settings();
    s.minDeg = -10.0f;
    s.maxDeg = 0.0f;  // equal to the limit but min < max holds: valid
    TEST_ASSERT_TRUE(settings::isValid(s));
    s.minDeg = 0.0f;  // min == max
    TEST_ASSERT_FALSE(settings::isValid(s));
    s = Settings();
    s.stopSettleMs = 500;
    TEST_ASSERT_EQUAL_STRING("stopSettleMs", firstInvalid(s));
    s = Settings();
    s.windReturnKmh = 45.0f;  // above the stow threshold
    TEST_ASSERT_EQUAL_STRING("windReturnKmh", firstInvalid(s));
    s = Settings();
    s.anemometerFactor = 0.5f;
    TEST_ASSERT_EQUAL_STRING("anemometerFactor", firstInvalid(s));
    s = Settings();
    s.latDeg = nanf("");
    TEST_ASSERT_EQUAL_STRING("latDeg", firstInvalid(s));
    s = Settings();
    s.mode = static_cast<settings::Mode>(7);
    TEST_ASSERT_EQUAL_STRING("mode", firstInvalid(s));
    s = Settings();
    memset(s.apPassword, 'x', sizeof(s.apPassword));  // not terminated
    TEST_ASSERT_EQUAL_STRING("apPassword", firstInvalid(s));
}

static void save_rejects_invalid_and_keeps_the_old_values() {
    sim::FakeSettingsStore blobs;
    SettingsStore store(blobs);
    Settings good;
    good.moveThresholdDeg = 7.0f;
    TEST_ASSERT_TRUE(store.save(good));
    Settings bad;
    bad.moveThresholdDeg = 99.0f;
    const char* field = nullptr;
    TEST_ASSERT_FALSE(store.save(bad, &field));
    TEST_ASSERT_EQUAL_STRING("moveThresholdDeg", field);
    Settings loaded;
    TEST_ASSERT_EQUAL(static_cast<int>(LoadResult::Loaded), static_cast<int>(store.load(loaded)));
    TEST_ASSERT_EQUAL_FLOAT(7.0f, loaded.moveThresholdDeg);
}

// ------------------------------------------------------------ R2
static void round_trip_keeps_every_field() {
    sim::FakeSettingsStore blobs;
    SettingsStore store(blobs);
    Settings s;
    s.latDeg = 35.689f;
    s.lonDeg = 51.389f;
    strcpy(s.cityId, "tehran");
    s.windStowKmh = 55.0f;
    s.mode = settings::Mode::Manual;
    strcpy(s.apPassword, "example-pass");
    s.passwordIsDefault = false;
    s.settingsPasswordHash[5] = 0xAB;
    TEST_ASSERT_TRUE(store.save(s));
    Settings out;
    TEST_ASSERT_EQUAL(static_cast<int>(LoadResult::Loaded), static_cast<int>(store.load(out)));
    TEST_ASSERT_EQUAL_MEMORY(&s, &out, sizeof(Settings));
}

static void missing_or_corrupt_settings_reset_to_defaults_with_an_event() {
    {
        sim::FakeSettingsStore blobs;
        sim::FakeEventLog log;
        SettingsStore store(blobs);
        Settings out;
        out.moveThresholdDeg = 9.0f;
        TEST_ASSERT_EQUAL(static_cast<int>(LoadResult::Reset), static_cast<int>(store.load(out, &log)));
        TEST_ASSERT_EQUAL_FLOAT(5.0f, out.moveThresholdDeg);
        TEST_ASSERT_EQUAL_size_t(1, log.events().size());
        TEST_ASSERT_TRUE(log.events()[0].first == hal::EventType::SettingsReset);
    }
    {  // a flipped bit in the payload
        sim::FakeSettingsStore blobs;
        SettingsStore store(blobs);
        Settings s;
        s.moveThresholdDeg = 8.0f;
        store.save(s);
        uint8_t buf[512];
        const size_t n = blobs.size("settings");
        blobs.load("settings", buf, n);
        buf[n - 1] ^= 0x01;
        blobs.save("settings", buf, n);
        sim::FakeEventLog log;
        Settings out;
        TEST_ASSERT_EQUAL(static_cast<int>(LoadResult::Reset), static_cast<int>(store.load(out, &log)));
        TEST_ASSERT_EQUAL_FLOAT(5.0f, out.moveThresholdDeg);
        TEST_ASSERT_EQUAL_size_t(1, log.events().size());
    }
    {  // random junk of any size must never crash the boot
        sim::FakeSettingsStore blobs;
        SettingsStore store(blobs);
        for (size_t len : {1u, 5u, 11u, 12u, 13u, 60u}) {
            uint8_t junk[64];
            memset(junk, 0xA5, sizeof(junk));
            blobs.save("settings", junk, len);
            Settings out;
            TEST_ASSERT_EQUAL(static_cast<int>(LoadResult::Reset), static_cast<int>(store.load(out)));
        }
    }
}

static void older_shorter_layout_is_migrated_with_new_fields_at_default() {
    sim::FakeSettingsStore blobs;
    SettingsStore store(blobs);
    Settings s;
    s.moveThresholdDeg = 6.5f;
    s.passwordIsDefault = false;
    s.currentPresent = true;
    s.currentFactor = 9.0f;
    store.save(s);

    // Rewrite the blob as the firmware before cycle 009 wrote it: the payload ends before the current-module fields.
    uint8_t buf[512];
    const size_t n = blobs.size("settings");
    blobs.load("settings", buf, n);
    const uint16_t newPayload = static_cast<uint16_t>(offsetof(Settings, currentPresent));
    memcpy(buf + 6, &newPayload, 2);
    const uint32_t crc = settings::crc32(buf + 12, newPayload);
    memcpy(buf + 8, &crc, 4);
    blobs.save("settings", buf, 12 + newPayload);

    sim::FakeEventLog log;
    Settings out;
    TEST_ASSERT_EQUAL(static_cast<int>(LoadResult::Migrated), static_cast<int>(store.load(out, &log)));
    TEST_ASSERT_EQUAL_FLOAT(6.5f, out.moveThresholdDeg);  // kept
    TEST_ASSERT_FALSE(out.passwordIsDefault);             // fields before the cut are kept
    TEST_ASSERT_FALSE(out.currentPresent);                // fields added later are back at their defaults
    TEST_ASSERT_EQUAL_FLOAT(5.0f, out.currentFactor);
    TEST_ASSERT_TRUE(log.events()[0].first == hal::EventType::SettingsMigrated);
}

static void newer_layout_is_not_guessed() {
    sim::FakeSettingsStore blobs;
    SettingsStore store(blobs);
    store.save(Settings());
    uint8_t buf[512];
    const size_t n = blobs.size("settings");
    blobs.load("settings", buf, n);
    const uint16_t v = 99;
    memcpy(buf + 4, &v, 2);
    blobs.save("settings", buf, n);
    Settings out;
    TEST_ASSERT_EQUAL(static_cast<int>(LoadResult::Reset), static_cast<int>(store.load(out)));
}

// ------------------------------------------------------------ R3 / R4 / R5 maths
static void epoch_and_calendar_agree() {
    TEST_ASSERT_EQUAL_INT64(946684800LL, rtc::toEpoch({2000, 1, 1, 0, 0, 0}));
    TEST_ASSERT_EQUAL_INT64(951868800LL, rtc::toEpoch({2000, 3, 1, 0, 0, 0}));
    TEST_ASSERT_EQUAL_INT64(2147483647LL, rtc::toEpoch({2038, 1, 19, 3, 14, 7}));
    const rtc::DateTime leap = rtc::fromEpoch(rtc::toEpoch({2028, 2, 29, 12, 34, 56}));
    TEST_ASSERT_EQUAL_INT(2028, leap.year);
    TEST_ASSERT_EQUAL_INT(2, leap.month);
    TEST_ASSERT_EQUAL_INT(29, leap.day);
    TEST_ASSERT_EQUAL_INT(12, leap.hour);
    TEST_ASSERT_EQUAL_INT(56, leap.second);
    // Every 7 hours 13 minutes 11 seconds over 70 years round-trips.
    for (int64_t t = 946684800LL; t < 3155760000LL; t += 7 * 3600 + 13 * 60 + 11) {
        TEST_ASSERT_EQUAL_INT64(t, rtc::toEpoch(rtc::fromEpoch(t)));
    }
}

static void bcd_and_plausibility() {
    for (int v = 0; v < 100; ++v) TEST_ASSERT_EQUAL_INT(v, rtc::fromBcd(rtc::toBcd(v)));
    TEST_ASSERT_EQUAL_HEX8(0x59, rtc::toBcd(59));
    TEST_ASSERT_TRUE(rtc::isPlausible({2026, 10, 29, 17, 37, 23}));
    TEST_ASSERT_FALSE(rtc::isPlausible({2026, 2, 31, 0, 0, 0}));
    TEST_ASSERT_FALSE(rtc::isPlausible({2000, 1, 1, 0, 0, 0}));  // before the project existed: not a set clock
    TEST_ASSERT_FALSE(rtc::isPlausible({2026, 13, 1, 0, 0, 0}));
    TEST_ASSERT_FALSE(rtc::isPlausible({2026, 5, 5, 24, 0, 0}));
}

// ------------------------------------------------------------ R6 text form
static void event_lines_are_one_csv_line_each() {
    char buf[events::kMaxLine];
    const size_t n = events::formatLine(buf, sizeof(buf), 1813561200LL, hal::EventType::Fault, "stall, retry 2\nnext");
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_STRING("1813561200,Fault,stall; retry 2 next\n", buf);

    char longDetail[200];
    memset(longDetail, 'x', sizeof(longDetail) - 1);
    longDetail[sizeof(longDetail) - 1] = '\0';
    const size_t m = events::formatLine(buf, sizeof(buf), 1, hal::EventType::PowerUp, longDetail);
    TEST_ASSERT_TRUE(m > 0);
    TEST_ASSERT_EQUAL_size_t(strlen("1,PowerUp,") + events::kMaxDetail + 1, m);

    char tiny[8];
    TEST_ASSERT_EQUAL_size_t(0, events::formatLine(tiny, sizeof(tiny), 1813561200LL, hal::EventType::Fault, "x"));
}

static void event_lines_parse_back() {
    char line[events::kMaxLine];
    events::formatLine(line, sizeof(line), 1813561234LL, hal::EventType::MoveEnd, "reason=stop angle=12.3");
    int64_t t = 0;
    char type[24], detail[96];
    TEST_ASSERT_TRUE(events::parseLine(line, t, type, sizeof(type), detail, sizeof(detail)));
    TEST_ASSERT_EQUAL_INT64(1813561234LL, t);
    TEST_ASSERT_EQUAL_STRING("MoveEnd", type);
    TEST_ASSERT_EQUAL_STRING("reason=stop angle=12.3", detail);
    TEST_ASSERT_FALSE(events::parseLine("garbage", t, type, sizeof(type), detail, sizeof(detail)));
    TEST_ASSERT_FALSE(events::parseLine("12,NoDetail", t, type, sizeof(type), detail, sizeof(detail)));
}

// ------------------------------------------------------------ R9
static void cities_are_the_31_province_capitals() {
    TEST_ASSERT_EQUAL_size_t(31, settings::kCityCount);
    for (size_t i = 0; i < settings::kCityCount; ++i) {
        const settings::City& c = settings::kCities[i];
        TEST_ASSERT_TRUE(c.lat > 25.0f && c.lat < 40.0f);
        TEST_ASSERT_TRUE(c.lon > 44.0f && c.lon < 64.0f);
        TEST_ASSERT_TRUE(strlen(c.id) < sizeof(Settings::cityId));
        TEST_ASSERT_TRUE(strlen(c.nameFa) > 0);
        for (size_t j = i + 1; j < settings::kCityCount; ++j) TEST_ASSERT_TRUE(strcmp(c.id, settings::kCities[j].id) != 0);
    }
    const settings::City* kerman = settings::findCity("kerman");
    TEST_ASSERT_NOT_NULL(kerman);
    TEST_ASSERT_EQUAL_FLOAT(Settings().latDeg, kerman->lat);
    TEST_ASSERT_EQUAL_FLOAT(Settings().lonDeg, kerman->lon);
    TEST_ASSERT_NULL(settings::findCity("atlantis"));
    TEST_ASSERT_NULL(settings::findCity(nullptr));
}

static void lat_lon_text_is_parsed_and_checked() {
    double lat = 0, lon = 0;
    TEST_ASSERT_TRUE(settings::parseLatLon("30.283, 57.083", lat, lon));
    TEST_ASSERT_EQUAL_DOUBLE(30.283, lat);
    TEST_ASSERT_EQUAL_DOUBLE(57.083, lon);
    TEST_ASSERT_TRUE(settings::parseLatLon("  30.2830,57.0830 ", lat, lon));
    TEST_ASSERT_TRUE(settings::parseLatLon("-33.5 151.2", lat, lon));
    TEST_ASSERT_EQUAL_DOUBLE(-33.5, lat);
    TEST_ASSERT_FALSE(settings::parseLatLon("abc", lat, lon));
    TEST_ASSERT_FALSE(settings::parseLatLon("30.1", lat, lon));
    TEST_ASSERT_FALSE(settings::parseLatLon("91, 10", lat, lon));
    TEST_ASSERT_FALSE(settings::parseLatLon("30.1, 200", lat, lon));
    TEST_ASSERT_FALSE(settings::parseLatLon("30.1, 57.2 extra", lat, lon));
    TEST_ASSERT_FALSE(settings::parseLatLon(nullptr, lat, lon));
}

// ------------------------------------------------------------ R8
static void supply_low_is_reported_once_after_three_seconds() {
    settings::SupplyWatch w;
    TEST_ASSERT_FALSE(w.onSecond(4.0f, 4.5f));
    TEST_ASSERT_FALSE(w.onSecond(4.0f, 4.5f));
    TEST_ASSERT_TRUE(w.onSecond(4.0f, 4.5f));   // third second below the limit
    TEST_ASSERT_FALSE(w.onSecond(4.0f, 4.5f));  // not again while it stays low
    TEST_ASSERT_FALSE(w.onSecond(4.55f, 4.5f)); // within the hysteresis: still armed off
    TEST_ASSERT_FALSE(w.onSecond(4.0f, 4.5f));
    TEST_ASSERT_FALSE(w.onSecond(5.0f, 4.5f));  // recovered
    TEST_ASSERT_FALSE(w.onSecond(4.0f, 4.5f));
    TEST_ASSERT_FALSE(w.onSecond(4.0f, 4.5f));
    TEST_ASSERT_TRUE(w.onSecond(4.0f, 4.5f));   // reported again after a recovery
}

static void a_short_dip_is_not_reported() {
    settings::SupplyWatch w;
    TEST_ASSERT_FALSE(w.onSecond(4.0f, 4.5f));
    TEST_ASSERT_FALSE(w.onSecond(4.0f, 4.5f));
    TEST_ASSERT_FALSE(w.onSecond(5.0f, 4.5f));
    TEST_ASSERT_FALSE(w.onSecond(4.0f, 4.5f));
    TEST_ASSERT_FALSE(w.onSecond(4.0f, 4.5f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(defaults_are_valid_and_match_the_proposal);
    RUN_TEST(validation_names_each_bad_field);
    RUN_TEST(save_rejects_invalid_and_keeps_the_old_values);
    RUN_TEST(round_trip_keeps_every_field);
    RUN_TEST(missing_or_corrupt_settings_reset_to_defaults_with_an_event);
    RUN_TEST(older_shorter_layout_is_migrated_with_new_fields_at_default);
    RUN_TEST(newer_layout_is_not_guessed);
    RUN_TEST(epoch_and_calendar_agree);
    RUN_TEST(bcd_and_plausibility);
    RUN_TEST(event_lines_are_one_csv_line_each);
    RUN_TEST(event_lines_parse_back);
    RUN_TEST(cities_are_the_31_province_capitals);
    RUN_TEST(lat_lon_text_is_parsed_and_checked);
    RUN_TEST(supply_low_is_reported_once_after_three_seconds);
    RUN_TEST(a_short_dip_is_not_reported);
    return UNITY_END();
}
