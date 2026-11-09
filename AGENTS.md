# COMPULSORY — read before anything else

Two rules come straight from the assignment sheet. **If either one is missed, the whole
project gets 0 points**, no matter how well the device works.

1. **The repository must be shared on GitHub with the user `tomasvicar`**
   (Settings → Collaborators) **before the lab ends.**
   Status right now: invitation `342422359` sent 2026-11-09 13:21, **still PENDING — he has to
   accept it, and he is not in the collaborator list yet.** Ask him in person; do not leave it
   to the last minute, and re-check that he appears in the **collaborator** list rather than
   only in the pending invitations:
   `gh api repos/judithazcona/AppII-realtime-edge-devices/collaborators --jq '.[].login'`
   `gh api repos/judithazcona/AppII-realtime-edge-devices/invitations --jq '.[].invitee.login'`

2. **At least 3 intermediate commits during the lab, each pushed to GitHub as we go.**
   A single commit at the end is not enough. Every step in `PLAN.md` is one commit, and the
   COMMIT TRACKER in `PLAN.md` is the evidence; keep it updated and keep the pushed-commit
   count above 3 with room to spare — the count must read 3+ well before 16:30, not at 16:58.

---

# Working rules

Rules for every change in this repository, and the contract between the group members and
any AI assistant working here. The device is an **ESP32-S3 N16R8 with an MPU-6050 (GY-521)
accelerometer + gyroscope** that classifies what is happening to it: calm breathing, fast
breathing, not attached, shaking — shown on the board's single addressable RGB LED. No audio,
no microphone: motion sensors only.

## Scope

1. **Work only inside this folder.** Read, create and modify files under the repository root
   only. Do not write to the Desktop, the user profile, system directories or another project.
   (Two deliberate exceptions, both agreed: the toolchain installing in the user folder under
   rule 7, and reading the downloaded assignment sheet where it sits.)
2. **Keep downloaded course material out of the repository.** The assignment page
   (`real-time-ai.html`, ~1.9 MB) stays outside the repo — course material in a public
   repository, and a large downloaded blob. Its requirements are restated in `PLAN.md`.

## Toolchains

3. **PC Python only through `uv`.** No bare `python`, `pip`, `pip3`, `conda` or hand-made
   virtualenvs.
   - Add a dependency with `uv add <pkg>`, never `pip install`.
   - Run anything with `uv run <script.py>` or `uv run python -c "..."`.
   - `pyproject.toml` and `uv.lock` are committed; `.venv/` is not.
   - If a needed tool cannot be installed with `uv`, stop and ask rather than falling back to
     another package manager.

4. **Firmware is built with `arduino-cli` and the `esp32:esp32` core**, Arduino-style C++
   sketches, **NimBLE-Arduino** for BLE. **Not PlatformIO, not the Arduino IDE.**
5. **Everything installs into the user folder, with no administrator rights.** `arduino-cli` in
   `%LOCALAPPDATA%\ArduinoCLI`, cores and staging under `%USERPROFILE%\.arduino15`, libraries
   under the user Arduino libraries directory. Nothing writes to `Program Files`, `Windows` or
   the machine-wide PATH. If something genuinely needs elevation, stop and ask.
6. **Commit the commands, not just the code.** `firmware/README.md` carries the exact
   `arduino-cli` invocations (core install, `compile --fqbn esp32:esp32:esp32s3 …`, `upload`,
   `monitor`) plus the library list, so another person can rebuild the device on a clean
   machine — that is part of the graded documentation.
7. **USB logging is a development tool.** Serial output is fine while building, but it must sit
   behind a compile-time switch, and the demo build must not need it (constraint 2 below).

## Reproducibility

8. **Fixed seeds.** Every script that trains, splits, shuffles or generates data sets the seed
   explicitly — Python `random`, NumPy, scikit-learn / the training framework — from one shared
   constant (e.g. `SEED` in a `config` module). Two runs of the same command on the same
   recordings must give the same numbers. A metric that moves between runs is a bug to fix or
   explain, not noise.

## Data (IMU recordings)

9. **Split whole recordings between training and test before cutting windows.** Each recording
   session — one continuous stretch of sensor data, one person, one placement, one class — is
   assigned to train / validation / test as a whole; only afterwards is it cut into windows.
   Neighbouring windows overlap and are near-duplicates, so windowing first and splitting after
   leaks and inflates the test score. This is a real risk here: during one session the sensor
   barely moves.
   - Assign splits with a fixed seed or a committed assignment list, so every experiment is
     evaluated on the same held-out sessions.
   - Report test metrics on held-out **sessions**, never on random windows. Record several
     sessions per class and make them genuinely different (person, placement, rhythm, table);
     "not attached" must include the board lying still.
   - Assignment tips we intend to use: log-transform the positive amplitude features, class
     weights if classes are imbalanced, smooth the decisions over a couple of seconds, and a
     small network on hand-made window features rather than a CNN on the raw signal.

## The device: computation on the chip

10. **All inference runs on the ESP32-S3.** Sensor reading, feature extraction and
    classification happen on the board. The PC is only for training and flashing. Never present
    a PC-side or browser-side computation as the deliverable: the web pages may *display* and
    *alarm*, but the state they show must have been computed on the chip.
11. **In the demo, USB is power only — no data may travel over it.**
    - The firmware **never waits for a serial connection**: no `while (!Serial)`, no
      `Serial.setTxTimeoutMs`, no blocking `printf` that stalls the loop, no startup delay while
      no host is attached. The board must reach its working state from a power bank alone and
      keep classifying with the data cable unplugged from the laptop.
    - Acceptance test for this rule: run the demo from a power bank with the data cable out of
      the laptop. Show it, do not assert it.
12. **Both network surfaces are graded, and they have different homes.**
    - The BLE monitor page is served over **HTTPS from GitHub Pages in a second, public
      repository**, because Web Bluetooth needs a secure context. The board's plain HTTP page
      *cannot* use Web Bluetooth, so the monitor page cannot live there. Device selection must
      start from a click or tap (a Connect button). Chrome or Edge on Windows/macOS/Android —
      test on the actual lab machine early; Bluetooth is the part that fails late.
    - The board's own page is served by the board as its **own Wi-Fi hotspot** over plain HTTP,
      showing the live signal and the probabilities of all classes.

## Process

13. **Keep `PLAN.md` up to date.** Same commit as the change it describes: tick steps, update
    the COMMIT TRACKER, note deviations and why. Every time the tracker is updated, print the
    number of commits pushed since the lab started:
    `git rev-list --count origin/judithbranch --since='2026-11-09T13:00:00+01:00'` — that
    number is the evidence for compulsory rule 2.
14. **Ask when something is unclear.** If a requirement is ambiguous, or the honest options have
    different consequences, stop and ask instead of choosing silently. Same for anything
    destructive, irreversible or outward-facing.
15. **Say how you checked it before calling it done.** Done comes with the command that was run
    and what it printed: the held-out accuracy and confusion matrix, the measured inference time,
    the flash/RAM figures, the board booting from a power bank, the LED changing colour, the
    alarm firing. For anything on the board, the proof is the hardware behaving, not the code
    looking right. If something was not run, say so.

## Git

16. **After every step that works, commit with a clear message AND push it immediately to
    `origin/judithbranch`.** Never leave work uncommitted or unpushed between steps.
17. **Never batch several steps into one commit.** One step from `PLAN.md` = one commit. Small,
    honest checkpoints; the history is itself evidence for compulsory rule 2.
18. **Do not create or push `main` during the lab.** Judith says at the end when to create it.
    All work goes to `judithbranch`.
19. **After every push, prove it reached GitHub.** Run `git ls-remote origin` and
    `gh api "repos/judithazcona/AppII-realtime-edge-devices/commits?sha=judithbranch"`, then show
    the newest commit hash and its time. **Do not say a step is done until this check has
    passed** — a commit that exists only on this laptop earns nothing.
20. **The repository is PUBLIC.** Anything committed is published to the whole internet, so never
    commit keys, Wi-Fi passwords, tokens or large raw recordings — **not even temporarily**,
    because deleting them later does not remove them from public history.
    - Wi-Fi credentials for the board's hotspot and any other secret go in a file listed in
      `.gitignore`, with a committed `*.example` template next to it.
    - Raw IMU recordings, extracted windows, feature caches and model binaries stay out; commit
      `data/README.md` describing the format, session naming and how to re-record instead.
    - Before each commit run `git status --short` and `git diff --cached --stat`, and never
      commit a single file over ~2 MB.
    - If a secret does get pushed, say so immediately so the history can be dealt with, and treat
      the credential as burned and change it.
