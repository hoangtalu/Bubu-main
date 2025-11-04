#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

enum class OtaPhase : uint8_t {
  WIFI_CONNECTING,
  CHECKING,
  DOWNLOADING,
  VERIFYING,
  FLASHING,
  OK,
  ERROR_,
  REBOOT_
};

void OTAAnim_begin(Adafruit_GC9A01A* tft, int16_t w=240, int16_t h=240);
void OTAAnim_start(OtaPhase p);
void OTAAnim_setProgress(float p01);   // 0..1 for DOWNLOADING/FLASHING
void OTAAnim_drawFrame();              // call ~30fps when OTA/portal showing