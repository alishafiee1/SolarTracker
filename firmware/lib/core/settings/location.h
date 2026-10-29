#pragma once
// Site location helpers (cycle 007, R9).
#include <stddef.h>

namespace settings {

struct City {
    const char* id;      // ASCII key stored in Settings
    const char* nameFa;  // shown in the web panel
    float lat;
    float lon;
};

extern const City kCities[];
extern const size_t kCityCount;  // 31 province capitals

const City* findCity(const char* id);

// Parses "30.283, 57.083" as copied from a map (comma or space separated, spaces around allowed).
// Returns false when the text is malformed or the values are out of range.
bool parseLatLon(const char* text, double& lat, double& lon);

}  // namespace settings
