// Copy this file to secrets.h (ignored by git) and set the values given to the client.
// secrets.h is optional: without it the development placeholders below are used, which are public.
#pragma once

// Hotspot (WPA2) password, at least 8 characters. Same for every device; the user can change it in the panel.
#define DEFAULT_AP_PASSWORD "suntracker-ap-dev"
// Password of the panel (manual moves, settings). Same for every device; the panel nags until it is changed.
#define DEFAULT_PANEL_PASSWORD "suntracker-dev"
