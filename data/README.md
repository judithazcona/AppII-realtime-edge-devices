# data/ — IMU recordings: format, naming, how to re-record

**Only this README is in the repository.** The recordings themselves are not committed:
the repository is public and the files are large (~90 KB per 12 s, ~330 KB per 45 s
session, and we need ~30 of them). Everything below describes what a recording looks like
so that a session can be re-recorded identically, or a lost dataset rebuilt.

| Directory | Contents | In git |
| --- | --- | --- |
| `data/raw/` | one CSV per recording session, as streamed from the board | **no** (`.gitignore`) |
| `data/windows/` | cut windows (step 5) | no |
| `data/features/` | feature matrix (step 5) | no |
| `data/splits/` | the session → train/val/test assignment (step 5) | no — the *assignment* is committed under `training/`, so the split is reproducible without the data |

## Where the code is

| Piece | Role |
| --- | --- |
| [`../firmware/data_recorder/data_recorder.ino`](../firmware/data_recorder/data_recorder.ino) | reads the MPU-6050 at a fixed 100 Hz and streams CSV rows over USB serial |
| [`../training/record.py`](../training/record.py) | asks for the label, controls the session, writes `data/raw/*.csv` |
| [`../training/config.py`](../training/config.py) | the shared `SEED`, the class list, the paths |
| [`../firmware/config.h`](../firmware/config.h) | sample rate, ranges, scales, window size — **the firmware is the source of truth for these**, and it sends them in its own header, so no number is duplicated in Python |
| [`../firmware/README.md`](../firmware/README.md) | the exact `arduino-cli` compile/upload commands |

## What is on disk today (recorded 2026-11-09, this dataset produced `docs/RESULTS.md`)

`uv run python training/record.py --list` prints this; the metadata columns below are copied from
the CSV headers, not from memory. Split = the committed
[`training/split_assignment.json`](../training/split_assignment.json).

| file (in `data/raw/`, not committed) | s | person | placement | split |
| --- | --- | --- | --- | --- |
| `session001_calm_breathing__calm_sitting` | 60 | sami | breadboard pressed on the sternum, sitting upright | train |
| `session002_calm_breathing__calm_judith` | 60 | judith | breadboard pressed on the sternum, sitting upright | **test** |
| `session001_fast_breathing__fast_sitting` | 60 | sami | same placement, fast shallow breathing | train |
| `session002_fast_breathing__fast_judith` | 60 | judith | same placement, fast shallow breathing | **test** |
| `session002_not_attached__table_still` | 45 | judith | lying completely still on the table, untouched | train |
| `session003_not_attached__hand_loose` | 45 | judith | held loosely in the hand, slow natural movements | train |
| `session004_not_attached__lap_pocket` | 45 | yaren | on the lap / in a jacket pocket, small body movements | **test** |
| `session005_not_attached__pickup_putdown` | 45 | judith | picked up and set down on different surfaces | val |
| `session003_shaking__side_to_side` | 45 | judith | one axis only | train |
| `session004_shaking__wobble_rotation` | 45 | yaren | rotating wobble | train |
| `session001_shaking__gentle_small` | 45 | judith | small gentle shake | val |
| `session002_shaking__vigorous_arm` | 45 | judith | vigorous full-arm shake | **test** |

**12 usable sessions: 4 + 4 + 2 + 2.** Three weaknesses that the numbers in `docs/RESULTS.md`
inherit, in order of damage:

1. **Two breathing sessions each** → one to fit on, one to hold out. Every calm/fast metric rests
   on one session of one person.
2. **`not_attached` in a lap pocket is held out and scores 26 % recall** — the model reads a board
   carried on a person as breathing. What is missing is not a cleverer model: it is more
   `not_attached` takes *on a moving person* (walking, sitting down, adjusting the board), plus
   breathing from more people and a second placement (abdomen, pocket-over-shirt) so "on a person"
   stops being a synonym for "breathing".
3. **Person coverage is uneven**: Sami appears only in the breathing pair; the shaking takes are
   almost all Judith's. "All three members appear in the data" is true; "the model is
   person-independent" is not.

**No usable `novel_*` material exists yet** — the only novel file is a 12 s smoke test
(`usable: no`). The abstention extension of step 10 needs `novel_tapping` / `novel_rolling` /
`novel_walking` recorded for real, and they cannot be invented afterwards.

## Recording protocol

The chest classes were closed to the recorder until the stage-1 (movement vs shaking) data was in
the bag, so a mis-recorded chest take could not contaminate the first demo; they opened afterwards
and are what rows 1–4 above are. Chest specifics: hold or tape the board flat on the sternum,
`record.py` prints the reminder for those two classes, and the subject must not talk or cough —
a cough is a wideband transient that the classifier cannot tell from a shake.

The target list the protocol asks for — the first two rows were met in full, the breathing classes
got 60 s takes and only 2 each, and the `novel_*` rows are still open:

| Class | Sessions | Length each | What to vary |
| --- | --- | --- | --- |
| `not_attached` | **4** | 45 s | **one must be the board lying completely still on the table, untouched** (the assignment warns that a still board is what a motion classifier gets wrong); one loose in the hand; one on the lap / in a jacket pocket; one picked up and set down. Different table, different surface (wood / paper / cloth). |
| `shaking` | **4** | 45 s | gentle small-amplitude; vigorous full-arm; one axis only (side-to-side); rotation / wobble rather than translation. Different strength and rhythm, and never the same shake twice. |
| `novel_tapping` | 1–2 | 45 s | **never trained on** — repeated finger taps on the board or the table beside it |
| `novel_rolling` | 1–2 | 45 s | **never trained on** — rolling the board across the table, slow and fast |
| `novel_walking` | 1 | 45 s | **never trained on** — walking with it in a hand / pocket |

Rules that keep the dataset honest:

- **All three group members should appear in every class.** Set `--person` correctly; one person's
  breathing or shake is not the class, it is that person's shake. Today they do not: Sami is only
  in the breathing pair, so treat person-independence as unproven (see the weaknesses above).
- **Keep each session pure**: one class from `GO` to the end. Transitions at the edges cost
  labels, so start the motion during the countdown, not after it.
- **`walking` and `tapping` stay in the `novel_*` files only.** They are the held-out
  material that shows the device refusing to answer (our extension, step 10). If they end up
  inside a trained class, the extension demo has nothing to refuse. These sessions cannot be
  invented afterwards, which is why they are recorded now even though they are not used until
  step 10.
- **Nothing shorter than 10 s** (the recorder refuses): a 2.5 s window at 50 % overlap needs
  ~14 windows to be more than an anecdote. 45 s gives ~17 windows per session.
- **The split is assigned per whole session before any windowing** (AGENTS.md rule 9).
  Neighbouring windows overlap and are near-duplicates, so windowing first and splitting after
  would leak and inflate the test score. `record.py` cannot enforce this — step 5 assigns
  whole sessions to train / validation / test with the fixed `SEED` and commits the
  assignment list.

## How to record

```powershell
# 1. once per session with the board: flash the recorder
$CLI  = "$env:LOCALAPPDATA\ArduinoCLI\arduino-cli.exe"
$FQBN = "esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PSRAM=disabled,CDCOnBoot=default"
& $CLI compile --fqbn $FQBN firmware/data_recorder
& $CLI upload  -p COM4 --fqbn $FQBN firmware/data_recorder

# 2. check the link without writing anything (3 s, prints the measured rate)
uv run python training/record.py --probe

# 3. record: it asks for class, session name, person, placement, then counts down 3-2-1-GO
uv run python training/record.py

# or fully on the command line, useful for repeating a take
uv run python training/record.py --class shaking --session vigorous_both_hands `
    --person sami --placement "held in both hands, standing" --duration 45

# a held-out novel movement (trainable=no in the file, name starts with novel_session)
uv run python training/record.py --class novel_walking --session corridor_pocket --person yaren

# 4. what do we have so far?
uv run python training/record.py --list
```

Close the Arduino monitor before recording: Windows will not open a COM port twice, and
`record.py` says so rather than failing quietly.

## CSV format

One file per session. UTF-8, LF, three sections:

1. **metadata** — `# key value` comment lines written by the recorder, then the same block
   prefixed `fw_` copied verbatim from the board's own stream header;
2. **one column-name line** (the only non-`#` line before the data);
3. **data** — one row per sample, exactly as the board emitted it, no re-formatting.

Read it with: `lines = [l for l in text.splitlines() if not l.startswith("#")]`, then
`csv.DictReader(lines)`.

### Columns

```
t_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw
```

| Column | Unit | Meaning |
| --- | --- | --- |
| `t_us` | µs | ESP32 `micros()` minus the `B` marker that started this session — **the sample clock is the board's, not the PC's**. A PC-side timestamp would carry OS scheduling jitter and show up later as fake frequency content. |
| `ax_g`, `ay_g`, `az_g` | g | accelerometer, ±8 g range, 4096 LSB/g |
| `gx_dps`, `gy_dps`, `gz_dps` | °/s | gyroscope, ±500 °/s, 65.5 LSB per °/s |
| `ax_raw` … `gz_raw` | LSB counts | the raw 16-bit registers, kept so the physical units can be recomputed if a range assumption ever changes instead of the data becoming wrong |

Both units are in the file on purpose: **g and °/s** because a feature computed on them means
the same thing on any board and the firmware divides by the same constants, and **raw counts**
because they are the thing the chip actually saw.

### Metadata keys

`session_name`, `class`, `trainable` (`yes`/`no`), `held_out`, `led_colour`, `person`,
`placement`, `condition`, `recorded_at` (ISO local), `samples`, `recorded_span_s`,
`measured_rate_hz`, `median_dt_ms`, `gaps_gt_1_75_period`, `abs_accel_max_g`,
`pipeline_seed`, `recorder`, then `fw_rate_hz`, `fw_dlpf_hz`, `fw_accel_full_scale_g`,
`fw_accel_lsb_per_g`, `fw_gyro_full_scale_dps`, `fw_gyro_lsb_per_dps`, `fw_i2c_address`,
`fw_who_am_i`, `fw_window_samples`, `fw_window_stride_samples`, `fw_columns`.

`measured_rate_hz`, `median_dt_ms` and `gaps_gt_1_75_period` are computed **from `t_us`**
when the take ends, not taken from the configured rate: the number of samples per second we
*ask* for and the number the board *delivers* are different claims, and only one of them can
be checked. A take that delivered 71 Hz would silently scale every frequency-domain feature.

## Fixed seeds

`training/config.py` holds `SEED = 20261109` and `seed_everything()`. Recording is not
random — there is no RNG in this path — but the recorder still calls `seed_everything()` and
stamps `pipeline_seed` into every CSV, so a recording carries the pipeline constant that will
later split and train on it, and one constant stays in charge (AGENTS.md rule 8).

## Smoke tests are not data

Files whose name contains `smoke_test` were link checks (the board on the desk for 12 s while
the recorder was written). `record.py --list` marks them `usable: no` and they must be
excluded from training. Delete them whenever you like; they are listed here because a stray
file that looks like data is worse than no file.

## Measured, so you can tell a good take from a broken one

| Check | Value seen on this hardware |
| --- | --- |
| Delivered rate | **100.00 Hz** (297 samples / 2.96 s and 1200 / 11.99 s), median dt **10.00 ms**, p99 **10.01–10.03 ms**, **0 gaps** |
| Board at rest on the table | `az` 0.96–1.00 g, `\|a\|` max 1.01 g, gyro ≈ 0–2.5 °/s bias |
| Compile size of the recorder | 302,681 bytes (23 % of the 1,310,720-byte app partition) |

If a take shows `gaps` > 0 or a rate more than ~5 Hz off 100 Hz, `record.py` prints a WARN —
do not record the real sessions on top of that, find out why first (bus speed, another program
holding the port, a USB hub in the middle).
