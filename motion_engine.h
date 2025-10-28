#pragma once
#include <Arduino.h>

namespace MotionEngine {

// Car states (exposed for optional debugging)
enum class CarState : uint8_t { IDLE_CAR = 0, ACCELERATING, BRAKING, TURNING_LEFT, TURNING_RIGHT };

// ---- Lifecycle ----
void begin(uint8_t sda = 7, uint8_t scl = 6, uint8_t mpu_addr = 0x68);
bool update();   // call once per frame; returns true iff an MTE just played

// ---- Tuning (optional) ----
void setAccelThresholds(float speedUp_mps2, float brake_mps2_neg);
void setTurnThresholdDps(float dps);
void setCooldownMs(unsigned long ms);
void setIdleDwellMs(unsigned long ms);
void setInvertYaw(bool invert);
void setDebug(bool enabled);

// ---- Introspection (optional) ----
CarState currentState();
bool isPlayingMTE();
uint8_t getDetectedI2CAddr();

} // namespace MotionEngine