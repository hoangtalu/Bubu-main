#include "bubu_emotions.h"
#include "emotion_engine.h"   // access the shared canvas/tft from NE

// ===== Reuse the SAME canvas that emotion_engine uses =====
// Make sure emotion_engine.h exposes:  extern GFXcanvas16 canvas;
// and that emotion_engine.cpp defines it.
extern GFXcanvas16 canvas;

namespace {

// Geometry
constexpr int CANVAS_W = 240;
constexpr int CANVAS_H = 240;
constexpr int CX = 120;
constexpr int CY = 120;
constexpr int EYE_R = 20;
constexpr int EYE_SIZE = 70;
constexpr int EYE_SPACING = 100;

inline void flush() {
  canvas.fillScreen(0x0000);
}

inline void drawEye(int x, int y, int w, int h) {
  canvas.fillRoundRect(x - w/2, y - h/2, w, h, EYE_R, 0xFFFF);
}

inline void flush(Adafruit_GC9A01A& tft) {
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), CANVAS_W, CANVAS_H);
}

inline void frameDelay(uint32_t ms = 16) { delay(ms); }

} // namespace

// ================= Public API =================

void BubuEmotions::begin(uint16_t, uint16_t) {
  // no-op (we reuse the shared NE canvas)
}

void BubuEmotions::showIdle(Adafruit_GC9A01A& tft) {
  flush();
  const int lx = CX - EYE_SPACING/2;
  const int rx = CX + EYE_SPACING/2;
  drawEye(lx, CY, EYE_SIZE, EYE_SIZE);
  drawEye(rx, CY, EYE_SIZE, EYE_SIZE);
  flush(tft);
}
//=======================================TURN LEFT=======================================//
void BubuEmotions::playTurnLeft(Adafruit_GC9A01A& tft) {
  int leftX  = CX - 50;
  int rightX = CX + 50;
  const int eyeW = EYE_SIZE, eyeH = EYE_SIZE;

  int phase = 0;
  int rightMergeSpeed = 5;
  unsigned long holdStart = 0;

  for (;;) {
    flush();
    switch (phase) {
      case 0: // travel left
        if (leftX > eyeW/2) { leftX -= 7; rightX -= 5; }
        else { phase = 1; }
        break;

      case 1: // merge (right eye catches up)
        if (rightX > leftX + 2) {
          rightMergeSpeed = min(20, int(rightMergeSpeed * 1.5f));
          rightX -= rightMergeSpeed;
        } else { phase = 2; holdStart = millis(); }
        break;

      case 2: // brief hold
        if (millis() - holdStart >= 300) phase = 3;
        break;

      case 3: // return to center
        if (rightX < CX + 50) rightX += 5;
        if (leftX  > CX - 50) leftX  -= 5;
        if (rightX >= CX + 50 && leftX <= CX - 50) { phase = 4; holdStart = millis(); }
        break;

      case 4: // settle hold then FINISH (NO wrap to phase 0)
        if (millis() - holdStart >= 300) { showIdle(tft); return; }
        break;
    }
    drawEye(leftX, CY, eyeW, eyeH);
    drawEye(rightX, CY, eyeW, eyeH);
    flush(tft);
    frameDelay();
  }
}

//=======================================TURN RIGHT=======================================//
void BubuEmotions::playTurnRight(Adafruit_GC9A01A& tft) {
  int leftX  = CX - 50;
  int rightX = CX + 50;
  const int eyeW = EYE_SIZE, eyeH = EYE_SIZE;

  int phase = 0;
  int leftMergeSpeed = 5;
  unsigned long holdStart = 0;

  for (;;) {
    flush();
    switch (phase) {
      case 0: // travel right
        if (rightX < CANVAS_W - eyeW/2) { rightX += 7; leftX += 5; }
        else { phase = 1; }
        break;

      case 1: // merge (left eye catches up)
        if (leftX < rightX - 2) {
          leftMergeSpeed = min(20, int(leftMergeSpeed * 1.5f));
          leftX += leftMergeSpeed;
        } else { phase = 2; holdStart = millis(); }
        break;

      case 2: // brief hold
        if (millis() - holdStart >= 300) phase = 3;
        break;

      case 3: // return to center
        if (leftX  > CX - 50) leftX  -= 5;
        if (rightX < CX + 50) rightX += 5;
        if (rightX >= CX + 50 && leftX <= CX - 50) { phase = 4; holdStart = millis(); }
        break;

      case 4: // settle hold then FINISH
        if (millis() - holdStart >= 300) { showIdle(tft); return; }
        break;
    }
    drawEye(leftX, CY, eyeW, eyeH);
    drawEye(rightX, CY, eyeW, eyeH);
    flush(tft);
    frameDelay();
  }
}
// ===== SPEED UP =====
void BubuEmotions::playSpeedUp(Adafruit_GC9A01A& tft) {
  const int spacing = EYE_SPACING;
  int size = EYE_SIZE;
  float v = 0;
  int phase = 0;
  unsigned long holdStart = 0;

  for (;;) {
    flush();
    switch (phase) {
      case 0: // expand fast
        v = min(15.0f, v + 7.0f);
        size += int(v);
        if (size >= 90) { size = 90; v = 0; phase = 1; holdStart = millis(); }
        break;

      case 1: // short hold
        if (millis() - holdStart >= 500) phase = 2;
        break;

      case 2: // ease back
        v = min(3.5f, v + 0.4f);
        size -= int(v);
        if (size <= EYE_SIZE) { size = EYE_SIZE; v = 0; phase = 3; holdStart = millis(); }
        break;

      case 3: // rest then FINISH
        if (millis() - holdStart >= 300) { showIdle(tft); return; }
        break;
    }
    int lx = CX - spacing/2;
    int rx = CX + spacing/2;
    drawEye(lx, CY, size, size);
    drawEye(rx, CY, size, size);
    flush(tft);
    frameDelay();
  }
}

// ===== BRAKES =====
void BubuEmotions::playBrakes(Adafruit_GC9A01A& tft) {
  float size = EYE_SIZE;
  float spacing = 90.0f;
  float sizeV = 0.0f, spacingV = 0.0f;
  int phase = 0;
  unsigned long holdStart = 0;

  for (;;) {
    flush();
    switch (phase) {
      case 0: // shrink & close
        sizeV    = min(2.5f, sizeV + 0.25f);
        spacingV = min(2.5f, spacingV + 0.25f);
        size    = max(50.0f, size - sizeV);
        spacing = max(60.0f, spacing - spacingV);
        if (size <= 50.0f && spacing <= 60.0f) { phase = 1; holdStart = millis(); }
        break;

      case 1: // hold small
        if (millis() - holdStart >= 600) phase = 2;
        break;

      case 2: // reopen
        sizeV    = min(2.5f, sizeV + 0.25f);
        spacingV = min(2.5f, spacingV + 0.25f);
        size    = min<float>(EYE_SIZE, size + sizeV);
        spacing = min<float>(90.0f, spacing + spacingV);
        if (size >= EYE_SIZE && spacing >= 90.0f) { phase = 3; holdStart = millis(); }
        break;

      case 3: // rest then FINISH
        if (millis() - holdStart >= 300) { showIdle(tft); return; }
        break;
    }
    int lx = CX - int(spacing/2);
    int rx = CX + int(spacing/2);
    drawEye(lx, CY, int(size), int(size));
    drawEye(rx, CY, int(size), int(size));
    flush(tft);
    frameDelay();
  }
}