// Cycle 015 — Wi-Fi scan list (rodo/suntracker/cycles/015-wifi-scan)
#include <unity.h>

#include <string>
#include <vector>

#include "net_logic.h"

using net::ScanEntry;

void setUp() {}
void tearDown() {}

static void the_strongest_network_comes_first() {
    std::vector<ScanEntry> e = {{"Far", -85, true}, {"Near", -40, true}, {"Mid", -65, false}};
    net::tidyScan(e);
    TEST_ASSERT_EQUAL_size_t(3, e.size());
    TEST_ASSERT_EQUAL_STRING("Near", e[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Mid", e[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Far", e[2].ssid.c_str());
    TEST_ASSERT_FALSE(e[1].secure);
}

static void an_ssid_seen_by_several_access_points_appears_once_with_its_best_signal() {
    std::vector<ScanEntry> e = {{"Office", -80, true}, {"Office", -55, true}, {"Office", -70, true}, {"Other", -60, true}};
    net::tidyScan(e);
    TEST_ASSERT_EQUAL_size_t(2, e.size());
    TEST_ASSERT_EQUAL_STRING("Office", e[0].ssid.c_str());
    TEST_ASSERT_EQUAL_INT(-55, e[0].rssi);
}

static void hidden_networks_are_left_out() {
    std::vector<ScanEntry> e = {{"", -30, true}, {"Visible", -70, true}};
    net::tidyScan(e);
    TEST_ASSERT_EQUAL_size_t(1, e.size());
    TEST_ASSERT_EQUAL_STRING("Visible", e[0].ssid.c_str());
}

static void the_list_is_capped() {
    std::vector<ScanEntry> e;
    for (int i = 0; i < 40; ++i) e.push_back({"net" + std::to_string(i), -30 - i, true});
    net::tidyScan(e);
    TEST_ASSERT_EQUAL_size_t(20, e.size());
    TEST_ASSERT_EQUAL_STRING("net0", e[0].ssid.c_str());  // the strongest are kept
    TEST_ASSERT_EQUAL_STRING("net19", e[19].ssid.c_str());
}

static void an_empty_scan_gives_an_empty_list() {
    std::vector<ScanEntry> e;
    net::tidyScan(e);
    TEST_ASSERT_EQUAL_size_t(0, e.size());
}

static void names_are_case_sensitive_and_kept_as_typed() {
    std::vector<ScanEntry> e = {{"Home", -50, true}, {"home", -60, true}, {"گوشی من", -70, true}};
    net::tidyScan(e);
    TEST_ASSERT_EQUAL_size_t(3, e.size());  // two networks that differ only by case are two networks
    TEST_ASSERT_EQUAL_STRING("گوشی من", e[2].ssid.c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(the_strongest_network_comes_first);
    RUN_TEST(an_ssid_seen_by_several_access_points_appears_once_with_its_best_signal);
    RUN_TEST(hidden_networks_are_left_out);
    RUN_TEST(the_list_is_capped);
    RUN_TEST(an_empty_scan_gives_an_empty_list);
    RUN_TEST(names_are_case_sensitive_and_kept_as_typed);
    return UNITY_END();
}
