#include "panel_server.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/semphr.h>

#include "panel_files.h"

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
void redirectHome(AsyncWebServerRequest* r) { r->redirect("http://192.168.4.1/"); }

void onBody(AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
    if (total > 4096) return;  // the largest real request is a few hundred bytes
    if (index == 0) {
        r->_tempObject = malloc(total + 1);  // freed by the library with the request
        if (r->_tempObject) static_cast<char*>(r->_tempObject)[total] = '\0';
    }
    if (r->_tempObject) memcpy(static_cast<char*>(r->_tempObject) + index, data, len);
}

void onRequest(AsyncWebServerRequest* r) {
    const String url = r->url();
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

bool PanelServer::begin(const char* apPassword) {
    self = this;
    mutex_ = xSemaphoreCreateMutex();

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(ssid_, sizeof(ssid_), "SunTracker-%02X%02X", mac[4], mac[5]);

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(ssid_, apPassword, 6, 0, 4)) return false;
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

    server.onRequestBody(onBody);
    server.onNotFound(onRequest);
    server.begin();
    return true;
}

}  // namespace web
