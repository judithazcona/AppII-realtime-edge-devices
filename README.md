# Real-time AI on a microcontroller — breathing and motion classifier

A wearable-style monitor that runs the whole machine-learning chain on an ESP32-S3: it reads
an **MPU-6050 accelerometer + gyroscope**, extracts features and classifies what is happening
to the board, and drives the board's RGB LED — all on the chip, in real time, with no computer
attached. The same state is also sent over Bluetooth Low Energy to a monitoring web page and
served by the board itself over its own Wi-Fi hotspot.

Group project for *Computer-Aided Medical Diagnostics (MPA-PRM) — Application IV: real-time AI
applications*, a 4-hour graded lab.

## Group

- Judith Azcona García
- Mohammed Sami Furqan
- Yaren Ustaosmanoglu

## The four states

One device recognises all four, shown on the RGB LED:

| LED colour | state |
| --- | --- |
| 🟢 green | calm breathing (sensor on the chest) |
| 🔴 red | fast breathing (sensor on the chest) |
| 🔵 blue | not attached (held in the hand, or lying on the table) |
| 🟣 magenta | shaking |

## Hardware

| Part | Role |
| --- | --- |
| ESP32-S3 N16R8 development board | sensing, features, classification, LED, BLE, web server |
| MPU-6050 (GY-521) module | 3-axis accelerometer + 3-axis gyroscope on I²C |
| Breadboard + jumper wires | wiring |
| USB cable | **power only in the demo** — no data over it, nothing computed on the PC |

![Wiring of the ESP32-S3 N16R8 with the GY-521 on the breadboard](docs/wiring.jpg)

*The GY-521 on the breadboard wired to the ESP32-S3 N16R8: `VCC`→`3V3`, `GND`→`GND`,
`SDA`→GPIO 8, `SCL`→GPIO 9, `AD0`/`INT`/`XDA`/`XCL` left unconnected. The pin-by-pin table,
the USB bridge and the measured board settings live in [`HARDWARE.md`](HARDWARE.md).*

## What it looks like when it runs

```
power bank → ESP32-S3 boots on its own → reads MPU-6050 → window → features → classifier
           → RGB LED: green / red / blue / magenta
           → BLE notify → monitor page (GitHub Pages): state, timeline, alarms
           → own Wi-Fi hotspot → http://192.168.4.1 : live signal + class probabilities
```

- **Monitoring page (BLE, Web Bluetooth):** _TODO — link the GitHub Pages address once the
  second public repository exists (required for the documentation part)._
- **Board's own page:** join the board's hotspot and open `http://192.168.4.1`.

## Grading

| Part | Points |
| --- | --- |
| Movement vs. shaking | 2.5 |
| Breathing (four states) | 2.5 |
| BLE monitor | 1.5 |
| Web page from the board | 1 |
| Own extension | 1.5 |
| Documentation | 1 |
| **Total** | **10** |

Points count only for what is demonstrated live to the teacher.

## Own extension: knowing when it does not know

Instead of always naming a class, the device **abstains** when it is not sure: below a
confidence threshold, or when the movement is far from anything it was trained on, it reports
*unsure*. The number that matters is accuracy against coverage — what fraction of windows it
answers, how often those are right, and how many of its refusals it would have got wrong. The
measurements land here with step 10 of [`PLAN.md`](PLAN.md).

## Repository layout

| Path | Purpose | Status |
| --- | --- | --- |
| `AGENTS.md` | Working rules, including the two compulsory rules that decide the grade | done |
| `PLAN.md` | States, grading, hard constraints, commit tracker, time plan, demo script, open questions | done |
| [`HARDWARE.md`](HARDWARE.md) | **The truth about the wiring**: pin table, USB bridge, board build settings, what is measured vs assumed — kept in step with `firmware/config.h` | done |
| `docs/` | Wiring photo (`docs/wiring.jpg`), figures and notes for the write-up | photo in |
| `firmware/` | ESP32-S3 firmware: sensor, features, model, LED, BLE, web server. `config.h` holds the pins; `bench_test` and `led_test` are the bring-up checks | bring-up running |
| `training/` | Data loading, features, training and evaluation scripts | planned |
| `data/` | Only `data/README.md` — raw recordings stay off the repo (rule 16) | planned |

The raw IMU recordings, extracted windows and model binaries are deliberately **not** in this
repository: it is public, and they are large. `data/README.md` will describe the CSV format and
how to re-record.

## How to run

**Training (PC).** Python runs through [uv](https://docs.astral.sh/uv/), so there is no manual
virtualenv:

```bash
uv add <package>      # install a dependency (updates pyproject.toml + uv.lock)
uv run <script.py>    # run a script inside the project environment
```

**Firmware (board).** Built with [`arduino-cli`](https://docs.arduino.cc/arduino-cli/) and the
`esp32:esp32` core, Arduino-style C++, with **NimBLE-Arduino** for BLE and Adafruit NeoPixel for
the on-board addressable RGB LED. Everything installs into the user folder, no administrator
rights:

```powershell
arduino-cli core install esp32:esp32                              # 3.3.12
arduino-cli lib install "NimBLE-Arduino" "Adafruit NeoPixel"
$FQBN = "esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PSRAM=disabled,CDCOnBoot=default"
arduino-cli compile --fqbn $FQBN firmware/<sketch>
arduino-cli upload  -p COM4 --fqbn $FQBN firmware/<sketch>
```

[`firmware/README.md`](firmware/README.md) holds the authoritative commands — the FQBN above is
the one every build here uses, with each option marked measured or assumed — and
[`HARDWARE.md`](HARDWARE.md) the pins (I²C on GPIO 8/9, `0x68`, LED on GPIO 48).

Every script sets its random seeds from one shared constant, so re-running a training script on
the same recordings reproduces the same metrics.

## Data and method

Each recording is one continuous session of a single class; the **train/test split is assigned
per whole session before any windowing**, so overlapping windows never leak across the split.
Metrics are reported on held-out sessions. Details — window length, features, model, results —
go here as the steps in [`PLAN.md`](PLAN.md) land.

## AI tools used

Allowed under the BUT rules, and we used:

- **Claude Code** — working rules and this repository's documentation, plan and commit
  history, and later code and debugging help. Every group member is expected to be able to
  explain the resulting code, as the assignment requires.

_Detail the exact tools and what each was used for before the demo._

## Assignments and rules

The assignment sheet ("Real-time AI on a microcontroller", MPA-PRM Application IV) is not
committed here — it is course material and a large download. Its compulsory conditions and
grading are restated at the top of [`AGENTS.md`](AGENTS.md) and [`PLAN.md`](PLAN.md).
