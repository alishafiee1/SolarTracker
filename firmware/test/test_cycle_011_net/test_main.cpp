// Cycle 011 — site Wi-Fi and notification logic (rodo/suntracker/cycles/011-site-wifi-bale)
#include <fakes.h>
#include <string.h>
#include <unity.h>

#include "net_logic.h"

using namespace net;

void setUp() {}
void tearDown() {}

// ------------------------------------------------------------ R1 backoff
static void reconnect_delays_double_up_to_five_minutes() {
    Backoff b;
    TEST_ASSERT_EQUAL_UINT32(5000, b.nextDelayMs());
    TEST_ASSERT_EQUAL_UINT32(10000, b.nextDelayMs());
    TEST_ASSERT_EQUAL_UINT32(20000, b.nextDelayMs());
    for (int i = 0; i < 10; ++i) b.nextDelayMs();
    TEST_ASSERT_EQUAL_UINT32(300000, b.nextDelayMs());
    TEST_ASSERT_EQUAL_UINT32(300000, b.nextDelayMs());
    b.reset();  // a successful connection starts over
    TEST_ASSERT_EQUAL_UINT32(5000, b.nextDelayMs());
}

// ------------------------------------------------------------ R2 NTP
static void ntp_corrects_a_clock_that_is_off_by_more_than_two_seconds() {
    const int64_t now = 1813561200;
    TEST_ASSERT_TRUE(shouldSetClockFromNtp(now - 60, true, now));
    TEST_ASSERT_TRUE(shouldSetClockFromNtp(now + 3, true, now));
    TEST_ASSERT_FALSE(shouldSetClockFromNtp(now + 2, true, now));
    TEST_ASSERT_FALSE(shouldSetClockFromNtp(now, true, now));
    TEST_ASSERT_TRUE(shouldSetClockFromNtp(0, false, now));        // a clock that was never set takes the NTP time
    TEST_ASSERT_FALSE(shouldSetClockFromNtp(now, true, 1000));     // nonsense from the network is ignored
    TEST_ASSERT_FALSE(shouldSetClockFromNtp(now, true, 0));
}

// ------------------------------------------------------------ R3 queue
static void messages_are_sent_one_per_ten_seconds() {
    NotifyQueue q;
    std::string t;
    TEST_ASSERT_TRUE(q.enqueue("fault:stall", "one", 0));
    TEST_ASSERT_TRUE(q.enqueue("fault:wire", "two", 0));
    TEST_ASSERT_TRUE(q.peek(1000, t));
    TEST_ASSERT_EQUAL_STRING("one", t.c_str());
    q.done(true, 1000);
    TEST_ASSERT_FALSE(q.peek(5000, t));  // not within 10 s of the last send
    TEST_ASSERT_TRUE(q.peek(11000, t));
    TEST_ASSERT_EQUAL_STRING("two", t.c_str());
    q.done(true, 11000);
    TEST_ASSERT_EQUAL_size_t(0, q.size());
    TEST_ASSERT_FALSE(q.peek(60000, t));
}

static void a_failed_send_is_retried_after_the_gap_and_keeps_the_message() {
    NotifyQueue q;
    std::string t;
    q.enqueue("fault:stall", "stall", 0);
    TEST_ASSERT_TRUE(q.peek(0, t));
    q.done(false, 0);  // no internet
    TEST_ASSERT_EQUAL_size_t(1, q.size());
    TEST_ASSERT_FALSE(q.peek(5000, t));
    TEST_ASSERT_TRUE(q.peek(10000, t));
    q.done(true, 10000);
    TEST_ASSERT_EQUAL_size_t(0, q.size());
}

static void the_same_fault_within_ten_minutes_is_merged() {
    NotifyQueue q;
    TEST_ASSERT_TRUE(q.enqueue("fault:stall", "stall", 0));
    TEST_ASSERT_FALSE(q.enqueue("fault:stall", "stall again", 60000));
    TEST_ASSERT_FALSE(q.enqueue("fault:stall", "stall again", 599000));
    TEST_ASSERT_EQUAL_size_t(1, q.size());
    TEST_ASSERT_TRUE(q.enqueue("fault:stall", "stall later", 601000));  // after 10 minutes it is news again
    TEST_ASSERT_TRUE(q.enqueue("fault:other", "other", 1000));          // a different fault is never merged
}

static void the_queue_keeps_the_newest_twenty() {
    NotifyQueue q;
    char key[16], text[16];
    for (int i = 0; i < 25; ++i) {
        snprintf(key, sizeof(key), "k%d", i);
        snprintf(text, sizeof(text), "m%d", i);
        q.enqueue(key, text, 0);
    }
    TEST_ASSERT_EQUAL_size_t(NotifyQueue::kCapacity, q.size());
    std::string t;
    q.peek(0, t);
    TEST_ASSERT_EQUAL_STRING("m5", t.c_str());  // the five oldest were dropped
}

static void the_message_names_the_device_the_fault_and_the_local_time() {
    // 2026-10-29 14:07:23 UTC is 17:37 in Iran
    const std::string m = formatNotification("SunTracker-455D", "خطا: گیرکردن بازو", 1793282843LL, 210);
    TEST_ASSERT_EQUAL_STRING("SunTracker-455D: خطا: گیرکردن بازو — 17:37", m.c_str());
    TEST_ASSERT_EQUAL_STRING("SunTracker-455D: باد شدید", formatNotification("SunTracker-455D", "باد شدید", 0, 210).c_str());
}

// ------------------------------------------------------------ R4 configuration and secrets
static NetConfig sample() {
    NetConfig c;
    strcpy(c.ssid, "HomeWifi");
    strcpy(c.password, "router-pass-1");
    strcpy(c.botToken, "123456:ABC-DEF_ghi");
    strcpy(c.chatId, "987654321");
    c.notifyEnabled = true;
    return c;
}

static void configuration_round_trips_and_survives_only_when_intact() {
    sim::FakeSettingsStore blobs;
    TEST_ASSERT_TRUE(save(blobs, sample()));
    NetConfig out;
    TEST_ASSERT_TRUE(load(blobs, out));
    TEST_ASSERT_EQUAL_STRING("HomeWifi", out.ssid);
    TEST_ASSERT_EQUAL_STRING("123456:ABC-DEF_ghi", out.botToken);
    TEST_ASSERT_TRUE(out.notifyEnabled);

    uint8_t buf[512];
    const size_t n = blobs.size("netcfg");
    blobs.load("netcfg", buf, n);
    buf[8] ^= 1;  // damage a byte of the stored configuration (the last byte of the blob is only padding)
    blobs.save("netcfg", buf, n);
    NetConfig again;
    TEST_ASSERT_FALSE(load(blobs, again));
    TEST_ASSERT_EQUAL_STRING("", again.ssid);
}

static void invalid_configurations_are_refused() {
    const char* bad = nullptr;
    NetConfig c = sample();
    strcpy(c.password, "short");
    TEST_ASSERT_FALSE(validate(c, &bad));
    TEST_ASSERT_EQUAL_STRING("password", bad);
    c = sample();
    strcpy(c.botToken, "has/slash");
    TEST_ASSERT_FALSE(validate(c, &bad));
    TEST_ASSERT_EQUAL_STRING("botToken", bad);
    c = sample();
    memset(c.ssid, 'x', sizeof(c.ssid));  // not terminated
    TEST_ASSERT_FALSE(validate(c, &bad));
    TEST_ASSERT_EQUAL_STRING("ssid", bad);
    c = sample();
    c.password[0] = '\0';  // an open network is allowed
    TEST_ASSERT_TRUE(validate(c, &bad));
    sim::FakeSettingsStore blobs;
    strcpy(c.botToken, "bad token");
    TEST_ASSERT_FALSE(save(blobs, c));
    TEST_ASSERT_EQUAL_size_t(0, blobs.size("netcfg"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(reconnect_delays_double_up_to_five_minutes);
    RUN_TEST(ntp_corrects_a_clock_that_is_off_by_more_than_two_seconds);
    RUN_TEST(messages_are_sent_one_per_ten_seconds);
    RUN_TEST(a_failed_send_is_retried_after_the_gap_and_keeps_the_message);
    RUN_TEST(the_same_fault_within_ten_minutes_is_merged);
    RUN_TEST(the_queue_keeps_the_newest_twenty);
    RUN_TEST(the_message_names_the_device_the_fault_and_the_local_time);
    RUN_TEST(configuration_round_trips_and_survives_only_when_intact);
    RUN_TEST(invalid_configurations_are_refused);
    return UNITY_END();
}
