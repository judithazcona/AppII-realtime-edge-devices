# Testing and verification log

The rule this file exists for (AGENTS.md rule 15): *done comes with the command that was run and
what it printed.* Each row says **PASS** (I ran it and saw the result), **FAIL** (with the exact
error) or **NOT RUN** (nobody should believe it works until it is run). Times are the lab clock.

## 1. Hardware bring-up

| what | how it was checked | result |
| --- | --- | --- |
| MPU-6050 on the bus | `firmware/bench_test` prints `WHO_AM_I` | **PASS** — `0x68`, the genuine-part number (`config.h` refuses to call anything else expected) |
| I²C pins | wired by hand, `Wire.begin(8, 9)` | **PASS** — 100 Hz streaming through `data_recorder` |
| on-board RGB LED pin | `firmware/led_test` probed GPIO 48 and 38, 14:35 | **PASS** — 48 showed red/green/blue/magenta, 38 stayed dark; observed by Judith, recorded in `HARDWARE.md` |
| sensor full-scale ranges | register readback in `bench_test` | **PASS** — ±8 g (4096 LSB/g), ±500 °/s (65.5 LSB/dps), DLPF 44 Hz |
| delivered sample rate | `data_recorder` header + measured from board timestamps in `record.py` | **PASS** — 100.00 Hz over a 45 s session |
| Wi-Fi / audio | none used | n/a — the device is motion-only; the board's hotspot page is **NOT RUN** (step 9) |

## 2. PC-side pipeline

| check | command | result |
| --- | --- | --- |
| split determinism | `uv run python training/train.py --export` twice | **PASS** — identical metric block; splits come from committed `training/split_assignment.json` |
| no window leakage | read of `assign_splits()` + committed session→split map | **PASS** by construction — whole sessions assigned before windowing |
| export fidelity | built into `train.py`: re-parse header, forward-propagate in numpy, diff vs sklearn | **PASS** — max \|Δp\| = 1.6e-07 (4-class), 7.4e-10 (2-class) |
| linear floor comparison | same run prints `LogisticRegression` on identical features/sessions | **PASS** — 100 % (2-class), 76.2 % vs MLP 76.8 % (4-class) |
| metrics on held-out sessions | same run, session-level confusion matrix | **PASS** — see [`RESULTS.md`](RESULTS.md) |
| C float literal rule | compile after export | **FAIL→FIXED** — `error: unable to find numeric literal operator 'operator""f'` on `1f`; exporter now emits `1.0f` via `c_float()` |
| split starvation | 4-class run with 2-session classes | **FAIL→FIXED** — first run trained on zero sessions for both breathing classes (42.7 % overall, calm/fast recall 0 %); `assign_splits` now guarantees ≥1 fitting session and mirrors a training session into test when a class has no held-out session |

## 3. Firmware builds

FQBN `esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PSRAM=disabled,CDCOnBoot=default`,
`arduino-cli compile` (full commands in [`firmware/README.md`](../firmware/README.md)).

| sketch | result |
| --- | --- |
| `led_test`, `bench_test`, `data_recorder` | **PASS** — compiled, flashed, observed on the board |
| `classifier` (2-class, no BLE) | **PASS** — 323 042 B flash (24 %) / 29 664 B RAM (9 %) |
| `classifier` (4-class, no BLE) | **PASS** — 327 578 B / 29 664 B |
| `classifier` + BLE | **PASS** — 578 857 B (44 %) / 40 172 B (12 %) |
| `classifier` + BLE + window-timing guard | **PASS compile only** — 578 953 B (44 %) / 40 180 B (12 %), **not yet on the chip**, see §5 |
| `debug_pipeline` (temporary, rule 7) | **PASS** — 590 337 B; compiled, flashed, logged |
| `grep -c Serial firmware/classifier/classifier.ino` | **PASS** — `0`: the demo sketch cannot talk to a host even by accident |
| config/model agreement | `static_assert` × 5 in `classifier.ino` | **PASS** — wrong window, stride, rate or feature count is a compile error |

NimBLE-Arduino **2.3.9**: **FAIL→FIXED** three API mismatches — `getConnections()` →
`getConnectedCount()`, `setScanResponse()` → `enableScanResponse(true)`,
`NimBLEConnInfo::getConnId` → `getConnHandle()`.

## 4. BLE, traced stage by stage (the `not_attached` report)

Report: connects, value stuck at `not_attached`, app says *"Notifications and indications
disabled"*. Diagnostic: `firmware/debug_pipeline`, the same `config.h` / `features.h` /
`model_infer.h` / window logic / BLE as the demo, plus logging.

| stage | evidence | verdict |
| --- | --- | --- |
| 1 sensor reads continuously | `[tick] samples=9243 hz=99.86 read_fails=0` | **PASS** |
| 2 windows fill at the trained rate | `[window] fill=250 sinceEval=125` | **PASS**, but see §5 — intermittent I²C dropouts stretch it |
| 3 classifier gets real data | `[feature] mag_std=0.00240 ax_jerk=0.00161 gmag_mean=1.3046 bb_energy=0.000162` — the numbers of a board lying still | **PASS** |
| 4 classifier produces different outputs | `[model] p=[0.0377 0.0031 0.9589 0.0004 ] winner=not_attached`, and a different winner when moved | **PASS** |
| 5 is `not_attached` hardcoded? | `grep` finds no literal; the string comes from `MODEL_CLASS_NAMES[g_state]` | **NO** — it was the *correct* answer for a motionless board on a desk |
| 6 characteristic updated on change | `publishState()` on the two-vote commit and on the 1 s heartbeat | **PASS** |
| 7 notifications actually transmitted | `[ble] heartbeat value='not_attached' subscribed=0 notify=OK total=92` | **PASS on the wire, ROOT CAUSE client-side**: `subscribed=0` — no client wrote `0x0001` to the CCCD (0x2902), so the stack correctly delivered nothing. Only the central can enable notifications. |

Two real firmware defects found while proving the above, both fixed:

1. **Name invisible to scanners.** A 128-bit service UUID takes 19 of the 31 advertising bytes, so
   the 13-byte name does not fit in `ADV_IND`. It now goes in the **scan response**, with
   `enableScanResponse(true)` set explicitly — without that the board appeared unnamed in NB Devices.
2. **First notifications skipped.** `publishState()` used to gate on `getConnectedCount() > 0`,
   which can still be 0 while a link is coming up. It now calls `notify()` unconditionally, and an
   `onSubscribe` callback pushes the current state the moment the CCCD is written.

**Still NOT RUN:** no BLE client has been *observed* receiving a live change (the GitHub Pages
monitor page does not exist yet; a laptop-side WinRT `BluetoothLEAdvertisementWatcher` could not be
subscribed from PowerShell in this environment — "Windows PowerShell no se puede suscribir a
eventos de Windows RT"). The server side is verified; the client side is not.

## 5. Defect found by the trace: stretched windows

Intermittent I²C dropouts do not shorten a window, they **stretch** it: 250 samples delivered over
6–10 s instead of 2.5 s makes every rate-like feature (jerk, zcr, the 0.15–0.90 Hz band) ~3×
smaller, and the model then reads a shake as breathing — silently wrong, on a board with no serial
to complain into. Fix in `firmware/classifier`: `windowTimingOk()` refuses a window that took more
than 1.5× its nominal time, clears the committed state, and blinks amber until a clean window
arrives. Compile **PASS**; on-hardware behaviour **NOT RUN** (see §6).

## 6. Flash status right now — read this before the demo

`arduino-cli board list` at 18:30 → **No boards found**; the upload attempt returned
`Could not open COM4 ... FileNotFoundError(2)`. **The build with the window-timing guard has not
been flashed.** The last firmware confirmed on the chip is the `debug_pipeline` diagnostic build,
which uses USB serial and is *not* the demo. Before presenting:

```bash
"$LOCALAPPDATA/ArduinoCLI/arduino-cli" compile --fqbn \
  "esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PSRAM=disabled,CDCOnBoot=default" \
  --upload --port COM4 firmware/classifier
```

## 7. Acceptance tests that are still open

| # | test | how | status |
| --- | --- | --- | --- |
| 1 | feature parity on the chip | one recorded window through `features.h`, diffed against `features.py` | **NOT RUN** — highest-value test left |
| 2 | power-bank boot, data cable out of the laptop | run from a battery, watch the LED reach a state | **NOT RUN** (the code never waits for a host — but rule 11 says show it, don't assert it) |
| 3 | four LED colours, four classes | move the board through calm / fast / loose / shake | **PARTIAL** — colours and boot sweep observed; a clean four-class pass needs ≥4 breathing sessions |
| 4 | BLE notification seen live in a browser | monitor page on GitHub Pages | **BLOCKED** — the second, public Pages repository has not been created |
| 5 | board's own hotspot page | step 9 of `PLAN.md` | **NOT STARTED** |
| 6 | abstention on `novel_*` | step 10 | **NOT STARTED** (the hook exists: `model_margin()`) |
| 7 | inference time per window, free heap | instrument `evaluateWindow()` | **NOT RUN** |
| 8 | `tomasvicar` still a collaborator | `gh api repos/judithazcona/AppII-realtime-edge-devices/collaborators --jq '.[].login'` | **PASS** 15:26 — re-run before the demo (worth 0 points if it lapsed) |
