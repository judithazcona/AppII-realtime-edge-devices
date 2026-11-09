# PLAN

Real-time AI on a microcontroller — ESP32-S3 N16R8 + MPU-6050 (GY-521).
Status: **2026-11-09, 13:47** — mid-lab. The lab runs **13:00–17:00**. Rules in
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
   Current status: ⚠️ **2 pushed and verified** (`2465333`, `9f71414`) — **one short of the
   minimum**, so step 3 must be pushed before anything else is attempted. Nine more are
   planned, one per step. This file's COMMIT TRACKER is the evidence; update it in the same
   commit as each step.

## Lab clock

| | |
| --- | --- |
| Lab start | **13:00** (confirmed) |
| Lab length | 4 hours |
| Lab end | **17:00** |
| Now | 13:47 → **3 h 13 min of lab time left** |
| **Commits pushed since lab start** | **2** — `git rev-list --count origin/judithbranch --since='2026-11-09T13:00:00+01:00'` |

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
| 2b | Toolchain, lab clock, LED and extension decided | this commit (hash recorded in #3) | recorded in #3 | yes — proof run right after this push |
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

### Step 3 — toolchain, wiring and sensor check
**Toolchain (decided):** `arduino-cli` with the `esp32:esp32` core, Arduino-style C++ sketches,
**NimBLE-Arduino** for BLE. **Not** PlatformIO and **not** the Arduino IDE. Installed per-user,
no administrator rights: binary in `%LOCALAPPDATA%\ArduinoCLI`, packages and libraries under
`%USERPROFILE%\.arduino15` and `%USERPROFILE%\Documents\Arduino\libraries`. The install was
started in the background at 13:47 because the core download is the long pole — it runs while
we wire the breadboard.
- Commit `arduino-cli.yaml` (board manager URLs, sketch/build paths) and the exact
  `arduino-cli compile --fqbn esp32:esp32:esp32s3 … ` / `upload` commands in `firmware/README.md`
  so anyone can rebuild without guessing.
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
- **Proof:** raw numbers on the serial monitor, and a 60-second file of a known-still board.

### Step 4 — data recorder
- Record labelled sessions from the PC-linked board over USB (allowed for data collection —
  the no-USB-data rule applies to the demo).
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

**190 minutes left** (13:50 → 17:00). Clock times, not aspirations. The movement-vs-shaking
demo is deliberately ready at **15:20, before the halfway-ish point**, because it proves the
whole chain — wiring, data, training, on-chip inference, LED — and everything after that is
adding classes and surfaces to a chain we already know works.

| Block | clock | min | work | banked points |
| --- | --- | --- | --- | --- |
| A | 13:20–13:50 | *(done)* | commits 1, 2 and 2b: rules, plan, toolchain decision | — |
| — | 13:47→ | *(parallel)* | **arduino-cli + esp32 core + NimBLE installing in the background** | — |
| B | 13:50–14:10 | 20 | wiring, I²C scan, live accel/gyro, **all four LED colours** → commit 3 | — |
| C1 | 14:10–14:25 | 15 | record stage-1 sessions (slow movement, shaking, not-attached) **+ 2–3 never-trained-on movements** → commit 4 | — |
| D1 | 14:25–14:45 | 20 | features + 2-class model + held-out metrics → commit 5 | — |
| **E** | **14:45–15:20** | 35 | **firmware: movement vs shaking on the board → commit 6 — DEMO GATE** | **2.5** |
| C2 | 15:20–15:35 | 15 | record chest sessions: calm + fast breathing, more not-attached | — |
| F | 15:35–16:05 | 30 | four-state model + firmware + LED → commit 7 | **5.0** |
| G | 16:05–16:25 | 20 | BLE characteristic + Pages monitor → commit 8 | **6.5** |
| H | 16:25–16:40 | 15 | board's Wi-Fi hotspot page → commit 9 | **7.5** |
| I | 16:40–16:52 | 12 | abstention extension + README finished → commit 10 | **9.0** |
| J | 16:52–17:00 | 8 | freeze, commit 11, compliance re-check, run the demo script once | **10.0** |

**Reality check, said honestly:** blocks G, H and I are 47 minutes for 4 points, which is
optimistic for BLE-plus-two-web-surfaces. That is why they come last, why their prerequisites
are pulled forward, and because we defend the 5 points banked at **16:05** rather than ending
with six half-working things and no commits.

Three people, three tracks, so the serial path stays free:
- **Track 1 — firmware:** B, E, F, H.
- **Track 2 — data and model:** C1, D1, C2, the retraining in F, the abstention threshold in I.
  Pushes commits 4 and 5 as soon as each works, without waiting for anyone.
- **Track 3 — pages and docs:** by **14:45** create the second public Pages repository and
  confirm the board is visible over BLE in nRF Connect, because a Bluetooth surprise found at
  16:00 kills the whole 1.5 points; then take the wiring photo in B and draft the README
  alongside D1/F so block I is only a finish, not a write-up.

**Decision point, 16:05.** If the four states are not live on the LED by then, drop block H
(board web page, 1 point) and spend its 15 minutes on F; keep G in reduced form (state + one
alarm + connection-lost alarm) rather than skipping it, since the Pages page is also the link
the README must show. Never cut commits or the README — one is compulsory and the other is a
graded part we can bank almost for free.

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
| (steps 3–11 to be filled as they run) | | |

## Decided by Judith (13:45)

- **Lab window 13:00–17:00.**
- **Toolchain: `arduino-cli` + `esp32:esp32` core, Arduino-style C++, NimBLE for BLE**, installed
  in the user folder without administrator rights. No PlatformIO, no Arduino IDE.
- **LED: the one single on-board addressable RGB LED** carries all four colours; no external LED.
- **Own extension: knowing when it does not know** (abstain on low confidence or unseen
  movement) — which is why step 4 also records never-trained-on movements.

## Open questions — MPU-6050 setup (settle these during step 3, before recording)

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
