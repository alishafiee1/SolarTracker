#include "location.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace settings {

// Province capitals, coordinates rounded to 3 decimals (about 100 m; far finer than tracking needs).
const City kCities[] = {
    {"tehran", "تهران", 35.689f, 51.389f},
    {"mashhad", "مشهد", 36.297f, 59.606f},
    {"isfahan", "اصفهان", 32.654f, 51.668f},
    {"tabriz", "تبریز", 38.080f, 46.292f},
    {"shiraz", "شیراز", 29.591f, 52.584f},
    {"ahvaz", "اهواز", 31.319f, 48.669f},
    {"qom", "قم", 34.639f, 50.876f},
    {"karaj", "کرج", 35.840f, 50.939f},
    {"rasht", "رشت", 37.281f, 49.583f},
    {"yazd", "یزد", 31.897f, 54.357f},
    {"kerman", "کرمان", 30.283f, 57.083f},
    {"zahedan", "زاهدان", 29.496f, 60.863f},
    {"bandar-abbas", "بندرعباس", 27.183f, 56.267f},
    {"urmia", "ارومیه", 37.555f, 45.073f},
    {"kermanshah", "کرمانشاه", 34.314f, 47.065f},
    {"hamadan", "همدان", 34.799f, 48.515f},
    {"arak", "اراک", 34.092f, 49.689f},
    {"sanandaj", "سنندج", 35.314f, 46.999f},
    {"ardabil", "اردبیل", 38.249f, 48.296f},
    {"zanjan", "زنجان", 36.677f, 48.480f},
    {"qazvin", "قزوین", 36.269f, 50.004f},
    {"semnan", "سمنان", 35.576f, 53.392f},
    {"birjand", "بیرجند", 32.866f, 59.221f},
    {"bojnord", "بجنورد", 37.475f, 57.329f},
    {"gorgan", "گرگان", 36.839f, 54.434f},
    {"sari", "ساری", 36.563f, 53.060f},
    {"bushehr", "بوشهر", 28.923f, 50.838f},
    {"yasuj", "یاسوج", 30.668f, 51.588f},
    {"shahr-e-kord", "شهرکرد", 32.327f, 50.864f},
    {"ilam", "ایلام", 33.638f, 46.422f},
    {"khorramabad", "خرم‌آباد", 33.487f, 48.355f},
};
const size_t kCityCount = sizeof(kCities) / sizeof(kCities[0]);

const City* findCity(const char* id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < kCityCount; ++i)
        if (strcmp(kCities[i].id, id) == 0) return &kCities[i];
    return nullptr;
}

bool parseLatLon(const char* text, double& lat, double& lon) {
    if (!text) return false;
    char* end = nullptr;
    const double a = strtod(text, &end);
    if (end == text) return false;
    while (*end == ' ' || *end == '\t') ++end;
    if (*end == ',') ++end;
    while (*end == ' ' || *end == '\t') ++end;
    const char* second = end;
    const double b = strtod(second, &end);
    if (end == second) return false;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
    if (*end != '\0') return false;
    if (!isfinite(a) || !isfinite(b) || a < -90.0 || a > 90.0 || b < -180.0 || b > 180.0) return false;
    lat = a;
    lon = b;
    return true;
}

}  // namespace settings
