# Use of AI tools in this project

**Tool: Claude Code** (Anthropic's terminal agent), driven interactively by Judith during the lab.
Commits made with its help carry `Co-Authored-By: Claude Code <noreply@anthropic.com>`, so the
history shows exactly which commits are AI-assisted — that is the honest version of the
declaration rather than a sentence claiming "we used AI sometimes".

## What it was used for

| area | what the assistant did | who decided / verified |
| --- | --- | --- |
| Toolchain setup | `uv` project, `arduino-cli` + `esp32:esp32` core install commands, library list, all recorded in `firmware/README.md` | Judith ran and confirmed them on her machine |
| Data recording | `training/record.py` (streaming, header-carried sensor settings, validation, `--probe`/`--list`), `firmware/data_recorder` | **All recordings are the group's own**: Sami and Judith breathed, shook, walked and put the board in pockets. The assistant never invented a sample |
| Feature design | 26 window features in `training/features.py`, mirror in `firmware/features.h`, breathing-band DFT feature | Proposal accepted because it fits the chip (no FFT on-device); effectiveness judged from the held-out confusion matrix |
| Training and export | `training/train.py`: session-level splits, MLP, logistic floor, `model_weights.h` exporter + round-trip check | Metrics read and interpreted by Judith; the "this is weak evidence" caveats are in `RESULTS.md` rather than hidden |
| Firmware | `classifier`, `led_test`, `bench_test`, `debug_pipeline`, BLE service/characteristic, LED state machine | Compile sizes, LED colours and BLE behaviour observed on the board |
| Debugging | root-caused the "stuck at `not_attached`" report with an instrumented build, found the CCCD/scan-response defects and the stretched-window defect | Log excerpts quoted in `TESTING.md` §4; the client-side half is still unverified and says so |
| Documentation | drafted these files from commands it had actually run | Every number is traceable to a command; unknowns are written as TODO, not filled in |

## What it was explicitly not allowed to do

The repo ships the guardrails in [`AGENTS.md`](../AGENTS.md), and they bind the assistant as much
as the people:

* **no invented hardware values** — a pin comes from the silkscreen, the wiring or a bench
  measurement, and the LED pin stayed marked *unknown* until `led_test` proved GPIO 48;
* **no invented results** — a metric is quoted with the command that produced it, and a test that
  was not run is listed under NOT RUN even when that makes the project look incomplete;
* **no secrets, no raw data in the public repo** (rule 20) — recordings stay gitignored,
  Wi-Fi credentials go in a gitignored file with a committed `.example`;
* **no PC-side computation presented as the deliverable** — inference is on the chip (rule 10);
* **nothing irreversible or outward-facing without asking first** — branches, remotes and anything
  published are confirmed with Judith;
* **no silent fallbacks** — if `uv` or `arduino-cli` cannot do something, it stops and asks
  instead of reaching for `pip` or PlatformIO.

## Two things worth naming honestly

* When a flash failed (`Could not open COM4`, 18:30), that is what went into the report — the
  board was not claimed to be running a build that never reached it.
* The weakest results are called weak in the text: two sessions per breathing class means the
  held-out number for calm/fast rests on **one session of one person**, and `not_attached` scores
  26 % held-out recall because a board in a lap pocket looks like a board on a chest.

## Human work the assistant did not do

Wiring the GY-521 on a breadboard and measuring the LED pin; recording every session; choosing the
four classes and their colours; the lab-machine Bluetooth trials; the split policy (whole sessions
first) and the decision to keep `data_recorder` on USB while recording; and the judgement that a
100 % two-class test score with an 82.6 % validation score means the test set was easy, not that
the model is good.
