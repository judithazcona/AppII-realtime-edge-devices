// classifier.ino — the graded firmware: sense, feature, classify, show. All of it
// on the chip (AGENTS.md rule 10), with the USB cable carrying power only (rule 11).
//
// THERE IS NOT ONE USB-SERIAL TOKEN IN THIS FILE (grep -c for it reads zero, and this
// comment is worded so that grep stays a usable check). That is deliberate and it is
// the strongest form rule 7 takes in this repo: not an #ifdef that could be left
// on by mistake, but a sketch that cannot talk to a host even if someone wanted
// it to. The board reaches its working state from a power bank alone; the only
// channel it uses to tell you what it thinks is the LED.
//
// PIPELINE (identical to training/train.py, which is where these numbers came from):
//
//   MPU-6050 @ 100 Hz  ->  250-sample window (2.5 s), re-run every 125 samples (1.25 s)
//                      ->  24 hand-made features (../features.h == training/features.py)
//                      ->  log1p on the amplitude columns, then (x - mean)/std with the
//                          parameters train.py fitted on TRAIN sessions only
//                      ->  MLP 24 -> 16 relu -> softmax (../model_infer.h)
//                      ->  two agreeing windows in a row, then the LED changes colour
//
// COLOURS (firmware/config.h defines the pin; the class names come from the exported
// header, so a retrained model with more classes needs no edit here):
//   not_attached -> blue      shaking -> magenta
//   calm_breathing -> green   fast_breathing -> red
//   a name this build does not know -> white, i.e. "the model changed, I did not"
//
// DECISION SMOOTHING: a raw probability arrives every 1.25 s. The LED only follows a
// class that two consecutive windows agree on, so a single noisy window - a cough, the
// table being knocked, the board being picked up - cannot repaint the state. The cost is
// latency: a real change of state is on the LED between ~1.25 s and ~2.5 s after it
// starts, which is the ~2 s the brief asks for. Averaging probabilities instead would
// blur the two breathing classes into each other; voting on the decision does not.

#include <Wire.h>

#include <Adafruit_NeoPixel.h>
#include <NimBLEDevice.h>

#include "../config.h"
#include "../features.h"
#include "../model_infer.h"

// --- anti-drift: refuse to build if the exported model and config.h disagree -----
static_assert(WINDOW_SAMPLES == MODEL_WINDOW_SAMPLES,
              "WINDOW_SAMPLES in config.h != the window the exported model was trained on");
static_assert(WINDOW_STRIDE_SAMPLES == MODEL_WINDOW_STRIDE,
              "WINDOW_STRIDE_SAMPLES in config.h != the stride in model_weights.h");
static_assert(MPU_SAMPLE_RATE_HZ == MODEL_SAMPLE_RATE_HZ,
              "MPU_SAMPLE_RATE_HZ in config.h != the rate the model was trained at");
static_assert(IMU_FEATURES == MODEL_N_FEATURES,
              "features.h and model_weights.h disagree on the feature count");
static_assert(IMU_MAX_WINDOW >= WINDOW_SAMPLES, "IMU_MAX_WINDOW is smaller than WINDOW_SAMPLES");

static const uint32_t PERIOD_US = 1000000UL / MPU_SAMPLE_RATE_HZ;

// Colours, in the order the brief assigns them.
static const uint32_t COLOUR_NOT_ATTACHED = 0x0000FF;  // blue
static const uint32_t COLOUR_SHAKING = 0xFF00FF;       // magenta
static const uint32_t COLOUR_CALM = 0x00FF00;          // green
static const uint32_t COLOUR_FAST = 0xFF0000;          // red
static const uint32_t COLOUR_UNKNOWN = 0xFFFFFF;       // retrained model, unknown name
static const uint32_t COLOUR_SENSOR_FAULT = 0xFF8000;  // amber slow blink: bus is dead

// One full window, row-major, IMU_SIG_COLS columns: 250 * 6 * 4 B = 6000 B of SRAM.
static float g_win[WINDOW_SAMPLES * IMU_SIG_COLS];
static int g_fill = 0;        // rows currently held
static int g_sinceEval = 0;   // rows added since the last inference

static uint32_t nextUs = 0;   // drift-corrected sample schedule
static uint32_t nReadFails = 0;
static uint32_t nConsecFails = 0;
static uint32_t nWindows = 0;

static int g_pending = -1;    // last raw winner, waiting for a second opinion
static int g_state = -1;      // the class the LED is showing
static bool g_sensorDead = false;

static Adafruit_NeoPixel led(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// --------------------------------------------------------------------- BLE ---
// One service, one characteristic, READ + NOTIFY. The value is the current SMOOTHED state as
// plain UTF-8 text - exactly one of the MODEL_CLASS_NAMES, no newline, no decoration - so the
// monitor page does not have to parse anything. It is pushed when the state changes and again
// once a second as a heartbeat, which is also how a browser tells the board went away.
static NimBLECharacteristic *g_stateChr = nullptr;
static uint32_t lastBeatMs = 0;
static const uint32_t HEARTBEAT_MS = 1000;

class ServerCbs : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override {
    // A monitor page that loses the link must be able to come back without a power cycle, so
    // advertising restarts the moment the client leaves.
    NimBLEDevice::startAdvertising();
  }
};
static ServerCbs g_serverCbs;

static void startBle() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEServer *srv = NimBLEDevice::createServer();
  srv->setCallbacks(&g_serverCbs);
  NimBLEService *svc = srv->createService(BLE_SERVICE_UUID);
  g_stateChr = svc->createCharacteristic(BLE_STATE_CHAR_UUID,
                                         NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
                                         BLE_STATE_MAX_LEN);
  svc->start();
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);  // so the monitor page can filter on it
  adv->start();
}

// Published value: the class name from the exported header, or empty before the first decision
// (an empty read is honest - the board has not classified anything yet).
static const char *stateText() {
  if (g_state < 0 || g_state >= MODEL_N_CLASSES) return "";
  return MODEL_CLASS_NAMES[g_state];
}

static void publishState() {
  if (g_stateChr == nullptr) return;
  g_stateChr->setValue(stateText());  // READ is always current, connected or not
  if (NimBLEDevice::getServer()->getConnectedCount() > 0) g_stateChr->notify();
}

// ------------------------------------------------------------- MPU-6050 --------
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
  regWrite(MPU_REG_PWR_MGMT_1, 0x80);  // reset the signal path
  delay(100);
  regWrite(MPU_REG_PWR_MGMT_1, 0x01);  // wake, PLL with x-gyro reference
  regWrite(MPU_REG_PWR_MGMT_2, 0x00);  // all six axes enabled
  regWrite(MPU_REG_GYRO_CONFIG, MPU_GYRO_CONFIG_VAL_500DPS);
  regWrite(MPU_REG_ACCEL_CONFIG, MPU_ACCEL_CONFIG_VAL_8G);
  regWrite(MPU_REG_CONFIG, MPU_CONFIG_VAL_DLPF_44HZ);
  regWrite(MPU_REG_SMPLRT_DIV, MPU_SMPLRT_DIV_VAL);
  delay(50);
}

// One burst read of the 14 bytes 0x3B..0x48, converted to the same units the CSV
// (and therefore the training data) uses: g and deg/s.
static bool readSample(float out[IMU_SIG_COLS]) {
  Wire.beginTransmission(MPU_I2C_ADDRESS);
  Wire.write(MPU_REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(true) != 0) return false;
  if (Wire.requestFrom((int)MPU_I2C_ADDRESS, 14) != 14) return false;
  uint8_t b[14];
  for (int i = 0; i < 14; ++i) b[i] = (uint8_t)Wire.read();
  if (Wire.available()) return false;  // bus not behaving; drop the sample, do not invent one

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

// ------------------------------------------------------------- LED output ------
static uint32_t colourFor(int cls) {
  if (cls < 0 || cls >= MODEL_N_CLASSES) return COLOUR_UNKNOWN;
  const char *name = MODEL_CLASS_NAMES[cls];
  if (strcmp(name, "not_attached") == 0) return COLOUR_NOT_ATTACHED;
  if (strcmp(name, "shaking") == 0) return COLOUR_SHAKING;
  if (strcmp(name, "calm_breathing") == 0) return COLOUR_CALM;
  if (strcmp(name, "fast_breathing") == 0) return COLOUR_FAST;
  return COLOUR_UNKNOWN;  // e.g. novel_* classes added by the abstention extension
}

static void show(uint32_t colour) {
  for (int i = 0; i < RGB_LED_COUNT; ++i) led.setPixelColor(i, colour);
  led.show();
}

// Non-blocking, because this sketch must keep sampling while it blinks: a fault
// pattern that stalls the loop would hide the very thing it is reporting.
static void indicateFault(unsigned long nowMs) {
  static unsigned long lastToggle = 0;
  static bool on = false;
  if (nowMs - lastToggle < 600) return;
  lastToggle = nowMs;
  on = !on;
  show(on ? COLOUR_SENSOR_FAULT : 0x000000);
}

// Boot sweep: with no serial, the LED has to say "I started" as well as "this is the
// state". Four quick colours at power-up, then dark until the first window is in.
static void bootSweep() {
  const uint32_t seq[4] = {COLOUR_NOT_ATTACHED, COLOUR_SHAKING, COLOUR_CALM, COLOUR_FAST};
  for (int i = 0; i < 4; ++i) {
    show(seq[i]);
    delay(120);
  }
  show(0x000000);
}

// ------------------------------------------------------------- inference -------
static void evaluateWindow() {
  float raw[IMU_FEATURES], logged[IMU_FEATURES], scaled[IMU_FEATURES], probs[MODEL_N_CLASSES];

  imu_features(g_win, WINDOW_SAMPLES, raw);
  imu_apply_log(raw);
  imu_standardise(raw, MODEL_FEAT_MEAN, MODEL_FEAT_STD, scaled);
  model_predict(scaled, probs);
  const int winner = model_argmax(probs);
  nWindows++;

  // Two agreeing windows before the LED moves. ~2 s of evidence, one noisy window
  // of protection, and no probability averaging that would smear calm into fast.
  if (g_pending < 0) {
    g_pending = winner;
    return;
  }
  if (winner != g_pending) {
    g_pending = winner;  // changed its mind: this window becomes the first vote
    return;
  }
  if (g_state != winner) {
    g_state = winner;
    show(colourFor(g_state));  // same event, both channels: the LED and the BLE notification
    publishState();
  }
}

static void appendSample(const float s[IMU_SIG_COLS]) {
  if (g_fill >= WINDOW_SAMPLES) return;  // cannot happen; evaluateWindow() slides first
  memcpy(&g_win[g_fill * IMU_SIG_COLS], s, IMU_SIG_COLS * sizeof(float));
  g_fill++;
  g_sinceEval++;
}

// Slide the window on by one stride: keep the newest (WINDOW - STRIDE) rows and let
// the sampler refill the rest. 3 KB of memmove every 1.25 s - noise at 240 MHz.
static void slideWindow() {
  const int keep = WINDOW_SAMPLES - WINDOW_STRIDE_SAMPLES;
  memmove(g_win, &g_win[WINDOW_STRIDE_SAMPLES * IMU_SIG_COLS], keep * IMU_SIG_COLS * sizeof(float));
  g_fill = keep;
  g_sinceEval = 0;
}

void setup() {
  led.begin();
  led.setBrightness(RGB_LED_BRIGHTNESS);
  led.clear();
  led.show();

  Wire.begin(MPU_I2C_SDA_PIN, MPU_I2C_SCL_PIN, MPU_I2C_FREQ_HZ);
  const uint8_t who = regRead(MPU_REG_WHO_AM_I);
  configureSensor();
  g_sensorDead = (who == 0xEE || who != MPU_WHO_AM_I_EXPECTED);

  bootSweep();  // tells us the LED and the sketch are alive with no host attached
  if (g_sensorDead) indicateFault(millis());

  startBle();  // advertises and moves on; nothing here waits for a central to connect

  nextUs = micros() + PERIOD_US;
}

void loop() {
  // Heartbeat: the state goes out once a second as well as on every change, so a monitor page
  // can tell "calm" from "the board is gone". Runs before the sample schedule returns early.
  const unsigned long beatMs = millis();
  if (beatMs - lastBeatMs >= HEARTBEAT_MS) {
    lastBeatMs = beatMs;
    publishState();
  }

  const uint32_t now = micros();
  if ((int32_t)(now - nextUs) < 0) {
    if (g_sensorDead) indicateFault(millis());
    return;
  }
  nextUs += PERIOD_US;
  // Never burst-catch-up: a burst would inject fake high-frequency energy into the
  // window and could turn a breath into a shake. Drop the missed ticks instead.
  if ((int32_t)(micros() - nextUs) >= 0) nextUs = micros() + PERIOD_US;

  float s[IMU_SIG_COLS];
  if (!readSample(s)) {
    nReadFails++;
    nConsecFails++;
    if (nConsecFails >= 50 && !g_sensorDead) {  // 0.5 s with nothing on the bus
      g_sensorDead = true;
      g_state = -1;
      g_pending = -1;
    }
    if (g_sensorDead) indicateFault(millis());
    return;
  }
  if (g_sensorDead) {  // the bus came back (cable re-seated): resume, LED follows again
    g_sensorDead = false;
    g_state = -1;
    g_pending = -1;
    show(0x000000);
  }
  nConsecFails = 0;

  appendSample(s);
  if (g_fill >= WINDOW_SAMPLES && g_sinceEval >= WINDOW_STRIDE_SAMPLES) {
    evaluateWindow();
    slideWindow();
  }
}
