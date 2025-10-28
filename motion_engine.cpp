#include "motion_engine.h"
#include "bubu_emotions.h"
#include "emotion_engine.h"
#include <Wire.h>
#include <math.h>

namespace MotionEngine {

// ================== CONFIG / TUNING ==================
static float ACCEL_SPEED = 4.0f;        // m/s^2 to trigger SpeedUp
static float ACCEL_BRAKE = -4.0f;       // m/s^2 to trigger Brakes
static float TURN_DPS    = 10.0f;        // deg/s to trigger Left/Right
static bool  INVERT_YAW  = false;       // flip sign if turns feel reversed
static unsigned long COOLDOWN_MS   = 3000;
static unsigned long IDLE_DWELL_MS = 500;
static bool DEBUG_PRINT = false;

// ================== INTERNAL STATE ===================
static bool     inited       = false;
static uint8_t  detectedAddr = 0x00;    // 0x68 or 0x69
static uint8_t  detectedWHO  = 0xFF;    // expect 0x70 for MPU6500
static bool     playingMTE   = false;
static unsigned long lastMTEEndedMs = 0;
static bool     readyForNext = false;
static unsigned long idleSeenSince = 0;

enum class RawCarState : uint8_t { IDLE_CAR=0, ACCELERATING, BRAKING, TURNING_LEFT, TURNING_RIGHT };
static RawCarState lastDetected = RawCarState::IDLE_CAR;

// ===== Fixed mount: X up, Y forward =====
// Mapping:
//  - Forward/Brake uses accel.Y (m/s^2)
//  - Yaw uses gyro.X (deg/s)
// Gravity is along -X when upright; we don’t need to “learn UP”.

// ================== I2C LOW-LEVEL ====================
static bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 1) != 1) return false;
  val = Wire.read();
  return true;
}
static bool readWHO(uint8_t addr, uint8_t &who) { return i2cRead8(addr, 0x75, who); }

// ===== Manual init/read for MPU6500 only (WHO=0x70) ===
static bool initMPU6500(uint8_t addr) {
  // Reset then wake w/ PLL
  i2cWrite8(addr, 0x6B, 0x80); delay(80);
  if (!i2cWrite8(addr, 0x6B, 0x01)) return false; // PWR_MGMT_1

  // DLPF & sample rate
  if (!i2cWrite8(addr, 0x1A, 0x04)) return false; // CONFIG DLPF ~21–41 Hz
  if (!i2cWrite8(addr, 0x19, 0x09)) return false; // SMPLRT_DIV (100 Hz)

  // Gyro ±500 dps, Accel ±8g
  if (!i2cWrite8(addr, 0x1B, 0x08)) return false; // GYRO_CONFIG
  if (!i2cWrite8(addr, 0x1C, 0x10)) return false; // ACCEL_CONFIG

  // Accel DLPF helpful on 6500
  i2cWrite8(addr, 0x1D, 0x03);                     // ACCEL_CONFIG2

  // Confirm WHO again
  uint8_t who=0xFF;
  if (!readWHO(addr, who)) return false;
  return (who == 0x70);
}

// One-shot read → accel (m/s²), gyro (deg/s), temp (°C)
static bool readMPU6500(uint8_t addr,
  float &ax, float &ay, float &az, float &gx_dps, float &gy_dps, float &gz_dps, float &tempC)
{
  uint8_t b[14];
  Wire.beginTransmission(addr);
  Wire.write(0x3B); // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 14) != 14) return false;
  for (int i=0;i<14;i++) b[i] = Wire.read();

  auto rd16 = [&](int i)->int16_t { return (int16_t)((b[i] << 8) | b[i+1]); };
  int16_t axr = rd16(0), ayr = rd16(2), azr = rd16(4);
  int16_t tr  = rd16(6);
  int16_t gxr = rd16(8), gyr = rd16(10), gzr = rd16(12);

  constexpr float G = 9.80665f;
  ax = (axr / 4096.0f) * G;   // ±8g
  ay = (ayr / 4096.0f) * G;
  az = (azr / 4096.0f) * G;

  gx_dps = (gxr / 65.5f);     // ±500 dps
  gy_dps = (gyr / 65.5f);
  gz_dps = (gzr / 65.5f);

  tempC = (tr / 340.0f) + 36.53f;
  return true;
}

// ================== LIFECYCLE ========================
void begin(uint8_t sda, uint8_t scl, uint8_t mpu_addr) {
  Wire.begin(sda, scl);
  delay(10);

  // Probe order: user-specified or default 0x68→0x69
  uint8_t order[2] = {0x68, 0x69};
  if (mpu_addr == 0x69) { order[0] = 0x69; order[1] = 0x68; }
  else if (mpu_addr == 0x68) { order[0] = 0x68; order[1] = 0x69; }

  // Find WHO=0x70 and init
  bool ok = false;
  for (int i=0;i<2;i++) {
    uint8_t addr = order[i], who = 0xFF;
    if (readWHO(addr, who) && who == 0x70) {
      if (initMPU6500(addr)) {
        detectedAddr = addr;
        detectedWHO  = who;
        ok = true;
        break;
      }
    }
  }
  if (!ok) {
    // Hard fail: only 6500 is supported in this build
    inited = false;
    if (DEBUG_PRINT) Serial.println("MotionEngine: MPU6500 not found.");
    return;
  }

  // Ready
  inited = true;
  playingMTE    = false;
  readyForNext  = true;
  idleSeenSince = 0;
  lastMTEEndedMs = 0;
  lastDetected  = RawCarState::IDLE_CAR;

  if (DEBUG_PRINT) {
    Serial.printf("MotionEngine: MPU6500 @0x%02X WHO=0x%02X (X up, Y fwd)\n",
                  detectedAddr, detectedWHO);
  }
}

// ================== MAPPING & STATE ==================
static inline RawCarState detectCarState(float fwd_mps2, float yaw_dps) {
  const bool accelerating  = (fwd_mps2 > ACCEL_SPEED);
  const bool braking       = (fwd_mps2 < ACCEL_BRAKE);
  const bool turningLeft   = (yaw_dps < -TURN_DPS);
  const bool turningRight  = (yaw_dps > TURN_DPS);

  if (turningLeft)  return RawCarState::TURNING_LEFT;
  if (turningRight) return RawCarState::TURNING_RIGHT;
  if (accelerating) return RawCarState::ACCELERATING;
  if (braking)      return RawCarState::BRAKING;
  return RawCarState::IDLE_CAR;
}

static inline void onMTEEnd(unsigned long now) {
  playingMTE = false;
  lastMTEEndedMs = now;
  // disarm / will rearm when idle dwell satisfied
  readyForNext = false;
  idleSeenSince = 0;
}

bool update() {
  if (!inited) return false;

  // Read one sample
  float ax, ay, az, gx_dps, gy_dps, gz_dps, tc;
  if (!readMPU6500(detectedAddr, ax, ay, az, gx_dps, gy_dps, gz_dps, tc)) return false;

  // Fixed mapping (X up, Y forward):
  float fwd = ay;                                        // m/s^2
  float yaw = INVERT_YAW ? -gx_dps : gx_dps;             // deg/s

  RawCarState current = detectCarState(fwd, yaw);

  // Optional debug
  if (DEBUG_PRINT) {
    static uint32_t dbgLast=0;
    if (millis()-dbgLast > 200) {
      Serial.printf("AY=%.2f m/s2  GX=%.1f dps  fwd=%.2f  yaw=%.1f  state=%d\n",
                    ay, gx_dps, fwd, yaw, (int)current);
      dbgLast = millis();
    }
  }

  // ==== Playback gating (unchanged logic) ====
  unsigned long now = millis();
  bool justPlayed = false;

  if (readyForNext && !playingMTE &&
      (now - lastMTEEndedMs) >= COOLDOWN_MS &&
      current != RawCarState::IDLE_CAR) {

    playingMTE = true;

    switch (current) {
      case RawCarState::TURNING_LEFT:  BubuEmotions::playTurnLeft(tft);  break;
      case RawCarState::TURNING_RIGHT: BubuEmotions::playTurnRight(tft); break;
      case RawCarState::ACCELERATING:  BubuEmotions::playSpeedUp(tft);   break;
      case RawCarState::BRAKING:       BubuEmotions::playBrakes(tft);    break;
      default: break;
    }

    onMTEEnd(millis());
    lastDetected = current;
    bubuEngineRestartCycle();
    justPlayed = true;

  } else {
    lastDetected = current;
    // Rearm logic: once idle long enough, rearm
    if (current == RawCarState::IDLE_CAR) {
      if (idleSeenSince == 0) idleSeenSince = now;
      if (!readyForNext && (now - idleSeenSince) >= IDLE_DWELL_MS) {
        readyForNext = true;
      }
    } else {
      idleSeenSince = 0; // moving again
    }
  }

  return justPlayed;
}

// ================== PUBLIC TUNING API =================
void setAccelThresholds(float speedUp_mps2, float brake_mps2_neg) {
  ACCEL_SPEED = speedUp_mps2;
  ACCEL_BRAKE = brake_mps2_neg;
}
void setTurnThresholdDps(float dps) { TURN_DPS = dps; }
void setCooldownMs(unsigned long ms) { COOLDOWN_MS = ms; }
void setIdleDwellMs(unsigned long ms) { IDLE_DWELL_MS = ms; }
void setInvertYaw(bool invert) { INVERT_YAW = invert; }
void setDebug(bool enabled) { DEBUG_PRINT = enabled; }

// ================== INTROSPECTION =====================
MotionEngine::CarState currentState() {
  switch (lastDetected) {
    case RawCarState::ACCELERATING:  return CarState::ACCELERATING;
    case RawCarState::BRAKING:       return CarState::BRAKING;
    case RawCarState::TURNING_LEFT:  return CarState::TURNING_LEFT;
    case RawCarState::TURNING_RIGHT: return CarState::TURNING_RIGHT;
    case RawCarState::IDLE_CAR:
    default:                         return CarState::IDLE_CAR;
  }
}
bool isPlayingMTE() { return playingMTE; }
uint8_t getDetectedI2CAddr() { return detectedAddr; }

} // namespace MotionEngine