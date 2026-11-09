"""Window features, written so the ESP32 can compute **the identical numbers**.

Every feature here has a one-line equivalent in ``firmware/features.h``. That constraint is
deliberate and it cost something: **no FFT, no percentiles, no correlation matrix**, because a
2.5 s window on the chip has to produce the same feature vector as this file, in the same
order, to the same rounding. What survives is what actually separates the classes:

* ``std`` / ``mad`` — how much the axis moves at all;
* ``jerk``          — mean |x[n+1] - x[n]|: a cheap high-frequency-energy proxy that does the
  job a spectral feature would, in O(N) with no transform. **Difference of consecutive
  samples, deliberately not divided by dt**, so the chip computes the same number;
* ``zcr``           — sign changes of the demeaned signal: breathing crosses slowly, shaking
  crosses constantly;
* ``mag_*``         — tri-axial magnitude, invariant to how the board is oriented (which is
  exactly the nuisance variable here: same breathing, board rotated);
* the same on the gyroscope, where rotation — i.e. shaking — lives.

Amplitude-like features are non-negative by construction and get ``log1p`` (assignment tip:
log-transform the positive amplitudes). A shake is ~10x a breath in std and 100x in variance,
so without the log it dominates the first layer on scale alone.

``zcr`` is a count, not an amplitude, and is left alone.
"""

from __future__ import annotations

import numpy as np

AXES = ("x", "y", "z")

#: feature order — `firmware/model_weights.h` writes the same order; do not reorder
FEATURE_NAMES: tuple[str, ...] = (
    *(f"a{a}_{k}" for a in AXES for k in ("std", "mad", "jerk", "zcr")),
    "mag_mean", "mag_std", "mag_max", "mag_min",
    *(f"g{a}_{k}" for a in AXES for k in ("std", "jerk")),
    "gmag_mean", "gmag_max",
)

#: the non-negative amplitude-like features that get log1p (everything but the zcr counts)
LOG_FEATURES: frozenset[str] = frozenset(n for n in FEATURE_NAMES if not n.endswith("zcr"))

N_FEATURES = len(FEATURE_NAMES)  # 24
_LOG_MASK = np.array([n in LOG_FEATURES for n in FEATURE_NAMES])

#: columns of one window in the CSV, of the 13 streamed: t_us, ax, ay, az, gx, gy, gz, ...
SIG_COLS = 6


def _std(centred: np.ndarray) -> float:
    return float(np.sqrt(np.mean(centred * centred)))  # population std, ddof=0


def window_features(win: np.ndarray) -> np.ndarray:
    """Raw (pre-log, pre-standardisation) features for one window.

    `win` is ``(N, >=6)``: ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps first.
    """
    win = np.asarray(win, dtype=np.float64)
    accel, gyro = win[:, 0:3], win[:, 3:6]
    a_mag = np.sqrt(np.sum(accel * accel, axis=1))
    g_mag = np.sqrt(np.sum(gyro * gyro, axis=1))

    feats: list[float] = []
    for k in range(3):
        x = accel[:, k]
        c = x - x.mean()
        feats += [_std(c), float(np.mean(np.abs(c))), float(np.mean(np.abs(np.diff(x)))),
                  _zcr(c)]
    feats += [float(a_mag.mean()), _std(a_mag - a_mag.mean()),
              float(a_mag.max()), float(a_mag.min())]
    for k in range(3):
        x = gyro[:, k]
        c = x - x.mean()
        feats += [_std(c), float(np.mean(np.abs(np.diff(x))))]
    feats += [float(g_mag.mean()), float(g_mag.max())]

    out = np.asarray(feats, dtype=np.float64)
    if out.shape != (N_FEATURES,):
        raise AssertionError(f"feature vector is {out.shape}, expected ({N_FEATURES},)")
    return out


def _zcr(centred: np.ndarray) -> float:
    sign = np.sign(centred)
    return float(np.count_nonzero(np.diff(sign) != 0))


def log_amplitudes(raw: np.ndarray) -> np.ndarray:
    """log1p on the amplitude-like columns only; zcr counts pass through."""
    raw = np.atleast_2d(np.asarray(raw, dtype=np.float64))
    out = raw.copy()
    out[:, _LOG_MASK] = np.log1p(np.clip(out[:, _LOG_MASK], 0.0, None))
    return out


def standardisation_params(logged: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Mean/std fitted on TRAIN ONLY — fitting the scaler on test data would leak."""
    mean = logged.mean(axis=0)
    std = logged.std(axis=0)
    std[std < 1e-9] = 1.0  # a constant column (a dead axis) must not divide by ~0
    return mean, std


def apply_standardisation(logged: np.ndarray, mean: np.ndarray, std: np.ndarray) -> np.ndarray:
    return (np.atleast_2d(logged) - mean) / std


def build_matrix(windows: np.ndarray) -> np.ndarray:
    """Raw feature matrix for many windows at once. `windows` is (M, N, 6)."""
    return np.vstack([window_features(w) for w in windows])
