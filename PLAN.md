# PLAN

Real-time AI on a microcontroller — ESP32-S3 N16R8 + MPU-6050 (GY-521).
Status: **2026-11-09, 13:35** (during the lab). Rules in [`AGENTS.md`](AGENTS.md).

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
   Current status: ✅ 1 pushed and verified so far — **need at least 2 more, and we plan 10**,
   one per step below. This file's COMMIT TRACKER is the evidence; update it in the same commit
   as each step.

## Lab clock

| | |
| --- | --- |
| Lab start | **TO CONFIRM** — working assumption: before 13:20 (commit 1 is 13:20:56 +01:00) |
| Lab length | 4 hours |
| Lab end | start + 4 h (≈ 17:00 if it started at 13:00) |
| Now | 13:35 |
| **Commits pushed since lab start** | **1** — `git rev-list --count origin/judithbranch --since="<lab start>"` |

> Print that count every time this tracker is updated (rule 9). It is the number that proves
> compulsory rule 2, so confirm the real lab start time and correct this table.

## COMMIT TRACKER

One step = one commit, never several steps batched (rule 13). A commit cannot contain its own
hash, so each row's hash and time are filled in by the **next** commit; the push proof for a
step is run and shown immediately after that step's push.

| # | step | commit hash | time of the commit | pushed & verified on GitHub |
| --- | --- | --- | --- | --- |
| 1 | Repo skeleton, working rules, plan, gitignore | `2465333` | 2026-11-09 13:20:56 +01:00 | yes — `ls-remote` hash matched local |
| 2 | Plan and rules fixed for the MPU-6050 assignment | this commit (hash recorded in #3) | recorded in #3 | yes — proof run immediately after this push |
| 3 | Wiring and sensor check (I²C scan, live accel + gyro) | — | — | no |
| 4 | Data recorder (labelled sessions, CSV off-repo) | — | — | no |
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

### Step 3 — wiring and sensor check
- MPU-6050 VCC → **3.3 V** (not 5 V), GND, SDA, SCL to the chosen GPIOs, AD0 left floating.
- I²C bus scan → expect the module at `0x68` (or `0x69` if AD0 is high); confirm who-me register
  (`0x68`) reads back `0x68`.
- Wake the chip (clear sleep in `PWR_MGMT_1`), set accelerometer full-scale range (likely ±8 g,
  so shaking does not saturate) and gyro range, read accel + gyro at a fixed rate, print to
  serial *while developing* (USB logging is a dev tool only — constraint 2).
- Sanity checks: gravity axis ≈ 1 g at rest, tap test, rotate-in-hand for the gyro.
- **Proof:** raw numbers on the serial monitor, and a 60-second file of a known-still board.

### Step 4 — data recorder
- Record labelled sessions from the PC-linked board over USB (allowed for data collection —
  the no-USB-data rule applies to the demo).
- One file per session, `sessionNNN_<class>.csv`, header with sample rate, ranges, person,
  placement; ≥3 varied sessions per class; "not attached" includes the board lying still.
- Stage 1 records slow movement + shaking + not-attached (enough for the first demo); stage 2
  adds chest sessions (calm and fast breathing) — several sessions, different rhythms.
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
- **Proof:** the metrics printed from a re-run, plus the confusion matrix.

### Step 6 — firmware: movement vs shaking on the board **← first graded demo**
- Run the same feature code on the chip: ring buffer → window → features → softmax.
- For a small MLP, exporting the weights into a C header and evaluating it by hand is often
  faster to land than a TFLite Micro conversion; either is fine as long as inference is on-chip.
- Smooth the decision over ~2 s (majority vote / hysteresis) before driving the LED.
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

### Step 10 — own extension + README
- Pick the extension **before recording**, so any extra labels are captured in step 4.
  Candidates that fit our data with no new hardware: breathing rate as a number in breaths/min
  against a metronome (and how short a window still works); "knowing when it does not know"
  (abstain on low confidence / unseen movement); a fall vs sit-down-fast test counted in false
  alarms per hour.
- README finished: wiring photo, monitor page link, build + flash, data and training method,
  results, AI tools used.
- **Proof:** the extension working; the rendered README on GitHub.

### Step 11 — final
- Freeze the code, re-check both compulsory rules (tomasvicar in collaborators, commit count),
  run the demo script end to end once from a power bank, close out PLAN.md with the real
  numbers and the verification log filled.

## Time plan for the rest of the lab

Budget ≈ **3 h 25 min** from now (confirm against the real lab end; put spare minutes into
more recording sessions, not into polish). Minutes are focused work; two tracks run in
parallel as noted. The aim is the movement-vs-shaking demo **ready well before the halfway
point**, because it proves the whole chain — wiring, data, training, on-chip inference, LED.

| Block | min | cum. | work | banked points |
| --- | --- | --- | --- | --- |
| A | 15 | 0:15 | *(done)* plan + rules fixed → commit 2 | — |
| B | 20 | 0:35 | wiring, I²C scan, live accel/gyro → commit 3 | — |
| C1 | 15 | 0:50 | record stage-1 sessions: slow movement, shaking, not-attached | — |
| D1 | 20 | 1:10 | features + 2-class model + held-out metrics | — |
| **E** | 30 | **1:40** | **firmware: movement vs shaking on the board → commit 6 — DEMO GATE** | **2.5** |
| C2 | 20 | 2:00 | record chest sessions: calm + fast breathing, more not-attached | — |
| F | 30 | 2:30 | four-state model + firmware + LED → commit 7 | **5.0** |
| G | 25 | 2:55 | BLE characteristic + Pages monitor → commit 8 | **6.5** |
| H | 20 | 3:15 | board's Wi-Fi hotspot page → commit 9 | **7.5** |
| I | 15 | 3:30 | own extension + README → commit 10 | **9.0** |
| J | 10 | 3:40 | freeze, final commit 11, compliance re-check, run demo script | **10.0** |

In parallel (so the serial path stays free):
- **Track 1 (firmware):** blocks B, E, F, H.
- **Track 2 (data + model):** C1, D1, C2, the retraining in F.
- **Track 3 (pages + docs):** create the second public Pages repo and test Web Bluetooth with
  nRF Connect *during* D1/E; draft the README and take the wiring photo during D1/F.
- Commit 4 (recorder) and 5 (model + evaluation) are pushed by track 2 as soon as each works.

If we fall behind, cut in this order — and never cut commits or the README:
board web page (1) → shrink the extension (1.5) → BLE alarms simplified but keep the page
(1.5). The 5 points in blocks E and F are protected: they are the graded chain, and they are
already banked by 2 h 30.

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
5. **Own extension — 1.5.** Demonstrate it, then: what it adds, how it was built, what the
   measured result is (e.g. breaths-per-minute error, or false alarms per hour).
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
| (steps 3–11 to be filled as they run) | | |

## Open questions — MPU-6050 setup and lab (answer before touching hardware)

**Board and wiring**
1. Which GPIOs do we use for SDA/SCL, and does the board's silkscreen match the N16R8 pinout we
   will document? Any of them already used by the board's LED or flash?
2. Is AD0 on the GY-521 floating (address `0x68`) or pulled high (`0x69`)? Any other device on
   the bus?
3. Power: is the board's 3V3 rail stable enough for the module, and are we sure we are **not**
   wiring VCC to 5 V?
4. Which RGB LED does the board actually have — an addressable WS2812/SK6812 on which GPIO, or a
   common three-pin LED on three GPIOs? (Determines the LED driver and whether magenta is
   achievable at the right brightness.)
5. Is a free button available for labelling/abort, or do we use BOOT (`GPIO0`)?

**Sensor configuration**
6. Accelerometer full-scale range: ±8 g or ±16 g so shaking does not clip? Gyro range ±500 °/s?
7. Sample rate: 50 Hz or 100 Hz — enough for breathing (~0.1–1 Hz) and shaking (>4 Hz) with one
   setting? And the DLPF / bandwidth setting?
8. Is the module a genuine MPU-6050 or a clone (some clones differ in gyro startup and register
   behaviour)? Affects the init sequence.

**Toolchain and data**
9. Firmware toolchain: Arduino + PlatformIO, or ESP-IDF? Is the toolchain and the USB-UART
   driver already installed on the lab machine, and which COM port does the board appear as?
10. On-chip inference: hand-exported weights in a C header, or TFLite Micro? (Affects step 5 —
    the model has to be exportable to whichever we choose.)
11. Can we record data with the board linked over USB during collection (allowed) and are
    3 varied sessions per class realistic in our time — do all three of us take turns so the
    data is not one person only?

**Graded extras**
12. Which machine + browser will run the Web Bluetooth page in the demo, and does it have
    working BLE? (Test with nRF Connect early.) May we create the second public repository now,
    and what should it be called?
13. Which own extension do we pick — it must be chosen before recording starts.
14. **Exact lab start and end time**, so the commit-count-since-start figure in the tracker is
    the honest number.
