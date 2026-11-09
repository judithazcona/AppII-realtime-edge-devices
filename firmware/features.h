// features.h — the 24 window features, in C, mirroring training/features.py line for line.
//
// This file is the reason the accuracy measured on the PC is the accuracy the board has.
// Every formula here is the formula numpy ran during training, in the same order, with the
// same conventions:
//
//   * std is the POPULATION std (divide by N, not N-1);
//   * jerk is mean |x[n+1] - x[n]| over N-1 differences, deliberately NOT divided by dt,
//     so no unit conversion can drift between the two implementations;
//   * zcr counts sign changes of the demeaned signal, with sign(0) == 0 exactly as np.sign;
//   * no FFT, no percentiles, nothing that needs a transform buffer or a sort.
//
// Keep the two in step: change one, change the other, then re-run train.py (it regenerates
// firmware/model_weights.h, whose MODEL_FEATURE_NAMES order must match FEATURE_ORDER below).

#pragma once

#include <math.h>
#include <string.h>

#define IMU_FEATURES 24
#define IMU_SIG_COLS 6  // ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps

// Order of the feature vector. Must equal features.FEATURE_NAMES:
//   ax_std ax_mad ax_jerk ax_zcr  ay_...  az_...
//   mag_mean mag_std mag_max mag_min
//   gx_std gx_jerk  gy_std gy_jerk  gz_std gz_jerk
//   gmag_mean gmag_max
#define FEAT_AX_STD 0  // + 4 per axis

// log1p mask, written out as a table instead of derived from indices: an earlier version
// tried to compute it from i%4 / i%2 and silently skipped gx_jerk, gy_jerk and gz_jerk.
// One byte per feature, in FEATURE_NAMES order; 1 = log1p it. Everything is logged except
// the three zcr counts (features.LOG_FEATURES).
static const uint8_t IMU_FEAT_LOG[IMU_FEATURES] = {
    /* ax */ 1, 1, 1, 0,
    /* ay */ 1, 1, 1, 0,
    /* az */ 1, 1, 1, 0,
    /* mag_mean, mag_std, mag_max, mag_min */ 1, 1, 1, 1,
    /* gx std, jerk */ 1, 1,
    /* gy std, jerk */ 1, 1,
    /* gz std, jerk */ 1, 1,
    /* gmag_mean, gmag_max */ 1, 1,
};

static inline bool imu_feat_is_log(int i) { return IMU_FEAT_LOG[i] != 0; }

static inline float imu_sign(float v) { return (v > 0.0f) ? 1.0f : ((v < 0.0f) ? -1.0f : 0.0f); }

// Column layout of `win`: one row per sample, time-ordered, IMU_SIG_COLS columns.
#define IMU_MAX_WINDOW 250  // = WINDOW_SAMPLES / MODEL_WINDOW_SAMPLES in config.h

static inline void imu_features(const float *win, int n, float *out) {
  if (n < 2 || n > IMU_MAX_WINDOW) {  // never read past the caller's buffer
    for (int i = 0; i < IMU_FEATURES; ++i) out[i] = 0.0f;
    return;
  }
  float a_mag[IMU_MAX_WINDOW], g_mag[IMU_MAX_WINDOW];
  for (int i = 0; i < n; ++i) {
    const float ax = win[i * IMU_SIG_COLS + 0], ay = win[i * IMU_SIG_COLS + 1],
                az = win[i * IMU_SIG_COLS + 2];
    const float gx = win[i * IMU_SIG_COLS + 3], gy = win[i * IMU_SIG_COLS + 4],
                gz = win[i * IMU_SIG_COLS + 5];
    a_mag[i] = sqrtf(ax * ax + ay * ay + az * az);
    g_mag[i] = sqrtf(gx * gx + gy * gy + gz * gz);
  }

  int k = 0;
  for (int axis = 0; axis < 3; ++axis) {  // accelerometer: std, mad, jerk, zcr
    float mean = 0.0f;
    for (int i = 0; i < n; ++i) mean += win[i * IMU_SIG_COLS + axis];
    mean /= (float)n;
    float var = 0.0f, mad = 0.0f, jerk = 0.0f, zcr = 0.0f, prev_sign = 0.0f;
    for (int i = 0; i < n; ++i) {
      const float c = win[i * IMU_SIG_COLS + axis] - mean;
      var += c * c;
      mad += fabsf(c);
      const float s = imu_sign(c);
      if (i > 0 && s != prev_sign) zcr += 1.0f;
      prev_sign = s;
      if (i > 0) jerk += fabsf(win[i * IMU_SIG_COLS + axis] - win[(i - 1) * IMU_SIG_COLS + axis]);
    }
    out[k++] = sqrtf(var / (float)n);
    out[k++] = mad / (float)n;
    out[k++] = jerk / (float)(n - 1);
    out[k++] = zcr;
  }

  // magnitude block: mean, std, max, min (orientation-free energy - what a breath moves)
  {
    float mean = 0.0f;
    for (int i = 0; i < n; ++i) mean += a_mag[i];
    mean /= (float)n;
    float var = 0.0f, mx = a_mag[0], mn = a_mag[0];
    for (int i = 0; i < n; ++i) {
      const float d = a_mag[i] - mean;
      var += d * d;
      if (a_mag[i] > mx) mx = a_mag[i];
      if (a_mag[i] < mn) mn = a_mag[i];
    }
    out[k++] = mean;
    out[k++] = sqrtf(var / (float)n);
    out[k++] = mx;
    out[k++] = mn;
  }

  for (int axis = 0; axis < 3; ++axis) {  // gyroscope: std, jerk (columns 3..5)
    const int col = 3 + axis;
    float mean = 0.0f;
    for (int i = 0; i < n; ++i) mean += win[i * IMU_SIG_COLS + col];
    mean /= (float)n;
    float var = 0.0f, jerk = 0.0f;
    for (int i = 0; i < n; ++i) {
      const float c = win[i * IMU_SIG_COLS + col] - mean;
      var += c * c;
      if (i > 0) jerk += fabsf(win[i * IMU_SIG_COLS + col] - win[(i - 1) * IMU_SIG_COLS + col]);
    }
    out[k++] = sqrtf(var / (float)n);
    out[k++] = jerk / (float)(n - 1);
  }

  {
    float mean = 0.0f, mx = g_mag[0];
    for (int i = 0; i < n; ++i) {
      mean += g_mag[i];
      if (g_mag[i] > mx) mx = g_mag[i];
    }
    out[k++] = mean / (float)n;
    out[k++] = mx;
  }
}

// log1p on the amplitude-like columns only, matching features.log_amplitudes().
static inline void imu_apply_log(float *feat) {
  for (int i = 0; i < IMU_FEATURES; ++i) {
    if (imu_feat_is_log(i)) feat[i] = log1pf(feat[i] < 0.0f ? 0.0f : feat[i]);
  }
}

// (x - mean) / std with the parameters train.py fitted on TRAIN sessions only.
static inline void imu_standardise(const float *feat, const float *mean, const float *stdv,
                                   float *out) {
  for (int i = 0; i < IMU_FEATURES; ++i) out[i] = (feat[i] - mean[i]) / stdv[i];
}
