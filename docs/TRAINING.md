# Training — from recordings to the bytes in the chip

Four scripts, one seed, one command. Everything here runs on the PC; **inference itself runs on
the ESP32** (`firmware/features.h` + `firmware/model_infer.h`), which is the graded constraint
(AGENTS.md rule 10).

```
firmware/data_recorder  --USB-->  training/record.py  -->  data/raw/*.csv   (gitignored)
                                                              |
                                        training/features.py  |  training/train.py
                                                              v
                                              firmware/model_weights.h  (committed, 8.8 KB)
```

## 1. Recording

```bash
uv run python training/record.py --probe                 # link check, writes nothing
uv run python training/record.py --class shaking --session vigorous_arm --duration 45
uv run python training/record.py --list                  # what is on disk, and is it usable
```

The board runs `firmware/data_recorder`, which streams
`t_us, ax..gz, gx..gz, dlpf, rate, afs, gfs` at 100 Hz and **sends its own settings in the stream
header**; `record.py` copies them into the CSV, so a recording always carries the ranges that
produced it instead of a constant that could go stale. Files are
`data/raw/sessionNNN_<class>__<take>.csv`; the directory is gitignored (public repo, rule 20).
`--list` refuses anything shorter than 10 s and flags takes we decided not to train on.

## 2. Splitting — whole sessions, before windowing

`training/split_assignment.json` is **committed**, so every experiment is evaluated on the same
held-out sessions (rule 9). Sessions are assigned to train/val/test as whole units and only then
cut into windows: neighbouring windows overlap by 50 % and are near-duplicates, so splitting after
windowing leaks and inflates the score. The assignment is produced by `assign_splits()` in
`train.py` from `SEED = 20261109` (`training/config.py`) and repaired deterministically when a
class is too thin:

* classes with ≥3 sessions get 2 train + 1 val + 1 test;
* a class with exactly 2 sessions (both breathing classes today) gets **1 train + 1 test** — the
  minimum that still measures something;
* a class with 1 session is reported as untrainable and left out of the metric block rather than
  silently trained and "validated" on itself.

Test metrics are always reported on held-out **sessions**, never on random windows.

## 3. Features — 26 numbers per 2.5 s window

`training/features.py`, 250 samples at 100 Hz, stride 125 samples (1.25 s, 50 % overlap):

| group | features | why |
| --- | --- | --- |
| per accel axis | `std, mad, jerk, zcr` × x,y,z | `jerk` = `mean|x[n+1]-x[n]|`, a high-frequency proxy with no transform |
| accel magnitude | `mag_mean, mag_std, mag_max, mag_min` | orientation-invariant — the board's pose is the nuisance variable |
| per gyro axis | `std, jerk` × x,y,z | rotation, i.e. shaking, lives here |
| gyro magnitude | `gmag_mean, gmag_max` | |
| breathing band | `bb_energy, bb_ratio` | `|DFT|` of the demeaned accel magnitude at 0.15/0.30/0.50/0.70/0.90 Hz, averaged; `bb_ratio` divides by `mag_std` |

Constraints that shaped the list: **no FFT, no percentiles, no correlation matrix** — the chip has
to reproduce every number in the same order. `std` is the population std (ddof=0), `jerk` is
deliberately *not* divided by `dt` so the firmware computes the identical value, `zcr` uses
`sign(0)=0`. Everything non-negative gets `log1p`; the three `zcr` counts do not. The scaler
(mean/std) is fitted on **train sessions only**.

The C++ mirror is `firmware/features.h`; `bb_ratio` divides by `out[13]` (`mag_std`), and a
`static_assert(IMU_FEATURES == MODEL_N_FEATURES)` in the sketch makes a one-sided change a
compile error instead of a silent accuracy cliff.

## 4. Model and export

```bash
uv run python training/train.py --export                        # all classes found on disk
uv run python training/train.py --classes not_attached,shaking --export
```

`MLPClassifier(hidden_layer_sizes=(16,), activation="relu", alpha=1e-3, max_iter=3000,
random_state=SEED)`, one hidden layer, softmax output; class imbalance is handled with per-window
`sample_weight` (`MLPClassifier` has no `class_weight`). A `LogisticRegression` on the same
features is printed next to it as a floor, so we can say what the non-linearity buys.

The exporter writes `firmware/model_weights.h`: weights, biases, the training scaler, the class
names, and the window/stride/rate the model expects. Two details that are not obvious:

* the binary case fits one logistic unit, which is widened to `(-z/2, +z/2)` so the shared softmax
  forward pass reproduces the sigmoid exactly;
* every constant is emitted through `c_float()`, which forces a `.` or an exponent — `1f` is a
  *user-defined numeric literal* to the ESP32 toolchain and does not compile.

**Verification built into the script:** the header is re-read from disk, forward-propagated in
numpy and compared with sklearn on the same inputs. Measured max |Δp| = **1.6e-07** (4 classes),
**7.4e-10** (2 classes). It then re-parses `config.h` and checks the window constants agree.

## 5. Reproducing the numbers in RESULTS.md

```bash
uv sync
uv run python training/train.py --classes not_attached,shaking --export   # section 2
uv run python training/train.py --export                                  # section 3
```

Same recordings + same seed ⇒ byte-identical metric block. If it ever moves, that is a bug
(rule 8), not noise.

## 6. What the training does *not* do yet

* no hyper-parameter search (`--hidden/--alpha/--epochs` exist but only the defaults were run);
* no per-session augmentation and no subject-independent cross-validation beyond the single
  held-out session per class;
* the `novel_*` sessions recorded for the abstention extension (step 10) are on disk but
  unlabelled into the model — abstention uses `model_margin()` in `firmware/model_infer.h`, and is
  **TODO**;
* no on-chip feature-parity test: the exporter proves the *weights* survive, not that
  `features.h` produces the same vector as `features.py`. TODO, and the highest-value test left.
