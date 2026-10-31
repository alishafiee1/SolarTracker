// Desktop simulator of the whole device (no hardware needed). It runs the real control stack and the real web API on top
// of the simulated actuator, sensors, wind and clock (lib/sim), and talks JSON lines over stdin/stdout to sim_server.py,
// which serves the real panel files over HTTP. Build: pio run -d firmware -e sim
#include <ArduinoJson.h>

#include <iostream>
#include <string>

#include "csv_format.h"
#include "event_format.h"
#include "web_api.h"
#include "world.h"

namespace {

sim::World* gWorld = nullptr;
double gWind = 0.0;

std::string readLogLines(size_t n) {
    // Same line format as the device log: "utc,type,detail\n", oldest first, newest `n` lines.
    const auto& ev = gWorld->log().events();
    const auto& tm = gWorld->log().times();
    const size_t from = ev.size() > n ? ev.size() - n : 0;
    std::string out;
    for (size_t i = from; i < ev.size(); ++i) {
        char line[events::kMaxLine];
        if (events::formatLine(line, sizeof(line), tm[i], ev[i].first, ev[i].second.c_str()) > 0) out += line;
    }
    return out;
}

void reply(const JsonDocument& d) {
    std::string s;
    serializeJson(d, s);
    std::cout << s << "\n" << std::flush;
}

}  // namespace

int main() {
    const int64_t start = 1813561200;  // 2027-06-21 07:00 UTC: morning in Kerman
    sim::World world(7, start, settings::Mode::Auto);
    gWorld = &world;
    world.windKmh = [](uint32_t) { return gWind; };
    world.calibrate();
    world.placePanel(0.0);

    sim::FakeRandom rng;
    webapi::Auth auth(rng);
    webapi::Deps deps(world.ctl(), world.cfg(), world.store(), world.blobs(), world.angle(), world.clock(), world.log(), world.wind(), auth);
    deps.fwVersion = "sim";
    deps.defaultPassword = "sim";
    deps.clockValid = [] { return gWorld->clockValid; };
    deps.utcNow = [] { return gWorld->clockValid ? gWorld->utc() : int64_t{0}; };
    deps.readLog = [](char* buf, size_t len, size_t n) {
        const std::string s = readLogLines(n);
        const size_t k = s.size() < len - 1 ? s.size() : len - 1;
        memcpy(buf, s.data(), k);
        buf[k] = '\0';
        return k;
    };
    deps.logBytes = [] { return readLogLines(100000).size(); };
    deps.onSettingsChanged = [&world] {
        motion::Timing t;
        t.stopSettleMs = world.cfg().stopSettleMs;
        t.minMoveGapMs = world.cfg().minMoveGapMs;
        t.maxMoveMs = world.cfg().maxMoveMs;
        t.currentPresent = world.cfg().currentPresent;
        t.zeroCurrentA = world.cfg().zeroCurrentA;
        world.motion().setTiming(t);
    };
    deps.netInfo = [] {
        webapi::NetInfo n;
        n.connected = true;
        strcpy(n.ip, "127.0.0.1");
        n.lastNotify = 1;
        return n;
    };
    deps.sendTestMessage = [] { return true; };
    deps.startWifiScan = [] {};
    deps.wifiScan = [] {
        webapi::WifiScan s;
        s.state = 2;
        s.networks = {{"WORKSHOP", -48, true}, {"Home-2.4G", -61, true}, {"Cafe", -77, false}};
        return s;
    };
    webapi::WebApi api(deps);

    std::cout << "{\"ready\":true}\n" << std::flush;
    std::string line;
    while (std::getline(std::cin, line)) {
        JsonDocument in;
        JsonDocument out;
        if (deserializeJson(in, line)) {
            out["error"] = "bad request line";
            reply(out);
            continue;
        }
        const std::string op = in["op"] | "";
        if (op == "step") {
            const int n = in["steps"] | 1;
            for (int i = 0; i < n; ++i) world.step();
            out["ok"] = true;
        } else if (op == "api") {
            webapi::Request req;
            req.post = std::string(in["method"] | "GET") == "POST";
            req.path = in["path"] | "/";
            req.query = in["query"] | "";
            req.body = in["body"] | "";
            req.cookie = in["cookie"] | "";
            req.nowMs = world.elapsedMs();  // the same clock the control logic uses
            if (req.path == "/api/log.csv") {
                if (!api.isLoggedIn(req)) {
                    out["status"] = 401;
                    out["body"] = "{\"error\":\"login-required\"}";
                } else {
                    std::string csv;
                    char buf[320];
                    const size_t h = events::csvHeader(buf, sizeof(buf));
                    csv.assign(buf, h);
                    const auto& ev = world.log().events();
                    const auto& tm = world.log().times();
                    for (size_t i = 0; i < ev.size(); ++i) {
                        const size_t n = events::csvRow(buf, sizeof(buf), tm[i], world.cfg().tzOffsetMin, events::typeName(ev[i].first), ev[i].second.c_str());
                        if (n) csv.append(buf, n);
                    }
                    out["status"] = 200;
                    out["body"] = csv;
                    out["csv"] = true;
                }
            } else {
                const webapi::Response r = api.handle(req);
                if (r.status == 200 && req.post && req.path == "/api/clock") {  // the simulated clock chip changed: follow it
                    world.setUtc(world.clock().utc());
                    world.clockValid = true;
                }
                out["status"] = r.status;
                out["body"] = r.body;
                out["cookie"] = r.setCookie;
            }
        } else if (op == "sim") {
            JsonObjectConst s = in["set"].as<JsonObjectConst>();
            if (s["windKmh"].is<double>()) gWind = s["windKmh"].as<double>();
            if (s["jammed"].is<bool>()) world.act.jammed = s["jammed"].as<bool>();
            if (s["wireBroken"].is<bool>()) world.act.wireBroken = s["wireBroken"].as<bool>();
            if (s["relayStuck"].is<bool>()) world.act.relayStuck = s["relayStuck"].as<bool>();
            if (s["sensorUnplugged"].is<bool>()) world.sensorUnplugged = s["sensorUnplugged"].as<bool>();
            if (s["clockValid"].is<bool>()) world.clockValid = s["clockValid"].as<bool>();
            if (s["currentModule"].is<bool>() && s["currentModule"].as<bool>()) world.enableCurrentModule();
            if (s["utc"].is<int64_t>()) world.setUtc(s["utc"].as<int64_t>());
            if (s["panelAngle"].is<double>()) world.placePanel(s["panelAngle"].as<double>());
            if (s["calibrate"].is<bool>() && s["calibrate"].as<bool>()) world.calibrate();
            if (s["runCurrentA"].is<double>()) world.act.runCurrentA = s["runCurrentA"].as<double>();
            out["ok"] = true;
        } else if (op == "state") {
            out["utc"] = world.utc();
            out["elapsedMs"] = world.elapsedMs();
            out["actualAngle"] = world.act.angleDeg;
            out["actuatorCurrentA"] = world.actuatorCurrentA();
            out["ch1"] = world.relays().power();
            out["ch2"] = world.relays().direction() == hal::Dir::West ? "west" : "east";
            out["windKmh"] = gWind;
            out["jammed"] = world.act.jammed;
            out["wireBroken"] = world.act.wireBroken;
            out["relayStuck"] = world.act.relayStuck;
            out["sensorUnplugged"] = world.sensorUnplugged;
            out["clockValid"] = world.clockValid;
            out["powerOns"] = world.powerOns();
            out["events"] = static_cast<uint32_t>(world.log().events().size());
            out["violations"] = world.relays().violations();
        } else {
            out["error"] = "unknown op";
        }
        reply(out);
    }
    return 0;
}
