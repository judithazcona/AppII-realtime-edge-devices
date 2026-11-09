# Working rules

These rules apply to every change in this repository. They are the contract between the
group members and any AI assistant working here.

## Scope

1. **Work only inside this folder.** Read, create and modify files under the repository
   root only. Do not write to the Desktop, the user profile, system directories or another
   project. Tools must be pointed at this folder, not run from elsewhere.

## Python

2. **Python only through `uv`.** No bare `python`, `pip`, `pip3`, `conda` or virtualenv
   created by hand.
   - Add a dependency with `uv add <pkg>`, never `pip install`.
   - Run anything with `uv run <script.py>` or `uv run python -c "..."`.
   - `pyproject.toml` and `uv.lock` are committed; `.venv/` is not.
   - If a needed tool cannot be installed with `uv`, stop and ask instead of falling back
     to another package manager.

## Reproducibility

3. **Fixed seeds.** Every script that trains, splits, shuffles or generates data sets the
   seed explicitly — Python `random`, NumPy, the ML framework and any data-loader worker
   seed — from a single shared constant (e.g. `SEED` in a `config` module). Two runs of the
   same command on the same data must produce the same numbers. Report a metric that moved
   between runs as a bug, not as noise.
   - Also fix the framework determinism flags where they exist, and note in the commit
     message anything that could not be made deterministic and why.

## Data

4. **Split whole recordings before cutting windows.** The train/validation/test split is
   assigned per *recording* (per session / per speaker), and only afterwards is each
   recording cut into windows. Windows from one recording may never straddle two splits —
   neighbouring windows are near-duplicates, so splitting after windowing leaks and
   inflates the test score.
   - The split is decided by a fixed seed or a written-down assignment list, committed with
     the data description, so every experiment is evaluated on the same test recordings.
   - Report test metrics on the held-out recordings only. Never tune on them.

## Hardware and demo

5. **The final computation runs on the ESP32-S3.** The PC may be used for training and for
   debugging logs, but inference must execute on the microcontroller. Do not add features
   that compute on the PC, in a browser or in a serial host script and call them the
   deliverable. Do not send raw audio to the PC for the answer.

6. **In the demo, USB is power only.** The demo must work with a phone charger / power bank
   and no host connected, so:
   - The firmware **never waits for a serial connection.** No `Serial.setTxTimeoutMs`,
     no `while (!Serial)`, no `portMAX_DELAY` write, nothing that blocks or delays startup
     or the inference loop while no host is attached.
   - Logging must be non-blocking and must degrade to a no-op when nobody is reading.
   - The boot → sense → infer → act loop must reach its normal working state on power-up
     alone. Test it by plugging into a charger with the data cable unplugged from the PC.

## Process

7. **Keep `PLAN.md` up to date.** Update it in the same commit that changes the state of
   the work: tick or cross off steps, note deviations from the plan and why, and record the
   current status. A plan that lies is worse than no plan.

8. **Ask when something is unclear.** If a requirement is ambiguous, or the honest options
   have different consequences, stop and ask rather than choosing silently. This includes
   destructive or irreversible actions.

9. **Say how you checked it before calling it done.** "Done" comes with the command you
   ran and what it printed: the test output, the measured number, the log lines, the flash
   and RAM figures, the confirmation that the device booted standalone. If something was
   not run, say that explicitly instead of implying it works.

## Git

10. **Commit and push after every step that works**, with a clear message that says *what*
    changed and *why* (e.g. `Add leak-free recording-level train/test split`, not `update`).
    A step counts as "works" when it has been checked as in rule 9. During the lab session
    there must be **at least 3 intermediate commits** — small, honest checkpoints rather
    than one big drop at the end.

11. **Never commit keys or large raw files.** No API tokens, Wi-Fi credentials, device
    certificates, `.env` files or private keys — those go in ignored files or in
    `secrets.h`-style includes that are listed in `.gitignore`, with a committed
    `*.example` template instead. Raw recordings, generated datasets, model binaries and
    build output stay out of the repository too; commit a `data/README.md` describing where
    the data comes from, its format and its licence, plus any scripts that regenerate it.
    Check `git status --short` and `git diff --cached --stat` before every commit.
