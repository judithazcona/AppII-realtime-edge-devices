# Firmware — exact commands to rebuild and flash this device

Copy-paste list of every `arduino-cli` command used on this device, so the board can be rebuilt
on a clean machine without guessing. Written per rule 6 of [`AGENTS.md`](../AGENTS.md): commit the
commands, not just the code.

* Built on Windows 11, **no administrator rights**, everything inside the user folder.
* Toolchain: [`arduino-cli`](https://docs.arduino.cc/arduino-cli/) +
  [`esp32:esp32`](https://github.com/espressif/arduino-esp32) core, Arduino-style C++,
  **NimBLE-Arduino** for BLE, **Adafruit NeoPixel** for the on-board LED.
  **Not** PlatformIO, **not** the Arduino IDE (rule 4).
* Pins and sensor settings come from [`config.h`](config.h) and
  [`HARDWARE.md`](../HARDWARE.md) — never re-hard-coded inside a sketch.

## 0. Where the toolchain lives (user folder only)

| What | Path on this machine |
| --- | --- |
| `arduino-cli` binary | `%LOCALAPPDATA%\ArduinoCLI\arduino-cli.exe` |
| Data dir (cores, packages, indexes, `arduino-cli.yaml`) | `%LOCALAPPDATA%\Arduino15` |
| Sketchbook / libraries | `%USERPROFILE%\Documents\Arduino\libraries` |

Nothing was written to `Program Files`, to Windows, or to the machine-wide PATH. In an
already-open shell the binary is not on `PATH`, so call it by full path:

```powershell
$CLI = "$env:LOCALAPPDATA\ArduinoCLI\arduino-cli.exe"
```

In **Git Bash / WSL** (what the commits in this repo were built with):

```bash
CLI="$LOCALAPPDATA/ArduinoCLI/arduino-cli.exe"     # e.g. /c/Users/<user>/AppData/Local/ArduinoCLI
```

The CLI config is **default** — `arduino-cli config dump` prints only
`board_manager: additional_urls: []`. The ESP32 core comes from the official index, so no
third-party board URL and no committed `arduino-cli.yaml` is needed.

## 1. Install the core and the libraries

```powershell
& $CLI core update-index
& $CLI core install esp32:esp32          # installed 3.3.12
& $CLI lib install "NimBLE-Arduino" "Adafruit NeoPixel"
```

Verified rather than assumed — this is what the machine reports now:

```
$ arduino-cli core list
ID          Installed Latest Name
esp32:esp32 3.3.12    3.3.12 esp32

$ arduino-cli lib list
Name              Installed Available Location Description
Adafruit NeoPixel 1.15.4    -         user     -
NimBLE-Arduino    2.3.9     -         user     -
```

## 2. Board and serial port

```powershell
& $CLI board list                 # -> COM4  serial  Serial Port (USB)
& $CLI board details -b esp32:esp32:esp32s3
```

The board enumerates as a **WCH CH9102** USB-to-UART bridge (`VID_1A86 PID_55D3`) on **UART0**,
not as the ESP32-S3 native USB. Windows already had a driver — no install, no elevation. Full
table in [`HARDWARE.md`](../HARDWARE.md).

## 3. The FQBN — and which of those settings are measured

Every compile and upload in this lab uses exactly this one string:

```
esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PSRAM=disabled,CDCOnBoot=default
```

| Option | Value | Status |
| --- | --- | --- |
| board | `esp32s3` (ESP32S3 Dev Module) | **checked** — `board details` resolves it on core 3.3.12 |
| `FlashSize` | `16M` | **measured** — `esptool flash_id` reported `Detected flash size: 16MB`, so the setting matches the silicon, not just the silkscreen |
| `FlashMode` | `dio` | **core default kept** — deliberate: QIO is faster, a wrong mode boot-loops, and a boot-loop costs demo time |
| `PSRAM` | `disabled` | **deliberate, not assumed-blind** — `flash_id` shows the N16R8 does carry 8 MB on-die PSRAM, so `PSRAM=opi` is available; nothing in this firmware needs it, and OPI PSRAM occupies GPIO 33–37 |
| `CDCOnBoot` | `default` (= Disabled) | **checked by reasoning** — `Serial` is UART0 behind the CH9102, so USB CDC is not the path; enabling CDC on a UART0-only board risks a board that only talks when a host is attached, which breaks rule 11 |
| `UploadSpeed`, `USBMode`, `PartitionScheme` | core defaults | **assumed** — upload at the default UART0 speed works, so no reason to change it |

The chip itself was read once, which is where the flash and PSRAM rows come from:

```powershell
& "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe" `
    --chip esp32s3 -p COM4 -b 921600 flash_id
# -> ESP32-S3 QFN56 rev v0.2, 16 MB flash, 8 MB embedded PSRAM (AP_3v3)
```

## 4. Compile

```powershell
$FQBN = "esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PSRAM=disabled,CDCOnBoot=default"
& $CLI compile --fqbn $FQBN firmware/bench_test
& $CLI compile --fqbn $FQBN firmware/led_test
```

Both commands were **re-run verbatim** just before this file was committed, so the sizes below
describe the documented command and not a remembered one:

```
bench_test   Sketch uses 303597 bytes (23%) of program storage space. Maximum is 1310720 bytes.
             Global variables use 23048 bytes (7%) of dynamic memory, leaving 304632 bytes.
led_test     Sketch uses 290130 bytes (22%) of program storage space. Maximum is 1310720 bytes.
             Global variables use 22208 bytes (6%) of dynamic memory, leaving 305472 bytes.
```

Plenty of room left in the 1.31 MB partition for NimBLE and the web server; if a later step needs
more, that is a `PartitionScheme` change, currently at the core default.

## 5. Upload

```powershell
& $CLI upload -p COM4 --fqbn $FQBN firmware/led_test
# stty-style auto-reset through the CH9102; if the board never resets, hold BOOT, tap EN,
# release BOOT, then retry.
```

## 6. Read the serial output

`Serial` runs at **115200 baud**.

```powershell
& $CLI monitor -p COM4 -c baudrate=115200
```

**Gotcha that cost time:** `arduino-cli monitor` **prints nothing when its output is piped or
redirected** — it needs a TTY. For scripted capture (that is how the bench numbers in
`HARDWARE.md` were captured) open the port directly and pulse RTS to auto-reset the board:

```powershell
$p = New-Object System.IO.Ports.SerialPort COM4,115200,None,8,One
$p.Open(); $p.RtsEnable = $true; Start-Sleep -m 100; $p.RtsEnable = $false
$deadline = (Get-Date).AddSeconds(15)
while ((Get-Date) -lt $deadline) { Write-Output $p.ReadLine() }
$p.Close()
```

## 7. Sketches in this folder

| Sketch | Uses `Serial`? | Purpose | Proof it gave |
| --- | --- | --- | --- |
| [`bench_test/`](bench_test/bench_test.ino) | **yes — the only sketch allowed to** (rule 7) | I²C scan, `WHO_AM_I`, 10 s accel/gyro table | one device at `0x68`; `WHO_AM_I` = `0x68` (genuine MPU-6050); 909 good reads / 0 failed at 100 Hz; gravity 0.98–0.99 g at rest |
| [`led_test/`](led_test/led_test.ino) | **no** | drives red, green, blue, magenta on GPIO 48, then tries GPIO 38, forever, on power alone | the on-board LED lit on **GPIO 48**; GPIO 38 stayed dark — observed by Judith at 14:35 |
| [`data_recorder/`](data_recorder/data_recorder.ino) | **yes — the second and last exception** (rule 7) | streams accel + gyro at a fixed 100 Hz as CSV rows for the dataset | **100.00 Hz delivered** (1200 samples in 11.99 s), median dt 10.00 ms, p99 10.01 ms, **0 gaps** |
| [`classifier/`](classifier/classifier.ino) | **no** | the graded firmware: 100 Hz sampling → 24 features → exported MLP → smoothed decision → LED colour | compiles to **323042 bytes (24 %)** flash, **29664 bytes (9 %)** RAM; `grep -c Serial` reads **0**; not uploaded yet, `COM4` was held by the recorder |
| [`config.h`](config.h) | — | pins, I²C address, full-scale ranges, sample rate, window size | shared by every sketch; paired with `HARDWARE.md` |
| [`features.h`](features.h) / [`model_infer.h`](model_infer.h) | — | the 24 window features and the MLP forward pass, in C | same formulas as `training/features.py`; included by `classifier` only |

`led_test` is deliberately written with **zero** `Serial` calls: it is the standing check that
this firmware family reaches its working state from a power bank with no host attached
(rule 11). `classifier` is written with zero as well, and that is not a style choice: it is the
demo sketch, so the absence of the token is what proves the cable can stay unplugged.

### 7.1 `classifier/` — the graded build

```powershell
& $CLI compile --fqbn $FQBN firmware/classifier   # 323042 bytes (24%), 29664 bytes (9%) RAM
& $CLI upload  -p COM4 --fqbn $FQBN firmware/classifier   # NOT RUN YET: COM4 held by record.py
```

The chain, all on the chip: MPU-6050 at 100 Hz → 250-sample window (2.5 s) re-run every 125
samples → the 24 features from `features.h` → `log1p` and standardisation with the parameters
`train.py` fitted on **train sessions only** → the MLP in `model_infer.h` → two agreeing
windows in a row → one colour. `config.h` supplies the pins and the rate, `model_weights.h`
supplies the class names, and the sketch maps names to colours, so re-exporting a 4-class model
needs no edit here.

Three `static_assert`s refuse to build if `config.h` and the exported header disagree about the
window, the stride, the sample rate or the feature count — the drift that would otherwise show
up as "the board disagrees with the notebook".

If the sensor stops answering (cable pulled, I²C dead), the LED blinks **amber** at ~1.7 s
period instead of freezing on the last state. There is no serial to complain into, so the LED
carries that message too.

## 8. Recording dataset sessions (cable attached on purpose)

```powershell
& $CLI compile --fqbn $FQBN firmware/data_recorder   # 302681 bytes (23%), 23048 bytes RAM
& $CLI upload  -p COM4 --fqbn $FQBN firmware/data_recorder

uv run python training/record.py --probe             # 3 s, writes nothing, prints the real Hz
uv run python training/record.py                     # asks for label + name, counts down, records
uv run python training/record.py --list              # what we have, and what still counts
```

Close the Arduino monitor first — Windows will not open `COM4` twice, and `record.py` says so
out loud instead of timing out.

The recorder answers two one-letter commands so the PC can drive a session without resetting
the board: **`H`** re-sends its header (rate, ranges, scales, column order — printed once at
boot, which a recorder connecting later would otherwise never see) and **`B`** zeroes `t_us`
and the counters so a CSV starts at zero. Sample timestamps come from the board's own
`micros()`, never from the PC clock.

This is a **data-collection tool**, the second and last exception to rule 7. The graded
firmware must still run from a power bank with no data crossing the cable (rule 11). The CSV
format, the per-class recording protocol and the re-record procedure are in
[`../data/README.md`](../data/README.md); recordings stay in `data/raw/`, which is gitignored.

## 9. Rebuild from scratch on a clean machine

1. Install `arduino-cli` into `%LOCALAPPDATA%\ArduinoCLI` (no admin).
2. Sections 1–2 above (core 3.3.12, both libraries, confirm the COM port).
3. Check the pins in [`config.h`](config.h) against the actual wiring in
   [`HARDWARE.md`](../HARDWARE.md) — GPIO 8/9 for I²C, GPIO 48 for the LED.
4. Section 4 + 5 with the FQBN from section 3.
5. Flash `led_test` and run it **from a power bank**: the LED should cycle
   red → green → blue → magenta with no cable attached to a computer.
