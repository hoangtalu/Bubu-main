#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// Motion Trigger Emotions (MTE) — draw using the shared canvas from emotion_engine
namespace BubuEmotions {
  // Kept for compatibility; now a no-op (we reuse the NE canvas)
  void begin(uint16_t w = 240, uint16_t h = 240);

  // Neutral/idle frame (two centered eyes)
  void showIdle(Adafruit_GC9A01A& tft);

  // MTE animations (blocking ~5 s each), return to idle when done
  void playTurnLeft(Adafruit_GC9A01A& tft);
  void playTurnRight(Adafruit_GC9A01A& tft);
  void playSpeedUp(Adafruit_GC9A01A& tft);
  void playBrakes(Adafruit_GC9A01A& tft);
}