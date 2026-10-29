#include "web_api.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "event_format.h"
#include "location.h"

namespace webapi {

namespace {

constexpr int64_t kMinUtc = 1704067200;  // 2024-01-01: anything earlier is not a set clock
constexpr int64_t kMaxUtc = 4102444800;  // 2100-01-01

Response json(int status, const JsonDocument& doc) {
    Response r;
    r.status = status;
    serializeJson(doc, r.body);
    return r;
}

Response error(int status, const char* code, const char* messageFa, const char* field = nullptr) {
    JsonDocument d;
    d["error"] = code;
    d["message"] = messageFa;
    if (field) d["field"] = field;
    return json(status, d);
}

Response ok() {
    JsonDocument d;
    d["ok"] = true;
    return json(200, d);
}

std::string cookieValue(const std::string& header, const char* name) {
    const std::string key = std::string(name) + "=";
    size_t pos = 0;
    while ((pos = header.find(key, pos)) != std::string::npos) {
        if (pos == 0 || header[pos - 1] == ' ' || header[pos - 1] == ';') {
            const size_t start = pos + key.size();
            const size_t end = header.find(';', start);
            return header.substr(start, end == std::string::npos ? std::string::npos : end - start);
        }
        pos += key.size();
    }
    return "";
}

const char* modeName(settings::Mode m) { return m == settings::Mode::Manual ? "manual" : "auto"; }

// Settings that the panel may read and write: name -> member.
struct FloatField { const char* name; float settings::Settings::*ptr; };
struct UintField { const char* name; uint32_t settings::Settings::*ptr; };
const FloatField kFloats[] = {
    {"latDeg", &settings::Settings::latDeg}, {"lonDeg", &settings::Settings::lonDeg}, {"minDeg", &settings::Settings::minDeg},
    {"maxDeg", &settings::Settings::maxDeg}, {"moveThresholdDeg", &settings::Settings::moveThresholdDeg},
    {"windStowKmh", &settings::Settings::windStowKmh}, {"windReturnKmh", &settings::Settings::windReturnKmh},
    {"anemometerFactor", &settings::Settings::anemometerFactor}, {"supplyLowV", &settings::Settings::supplyLowV},
};
const UintField kUints[] = {
    {"stallCheckMs", &settings::Settings::stallCheckMs}, {"preDawnMinutes", &settings::Settings::preDawnMinutes},
    {"stopSettleMs", &settings::Settings::stopSettleMs}, {"minMoveGapMs", &settings::Settings::minMoveGapMs},
    {"maxMoveMs", &settings::Settings::maxMoveMs}, {"windStowDelayS", &settings::Settings::windStowDelayS},
    {"windReturnMin", &settings::Settings::windReturnMin},
};

}  // namespace

std::string statusTextFa(control::StatusText t, uint32_t waitS) {
    using T = control::StatusText;
    switch (t) {
        case T::Tracking: return "ردیابی خورشید";
        case T::NightStow: return "شب — حالت ایمن";
        case T::WindStow: return "باد شدید — حالت ایمن";
        case T::ManualMode: return "حالت دستی";
        case T::FaultStall: return "خطا: گیرکردن بازو";
        case T::FaultMoveTimeout: return "خطا: زمان حرکت تمام شد";
        case T::FaultAngleSensor: return "خطا: سنسور زاویه";
        case T::FaultInterlock: return "خطا: نقض قفل جهت";
        case T::FaultResetLoop: return "خطا: ریست‌های پیاپی";
        case T::WaitingSafety: {
            char buf[64];
            snprintf(buf, sizeof(buf), "منتظر تأخیر ایمنی (%lu ثانیه)", static_cast<unsigned long>(waitS));
            return buf;
        }
        case T::ClockNotSet: return "ساعت تنظیم نشده";
        case T::NeedCalibration: return "کالیبراسیون لازم است";
    }
    return "";
}

Response WebApi::handle(const Request& req) {
    nowMs_ = req.nowMs;
    const std::string& p = req.path;
    // Public endpoints.
    if (!req.post && p == "/api/status") return status();
    if (!req.post && p == "/api/cities") return cities();
    if (req.post && p == "/api/login") return login(req);

    // Everything else needs a session.
    if (p.rfind("/api/", 0) != 0) return error(404, "not-found", "پیدا نشد");
    const std::string token = cookieValue(req.cookie, "st");
    if (!d_.auth.check(token.c_str(), req.nowMs)) return error(401, "login-required", "ابتدا وارد شوید");

    if (req.post && p == "/api/logout") {
        d_.auth.logout(token.c_str());
        return ok();
    }
    if (!req.post && p == "/api/settings") return getSettings();
    if (req.post && p == "/api/settings") return postSettings(req);
    if (req.post && p == "/api/location") return postLocation(req);
    if (req.post && p == "/api/clock") return postClock(req);
    if (req.post && p == "/api/mode") return postMode(req);
    if (req.post && p == "/api/manual") return postManual(req);
    if (req.post && p == "/api/manual/keepalive") {
        d_.ctl.command(control::Command::KeepAlive, req.nowMs);
        return ok();
    }
    if (req.post && p == "/api/fault/reset") {
        d_.ctl.command(control::Command::ResetFault, req.nowMs);
        return ok();
    }
    if (!req.post && p == "/api/calibrate") return getCalibrate();
    if (req.post && p.rfind("/api/calibrate/", 0) == 0) return calibrate(req, p.substr(15));
    if (!req.post && p == "/api/log") return getLog(req);
    if (req.post && p == "/api/password") return postPassword(req);
    return error(404, "not-found", "پیدا نشد");
}

Response WebApi::status() {
    JsonDocument d;
    const control::Status s = d_.ctl.status(nowMs_);
    d["angleDeg"] = s.angleDeg;
    d["targetDeg"] = s.targetDeg;
    d["windKmh"] = d_.wind.speedKmh();
    d["windStow"] = d_.wind.stow();
    d["currentA"] = nullptr;
    d["mode"] = modeName(d_.ctl.mode());
    d["state"] = control::stateName(s.state);
    d["statusId"] = static_cast<int>(s.text);
    d["text"] = statusTextFa(s.text, s.waitS);
    if (s.fault == control::FaultCode::None) d["fault"] = nullptr;
    else d["fault"] = control::faultName(s.fault);
    d["moving"] = s.moving;
    d["waitingS"] = s.waitS;
    d["clockValid"] = d_.clockValid();
    d["calibrated"] = d_.angle.calibrated();
    d["utc"] = d_.utcNow();
    d["tzOffsetMin"] = d_.cfg.tzOffsetMin;
    d["fwVersion"] = d_.fwVersion;
    d["passwordIsDefault"] = d_.cfg.passwordIsDefault;
    return json(200, d);
}

Response WebApi::login(const Request& req) {
    JsonDocument in;
    if (deserializeJson(in, req.body)) return error(400, "bad-json", "درخواست نامعتبر است");
    std::string token;
    const char* password = in["password"] | "";
    switch (d_.auth.login(d_.cfg, d_.defaultPassword, password, req.nowMs, token)) {
        case LoginResult::Locked: return error(429, "locked", "تلاش زیاد؛ یک دقیقه صبر کنید");
        case LoginResult::WrongPassword: return error(401, "wrong-password", "رمز اشتباه است");
        case LoginResult::Ok: break;
    }
    JsonDocument d;
    d["ok"] = true;
    d["passwordIsDefault"] = d_.cfg.passwordIsDefault;
    Response r = json(200, d);
    r.setCookie = "st=" + token + "; HttpOnly; SameSite=Strict; Path=/; Max-Age=1800";
    return r;
}

Response WebApi::cities() {
    JsonDocument d;
    JsonArray a = d["cities"].to<JsonArray>();
    for (size_t i = 0; i < settings::kCityCount; ++i) {
        JsonObject o = a.add<JsonObject>();
        o["id"] = settings::kCities[i].id;
        o["name"] = settings::kCities[i].nameFa;
        o["lat"] = settings::kCities[i].lat;
        o["lon"] = settings::kCities[i].lon;
    }
    return json(200, d);
}

Response WebApi::getSettings() {
    JsonDocument d;
    for (const FloatField& f : kFloats) d[f.name] = d_.cfg.*f.ptr;
    for (const UintField& f : kUints) d[f.name] = d_.cfg.*f.ptr;
    d["cityId"] = d_.cfg.cityId;
    d["tzOffsetMin"] = d_.cfg.tzOffsetMin;
    d["mode"] = modeName(d_.cfg.mode);
    return json(200, d);
}

bool WebApi::saveCfg(const settings::Settings& next, Response& err) {
    const char* bad = nullptr;
    if (!d_.store.save(next, &bad)) {
        err = error(400, "invalid", "مقدار نامعتبر", bad);
        return false;
    }
    d_.cfg = next;
    d_.onSettingsChanged();
    return true;
}

Response WebApi::postSettings(const Request& req) {
    JsonDocument in;
    if (deserializeJson(in, req.body) || !in.is<JsonObject>()) return error(400, "bad-json", "درخواست نامعتبر است");
    settings::Settings next = d_.cfg;
    std::string changed;
    for (const FloatField& f : kFloats) {
        if (!in[f.name].is<float>()) continue;
        next.*f.ptr = in[f.name].as<float>();
        changed += std::string(changed.empty() ? "" : " ") + f.name;
    }
    for (const UintField& f : kUints) {
        if (!in[f.name].is<uint32_t>()) continue;
        next.*f.ptr = in[f.name].as<uint32_t>();
        changed += std::string(changed.empty() ? "" : " ") + f.name;
    }
    if (in["tzOffsetMin"].is<int>()) {
        next.tzOffsetMin = static_cast<int16_t>(in["tzOffsetMin"].as<int>());
        changed += std::string(changed.empty() ? "" : " ") + "tzOffsetMin";
    }
    Response err;
    if (!saveCfg(next, err)) return err;
    d_.log.append(hal::EventType::SettingsChange, changed.c_str());
    return getSettings();
}

Response WebApi::postLocation(const Request& req) {
    JsonDocument in;
    if (deserializeJson(in, req.body)) return error(400, "bad-json", "درخواست نامعتبر است");
    settings::Settings next = d_.cfg;
    if (in["city"].is<const char*>()) {
        const settings::City* c = settings::findCity(in["city"].as<const char*>());
        if (!c) return error(400, "unknown-city", "شهر ناشناخته است", "city");
        next.latDeg = c->lat;
        next.lonDeg = c->lon;
        snprintf(next.cityId, sizeof(next.cityId), "%s", c->id);
    } else if (in["text"].is<const char*>()) {
        double lat, lon;
        if (!settings::parseLatLon(in["text"].as<const char*>(), lat, lon)) return error(400, "bad-coordinates", "مختصات نامعتبر است؛ مثل «30.283, 57.083»", "text");
        next.latDeg = static_cast<float>(lat);
        next.lonDeg = static_cast<float>(lon);
        snprintf(next.cityId, sizeof(next.cityId), "custom");
    } else {
        return error(400, "bad-json", "درخواست نامعتبر است");
    }
    Response err;
    if (!saveCfg(next, err)) return err;
    char detail[48];
    snprintf(detail, sizeof(detail), "location %s %.3f %.3f", next.cityId, static_cast<double>(next.latDeg), static_cast<double>(next.lonDeg));
    d_.log.append(hal::EventType::SettingsChange, detail);
    return getSettings();
}

Response WebApi::postClock(const Request& req) {
    JsonDocument in;
    if (deserializeJson(in, req.body) || !in["utc"].is<int64_t>()) return error(400, "bad-json", "درخواست نامعتبر است");
    const int64_t utc = in["utc"].as<int64_t>();
    if (utc < kMinUtc || utc >= kMaxUtc) return error(400, "bad-time", "زمان نامعتبر است", "utc");
    const int64_t before = d_.utcNow();
    if (!d_.clock.setUtc(utc)) return error(500, "clock-failed", "تنظیم ساعت ناموفق بود");
    char detail[64];
    snprintf(detail, sizeof(detail), "%lld -> %lld", static_cast<long long>(before), static_cast<long long>(utc));
    d_.log.append(hal::EventType::ClockSet, detail);
    return ok();
}

Response WebApi::postMode(const Request& req) {
    JsonDocument in;
    if (deserializeJson(in, req.body)) return error(400, "bad-json", "درخواست نامعتبر است");
    const char* m = in["mode"] | "";
    settings::Settings next = d_.cfg;
    if (strcmp(m, "auto") == 0) {
        d_.ctl.command(control::Command::ModeAuto, req.nowMs);
        next.mode = settings::Mode::Auto;
    } else if (strcmp(m, "manual") == 0) {
        d_.ctl.command(control::Command::ModeManual, req.nowMs);
        next.mode = settings::Mode::Manual;
    } else {
        return error(400, "bad-mode", "حالت نامعتبر است", "mode");
    }
    Response err;
    saveCfg(next, err);  // a failed save only means the mode is not remembered after a reboot
    return ok();
}

Response WebApi::postManual(const Request& req) {
    JsonDocument in;
    if (deserializeJson(in, req.body)) return error(400, "bad-json", "درخواست نامعتبر است");
    const char* dir = in["dir"] | "";
    control::Command cmd;
    if (strcmp(dir, "east") == 0) cmd = control::Command::ManualEast;
    else if (strcmp(dir, "west") == 0) cmd = control::Command::ManualWest;
    else if (strcmp(dir, "stop") == 0) cmd = control::Command::ManualStop;
    else return error(400, "bad-dir", "جهت نامعتبر است", "dir");
    switch (d_.ctl.command(cmd, req.nowMs)) {
        case control::CommandResult::Ok: return ok();
        case control::CommandResult::WindActive: return error(409, "wind", "باد شدید — فرمان دستی پذیرفته نمی‌شود");
        case control::CommandResult::FaultActive: return error(409, "fault", "ابتدا خطا را ریست کنید");
        case control::CommandResult::NeedsManualMode:
        case control::CommandResult::NotInManualMode: return error(409, "need-manual-mode", "ابتدا حالت دستی را انتخاب کنید");
    }
    return ok();
}

Response WebApi::getCalibrate() {
    static const char* const names[] = {"ok", "pending", "moving", "too-small-angle", "too-large-angle", "bad-gravity", "sensor-fault", "not-captured"};
    JsonDocument d;
    d["capturing"] = d_.angle.capturing();
    d["result"] = names[static_cast<int>(d_.angle.captureResult())];
    d["calibrated"] = d_.angle.calibrated();
    return json(200, d);
}

Response WebApi::calibrate(const Request& req, const std::string& step) {
    (void)req;
    if (step == "horizontal" || step == "east") {
        if (!d_.angle.beginCapture(step == "east" ? angle::CalStep::East : angle::CalStep::Horizontal))
            return error(409, "horizontal-first", "ابتدا مرحله افقی را انجام دهید");
        return getCalibrate();
    }
    if (step == "cancel") {
        d_.angle.cancelCapture();
        return ok();
    }
    if (step == "save") {
        if (!d_.angle.calibrated()) return error(409, "not-calibrated", "کالیبراسیون کامل نیست");
        d_.angle.setCreatedUtc(static_cast<uint32_t>(d_.utcNow()));
        if (!d_.angle.saveTo(d_.blobs)) return error(500, "save-failed", "ذخیره ناموفق بود");
        d_.log.append(hal::EventType::SettingsChange, "angle calibration saved");
        return ok();
    }
    return error(404, "not-found", "پیدا نشد");
}

Response WebApi::getLog(const Request& req) {
    size_t n = 100;
    const size_t q = req.query.find("n=");
    if (q != std::string::npos) n = static_cast<size_t>(atoi(req.query.c_str() + q + 2));
    if (n < 1) n = 1;
    if (n > 100) n = 100;
    JsonDocument d;
    JsonArray a = d["events"].to<JsonArray>();
    if (d_.readLog) {
        std::string buf(n * events::kMaxLine + 8, '\0');
        const size_t len = d_.readLog(&buf[0], buf.size(), n);
        size_t pos = 0;
        while (pos < len) {
            const size_t nl = buf.find('\n', pos);
            if (nl == std::string::npos) break;
            int64_t t;
            char type[24], detail[96];
            const std::string line = buf.substr(pos, nl - pos + 1);
            if (events::parseLine(line.c_str(), t, type, sizeof(type), detail, sizeof(detail))) {
                JsonObject o = a.add<JsonObject>();
                o["t"] = t;
                o["type"] = type;
                o["detail"] = detail;
            }
            pos = nl + 1;
        }
    }
    return json(200, d);
}

Response WebApi::postPassword(const Request& req) {
    JsonDocument in;
    if (deserializeJson(in, req.body)) return error(400, "bad-json", "درخواست نامعتبر است");
    if (!d_.auth.verify(d_.cfg, d_.defaultPassword, in["old"] | "")) return error(401, "wrong-password", "رمز فعلی اشتباه است");
    settings::Settings next = d_.cfg;
    if (!d_.auth.setPassword(next, in["new"] | "")) return error(400, "weak-password", "رمز باید دست‌کم ۸ نویسه باشد", "new");
    Response err;
    if (!saveCfg(next, err)) return err;
    d_.log.append(hal::EventType::SettingsChange, "settings password changed");
    return ok();  // every session ended: the panel logs in again with the new password
}

}  // namespace webapi
