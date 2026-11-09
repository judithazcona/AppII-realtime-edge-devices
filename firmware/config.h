// config.h — the one place hardware pins and sensor settings are defined.
//
// HARDWARE.md is the truth about the wiring; change this file and HARDWARE.md
// together, in the same commit (rule in AGENTS.md).
//
// Nothing here is a guess: every value is either stated by the wiring we built
// (HARDWARE.md), marked UNTESTED pending a bench measurement, or marked DEFAULT
// for the settings we deliberately left at the core default.

#pragma once

// ---------------------------------------------------------------- I2C bus ---
// GY-521 SDA -> GPIO 8, SCL -> GPIO 9 (wired by hand, see HARDWARE.md).
#define MPU_I2C_SDA_PIN 8
#define MPU_I2C_SCL_PIN 9

// 400 kHz is within the MPU-6050's Fast-mode limit. Short breadboard wires.
#define MPU_I2C_FREQ_HZ 400000L

// AD0 is not connected -> floating low -> 0x68. (0x69 if AD0 were tied high.)
#define MPU_I2C_ADDRESS 0x68

// ------------------------------------------------------------------- LED ----
// The board has ONE on-board addressable RGB LED (WS2812-family) that carries
// all four state colours. led_test probed GPIO 48 and GPIO 38 to find it.
//
// MEASURED 2026-11-09 14:35 by firmware/led_test: GPIO 48 produced red, green,
// blue and magenta on the on-board LED, GPIO 38 stayed dark. Observed by
// Judith; recorded in HARDWARE.md.
#define RGB_LED_PIN 48
#define RGB_LED_COUNT 1

// Brightness 0-255 for the single pixel. Kept low: full white is painfully
// bright in a lit room and 50 mA is easy on the 3V3 rail.
#define RGB_LED_BRIGHTNESS 60

// ------------------------------------------------------------ MPU-6050 ------
// Registers we touch.
#define MPU_REG_SMPLRT_DIV 0x19
#define MPU_REG_CONFIG 0x1A
#define MPU_REG_GYRO_CONFIG 0x1B
#define MPU_REG_ACCEL_CONFIG 0x1C
#define MPU_REG_ACCEL_XOUT_H 0x3B  // 14 bytes 0x3B..0x48: accel, temp, gyro
#define MPU_REG_PWR_MGMT_1 0x6B
#define MPU_REG_PWR_MGMT_2 0x6C
#define MPU_REG_WHO_AM_I 0x75

// Expected WHO_AM_I for a genuine MPU-6050. Clones often answer 0x98 or 0x70;
// bench_test prints what it actually reads instead of assuming.
#define MPU_WHO_AM_I_EXPECTED 0x68

// FULL_SCALE_RANGE: accelerometer +-8 g.
// Chosen so violent shaking does not clip the signal: at +-2 g the sensor
// saturates at 2 g and shaking is exactly the class that saturates it.
#define MPU_ACCEL_FULL_SCALE_G 8
#define MPU_ACCEL_LSB_PER_G 4096.0f  // 32768 / 8

// Gyroscope +-500 deg/s.
#define MPU_GYRO_FULL_SCALE_DPS 500
#define MPU_GYRO_LSB_PER_DPS 65.5f  // 32768 / 500

// Register values for those ranges.
#define MPU_ACCEL_CONFIG_VAL_8G 0x10
#define MPU_GYRO_CONFIG_VAL_500DPS 0x08

// Sampling: 100 Hz. Fast enough for shaking (>4 Hz), far more than enough for
// breathing (0.1-1 Hz), cheap on the bus.
#define MPU_SAMPLE_RATE_HZ 100
// Sample-rate divider: internal rate 1 kHz / (1 + divider) with DLPF enabled.
// 100 Hz -> divider 9.
#define MPU_SMPLRT_DIV_VAL ((1000 / MPU_SAMPLE_RATE_HZ) - 1)

// DLPF bandwidth 44 Hz (CONFIG = 0x03) with a 1 kHz gyro rate.
#define MPU_CONFIG_VAL_DLPF_44HZ 0x03

// --------------------------------------------------------------------- BLE ---
// Generated once with `uv run python -c "import uuid; print(uuid.uuid4())"` on 2026-11-09 and
// kept here so the monitor page and the firmware cannot drift apart (docs/BLE.md repeats them).
// Random 128-bit UUIDs: nothing else in the lab advertises these.
#define BLE_DEVICE_NAME "AppII-Monitor"
#define BLE_SERVICE_UUID "53bc7f1a-6719-4a10-917d-11be1aba66a3"
#define BLE_STATE_CHAR_UUID "e8507aa2-f631-4a05-893c-2a781f9d5be3"
// READ + NOTIFY. Value is the current smoothed state as plain UTF-8, one of the
// MODEL_CLASS_NAMES, with no newline and no other characters.
#define BLE_STATE_MAX_LEN 32

// ------------------------------------------------------------------ model ---
// Feature window used by the firmware and by the training scripts. 2.5 s at
// 100 Hz = 250 samples, 50 % overlap -> a new decision every 1.25 s.
#define WINDOW_SAMPLES 250
#define WINDOW_STRIDE_SAMPLES 125
#define WINDOW_MS (1000 / MPU_SAMPLE_RATE_HZ * WINDOW_SAMPLES)
