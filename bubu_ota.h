#pragma once
#include <Adafruit_GC9A01A.h>

namespace BubuOTA {

// Optional: override default manifest
void setManifestURL(const char* url);

// Call once after display init
void begin(Adafruit_GC9A01A* tft, int pinSCK=4, int pinMOSI=3, int pinCS=2);

// Call in loop; runs portal/OTA state machine and draws animation while active
void loop();

// If you want to force the portal (e.g. a button long-press)
void startPortal();

// True while OTA/portal UI owns the screen (pause your normal render)
bool busy();

} // namespace