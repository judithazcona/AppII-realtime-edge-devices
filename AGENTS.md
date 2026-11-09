# COMPULSORY — read before anything else

Two rules come straight from the assignment sheet. **If either one is missed, the whole
project gets 0 points**, no matter how well the device works.

1. **The repository must be shared on GitHub with the user `tomasvicar`**
   (Settings → Collaborators) **before the lab ends.**
   Status right now: invitation `342422359` sent 2026-11-09 13:21, **still pending** — he has
   to accept it. He is not in the collaborator list yet. Check with:
   `gh api repos/judithazcona/AppII-realtime-edge-devices/collaborators --jq '.[].login'`
   and `gh api repos/judithazcona/AppII-realtime-edge-devices/invitations`.
   Ask him in person to accept it; do not leave this to the last minute, and re-check that he
   has actually appeared in the collaborator list, not just in the pending invitations.

2. **At least 3 intermediate commits during the lab, each pushed to GitHub as we go.**
   A single commit at the end is not enough. Every step in `PLAN.md` is one commit, and the
   COMMIT TRACKER in `PLAN.md` is the evidence; keep it updated and keep the pushed-commit
   count above 3 with room to spare.

---

# Working rules

Rules for every change in this repository, and the contract between the group members and
any AI assistant working here. The device is an **ESP32-S3 N16R8 with an MPU-6050 (GY-521)
accelerometer + gyroscope** that classifies what is happening to it: calm breathing, fast
breathing, not attached, shaking. No audio, no microphone — motion sensors only.

## Scope

1. **Work only inside this folder.** Read, create and modify files under the repository root
   only. Do not write to the Desktop, the user profile, system directories or another
   project.
2. **Keep downloaded course material out of the repository.** The assignment page
   (`real-time-ai.html`, ~1.9 MB) stays outside the repo — it is course material in a public
   repository and it is a large downloaded blob. Its requirements are summarised in `PLAN.md`.

## Python

3. **Python only through `uv`.** No bare `python`, `pip`, `pip3`, `conda` or hand-made
   virtualenvs.
   - Add a dependency with `uv add <pkg>`, never `pip install`.
   - Run anything with `uv run <script.py>` or `uv run python -c "..."`.
   - `pyproject.toml` and `uv.lock` are committed; `.venv/` is not.
   - If a needed tool cannot be installed with `uv`, stop and ask rather than falling back to
     another package manager.

## Reproducibility

4. **Fixed seeds.** Every script that trains, splits, shuffles or generates data sets the
   seed explicitly — Python `random`, NumPy, scikit-learn / the training framework, and any
   data-loader worker seed — from one shared constant (e.g. `SEED` in a `config` module).
   Two runs of the same command on the same data must give the same numbers. A metric that
   moves between runs is a bug to fix or explain, not noise.

## Data (IMU recordings)

5. **Split whole recordings between training and test before cutting windows.** Each
   recording session (one continuous stretch of sensor data, one person, one placement, one
   class) is assigned to train / validation / test as a whole; only afterwards is it cut into
   windows. Neighbouring windows overlap and are near-duplicates, so windowing first and
   splitting after leaks and inflates the test score — a real risk here, because during one
   session the sensor barely moves.
   - Assign splits with a fixed seed or a committed assignment list, so every experiment is
     evaluated on the same held-out sessions.
   - Report test metrics on held-out **sessions**, not on random windows. Record several
     sessions per class and make them genuinely different (different person, placement,
     rhythm, table); "not attached" must include the board lying still on the table.
   - Positive amplitude features: try a log transform. Imbalanced classes: try class weights.
     Smooth the decisions over a couple of seconds before driving the LED.

## The device: computation on the chip

6. **All inference runs on the ESP32-S3.** Sensor reading, feature extraction and
   classification happen on the board. The PC is only for training the model and flashing it.
   Never present a PC-side or browser-side computation as the deliverable: the monitor web
   page may *display* and *alarm*, but the state it shows must have been computed on the chip.
7. **In the demo, USB is power only — no data may travel over it.**
   - The firmware **never waits for a serial connection**: no `while (!Serial)`, no
     `Serial.setTxTimeoutMs`, no blocking `printf` that stalls the loop, nothing that delays
     startup while no host is attached. The board must reach its normal working state from a
     power bank alone, and must keep classifying if the USB data line is unplugged.
   - Logging over USB/UART is a development tool only. Keep it behind a build flag or a
     compile-time switch, and make sure the demo build does not need it.
   - Prove it by running the demo from a power bank / charger with the data cable unplugged
     from the laptop. That is the acceptance test for this rule.
8. **Two network surfaces, and both are graded.**
   - The BLE monitor page must be served over **HTTPS from GitHub Pages in a second, public
     repository**, because Web Bluetooth only works in a secure context. The board's own plain
     HTTP page *cannot* use Web Bluetooth, so the monitor page cannot live there. Device
     selection must start from a click or tap (a Connect button). Chrome or Edge on
     Windows/macOS/Android — test on the actual lab machine early, Bluetooth is the part that
     fails late.
   - The board's own page is served by the board as its **own Wi-Fi hotspot** over plain
     HTTP, showing the live signal and the probabilities of all classes.

## Process

9. **Keep `PLAN.md` up to date.** Same commit as the change it describes: tick steps, update
   the COMMIT TRACKER, note deviations and why. Every time the tracker is updated, print the
   number of commits pushed since the lab started:
   `git rev-list --count origin/judithbranch --since="<lab start, ISO>"` — this is the
   evidence for compulsory rule 2.
10. **Ask when something is unclear.** If a requirement is ambiguous, or the honest options
    have different consequences, stop and ask instead of choosing silently. Same for anything
    destructive, irreversible or outward-facing.
11. **Say how you checked it before calling it done.** "Done" comes with the command that was
    run and what it printed: the held-out accuracy and confusion matrix, the measured
    inference time, the flash/RAM figures, the board booting from a power bank, the LED
    changing colour, the alarm firing. For anything on the board, the proof is the hardware
    behaving, not the code looking right. If something was not run, say so.

## Git

12. **After every step that works, commit with a clear message AND push it immediately to
    `origin/judithbranch`.** Never leave work uncommitted or unpushed between steps.
13. **Never batch several steps into one commit.** One step from `PLAN.md` = one commit. Small,
    honest checkpoints; the history is itself part of the evidence for compulsory rule 2.
14. **Do not create or push `main` during the lab.** Judith says at the end when to create it.
    All work goes to `judithbranch`.
15. **After every push, prove it reached GitHub.** Run
    `git ls-remote origin` and
    `gh api "repos/judithazcona/AppII-realtime-edge-devices/commits?sha=judithbranch"`,
    then show the newest commit hash and its time. **Do not say a step is done until this
    check has passed** — a commit that exists only on this laptop earns nothing.
16. **The repository is PUBLIC.** Anything committed is published to the whole internet, so
    never commit keys, Wi-Fi passwords, tokens or large raw recordings — **not even
    temporarily**, because deleting them later does not remove them from public history.
    - Wi-Fi credentials for the board's hotspot and any other secret go in a file listed in
      `.gitignore`, with a committed `*.example` template next to it.
    - Raw IMU recordings, extracted windows, feature caches and model binaries stay out of the
      repo; commit `data/README.md` describing the format, the session naming and how to
      re-record instead.
    - Before each commit run `git status --short` and `git diff --cached --stat`, and do not
      commit any single file over ~2 MB.
    - If a secret ever does get pushed, tell Judith immediately so the history can be dealt
      with — and treat the credential as burned and change it.
