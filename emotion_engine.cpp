#include "emotion_engine.h"
#include "helpers.h"
#include "fortune_teller.h"
#include <U8g2_for_Adafruit_GFX.h>
#include <math.h>

// ===== Hardware objects (same as master) =====
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(240, 240);
U8G2_FOR_ADAFRUIT_GFX u8g2;

// === Global Settings ===
int centerX = 120, centerY = 120;
int eyeRadius = 35;
int eyeDistance = 50;
// === Eye Shape Settings (for NORMAL blink/squircle eyes) ===
int eyeWidth  = 70;
int eyeHeight = 70;
int eyeCorner = 20;
// === Emotion Cycle ===
EmotionType currentEmotion = NORMAL;
unsigned long emotionStartTime = 0;
unsigned long emotionDuration[] = {
  8000, // Normal
  8000, // Sad
  8000, // Confuse
  8000, // Love
  8000, // Cyclop
  7000, // Shock
  8000, // Drunk
  6000, // Furious
  6000, // Angry
  8000, // Doubt
  8000, // Angry2
  8000,  // Smile
  8000,  // Banh_chung
  8000, // DeadPool
  8000,  //Fortune_teller
  65000, //CARVE_SESSION
  65000,   //SLEEPY
  8000,    //CRY
  32000      //FIREWORKS
 };
// Weighted selection for non-NORMAL emotions.
// Units are arbitrary (they don't have to sum to 100).
// Index mapping: {NORMAL, SAD, CONFUSE, LOVE, CYCLOP, SHOCK, DRUNK, FURIOUS, ANGRY, DOUBT, ANGRY2, SMILE.......}
static uint8_t emotionWeight[19] = { 
  0,   // NORMAL (unused by picker)
  15,  // SAD
  15,  // CONFUSE
  40,  // LOVE
  15,  // CYCLOP
  15,  // SHOCK
  20,  // DRUNK
  1,  // FURIOUS
  15,  // ANGRY
  15,  // DOUBT
  10,   // ANGRY2
  30,   // SMILE
  1,   // BANH_CHUNG
  15,  //DEADPOOL
  10,   //FORTUNE_TELLER
  40,   //carved_session
  15,   //SLEEPY
  10,    //CRY
  15    //FIREWORKS
};

// ===== Idle Background Tint (random, drifting hue) ==================
// Turn on/off globally:
static const bool IDLE_BG_TINT_ENABLED = true;
// ===== Idle Background Tint (random drifting hue) =====
static const uint8_t IDLE_BG_MIN_F     = 32;    // darker
static const uint8_t IDLE_BG_MAX_F     = 96;    // brighter
static const uint32_t IDLE_HUE_HOLD_MS = 2500;  // sit on a hue
static const uint32_t IDLE_HUE_FADE_MS = 1800;  // crossfade time
static const float    IDLE_BREATH_HZ   = 0.10f; // brightness breathing
bool gPreserveBackground = false;

// If you already have bi_dim565/bi_hue2rgb565 from intro, you can use those.
static inline uint16_t idle_dim565(uint16_t c,uint8_t f){
  uint8_t r5=(c>>11)&31, g6=(c>>5)&63, b5=c&31;
  uint8_t r=(r5<<3)|(r5>>2), g=(g6<<2)|(g6>>4), b=(b5<<3)|(b5>>2);
  r=(uint8_t)(((uint16_t)r*f)>>8);
  g=(uint8_t)(((uint16_t)g*f)>>8);
  b=(uint8_t)(((uint16_t)b*f)>>8);
  return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
}
static inline uint16_t idle_hue2rgb565(uint8_t hue){
  uint8_t seg=hue>>5, off=(hue&31)<<3, r=0,g=0,b=0;
  switch(seg){
    case 0: r=255;       g=off;       b=0;   break;
    case 1: r=255-off;   g=255;       b=0;   break;
    case 2: r=0;         g=255;       b=off; break;
    case 3: r=0;         g=255-off;   b=255; break;
    case 4: r=off;       g=0;         b=255; break;
    default:r=255;       g=0;         b=255-off;     break;
  }
  return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
}

static uint8_t  idleHueFrom = 0, idleHueTo = 64;
static unsigned long idlePhaseStart = 0;
static bool idleTintInited = false;

static uint8_t idle_hueLerp(uint8_t a, uint8_t b, float t){
  int da = b - a;
  if (da > 96)  da -= 192;
  if (da < -96) da += 192;
  int h = (int)roundf(a + da * t);
  if (h < 0)    h += 192;
  if (h >= 192) h -= 192;
  return (uint8_t)h;
}

static void drawIdleTintBackground(unsigned long now){
  if(!idleTintInited){
    idleHueFrom = (uint8_t)(esp_random() % 192);
    idleHueTo   = (uint8_t)(esp_random() % 192);
    idlePhaseStart = now;
    idleTintInited = true;
  }

  unsigned long phaseElapsed = now - idlePhaseStart;
  uint8_t hue;

  if (phaseElapsed < IDLE_HUE_HOLD_MS){
    hue = idleHueFrom;
  } else if (phaseElapsed < IDLE_HUE_HOLD_MS + IDLE_HUE_FADE_MS){
    float t = (phaseElapsed - IDLE_HUE_HOLD_MS) / (float)IDLE_HUE_FADE_MS;
    t = t*t*(3.f - 2.f*t); // ease
    hue = idle_hueLerp(idleHueFrom, idleHueTo, t);
  } else {
    idleHueFrom = idleHueTo;
    uint8_t step = 24 + (esp_random() % 64); // 24..87
    idleHueTo = (uint8_t)((idleHueFrom + step) % 192);
    idlePhaseStart = now;
    hue = idleHueFrom;
  }

  float breath = 0.5f + 0.5f * sinf(2.0f * 3.1415926f * IDLE_BREATH_HZ * (now / 1000.0f));
  uint8_t f = (uint8_t)(IDLE_BG_MIN_F + (IDLE_BG_MAX_F - IDLE_BG_MIN_F) * breath);

  canvas.fillScreen( idle_dim565( idle_hue2rgb565(hue), f ) );
}

// === NORMAL & SAD Movement ===
float eyeOffsetX = 0, eyeOffsetY = 0;
float targetX = 0, targetY = 0;
unsigned long lastMoveTime = 0;
int moveInterval = 1000;
float easing = 0.1;

// === NORMAL Blink ===
enum BlinkState { IDLE, CLOSING, CLOSED, OPENING };
BlinkState normalBlinkState = IDLE;
unsigned long blinkStart = 0;
int blinkDuration = 150;
unsigned long nextBlinkTime = 0;
float blinkProgress = 0.0;

// === SAD Rain ===
const int numDrops = 40;
int dropX[numDrops], dropY[numDrops], dropSpeed[numDrops];

// === CONFUSE Fog ===
const int numFog = 25;
float fogX[numFog], fogY[numFog], angle[numFog], radius[numFog];
int fogSize[numFog];
uint16_t fogColor[numFog];
bool swapState = false;
unsigned long lastSwitch = 0;

// ==== LOVE (heart eyes + cheek glow) ====
static uint16_t loveColor;

static int  heartW = 20;          // ellipse half-width
static int  heartH = 30;          // ellipse half-height
static float tiltDeg = 45.0f;     // heart lobes tilt
static int  bobAmplitude = 4;     // eye bobbing (px)
static float bobSpeedMs = 300.0f; // bob speed (ms divisor)

// Cheek glow tuning (lightweight)
static const int CHEEK_RINGS      = 8;     // fewer = faster
static float     cheekHz          = 0.55f; // pulse frequency
static int       cheekBaseRadius  = 35;    // average glow radius
static int       cheekPulseAmp    = 6;     // +/- radius change
static int       cheekYOffset     = 60;    // below eyes
static int       cheekXOffset     = 130;   // horizontal from eye centers
static uint16_t  cheekInnerColor;          // set in setup
static uint16_t  cheekOuterColor;          // set in setup
static bool g_carveSessionDone = false;

// === CYCLOP Eye ===
float cyclopScale = 1.0, scalePhase = 0.0;
float pupilX = 0, pupilY = 0, targetPX = 0, targetPY = 0;
unsigned long lastCyclopMove = 0;
bool cyclopPaused = false;

// === SHOCK ===
unsigned long shockStartTime = 0;
bool shockBlinked = false;

// === DRUNK ===
float phaseLeft = 0, phaseRight = 0;

// ------------------------------------------------------------------
// NEW: Cycle state machine (NORMAL is the hub)
// (enum is declared in the header; don't redeclare here)
// ------------------------------------------------------------------
CycleState cycleState = CycleState::NORMAL_IDLE;   // matches `extern` in .h

static uint32_t normalPhaseStart = 0;
static uint32_t normalIdleTargetMs = 0;        // randomized 3–8 s each time
static const uint16_t RECENTER_PIX_TOL = 1;    // snap-to-center tolerance
static const float    RECENTER_EASE    = 0.20f; // easing when recentering
// Forward declaration so we can call it from triggerRandomEmotion()
static EmotionType pickWeightedEmotion();
static void startNormalIdle(uint32_t now) {
  cycleState = CycleState::NORMAL_IDLE;
  normalPhaseStart = now;
  normalIdleTargetMs = random(3000, 8001); // 3–8 seconds
}

// Implement the non-static function declared in the header
void triggerRandomEmotion(uint32_t now) {
  EmotionType e = pickWeightedEmotion();
  currentEmotion = e;
  emotionStartTime = now;
  cycleState = CycleState::PLAY_EMOTION;
  if (currentEmotion == CARVE_SESSION) g_carveSessionDone = false;
  // Start stateful modules on entry
  if (currentEmotion == FORTUNE_TELLER) {
    FortuneTeller::begin(emotionDuration[FORTUNE_TELLER]);
  }
}

// FIXED: Corrected array bounds for emotion selection
static EmotionType pickWeightedEmotion() {
  // auto-size based on emotionWeight[]
  const int count = sizeof(emotionWeight) / sizeof(emotionWeight[0]);

  uint16_t total = 0;
  for (int i = 1; i < count; ++i) {   // skip NORMAL at 0
    total += emotionWeight[i];
  }
  if (total == 0) return SAD; // fallback if all weights are zero

  uint16_t r = (uint16_t)random(1, total + 1);
  uint16_t acc = 0;
  for (int i = 1; i < count; ++i) {
    acc += emotionWeight[i];
    if (r <= acc) return (EmotionType)i;
  }
  return SAD; // safety fallback
}

// Forward declarations of emotion draw functions
static void drawNormal(unsigned long now);
static void drawSad(unsigned long now);
static void drawConfuse(unsigned long now);
static void drawLove(unsigned long now);
static void drawCyclop(unsigned long now);
static void drawShock(unsigned long now);
static void drawDrunk(unsigned long now);
static void drawFurious(unsigned long now);
static void drawAngry(unsigned long now);
static void drawDoubt(unsigned long now);
static void drawAngry2(unsigned long now);
static void drawSmile(unsigned long now);
static void drawBanhChung(unsigned long now);
static void drawDEADPOOL(unsigned long now);
static void drawCARVE_SESSION(unsigned long now);
static void drawSLEEPY(unsigned long now);
static void drawCRY(unsigned long now);
static void drawFIREWORKS(unsigned long now);
static bool drawBOOT_INTRO(unsigned long now);  // forward declaration

void bubuEngineRestartCycle() {
  // Recenter & restart NORMAL idle window
  cycleState = CycleState::NORMAL_IDLE;

  // Reset timers/targets so the cycle truly restarts fresh
  emotionStartTime   = millis();
  normalPhaseStart   = millis();
  normalIdleTargetMs = random(3000, 8001); // 3–8s idle

  // Optional: snap eyes near center so the restart looks clean
  eyeOffsetX = 0.0f;
  eyeOffsetY = 0.0f;

  // Optional: clear canvas immediately (prevents lingering MTE frame if any)
  canvas.fillScreen(GC9A01A_BLACK);
}

// ===== Public "setup/loop" equivalents =====
void bubuEngineSetup() {
  Serial.begin(115200);
  SPI.begin(4, -1, 3);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(GC9A01A_BLACK);
  randomSeed(analogRead(0));
  FortuneTeller::setup(&tft);
  emotionStartTime = millis();
  nextBlinkTime = millis() + random(1000, 3000);
  lastMoveTime = millis();

  u8g2.begin(canvas);
  u8g2.setFont(u8g2_font_unifont_t_vietnamese2);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(GC9A01A_WHITE);
  
  for (int i = 0; i < numDrops; i++) {
    dropX[i] = random(0, 240);
    dropY[i] = random(0, 240);
    dropSpeed[i] = random(1, 12);
  }

  for (int i = 0; i < numFog; i++) {
    fogX[i] = 120;
    fogY[i] = 120;
    angle[i] = random(0, 628) / 100.0;
    radius[i] = random(30, 100);
    fogSize[i] = random(5, 21);
    fogColor[i] = tft.color565(random(100, 255), random(100, 255), random(100, 255));
  }

  shockStartTime = millis();

  // NEW: start in NORMAL idle phase
  startNormalIdle(millis());
  // LOVE: colors
  loveColor       = tft.color565(255, 0, 160);  // pink eyes
  cheekInnerColor = tft.color565(255, 60, 170); // soft pink core
  cheekOuterColor = tft.color565( 40,  0,  35); // darker rim
}

void bubuEngineLoop() {
  uint32_t now = millis();
  bool stillActive = false;
  bool skipClear = (cycleState == CycleState::PLAY_EMOTION && currentEmotion == FIREWORKS);

  // Only clear if we’re not preserving a background
  if (!skipClear && !gPreserveBackground) {
    canvas.fillScreen(GC9A01A_BLACK);
  }

  switch (cycleState) {
  case CycleState::NORMAL_IDLE: {
    // Wander + blink
    gPreserveBackground = true;
    drawIdleTintBackground(now);  
    gPreserveBackground = false;   // ← tint first
    drawNormal(now);                 // ← eyes on top
    

    // After random idle, begin recenter
    if (now - normalPhaseStart >= normalIdleTargetMs) {
    cycleState = CycleState::NORMAL_RECENTER;
    }
  } break;

    case CycleState::NORMAL_RECENTER: {
    // Ease offsets back to center
    eyeOffsetX += (0.0f - eyeOffsetX) * RECENTER_EASE;
    eyeOffsetY += (0.0f - eyeOffsetY) * RECENTER_EASE;

    // Render a frame while recentering
    gPreserveBackground = true;
    drawIdleTintBackground(now);                           // ← tint
    drawBlinkingEyes(centerX + eyeOffsetX, centerY + eyeOffsetY, blinkProgress);
    gPreserveBackground = false;

    if (fabsf(eyeOffsetX) < 0.2f && fabsf(eyeOffsetY) < 0.2f) {
    triggerRandomEmotion(now);
    }
  } break;

    case CycleState::PLAY_EMOTION: {
      stillActive = false;

      switch (currentEmotion) {
        case NORMAL: { gPreserveBackground = true; drawIdleTintBackground(now); drawNormal(now); gPreserveBackground = false; break; }
        case SAD:     drawSad(now);     stillActive = (now - emotionStartTime <= emotionDuration[SAD]);     break;
        case CONFUSE: drawConfuse(now); stillActive = (now - emotionStartTime <= emotionDuration[CONFUSE]); break;
        case LOVE:    drawLove(now);    stillActive = (now - emotionStartTime <= emotionDuration[LOVE]);    break;
        case CYCLOP:  drawCyclop(now);  stillActive = (now - emotionStartTime <= emotionDuration[CYCLOP]);  break;
        case SHOCK:   drawShock(now);   stillActive = (now - emotionStartTime <= emotionDuration[SHOCK]);   break;
        case DRUNK:   drawDrunk(now);   stillActive = (now - emotionStartTime <= emotionDuration[DRUNK]);   break;
        case FURIOUS: drawFurious(now); stillActive = (now - emotionStartTime <= emotionDuration[FURIOUS]); break;
        case ANGRY:   drawAngry(now);   stillActive = (now - emotionStartTime <= emotionDuration[ANGRY]);   break;
        case DOUBT:   drawDoubt(now);   stillActive = (now - emotionStartTime <= emotionDuration[DOUBT]);   break;
        case ANGRY2:  drawAngry2(now);  stillActive = (now - emotionStartTime <= emotionDuration[ANGRY2]);  break;
        case SMILE:  drawSmile(now);  stillActive = (now - emotionStartTime <= emotionDuration[SMILE]);  break;
        case BANH_CHUNG:  drawBanhChung(now);   stillActive = (now - emotionStartTime <= emotionDuration[BANH_CHUNG]);  break;
        case DEADPOOL:  drawDEADPOOL(now);  stillActive = (now - emotionStartTime <= emotionDuration[DEADPOOL]);  break;
        case FORTUNE_TELLER:  stillActive = FortuneTeller::loop();  break;
        case CARVE_SESSION: drawCARVE_SESSION(now);   stillActive = (now - emotionStartTime <= emotionDuration[CARVE_SESSION]);   break;
        case SLEEPY:  drawSLEEPY(now);  stillActive = (now - emotionStartTime <= emotionDuration[SLEEPY]);  break;
        case CRY:  drawCRY(now);  stillActive = (now - emotionStartTime <= emotionDuration[CRY]);  break;
        case FIREWORKS:       drawFIREWORKS(now);        stillActive = (now - emotionStartTime <= emotionDuration[FIREWORKS]);      break;
        case BOOT_INTRO: {
  bool done = drawBOOT_INTRO(now);
  stillActive = !done;
  if (done) {
    currentEmotion   = NORMAL;   // after intro, go to normal eyes
    emotionStartTime = now;
  }
  break;
}
      }
    if (!stillActive) {
      // Teardown stateful modules
      if (currentEmotion == FORTUNE_TELLER) {
      FortuneTeller::end();
      }
      cycleState = CycleState::RETURN_TO_NORMAL;
      }
    } break;

    case CycleState::RETURN_TO_NORMAL: {
      // Place eyes near center and resume NORMAL idle
      eyeOffsetX = (random(0, 2) == 0) ? -1.0f : 1.0f;
      eyeOffsetY = (random(0, 2) == 0) ? -1.0f : 1.0f;

      // One transitional NORMAL frame (optional)
      drawBlinkingEyes(centerX + eyeOffsetX, centerY + eyeOffsetY, blinkProgress);

      // Re-arm NORMAL idle window
      startNormalIdle(now);
    } break;
  }

  // Present the frame (skip while Fortune Teller is actively drawing)
  if (!(cycleState == CycleState::PLAY_EMOTION && currentEmotion == FORTUNE_TELLER)) {
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 240);
  }
  delay(20);
}

// ===============================
// BOOT INTRO (Pulse Rings → Eyes)
// ===============================
// External engine globals/handlers
extern Adafruit_GC9A01A tft;
extern GFXcanvas16      canvas;

extern int centerX, centerY;
extern int eyeWidth, eyeHeight, eyeCorner, eyeDistance;
extern void drawNormal(unsigned long now);
extern void drawBlinkingEyes(float cx, float cy, float progress);

// ---- knobs ----
static const uint16_t BI_RING_PHASE_MS   = 2000;
static const uint8_t  BI_RING_COUNT      = 6;
static const int      BI_RING_THICKNESS  = 5;
static const float    BI_RING_SPACING    = 16.f;
static const float    BI_RING_MAX_R      = 100.f;
static const uint16_t BI_EYES_FADE_MS    = 2000;   // eyes fade in from black
static const uint16_t BI_BLINK_MS        = 200;
// --- background tint knobs ---
static const bool  BI_BG_TINT       = true;   // turn on/off background color
static const uint8_t BI_BG_MIN_F    = 24;     // 0..255 dim at darkest (ring phase)
static const uint8_t BI_BG_MAX_F    = 90;     // 0..255 dim at brightest (ring phase)
static const float BI_BG_HUE_SWEEP  = 64.f;   // how much hue drifts over the ring phase

// ---- helpers ----
static inline float bi_clampf(float x,float a,float b){ return x<a?a:(x>b?b:x); }
static inline float bi_easeOut(float t){ t=bi_clampf(t,0.f,1.f); return 1.f-(1.f-t)*(1.f-t); }
static inline float bi_easeInOut(float t){ t=bi_clampf(t,0.f,1.f); return t*t*(3.f-2.f*t); }

static inline uint16_t bi_dim565(uint16_t c,uint8_t f){
  uint8_t r5=(c>>11)&31, g6=(c>>5)&63, b5=c&31;
  uint8_t r=(r5<<3)|(r5>>2), g=(g6<<2)|(g6>>4), b=(b5<<3)|(b5>>2);
  r=(uint8_t)(((uint16_t)r*f)>>8);
  g=(uint8_t)(((uint16_t)g*f)>>8);
  b=(uint8_t)(((uint16_t)b*f)>>8);
  return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
}
static void bi_dimCanvas(uint8_t f){
  uint16_t *buf = (uint16_t*)canvas.getBuffer();
  for (int i=0, N=240*240; i<N; ++i) buf[i] = bi_dim565(buf[i], f);
}
static inline uint16_t bi_hue2rgb565(uint8_t hue){
  uint8_t seg=hue>>5, off=(hue&31)<<3, r=0,g=0,b=0;
  switch(seg){
    case 0:r=255;       g=off;       b=0;   break;
    case 1:r=255-off;   g=255;       b=0;   break;
    case 2:r=0;         g=255;       b=off; break;
    case 3:r=0;         g=255-off;   b=255; break;
    case 4:r=off;       g=0;         b=255; break;
    default:r=255;      g=0;         b=255-off; break;
  }
  return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
}
static void bi_drawRing(float radius, uint16_t baseCol){
  int r0 = (int)roundf(radius);
  for(int t=0; t<BI_RING_THICKNESS; ++t){
    int rr = r0 + t - BI_RING_THICKNESS/2;
    if (rr <= 0) continue;
    float k   = bi_clampf(1.f - (radius/BI_RING_MAX_R), 0.f, 1.f);
    uint8_t f = (uint8_t)(80 + 160*k); // 80..240 soften toward edge
    canvas.drawCircle(centerX, centerY, rr, bi_dim565(baseCol, f));
  }
}

// ---- palette per cycle ----
static uint16_t bi_palette[BI_RING_COUNT];
static uint8_t  bi_baseHue=0, bi_hueStep=0;
static inline void bi_regenPalette(){
  bi_baseHue = (uint8_t)(esp_random() % 192);
  bi_hueStep = (uint8_t)(8 + (esp_random() % 32)); // 8..39
  for (uint8_t i=0;i<BI_RING_COUNT;i++){
    uint8_t h = (uint8_t)(bi_baseHue + i*bi_hueStep) % 192;
    bi_palette[i] = bi_hue2rgb565(h);
  }
}

// ---- state ----
static bool          bi_inited = false;
static unsigned long bi_t0     = 0;
static uint32_t      bi_cycle  = 0;

static void bi_reset(unsigned long now){
  bi_inited = true;
  bi_t0     = now;
  bi_cycle  = 0;
  bi_regenPalette();
  extern bool gPreserveBackground;
  if (!gPreserveBackground) {
  canvas.fillScreen(GC9A01A_BLACK);}
}

// PUBLIC: draw one frame of the boot intro. Return true when finished.
static bool drawBOOT_INTRO(unsigned long now)
{
  if (!bi_inited) bi_reset(now);

  unsigned long elapsed = now - bi_t0;
  const unsigned long BI_TOTAL =
      (unsigned long)BI_RING_PHASE_MS + BI_EYES_FADE_MS + BI_BLINK_MS;

  // new cycle → new palette (only matters if you ever replay intro)
  uint32_t cycle = elapsed / BI_TOTAL;
  if (cycle != bi_cycle) {
    bi_cycle = cycle;
    bi_regenPalette();
  }

  // -------- frame drawing --------
  if (elapsed < BI_RING_PHASE_MS) {
    // Phase 1: colorful rings expand
    canvas.fillScreen(GC9A01A_BLACK);

    float t = elapsed / (float)BI_RING_PHASE_MS;  // 0..1
    float growth = bi_easeOut(t);

    for (uint8_t i = 0; i < BI_RING_COUNT; i++) {
      float r = (BI_RING_SPACING * i) + growth * BI_RING_MAX_R;
      if (r <= BI_RING_MAX_R) bi_drawRing(r, bi_palette[i]);
    }

  } else {
    // Phase 2: eyes fade in from black → quick blink
    unsigned long tEyes = elapsed - BI_RING_PHASE_MS;

    canvas.fillScreen(GC9A01A_BLACK);

    bool  blink      = false;
    float brightness = 1.0f;

    if (tEyes < BI_EYES_FADE_MS) {
      brightness = bi_easeInOut(tEyes / (float)BI_EYES_FADE_MS); // 0→1
    } else if (tEyes < BI_EYES_FADE_MS + BI_BLINK_MS) {
      blink = true;
      brightness = 1.0f;
    } else {
      brightness = 1.0f;
    }

    // draw your normal eyes (white) then dim whole frame toward black
    if (!blink) {
      drawNormal(now);
    } else {
      // progress=1 → fully shut line; adapt if your blink API differs
      drawBlinkingEyes((float)centerX, (float)centerY, 1.0f);
    }
    uint8_t fade = (uint8_t)(255 * brightness); // 0=black, 255=full
    bi_dimCanvas(fade);
  }

  // present
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 240);

  // done?
  if (elapsed >= BI_TOTAL) {
    bi_inited = false;   // re-arm if ever replayed
    return true;
  }
  return false;
}
// === NORMAL ===
static void drawNormal(unsigned long now) {
  if (now - lastMoveTime > moveInterval) {
    targetX = random(-20, 21);
    targetY = random(-20, 21);
    lastMoveTime = now;
  }

  eyeOffsetX += (targetX - eyeOffsetX) * easing;
  eyeOffsetY += (targetY - eyeOffsetY) * easing;

  if (normalBlinkState == IDLE && now > nextBlinkTime) {
    normalBlinkState = CLOSING;
    blinkStart = now;
  } else if (normalBlinkState == CLOSING) {
    blinkProgress = float(now - blinkStart) / blinkDuration;
    if (blinkProgress >= 1.0) {
      blinkProgress = 1.0;
      normalBlinkState = CLOSED;
      blinkStart = now;
    }
  } else if (normalBlinkState == CLOSED) {
    if (now - blinkStart > 100) {
      normalBlinkState = OPENING;
      blinkStart = now;
    }
  } else if (normalBlinkState == OPENING) {
    blinkProgress = 1.0 - float(now - blinkStart) / blinkDuration;
    if (blinkProgress <= 0.0) {
      blinkProgress = 0.0;
      normalBlinkState = IDLE;
      nextBlinkTime = now + random(1000, 3000);
    }
  }

  drawBlinkingEyes(centerX + eyeOffsetX, centerY + eyeOffsetY, blinkProgress);
}

// === SAD ===
static void drawSad(unsigned long now) {
  if (now - lastMoveTime > moveInterval) {
    targetX = random(-20, 21);
    targetY = random(-20, 21);
    lastMoveTime = now;
  }

  eyeOffsetX += (targetX - eyeOffsetX) * easing;
  eyeOffsetY += (targetY - eyeOffsetY) * easing;

  for (int i = 0; i < numDrops; i++) {
    dropY[i] += dropSpeed[i];
    if (dropY[i] > 240) {
      dropY[i] = 0;
      dropX[i] = random(0, 240);
    }
    int alpha = 100 + sin((now + i * 100) / 300.0) * 50;
    uint8_t b = constrain(alpha, 50, 180);
    uint16_t color = tft.color565(b, b, b + 30);
    canvas.drawFastVLine(dropX[i], dropY[i], 8, color);
  }

  drawBlinkingEyes(centerX + eyeOffsetX, centerY + eyeOffsetY, blinkProgress);
}

// === CONFUSE ===
static void drawConfuse(unsigned long now) {
  if (now - lastSwitch > 500) {
    swapState = !swapState;
    lastSwitch = now;
  }

  for (int i = 0; i < numFog; i++) {
    float swirl = 0.002 + (i * 0.0002);
    angle[i] += swirl;
    fogX[i] = 120 + cos(angle[i]) * radius[i];
    fogY[i] = 120 + sin(angle[i]) * radius[i];
    drawCircleWithOpacity(fogX[i], fogY[i], fogSize[i], fogColor[i], 127);
  }

  if (!swapState) {
    drawEyeCircle(centerX - eyeDistance, centerY, GC9A01A_YELLOW);
    drawEyeFlat(centerX + eyeDistance, centerY, GC9A01A_BLUE);
  } else {
    drawEyeFlat(centerX - eyeDistance, centerY, GC9A01A_BLUE);
    drawEyeCircle(centerX + eyeDistance, centerY, GC9A01A_YELLOW);
  }
}

// === LOVE ===
static inline void putPixelSafe(int x, int y, uint16_t c) {
  if ((unsigned)x < 240u && (unsigned)y < 240u) canvas.drawPixel(x, y, c);
}

// One heart = two rotated ellipses touching
static void drawLoveHeartShape(int cx, int cy, int w, int h, float tiltDeg, uint16_t col) {
  float t = tiltDeg;
  // left/right lobes (rotate simple filled ellipses)
  // (we draw directly into canvas to keep it fast)
  auto drawLoveEllipse = [&](int ex, int ey, int w, int h, float angleDeg, uint16_t col) {
    float angle = angleDeg * DEG_TO_RAD, ca = cosf(angle), sa = sinf(angle);
    for (int dy = -h; dy <= h; dy++) {
      float yRatio = (float)dy / (float)h;
      float span   = sqrtf(fmaxf(0.0f, 1.0f - yRatio * yRatio)) * (float)w;
      int x0 = (int)floorf(-span);
      int x1 = (int) ceilf( span);
      for (int i = x0; i <= x1; i++) {
        float xf = (float)i;
        float xr = xf * ca - (float)dy * sa;
        float yr = xf * sa + (float)dy * ca;
        int px = (int)floorf(ex + xr + 0.5f);
        int py = (int)floorf(ey + yr + 0.5f);
        putPixelSafe(px, py, col);
      }
    }
  };
  drawLoveEllipse(cx - 10, cy, w, h, -t, col);
  drawLoveEllipse(cx + 10, cy, w, h,  +t, col);
}

// Small gradient circle (rings) for cheeks
static inline void makeRamp(uint16_t outer, uint16_t inner, uint16_t ramp[], int rings) {
  uint8_t ir = ((inner >> 11) & 0x1F) << 3;
  uint8_t ig = ((inner >>  5) & 0x3F) << 2;
  uint8_t ib = ( inner        & 0x1F) << 3;

  uint8_t or_ = ((outer >> 11) & 0x1F) << 3;
  uint8_t og  = ((outer >>  5) & 0x3F) << 2;
  uint8_t ob  = ( outer        & 0x1F) << 3;

  for (int i = 0; i < rings; ++i) {
    float t = (float)i / (rings - 1);     // 0..1 outer->inner
    t = t*t*(3 - 2*t);                    // ease
    uint8_t r = or_ + (uint8_t)((ir - or_) * t);
    uint8_t g = og  + (uint8_t)((ig - og ) * t);
    uint8_t b = ob  + (uint8_t)((ib - ob ) * t);
    ramp[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
}

static inline void drawGradientCircleFast(int cx, int cy, int r, uint16_t inner, uint16_t outer, int rings) {
  static uint16_t ramp[16];               // CHEEK_RINGS <= 16
  if (rings > 16) rings = 16;
  makeRamp(outer, inner, ramp, rings);
  for (int i = 0; i < rings; ++i) {
    float t = (float)i / (rings - 1);
    t = t*t*(3 - 2*t);
    int rr = (int)(r * (1.0f - t) + 0.5f);   // big → small
    if (rr <= 0) rr = 1;
    canvas.fillCircle(cx, cy, rr, ramp[i]);
  }
}

static void drawCheekGlows(uint32_t now) {
  float phase = (now * 0.001f) * (2.0f * PI * cheekHz);
  float s = (sinf(phase) + 1.0f) * 0.5f;               // 0..1
  int r = cheekBaseRadius + (int)((s * 2.0f - 1.0f) * cheekPulseAmp);
  if (r < 6) r = 6;

  int leftCX  = centerX - eyeDistance + cheekXOffset;
  int rightCX = centerX + eyeDistance - cheekXOffset;
  int cy      = centerY + cheekYOffset;

  drawGradientCircleFast(leftCX,  cy, r, cheekInnerColor, cheekOuterColor, CHEEK_RINGS);
  drawGradientCircleFast(rightCX, cy, r, cheekInnerColor, cheekOuterColor, CHEEK_RINGS);
}

// NEW love: draw cheeks then heart eyes (uses bobbing)
static void drawLove(unsigned long now) {
  // cheeks first (background)
  drawCheekGlows(now);

  // heart eyes with subtle vertical bob
  float offsetY = sinf(now / bobSpeedMs) * bobAmplitude;
  drawLoveHeartShape(centerX - eyeDistance, centerY + offsetY, heartW, heartH, tiltDeg, loveColor);
  drawLoveHeartShape(centerX + eyeDistance, centerY + offsetY, heartW, heartH, tiltDeg, loveColor);
}

// === CYCLOP ===
static void drawCyclop(unsigned long now) {
  scalePhase += 0.03;
  cyclopScale = 1.0 + 0.05 * sin(scalePhase);

  if (!cyclopPaused && now - lastCyclopMove > 1000) {
    targetPX = random(-50, 51);
    targetPY = random(-50, 51);
    lastCyclopMove = now;
    cyclopPaused = true;
  } else if (cyclopPaused && now - lastCyclopMove > 500) {
    cyclopPaused = false;
  }

  if (!cyclopPaused) {
    pupilX += (targetPX - pupilX) * 0.1;
    pupilY += (targetPY - pupilY) * 0.1;
  }

  canvas.fillScreen(GC9A01A_YELLOW);
  int r = 100 * cyclopScale;
  canvas.fillCircle(centerX, centerY, r, GC9A01A_WHITE);
  canvas.fillCircle(centerX + pupilX * cyclopScale, centerY + pupilY * cyclopScale, 30, GC9A01A_BLACK);
}

// === SHOCK ===
static void drawShock(unsigned long now) {
  unsigned long e = now - shockStartTime;
  int leftX = centerX - eyeDistance;
  int rightX = centerX + eyeDistance;
  int yOffset = 0;

  if (e < 3000) {
    yOffset = sin(now / 30.0) * 10;
    drawCircleEyes(leftX, rightX, centerY + yOffset, 35);
  } else if (e < 4000) {
    drawCircleEyes(leftX, rightX, centerY, 35);
  } else if (e < 5000) {
    if (!shockBlinked) {
      drawBlinkLine(leftX, centerY + 17, 35, 4, GC9A01A_WHITE);
      drawBlinkLine(rightX, centerY + 17, 35, 4, GC9A01A_WHITE);
      shockBlinked = true;
    } else {
      drawCircleEyes(leftX, rightX, centerY, 35);
    }
  } else {
    shockStartTime = now;
    shockBlinked = false;
  }
}

// === DRUNK ===
static void drawDrunk(unsigned long now) {
  phaseLeft += 0.1;
  phaseRight -= 0.1;
  drawDrunkWhirlpool(centerX - eyeDistance, centerY, 35, true, phaseLeft);
  drawDrunkWhirlpool(centerX + eyeDistance, centerY, 35, false, phaseRight);
}

// === FURIOUS ===
static void drawFurious(unsigned long now) {
  // total 8s emotion (phase split can be adjusted)
  const unsigned long P0 = 1600;  // enter (squint/inward)
  const unsigned long P1 = 4400;  // hold + shake
  const unsigned long P2 = 400;  // release

  // elapsed time for this emotion
  const unsigned long elapsed = now - emotionStartTime;

  // eye color fade: 0..6000 ms -> 0xFFFF (white) to 0xF800 (red)
  const uint16_t startColor = 0xFFFF;
  const uint16_t endColor   = 0xF800;
  const float    tColor     = (elapsed < 6000UL) ? (float)elapsed / 6000.0f : 1.0f;
  const uint16_t colEye     = HE_colorLerp(startColor, endColor, tColor); // from helpers.h/.cpp

  // geometry (use your globals)
  const int cx = canvas.width()  / 2;
  const int cy = canvas.height() / 2;
  const int baseW   = eyeWidth;
  const int baseH   = eyeHeight;
  const int leftCX  = cx - eyeDistance;
  const int rightCX = cx + eyeDistance;
  const int r       = eyeRadius;



  // helpers that use the lerped color
  auto eyeBox = [&](int ecx, int ecy, int w, int h) {
    canvas.fillRoundRect(ecx - w/2, ecy - h/2, w, h, r, colEye);
  };
  auto brows = [&](int lc, int rc, int y, int extent) {
    const int l_out_x = lc - (baseW/2), l_in_x  = lc + (baseW/2);
    const int r_out_x = rc + (baseW/2), r_in_x  = rc - (baseW/2);
    for (int t = 0; t < 7; ++t) {
      canvas.drawLine(l_out_x + 30, y - extent + t, l_in_x, y + extent + t, colEye);
      canvas.drawLine(r_out_x - 30, y - extent + t, r_in_x, y + extent + t, colEye);
    }
  };

  // phase 0: enter (squint + slight inward)
  if (elapsed < P0) {
    const float t = (float)elapsed / (float)P0;
    const int   eh  = baseH - int(t * 25.0f);
    const int   lcx = leftCX  + int(t * 4.0f);
    const int   rcx = rightCX - int(t * 4.0f);
    eyeBox(lcx, cy, baseW, eh);
    eyeBox(rcx, cy, baseW, eh);
    return;
  }

  // phase 1: hold + shake
  if (elapsed < P0 + P1) {
    const unsigned long in = elapsed - P0;
    const int shake = ((in / 50) % 2 == 0) ? +2 : -2;  // ~20Hz, 2px
    const int eh = baseH - 25;
    eyeBox(leftCX  + shake, cy, baseW, eh);
    eyeBox(rightCX + shake, cy, baseW, eh);
    brows(leftCX + shake, rightCX + shake, cy - baseH/2 - 6, 10);
    return;
  }

  // phase 2: release
  if (elapsed < P0 + P1 + P2) {
    const unsigned long in = elapsed - (P0 + P1);
    const float t = (float)in / (float)P2;
    const int   eh  = (baseH - 25) + int(t * 25.0f);
    const int   lcx = leftCX  + int((1.0f - t) * 4.0f);
    const int   rcx = rightCX - int((1.0f - t) * 4.0f);
    eyeBox(lcx, cy, baseW, eh);
    eyeBox(rcx, cy, baseW, eh);
    return;
  }
}

// === ANGRY ===

static inline void EE_drawAngryEyes() {
  // Use your globals: centerX/centerY/eyeRadius/eyeDistance
  const int h = 35;  // or use eyeHeight if you prefer
  const int leftX  = centerX + eyeDistance;
  const int rightX = centerX - eyeDistance;

  // white rounded eye boxes
  canvas.fillRoundRect(leftX  - eyeRadius, centerY - h/2,
                       eyeRadius*2, h, eyeRadius, 0xFFFF);
  canvas.fillRoundRect(rightX - eyeRadius, centerY - h/2,
                       eyeRadius*2, h, eyeRadius, 0xFFFF);
}

static inline void EE_drawAngryCross(uint8_t intensity) {
  // Fast RGB565 red from 0..255 intensity
  const uint16_t col = ((uint16_t)(intensity & 0xF8) << 8);

  const int rightEyeX = centerX + eyeDistance;
  const int crossX = rightEyeX + eyeRadius - 5;  // just off the right eye edge
  const int crossY = centerY - 40;

  const int outerLength = 30, outerThickness = 15;
  const int innerLength = 30, innerThickness = 5;

  // outer red cross
  canvas.fillRect(crossX - outerLength/2,   crossY - outerThickness/2,
                  outerLength,              outerThickness,            col);
  canvas.fillRect(crossX - outerThickness/2,crossY - outerLength/2,
                  outerThickness,           outerLength,               col);

  // inner cutout (black hole)
  canvas.fillRect(crossX - innerLength/2,    crossY - innerThickness/2,
                  innerLength,               innerThickness,           0x0000);
  canvas.fillRect(crossX - innerThickness/2, crossY - innerLength/2,
                  innerThickness,            innerLength,              0x0000);
}

static void drawAngry(unsigned long now) {

  // 1s pulse (0→1→0) based on elapsed time of this emotion
  const uint32_t e      = now - emotionStartTime;
  const uint32_t cycle  = e % 1000;
  const float pulse     = (cycle < 500)
                          ? (cycle / 500.0f)
                          : (1.0f - (cycle - 500) / 500.0f);
  const uint8_t intensity = (uint8_t)(pulse * 255.0f);

  // Draw eyes + pulsing red cross
  EE_drawAngryEyes();
  EE_drawAngryCross(intensity);
}

// === DOUBT (NEW) ===
// Randomized per-cycle: which eye grows, timings, and growth intensity.
enum EyeSide : uint8_t { LEFT_EYE = 0, RIGHT_EYE = 1 };
static uint16_t DOUBT_totalMs  = 2100;
static uint16_t DOUBT_growMs   = 100;
static uint16_t DOUBT_holdMs   = 1000;
static uint16_t DOUBT_returnMs = 1000;
static float    DOUBT_maxScale = 1.30f; // 1.20–1.40 per cycle
static int      DOUBT_distBoost = 4;    // px apart at full growth
static EyeSide  DOUBT_side = LEFT_EYE;

static void DOUBT_resetParamsAndPickSide() {
  DOUBT_growMs   = random(80, 301);     // 80–300 ms
  DOUBT_holdMs   = random(800, 1501);   // 0.8–1.5 s
  DOUBT_returnMs = random(300, 701);    // 0.3–0.7 s
  DOUBT_totalMs  = DOUBT_growMs + DOUBT_holdMs + DOUBT_returnMs;

  DOUBT_maxScale  = 1.20f + (random(0, 21) / 100.0f); // 1.20–1.40
  DOUBT_distBoost = random(4, 12);

  DOUBT_side = (random(0, 2) == 0) ? LEFT_EYE : RIGHT_EYE;
}

static void drawDoubt(unsigned long now) {
  static unsigned long start = 0;
  if (start == 0) { start = now; DOUBT_resetParamsAndPickSide(); }

  unsigned long e = (now - start);
  if (e >= DOUBT_totalMs) {
    start = now;
    DOUBT_resetParamsAndPickSide();
    e = 0;
  }

  // base values
  const int baseW = eyeWidth;
  const int baseH = eyeHeight;
  const int baseR = eyeCorner;
  const int baseD = eyeDistance;

  // outputs
  int Lw = baseW, Lh = baseH, Lr = baseR;
  int Rw = baseW, Rh = baseH, Rr = baseR;
  int dist = baseD;

  const bool scaleLeft  = (DOUBT_side == LEFT_EYE);
  const bool scaleRight = !scaleLeft;

  if (e < DOUBT_growMs) {
    float t = EE_jitteredEase((float)e / (float)DOUBT_growMs);
    float s = 1.0f + (DOUBT_maxScale - 1.0f) * t;
    dist  = baseD + (int)(DOUBT_distBoost * t + 0.5f);
    if (scaleLeft) {
      Lw = (int)(baseW * s + 0.5f);
      Lh = (int)(baseH * s + 0.5f);
      Lr = (int)(baseR * s + 0.5f);
    } else {
      Rw = (int)(baseW * s + 0.5f);
      Rh = (int)(baseH * s + 0.5f);
      Rr = (int)(baseR * s + 0.5f);
    }
  } else if (e < DOUBT_growMs + DOUBT_holdMs) {
    float s = DOUBT_maxScale;
    dist  = baseD + DOUBT_distBoost;
    if (scaleLeft) {
      Lw = (int)(baseW * s + 0.5f);
      Lh = (int)(baseH * s + 0.5f);
      Lr = (int)(baseR * s + 0.5f);
    } else {
      Rw = (int)(baseW * s + 0.5f);
      Rh = (int)(baseH * s + 0.5f);
      Rr = (int)(baseR * s + 0.5f);
    }
  } else {
    unsigned long e2 = e - (DOUBT_growMs + DOUBT_holdMs);
    float t = EE_jitteredEase((float)e2 / (float)DOUBT_returnMs);
    float s = 1.0f + (DOUBT_maxScale - 1.0f) * (1.0f - t);
    dist  = baseD + (int)(DOUBT_distBoost * (1.0f - t) + 0.5f);
    if (scaleLeft) {
      Lw = (int)(baseW * s + 0.5f);
      Lh = (int)(baseH * s + 0.5f);
      Lr = (int)(baseR * s + 0.5f);
    } else {
      Rw = (int)(baseW * s + 0.5f);
      Rh = (int)(baseH * s + 0.5f);
      Rr = (int)(baseR * s + 0.5f);
    }
  }

  // draw both eyes
  int leftX  = centerX - dist;
  int rightX = centerX + dist;
  canvas.fillRoundRect(leftX  - Lw/2,  centerY - Lh/2,  Lw,  Lh,  Lr, 0xFFFF);
  canvas.fillRoundRect(rightX - Rw/2,  centerY - Rh/2,  Rw,  Rh,  Rr, 0xFFFF);
}

// === ANGRY2 (angry brow morph + side-to-side jitter) ===
static void drawAngry2(uint32_t now) {
  // ----- Timing (matches your sketch) -----
  const uint32_t PHASE_TO_ANGRY = 1000;   // normal -> angry
  const uint32_t PHASE_HOLD     = 2000;  // hold angry
  const uint32_t PHASE_TO_NORM  = 500;   // angry -> normal
  const uint32_t TOTAL_MS = PHASE_TO_ANGRY + PHASE_HOLD + PHASE_TO_NORM;

  // ----- Look & feel -----
  const int TOP_RISE_MAX   = 22;       // top “carve” amount (angry brow)
  const uint16_t COL_WHITE = 0xFFFF;
  const uint16_t COL_RED   = 0xF800;

  // Jitter (side-to-side shake during HOLD)
  const unsigned JITTER_STEP_MS = 60;
  const int      JITTER_MAG_PX  = 2;

  const uint32_t e = now % TOTAL_MS;

  // Use engine globals for geometry
  int leftX  = centerX - eyeDistance;
  int rightX = centerX + eyeDistance;

  // top-carve (per eye)
  int riseL_L = 0, riseR_L = 0; // left eye rises on center side => use riseR_L
  int riseL_R = 0, riseR_R = 0; // right eye rises on center side => use riseL_R
  uint16_t eyeCol = COL_WHITE;

  if (e < PHASE_TO_ANGRY) {
    // NORMAL -> ANGRY (with a touch of jitter on easing)
    float t = EE_jitteredEase(e / float(PHASE_TO_ANGRY), 0.015f);
    riseR_L = int(TOP_RISE_MAX * t + 0.5f); // left slopes down toward center
    riseL_R = int(TOP_RISE_MAX * t + 0.5f); // right slopes down toward center
    eyeCol  = HE_colorLerp(COL_WHITE, COL_RED, t);

  } else if (e < PHASE_TO_ANGRY + PHASE_HOLD) {
    // HOLD ANGRY
    riseR_L = TOP_RISE_MAX;
    riseL_R = TOP_RISE_MAX;
    eyeCol  = COL_RED;

    int dx = HE_jitter(now, JITTER_STEP_MS, JITTER_MAG_PX);
    leftX  += dx;
    rightX += dx;

  } else {
    // ANGRY -> NORMAL
    const uint32_t e2 = e - (PHASE_TO_ANGRY + PHASE_HOLD);
    float t = EE_jitteredEase(e2 / float(PHASE_TO_NORM), 0.015f);
    float back = 1.0f - t;
    riseR_L = int(TOP_RISE_MAX * back + 0.5f);
    riseL_R = int(TOP_RISE_MAX * back + 0.5f);
    eyeCol  = HE_colorLerp(COL_RED, COL_WHITE, t);
  }



  // Base rounded eyes
  auto eyeBox = [&](int cx, int cy, int w, int h, int r, uint16_t col) {
    canvas.fillRoundRect(cx - w/2, cy - h/2, w, h, r, col);
  };

  // Carve everything ABOVE a diagonal from (x0, y0+riseL) to (x1, y0+riseR)
  auto carveTopDiagonal = [&](int cx, int cy, int riseL, int riseR) {
    int x0 = cx - eyeWidth/2;
    int x1 = cx + eyeWidth/2;
    int y0 = cy - eyeHeight/2;
    int yTop = y0 - 60; // far above
    int yA = y0 + riseL;
    int yB = y0 + riseR;
    // Two triangles fill everything above the diagonal with background
    canvas.fillTriangle(x0, yA, x1, yB, x0, yTop, 0x0000);
    canvas.fillTriangle(x1, yB, x1, yTop, x0, yTop, 0x0000);
  };

  // Draw both eyes with top-carve
  // Left eye: center-side is RIGHT → use riseR_L
  eyeBox(leftX, centerY, eyeWidth, eyeHeight, eyeCorner, eyeCol);
  if (riseR_L > 0) carveTopDiagonal(leftX, centerY, /*riseL*/0, /*riseR*/riseR_L);

  // Right eye: center-side is LEFT → use riseL_R
  eyeBox(rightX, centerY, eyeWidth, eyeHeight, eyeCorner, eyeCol);
  if (riseL_R > 0) carveTopDiagonal(rightX, centerY, /*riseL*/riseL_R, /*riseR*/0);
}
// === SMILE (carved eyes + gentle giggle + twinkling sparkles) ===
static void drawSmile(uint32_t nowMs) {
  // ---------- Timings (ms) ----------
  const uint32_t PHASE_TO_SMILE = 500;
  const uint32_t PHASE_HOLD     = 2000;
  const uint32_t PHASE_TO_NORM  = 500;
  const uint32_t TOTAL_MS       = PHASE_TO_SMILE + PHASE_HOLD + PHASE_TO_NORM;

  // ---------- Carve ranges ----------
  const int carveY_start = 100;  // px below eye center (barely touches)
  const int carveY_end   = 52;   // px below eye center (smile)
  const int carveR_start = 60;
  const int carveR_end   = 60;

  // ---------- Giggle ----------
  const float GIGGLE_HZ     = 4.0f;
  const int   GIGGLE_AMP_Y  = 4;
  const int   GIGGLE_AMP_R  = 2;
  const float GIGGLE_PHASE_OFFSET = 0.0f;

  // Colors
  const uint16_t COL_BG   = 0x0000; // black
  const uint16_t COL_EYE  = 0xFFFF; // white
  const uint16_t COL_YELL = 0xFFE0; // yellow for sparkles

  // ---------- Tiny helpers (local) ----------
  auto lerpI = [](int a, int b, float t){ return a + (int)((b - a) * t + 0.5f); };

  auto scale565 = [](uint16_t c, float brightness)->uint16_t {
    if (brightness <= 0) return 0;
    if (brightness >= 1) return c;
    uint8_t r = ((c >> 11) & 0x1F) << 3;
    uint8_t g = ((c >> 5)  & 0x3F) << 2;
    uint8_t b = ( c        & 0x1F) << 3;
    r = (uint8_t)(r * brightness);
    g = (uint8_t)(g * brightness);
    b = (uint8_t)(b * brightness);
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  };

  auto drawRoundedEye = [&](int cx, int cy, int w, int h, int r, uint16_t col){
    canvas.fillRoundRect(cx - w/2, cy - h/2, w, h, r, col);
  };
  auto carveEye = [&](int cx, int cy, int yOff, int r){
    if (r > 0) canvas.fillCircle(cx, cy + yOff, r, COL_BG);
  };
  auto drawCarvedEye = [&](int cx, int cy, int carveYOff, int carveR){
    drawRoundedEye(cx, cy, eyeWidth, eyeHeight, eyeCorner, COL_EYE);
    carveEye(cx, cy, carveYOff, carveR);
  };

  auto drawSparkle4 = [&](int cx, int cy, uint16_t color, int size){
    for (int i = 0; i < size; i++) { // vertical diamond
      int w = size - i;
      canvas.drawFastHLine(cx - w/2, cy - i, w, color);
      canvas.drawFastHLine(cx - w/2, cy + i, w, color);
    }
    for (int i = 0; i < size; i++) { // horizontal diamond
      int h = size - i;
      canvas.drawFastVLine(cx - i, cy - h/2, h, color);
      canvas.drawFastVLine(cx + i, cy - h/2, h, color);
    }
  };

  struct Sparkle { int x, y, size; unsigned long offset, period; };
  static Sparkle sparkles[7];
  static bool sparkleInit = false;

  // Re-init sparkles at the start of each SMILE play
  if (!sparkleInit || (nowMs - emotionStartTime) < 20) {
    const int leftX  = centerX - eyeDistance;
    const int rightX = centerX + eyeDistance;
    sparkles[0] = { leftX  - 40, centerY - 60, 6, (unsigned long)random(0,1000), (unsigned long)(600 + random(0,500)) };
    sparkles[1] = { leftX  + 50, centerY - 70, 5, (unsigned long)random(0,1000), (unsigned long)(700 + random(0,600)) };
    sparkles[2] = { rightX + 42, centerY - 55, 7, (unsigned long)random(0,1000), (unsigned long)(800 + random(0,500)) };
    sparkles[3] = { rightX - 52, centerY - 68, 5, (unsigned long)random(0,1000), (unsigned long)(650 + random(0,700)) };
    sparkles[4] = { leftX  - 60, centerY + 30, 6, (unsigned long)random(0,1000), (unsigned long)(900 + random(0,600)) };
    sparkles[5] = { rightX + 58, centerY + 28, 6, (unsigned long)random(0,1000), (unsigned long)(750 + random(0,700)) };
    sparkles[6] = { centerX - 70,      centerY - 10, 5, (unsigned long)random(0,1000), (unsigned long)(700 + random(0,600)) };
    sparkles[7] = { centerX + 72,      centerY - 15, 7, (unsigned long)random(0,1000), (unsigned long)(800 + random(0,700)) };

    sparkleInit = true;
  }
  // ---------- Build frame ----------
  const uint32_t e = nowMs % TOTAL_MS;


  if (e < PHASE_TO_SMILE) {
    // NORMAL -> SMILE
    const float t = EE_easeInOut(e / float(PHASE_TO_SMILE));
    const int yOff = lerpI(carveY_start, carveY_end, t);
    const int rVal = lerpI(carveR_start, carveR_end, t);
    drawCarvedEye(centerX - eyeDistance, centerY, yOff, rVal);
    drawCarvedEye(centerX + eyeDistance, centerY, yOff, rVal);

  } else if (e < PHASE_TO_SMILE + PHASE_HOLD) {
    // HOLD (giggle)
    const int yBase = carveY_end;
    const int rBase = carveR_end;

    const float tsec = nowMs / 1000.0f;
    const float w    = 2.0f * PI * GIGGLE_HZ;

    // Left
    {
      int y = yBase + (int)(GIGGLE_AMP_Y * sinf(w * tsec));
      int r = rBase + (int)(GIGGLE_AMP_R * sinf(w * tsec * 1.7f));
      if (r < 1) r = 1;
      drawCarvedEye(centerX - eyeDistance, centerY, y, r);
    }
    // Right (phase shifted)
    {
      int y = yBase + (int)(GIGGLE_AMP_Y * sinf(w * tsec + GIGGLE_PHASE_OFFSET));
      int r = rBase + (int)(GIGGLE_AMP_R * sinf(w * tsec * 1.7f + GIGGLE_PHASE_OFFSET));
      if (r < 1) r = 1;
      drawCarvedEye(centerX + eyeDistance, centerY, y, r);
    }

  } else {
    // SMILE -> NORMAL
    const uint32_t e2 = e - (PHASE_TO_SMILE + PHASE_HOLD);
    const float t = EE_easeInOut(e2 / float(PHASE_TO_NORM));
    const int yOff = lerpI(carveY_end,   carveY_start, t);
    const int rVal = lerpI(carveR_end,   carveR_start, t);
    drawCarvedEye(centerX - eyeDistance, centerY, yOff, rVal);
    drawCarvedEye(centerX + eyeDistance, centerY, yOff, rVal);
  }

  // ---------- Twinkling sparkles on top ----------
  for (int i = 0; i < 6; ++i) {
    const unsigned long T = sparkles[i].period;
    const unsigned long tlocal = (nowMs + sparkles[i].offset) % T;
    const float phase = tlocal / float(T); // 0..1
    const float fade = 0.5f * (1.0f - cosf(2.0f * PI * phase)); // 0→1→0
    drawSparkle4(sparkles[i].x, sparkles[i].y, scale565(COL_YELL, fade), sparkles[i].size);
  }
}
// === BANH_CHUNG (leaf-wrapped cake eyes + thin double ribbons + random sparkles) ===
static void drawBanhChung(uint32_t nowMs){
  // —— timings (ms) ——
  const uint32_t PHASE_IN   = 500, PHASE_HOLD = 3500, PHASE_OUT  = 500;
  const uint32_t TOTAL_MS   = PHASE_IN + PHASE_HOLD + PHASE_OUT;

  // —— colors (RGB565) ——
  const uint16_t COL_BG   = 0x0000; // black
  const uint16_t COL_WHITE= 0xFFFF;
  const uint16_t LEAF_MID = 0x07E0; // bright leaf green
  const uint16_t RIBBON   = 0xB4A5; // brown you picked
  const uint16_t GLINT    = 0xFFE0; // yellow sparkle

  // —— geometry (from engine globals) ——
  const int Lx = centerX - eyeDistance;
  const int Rx = centerX + eyeDistance;
  const int Ey = centerY;

  // —— helpers (local, no collisions) ——
  auto clamp01 = [](float v){ return v<0?0:(v>1?1:v); };
  auto easeInOut = [&](float t){
    t = clamp01(t);
    return (t < 0.5f) ? (2.0f*t*t) : (-1.0f + (4.0f - 2.0f*t)*t);
  };
  auto colorLerp565 = [&](uint16_t c1, uint16_t c2, float t){
    t = clamp01(t);
    uint8_t r1=((c1>>11)&0x1F), g1=((c1>>5)&0x3F), b1=(c1&0x1F);
    uint8_t r2=((c2>>11)&0x1F), g2=((c2>>5)&0x3F), b2=(c2&0x1F);
    uint8_t r=r1+(int)((r2-r1)*t+0.5f), g=g1+(int)((g2-g1)*t+0.5f), b=b1+(int)((b2-b1)*t+0.5f);
    return (uint16_t)((r<<11)|(g<<5)|b);
  };
  auto scale565 = [&](uint16_t c, float b){
    if (b <= 0) return (uint16_t)0;
    if (b >= 1) return c;
    uint8_t r = ((c>>11)&0x1F)<<3, g=((c>>5)&0x3F)<<2, bl=(c&0x1F)<<3;
    r=(uint8_t)(r*b); g=(uint8_t)(g*b); bl=(uint8_t)(bl*b);
    return (uint16_t)(((r&0xF8)<<8)|((g&0xFC)<<3)|(bl>>3));
  };
  auto fillEyeBox = [&](int cx,int cy,int w,int h,int r,uint16_t col){
    canvas.fillRoundRect(cx - w/2, cy - h/2, w, h, r, col);
  };
  auto drawRibbonRect = [&](int cx,int cy,int w,int h){
    int band = max(3, w/14);    // thin strap
    int gap  = band * 6;        // spacing between the two straps
    canvas.fillRect(cx - gap/2 - band/2, cy - h/2, band, h, RIBBON);
    canvas.fillRect(cx + gap/2 - band/2, cy - h/2, band, h, RIBBON);
    canvas.fillRect(cx - w/2, cy - gap/2 - band/2, w, band, RIBBON);
    canvas.fillRect(cx - w/2, cy + gap/2 - band/2, w, band, RIBBON);
  };
  auto drawSparkle4 = [&](int cx, int cy, uint16_t color, int size){
    for (int i = 0; i < size; i++) {
      int w = size - i;
      canvas.drawFastHLine(cx - w/2, cy - i, w, color);
      canvas.drawFastHLine(cx - w/2, cy + i, w, color);
    }
    for (int i = 0; i < size; i++) {
      int h = size - i;
      canvas.drawFastVLine(cx - i, cy - h/2, h, color);
      canvas.drawFastVLine(cx + i, cy - h/2, h, color);
    }
  };

  // —— sparkle state (random across screen, reshuffle once per cycle) ——
  struct Sparkle { int x, y, size; unsigned long offset, period; };
  static const int NUM_SPARKLES = 10;
  static Sparkle sparkles[NUM_SPARKLES];
  static uint32_t last_cycle_id = 0xFFFFFFFF;

  auto initSparklesForCycle = [&](uint32_t cycle_id){
    for (int i = 0; i < NUM_SPARKLES; i++) {
      int s = random(4, 8);
      int margin = s + 2;
      sparkles[i].x = random(margin, 240 - margin);
      sparkles[i].y = random(margin, 240 - margin);
      sparkles[i].size = s;
      sparkles[i].offset = (unsigned long)random(0, 1000);
      sparkles[i].period = (unsigned long)(600 + random(0, 700)); // 600..1299 ms
    }
    last_cycle_id = cycle_id;
  };

  // —— phase progress ——
  const uint32_t e = nowMs % TOTAL_MS;
  const uint32_t cycle_id = nowMs / TOTAL_MS;
  if (cycle_id != last_cycle_id) initSparklesForCycle(cycle_id);

  float tIn=0.f, tOut=0.f;
  if (e < PHASE_IN) tIn = easeInOut(e / float(PHASE_IN));
  else if (e < PHASE_IN + PHASE_HOLD) tIn = 1.f;
  else { uint32_t e2 = e - (PHASE_IN + PHASE_HOLD); tOut = easeInOut(e2 / float(PHASE_OUT)); }
  float tMorph = (tIn > 0.f) ? tIn : (1.f - tOut);

  // morph: normal eye → cake
  const int w0 = eyeWidth,  h0 = eyeHeight, r0 = eyeCorner;
  const int w1 = eyeWidth + 6, h1 = eyeHeight + 6, r1 = 8;
  const int wM = w0 + (int)((w1 - w0) * tMorph + 0.5f);
  const int hM = h0 + (int)((h1 - h0) * tMorph + 0.5f);
  const int rM = r0 + (int)((r1 - r0) * tMorph + 0.5f);
  const uint16_t colEye = colorLerp565(COL_WHITE, LEAF_MID, tMorph);

  // gentle bounce during hold
  int bounceY = 0;
  if (tIn == 1.f && tOut == 0.f) {
    float tsec = nowMs / 1000.0f;
    bounceY = (int)(2.0f * sinf(2.0f * PI * 2.2f * tsec));
  }

  // draw frame


  fillEyeBox(Lx, Ey + bounceY, wM, hM, rM, colEye);
  if (tMorph > 0.15f) drawRibbonRect(Lx, Ey + bounceY, wM, hM);

  fillEyeBox(Rx, Ey - bounceY, wM, hM, rM, colEye);
  if (tMorph > 0.15f) drawRibbonRect(Rx, Ey - bounceY, wM, hM);

  // twinkling sparkles only during hold
  if (tIn == 1.f && tOut == 0.f) {
    for (int i = 0; i < NUM_SPARKLES; ++i) {
      const unsigned long T  = sparkles[i].period;
      const unsigned long tl = (nowMs + sparkles[i].offset) % T;
      const float phase = tl / float(T);
      const float fade  = 0.5f * (1.f - cosf(2.f * PI * phase));
      drawSparkle4(sparkles[i].x, sparkles[i].y, scale565(GLINT, fade), sparkles[i].size);
    }
  }
}
// ===================== DEADPOOL (ring -> eyes -> brow -> dots -> divider) =====================
namespace {
  // ---- Tunables (match your standalone) ----
  const uint16_t DP_COL_BG   = 0x0000; // black
  const uint16_t DP_COL_EYE  = 0xFFFF; // white
  const uint16_t DP_COL_TRI  = 0x0000; // black brow (use 0xC618 for gray)
  const uint16_t DP_COL_RED  = 0xF800; // ring + divider + dots

  // Eyes
  const int DP_EYE_W = 70, DP_EYE_H = 40, DP_EYE_R = 20;
  const int DP_EYE_SPACING = 50;

  // Ring/divider
  // OUTER_R intended to touch the edge on 240x240. If your canvas is already cleared each frame,
  // we can skip explicit clear and let the ring overwrite.
  // If needed, uncomment a full clear: canvas.fillScreen(DP_COL_BG);
  const int DP_OUTER_R    = 120; // 120
  const int DP_RING_THICK = 16;
  const int DP_DIVIDER_W  = 18;

  // Brow (apex-down triangle)
  const int DP_TRI_W = 140;
  const int DP_TRI_H = 40;
  const int DP_TRI_SHIFT_UP = -25;

  // Motion phases (ms)
  const uint32_t DP_PHASE_IN_MS   = 800;
  const uint32_t DP_PHASE_HOLD_MS = 6400;
  const uint32_t DP_PHASE_OUT_MS  = 800;
  const uint32_t DP_TOTAL         = DP_PHASE_IN_MS + DP_PHASE_HOLD_MS + DP_PHASE_OUT_MS;
  const int      DP_MOVE_AMPL_PX  = 18;

  // Dots (spawn on HOLD; clear on IN)
  const int   DP_DOTS_COUNT   = 240;
  const float DP_DOT_R_MIN    = 1.5f;
  const float DP_DOT_R_MAX    = 5.0f;
  const float DP_BIAS_X       = 3.0f;   // stronger density toward right
  const float DP_BIAS_Y       = 3.0f;   // stronger density toward bottom
  const float DP_ACCEPT_GAMMA = 0.5f;   // thinner toward top-left

  struct DPDot { int x, y, r; };
  static DPDot dpDots[DP_DOTS_COUNT];
  static int   dpDotsPlaced = 0;
  static bool  dpDotsActive = false;

  enum DPPhase { DP_IN, DP_HOLD, DP_OUT };
  static DPPhase dpPrevPhase = DP_OUT; // start such that first IN clears

  // helpers
  inline float dpFrand() { return (float)random(0, 10000) / 10000.0f; }
  inline float dpRandBiasedHigh(float k) {
    float r = dpFrand();
    return 1.0f - powf(r, k); // bias toward 1.0
  }
  inline float dpEaseInOut(float x){
    if (x <= 0) return 0; if (x >= 1) return 1;
    return (x < 0.5f) ? (2.0f*x*x) : (-1.0f + (4.0f - 2.0f*x)*x);
  }

  bool dpSampleDot(int &xOut, int &yOut, int &rOut) {
    float ux = dpRandBiasedHigh(DP_BIAS_X);
    float uy = dpRandBiasedHigh(DP_BIAS_Y);
    int   x  = (int)(ux * (240 - 1) + 0.5f);
    int   y  = (int)(uy * (240 - 1) + 0.5f);

    float p = powf((x / (float)240) * (y / (float)240), DP_ACCEPT_GAMMA);
    if (dpFrand() > p) return false;

    float rr = DP_DOT_R_MIN + (DP_DOT_R_MAX - DP_DOT_R_MIN) * dpFrand();
    int r = (int)(rr + 0.5f); if (r < 1) r = 1;
    xOut = x; yOut = y; rOut = r;
    return true;
  }

  void dpGenerateDots() {
    dpDotsPlaced = 0;
    const int MAX_TRIES = DP_DOTS_COUNT * 12;
    int tries = 0;
    while (dpDotsPlaced < DP_DOTS_COUNT && tries < MAX_TRIES) {
      ++tries;
      int x, y, r;
      if (!dpSampleDot(x, y, r)) continue;
      dpDots[dpDotsPlaced++] = {x, y, r};
    }
    dpDotsActive = true;
  }

  void dpClearDots() { dpDotsActive = false; dpDotsPlaced = 0; }

  // drawing
  inline void dpDrawRing() {
    int innerR = DP_OUTER_R - DP_RING_THICK; if (innerR < 0) innerR = 0;
    // base is already blanked by engine; draw ring
    canvas.fillCircle(centerX, centerY, DP_OUTER_R, DP_COL_RED);
    canvas.fillCircle(centerX, centerY, innerR, DP_COL_BG);
  }
  inline void dpDrawEyes() {
    int leftX  = centerX - DP_EYE_SPACING;
    int rightX = centerX + DP_EYE_SPACING;
    canvas.fillRoundRect(leftX  - DP_EYE_W/2, centerY - DP_EYE_H/2, DP_EYE_W, DP_EYE_H, DP_EYE_R, DP_COL_EYE);
    canvas.fillRoundRect(rightX - DP_EYE_W/2, centerY - DP_EYE_H/2, DP_EYE_W, DP_EYE_H, DP_EYE_R, DP_COL_EYE);
  }
  inline void dpDrawBrow(int yOff) {
    int baseY = centerY - DP_TRI_H/2 + yOff + DP_TRI_SHIFT_UP;
    int apexY = centerY + DP_TRI_H/2 + yOff + DP_TRI_SHIFT_UP;
    int baseL = centerX - DP_TRI_W/2, baseR = centerX + DP_TRI_W/2, apexX = centerX;
    canvas.fillTriangle(baseL, baseY, baseR, baseY, apexX, apexY, DP_COL_TRI);
  }
  inline void dpDrawDots() {
    if (!dpDotsActive || dpDotsPlaced <= 0) return;
    for (int i = 0; i < dpDotsPlaced; ++i) {
      const DPDot &d = dpDots[i];
      canvas.fillCircle(d.x, d.y, d.r, DP_COL_RED);
    }
  }
  inline void dpDrawDivider() {
    int x0 = centerX - DP_DIVIDER_W/2;
    canvas.fillRect(x0, 0, DP_DIVIDER_W, 240, DP_COL_RED);
  }
} // namespace

static void drawDEADPOOL(uint32_t now) {
  // Compute elapsed in this emotion (one IN->HOLD->OUT cycle)
  uint32_t elapsed = now - emotionStartTime;
  if (elapsed > DP_TOTAL) elapsed = DP_TOTAL;

  // Phase and offset
  DPPhase curPhase;
  int yOff = 0;
  if (elapsed < DP_PHASE_IN_MS) {
    curPhase = DP_IN;
    float t = (DP_PHASE_IN_MS == 0) ? 1.f : (elapsed / float(DP_PHASE_IN_MS));
    yOff = int(dpEaseInOut(t) * DP_MOVE_AMPL_PX + 0.5f);
  } else if (elapsed < DP_PHASE_IN_MS + DP_PHASE_HOLD_MS) {
    curPhase = DP_HOLD;
    yOff = DP_MOVE_AMPL_PX;
  } else {
    curPhase = DP_OUT;
    uint32_t e2 = elapsed - (DP_PHASE_IN_MS + DP_PHASE_HOLD_MS);
    float t = (DP_PHASE_OUT_MS == 0) ? 1.f : (e2 / float(DP_PHASE_OUT_MS));
    yOff = int((1.0f - dpEaseInOut(t)) * DP_MOVE_AMPL_PX + 0.5f);
  }

  // Phase transitions: spawn/clear dots
  if (curPhase == DP_IN && dpPrevPhase != DP_IN)  dpClearDots();
  if (curPhase == DP_HOLD && dpPrevPhase != DP_HOLD) dpGenerateDots();
  dpPrevPhase = curPhase;

  // Compose frame (engine should have cleared canvas this frame; if not, uncomment next line)
  // canvas.fillScreen(DP_COL_BG);
  dpDrawRing();         // 1) ring
  dpDrawEyes();         // 2) eyes
  dpDrawBrow(yOff);     // 3) brow triangle
  dpDrawDots();         // 4) dots during HOLD
  dpDrawDivider();      // 5) divider on top
}
// ------------------ CARVE_SESSION (ported from your standalone) ------------------
void drawCARVE_SESSION(unsigned long now) {
  static bool firstFrame = true;   
  constexpr uint32_t CARVE_EASE_IN   = 600;
  constexpr uint32_t CARVE_HOLD      = 2000;
  constexpr uint32_t CARVE_EASE_OUT  = 600;
  constexpr uint32_t CARVE_EMO_TOTAL = CARVE_EASE_IN + CARVE_HOLD + CARVE_EASE_OUT;
  // === Local persistent state for this emotion ===
  struct {
    bool     inited = false;

    // Layout (use your engine’s if you already define these globally)
    int centerX = 120, centerY = 120;
    int eyeRadius = 35;
    int eyeDistance = 50;
    int eyeWidth  = 70;
    int eyeHeight = 70;
    int eyeCorner = 20;

    // Wander & blink (blink only in NORMAL; disabled during EMO)
    float wanderX = 0, wanderY = 0;
    float targetX = 0, targetY = 0;
    unsigned long lastMoveTime = 0;
    int   moveInterval = 1000;
    float easing = 0.10f;

    enum BlinkState { BLINK_IDLE, BLINK_CLOSING, BLINK_CLOSED, BLINK_OPENING };
    BlinkState blinkState = BLINK_IDLE;
    unsigned long blinkStart = 0;
    int           blinkDuration = 150;
    unsigned long nextBlinkTime = 0;
    float         blinkProgress = 0.0f;

    // Phase machine
    enum Phase : uint8_t { PH_NORMAL_IDLE=0, PH_NORMAL_RECENTER, PH_PLAY_EMO, PH_RETURN_TO_NORMAL, PH_DONE };
    Phase phase = PH_NORMAL_IDLE;
    uint32_t phaseStart = 0;
    uint32_t normalIdleTargetMs = 0; // random 3–8 s

    // Emotions inside the session
    enum Mode : uint8_t { MODE_SUSPICIOUS=0, MODE_HAPPY=1, MODE_TOPCUT=2, MODE_WORRY=3 };
    Mode modeThisCycle = MODE_SUSPICIOUS;
    bool suspiciousLeft = true;


    // Session timer (whole string)
    bool     sessionActive = true;
    uint32_t sessionStopAt = 0;

  } static S;

  // --- Tiny helpers (local) ---
  auto easeInOut = [](float t)->float {
    if (t<=0) return 0; if (t>=1) return 1;
    return (t<0.5f) ? (2*t*t) : (-1 + (4 - 2*t)*t);
  };

  auto eyeBox = [](int cx, int cy, int w, int h, int r, uint16_t col){
    canvas.fillRoundRect(cx - w/2, cy - h/2, w, h, r, col);
  };

  auto carveTopDiagonal = [&](int cx, int cy, int riseL, int riseR, uint16_t bg=GC9A01A_BLACK) {
    const int x0  = cx - S.eyeWidth/2;
    const int x1  = cx + S.eyeWidth/2;
    const int y0  = cy - S.eyeHeight/2;
    const int yTop= y0 - 60;
    const int yA  = y0 + riseL;  // LEFT edge height
    const int yB  = y0 + riseR;  // RIGHT edge height
    canvas.fillTriangle(x0, yA, x1, yB, x0, yTop, bg);
    canvas.fillTriangle(x1, yB, x1, yTop, x0, yTop, bg);
  };

  auto carveBottomDiagonal = [&](int cx, int cy, int dropL, int dropR, uint16_t bg=GC9A01A_BLACK) {
    const int x0  = cx - S.eyeWidth/2;
    const int x1  = cx + S.eyeWidth/2;
    const int y1  = cy + S.eyeHeight/2;
    const int yBot= y1 + 60;
    const int yA  = y1 - dropL;  // LEFT edge height
    const int yB  = y1 - dropR;  // RIGHT edge height
    canvas.fillTriangle(x0, yA, x1, yB, x0, yBot, bg);
    canvas.fillTriangle(x1, yB, x1, yBot, x0, yBot, bg);
  };

  auto carveEyeCorner = [&](int cx, int cy, bool isLeftEye, bool topNotBottom, int depthInner, int depthOuter, uint16_t bg=GC9A01A_BLACK) {
    const bool innerIsLeft = !isLeftEye;
    const int leftDepth  = innerIsLeft ? depthInner : depthOuter;
    const int rightDepth = innerIsLeft ? depthOuter : depthInner;
    if (topNotBottom) carveTopDiagonal(cx,  cy, leftDepth,  rightDepth,  bg);
    else              carveBottomDiagonal(cx, cy, leftDepth, rightDepth, bg);
  };

  auto updateWander = [&](uint32_t now){
    if (now - S.lastMoveTime > (uint32_t)S.moveInterval) {
      S.targetX = random(-20, 21);
      S.targetY = random(-20, 21);
      S.lastMoveTime = now;
    }
    S.wanderX += (S.targetX - S.wanderX) * S.easing;
    S.wanderY += (S.targetY - S.wanderY) * S.easing;
  };

  auto updateBlink = [&](uint32_t now){
    using BS = decltype(S)::BlinkState;
    switch (S.blinkState) {
      case BS::BLINK_IDLE:
        if (now > S.nextBlinkTime) { S.blinkState = BS::BLINK_CLOSING; S.blinkStart = now; }
        break;
      case BS::BLINK_CLOSING: {
        float t = float(now - S.blinkStart) / S.blinkDuration;
        S.blinkProgress = (t >= 1.0f) ? 1.0f : t;
        if (S.blinkProgress >= 1.0f) { S.blinkState = BS::BLINK_CLOSED; S.blinkStart = now; }
      } break;
      case BS::BLINK_CLOSED:
        if (now - S.blinkStart > 100) { S.blinkState = BS::BLINK_OPENING; S.blinkStart = now; }
        break;
      case BS::BLINK_OPENING: {
        float t = float(now - S.blinkStart) / S.blinkDuration;
        S.blinkProgress = 1.0f - ((t >= 1.0f) ? 1.0f : t);
        if (S.blinkProgress <= 0.0f) { S.blinkProgress = 0.0f; S.blinkState = BS::BLINK_IDLE; S.nextBlinkTime = now + random(1000, 3000); }
      } break;
    }
  };

  auto drawNormalEyesWithBlink = [&](int cxOverride = INT32_MIN, int cyOverride = INT32_MIN){
    int hOpen = S.eyeHeight;
    int h = (int)(hOpen * (1.0f - 0.75f * S.blinkProgress)); // 0→1 closes ~75%
    if (h < 4) h = 4;

    const int leftX  = (cxOverride==INT32_MIN) ? S.centerX - S.eyeDistance + (int)roundf(S.wanderX) : cxOverride - S.eyeDistance;
    const int rightX = (cxOverride==INT32_MIN) ? S.centerX + S.eyeDistance + (int)roundf(S.wanderX) : cxOverride + S.eyeDistance;
    const int cy     = (cyOverride==INT32_MIN) ? S.centerY + (int)roundf(S.wanderY) : cyOverride;

    eyeBox(leftX,  cy, S.eyeWidth, h, S.eyeCorner, GC9A01A_WHITE);
    eyeBox(rightX, cy, S.eyeWidth, h, S.eyeCorner, GC9A01A_WHITE);
  };

  auto drawBaseEyesNoBlink = [&](){
    const int leftX  = S.centerX - S.eyeDistance + (int)roundf(S.wanderX);
    const int rightX = S.centerX + S.eyeDistance + (int)roundf(S.wanderX);
    const int cy     = S.centerY + (int)roundf(S.wanderY);
    eyeBox(leftX,  cy, S.eyeWidth, S.eyeHeight, S.eyeCorner, GC9A01A_WHITE);
    eyeBox(rightX, cy, S.eyeWidth, S.eyeHeight, S.eyeCorner, GC9A01A_WHITE);
  };

  auto startNormal = [&](uint32_t now){
    S.phase = decltype(S)::PH_NORMAL_IDLE;
    S.phaseStart = now;
    S.normalIdleTargetMs = random(3000, 8001); // 3–8 s
  };

  auto startEmotion = [&](uint32_t now){
    S.phase = decltype(S)::PH_PLAY_EMO;
    S.phaseStart = now;
    S.modeThisCycle = (decltype(S)::Mode)random(0,4); // 0..3
    S.suspiciousLeft = (random(0,2) == 0);
  };

  // --- One-time init when the emotion starts ---
  if (!S.inited) {
    S.inited = true;
    g_carveSessionDone = false;   // reset the global "done" flag
    firstFrame = false;  
    // Seed blink timing and wander
    S.nextBlinkTime = now + random(1000, 3000);
    S.lastMoveTime  = now;
    // Random session length 30–60 s, anchored to emotionStartTime (global)
    extern unsigned long emotionStartTime;
    S.sessionStopAt = emotionStartTime + random(50000, 60001);
    S.sessionActive = true;
    // Start in normal idle
    startNormal(now);
  }

  // --- Frame clear (leave overall engine’s clear as-is; we just draw content) ---
  // Expect the engine to have already cleared the canvas each frame.

  // --- Phase machine (draw) ---
  switch (S.phase) {
    case decltype(S)::PH_NORMAL_IDLE: {
      updateWander(now);
      updateBlink(now);
      drawNormalEyesWithBlink();

      const bool timeUp = S.sessionActive && (now >= S.sessionStopAt);
      if (timeUp) {
        S.phase = decltype(S)::PH_NORMAL_RECENTER;
        break;
      }
      if (now - S.phaseStart >= S.normalIdleTargetMs) {
        S.phase = decltype(S)::PH_NORMAL_RECENTER;
      }
    } break;

    case decltype(S)::PH_NORMAL_RECENTER: {
      S.wanderX *= 0.80f;
      S.wanderY *= 0.80f;
      updateBlink(now);
      drawNormalEyesWithBlink();

      if (fabsf(S.wanderX) < 1.0f && fabsf(S.wanderY) < 1.0f) {
        S.wanderX = S.wanderY = 0.0f;
        if (S.sessionActive && now < S.sessionStopAt) {
          startEmotion(now);
        } else {
          S.phase = decltype(S)::PH_DONE;
          S.phaseStart = now;
          g_carveSessionDone = true; 
          firstFrame = true;    
        }
      }
    } break;

    case decltype(S)::PH_PLAY_EMO: {
      updateWander(now);
      drawBaseEyesNoBlink();

      uint32_t e = now - S.phaseStart;
      if (e > CARVE_EMO_TOTAL) e = CARVE_EMO_TOTAL;
      float t;
      if (e < CARVE_EASE_IN)                          t = easeInOut(float(e) / CARVE_EASE_IN);
      else if (e < CARVE_EASE_IN + CARVE_HOLD)        t = 1.0f;
      else                                            t = 1.0f - easeInOut(float(e - (CARVE_EASE_IN + CARVE_HOLD)) / CARVE_EASE_OUT);

      if (t > 0.f) {
        const int leftX  = S.centerX - S.eyeDistance + (int)roundf(S.wanderX);
        const int rightX = S.centerX + S.eyeDistance + (int)roundf(S.wanderX);
        const int cy     = S.centerY + (int)roundf(S.wanderY);

        if (S.modeThisCycle == decltype(S)::MODE_SUSPICIOUS) {
          const int rise = int(22 * t + 0.5f);
          if (S.suspiciousLeft) {
            carveEyeCorner(leftX,  cy, /*isLeftEye=*/true,  /*top*/true, /*inner*/rise, /*outer*/0);
          } else {
            carveEyeCorner(rightX, cy, /*isLeftEye=*/false, /*top*/true, /*inner*/rise, /*outer*/0);
          }

        } else if (S.modeThisCycle == decltype(S)::MODE_HAPPY) {
          const int dropO = int(28 * t + 0.5f); // outside deeper
          const int dropI = int( 8 * t + 0.5f); // inside shallow
          carveEyeCorner(leftX,  cy, true,  /*bottom*/false, /*inner*/dropI, /*outer*/dropO);
          carveEyeCorner(rightX, cy, false, /*bottom*/false, /*inner*/dropI, /*outer*/dropO);

        } else if (S.modeThisCycle == decltype(S)::MODE_TOPCUT) {
          const int riseO = int(26 * t + 0.5f);
          const int riseI = int( 6 * t + 0.5f);
          carveEyeCorner(leftX,  cy, true,  /*top*/true, /*inner*/riseI, /*outer*/riseO);
          carveEyeCorner(rightX, cy, false, /*top*/true, /*inner*/riseI, /*outer*/riseO);

        } else { // MODE_WORRY (single eye, top outer chamfer)
          const int riseO = int(26 * t + 0.5f);
          const int riseI = int( 6 * t + 0.5f);
          if (S.suspiciousLeft) {
            carveEyeCorner(leftX,  cy, true,  /*top*/true,  /*inner*/riseI, /*outer*/riseO);
          } else {
            carveEyeCorner(rightX, cy, false, /*top*/true,  /*inner*/riseI, /*outer*/riseO);
          }
        }
      }

      if (now - S.phaseStart >= CARVE_EMO_TOTAL) {
        S.phase = decltype(S)::PH_RETURN_TO_NORMAL;
        S.phaseStart = now;
      }
    } break;

    case decltype(S)::PH_RETURN_TO_NORMAL: {
      updateWander(now);
      updateBlink(now);
      drawNormalEyesWithBlink();
      S.phase = decltype(S)::PH_NORMAL_IDLE;
      S.phaseStart = now;
    } break;

    case decltype(S)::PH_DONE: {
      S.wanderX = 0.0f;
      S.wanderY = 0.0f;
      updateBlink(now);
      drawNormalEyesWithBlink(S.centerX, S.centerY);
      // stay here; engine will time out via emotionDuration[] ceiling
    } break;
  }
}
// ------------------ /CARVE_SESSION ------------------

//  -----------------SLEEPY----------------
static inline uint16_t _ee_rgb565(uint8_t r, uint8_t g, uint8_t b){
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static inline uint16_t _ee_gray565(uint8_t v){ return _ee_rgb565(v, v, v); }

void drawSLEEPY(uint32_t now) {
  // Reset local state whenever SLEEPY starts
  static uint32_t lastStart = 0;
  const bool freshStart = (lastStart != emotionStartTime);
  if (freshStart) lastStart = emotionStartTime;

  // ---- smooth vertical wobble (buttery, no jitter) ----
  static float  eyeY    = 0.0f;
  static float  phase   = 0.0f;
  static uint32_t lastT = 0;

  const float wobbleAmp  = 0.085f * eyeWidth;  // ~6 px if eyeWidth=70
  const float wobbleFreq = 0.48f;              // Hz (~2.1s cycle)
  const float wobbleEase = 0.08f;              // 0..1

  if (freshStart) {
    eyeY  = (float)centerY;
    phase = 0.0f;
    lastT = now;
  }

  float dt = (lastT == 0) ? 0.016f : (now - lastT) * 0.001f;
  lastT = now;

  const float omega  = 2.0f * PI * wobbleFreq;
  const float target = (float)centerY + wobbleAmp * sinf(phase);
  phase += omega * dt;
  eyeY += (target - eyeY) * wobbleEase;
  const int cy = (int)roundf(eyeY);

  // ---- tiny "Z" particle pool ----
  struct ZZ {
    float    x, y, vy;
    uint16_t color;
    uint8_t  size;
    uint16_t birth, life;
    bool     alive;
  };
  static ZZ pool[3];
  static uint32_t lastEmit = 0;
  const  uint16_t EMIT_MS  = 350;

  if (freshStart) {
    for (auto &p : pool) p.alive = false;
    lastEmit = now;
  }

  if ((now - lastEmit) >= EMIT_MS) {
    lastEmit = now;
    for (auto &p : pool) if (!p.alive) {
      p.x    = (float)(centerX + random(-6, 7));
      p.y    = (float)(cy - (eyeHeight/2 + 5));
      p.vy   = -0.35f - 0.10f * random(0, 25);
      p.size = 1 + (uint8_t)random(0, 3);        // 1..3
      p.birth= (uint16_t)(now & 0xFFFF);
      p.life = 1400 + (uint16_t)random(0, 600);  // 1.4–2.0 s
      p.color= _ee_gray565(220);
      p.alive= true;
      break;
    }
  }

  // ---- closed lids using your existing helper (handles spacing) ----
  // progress: 0=open … 1=fully closed
  drawBlinkingEyes((float)centerX, (float)cy, 1.0f);

  // ---- draw the Zs ----
  canvas.setTextWrap(false);
  for (auto &p : pool) {
    if (!p.alive) continue;

    uint16_t age = (uint16_t)(now & 0xFFFF) - p.birth; // wrap-safe
    if (age >= p.life) { p.alive = false; continue; }

    p.y += p.vy;

    uint8_t v = (uint8_t)(220 - (220 * age) / p.life);
    p.color = _ee_gray565(v);

    int idx = (int)(&p - &pool[0]);
    int x = (int)(p.x + sinf(0.0025f * (float)now + 0.6f * idx) * 2.0f);
    int y = (int)(p.y);

    canvas.setTextSize(p.size);
    canvas.setTextColor(p.color);
    canvas.setCursor(x, y);
    canvas.print('Z');
  }
}

// ===== CRY emotion implementation =====

// --- Colors (match your palette) ---
static const uint16_t CRY_COL_BG     = GC9A01A_BLACK;
static const uint16_t CRY_COL_EYE    = GC9A01A_WHITE;
static const uint16_t CRY_COL_TEAR_L = 0xC618;  // light gray
static const uint16_t CRY_COL_TEAR_R = 0xD67A;  // light gray alt

// --- Timing (ms) — keep in sync with emotionDuration[CRY] ---
static const uint32_t CRY_PHASE_INTRO = 400;
static const uint32_t CRY_PHASE_IN    = 1000;
static const uint32_t CRY_PHASE_HOLD  = 5000;
static const uint32_t CRY_PHASE_OUT   = 1000;

// --- Wobble params ---
static const float CRY_WOBBLE_A_MAX = 10.0f;  // px
static const float CRY_WOBBLE_F     = 1.6f;   // Hz

// --- Small helpers (prefixed to avoid conflicts) ---
static inline float CRY_clamp01(float x){ return (x<0)?0:((x>1)?1:x); }
static inline float CRY_easeInOut(float t){
  t = CRY_clamp01(t);
  return (t < 0.5f) ? (2.0f*t*t) : (-1.0f + (4.0f - 2.0f*t)*t);
}
static inline float CRY_lerpf(float a, float b, float t){ return a + (b - a)*t; }

static inline void CRY_eyeBox(int cx, int cy, int w, int h, int r, uint16_t col){
  canvas.fillRoundRect(cx - w/2, cy - h/2, w, h, r, col);
}

// Tears (left eye = 4 drops)
static void CRY_drawTearsLeftEye(int cx, int cy, float p){
  const int baseY = cy + eyeHeight/2 - 6;
  const uint16_t c = CRY_COL_TEAR_L;
  int x1 = cx - 20;  int w1 = 10; int h1 = 34;
  int x2 = cx - 11;  int w2 =  8; int h2 = 26;
  int x3 = cx -  5;  int w3 = 12; int h3 = 44;
  int x4 = cx +  6;  int w4 =  8; int h4 = 24;
  canvas.fillRoundRect(x1, baseY, w1, (int)(h1*p + 0.5f), 5, c);
  canvas.fillRoundRect(x2, baseY, w2, (int)(h2*p + 0.5f), 4, c);
  canvas.fillRoundRect(x3, baseY, w3, (int)(h3*p + 0.5f), 6, c);
  canvas.fillRoundRect(x4, baseY, w4, (int)(h4*p + 0.5f), 4, c);
}

// Tears (right eye = 3 drops)
static void CRY_drawTearsRightEye(int cx, int cy, float p){
  const int baseY = cy + eyeHeight/2 - 6;
  const uint16_t c = CRY_COL_TEAR_R;
  int x1 = cx - 10;  int w1 = 20; int h1 = 30;
  int x2 = cx -  2;  int w2 = 12; int h2 = 40;
  int x3 = cx + 10;  int w3 = 10; int h3 = 22;
  canvas.fillRoundRect(x1, baseY, w1, (int)(h1*p + 0.5f), 5, c);
  canvas.fillRoundRect(x2, baseY, w2, (int)(h2*p + 0.5f), 6, c);
  canvas.fillRoundRect(x3, baseY, w3, (int)(h3*p + 0.5f), 4, c);
}

static void CRY_drawFrame(float pTears, float wobbleA, float tSec){


  // Eyes & tears wobble together
  const float phase = TWO_PI * CRY_WOBBLE_F * tSec;
  const int wobX = (int)lroundf(wobbleA * sinf(phase));
  const int wobY = (int)lroundf((wobbleA * 0.6f) * sinf(phase + 1.3f));

  const int leftX  = centerX - eyeDistance + wobX;
  const int rightX = centerX + eyeDistance + wobX;
  const int cy     = centerY + wobY;

  // 1) tears behind
  if (pTears > 0.0f){
    CRY_drawTearsLeftEye(leftX,  cy, pTears);
    CRY_drawTearsRightEye(rightX, cy, pTears);
  }

  // 2) eyes on top
  CRY_eyeBox(leftX,  cy, eyeWidth, eyeHeight, eyeCorner, CRY_COL_EYE);
  CRY_eyeBox(rightX, cy, eyeWidth, eyeHeight, eyeCorner, CRY_COL_EYE);
}

// Main draw: obeys engine timing via emotionStartTime / now
static void drawCRY(unsigned long now){
  // Compute elapsed within this emotion
  const unsigned long start = emotionStartTime;  // from your engine
  unsigned long e = now - start;

  // Clamp to total duration for safety
  const uint32_t TOTAL_MS = CRY_PHASE_INTRO + CRY_PHASE_IN + CRY_PHASE_HOLD + CRY_PHASE_OUT;
  if (e > TOTAL_MS) e = TOTAL_MS;

  float pTears = 0.0f;   // 0..1
  float wobA   = 0.0f;   // px

  if (e <= CRY_PHASE_INTRO){
    pTears = 0.0f; wobA = 0.0f;
  } else if (e <= CRY_PHASE_INTRO + CRY_PHASE_IN){
    const float t = float(e - CRY_PHASE_INTRO) / float(CRY_PHASE_IN);
    const float k = CRY_easeInOut(t);
    pTears = k;
    wobA   = CRY_lerpf(0.0f, CRY_WOBBLE_A_MAX, k);
  } else if (e <= CRY_PHASE_INTRO + CRY_PHASE_IN + CRY_PHASE_HOLD){
    pTears = 1.0f;
    wobA   = CRY_WOBBLE_A_MAX;
  } else {
    const uint32_t base = CRY_PHASE_INTRO + CRY_PHASE_IN + CRY_PHASE_HOLD;
    const float t = float(e - base) / float(CRY_PHASE_OUT);
    const float k = CRY_easeInOut(t);
    pTears = 1.0f - k;
    wobA   = CRY_lerpf(CRY_WOBBLE_A_MAX, 0.0f, k);
  }

  const float tSec = float(e) / 1000.0f;
  CRY_drawFrame(pTears, wobA, tSec);

  // Blit to screen (same as your other emotions)
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 240);
}

// ===================================
// Fireworks system
// ===================================
namespace FW {
  static uint32_t fw_seed = 0xA5F0361Du;
  static inline uint32_t fw_xrng(uint32_t &s) {
    uint32_t x = s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s = x;
    return x;
  }
  static inline uint16_t fw_hue2rgb565(uint8_t hue) {
    uint8_t seg = hue >> 5;
    uint8_t off = (hue & 31) << 3;
    uint8_t r=0,g=0,b=0;
    switch(seg){
      case 0: r=255; g=off;  b=0;    break;
      case 1: r=255-off; g=255; b=0; break;
      case 2: r=0; g=255; b=off;     break;
      case 3: r=0; g=255-off; b=255; break;
      case 4: r=off; g=0; b=255;     break;
      default:r=255; g=0; b=255-off; break;
    }
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
  static inline uint16_t fw_dim565(uint16_t c, uint8_t factor){
    uint8_t r5 = (c >> 11) & 0x1F;
    uint8_t g6 = (c >> 5)  & 0x3F;
    uint8_t b5 =  c        & 0x1F;
    uint8_t r = (r5 << 3) | (r5 >> 2);
    uint8_t g = (g6 << 2) | (g6 >> 4);
    uint8_t b = (b5 << 3) | (b5 >> 2);
    r = (uint8_t)(((uint16_t)r * factor) >> 8);
    g = (uint8_t)(((uint16_t)g * factor) >> 8);
    b = (uint8_t)(((uint16_t)b * factor) >> 8);
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
  struct Particle {
    float  x, y;
    float  vx, vy;
    uint16_t color;
    uint8_t life;
    bool   alive;
  };
  struct Burst {
    bool   active;
    float  cx, cy;
    uint16_t baseColor;
    uint8_t  count;
    uint8_t  life;
    uint8_t  sparkle;
  };
  static const uint8_t  MAX_BURSTS    = 8;
  static const uint8_t  P_PER_BURST   = 25;
  static const float    SPEED_MIN     = 1.4f;
  static const float    SPEED_MAX     = 3.2f;
  static const float    GRAVITY       = 0.2f;
  static const uint8_t  GLOBAL_FADE   = 230;
  static const uint8_t  BURST_FADE    = 2;
  static const uint8_t  PARTICLE_FADE = 2;
  Burst     bursts[MAX_BURSTS];
  Particle  particles[MAX_BURSTS][P_PER_BURST];
  
  static void fadeCanvas(uint8_t factor){
    uint16_t *buf = (uint16_t*)canvas.getBuffer();
    const int N = 240 * 240;
    for(int i=0;i<N;i++){
      buf[i] = fw_dim565(buf[i], factor);
    }
  }
  static void spawnBurst(){
    uint8_t idx = 255;
    for(uint8_t i=0;i<MAX_BURSTS;i++){
      if(!bursts[i].active){ idx = i; break; }
    }
    if(idx == 255) return;
    Burst &b = bursts[idx];
    b.active    = true;
    b.cx        = 20 + (fw_xrng(fw_seed) % 200);
    b.cy        = 30 + (fw_xrng(fw_seed) % 160);
    b.baseColor = fw_hue2rgb565((uint8_t)(fw_xrng(fw_seed) % 192));
    b.count     = P_PER_BURST;
    b.life      = 255;
    b.sparkle   = 32 + (fw_xrng(fw_seed) % 64);
    for(uint8_t p=0;p<P_PER_BURST;p++){
      float angle = (2.0f * PI * p) / P_PER_BURST;
      float spd   = SPEED_MIN + ((fw_xrng(fw_seed) % 1000) / 1000.0f) * (SPEED_MAX - SPEED_MIN);
      Particle &pt = particles[idx][p];
      pt.x = b.cx;
      pt.y = b.cy;
      pt.vx = cosf(angle) * spd;
      pt.vy = sinf(angle) * spd;
      pt.color = b.baseColor;
      pt.life  = 230;
      pt.alive = true;
    }
  }
  static void stepAndDraw(){
    fadeCanvas(GLOBAL_FADE);
    for(uint8_t i=0;i<MAX_BURSTS;i++){
      if(!bursts[i].active) continue;
      Burst &b = bursts[i];
      bool anyAlive = false;
      for(uint8_t p=0;p<b.count;p++){
        Particle &pt = particles[i][p];
        if(!pt.alive) continue;
        pt.vy += GRAVITY * 0.20f;
        pt.x  += pt.vx;
        pt.y  += pt.vy;
        if(pt.life > PARTICLE_FADE) pt.life -= PARTICLE_FADE; else pt.life = 0;
        if(pt.life == 0) { pt.alive = false; continue; }
        anyAlive = true;
        uint16_t col = pt.color;
        if((fw_xrng(fw_seed) & 255) < b.sparkle){
          uint16_t r = ((col >> 11) & 0x1F);
          uint16_t g = ((col >> 5)  & 0x3F);
          uint16_t bl= ( col        & 0x1F);
          r = min<uint16_t>(31, r + 8);
          g = min<uint16_t>(63, g + 8);
          bl= min<uint16_t>(31, bl+ 8);
          col = (r << 11) | (g << 5) | bl;
        }
        col = fw_dim565(col, pt.life);
        int16_t xi = (int16_t)pt.x;
        int16_t yi = (int16_t)pt.y;
        if((uint16_t)xi < 240 && (uint16_t)yi < 240){
          canvas.drawPixel(xi, yi, col);
          if(xi+1 < 240) canvas.drawPixel(xi+1, yi, col);
          if(yi+1 < 240) canvas.drawPixel(xi, yi+1, col);
          if(xi+1 < 240 && yi+1 < 240) canvas.drawPixel(xi+1, yi+1, col);
        }
      }
      if(b.life > BURST_FADE) b.life -= BURST_FADE; else b.life = 0;
      if(!anyAlive || b.life == 0){
        b.active = false;
      }
    }
    if((fw_xrng(fw_seed) & 255) < 28){
      spawnBurst();
    }
  }
  void reset(){

    for(uint8_t i=0;i<MAX_BURSTS;i++){
      bursts[i].active = false;
      bursts[i].count  = P_PER_BURST;
      bursts[i].life   = 0;
      bursts[i].sparkle= 0;
    }
    fw_seed ^= (uint32_t)((uintptr_t)&canvas);
    fw_seed ^= millis() * 747796405u;
  }
} // namespace FW

// Public draw entry for FIREWORKS
static void drawFIREWORKS(unsigned long now){
  FW::stepAndDraw();
}

// ===================================
// End of Fireworks system