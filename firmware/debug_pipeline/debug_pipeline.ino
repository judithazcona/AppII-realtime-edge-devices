// debug_pipeline.ino — TEMPORARY diagnostic copy of the classifier pipeline with USB logging.
//
// Why a separate sketch instead of Serial in firmware/classifier: AGENTS.md rule 7 keeps the
// demo sketch free of any USB-serial token (rule 11: in the demo the cable is power only), and
// an #ifdef that can be left on is weaker than a sketch that cannot talk. This file is the
// third such development-only sketch, after bench_test and data_recorder, and it is deleted
// when the pipeline is proven. firmware/classifier.ino is NOT modified by it.
//
// It runs the SAME code path as the demo - same config.h, same features.h, same model_infer.h,
// same window/slide logic, same two-vote smoothing, same BLE - and prints every stage so the
// first broken stage is visible:
//
//   [sensor]   WHO_AM_I, samples/s measured from micros(), read failures
//   [window]   buffer fill, samples since last evaluation
//   [feature]  the raw and standardised features that matter (mag_std, jerk, bb_energy, bb_ratio)
//   [model]    all four probabilities and the argmax
//   [state]    pending vote, committed state, and the string handed to BLE
//   [ble]      subscribed flag, notify() return value
//
// The board is otherwise untouched: no values are invented, no state is cycled, nothing is
// hardcoded. If the model says not_attached while the board lies still, that is the correct
// answer and the log shows the numbers behind it.

#include <Wire.h>

#include <Adafruit_NeoPixel.h>
#include <NimBLEDevice.h>

#include "../config.h"
#include "../features.h"
#include "../model_infer.h"

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t PERIOD_US = 1000000UL / MPU_SAMPLE_RATE_HZ;
static const uint32_t HEARTBEAT_MS = 1000;

static float g_win[WINDOW_SAMPLES * IMU_SIG_COLS];
static int g_fill = 0;
static int g_sinceEval = 0;
static uint32_t nextUs = 0;
static uint32_t epochUs = 0;
static uint32_t nSamples = 0;
static uint32_t nReadFails = 0;
static uint32_t nWindows = 0;
static int g_pending = -1;
static int g_state = -1;
static bool g_sensorDead = false;

static Adafruit_NeoPixel led(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
static NimBLECharacteristic *g_stateChr = nullptr;
static bool g_subscribed = false;
static uint32_t lastBeatMs = 0;
static uint32_t nNotifies = 0;
static uint32_t lastNotifyOk = 0;

static void regWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_I2C_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

static uint8_t regRead(uint8_t reg) {
  Wire.beginTransmission(MPU_I2C_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(true) != 0) return 0xEE;
  if (Wire.requestFrom((int)MPU_I2C_ADDRESS, 1) != 1) return 0xEE;
  return Wire.read();
}

static void configureSensor() {
  regWrite(MPU_REG_PWR_MGMT_1, 0x80);
  delay(100);
  regWrite(MPU_REG_PWR_MGMT_1, 0x01);
  regWrite(MPU_REG_PWR_MGMT_2, 0x00);
  regWrite(MPU_REG_GYRO_CONFIG, MPU_GYRO_CONFIG_VAL_500DPS);
  regWrite(MPU_REG_ACCEL_CONFIG, MPU_ACCEL_CONFIG_VAL_8G);
  regWrite(MPU_REG_CONFIG, MPU_CONFIG_VAL_DLPF_44HZ);
  regWrite(MPU_REG_SMPLRT_DIV, MPU_SMPLRT_DIV_VAL);
  delay(50);
}

static bool readSample(float out[IMU_SIG_COLS]) {
  Wire.beginTransmission(MPU_I2C_ADDRESS);
  Wire.write(MPU_REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(true) != 0) return false;
  if (Wire.requestFrom((int)MPU_I2C_ADDRESS, 14) != 14) return false;
  uint8_t b[14];
  for (int i = 0; i < 14; ++i) b[i] = (uint8_t)Wire.read();
  if (Wire.available()) return false;
  const int16_t ax = (int16_t)((b[0] << 8) | b[1]);
  const int16_t ay = (int16_t)((b[2] << 8) | b[3]);
  const int16_t az = (int16_t)((b[4] << 8) | b[5]);
  const int16_t gx = (int16_t)((b[8] << 8) | b[9]);
  const int16_t gy = (int16_t)((b[10] << 8) | b[11]);
  const int16_t gz = (int16_t)((b[12] << 8) | b[13]);
  out[0] = ax / MPU_ACCEL_LSB_PER_G;
  out[1] = ay / MPU_ACCEL_LSB_PER_G;
  out[2] = az / MPU_ACCEL_LSB_PER_G;
  out[3] = gx / MPU_GYRO_LSB_PER_DPS;
  out[4] = gy / MPU_GYRO_LSB_PER_DPS;
  out[5] = gz / MPU_GYRO_LSB_PER_DPS;
  return true;
}

static uint32_t colourFor(int cls) {
  if (cls < 0 || cls >= MODEL_N_CLASSES) return 0x000000;
  const char *n = MODEL_CLASS_NAMES[cls];
  if (strcmp(n, "not_attached") == 0) return 0x0000FF;
  if (strcmp(n, "shaking") == 0) return 0xFF00FF;
  if (strcmp(n, "calm_breathing") == 0) return 0x00FF00;
  if (strcmp(n, "fast_breathing") == 0) return 0xFF0000;
  return 0xFFFFFF;
}

static void show(uint32_t c) {
  for (int i = 0; i < RGB_LED_COUNT; ++i) led.setPixelColor(i, c);
  led.show();
}

static const char *stateText() {
  if (g_state < 0 || g_state >= MODEL_N_CLASSES) return "";
  return MODEL_CLASS_NAMES[g_state];
}

static void publishState(const char *why) {
  if (g_stateChr == nullptr) return;
  const char *text = stateText();
  const size_t len = strlen(text);
  if (len == 0) return;
  g_stateChr->setValue((uint8_t *)text, len);
  const bool ok = g_stateChr->notify();
  if (ok) {
    nNotifies++;
    lastNotifyOk = millis();
  }
  Serial.printf("[ble]    %s value='%s' subscribed=%d notify=%s total=%lu\r\n", why, text,
                (int)g_subscribed, ok ? "OK" : "no-subscriber", (unsigned long)nNotifies);
}

class ServerCbs : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &ci) override {
    Serial.printf("[ble]    connected, conn_handle=%u -> push state\r\n", ci.getConnHandle());
    publishState("onConnect");
  }
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int reason) override {
    Serial.printf("[ble]    disconnected reason=%d -> re-advertising\r\n", reason);
    g_subscribed = false;
    NimBLEDevice::startAdvertising();
  }
};
static ServerCbs g_serverCbs;

class StateCbs : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &, uint16_t subValue) override {
    g_subscribed = (subValue != 0);
    Serial.printf("[ble]    CCCD written 0x%04X (notify %s)\r\n", (unsigned)subValue,
                  g_subscribed ? "ON" : "OFF");
    if (g_subscribed) publishState("onSubscribe");
  }
  void onRead(NimBLECharacteristic *, NimBLEConnInfo &) override {
    Serial.printf("[ble]    READ by client -> '%s'\r\n", stateText());
  }
};
static StateCbs g_stateCbs;

static void startBle() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEServer *srv = NimBLEDevice::createServer();
  srv->setCallbacks(&g_serverCbs);
  NimBLEService *svc = srv->createService(BLE_SERVICE_UUID);
  g_stateChr = svc->createCharacteristic(BLE_STATE_CHAR_UUID,
                                         NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
                                         BLE_STATE_MAX_LEN);
  g_stateChr->setCallbacks(&g_stateCbs);
  svc->start();
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->enableScanResponse(true);
  adv->setName(BLE_DEVICE_NAME);
  adv->addServiceUUID(BLE_SERVICE_UUID);
  const bool started = adv->start();
  Serial.printf("[ble]    advertising '%s' started=%d\r\n", BLE_DEVICE_NAME, (int)started);
}

static void evaluateWindow() {
  float raw[IMU_FEATURES], scaled[IMU_FEATURES], probs[MODEL_N_CLASSES];
  imu_features(g_win, WINDOW_SAMPLES, raw);
  imu_apply_log(raw);
  imu_standardise(raw, MODEL_FEAT_MEAN, MODEL_FEAT_STD, scaled);
  model_predict(scaled, probs);
  int winner = 0;
  for (int k = 1; k < MODEL_N_CLASSES; ++k) {
    if (probs[k] > probs[winner]) winner = k;
  }
  nWindows++;

  // The same numbers a PC run would produce, for the same window length. mag_std and ax_jerk
  // separate shaking, bb_energy and bb_ratio are the breathing band.
  Serial.printf("[window] n=%u fill=%d sinceEval=%d\r\n", (unsigned)nWindows, g_fill,
                g_sinceEval);
  Serial.printf("[feature] mag_std=%.5f ax_jerk=%.5f gmag_mean=%.4f bb_energy=%.6f bb_ratio=%.5f\r\n",
                raw[13], raw[2], raw[22], raw[24], raw[25]);
  Serial.printf("[model]   p=[");
  for (int k = 0; k < MODEL_N_CLASSES; ++k) Serial.printf("%.4f ", probs[k]);
  Serial.printf("] winner=%s\r\n", MODEL_CLASS_NAMES[winner]);

  if (g_pending < 0) {
    g_pending = winner;
    Serial.printf("[state]   first vote: %s (needs one more agreeing window)\r\n",
                  MODEL_CLASS_NAMES[winner]);
    return;
  }
  if (winner != g_pending) {
    g_pending = winner;
    Serial.printf("[state]   vote changed -> %s (waiting for agreement)\r\n",
                  MODEL_CLASS_NAMES[winner]);
    return;
  }
  if (g_state != winner) {
    g_state = winner;
    show(colourFor(g_state));
    Serial.printf("[state]   COMMITTED -> %s (LED %06X)\r\n", MODEL_CLASS_NAMES[g_state],
                  (unsigned)colourFor(g_state));
    publishState("on-change");
  } else {
    Serial.printf("[state]   agreed, state stays %s\r\n", MODEL_CLASS_NAMES[g_state]);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);  // never waited on by the loop; a host may be absent entirely

  led.begin();
  led.setBrightness(RGB_LED_BRIGHTNESS);
  led.clear();
  led.show();

  Wire.begin(MPU_I2C_SDA_PIN, MPU_I2C_SCL_PIN, MPU_I2C_FREQ_HZ);
  const uint8_t who = regRead(MPU_REG_WHO_AM_I);
  configureSensor();
  Serial.printf("\r\n[sensor]  WHO_AM_I=0x%02X expected=0x%02X addr=0x%02X sda=%d scl=%d\r\n", who,
                MPU_WHO_AM_I_EXPECTED, MPU_I2C_ADDRESS, MPU_I2C_SDA_PIN, MPU_I2C_SCL_PIN);
  Serial.printf("[sensor]  rate=%d Hz window=%d stride=%d features=%d model=%d classes "
                "(win=%d stride=%d)\r\n",
                MPU_SAMPLE_RATE_HZ, WINDOW_SAMPLES, WINDOW_STRIDE_SAMPLES, IMU_FEATURES,
                MODEL_N_CLASSES, MODEL_WINDOW_SAMPLES, MODEL_WINDOW_STRIDE);
  g_sensorDead = (who != MPU_WHO_AM_I_EXPECTED);
  Serial.printf("[sensor]  usable=%d\r\n", (int)!g_sensorDead);

  startBle();

  Serial.println("[model]   classes:");
  for (int k = 0; k < MODEL_N_CLASSES; ++k) {
    Serial.printf("           [%d] %s\r\n", k, MODEL_CLASS_NAMES[k]);
  }

  epochUs = micros();
  nextUs = micros() + PERIOD_US;
}

void loop() {
  const unsigned long nowMs = millis();

  if (nowMs - lastBeatMs >= HEARTBEAT_MS) {
    lastBeatMs = nowMs;
    const double hz = nSamples * 1e6 / (double)(micros() - epochUs);
    Serial.printf("[tick]  uptime=%lu samples=%lu hz=%.2f read_fails=%lu state='%s'\r\n",
                  (unsigned long)nowMs, (unsigned long)nSamples, hz,
                  (unsigned long)nReadFails, stateText());
    publishState("heartbeat");
  }

  const uint32_t now = micros();
  if ((int32_t)(now - nextUs) < 0) return;
  nextUs += PERIOD_US;
  if ((int32_t)(micros() - nextUs) >= 0) nextUs = micros() + PERIOD_US;

  float s[IMU_SIG_COLS];
  if (!readSample(s)) {
    nReadFails++;
    if (nReadFails % 50 == 1) Serial.println("[sensor]  READ FAILED (no bytes from 0x68)");
    return;
  }
  nSamples++;
  if (nSamples <= 3) {
    Serial.printf("[sensor]  sample ax=%.4f ay=%.4f az=%.4f gx=%.3f gy=%.3f gz=%.3f\r\n", s[0],
                  s[1], s[2], s[3], s[4], s[5]);
  }

  memcpy(&g_win[g_fill * IMU_SIG_COLS], s, IMU_SIG_COLS * sizeof(float));
  g_fill++;
  g_sinceEval++;

  if (g_fill >= WINDOW_SAMPLES && g_sinceEval >= WINDOW_STRIDE_SAMPLES) {
    evaluateWindow();
    const int keep = WINDOW_SAMPLES - WINDOW_STRIDE_SAMPLES;
    memmove(g_win, &g_win[WINDOW_STRIDE_SAMPLES * IMU_SIG_COLS],
            keep * IMU_SIG_COLS * sizeof(float));
    g_fill = keep;
    g_sinceEval = 0;
  }
}
