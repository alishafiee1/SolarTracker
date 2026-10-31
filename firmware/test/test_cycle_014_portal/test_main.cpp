// Cycle 014 — captive portal redirect decision (rodo/suntracker/cycles/014-captive-portal)
#include <unity.h>

#include "portal.h"

using web::Redirect;

void setUp() {}
void tearDown() {}

static const uint8_t kHotspot[4] = {192, 168, 4, 1};
static const uint8_t kLan[4] = {192, 168, 1, 102};

static void probes_from_the_hotspot_go_to_the_panel_address() {
    for (const char* p : {"/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html", "/connecttest.txt", "/ncsi.txt", "/redirect",
                          "/canonical.html", "/success.txt"}) {
        TEST_ASSERT_TRUE(web::isKnownProbe(p));
        TEST_ASSERT_TRUE_MESSAGE(web::redirectFor(kHotspot, p) == Redirect::Absolute, p);
    }
}

static void any_unknown_path_from_the_hotspot_is_redirected_too() {
    TEST_ASSERT_TRUE(web::redirectFor(kHotspot, "/favicon.ico") == Redirect::Absolute);
    TEST_ASSERT_TRUE(web::redirectFor(kHotspot, "/some/page") == Redirect::Absolute);
    TEST_ASSERT_FALSE(web::isKnownProbe("/favicon.ico"));
}

// A computer on the home network keeps the address it typed.
static void requests_from_the_site_network_get_a_relative_redirect() {
    TEST_ASSERT_TRUE(web::redirectFor(kLan, "/favicon.ico") == Redirect::Relative);
    TEST_ASSERT_TRUE(web::redirectFor(kLan, "/generate_204") == Redirect::Relative);
}

static void the_panel_and_the_api_are_never_redirected() {
    TEST_ASSERT_TRUE(web::redirectFor(kHotspot, "/") == Redirect::None);
    TEST_ASSERT_TRUE(web::redirectFor(kLan, "/") == Redirect::None);
    TEST_ASSERT_TRUE(web::redirectFor(kHotspot, "/api/status") == Redirect::None);
    TEST_ASSERT_TRUE(web::redirectFor(kLan, "/api/log.csv") == Redirect::None);
}

static void only_the_exact_hotspot_address_counts() {
    const uint8_t near1[4] = {192, 168, 4, 2};
    const uint8_t near2[4] = {192, 168, 5, 1};
    TEST_ASSERT_FALSE(web::isHotspotAddress(near1));
    TEST_ASSERT_FALSE(web::isHotspotAddress(near2));
    TEST_ASSERT_TRUE(web::isHotspotAddress(kHotspot));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(probes_from_the_hotspot_go_to_the_panel_address);
    RUN_TEST(any_unknown_path_from_the_hotspot_is_redirected_too);
    RUN_TEST(requests_from_the_site_network_get_a_relative_redirect);
    RUN_TEST(the_panel_and_the_api_are_never_redirected);
    RUN_TEST(only_the_exact_hotspot_address_counts);
    return UNITY_END();
}
