#include "helpers.h"

void drawShape(int cx, int cy, int w, int h, int corner, uint8_t fill) {
  uint16_t col = tft.color565(fill, fill, fill);
  canvas.fillRoundRect(cx - w/2, cy - h/2, w, h, corner, col);
}
// access globals defined in emotion_engine.cpp
extern int centerX, centerY;
extern int eyeRadius;
extern int eyeDistance;
extern int eyeWidth, eyeHeight, eyeCorner;
// The implementations below are copied verbatim from your master,
// only moved here so emotion_engine.cpp stays focused on state/flow.

// === NORMAL helper ===
void drawBlinkingEyes(float cx, float cy, float progress) {
  int leftX  = (int)(cx - eyeDistance);
  int rightX = (int)(cx + eyeDistance);

  float currentH = eyeHeight * (1.0f - progress);  // 70 → 0 while blinking

  if (currentH > 0.0f) {
    canvas.fillRoundRect(leftX  - eyeWidth/2,  cy - currentH/2, eyeWidth, currentH, eyeCorner, GC9A01A_WHITE);
    canvas.fillRoundRect(rightX - eyeWidth/2,  cy - currentH/2, eyeWidth, currentH, eyeCorner, GC9A01A_WHITE);
  } else {
    drawBlinkLine(leftX,  cy, eyeWidth/2, 4, GC9A01A_WHITE);
    drawBlinkLine(rightX, cy, eyeWidth/2, 4, GC9A01A_WHITE);
  }
}
void drawBlinkLine(int x, int y, int w, int h, uint16_t c) {
  canvas.fillRoundRect(x - w, y - h / 2, w * 2, h, h / 2, c);
}

// === SAD helper ===
void drawCircleEyes(int lx, int rx, int y, int r) {
  canvas.fillCircle(lx, y, r, GC9A01A_WHITE);
  canvas.fillCircle(rx, y, r, GC9A01A_WHITE);
}

// === CONFUSE helpers ===
void drawCircleWithOpacity(int x, int y, int r, uint16_t color, uint8_t alpha) {
  for (int dy = -r; dy <= r; dy++) {
    for (int dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy <= r * r) {
        int px = x + dx, py = y + dy;
        if (px >= 0 && px < 240 && py >= 0 && py < 240) {
          uint16_t bg = canvas.getPixel(px, py);
          uint8_t r1 = (color >> 11) << 3, g1 = ((color >> 5) & 0x3F) << 2, b1 = (color & 0x1F) << 3;
          uint8_t r0 = (bg >> 11) << 3, g0 = ((bg >> 5) & 0x3F) << 2, b0 = (bg & 0x1F) << 3;
          uint8_t rM = (r0 * (255 - alpha) + r1 * alpha) / 255;
          uint8_t gM = (g0 * (255 - alpha) + g1 * alpha) / 255;
          uint8_t bM = (b0 * (255 - alpha) + b1 * alpha) / 255;
          canvas.drawPixel(px, py, tft.color565(rM, gM, bM));
        }
      }
    }
  }
}

void drawEyeCircle(int x, int y, uint16_t c) {
  canvas.fillCircle(x, y, 35, c);
}
void drawEyeFlat(int x, int y, uint16_t c) {
  canvas.fillRoundRect(x - 35, y - 21, 70, 42, 12, c);
}


// === DRUNK helper ===
void drawDrunkWhirlpool(int cx, int cy, int r, bool cw, float phase) {
  int n = 70;
  float maxA = TWO_PI * 2.5;
  float stepA = maxA / n;
  float stepR = float(r) / n;
  for (int i = 0; i < n; i++) {
    float t = stepA * i * (cw ? 1 : -1) + phase;
    float rad = i * stepR;
    int x = cx + cos(t) * rad;
    int y = cy + sin(t) * rad;
    uint8_t s = map(i, 0, n, 255, 80);
    uint16_t color = tft.color565(s, 0, s);
    canvas.fillCircle(x, y, 2, color);
  }
}
// ===FURIOUS helper ===
// Rounded eye box (defaults to your global eyeRadius when r < 0)
extern int eyeRadius;  // provided by emotion_engine.cpp

void HE_drawEyeBox(int cx, int cy, int w, int h, int r, uint16_t color) {
  int rr = (r < 0) ? eyeRadius : r;
  canvas.fillRoundRect(cx - w/2, cy - h/2, w, h, rr, color);
}

// Symmetric angled brows, slanted toward center.
void HE_drawBrowsAngled(int leftCx, int rightCx, int y, int extent, int baseEyeW, uint16_t color) {
  const int l_out_x = leftCx  - (baseEyeW / 2);
  const int l_in_x  = leftCx  + (baseEyeW / 2);
  const int r_out_x = rightCx + (baseEyeW / 2);
  const int r_in_x  = rightCx - (baseEyeW / 2);
  for (int t = 0; t < 4; ++t) {
    canvas.drawLine(l_out_x, y - extent + t, l_in_x, y + extent + t, color);
    canvas.drawLine(r_out_x, y - extent + t, r_in_x, y + extent + t, color);
  }
}
uint16_t HE_colorLerp(uint16_t c1, uint16_t c2, float t) {
  t = HE_clamp(t, 0.0f, 1.0f);
  // extract 5-6-5 components
  int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  int r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  int r = r1 + (int)((r2 - r1) * t);
  int g = g1 + (int)((g2 - g1) * t);
  int b = b1 + (int)((b2 - b1) * t);
  return (r << 11) | (g << 5) | b;
}
// DOUBT //
void HE_drawIdleEyes(int centerX, int centerY, int eyeDistance, int eyeW, int eyeH, int eyeR, uint16_t color) {
  const int leftCx  = centerX - eyeDistance;
  const int rightCx = centerX + eyeDistance;
  canvas.fillRoundRect(leftCx  - eyeW/2, centerY - eyeH/2, eyeW, eyeH, eyeR, color);
  canvas.fillRoundRect(rightCx - eyeW/2, centerY - eyeH/2, eyeW, eyeH, eyeR, color);
}
// Basic ease-in-out parabola (0..1 -> 0..1)
float EE_easeInOut(float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  if (t < 0.5f) return 2.0f * t * t;               // accelerate
  return -1.0f + (4.0f - 2.0f * t) * t;            // decelerate
}

// Eased + small noise (amp = ± amount, e.g. 0.02 = ±2%)
float EE_jitteredEase(float progress, float amp) {
  float eased = EE_easeInOut(progress);
  eased += (random(-1000, 1001) / 1000.0f) * amp;  // -1..1 scaled by amp
  if (eased < 0) eased = 0;
  if (eased > 1) eased = 1;
  return eased;
}