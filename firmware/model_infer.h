// model_infer.h — forward pass for the MLP that training/train.py exported.
//
// Reads MODEL_* arrays from model_weights.h (generated; do not edit). Plain float32, no
// allocation, no dependency on Arduino or on any inference framework: one 24-float input,
// one 16-float hidden layer, one 4-float output. On the ESP32-S3's FPU this is a few thousand
// multiply-adds, i.e. microseconds next to the 12.5 ms a 2.5 s window takes to arrive.
//
// Indexing matches the generator exactly:
//   h[j]      = MODEL_B1[j] + sum_i x[i] * MODEL_W1[i*MODEL_N_HIDDEN + j]
//   logits[k] = MODEL_B2[k] + sum_j h[j] * MODEL_W2[j*MODEL_N_CLASSES + k]
// For two classes train.py widens the single logistic unit sklearn fits into the pair
// (-z/2, +z/2), so this one softmax path is correct for 2 classes and for 4.

#pragma once

#include <math.h>

#include "features.h"
#include "model_weights.h"

static float model_hidden[MODEL_N_HIDDEN];

// `x` must already be log1p'd and standardised (imu_apply_log + imu_standardise).
static inline void model_predict(const float *x, float *probs) {
  for (int j = 0; j < MODEL_N_HIDDEN; ++j) {
    float acc = MODEL_B1[j];
    const float *w = &MODEL_W1[j];  // row-major by feature: W1[i*HIDDEN + j]
    for (int i = 0; i < MODEL_N_FEATURES; ++i) acc += x[i] * w[i * MODEL_N_HIDDEN];
    model_hidden[j] = acc > 0.0f ? acc : 0.0f;  // relu
  }

  float max_logit = -1e30f;
  float logits[MODEL_N_CLASSES];
  for (int k = 0; k < MODEL_N_CLASSES; ++k) {
    float acc = MODEL_B2[k];
    const float *w = &MODEL_W2[k];  // W2[j*CLASSES + k]
    for (int j = 0; j < MODEL_N_HIDDEN; ++j) acc += model_hidden[j] * w[j * MODEL_N_CLASSES];
    logits[k] = acc;
    if (acc > max_logit) max_logit = acc;
  }

  float sum = 0.0f;
  for (int k = 0; k < MODEL_N_CLASSES; ++k) {
    probs[k] = expf(logits[k] - max_logit);  // shifted, so the exponential cannot overflow
    sum += probs[k];
  }
  if (sum <= 0.0f) sum = 1.0f;
  for (int k = 0; k < MODEL_N_CLASSES; ++k) probs[k] /= sum;
}

static inline int model_argmax(const float *probs) {
  int best = 0;
  for (int k = 1; k < MODEL_N_CLASSES; ++k) {
    if (probs[k] > probs[best]) best = k;
  }
  return best;
}

// Runner-up margin, 0..1. The abstention extension (step 10) reports *unsure* when this is
// small or when the top probability is low; it is computed from the first window so that the
// decision-smoothing logic in the sketch can stay unchanged when the threshold goes in.
static inline float model_margin(const float *probs) {
  float top = -1.0f, second = -1.0f;
  for (int k = 0; k < MODEL_N_CLASSES; ++k) {
    if (probs[k] >= top) {
      second = top;
      top = probs[k];
    } else if (probs[k] > second) {
      second = probs[k];
    }
  }
  return top - second;
}
