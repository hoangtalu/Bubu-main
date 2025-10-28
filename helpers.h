#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// Use the same canvas/tft as engine (defined in emotion_engine.cpp)
extern Adafruit_GC9A01A tft;
extern GFXcanvas16 canvas;

// === Prototypes for helper drawing & small utilities moved out of the engine ===
void drawShape(int cx, int cy, int w, int h, int corner, uint8_t fill);
// from NORMAL
void drawBlinkingEyes(float cx, float cy, float progress);

// tiny utility used by many states
void drawBlinkLine(int x, int y, int w, int h, uint16_t c);

// from SAD
void drawCircleEyes(int lx, int rx, int y, int r);

// from CONFUSE
void drawCircleWithOpacity(int x, int y, int r, uint16_t color, uint8_t alpha);
void drawEyeCircle(int x, int y, uint16_t c);
void drawEyeFlat(int x, int y, uint16_t c);

// from DRUNK
void drawDrunkWhirlpool(int cx, int cy, int r, bool cw, float phase);
uint16_t HE_colorLerp(uint16_t c1, uint16_t c2, float t);

// from ANGRY
void triggerAngry(uint32_t now);
bool handleAngry(uint32_t now);

// from FURIOUS
// -------- Colors (RGB565) --------
constexpr uint16_t COL_BG   = 0x0000; // black
constexpr uint16_t COL_FG   = 0xFFFF; // white

// -------- Geometry helpers (use your global engine geometry) --------
void HE_drawEyeBox(int cx, int cy, int w, int h, int r = -1, uint16_t color = COL_FG);
void HE_drawBrowsAngled(int leftCx, int rightCx, int y, int extent, int baseEyeW, uint16_t color = COL_FG);
void HE_drawIdleEyes(int centerX, int centerY, int eyeDistance, int eyeW, int eyeH, int eyeR, uint16_t color = COL_FG);

// -------- Canvas helpers --------
inline void HE_clearCanvas(uint16_t color = COL_BG) { canvas.fillScreen(color); }
inline void HE_drawLine(int x0, int y0, int x1, int y1, uint16_t color = COL_FG) { canvas.drawLine(x0, y0, x1, y1, color); }
inline int  HE_centerX() { return canvas.width() / 2; }
inline int  HE_centerY() { return canvas.height() / 2; }

// -------- Math / easing --------
inline float HE_clamp(float v, float lo, float hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }
inline float HE_lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float HE_easeInOutCubic(float t) {
  t = HE_clamp(t, 0.0f, 1.0f);
  return (t < 0.5f) ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

// Phase helpers: return 0..1 within a time slice (or -1 if outside)
inline float HE_phase01(unsigned long now, unsigned long start, unsigned long dur) {
  if (now < start) return -1.0f;
  if (now >= start + dur) return -1.0f;
  return float(now - start) / float(dur);
}

// Jitter (for shake effects)
inline int HE_jitter(unsigned long seedMs, unsigned stepMs, int magnitude) {
  return ((seedMs / stepMs) % 2 == 0) ? +magnitude : -magnitude;
}
// These are implemented in helpers.cpp
uint16_t HE_colorLerp(uint16_t c1, uint16_t c2, float t);

// Simple easing utilities (usable by any emotion)
float EE_easeInOut(float t);
float EE_jitteredEase(float progress, float amp = 0.02f);