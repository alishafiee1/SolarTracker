#include "sct013_current.h"

#include <Arduino.h>
#include <math.h>

namespace drivers {

void Sct013Current::task(void* arg) {
    Sct013Current* self = static_cast<Sct013Current*>(arg);
    for (;;) {
        const uint32_t start = millis();
        double sum = 0, sumSq = 0;
        uint32_t n = 0;
        while (millis() - start < 200) {
            const double v = analogReadMilliVolts(kPin) / 1000.0;
            sum += v;
            sumSq += v * v;
            ++n;
        }
        if (n > 100) {
            const double mean = sum / n;  // the bias
            const double var = sumSq / n - mean * mean;
            const float rmsV = var > 0 ? static_cast<float>(sqrt(var)) : 0.0f;
            self->volts_ = rmsV;
            self->rms_ = rmsV * self->factor_;
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void Sct013Current::begin(float ampsPerVolt) {
    factor_ = ampsPerVolt;
    if (started_) return;
    started_ = true;
    analogSetPinAttenuation(kPin, ADC_11db);
    xTaskCreatePinnedToCore(task, "sct013", 3072, this, 1, nullptr, 0);
}

}  // namespace drivers
