#pragma once
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// Pins (same as your master)
#define TFT_CS   2
#define TFT_DC   5
#define TFT_RST  -1

// Public hardware objects (defined in emotion_engine.cpp)
extern Adafruit_GC9A01A tft;
extern GFXcanvas16 canvas;
extern unsigned long emotionStartTime;
extern bool gPreserveBackground;

// ===== Emotion Cycle =====
enum EmotionType { NORMAL, SAD, CONFUSE, LOVE, CYCLOP, SHOCK, DRUNK, FURIOUS, ANGRY, DOUBT, ANGRY2, SMILE, BANH_CHUNG, DEADPOOL, FORTUNE_TELLER, CARVE_SESSION, SLEEPY, CRY, FIREWORKS, BOOT_INTRO, EMOTION_COUNT };
extern EmotionType currentEmotion;
// === Color change (c1 to c2)===
uint16_t HE_colorLerp(uint16_t c1, uint16_t c2, float t);
// ===== New Cycle State Machine =====
enum class CycleState : uint8_t { BOOTING, NORMAL_IDLE, NORMAL_RECENTER, PLAY_EMOTION, RETURN_TO_NORMAL };

// Expose current state so you can debug if needed
extern CycleState cycleState;
// Restart the normal-emotion (NE) cycle from the beginning
void bubuEngineRestartCycle();
// Optional: helper to start a random emotion from NORMAL
void triggerRandomEmotion(uint32_t now);

// Optional: set per-emotion weights (0..255). Index 0 (NORMAL) is ignored.
void setEmotionWeights(const uint8_t weights[10]);
// from ANGRY
void triggerAngry(uint32_t now);
bool handleAngry(uint32_t now);

// Public entry points that mirror your original setup()/loop()
void bubuEngineSetup();
void bubuEngineLoop();


// (Optional) convenience trigger
void EE_triggerCARVE_SESSION();

// --- Cry (tears + wobble) ---
void EE_Cry_begin(uint32_t now);
void EE_Cry_step(uint32_t now);     // call every frame while Cry is active
bool EE_Cry_isDone(uint32_t now);   // true when sequence finished
