# PLAN

Working plan for the lab. Status date: **2026-11-09**.

Rules that constrain every step live in [`AGENTS.md`](AGENTS.md) — in particular the
recording-level split, fixed seeds, on-device inference and the "USB is power only" demo
constraint. Update this file in the same commit as the change it describes (rule 7).

## Goal

Train a small model on our own audio recordings and run inference **on the ESP32-S3** from
microphone input, with no PC involved at demo time (USB supplies power only).

> **To confirm with the group/tutor:** the exact sound or command set to detect, the
> microphone hardware, and the required accuracy/latency target. See *Open questions* below.

## Current status

Repository skeleton only: working rules, plan, gitignore, README. No code, no data yet.

## Steps

### 0. Repository skeleton
- [x] `AGENTS.md` (working rules), `CLAUDE.md` (`@AGENTS.md`), `PLAN.md`, `README.md`,
      `.gitignore`
- [x] Private GitHub repository, first commit pushed
- [ ] Python environment pinned: `uv init` → `pyproject.toml` + `uv.lock`, committed
- [ ] Fix the shared `SEED` constant in one config module used by every script

**Checkpoint commit:** `Add project skeleton, working rules and plan`

### 1. Recordings
- [ ] Record the corpus (who speaks, which words/sounds, how many takes, sample rate)
- [ ] Store raw recordings outside the repo; add `data/README.md` with format, licence,
      naming convention and how to obtain them
- [ ] `uv run` script to list/validate recordings: sample rate, length, clipping, channel
      count — print a table per recording

### 2. Dataset: features and leak-free split
- [ ] Decide features (e.g. log-mel spectrogram frames) and window/hop sizes
- [ ] **Assign each whole recording to train/val/test first, then cut windows** (rule 4);
      commit the recording→split assignment list so every experiment shares it
- [ ] Assert in code that no recording id appears in two splits, and that the window count
      per split is printed
- [ ] Fixed seed for shuffling and for any augmentation

**Checkpoint commit:** `Add feature extraction and recording-level train/val/test split`

### 3. Baseline model on the PC
- [ ] Train the smallest model that can solve the task (start simple, e.g. small CNN or
      logistic regression over mel features)
- [ ] Report val/test accuracy per held-out **recording**, plus the confusion matrix
- [ ] Record model size, and the training command, so the run is reproducible

### 4. Port to the ESP32-S3
- [ ] Convert to a format the S3 runs (TFLite / esp-nn), int8 quantised with representative
      data from the **training** recordings only
- [ ] Compare PC vs on-device outputs on the same input windows (max absolute error, and
      whether the predicted label survives quantisation)
- [ ] Check flash/RAM footprint and inference time per window on the device

**Checkpoint commit:** `Add quantised model export and on-device inference`

### 5. Firmware
- [ ] Boot → record window → infer → indicate result (LED/output), running standalone
- [ ] **No serial dependency:** nothing blocks on `Serial`, no `while (!Serial)`, logging
      non-blocking and silently dropped when no host reads it (rule 6)
- [ ] Watchdog / recovery: a failure in one window must not stop the loop
- [ ] Flash + RAM + inference-time figures recorded here

### 6. Demo and write-up
- [ ] Demo verified with the data cable unplugged from the PC (charger only) — this is the
      acceptance test for rule 6
- [ ] End-to-end check: one command to retrain, one to export the model, one to flash
- [ ] Report: method, leak-free split rationale, metrics, on-device numbers, limitations
- [ ] Final `PLAN.md` update matching reality

## Commit checkpoints

At least 3 intermediate commits during the lab (rule 10). Natural ones: skeleton → dataset
& split → baseline model → quantised export + firmware → demo/report. Push each when it is
verified, not at the end.

## Verification log

| What was checked | How | Result |
| --- | --- | --- |
| (fill in as steps complete, per rule 9) | | |

## Open questions

1. Which sounds/commands must the device detect, and how many classes?
2. Which ESP32-S3 board and microphone (onboard PDM mic, I2S MEMS module, or line in)?
3. Sample rate and window length needed for the task, and the latency budget?
4. Accuracy target, and is it measured per window or per recording/session?
5. Do we have recordings already, or is recording part of the lab time?
6. Toolchain preference for the firmware: Arduino/PlatformIO or ESP-IDF?
7. Any constraint on model frameworks (e.g. must use TFLite Micro / esp-nn)?
