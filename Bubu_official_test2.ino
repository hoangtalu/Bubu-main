#include "emotion_engine.h"
#include "motion_engine.h"

extern void setCycleStateBooting();
void setup() {
  bubuEngineSetup();                 // init TFT/canvas + emotion engine
  MotionEngine::begin(7, 6, 0x00);  
  cycleState = CycleState::BOOTING; 

  cycleState       = CycleState::PLAY_EMOTION;  // not BOOTING (no handler for that)
  currentEmotion   = BOOT_INTRO;                // plain enum, not Emotion::BOOT_INTRO
  emotionStartTime = millis();
  MotionEngine::setInvertYaw(false);         // set true if L/R feels reversed
  MotionEngine::setTurnThresholdDps(10.0f);  // optional tuning
  MotionEngine::setAccelThresholds(4.0f, -4.0f);
  MotionEngine::setDebug(true);
}
void loop() {
  // If a motion animation just played, skip one engine cycle (it will redraw idle next loop)
  bool played = MotionEngine::update();
  if (!played) {
    bubuEngineLoop();                // idle/random emotions + recentering
  }
}