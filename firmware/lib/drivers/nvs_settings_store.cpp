#include "nvs_settings_store.h"

#include <Preferences.h>

namespace drivers {

namespace {
constexpr const char* kNamespace = "suntracker";
}

bool NvsSettingsStore::load(const char* key, void* buf, size_t len) {
    Preferences p;
    if (!p.begin(kNamespace, true)) return false;
    const bool ok = p.getBytesLength(key) == len && p.getBytes(key, buf, len) == len;
    p.end();
    return ok;
}

bool NvsSettingsStore::save(const char* key, const void* buf, size_t len) {
    Preferences p;
    if (!p.begin(kNamespace, false)) return false;
    const bool ok = p.putBytes(key, buf, len) == len;
    p.end();
    return ok;
}

size_t NvsSettingsStore::size(const char* key) {
    Preferences p;
    if (!p.begin(kNamespace, true)) return 0;
    const size_t n = p.getBytesLength(key);
    p.end();
    return n;
}

}  // namespace drivers
