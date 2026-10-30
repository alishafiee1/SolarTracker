#include "panel_server.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/semphr.h>

#include "csv_stream.h"
#include "panel_files.h"
#include "rtc_time.h"

namespace web {

namespace {
AsyncWebServer server(80);
PanelServer* self = nullptr;

void sendJson(AsyncWebServerRequest* r, const webapi::Response& res) {
    AsyncWebServerResponse* out = r->beginResponse(res.status, "application/json", res.body.c_str());
    out->addHeader("Cache-Control", "no-store");
    if (!res.setCookie.empty()) out->addHeader("Set-Cookie", res.setCookie.c_str());
    r->send(out);
}

void sendFile(AsyncWebServerRequest* r, const PanelFile& f) {
    AsyncWebServerResponse* out = r->beginResponse_P(200, f.type, f.data, f.size);
    out->addHeader("Content-Encoding", "gzip");
    out->addHeader("Cache-Control", "no-cache");
    r->send(out);
}

// Captive portals: phones probe a few well-known URLs; a redirect to the panel makes them open it.
// A relative redirect works through the hotspot (192.168.4.1) and through the site Wi-Fi address alike.
void redirectHome(AsyncWebServerRequest* r) { r->redirect("/"); }

void onBody(AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
    if (total > 4096) return;  // the largest real request is a few hundred bytes
    if (index == 0) {
        r->_tempObject = malloc(total + 1);  // freed by the library with the request
        if (r->_tempObject) static_cast<char*>(r->_tempObject)[total] = '\0';
    }
    if (r->_tempObject) memcpy(static_cast<char*>(r->_tempObject) + index, data, len);
}

// Module 012: stream the event log as CSV. The login is checked once; the rows are produced chunk by chunk
// under the same lock the control loop uses, so a download never holds the loop for more than one chunk.
void sendCsv(AsyncWebServerRequest* r) {
    if (!self->csvLog()) {
        r->send(404, "text/plain", "not found");
        return;
    }
    webapi::Request req;
    if (r->hasHeader("Cookie")) req.cookie = r->header("Cookie").c_str();
    req.nowMs = millis();
    self->lock();
    const bool ok = self->api().isLoggedIn(req);
    self->unlock();
    if (!ok) {
        r->send(401, "application/json", "{\"error\":\"login-required\"}");
        return;
    }
    auto* stream = new drivers::CsvStream(*self->csvLog(), self->tzOffsetMin());
    const rtc::DateTime d = rtc::fromEpoch(self->utcNow() + static_cast<int64_t>(self->tzOffsetMin()) * 60);
    char name[64];
    snprintf(name, sizeof(name), "attachment; filename=\"suntracker-%s-%04d-%02d-%02d.csv\"", self->ssid() + 11, d.year > 2000 ? d.year : 1970, d.month, d.day);
    AsyncWebServerResponse* out = r->beginChunkedResponse("text/csv; charset=utf-8", [stream](uint8_t* buffer, size_t maxLen, size_t) -> size_t {
        self->lock();
        const size_t n = stream->next(reinterpret_cast<char*>(buffer), maxLen);
        self->unlock();
        return n;
    });
    out->addHeader("Content-Disposition", name);
    out->addHeader("Cache-Control", "no-store");
    r->onDisconnect([stream]() { delete stream; });
    r->send(out);
}

// Module 010: firmware upload. The login is checked on the first chunk; the chunks go straight to the updater.
bool updateAllowed = false;

void onUpdateUpload(AsyncWebServerRequest* r, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
    (void)filename;
    if (!self->ota()) return;
    if (index == 0) {
        webapi::Request req;
        if (r->hasHeader("Cookie")) req.cookie = r->header("Cookie").c_str();
        req.nowMs = millis();
        self->lock();
        updateAllowed = self->api().isLoggedIn(req);
        self->unlock();
    }
    if (!updateAllowed) return;
    self->ota()->handleChunk(data, len, index, final, millis(), self->otaLog());
}

void onUpdateDone(AsyncWebServerRequest* r) {
    if (!self->ota()) {
        r->send(404, "application/json", "{\"error\":\"no-update\"}");
    } else if (!updateAllowed) {
        r->send(401, "application/json", "{\"error\":\"login-required\",\"message\":\"ابتدا وارد شوید\"}");
    } else if (self->ota()->succeeded()) {
        r->send(200, "application/json", "{\"ok\":true,\"restarting\":true,\"message\":\"نصب شد؛ دستگاه دوباره راه‌اندازی می‌شود\"}");
    } else {
        String body = String("{\"error\":\"invalid-file\",\"message\":\"") + self->ota()->errorText() + "\"}";
        r->send(400, "application/json", body);
    }
}

void onRequest(AsyncWebServerRequest* r) {
    const String url = r->url();
    if (url == "/api/log.csv") {
        sendCsv(r);
        return;
    }
    if (url.startsWith("/api/")) {
        webapi::Request req;
        req.post = r->method() == HTTP_POST;
        req.path = url.c_str();
        if (r->hasHeader("Cookie")) req.cookie = r->header("Cookie").c_str();
        if (r->_tempObject) req.body = static_cast<const char*>(r->_tempObject);
        const int q = r->url().indexOf('?');
        if (q >= 0) req.query = r->url().substring(q + 1).c_str();
        // Query strings are not part of url(); rebuild n=... for /api/log.
        if (r->hasParam("n")) req.query = String("n=" + r->getParam("n")->value()).c_str();
        req.nowMs = millis();
        self->lock();
        const webapi::Response res = self->api().handle(req);
        self->unlock();
        self->countServed();
        sendJson(r, res);
        return;
    }
    if (r->method() == HTTP_GET) {
        for (size_t i = 0; i < kPanelFileCount; ++i) {
            if (url == kPanelFiles[i].path) {
                sendFile(r, kPanelFiles[i]);
                return;
            }
        }
        redirectHome(r);
        return;
    }
    r->send(404, "text/plain", "not found");
}
}  // namespace

void PanelServer::lock() {
    if (mutex_) xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
}
void PanelServer::unlock() {
    if (mutex_) xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

uint8_t PanelServer::clients() const { return WiFi.softAPgetStationNum(); }

void PanelServer::setLogCsv(drivers::LittleFsEventLog* log, std::function<int()> tz, std::function<int64_t()> utc) {
    csvLog_ = log;
    tz_ = tz;
    utc_ = utc;
}

bool PanelServer::begin(const char* apPassword) {
    self = this;
    mutex_ = xSemaphoreCreateMutex();

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(ssid_, sizeof(ssid_), "SunTracker-%02X%02X", mac[4], mac[5]);

    WiFi.mode(WIFI_AP_STA);  // the hotspot always stays up; the station part is used by the site Wi-Fi module
    if (!WiFi.softAP(ssid_, apPassword, 6, 0, 4)) return false;
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

    server.on("/api/update", HTTP_POST, onUpdateDone, onUpdateUpload);
    server.onRequestBody(onBody);
    server.onNotFound(onRequest);
    server.begin();
    return true;
}

}  // namespace web
