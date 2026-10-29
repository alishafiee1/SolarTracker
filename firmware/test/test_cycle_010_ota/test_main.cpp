// Cycle 010 — offline update logic (rodo/suntracker/cycles/010-offline-update)
#include <initializer_list>
#include <string.h>
#include <unity.h>

#include "ota_logic.h"

using namespace ota;

void setUp() {}
void tearDown() {}

static void feedAll(MagicScanner& s, const char* text, size_t chunk) {
    const size_t n = strlen(text);
    for (size_t i = 0; i < n; i += chunk) s.feed(reinterpret_cast<const uint8_t*>(text + i), n - i < chunk ? n - i : chunk);
}

// ------------------------------------------------------------ R3 magic text
static void the_magic_is_found_in_any_chunking() {
    const char* image = "xxxxxxxxxxSUNTRACKER-FWyyyyyyyy";
    for (size_t chunk : {1u, 2u, 3u, 5u, 7u, 13u, 64u}) {
        MagicScanner s;
        feedAll(s, image, chunk);
        TEST_ASSERT_TRUE_MESSAGE(s.found(), "magic split across chunks must still be found");
    }
}

static void almost_the_magic_is_not_the_magic() {
    MagicScanner s;
    feedAll(s, "SUNTRACKER-F SUNTRACKER-FX SUNTRACKE SUNTRACKER_FW", 3);
    TEST_ASSERT_FALSE(s.found());
}

static void a_restart_inside_a_partial_match_still_matches() {
    MagicScanner s;
    feedAll(s, "SUNSUNTRACKER-FW", 4);  // the first SUN is a false start
    TEST_ASSERT_TRUE(s.found());
}

static void the_firmware_magic_has_the_expected_text() { TEST_ASSERT_EQUAL_STRING("SUNTRACKER-FW", kMagic); }

// ------------------------------------------------------------ upload limits
static void a_normal_upload_is_accepted() {
    UploadSession u(1000, 0);
    const char* part1 = "header....SUNTRA";
    const char* part2 = "CKER-FW....rest";
    TEST_ASSERT_TRUE(u.accept(reinterpret_cast<const uint8_t*>(part1), strlen(part1), 100) == UploadStatus::Ok);
    TEST_ASSERT_TRUE(u.accept(reinterpret_cast<const uint8_t*>(part2), strlen(part2), 200) == UploadStatus::Ok);
    TEST_ASSERT_TRUE(u.finish() == UploadStatus::Ok);
    TEST_ASSERT_EQUAL_size_t(strlen(part1) + strlen(part2), u.received());
}

static void an_image_of_another_project_is_refused() {
    UploadSession u(1000, 0);
    const char* other = "a perfectly valid looking image without the project text";
    u.accept(reinterpret_cast<const uint8_t*>(other), strlen(other), 10);
    TEST_ASSERT_TRUE(u.finish() == UploadStatus::NoMagic);
}

static void an_empty_upload_is_refused() {
    UploadSession u(1000, 0);
    TEST_ASSERT_TRUE(u.finish() == UploadStatus::Empty);
}

static void a_file_bigger_than_the_slot_is_refused() {
    UploadSession u(100, 0);
    uint8_t block[60] = {};
    TEST_ASSERT_TRUE(u.accept(block, sizeof(block), 1) == UploadStatus::Ok);
    TEST_ASSERT_TRUE(u.accept(block, sizeof(block), 2) == UploadStatus::TooBig);
}

static void an_upload_that_takes_more_than_three_minutes_is_aborted() {
    UploadSession u(100000, 1000);
    uint8_t b[10] = {};
    TEST_ASSERT_TRUE(u.accept(b, sizeof(b), 1000 + UploadSession::kMaxDurationMs) == UploadStatus::Ok);
    TEST_ASSERT_TRUE(u.accept(b, sizeof(b), 1000 + UploadSession::kMaxDurationMs + 1) == UploadStatus::TimedOut);
}

static void the_clock_wrap_does_not_abort_a_fresh_upload() {
    UploadSession u(100000, 0xFFFFFF00u);
    uint8_t b[10] = {};
    TEST_ASSERT_TRUE(u.accept(b, sizeof(b), 0x10u) == UploadStatus::Ok);  // 272 ms later, after the wrap
}

// ------------------------------------------------------------ R4 probation and rollback
static void a_normal_boot_is_normal() {
    BootState s;
    TEST_ASSERT_TRUE(onBoot(s) == BootAction::Normal);
    TEST_ASSERT_EQUAL_UINT8(0, s.bootTries);
}

static void a_new_image_that_confirms_itself_stays() {
    BootState s;
    startProbation(s);
    TEST_ASSERT_TRUE(onBoot(s) == BootAction::Probation);
    confirm(s);  // the self-test passed after 60 s
    TEST_ASSERT_TRUE(onBoot(s) == BootAction::Normal);
}

static void a_new_image_that_keeps_crashing_is_rolled_back_at_the_fourth_start() {
    BootState s;
    startProbation(s);
    TEST_ASSERT_TRUE(onBoot(s) == BootAction::Probation);  // 1
    TEST_ASSERT_TRUE(onBoot(s) == BootAction::Probation);  // 2 (crashed before confirming)
    TEST_ASSERT_TRUE(onBoot(s) == BootAction::Probation);  // 3
    TEST_ASSERT_TRUE(onBoot(s) == BootAction::Rollback);   // the old image is started again
}

static void probation_survives_a_confirmation_between_updates() {
    BootState s;
    startProbation(s);
    onBoot(s);
    confirm(s);
    startProbation(s);  // the next update starts with a fresh counter
    TEST_ASSERT_EQUAL_UINT8(0, s.bootTries);
    TEST_ASSERT_TRUE(onBoot(s) == BootAction::Probation);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(the_magic_is_found_in_any_chunking);
    RUN_TEST(almost_the_magic_is_not_the_magic);
    RUN_TEST(a_restart_inside_a_partial_match_still_matches);
    RUN_TEST(the_firmware_magic_has_the_expected_text);
    RUN_TEST(a_normal_upload_is_accepted);
    RUN_TEST(an_image_of_another_project_is_refused);
    RUN_TEST(an_empty_upload_is_refused);
    RUN_TEST(a_file_bigger_than_the_slot_is_refused);
    RUN_TEST(an_upload_that_takes_more_than_three_minutes_is_aborted);
    RUN_TEST(the_clock_wrap_does_not_abort_a_fresh_upload);
    RUN_TEST(a_normal_boot_is_normal);
    RUN_TEST(a_new_image_that_confirms_itself_stays);
    RUN_TEST(a_new_image_that_keeps_crashing_is_rolled_back_at_the_fourth_start);
    RUN_TEST(probation_survives_a_confirmation_between_updates);
    return UNITY_END();
}
