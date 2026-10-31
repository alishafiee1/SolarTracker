#pragma once
// Captive-portal redirect decision (cycle 014). Phones probe a few well-known addresses to find out whether a Wi-Fi
// network has a sign-in page; answering them with a redirect makes the phone open the panel by itself.
#include <stdint.h>

#include <string>

namespace web {

enum class Redirect : uint8_t {
    None,      // answer the request itself
    Absolute,  // 302 to http://192.168.4.1/ (a phone on the hotspot; probes arrive with foreign Host headers)
    Relative,  // 302 to / (a computer on the site Wi-Fi, which must stay on the address it typed)
};

constexpr uint8_t kHotspotIp[4] = {192, 168, 4, 1};

inline bool isHotspotAddress(const uint8_t ip[4]) {
    return ip[0] == kHotspotIp[0] && ip[1] == kHotspotIp[1] && ip[2] == kHotspotIp[2] && ip[3] == kHotspotIp[3];
}

// `localIp` is the address of this device that the request arrived on.
inline Redirect redirectFor(const uint8_t localIp[4], const std::string& path) {
    if (path == "/" || path.rfind("/api/", 0) == 0) return Redirect::None;
    return isHotspotAddress(localIp) ? Redirect::Absolute : Redirect::Relative;
}

// The well-known probe paths (for documentation and tests; every unknown path is redirected anyway).
inline bool isKnownProbe(const std::string& path) {
    static const char* const probes[] = {"/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html", "/connecttest.txt",
                                         "/ncsi.txt", "/redirect", "/canonical.html", "/success.txt"};
    for (const char* p : probes)
        if (path == p) return true;
    return false;
}

}  // namespace web
