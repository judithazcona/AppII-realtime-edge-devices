// bench_test.ino — bring-up check for the GY-521 (MPU-6050) on the ESP32-S3 N16R8.
//
// WHAT THIS DOES:  I2C bus scan, WHO_AM_I read (register 0x75, a genuine
//                  MPU-6050 answers 0x68), then 10 seconds of live accelerometer
//                  and gyroscope samples over the serial port.
//
// SERIAL IS ALLOWED ONLY IN THIS BENCH SKETCH. The graded firmware must not
// depend on a serial connection at all (AGENTS.md rule 7 and rule 11): USB is a
// power cable in the demo, and no data may travel over it.
//
// Pins, address, ranges and sample rate all come from ../config.h, which mirrors
// HARDWARE.md. Nothing is hard-coded twice.

#include <Wire.h>

#include "../config.h"

static const uint32_t SERIAL_BAUD = 115200;
static const unsigned long BENCH_SECONDS = 10;
static const unsigned long PRINT_EVERY_MS = 100;

static bool sensorPresent = false;

static void regWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_I2C_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// Returns the register value, or 0xEE if the device did not answer.
static uint8_t regRead(uint8_t reg) {
  Wire.beginTransmission(MPU_I2C_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(true) != 0) return 0xEE;
  if (Wire.requestFrom((int)MPU_I2C_ADDRESS, 1) != 1) return 0xEE;
  return Wire.read();
}

// Walk the whole 7-bit space and report everything that answers.
static void scanI2C() {
  Serial.println(F("--- I2C scan (0x08 .. 0x77) ---"));
  uint8_t found = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device at 0x%02X", addr);
      if (addr == MPU_I2C_ADDRESS) Serial.print(F("   <- the GY-521 we expect"));
      Serial.println();
      found++;
    }
  }
  if (found == 0) {
    Serial.println(F("  nothing answered. Try, in this order:"));
    Serial.println(F("    1. SDA and SCL swapped (the most common cause). SDA=GPIO8, SCL=GPIO9."));
    Serial.println(F("    2. VCC on 3V3 and not 5V, and GND really connected (common ground)."));
    Serial.println(F("    3. Loose breadboard contacts on the module header."));
    Serial.println(F("    4. If it answers at 0x69 instead, AD0 is pulled high."));
  }
}

static void configureSensor() {
  // Reset the signal path, then wake the chip: bit 6 of PWR_MGMT_1 is SLEEP.
  regWrite(MPU_REG_PWR_MGMT_1, 0x80);  // DEVICE_RESET
  delay(100);
  regWrite(MPU_REG_PWR_MGMT_1, 0x01);  // wake, PLL with x-axis gyro reference
  regWrite(MPU_REG_PWR_MGMT_2, 0x00);  // keep all six axes enabled
  regWrite(MPU_REG_GYRO_CONFIG, MPU_GYRO_CONFIG_VAL_500DPS);
  regWrite(MPU_REG_ACCEL_CONFIG, MPU_ACCEL_CONFIG_VAL_8G);
  regWrite(MPU_REG_CONFIG, MPU_CONFIG_VAL_DLPF_44HZ);
  regWrite(MPU_REG_SMPLRT_DIV, MPU_SMPLRT_DIV_VAL);
  delay(50);
}

// Read the 14 bytes of accel + temperature + gyro in one burst.
static bool readSample(int16_t accel[3], int16_t &tempRaw, int16_t gyro[3]) {
  Wire.beginTransmission(MPU_I2C_ADDRESS);
  Wire.write(MPU_REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(true) != 0) return false;
  if (Wire.requestFrom((int)MPU_I2C_ADDRESS, 14) != 14) return false;
  const uint8_t axh = Wire.read(), axl = Wire.read();
  const uint8_t ayh = Wire.read(), ayl = Wire.read();
  const uint8_t azh = Wire.read(), azl = Wire.read();
  const uint8_t th = Wire.read(), tl = Wire.read();
  const uint8_t gxh = Wire.read(), gxl = Wire.read();
  const uint8_t gyh = Wire.read(), gyl = Wire.read();
  const uint8_t gzh = Wire.read(), gzl = Wire.read();
  accel[0] = (int16_t)((axh << 8) | axl);
  accel[1] = (int16_t)((ayh << 8) | ayl);
  accel[2] = (int16_t)((azh << 8) | azl);
  tempRaw = (int16_t)((th << 8) | tl);
  gyro[0] = (int16_t)((gxh << 8) | gxl);
  gyro[1] = (int16_t)((gyh << 8) | gyl);
  gyro[2] = (int16_t)((gzh << 8) | gzl);
  return true;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1500);  // give the host time to attach its monitor
  Serial.println();
  Serial.println(F("=============================================="));
  Serial.println(F(" GY-521 bench test  (serial allowed HERE ONLY)"));
  Serial.println(F("=============================================="));
  Serial.printf("SDA=GPIO%d  SCL=GPIO%d  address=0x%02X  I2C=%ld Hz\n",
                MPU_I2C_SDA_PIN, MPU_I2C_SCL_PIN, MPU_I2C_ADDRESS, (long)MPU_I2C_FREQ_HZ);

  if (!Wire.begin(MPU_I2C_SDA_PIN, MPU_I2C_SCL_PIN, MPU_I2C_FREQ_HZ)) {
    Serial.println(F("FATAL: Wire.begin() failed - the I2C peripheral could not start."));
  }

  scanI2C();

  const uint8_t who = regRead(MPU_REG_WHO_AM_I);
  Serial.printf("--- WHO_AM_I (reg 0x%02X) read 0x%02X ---\n", MPU_REG_WHO_AM_I, who);
  if (who == MPU_WHO_AM_I_EXPECTED) {
    Serial.println(F("    matches 0x68: a genuine MPU-6050. Good."));
    sensorPresent = true;
  } else if (who == 0x98 || who == 0x70 || who == 0x72) {
    // Real-world clones answer these instead; the registers we use still work.
    Serial.println(F("    NOT 0x68, but this is a known MPU-6050 CLONE answer."));
    Serial.println(F("    Continuing - accel/gyro registers are compatible."));
    sensorPresent = true;
  } else if (who == 0xEE) {
    Serial.println(F("    no answer from 0x68 - the sensor did not acknowledge."));
  } else {
    Serial.println(F("    unexpected value. Not the MPU-6050 answer we expected."));
    sensorPresent = true;  // something answered; let the live stream judge it
  }

  if (sensorPresent) {
    configureSensor();
    uint8_t check = regRead(MPU_REG_PWR_MGMT_1);
    Serial.printf("--- PWR_MGMT_1 now 0x%02X (bit 6 clear means awake) ---\n", check);
    Serial.printf("--- streaming for %lu s, one line every %lu ms ---\n",
                  BENCH_SECONDS, PRINT_EVERY_MS);
    Serial.println(F("expected at rest: the gravity axis near 1.00 g, gyro near 0"));
    Serial.println(F("millis    ax_g    ay_g    az_g   | gx_dps  gy_dps  gz_dps"));
  } else {
    Serial.println(F("RESULT: FAIL - no usable sensor on the bus."));
  }
}

void loop() {
  static unsigned long startMs = 0;
  static unsigned long lastPrint = 0;
  static unsigned long okCount = 0;
  static unsigned long failCount = 0;
  static bool reported = false;

  const unsigned long now = millis();
  if (startMs == 0) startMs = now;

  if (!sensorPresent) {
    if (!reported) {
      reported = true;
      Serial.println(F("Stopping. Nothing to read."));
    }
    return;
  }

  int16_t accel[3], gyro[3], tempRaw;
  const bool ok = readSample(accel, tempRaw, gyro);
  if (ok) okCount++; else failCount++;

  const bool inWindow = (now - startMs) < BENCH_SECONDS * 1000UL;
  if (inWindow && ok && (now - lastPrint) >= PRINT_EVERY_MS) {
    lastPrint = now;
    Serial.printf("%7lu  %6.2f  %6.2f  %6.2f   | %7.1f %7.1f %7.1f\n",
                  now - startMs,
                  accel[0] / MPU_ACCEL_LSB_PER_G, accel[1] / MPU_ACCEL_LSB_PER_G,
                  accel[2] / MPU_ACCEL_LSB_PER_G,
                  gyro[0] / MPU_GYRO_LSB_PER_DPS, gyro[1] / MPU_GYRO_LSB_PER_DPS,
                  gyro[2] / MPU_GYRO_LSB_PER_DPS);
  }

  if (!inWindow && !reported) {
    reported = true;
    Serial.println(F("--- bench window finished ---"));
    Serial.printf("good reads: %lu   failed reads: %lu\n", okCount, failCount);
    Serial.println(failCount == 0 && okCount > 20
                       ? F("RESULT: PASS - the GY-521 answers and streams.")
                       : F("RESULT: PARTIAL - reads are failing, check wiring/bus speed."));
    Serial.println(F("Now holding position: keeps reading, prints one line every 5 s."));
  }

  if (!inWindow && (now - lastPrint) >= 5000) {
    lastPrint = now;
    if (ok) {
      Serial.printf("alive  az=%6.2f g  gz=%6.1f dps   ok=%lu fail=%lu\n",
                    accel[2] / MPU_ACCEL_LSB_PER_G, gyro[2] / MPU_GYRO_LSB_PER_DPS,
                    okCount, failCount);
    }
  }

  // Pace the loop near the configured sample rate.
  delay(1000 / MPU_SAMPLE_RATE_HZ);
}
