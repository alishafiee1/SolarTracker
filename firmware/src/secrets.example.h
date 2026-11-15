// Copy this file to secrets.h (ignored by git) and set the values given to the client.
// secrets.h is optional: without it the development placeholders below are used, which are public.
#pragma once

// Key that signs firmware updates (cycle 021). With it set, the panel only installs files signed with the same key
// (the build writes firmware.signed.bin). Make your own: python -c "import secrets; print(secrets.token_hex(32))"
// Never reuse the example. Without this line unsigned files are accepted.
// #define OTA_SIGN_KEY "put-64-random-hex-characters-here"

// Hotspot (WPA2) password, at least 8 characters. Same for every device; the user can change it in the panel.
#define DEFAULT_AP_PASSWORD "suntracker-ap-dev"
// Password of the panel (manual moves, settings). Same for every device; the panel nags until it is changed.
#define DEFAULT_PANEL_PASSWORD "suntracker-dev"
