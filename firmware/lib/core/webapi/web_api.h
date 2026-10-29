#pragma once
// REST API of the web panel as a pure function of a request (cycle 008). The HTTP server only
// converts requests and responses; routing, login, validation and the mapping onto the control
// logic live here so they are tested on the host.

#include <hal.h>
#include <stdint.h>

#include <functional>
#include <string>

#include "angle_provider.h"
#include "auth.h"
#include "control_state_machine.h"
#include "net_logic.h"
#include "settings.h"
#include "wind_monitor.h"

namespace webapi {

struct Request {
    bool post = false;
    std::string path;    // "/api/status"
    std::string query;   // "n=100" (without the question mark)
    std::string body;    // JSON text
    std::string cookie;  // value of the Cookie header
    uint32_t nowMs = 0;
};

struct Response {
    int status = 200;
    std::string body;       // always JSON
    std::string setCookie;  // full Set-Cookie header value, empty if none
};

// Site Wi-Fi and notification state shown by the API (module 011). No secrets.
struct NetInfo {
    bool connected = false;
    char ip[16] = "";
    int lastNotify = -1;  // -1 never tried, 0 failed, 1 sent
};

struct Deps {
    Deps(control::ControlStateMachine& c, settings::Settings& s, settings::SettingsStore& st, hal::ISettingsStore& b, angle::AngleProvider& a,
         hal::IClock& cl, hal::IEventLog& l, wind::WindMonitor& w, Auth& au)
        : ctl(c), cfg(s), store(st), blobs(b), angle(a), clock(cl), log(l), wind(w), auth(au) {}

    control::ControlStateMachine& ctl;
    settings::Settings& cfg;
    settings::SettingsStore& store;
    hal::ISettingsStore& blobs;  // raw blobs (the angle calibration)
    angle::AngleProvider& angle;
    hal::IClock& clock;
    hal::IEventLog& log;
    wind::WindMonitor& wind;
    Auth& auth;
    const char* fwVersion = "dev";
    const char* defaultPassword = "";
    std::function<bool()> clockValid = [] { return true; };
    std::function<int64_t()> utcNow = [] { return int64_t{0}; };
    std::function<size_t(char*, size_t, size_t)> readLog;  // (buffer, size, newest N lines) -> bytes
    std::function<void()> onSettingsChanged = [] {};
    // Module 011 (all optional): the stored network configuration lives in `blobs`.
    std::function<void()> onNetworkChanged = [] {};
    std::function<NetInfo()> netInfo = [] { return NetInfo(); };
    std::function<bool()> sendTestMessage = [] { return false; };
};

class WebApi {
public:
    explicit WebApi(Deps deps) : d_(std::move(deps)) {}
    Response handle(const Request& req);
    // True when the request carries a live session cookie (for endpoints that stream, like the CSV download).
    bool isLoggedIn(const Request& req);

private:
    Response status();
    Response login(const Request& req);
    Response getSettings();
    Response postSettings(const Request& req);
    Response postLocation(const Request& req);
    Response postClock(const Request& req);
    Response postMode(const Request& req);
    Response postManual(const Request& req);
    Response calibrate(const Request& req, const std::string& step);
    Response getCalibrate();
    Response getLog(const Request& req);
    Response postPassword(const Request& req);
    Response cities();
    Response getNetwork();
    Response postNetwork(const Request& req);
    Response postNotify(const Request& req);
    Response testNotify();
    bool saveCfg(const settings::Settings& next, Response& err);

    Deps d_;
    uint32_t nowMs_ = 0;
};

// Persian text of a status id, with the safety wait inserted where it has a number.
std::string statusTextFa(control::StatusText t, uint32_t waitS);

}  // namespace webapi
