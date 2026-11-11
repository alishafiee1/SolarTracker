#include "site_net.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/semphr.h>
#include <time.h>

namespace drivers {

void SiteNet::apply(const net::NetConfig& cfg) {
    if (!mutex_) mutex_ = xSemaphoreCreateMutex();
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    cfg_ = cfg;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    wanted_ = cfg.ssid[0] != '\0';
    backoff_.reset();
    nextTryMs_ = 0;
    if (wanted_) {
        WiFi.disconnect(false);  // drop the old station connection, keep the hotspot
        connected_ = false;
    }
    if ((cfg.notifyEnabled || wanted_) && !taskStarted_) {
        taskStarted_ = true;
        xTaskCreatePinnedToCore(task, "sitenet", 8192, this, 1, nullptr, 0);
    }
}

void SiteNet::service(uint32_t nowMs, hal::IClock& rtc, bool rtcValid, hal::IEventLog* log, bool setClockFromNtp) {
    if (!wanted_) return;
    if (WiFi.status() == WL_CONNECTED) {
        if (!connected_) {
            connected_ = true;
            backoff_.reset();
            snprintf(ip_, sizeof(ip_), "%s", WiFi.localIP().toString().c_str());
            if (log) log->append(hal::EventType::StateChange, "site wifi connected");
        }
        // NTP at the first connection and then once a day (R2).
        if (!ntpStarted_ || nowMs - lastNtpMs_ > 24u * 3600u * 1000u) {
            ntpStarted_ = true;
            lastNtpMs_ = nowMs;
            configTime(0, 0, "pool.ntp.org", "time.google.com");
        }
        const time_t t = time(nullptr);
        if (setClockFromNtp && ntpStarted_ && t > 1704067200 && net::shouldSetClockFromNtp(rtc.utc(), rtcValid, static_cast<int64_t>(t))) {
            if (rtc.setUtc(static_cast<int64_t>(t)) && log) log->append(hal::EventType::ClockSet, "ntp");
        }
        return;
    }
    if (connected_) {
        connected_ = false;
        ip_[0] = '\0';
        if (log) log->append(hal::EventType::StateChange, "site wifi lost");
    }
    if (nowMs >= nextTryMs_) {  // R1: reconnect with backoff; the hotspot is not affected
        nextTryMs_ = nowMs + backoff_.nextDelayMs();
        WiFi.begin(cfg_.ssid, cfg_.password[0] ? cfg_.password : nullptr);
    }
}

bool SiteNet::notify(const std::string& key, const std::string& text, uint32_t nowMs) {
    if (!mutex_) return false;
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    const bool enabled = cfg_.notifyEnabled && cfg_.botToken[0] && cfg_.chatId[0];
    const bool queued = enabled && queue_.enqueue(key.c_str(), text.c_str(), nowMs);
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    return enabled && queued;
}

void SiteNet::forget(const std::string& key) {
    if (!mutex_) return;
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    queue_.forget(key.c_str());
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

bool SiteNet::post(const std::string& text) {
    net::NetConfig c;
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    c = cfg_;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));

    WiFiClientSecure client;
    client.setInsecure();  // the server certificate is not checked (no CA bundle in the image); the token is still encrypted in transit
    HTTPClient http;
    http.setTimeout(10000);
    const String url = String("https://tapi.bale.ai/bot") + c.botToken + "/sendMessage";
    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "application/json");
    JsonDocument d;
    d["chat_id"] = c.chatId;
    d["text"] = text;
    std::string body;
    serializeJson(d, body);
    const int code = http.POST(body.c_str());
    const String reply = code > 0 ? http.getString() : String();
    http.end();
    return code == 200 && reply.indexOf("\"ok\":true") >= 0;
}

void SiteNet::sendLoop() {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!connected_) continue;
        std::string text;
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        const bool have = cfg_.notifyEnabled && queue_.peek(millis(), text);
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        if (!have) continue;
        const bool sent = post(text);  // up to 10 s, outside every lock
        lastNotify_ = sent ? 1 : 0;
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        queue_.done(sent, millis());
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    }
}

void SiteNet::task(void* self) { static_cast<SiteNet*>(self)->sendLoop(); }

}  // namespace drivers
