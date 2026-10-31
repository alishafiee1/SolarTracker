// Cycle 008 — web API logic (rodo/suntracker/cycles/008-web-panel)
// The HTTP server and the panel pages are tested on the device; here the API is driven as a pure function.
#include <ArduinoJson.h>
#include <fakes.h>
#include <string.h>
#include <unity.h>

#include "sha256.h"
#include "web_api.h"

using webapi::Request;
using webapi::Response;

void setUp() {}
void tearDown() {}

static void hex(const uint8_t* in, char* out) {
    static const char* d = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out[i * 2] = d[in[i] >> 4];
        out[i * 2 + 1] = d[in[i] & 15];
    }
    out[64] = '\0';
}

// ------------------------------------------------------------ SHA-256
static void sha256_known_vectors() {
    char h[65];
    uint8_t out[32];
    {
        webapi::Sha256 s;
        s.update("abc", 3);
        s.final(out);
        hex(out, h);
        TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", h);
    }
    {
        webapi::Sha256 s;
        s.final(out);
        hex(out, h);
        TEST_ASSERT_EQUAL_STRING("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", h);
    }
    {
        const char* m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";  // two blocks
        webapi::Sha256 s;
        s.update(m, strlen(m));
        s.final(out);
        hex(out, h);
        TEST_ASSERT_EQUAL_STRING("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", h);
    }
    {  // splitting the input anywhere gives the same hash
        const char* m = "The quick brown fox jumps over the lazy dog";
        webapi::Sha256 a, b;
        a.update(m, strlen(m));
        for (size_t i = 0; i < strlen(m); ++i) b.update(m + i, 1);
        uint8_t oa[32], ob[32];
        a.final(oa);
        b.final(ob);
        TEST_ASSERT_EQUAL_MEMORY(oa, ob, 32);
        hex(oa, h);
        TEST_ASSERT_EQUAL_STRING("d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592", h);
    }
}

// ------------------------------------------------------------ rig
struct Rig {
    sim::FakeTime time;
    sim::FakeRelays relays;
    sim::FakeEventLog log;
    sim::FakeSettingsStore blobs;
    sim::FakeAccel accel;
    sim::FakeClock clock;
    sim::FakeRandom rng;
    settings::Settings cfg;
    settings::SettingsStore store;
    motion::MotionController motion;
    control::ControlStateMachine ctl;
    angle::AngleProvider angle;
    wind::WindMonitor wind;
    webapi::Auth auth;
    webapi::WebApi api;
    int changedCalls = 0;
    int networkChanged = 0;
    int testCalls = 0;
    int scansStarted = 0;
    webapi::WifiScan scanResult;
    size_t logStored = 4200;
    bool testResult = true;
    std::string logText;
    uint32_t nowMs = 100000;

    static webapi::WebApi makeApi(Rig& r) {
        webapi::Deps d{r.ctl, r.cfg, r.store, r.blobs, r.angle, r.clock, r.log, r.wind, r.auth};
        d.fwVersion = "test-1";
        d.defaultPassword = "default-pass-1";
        d.utcNow = [&r] { return r.clock.utc(); };
        d.onSettingsChanged = [&r] { ++r.changedCalls; };
        d.onNetworkChanged = [&r] { ++r.networkChanged; };
        d.sendTestMessage = [&r] {
            ++r.testCalls;
            return r.testResult;
        };
        d.startWifiScan = [&r] {
            ++r.scansStarted;
            r.scanResult.state = 1;
        };
        d.wifiScan = [&r] { return r.scanResult; };
        d.logBytes = [&r] { return r.logStored; };
        d.netInfo = [] {
            webapi::NetInfo n;
            n.connected = true;
            strcpy(n.ip, "192.168.1.50");
            n.lastNotify = 1;
            return n;
        };
        d.readLog = [&r](char* buf, size_t len, size_t n) {
            (void)n;
            snprintf(buf, len, "%s", r.logText.c_str());
            return strlen(buf);
        };
        return webapi::WebApi(d);
    }

    Rig() : relays(&time), store(blobs), motion(relays, motion::Timing(), 0), ctl(motion, cfg, &log), angle(accel), auth(rng), api(makeApi(*this)) {
        clock.setUtc(1813561200);
    }

    Response call(bool post, const char* path, const char* body = "", const std::string& cookie = "", const char* query = "") {
        Request r;
        r.post = post;
        r.path = path;
        r.body = body;
        r.cookie = cookie;
        r.query = query;
        r.nowMs = nowMs;
        return api.handle(r);
    }
    std::string login(const char* password = "default-pass-1") {
        char body[96];
        snprintf(body, sizeof(body), "{\"password\":\"%s\"}", password);
        const Response r = call(true, "/api/login", body);
        if (r.status != 200) return "";
        const size_t semi = r.setCookie.find(';');
        return r.setCookie.substr(0, semi);  // "st=<token>"
    }
};

static JsonDocument parse(const Response& r) {
    JsonDocument d;
    TEST_ASSERT_TRUE_MESSAGE(!deserializeJson(d, r.body), r.body.c_str());
    return d;
}

// ------------------------------------------------------------ public endpoints
static void status_is_public_and_complete() {
    Rig r;
    const Response res = r.call(false, "/api/status");
    TEST_ASSERT_EQUAL_INT(200, res.status);
    JsonDocument d = parse(res);
    TEST_ASSERT_EQUAL_STRING("auto", d["mode"] | "?");
    TEST_ASSERT_EQUAL_STRING("test-1", d["fwVersion"] | "?");
    TEST_ASSERT_EQUAL_STRING("ردیابی خورشید", d["text"] | "?");
    TEST_ASSERT_TRUE(d["fault"].isNull());
    TEST_ASSERT_TRUE(d["currentA"].isNull());
    TEST_ASSERT_EQUAL_INT64(1813561200, d["utc"].as<int64_t>());
    TEST_ASSERT_EQUAL_INT(210, d["tzOffsetMin"].as<int>());
    TEST_ASSERT_TRUE(d["passwordIsDefault"].as<bool>());
    TEST_ASSERT_FALSE(d["calibrated"].as<bool>());
}

static void cities_list_has_31_entries() {
    Rig r;
    JsonDocument d = parse(r.call(false, "/api/cities"));
    TEST_ASSERT_EQUAL_size_t(31, d["cities"].as<JsonArray>().size());
}

// ------------------------------------------------------------ R4 login
static void protected_endpoints_need_a_session() {
    Rig r;
    const char* paths[] = {"/api/settings", "/api/calibrate", "/api/log"};
    for (const char* p : paths) TEST_ASSERT_EQUAL_INT(401, r.call(false, p).status);
    const char* posts[] = {"/api/mode", "/api/manual", "/api/manual/keepalive", "/api/settings", "/api/location", "/api/clock", "/api/fault/reset", "/api/calibrate/east", "/api/password"};
    for (const char* p : posts) TEST_ASSERT_EQUAL_INT(401, r.call(true, p, "{}").status);
    TEST_ASSERT_EQUAL_INT(401, r.call(false, "/api/settings", "", "st=not-a-real-token").status);
    JsonDocument d = parse(r.call(false, "/api/settings"));
    TEST_ASSERT_EQUAL_STRING("login-required", d["error"] | "?");
}

static void login_with_the_default_password_sets_a_safe_cookie() {
    Rig r;
    const Response res = r.call(true, "/api/login", "{\"password\":\"default-pass-1\"}");
    TEST_ASSERT_EQUAL_INT(200, res.status);
    TEST_ASSERT_TRUE(res.setCookie.rfind("st=", 0) == 0);
    TEST_ASSERT_TRUE(res.setCookie.find("HttpOnly") != std::string::npos);
    TEST_ASSERT_TRUE(res.setCookie.find("SameSite=Strict") != std::string::npos);
    const size_t semi = res.setCookie.find(';');
    TEST_ASSERT_EQUAL_size_t(3 + 32, semi);  // "st=" + 128-bit token as 32 hex digits
    // The cookie opens the protected endpoints, also among other cookies.
    const std::string cookie = "a=b; " + res.setCookie.substr(0, semi) + "; c=d";
    TEST_ASSERT_EQUAL_INT(200, r.call(false, "/api/settings", "", cookie).status);
}

static void a_wrong_password_is_refused_and_five_failures_lock_logins() {
    Rig r;
    for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL_INT(401, r.call(true, "/api/login", "{\"password\":\"nope\"}").status);
    r.nowMs += 1000;
    TEST_ASSERT_EQUAL_INT(401, r.call(true, "/api/login", "{\"password\":\"nope\"}").status);  // the 5th
    r.nowMs += 1000;
    TEST_ASSERT_EQUAL_INT(429, r.call(true, "/api/login", "{\"password\":\"default-pass-1\"}").status);  // even the right one
    r.nowMs += 62000;
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/login", "{\"password\":\"default-pass-1\"}").status);
}

static void sessions_expire_after_30_idle_minutes_and_are_limited_to_four() {
    Rig r;
    const std::string c1 = r.login();
    TEST_ASSERT_EQUAL_INT(200, r.call(false, "/api/settings", "", c1).status);
    r.nowMs += 29 * 60 * 1000;
    TEST_ASSERT_EQUAL_INT(200, r.call(false, "/api/settings", "", c1).status);  // use refreshes the timer
    r.nowMs += 29 * 60 * 1000;
    TEST_ASSERT_EQUAL_INT(200, r.call(false, "/api/settings", "", c1).status);
    r.nowMs += 31 * 60 * 1000;
    TEST_ASSERT_EQUAL_INT(401, r.call(false, "/api/settings", "", c1).status);

    std::string cookies[5];
    for (int i = 0; i < 5; ++i) {
        r.nowMs += 1000;
        cookies[i] = r.login();
    }
    TEST_ASSERT_EQUAL_size_t(4, r.auth.sessionCount());
    TEST_ASSERT_EQUAL_INT(401, r.call(false, "/api/settings", "", cookies[0]).status);  // the oldest was dropped
    TEST_ASSERT_EQUAL_INT(200, r.call(false, "/api/settings", "", cookies[4]).status);
}

static void password_change_needs_the_old_one_and_a_strong_new_one() {
    Rig r;
    const std::string c = r.login();
    TEST_ASSERT_EQUAL_INT(401, r.call(true, "/api/password", "{\"old\":\"wrong\",\"new\":\"a-new-pass-77\"}", c).status);
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/password", "{\"old\":\"default-pass-1\",\"new\":\"short\"}", c).status);
    TEST_ASSERT_TRUE(r.cfg.passwordIsDefault);
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/password", "{\"old\":\"default-pass-1\",\"new\":\"a-new-pass-77\"}", c).status);
    TEST_ASSERT_FALSE(r.cfg.passwordIsDefault);
    TEST_ASSERT_EQUAL_INT(401, r.call(false, "/api/settings", "", c).status);  // every session ended
    r.nowMs += 70000;
    TEST_ASSERT_EQUAL_size_t(0, r.login("default-pass-1").size());               // the old default is gone
    TEST_ASSERT_TRUE(r.login("a-new-pass-77").size() > 0);
    // And it survives a restart: the hash is in the stored settings.
    settings::Settings loaded;
    r.store.load(loaded);
    TEST_ASSERT_FALSE(loaded.passwordIsDefault);
    TEST_ASSERT_EQUAL_MEMORY(r.cfg.settingsPasswordHash, loaded.settingsPasswordHash, 32);
}

// ------------------------------------------------------------ R5/R6 behaviour
static void settings_can_be_read_and_changed_with_validation() {
    Rig r;
    const std::string c = r.login();
    JsonDocument d = parse(r.call(false, "/api/settings", "", c));
    TEST_ASSERT_EQUAL_FLOAT(5.0f, d["moveThresholdDeg"].as<float>());
    TEST_ASSERT_EQUAL_FLOAT(6.8f, d["anemometerFactor"].as<float>());

    const Response ok = r.call(true, "/api/settings", "{\"moveThresholdDeg\":7,\"anemometerFactor\":5.5,\"unknownKey\":1}", c);
    TEST_ASSERT_EQUAL_INT(200, ok.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.cfg.moveThresholdDeg);
    TEST_ASSERT_EQUAL_FLOAT(5.5f, r.cfg.anemometerFactor);
    TEST_ASSERT_EQUAL_INT(1, r.changedCalls);
    settings::Settings loaded;
    r.store.load(loaded);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, loaded.moveThresholdDeg);
    TEST_ASSERT_TRUE(r.log.events().back().first == hal::EventType::SettingsChange);

    const Response bad = r.call(true, "/api/settings", "{\"moveThresholdDeg\":99}", c);
    TEST_ASSERT_EQUAL_INT(400, bad.status);
    JsonDocument e = parse(bad);
    TEST_ASSERT_EQUAL_STRING("moveThresholdDeg", e["field"] | "?");
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.cfg.moveThresholdDeg);  // unchanged
    TEST_ASSERT_EQUAL_INT(1, r.changedCalls);
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/settings", "not json", c).status);
}

static void hotspot_password_can_be_changed_but_is_never_sent_back() {
    Rig r;
    const std::string c = r.login();
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/settings", "{\"apPassword\":\"short\"}", c).status);
    TEST_ASSERT_EQUAL_STRING("", r.cfg.apPassword);
    const Response ok = r.call(true, "/api/settings", "{\"apPassword\":\"my-hotspot-pass\"}", c);
    TEST_ASSERT_EQUAL_INT(200, ok.status);
    TEST_ASSERT_EQUAL_STRING("my-hotspot-pass", r.cfg.apPassword);
    TEST_ASSERT_TRUE(ok.body.find("my-hotspot-pass") == std::string::npos);
    TEST_ASSERT_TRUE(r.call(false, "/api/settings", "", c).body.find("apPassword") == std::string::npos);
    TEST_ASSERT_TRUE(r.log.events().back().second.find("my-hotspot-pass") == std::string::npos);  // not in the event log either
}

static void location_by_city_or_pasted_coordinates() {
    Rig r;
    const std::string c = r.login();
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/location", "{\"city\":\"tehran\"}", c).status);
    TEST_ASSERT_EQUAL_STRING("tehran", r.cfg.cityId);
    TEST_ASSERT_EQUAL_FLOAT(35.689f, r.cfg.latDeg);
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/location", "{\"text\":\"36.5, 52.25\"}", c).status);
    TEST_ASSERT_EQUAL_STRING("custom", r.cfg.cityId);
    TEST_ASSERT_EQUAL_FLOAT(52.25f, r.cfg.lonDeg);
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/location", "{\"text\":\"95, 52\"}", c).status);
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/location", "{\"city\":\"atlantis\"}", c).status);
    TEST_ASSERT_EQUAL_FLOAT(52.25f, r.cfg.lonDeg);
}

static void clock_set_from_the_phone() {
    Rig r;
    const std::string c = r.login();
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/clock", "{\"utc\":1830000000}", c).status);
    TEST_ASSERT_EQUAL_INT64(1830000000, r.clock.utc());
    TEST_ASSERT_TRUE(r.log.events().back().first == hal::EventType::ClockSet);
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/clock", "{\"utc\":1000}", c).status);  // 1970: not a real time
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/clock", "{}", c).status);
    TEST_ASSERT_EQUAL_INT64(1830000000, r.clock.utc());
}

static void manual_commands_and_their_refusals() {
    Rig r;
    const std::string c = r.login();
    JsonDocument e = parse(r.call(true, "/api/manual", "{\"dir\":\"west\"}", c));
    TEST_ASSERT_EQUAL_STRING("need-manual-mode", e["error"] | "?");
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/mode", "{\"mode\":\"manual\"}", c).status);
    TEST_ASSERT_TRUE(r.cfg.mode == settings::Mode::Manual);  // remembered across reboots
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/manual", "{\"dir\":\"west\"}", c).status);
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/manual/keepalive", "{}", c).status);
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/manual", "{\"dir\":\"stop\"}", c).status);
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/manual", "{\"dir\":\"up\"}", c).status);
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/mode", "{\"mode\":\"turbo\"}", c).status);

    control::Inputs in;
    in.windStow = true;
    r.ctl.tick(r.time.nowMs(), in);
    JsonDocument w = parse(r.call(true, "/api/manual", "{\"dir\":\"east\"}", c));
    TEST_ASSERT_EQUAL_STRING("wind", w["error"] | "?");
    TEST_ASSERT_EQUAL_STRING("باد شدید — حالت ایمن", parse(r.call(false, "/api/status"))["text"] | "?");

    in.windStow = false;
    in.angleOk = false;
    r.ctl.tick(r.time.nowMs() + 100, in);
    JsonDocument f = parse(r.call(true, "/api/manual", "{\"dir\":\"east\"}", c));
    TEST_ASSERT_EQUAL_STRING("fault", f["error"] | "?");
    TEST_ASSERT_EQUAL_STRING("angle-sensor", parse(r.call(false, "/api/status"))["fault"] | "?");
    in.angleOk = true;
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/fault/reset", "{}", c).status);
    r.ctl.tick(r.time.nowMs() + 200, in);
    TEST_ASSERT_TRUE(parse(r.call(false, "/api/status"))["fault"].isNull());
}

static void calibration_wizard() {
    Rig r;
    const std::string c = r.login();
    TEST_ASSERT_EQUAL_INT(409, r.call(true, "/api/calibrate/east", "{}", c).status);  // east cannot come first
    TEST_ASSERT_EQUAL_INT(409, r.call(true, "/api/calibrate/save", "{}", c).status);  // nothing to save yet

    r.accel.setGravity({0.0f, 0.0f, 1.0f});
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/calibrate/horizontal", "{}", c).status);
    TEST_ASSERT_EQUAL_STRING("pending", parse(r.call(false, "/api/calibrate", "", c))["result"] | "?");
    for (int i = 0; i < 50; ++i) r.angle.sample();
    TEST_ASSERT_EQUAL_STRING("ok", parse(r.call(false, "/api/calibrate", "", c))["result"] | "?");

    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/calibrate/east", "{}", c).status);
    r.accel.setGravity({0.0f, 0.1392f, 0.9903f});  // about 8 degrees
    for (int i = 0; i < 50; ++i) r.angle.sample();
    JsonDocument done = parse(r.call(false, "/api/calibrate", "", c));
    TEST_ASSERT_EQUAL_STRING("ok", done["result"] | "?");
    TEST_ASSERT_TRUE(done["calibrated"].as<bool>());

    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/calibrate/save", "{}", c).status);
    TEST_ASSERT_TRUE(r.blobs.size("angle_cal") > 0);
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/calibrate/cancel", "{}", c).status);
    TEST_ASSERT_EQUAL_INT(404, r.call(true, "/api/calibrate/dance", "{}", c).status);
}

static void log_lines_come_back_as_json() {
    Rig r;
    const std::string c = r.login();
    r.logText = "1813561200,PowerUp,power-on\n1813561260,MoveStart,dir=west angle=3.0\n1813561300,Fault,stall; retry 1\n";
    JsonDocument d = parse(r.call(false, "/api/log", "", c, "n=50"));
    JsonArray a = d["events"].as<JsonArray>();
    TEST_ASSERT_EQUAL_size_t(3, a.size());
    TEST_ASSERT_EQUAL_STRING("MoveStart", a[1]["type"] | "?");
    TEST_ASSERT_EQUAL_INT64(1813561300, a[2]["t"].as<int64_t>());
    TEST_ASSERT_EQUAL_STRING("stall; retry 1", a[2]["detail"] | "?");
}

// Module 011: site Wi-Fi and notification settings. The password and the bot token are write-only.
static void network_and_notification_settings_never_return_their_secrets() {
    Rig r;
    const std::string c = r.login();
    TEST_ASSERT_EQUAL_INT(401, r.call(true, "/api/network", "{\"ssid\":\"x\"}").status);
    const Response res = r.call(true, "/api/network", "{\"ssid\":\"HomeWifi\",\"password\":\"router-pass-1\"}", c);
    TEST_ASSERT_EQUAL_INT(200, res.status);
    TEST_ASSERT_TRUE(res.body.find("router-pass-1") == std::string::npos);
    TEST_ASSERT_EQUAL_INT(1, r.networkChanged);
    const Response n = r.call(true, "/api/notify", "{\"token\":\"123456:ABC-DEF\",\"chatId\":\"987\",\"enabled\":true}", c);
    TEST_ASSERT_EQUAL_INT(200, n.status);
    TEST_ASSERT_TRUE(n.body.find("123456:ABC-DEF") == std::string::npos);
    JsonDocument d = parse(r.call(false, "/api/network", "", c));
    TEST_ASSERT_EQUAL_STRING("HomeWifi", d["ssid"] | "?");
    TEST_ASSERT_TRUE(d["passwordSet"].as<bool>());
    TEST_ASSERT_TRUE(d["tokenSet"].as<bool>());
    TEST_ASSERT_TRUE(d["enabled"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("987", d["chatId"] | "?");
    // Nothing secret in the event log either.
    for (const auto& e : r.log.events()) {
        TEST_ASSERT_TRUE(e.second.find("router-pass-1") == std::string::npos);
        TEST_ASSERT_TRUE(e.second.find("123456:ABC-DEF") == std::string::npos);
    }
    // A later change of only the SSID keeps the stored password and token.
    r.call(true, "/api/network", "{\"ssid\":\"OtherWifi\"}", c);
    net::NetConfig stored;
    TEST_ASSERT_TRUE(net::load(r.blobs, stored));
    TEST_ASSERT_EQUAL_STRING("OtherWifi", stored.ssid);
    TEST_ASSERT_EQUAL_STRING("router-pass-1", stored.password);
    TEST_ASSERT_EQUAL_STRING("123456:ABC-DEF", stored.botToken);
}

static void invalid_network_values_are_refused() {
    Rig r;
    const std::string c = r.login();
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/network", "{\"ssid\":\"Home\",\"password\":\"short\"}", c).status);
    TEST_ASSERT_EQUAL_INT(400, r.call(true, "/api/notify", "{\"token\":\"bad/token\"}", c).status);
    TEST_ASSERT_EQUAL_INT(0, r.networkChanged);
}

static void the_test_message_needs_a_configuration_and_reports_failure() {
    Rig r;
    const std::string c = r.login();
    TEST_ASSERT_EQUAL_INT(409, r.call(true, "/api/notify/test", "{}", c).status);
    TEST_ASSERT_EQUAL_INT(0, r.testCalls);
    r.call(true, "/api/notify", "{\"token\":\"123456:ABC\",\"chatId\":\"987\",\"enabled\":true}", c);
    TEST_ASSERT_EQUAL_INT(200, r.call(true, "/api/notify/test", "{}", c).status);
    TEST_ASSERT_EQUAL_INT(1, r.testCalls);
    r.testResult = false;
    TEST_ASSERT_EQUAL_INT(502, r.call(true, "/api/notify/test", "{}", c).status);
}

static void status_shows_the_site_connection_without_secrets() {
    Rig r;
    JsonDocument d = parse(r.call(false, "/api/status"));
    TEST_ASSERT_TRUE(d["staConnected"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("192.168.1.50", d["staIp"] | "?");
    TEST_ASSERT_TRUE(d["lastNotifyOk"].as<bool>());
}

// Cycle 015: the scan is started by a POST and read by a GET; both need a login.
static void the_wifi_scan_is_started_and_read_in_two_steps() {
    Rig r;
    TEST_ASSERT_EQUAL_INT(401, r.call(true, "/api/wifi/scan", "{}").status);
    TEST_ASSERT_EQUAL_INT(401, r.call(false, "/api/wifi/scan").status);
    const std::string c = r.login();
    JsonDocument started = parse(r.call(true, "/api/wifi/scan", "{}", c));
    TEST_ASSERT_EQUAL_INT(1, r.scansStarted);
    TEST_ASSERT_EQUAL_STRING("scanning", started["state"] | "?");
    TEST_ASSERT_EQUAL_size_t(0, started["networks"].as<JsonArray>().size());
    r.scanResult.state = 2;
    r.scanResult.networks = {{"WORKSHOP", -48, true}, {"Cafe", -72, false}};
    JsonDocument done = parse(r.call(false, "/api/wifi/scan", "", c));
    TEST_ASSERT_EQUAL_STRING("done", done["state"] | "?");
    JsonArray a = done["networks"].as<JsonArray>();
    TEST_ASSERT_EQUAL_size_t(2, a.size());
    TEST_ASSERT_EQUAL_STRING("WORKSHOP", a[0]["ssid"] | "?");
    TEST_ASSERT_EQUAL_INT(-48, a[0]["rssi"].as<int>());
    TEST_ASSERT_TRUE(a[0]["secure"].as<bool>());
    TEST_ASSERT_FALSE(a[1]["secure"].as<bool>());
}

// Cycle 017: the log answer says how full the log is.
static void the_log_answer_includes_the_fill_of_the_log() {
    Rig r;
    const std::string c = r.login();
    JsonDocument d = parse(r.call(false, "/api/log", "", c));
    TEST_ASSERT_EQUAL_UINT32(4200, d["storedBytes"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(131072, d["maxBytes"].as<uint32_t>());
}

static void unknown_paths_are_404() {
    Rig r;
    const std::string c = r.login();
    TEST_ASSERT_EQUAL_INT(404, r.call(false, "/index.html").status);
    TEST_ASSERT_EQUAL_INT(404, r.call(false, "/api/nothing", "", c).status);
}

static void persian_status_texts_exist_for_every_id() {
    for (int i = 0; i <= static_cast<int>(control::StatusText::FaultOvercurrent); ++i) {
        const std::string t = webapi::statusTextFa(static_cast<control::StatusText>(i), 7);
        TEST_ASSERT_TRUE(t.size() > 4);
    }
    TEST_ASSERT_EQUAL_STRING("منتظر تأخیر ایمنی (7 ثانیه)", webapi::statusTextFa(control::StatusText::WaitingSafety, 7).c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(sha256_known_vectors);
    RUN_TEST(status_is_public_and_complete);
    RUN_TEST(cities_list_has_31_entries);
    RUN_TEST(protected_endpoints_need_a_session);
    RUN_TEST(login_with_the_default_password_sets_a_safe_cookie);
    RUN_TEST(a_wrong_password_is_refused_and_five_failures_lock_logins);
    RUN_TEST(sessions_expire_after_30_idle_minutes_and_are_limited_to_four);
    RUN_TEST(password_change_needs_the_old_one_and_a_strong_new_one);
    RUN_TEST(settings_can_be_read_and_changed_with_validation);
    RUN_TEST(hotspot_password_can_be_changed_but_is_never_sent_back);
    RUN_TEST(location_by_city_or_pasted_coordinates);
    RUN_TEST(clock_set_from_the_phone);
    RUN_TEST(manual_commands_and_their_refusals);
    RUN_TEST(calibration_wizard);
    RUN_TEST(log_lines_come_back_as_json);
    RUN_TEST(network_and_notification_settings_never_return_their_secrets);
    RUN_TEST(invalid_network_values_are_refused);
    RUN_TEST(the_test_message_needs_a_configuration_and_reports_failure);
    RUN_TEST(status_shows_the_site_connection_without_secrets);
    RUN_TEST(the_wifi_scan_is_started_and_read_in_two_steps);
    RUN_TEST(the_log_answer_includes_the_fill_of_the_log);
    RUN_TEST(unknown_paths_are_404);
    RUN_TEST(persian_status_texts_exist_for_every_id);
    return UNITY_END();
}
