# HARDWARE.md — the truth about the wiring

This file is the authority on how the device is wired. It is updated **together with
[`firmware/config.h`](firmware/config.h)**: a pin that appears in one and not the other is a bug.

Wiring as built by hand on the breadboard, 2026-11-09 ~14:15. Photo:
[`docs/wiring.jpg`](docs/wiring.jpg) — 840x1493 JPEG, 93 KB, EXIF stripped so the public repo
publishes no GPS coordinates; the full-resolution original sits at `docs/raw/`, which is
gitignored.

## GY-521 (MPU-6050) → ESP32-S3 N16R8

| GY-521 pin | ESP32-S3 GPIO | Printed label | Purpose | Wire colour |
| --- | --- | --- | --- | --- |
| `VCC` | — | `3V3` | 3.3 V power. **Not 5 V.** | |
| `GND` | — | `GND` | Ground, common with the board | |
| `SDA` | **GPIO 8** | *silkscreen label to read off the board* | I²C data | |
| `SCL` | **GPIO 9** | *silkscreen label to read off the board* | I²C clock | |
| `AD0` | not connected | — | Address select; left floating → I²C address **`0x68`** | |
| `INT` | not connected | — | Data-ready interrupt, unused | |
| `XDA` | not connected | — | Auxiliary I²C (external mag), unused | |
| `XCL` | not connected | — | Auxiliary I²C clock, unused | |

The GY-521 module carries its own I²C pull-ups, so none are added.

## On-board addressable RGB LED

| Item | Value | Status |
| --- | --- | --- |
| LED | the board's **single on-board addressable RGB LED** (WS2812-family), no external LED | confirmed by Judith |
| Data GPIO | **GPIO 48** (`RGB_LED_PIN` in `firmware/config.h`) | ✅ **measured 14:35** — `firmware/led_test` drove GPIO 48 through red, green, blue and magenta and the on-board LED followed; **GPIO 38 stayed dark** |
| Printed label | *to read off the board silkscreen* | not yet recorded |
| Wire | none — on-board | n/a |

## USB and serial

| Item | Value | How it was checked |
| --- | --- | --- |
| USB bridge on the board | **WCH CH9102** (`VID_1A86 PID_55D3`), i.e. an external USB-to-UART chip on **UART0**, *not* the ESP32-S3 native USB | `Get-PnpDevice` instance id `USB\VID_1A86&PID_55D3\5CF8036418` |
| Serial port | **`COM4`** | `arduino-cli board list` → `COM4 serial Serial Port (USB)` |
| Driver needed? | **No** — Windows bound its own driver, device status `OK` | `Get-PnpDevice` status column; no install, no admin rights used |
| Consequence for the build | `Serial` is UART0, so **USB CDC On Boot = Disabled** and upload via UART0 | — |
| Consequence for the demo | the graded demo must push no data over this cable: power only | rule 11 in `AGENTS.md` |

## Board settings used for compilation

| Setting | Value | Checked or assumed |
| --- | --- | --- |
| FQBN | `esp32:esp32:esp32s3` (ESP32S3 Dev Module) | **checked** — `board details` resolves it, core 3.3.12 |
| Chip | ESP32-S3 QFN56 rev v0.2, 240 MHz, **Embedded PSRAM 8MB (AP_3v3)** | **measured** — `esptool --chip esp32s3 -p COM4 flash_id` |
| Flash size | `16M` | **measured** — `flash_id` reported `Detected flash size: 16MB`, so the setting matches the silicon and not just the silkscreen |
| PSRAM | `disabled` | **deliberate** — the chip does have 8 MB on-die, so `PSRAM=opi` is available if we ever need it; nothing in our firmware does, and OPI PSRAM claims GPIO 33–37 |
| Flash mode | `dio` (core default) | **default kept** — QIO is faster but the wrong mode boot-loops; not worth the risk in lab time |
| USB CDC On Boot | `default` (Disabled) | **checked by reasoning** — we are on the CH9102 bridge on UART0, so CDC is not the path |
| Upload / USB mode | defaults (UART0 / Hardware CDC) | same reasoning |

## Not wired / not yet known

- The silkscreen labels next to GPIO 8 and 9, and next to the LED (record them for the
  documentation part).
- Whether GPIO 8 or 9 is shared with anything else on this specific board — no conflict seen
  with UART0 (43/44) or the LED (48), and the bus and the LED both work simultaneously in
  software terms, but they have not yet been exercised in the same sketch.
- Wire colours: deliberately blank in the table until we photograph the build and fill them in.

## Bench evidence

| Check | Command / sketch | Result |
| --- | --- | --- |
| I²C bus scan | `firmware/bench_test` | exactly one device, **0x68** |
| WHO_AM_I (reg `0x75`) | `firmware/bench_test` | **0x68** — genuine MPU-6050, not a clone |
| Live stream | `firmware/bench_test`, 10 s at 100 Hz, ±8 g / ±500 dps | **909 good reads, 0 failed**; gravity 0.98–0.99 g on one axis at rest, gyro near 0 |
| Chip / flash / PSRAM | `esptool --chip esp32s3 -p COM4 flash_id` | ESP32-S3 QFN56 rev v0.2, **16 MB flash**, **8 MB embedded PSRAM** |
| RGB LED data pin | `firmware/led_test` (no Serial at all) | **GPIO 48** lights red/green/blue/magenta; **GPIO 38 dark** — observed by Judith |
