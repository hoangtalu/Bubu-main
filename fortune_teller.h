// fortune_teller.h
// Bubu Emotion: Fortune Teller (VN glyphs + auto-fit scaler)
// v1.0 (2025-09-04) — Owner: Heart & Soul (Firmware)

#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// Public API (same shape as other emotions)
namespace FortuneTeller {
  // Call once at boot (after tft.begin())
  void setup(Adafruit_GC9A01A* tftRef);

  // Called by emotion engine when this emotion starts
  void begin(uint32_t durationMs = 7999);

  // Called each frame from bubuEngineLoop() until it returns false
  // Returns: stillActive?
  bool loop();

  // Optional: force stop (engine will call this on override/teardown)
  void end();

  // Optional helper: trigger a fresh random fortune next time begin() runs
  void reseed();
}