# App II — Real-time Edge Devices

Lab project: train a small model on our own audio recordings and run the inference in
real time **on an ESP32-S3**, with no computer involved at demo time — USB supplies power
only.

## Group

- Judith Azcona García
- Mohammed Sami Furqan
- Yaren Ustaosmanoglu

## Repository layout

| Path | Purpose |
| --- | --- |
| [`AGENTS.md`](AGENTS.md) | Working rules for the group and for AI assistants (seeds, leak-free splits, on-device inference, git hygiene) |
| [`CLAUDE.md`](CLAUDE.md) | Points at `AGENTS.md` so Claude Code picks up the same rules |
| [`PLAN.md`](PLAN.md) | Current status, step-by-step plan, verification log, open questions |
| `docs/` | Notes, figures and material for the write-up |
| `data/` | Only `data/README.md` is committed — raw recordings stay off the repo (rule 11) |

## Status

Repository skeleton: working rules, plan and git hygiene are in place; the dataset, the
model and the firmware are not started yet. See [`PLAN.md`](PLAN.md) for the next steps.

## Working rules (summary)

The full text is in [`AGENTS.md`](AGENTS.md). In short:

- Work only inside this folder; Python only through `uv` (`uv add`, `uv run`).
- Fixed seeds everywhere — same command, same data, same numbers.
- Split **whole recordings** between train and test *before* cutting windows, so neighbouring
  windows never leak across the split.
- Inference runs on the ESP32-S3; the firmware must never wait for a serial connection.
- Keep `PLAN.md` up to date, say how something was checked before calling it done.
- Commit and push after every verified step; never commit keys or large raw files.

## Getting started

Python runs through [uv](https://docs.astral.sh/uv/), so no manual virtualenv:

```bash
uv add <package>      # install a dependency (updates pyproject.toml + uv.lock)
uv run <script.py>    # run a script inside the project environment
```

The `pyproject.toml` / `uv.lock` pair and the firmware toolchain are still to be added —
they are step 0 in [`PLAN.md`](PLAN.md).
