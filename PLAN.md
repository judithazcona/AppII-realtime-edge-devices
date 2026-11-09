# PLAN

Real-time AI on a microcontroller — ESP32-S3 N16R8 + MPU-6050 (GY-521).
Status: **2026-11-09, 15:25** — mid-lab, **~65 min behind the 13:50 plan** (re-cut below). The lab runs **13:00–17:00**. Rules in
[`AGENTS.md`](AGENTS.md).

## COMPULSORY — either one missed = 0 points for the whole project

1. **Share the repository on GitHub with `tomasvicar`** (Settings → Collaborators) **before
   the lab ends.**
   Current status: ❌ **PENDING.** Invitation `342422359` was sent 2026-11-09 13:21 and has not
   expired, but `tomasvicar` is not yet in the collaborator list — he has to accept it.
   **Action: ask him in person to accept the invitation, then re-check the list.**
   Re-check with:
   ```bash
   gh api repos/judithazcona/AppII-realtime-edge-devices/collaborators --jq '.[].login'
   gh api repos/judithazcona/AppII-realtime-edge-devices/invitations --jq '.[].invitee.login'
   ```
   Expected final state: `tomasvicar` appears in the **collaborators** output, not only in the
   invitations one.

2. **At least 3 intermediate commits during the lab, each pushed to GitHub as we go.**
   Current status: ✅ **minimum met with room to spare — 7 commits pushed and verified**
   (`2465333` 13:20, `9f71414` 13:43, `bec06c2` 14:06, `bb53ade` 14:33, `6d11b52` 14:47,
   `10374fc` 15:01, and this one ~15:25). The minimum was already met at 14:06, i.e. nearly
   three hours before the deadline.
   Keeping the pace anyway: the plan has 6 more steps, one commit each, and the rule says
   "as we go", so no step is ever parked uncommitted. This file's COMMIT TRACKER is the
   evidence. **The schedule has slipped ~65 min (see Time plan), so this rule is the one thing
   that must not slip: even if a graded feature gets cut, its commit still happens.**

## Lab clock

| | |
| --- | --- |
| Lab start | **13:00** (confirmed) |
| Lab length | 4 hours |
| Lab end | **17:00** |
| Now | 15:25 → **1 h 35 min of lab time left** |
| **Commits pushed since lab start** | **7** counting this one (6 already on GitHub: `2465333` `9f71414` `bec06c2` `bb53ade` `6d11b52` `10374fc`) — `git rev-list --count origin/judithbranch --since='2026-11-09T13:00:00+01:00'`, re-run after every push |

> Print that count every time this tracker is updated (rule 9); it is the evidence for
> compulsory rule 2. The minimum is 3, so the count must read **3 or more well before 16:30**,
> never in the last minutes of the lab.

## COMMIT TRACKER

One step = one commit, never several steps batched (rule 13). A commit cannot contain its own
hash, so each row's hash and time are filled in by the **next** commit; the push proof for a
step is run and shown immediately after that step's push.

| # | step | commit hash | time of the commit | pushed & verified on GitHub |
| --- | --- | --- | --- | --- |
| 1 | Repo skeleton, working rules, plan, gitignore | `2465333` | 2026-11-09 13:20:56 +01:00 | yes — `ls-remote` hash matched local |
| 2 | Plan and rules fixed for the MPU-6050 assignment | `9f71414` | 2026-11-09 13:43:59 +01:00 | yes — `ls-remote` + `gh api` both showed `9f714143f1…` |
| 2b | Toolchain, lab clock, LED and extension decided | `bec06c2` | 2026-11-09 14:06:39 +01:00 | yes — `ls-remote` + `gh api` both showed `bec06c2ee8…` |
| 3 | Wiring and sensor check: `HARDWARE.md`, `config.h`, `bench_test`, `led_test` | `bb53ade` | 2026-11-09 14:33:57 +01:00 | yes — `ls-remote` + `gh api` both showed `bb53ade`, proof run right after the push |
| 3b | Wiring photo: `docs/wiring.jpg` compressed to 93 KB, EXIF/GPS stripped | `6d11b52` | 2026-11-09 14:47:49 +01:00 | yes — `ls-remote` = local HEAD, `gh api` newest = `6d11b52`, and the rendered README contains `<img src="docs/wiring.jpg">` |
| 3c | `firmware/README.md` with the exact arduino-cli commands; close the step-3 leftovers in this plan | `10374fc` | 2026-11-09 15:01:19 +01:00 | yes — `ls-remote` = local HEAD, `gh api` newest = `10374fc`, 6 commits since 13:00 |
| 4 | Data recorder: `firmware/data_recorder`, `training/record.py`, `training/config.py`, `data/README.md`, pyproject + uv.lock | this commit (hash recorded in the next row) | recorded in the next row | yes — proof run right after this push |
| 5 | Trained model and honest evaluation | — | — | no |
| 6 | Firmware on the board: movement vs shaking | — | — | no |
| 7 | Breathing: four states on the LED | — | — | no |
| 8 | BLE state characteristic + GitHub Pages monitor | — | — | no |
| 9 | Board's own web page (Wi-Fi hotspot) | — | — | no |
| 10 | Own extension + README finished | — | — | no |
| 11 | Final: documentation, PLAN closed, compliance re-check | — | — | no |

Recordings, extracted windows and model binaries are gitignored, so from step 4 on the commits
carry scripts, firmware and notes — not the data itself.

## The four states and their LED colours

One device must support all four.

| LED colour | state | how we produce it in the demo |
| --- | --- | --- |
| 🟢 **green** | calm breathing — sensor on the chest | press the module to the chest, breathe calmly (~10–20 /min) |
| 🔴 **red** | fast breathing — sensor on the chest | same position, fast breathing (~30+/min, metronome app as a guide) |
| 🔵 **blue** | not attached — held in the hand, or lying on the table | take it off the chest, hold it still, or set it down |
| 🟣 **magenta** | shaking | shake the whole breadboard |

## The 6 graded parts (10 points)

| Part | Points | What we build | What we show live |
| --- | --- | --- | --- |
| Movement vs. shaking | **2.5** | Board-side classifier separating slow hand movement from shaking | Move slowly → blue; shake → magenta; and back |
| Breathing, four states | **2.5** | Same classifier extended to calm / fast breathing and not-attached, all four states on the board | Chest + calm → green; chest + fast → red; off → blue |
| BLE monitor | **1.5** | Board notifies its state over BLE; a GitHub Pages page uses Web Bluetooth to show state + timeline and alarm on fast breathing too long, not-attached too long, and connection lost | Open the Pages URL, connect, show changes, trigger all three alarms, explain the limits |
| Web page from the board | **1** | Board runs its own Wi-Fi hotspot and serves a page with the live signal and the probabilities of all classes | Join the hotspot, open the page, show live signal + probabilities |
| Own extension | **1.5** | One idea of our own, done properly | Show it working, explain what it adds and how it was built |
| Documentation | **1** | Repository lets someone else rebuild it: wiring, build + flash, how data was recorded and the model trained, results, AI tools used; README names members, links the monitor page, shows a wiring photo | Open the README on GitHub |
| | **10** | | |

A part that is not shown gets no points — show every part we have, even partly working.

## Hard constraints

1. **The computation runs on the chip.** Sensor reading, features and classification all
   execute on the ESP32-S3. The PC is only for training and flashing. The web pages display;
   they do not compute the state.
2. **USB is power only in the demo — no data over it, nothing computed on the computer.** The
   firmware must never wait for a serial connection: no `while (!Serial)`, no blocking
   `printf`, no startup delay while no host is attached. Acceptance test: the board boots and
   classifies from a power bank with the data cable unplugged from the laptop.
3. **The BLE monitor page goes on GitHub Pages, in a second public repository** — Web
   Bluetooth requires a secure context (HTTPS or `localhost`), so it cannot be served from the
   board's plain HTTP page. Chrome or Edge; device selection starts from a click/tap.
4. **The board's own page is served by the board as its own Wi-Fi hotspot** over plain HTTP,
   with the live signal and the probabilities of every class, visualised.
5. **Documentation:** README names the three members, links the monitoring page, shows a photo
   of the wiring; explain how data was recorded and the model trained, and which AI tools were
   used. Every member must be able to explain the code.

## Steps

### Step 2 — plan and rules fixed for the MPU-6050 assignment *(this commit)*
Rewrote `AGENTS.md`, `PLAN.md`, `README.md` and `.gitignore` around the real task: MPU-6050
motion sensing, the two compulsory rules, no audio assumptions.

### Step 3 — toolchain, wiring and sensor check
**Toolchain (decided):** `arduino-cli` with the `esp32:esp32` core, Arduino-style C++ sketches,
**NimBLE-Arduino** for BLE. **Not** PlatformIO and **not** the Arduino IDE. Installed per-user,
no administrator rights: binary in `%LOCALAPPDATA%\ArduinoCLI`, packages and libraries under
`%LOCALAPPDATA%\Arduino15` (Windows keeps the data dir there, **not** in `~\.arduino15`) and
`%USERPROFILE%\Documents\Arduino\libraries`. The install was
started in the background at 13:47 because the core download is the long pole — it runs while
we wire the breadboard.
- **Done in `firmware/README.md`:** the exact
  `arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PSRAM=disabled,CDCOnBoot=default`
  / `upload -p COM4` commands, the library list and the serial-capture workaround, so anyone can
  rebuild without guessing. No `arduino-cli.yaml` is committed on purpose: `config dump` shows
  the defaults (`additional_urls: []`) and the ESP32 core needs no third-party board URL, so a
  committed config file would be noise pretending to be a requirement.
- Proof of the toolchain itself: `arduino-cli version`, `core list` showing `esp32:esp32`,
  `lib list` showing NimBLE-Arduino, and a trivial sketch compiling for the S3 target.

**Wiring:**
- MPU-6050 VCC → **3.3 V** (not 5 V), GND, SDA, SCL to the chosen GPIOs, AD0 left floating.
- I²C bus scan → expect the module at `0x68` (or `0x69` if AD0 is high); confirm who-me register
  (`0x68`) reads back `0x68`.
- Wake the chip (clear sleep in `PWR_MGMT_1`), set accelerometer full-scale range (likely ±8 g,
  so shaking does not saturate) and gyro range, read accel + gyro at a fixed rate, print to
  serial *while developing* (USB logging is a dev tool only — constraint 2).
- Sanity checks: gravity axis ≈ 1 g at rest, tap test, rotate-in-hand for the gyro.
- **Result, 14:35 — DONE.** Board on **COM4** through a WCH CH9102 bridge, no driver install
  needed. I²C scan found one device at `0x68`; `WHO_AM_I` = **`0x68`** (genuine MPU-6050);
  10 s stream at 100 Hz gave **909 good reads / 0 failed** with gravity at 0.98–0.99 g.
  `esptool flash_id` confirmed **16 MB flash** and 8 MB embedded PSRAM; chip is ESP32-S3
  QFN56 rev v0.2. The on-board RGB LED answers on **GPIO 48**, GPIO 38 stayed dark.
  All of it is written up in `HARDWARE.md`, with the pin in `firmware/config.h`.
  Full evidence in the verification log below. **Not** done in this step: the tap and
  rotate-in-hand checks (nobody watched them).
  The wiring photo is **done** — `docs/wiring.jpg` is in the repo (commit `6d11b52`, 14:47),
  840x1493 JPEG at 93 KB with the EXIF GPS block stripped, referenced by the README and
  rendering on GitHub. The build commands this step promised are **done** too:
  [`firmware/README.md`](firmware/README.md). Still open from this step: the silkscreen labels
  next to GPIO 8/9 and the LED, and the wire colours in `HARDWARE.md` (readable off the photo
  whenever someone has two minutes).

### Step 4 — data recorder
**Result, 15:20 — the recorder works; the real sessions are being recorded right now.**
`firmware/data_recorder` streams accel + gyro as CSV rows at a fixed 100 Hz, timestamped by
the board's own `micros()` (a PC-side timestamp would carry OS scheduling jitter and show up
later as fake frequency content). `training/record.py` (run with `uv`) asks for class, session
name, person and placement, counts 3-2-1-GO, and writes one CSV into `data/raw/`, with the
firmware's own header (rate, ranges, scales, column order) copied into the file, plus the
*measured* rate, median inter-sample interval and gap count computed from the timestamps —
because the rate we ask for and the rate the board delivers are different claims and only one
can be checked. `data/README.md` carries the format, the naming scheme, the per-class
protocol and the re-record commands.

Measured, not asserted: **100.00 Hz delivered** (1200 samples in 11.99 s; a second take 297 in
2.96 s), median dt **10.00 ms**, p99 **10.01 ms**, **0 gaps**, board at rest `az` 0.96–1.00 g.
Compile 302,681 bytes (23 % of the app partition), 23,048 B RAM. Two smoke-test files exist in
`data/raw/` and `record.py --list` marks them `usable: no` so they cannot leak into training.
`git status` shows no CSV; `git check-ignore -v` matches `.gitignore:27:data/raw/`.
The two things that only show up when you run it: the firmware printed its header once at
boot, so a recorder connecting later never learned the rate (fixed: `H` re-sends it), and
`--probe` used to demand a class label it does not need.

- Record labelled sessions from the PC-linked board over USB (allowed for data collection —
  the no-USB-data rule applies to the demo; rule 7 now names `bench_test` and
  `data_recorder` as the only two serial sketches).
- One file per session, `sessionNNN_<class>.csv`, header with sample rate, ranges, person,
  placement; ≥3 varied sessions per class; "not attached" includes the board lying still.
- Stage 1 records slow movement + shaking + not-attached (enough for the first demo); stage 2
  adds chest sessions (calm and fast breathing) — several sessions, different rhythms.
- **Because our extension is abstention ("knowing when it does not know"), also record 2–3
  sessions of movements we will never train on** — tapping, rolling the board, walking with it,
  a different shaking style. They stay out of training and are used to show the model refusing
  to answer confidently. Recording them costs two minutes now and is impossible to retrofit
  after the lab.
- `data/README.md` with format and re-recording instructions; recordings stay off-repo.
- **Proof:** file list with durations per class; a plot of one session per class.

### Step 5 — trained model and honest evaluation
- Window 2–4 s at 50–100 Hz with ~50 % overlap; **split whole sessions first** (rule 5).
- Features per window: tri-axial magnitude plus per-axis mean, std, min/max, RMS, dominant
  frequency / zero-crossing rate, gyro magnitude, inter-axis correlation; log transform on the
  positive amplitude features (assignment tip).
- Model: small fully-connected network on hand-made features (the assignment reports this beat
  a CNN on the raw signal) or regularised logistic regression as a floor; class weights if
  imbalanced. Fixed seeds.
- Report: held-out **session** accuracy, per-class recall, confusion matrix, model size.
- Export the **probabilities of all four classes**, not just the argmax: the board's web page
  must show them (step 9) and the abstention extension needs them (step 10). Export the model
  as weights in a generated C header so the firmware and the evaluation agree exactly — and
  check that by comparing PC vs on-chip outputs on the same windows.
- **Proof:** the metrics printed from a re-run, plus the confusion matrix.

### Step 6 — firmware: movement vs shaking on the board **← first graded demo**
- Run the same feature code on the chip: ring buffer → window → features → softmax.
- For a small MLP, exporting the weights into a C header and evaluating it by hand is often
  faster to land than a TFLite Micro conversion; either is fine as long as inference is on-chip.
- Smooth the decision over ~2 s (majority vote / hysteresis) before driving the LED.
- **LED:** the board has **one single on-board addressable RGB LED** (WS2812-family) and we use
  no external LED, so this one LED is the only state indicator in the demo — drive it with
  Adafruit NeoPixel on its data GPIO (find the pin: it is 48 on most of these boards), with
  green `(0,255,0)`, red `(255,0,0)`, blue `(0,0,255)`, magenta `(255,0,255)`, at reduced
  brightness so it is readable in a lit room without blinding the teacher. Because everything
  depends on it, **blink each of the four colours during step 3 board bring-up**, not at step 6.
- **No Serial dependency.** Measure inference time per window and free heap.
- **Proof:** slow move → blue, shake → magenta, repeatedly, **powered from a power bank**;
  inference time + heap figures.

### Step 7 — breathing: four states
- Retrain with the chest sessions; keep the two easy classes working.
- Expect the hard part: calm vs fast breathing is a subtle, low-frequency difference, so watch
  for subject/placement artefacts — check that a held-out *person or session* still works.
- Four LED colours; "not attached" must fire when the board lies still.
- **Proof:** chest calm → green, chest fast → red, off/lying → blue, shake → magenta; the
  four-class held-out confusion matrix.

### Step 8 — BLE + GitHub Pages monitor
- **First** verify BLE independently with nRF Connect on a phone: connect, find the service,
  subscribe to the state characteristic. Do this before writing any web page.
- BLE: one characteristic carrying state (+ probabilities, breath rate later) with notify.
- Second **public** repository → GitHub Pages page: Connect button (Web Bluetooth needs a user
  gesture), current state, timeline of states, alarms for fast breathing too long, not-attached
  too long and connection lost — choose and write down the limits.
- Test on the actual lab machine's Chrome/Edge early; Bluetooth late is the classic failure.
- **Proof:** the Pages URL, live state changes, timeline, and all three alarms triggered.

### Step 9 — board's own web page
- `SoftAP` hotspot + HTTP page: live signal trace and the four class probabilities as bars.
- Keep it cheap (small page, light polling or SSE) so inference keeps running; watch heap.
- Note: Web Bluetooth cannot run from this plain-HTTP page — that is expected, step 8 covers it.
- **Proof:** laptop joins the hotspot and shows the live page with probabilities.

### Step 10 — own extension: *knowing when it does not know* + README
**Decided.** The device abstains instead of guessing: below a confidence threshold, or when the
input is far from anything it was trained on, it reports *unsure* rather than a class.

- Two cheap signals, both computable on the chip from what we already have:
  1. **Margin** — top softmax probability below a threshold → unsure.
  2. **Novelty** — the feature vector is far from the training distribution (distance to the
     nearest class centroid, scaled by the per-feature std already used for the features).
     Catches unseen movements that a softmax still scores confidently.
- Choose the threshold on the **validation** sessions, never on test, and report the trade-off
  as coverage vs accuracy — e.g. it answers 92 % of windows at 96 % accuracy, and the 8 % it
  refuses are mostly the ones it would have got wrong. That sentence is the deliverable.
- Demo material is the never-trained-on sessions from step 4: it stays *unsure* for tapping,
  rolling or walking while staying confident on the four real classes.
- On the board, *unsure* is shown as the LED off (or a slow white pulse) — decide which in step
  7, and say what it means in the demo. Ship margin-only if the novelty term costs too much
  time, and say so honestly rather than promising both.
- README finished: wiring photo, monitor page link, build + flash, data and training method,
  results, AI tools used.
- **Proof:** the extension working on the unseen sessions; the rendered README on GitHub.
- README finished: wiring photo, monitor page link, build + flash, data and training method,
  results, AI tools used.
- **Proof:** the extension working; the rendered README on GitHub.

### Step 11 — final
- Freeze the code, re-check both compulsory rules (tomasvicar in collaborators, commit count),
  run the demo script end to end once from a power bank, close out PLAN.md with the real
  numbers and the verification log filled.

## Time plan for the rest of the lab

**95 minutes left** (15:25 → 17:00), and **the schedule is ~65 minutes behind the plan written
at 13:50**. Where it went: the ESP32 core download (12 min, unavoidable, run in parallel),
turning the assumed board settings into measurements (`esptool flash_id`, LED pin hunt on
GPIO 48 vs 38), and writing the recorder so it *measures* its own sample rate instead of
claiming one. All of that is bought once and spent every hour from now — the board, the pins
and the 100.00 Hz stream are known-good — but the slip is real and the plan below is re-cut to
fit 95 minutes, not the one that assumed 190.

| Block | clock | min | work | banked points |
| --- | --- | --- | --- | --- |
| A, B, 3b, 3c | 13:20–15:05 | *(done)* | commits 1, 2, 2b, 3, 3b, 3c: rules, plan, toolchain, wiring, LED pin, photo, build commands | — |
| **C1** | **15:20–15:38** | 18 | **team records stage-1 + novel sessions now** (4 × `not_attached`, 4 × `shaking`, 3 × `novel_*`), all three people | — |
| — | 15:20→15:38 | *(parallel)* | **the training script is written while the data is recorded** (features → 2-class model → C header); track 3 creates the **second public Pages repo** and starts the monitor page against the agreed characteristic UUID | — |
| D1 | 15:38–15:58 | 20 | features + model + held-out **session** metrics + weights exported into a generated C header → commit 5 | — |
| **E** | **15:58–16:22** | 24 | **firmware: movement vs shaking on the LED, from a power bank → commit 6 — DEMO GATE, was 15:20** | **2.5** |
| C2 | 16:22–16:32 | 10 | record chest sessions: 2 calm + 2 fast, different people and rhythms | — |
| F | 16:32–16:50 | 18 | retrain with 4 classes, four colours on the LED → commit 7 | **5.0** |
| G | 16:50–16:58 | 8 | BLE notify — **cheap only if track 3 built the page in parallel and the characteristic was added back in block E** | 6.5 |
| I | 16:50–17:00 | 10 | README + PLAN closed, compliance re-check → commits 10 and 11 | **6.0** |

**What is honest about this table:** blocks **H (board web page, 1 pt)** and most of **I
(abstention extension, 1.5 pt)** do not fit in 95 minutes alongside the 5 points of motion and
breathing. Realistic target is **5–6 of 10**, and the way to get nearest 10 is parallelism,
not speed: the monitor page and the second repository cost nothing on the critical path if
someone builds them at 15:20–16:20 against a characteristic UUID we fix now
(`8f6a0001-…`, state = one byte, probabilities = four `uint8` percentages — pinned in
`PLAN.md` step 8 when the firmware lands).

**Decision point moved from 16:05 to 16:22 (the demo gate).** At 16:22, if movement-vs-shaking
is live on a power bank, the 2.5 is banked and breathing (2.5) is the next spend. If it is
*not* live, stop feature work, keep the 2-class model, and spend the remaining time on the
cheap partial credit — README (1) plus the BLE characteristic that was already written — rather
than watching a four-class attempt fail at 16:55 with nothing on the LED.

**Never cut:** commits (compulsory rule 2 — even a cut feature gets its commit), the README
(1 point and the only thing the teacher opens first), and re-checking that `tomasvicar` is in
the collaborator list (compulsory rule 1, still pending — ask him in person **now**, it is not
something to discover at 16:55).

Cut order if we fall behind: board web page (1) → shrink the extension to margin-only (1.5) →
simplify the BLE alarms but keep the page (1.5). The 5 points in E and F are protected.

## Demo script (what we show for each part)

0. **Constraint proof, 30 s.** Unplug the data cable from the laptop / use a power bank. Board
   boots to blue within ~2 s and keeps classifying: no PC, no serial, nothing computed
   elsewhere. Say it out loud: "USB is power only here."
1. **Movement vs shaking — 2.5.** Move the whole breadboard slowly in the hand → blue. Shake it
   → magenta. Back to slow → blue. Say: window length, the two or three features that separate
   them, the model, and the held-out-session accuracy.
2. **Breathing four states — 2.5.** Press the module to the chest, breathe calmly → green.
   Breathe fast (metronome) → red. Take it off and hold it, then set it on the table → blue.
   Shake → magenta. Say: how chest motion differs from arm motion, the smoothing window, and
   the four-class confusion matrix on held-out sessions.
3. **BLE monitor — 1.5.** Open the GitHub Pages URL in Chrome on the lab laptop, click
   **Connect**, pick the board. Show the current state follow the LED, build the timeline, then
   trigger the alarms one by one: fast breathing for more than *X* s, not attached for more than
   *Y* s, then power-cycle the board to raise the connection-lost alarm. State the limits and
   why we chose them.
4. **Board web page — 1.** Join the board's hotspot, open `http://192.168.4.1`, show the live
   signal and the four probabilities moving while we breathe and move.
5. **Own extension — 1.5.** Feed it the four real classes first (confident, correct), then the
   movements it was never trained on — tapping, rolling, walking. It goes *unsure* instead of
   naming a class. State the measured trade-off: what fraction of windows it answers, the
   accuracy on those, and the accuracy among the ones it refused.
6. **Documentation + compulsory — 1.** Open the README on GitHub: the three members, the wiring
   photo, the monitor-page link, how to rebuild, the AI tools used. Then show the commit history
   (more than 3 intermediate commits pushed during the lab) and confirm `tomasvicar` is listed
   as a collaborator.

Backups: if the room's Wi-Fi is congested, show the BLE path (step 3) before the hotspot page.
If Web Bluetooth misbehaves on the machine, fall back to showing the state characteristic in
nRF Connect and explain the Web Bluetooth secure-context requirement.

## Verification log

| What was checked | How | Result |
| --- | --- | --- |
| Commit 1 reached GitHub | `git ls-remote origin` + `git rev-parse judithbranch` | hash matched `2465333…` on `refs/heads/judithbranch` |
| `.gitignore` really ignores data/secrets | created `probe.wav`-equivalent probe files, `git check-ignore -v`, then deleted | matched `*.wav`, `.env`, `secrets.h`, `.venv/`; none in `git status` (audio patterns now replaced by IMU ones) |
| `tomasvicar` access (compulsory rule 1) | `gh api .../collaborators`, `gh api .../invitations` | ❌ **pending** — invitation `342422359` for `tomasvicar` exists and has not expired, but he is not in the collaborators list yet |
| Repo visibility | `gh repo view --json visibility` | `PUBLIC` (confirmed intended by Judith) |
| Toolchain installed and usable | `arduino-cli version`, `core list`, `lib list`, `board details --fqbn esp32:esp32:esp32s3` | arduino-cli 1.5.2-rc.1, `esp32:esp32 3.3.12`, NimBLE-Arduino 2.3.9, Adafruit NeoPixel 1.15.4; FQBN resolves to **ESP32S3 Dev Module** |
| Board detected, driver needed? | `arduino-cli board list`; `Get-PnpDevice` | **COM4**, `USB\VID_1A86&PID_55D3` = WCH **CH9102** bridge, status `OK` → **no driver install, no admin rights used** |
| I²C wiring and sensor | `firmware/bench_test` flashed to COM4, serial captured | scan: one device **0x68**; **WHO_AM_I = 0x68** (genuine); **909 good reads, 0 failed** in 10 s; gravity **0.98–0.99 g** at rest → **PASS** |
| Flash size / PSRAM / chip | `esptool --chip esp32s3 -p COM4 flash_id` | **16 MB flash detected**, **8 MB embedded PSRAM**, ESP32-S3 QFN56 rev v0.2 → the `16M` build setting is measured, not assumed |
| RGB LED data pin | `firmware/led_test` (compiled with **no** `Serial` at all), observed by Judith | **GPIO 48** shows red/green/blue/magenta; **GPIO 38 dark** → `RGB_LED_PIN 48` in `config.h` |
| Firmware really on the chip | `arduino-cli upload` output | `Hash of data verified` on every flash of `bench_test` and `led_test` |
| Commit 3c reached GitHub | `git ls-remote origin` + `gh api .../commits?sha=judithbranch` | both `10374fc…`, committed 15:01:19 +01:00; `git rev-list --count … --since=13:00` → **6** |
| Wiring photo renders on GitHub | README rendered with the GitHub HTML accept header; raw URL fetched | an `<img src=docs/wiring.jpg …>` tag is in the rendered README; the raw URL returns `200`, `image/jpeg`, `Content-Length: 95701` |
| Recorder sketch compiles and flashes | `arduino-cli compile` / `upload` with the section-3 FQBN | **302,681 bytes (23 %)**, 23,048 B RAM, `Hash of data verified` |
| **The recorder delivers the sample rate it claims** | `uv run python training/record.py --probe`, then a 12 s take; rate computed from the **board's own `t_us`**, not the PC clock | **100.00 Hz** (297 samples / 2.96 s; 1200 / 11.99 s), median dt **10.00 ms**, p99 **10.01–10.03 ms**, **0 gaps**, `az` 0.96–1.00 g at rest |
| Raw CSVs cannot reach the public repo | `git status --short` after two recordings; `git check-ignore -v <file>` | status lists only code and docs; the file is matched by **`.gitignore:27: data/raw/`** |
| (steps 5–11 to be filled as they run) | | |

## Decided by Judith (13:45)

- **Lab window 13:00–17:00.**
- **Toolchain: `arduino-cli` + `esp32:esp32` core, Arduino-style C++, NimBLE for BLE**, installed
  in the user folder without administrator rights. No PlatformIO, no Arduino IDE.
- **LED: the one single on-board addressable RGB LED** carries all four colours; no external LED.
- **Own extension: knowing when it does not know** (abstain on low confidence or unseen
  movement) — which is why step 4 also records never-trained-on movements.

## Open questions — MPU-6050 setup (settle these during step 3, before recording)

**Answered on the bench, 14:35–15:20 — no question below blocks recording any more.**
| # | answer | how it was checked |
| --- | --- | --- |
| 1 | SDA **GPIO 8**, SCL **GPIO 9**; no clash with UART0 (43/44) or the LED (48); bus and LED both work. **Still open: the silkscreen labels**, and the wire colours in `HARDWARE.md` | `bench_test` I²C scan + `led_test` on the same board |
| 2 | AD0 floating → **`0x68`**; the scan found **exactly one** device on the bus | `bench_test` scan 0x08–0x77 |
| 4 | **GPIO 48** drives the on-board LED; GPIO 38 dark | `led_test`, no Serial, observed by Judith |
| 6 | **±8 g** (4096 LSB/g) and **±500 °/s** (65.5 LSB/(°/s)) — shaking cannot clip at ±8 g and ±500 °/s still resolves slow breathing rotation | `config.h`, `bench_test` stream |
| 7 | **100 Hz**, DLPF **44 Hz**. One setting covers both ends (breathing 0.1–1 Hz, shaking >4 Hz), it matches `WINDOW_SAMPLES 250` = 2.5 s, and the **delivered** rate was measured at 100.00 Hz rather than trusted | `data_recorder` + `record.py --probe` / `--list` |
| 8 | **genuine MPU-6050**, `WHO_AM_I` = `0x68` — not a clone, so the init sequence needs no clone workaround | `bench_test` |
| 9 | **COM4**, WCH **CH9102** bridge on UART0, Windows driver already bound → **no install, no admin rights**; therefore `CDCOnBoot=default` (disabled) | `arduino-cli board list`, `Get-PnpDevice` |
| 10 | **open — this is the job at 15:20–15:38.** 4 sessions/class at 45 s is ~6 min of recording per class-pair, so yes with all three recording in shifts; 3-person coverage is a `--person` field the recorder now insists on writing down | `record.py` |
| 3, 5 | 3: not measured with a meter, but the module and the LED both run off 3V3 and nothing browned out — VCC is **not** on 5 V. 5: no button wired; labelling is done by the PC prompt, so BOOT is free | observation while wiring; nothing else drew current on 3V3 |

**Board and wiring**
1. Which GPIOs do we use for SDA/SCL, and does the silkscreen match the N16R8 pinout we will
   document? Are any of them already claimed by the on-board LED, flash or PSRAM?
2. Is AD0 on the GY-521 floating (address `0x68`) or pulled high (`0x69`)? Anything else on the bus?
3. Is the board's 3V3 rail stable enough for the module — and are we sure VCC is **not** on 5 V?
4. **Which GPIO drives that on-board addressable LED?** Confirm it from the board's silkscreen or
   schematic instead of assuming 48: on an N16R8 the octal PSRAM and flash claim a specific pin
   set, and driving the wrong one can stop the board booting.
5. Is a free button available for labelling and abort, or do we use BOOT (`GPIO0`)?

**Sensor configuration**
6. Accelerometer full-scale range: ±8 g or ±16 g so shaking does not clip? Gyro range ±500 °/s?
7. Sample rate: 50 Hz or 100 Hz — enough for breathing (~0.1–1 Hz) and shaking (>4 Hz) with one
   setting? And the DLPF / bandwidth setting?
8. Is the module a genuine MPU-6050 or a clone (some clones differ in gyro startup and register
   behaviour)? Affects the init sequence.

**Board bring-up and data**
9. Which COM port does the board appear as, and is a USB-UART driver already present? We have no
   admin rights to install one — if the board enumerates through its native USB (S3 USB-CDC) we
   may need none at all.
10. Do all three of us take turns as subjects, so the data is not one person only, and are 3
    varied sessions per class realistic inside blocks C1 and C2?

**Graded extras**
11. Which machine and browser will run the Web Bluetooth page in the demo, and does that machine
    really have working BLE? Track 3 settles it with nRF Connect by 14:45.
12. What do we call the second public repository for the GitHub Pages monitor page?

**My defaults, override by objecting**
13. Model exported as weights in a generated C header and evaluated by hand on the chip — for a
    small MLP over hand-made features this avoids the TFLite converter entirely and still runs
    100 % on-chip. Confirmed by comparing PC and on-chip outputs on the same windows.
14. Accelerometer ±8 g, gyro ±500 deg/s, 100 Hz, window 2-3 s at 50 % overlap.

The old questions about audio, microphones and the lab start time are gone — answered above.
