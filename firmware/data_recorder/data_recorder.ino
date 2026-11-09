// data_recorder.ino — stream raw MPU-6050 samples over USB serial for building
// the training dataset. This is a DATA-COLLECTION TOOL, not the demo firmware:
// serial use here is allowed by AGENTS.md rule 7 (second exception after
// bench_test) because the PC is deliberately attached while we record. The
// graded firmware (steps 6-9) must still run from a power bank with no data
// crossing the cable (rule 11).
//
// Output is a machine-readable stream that training/record.py turns into one
// CSV per labelled session:
//
//   # key value          comment/header lines, parsed once and copied into the CSV
//   t_us,ax_g,...        one data row per sample, 100 Hz
//
// Every sample carries the ESP32's own `micros()` timestamp in t_us, NOT the
// PC clock: the PC's read loop can be delayed by anything the OS feels like
// doing, and a jittery timestamp is a lie that later shows up as fake
// frequency content. The MPU is clocked by its own internal oscillator, so the
// real delivered rate is measured from t_us, not assumed (see the summary that
// record.py prints when a session ends).
//
// Pins, address, ranges, scales and the sample rate all come from
// ../config.h, which mirrors HARDWARE.md. Nothing is set twice.

#include <Wire.h>

#include "../config.h"

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t PERIOD_US = 1000000UL / MPU_SAMPLE_RATE_HZ;  // 10000 us at 100 Hz
static const unsigned long PROGRESS_EVERY_MS = 5000;

static uint32_t nextUs = 0;      // drift-corrected schedule
static uint32_t epochUs = 0;     // t_us reference; 'B' from the PC resets this
static uint32_t nSamples = 0;
static uint32_t nFails = 0;
static uint32_t lastProgressMs = 0;

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

// One burst read of the 14 bytes 0x3B..0x48: accel, temperature, gyro.
static bool readSample(int16_t accel[3], int16_t gyro[3]) {
  Wire.beginTransmission(MPU_I2C_ADDRESS);
  Wire.write(MPU_REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(true) != 0) return false;
  if (Wire.requestFrom((int)MPU_I2C_ADDRESS, 14) != 14) return false;
  const uint8_t b[14] = {
      (uint8_t)Wire.read(), (uint8_t)Wire.read(), (uint8_t)Wire.read(), (uint8_t)Wire.read(),
      (uint8_t)Wire.read(), (uint8_t)Wire.read(), (uint8_t)Wire.read(), (uint8_t)Wire.read(),
      (uint8_t)Wire.read(), (uint8_t)Wire.read(), (uint8_t)Wire.read(), (uint8_t)Wire.read(),
      (uint8_t)Wire.read(), (uint8_t)Wire.read(),
  };
  if (Wire.available()) return false;  // bus is not behaving; drop the sample
  accel[0] = (int16_t)((b[0] << 8) | b[1]);
  accel[1] = (int16_t)((b[2] << 8) | b[3]);
  accel[2] = (int16_t)((b[4] << 8) | b[5]);
  gyro[0] = (int16_t)((b[8] << 8) | b[9]);
  gyro[1] = (int16_t)((b[10] << 8) | b[11]);
  gyro[2] = (int16_t)((b[12] << 8) | b[13]);
  return true;
}

// Header the PC-side recorder parses. Everything the CSV needs to be readable
// two months from now, without this sketch in front of you: format version,
// column order, rate, ranges, scales and the pins.
static void printHeader(uint8_t who) {
  Serial.println(F("# imu_csv_version 1"));
  Serial.println(F("# device ESP32-S3 N16R8 + MPU-6050 (GY-521), firmware/data_recorder"));
  Serial.printf("# sda_gpio %d\r\n", MPU_I2C_SDA_PIN);
  Serial.printf("# scl_gpio %d\r\n", MPU_I2C_SCL_PIN);
  Serial.printf("# i2c_hz %ld\r\n", (long)MPU_I2C_FREQ_HZ);
  Serial.printf("# i2c_address 0x%02X\r\n", MPU_I2C_ADDRESS);
  Serial.printf("# who_am_i 0x%02X\r\n", who);
  Serial.printf("# rate_hz %d\r\n", MPU_SAMPLE_RATE_HZ);
  Serial.printf("# dlpf_hz 44\r\n");
  Serial.printf("# accel_full_scale_g %d\r\n", MPU_ACCEL_FULL_SCALE_G);
  Serial.printf("# accel_lsb_per_g %.1f\r\n", (double)MPU_ACCEL_LSB_PER_G);
  Serial.printf("# gyro_full_scale_dps %d\r\n", MPU_GYRO_FULL_SCALE_DPS);
  Serial.printf("# gyro_lsb_per_dps %.1f\r\n", (double)MPU_GYRO_LSB_PER_DPS);
  Serial.printf("# window_samples %d\r\n", WINDOW_SAMPLES);
  Serial.printf("# window_stride_samples %d\r\n", WINDOW_STRIDE_SAMPLES);
  Serial.println(F("# units accel=g, gyro=deg_per_second, raw=LSB counts"));
  Serial.println(F("# timestamp t_us=microseconds since the last 'B' marker, ESP32 clock"));
  Serial.println(F("# columns t_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw"));
  Serial.println(F("# commands: 'H' re-send this header, 'B' reset t_us and start a clean session"));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);  // let the USB-UART settle; no host is waited for anywhere (rule 11)

  Wire.begin(MPU_I2C_SDA_PIN, MPU_I2C_SCL_PIN, MPU_I2C_FREQ_HZ);
  const uint8_t who = regRead(MPU_REG_WHO_AM_I);
  configureSensor();
  printHeader(who);

  if (who == 0xEE) {
    // Loud on purpose: a recording session against a dead bus is 60 s wasted.
    Serial.println(F("# ERROR no device answered at 0x68 - check wiring, then re-open the port"));
  }
  epochUs = micros();
  nextUs = micros() + PERIOD_US;
}

void emitSample() {
  int16_t accel[3], gyro[3];
  if (!readSample(accel, gyro)) {
    nFails++;  // no line at all: the gap in t_us is the evidence, do not invent a row
    return;
  }
  const uint32_t tUs = (uint32_t)(micros() - epochUs);
  nSamples++;
  Serial.printf("%lu,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%d,%d,%d,%d,%d,%d\n", tUs,
                accel[0] / MPU_ACCEL_LSB_PER_G, accel[1] / MPU_ACCEL_LSB_PER_G,
                accel[2] / MPU_ACCEL_LSB_PER_G, gyro[0] / MPU_GYRO_LSB_PER_DPS,
                gyro[1] / MPU_GYRO_LSB_PER_DPS, gyro[2] / MPU_GYRO_LSB_PER_DPS, accel[0],
                accel[1], accel[2], gyro[0], gyro[1], gyro[2]);
}

void loop() {
  // Host commands. 'H' re-sends the header: the header is printed once at boot, but the
  // PC opens the port whenever it likes (and does not necessarily reset the board by
  // toggling DTR/RTS), so a recorder that connects mid-run would otherwise never learn
  // the rate, ranges or column order it is reading. 'B' asks for a clean session start:
  // zero the clock and the counters so the CSV starts at t_us=0.
  while (Serial.available()) {
    const int c = Serial.read();
    if (c == 'H') {
      printHeader(regRead(MPU_REG_WHO_AM_I));
    } else if (c == 'B') {
      epochUs = micros();
      nSamples = 0;
      nFails = 0;
      nextUs = micros() + PERIOD_US;
      Serial.println(F("# begin t_us=0"));
    }
  }

  const uint32_t now = micros();
  if ((int32_t)(now - nextUs) >= 0) {
    nextUs += PERIOD_US;
    // If we fell behind by a whole period (blocking Serial.write at 115200),
    // resync instead of firing a burst of catch-up samples: a burst would put
    // fake high-frequency energy into the recording.
    if ((int32_t)(micros() - nextUs) >= 0) nextUs = micros() + PERIOD_US;
    emitSample();
  }

  const unsigned long nowMs = millis();
  if (nowMs - lastProgressMs >= PROGRESS_EVERY_MS) {
    lastProgressMs = nowMs;
    Serial.printf("# progress samples=%lu read_fails=%lu uptime_ms=%lu\r\n", nSamples, nFails,
                  nowMs);
  }
}
