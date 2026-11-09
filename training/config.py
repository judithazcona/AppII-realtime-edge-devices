"""Shared configuration for every PC-side script: the seed, the paths, the classes.

One constant for randomness (AGENTS.md rule 8). Every script that shuffles, splits,
trains or generates data calls :func:`seed_everything` before it does anything, so two
runs of the same command on the same recordings print the same numbers. If a metric
moves between runs, that is a bug to fix, not noise.

The hardware numbers (sample rate, full-scale ranges, scales, window size) live in
``firmware/config.h`` and are **not** repeated here: ``data_recorder.ino`` sends them in
its stream header and ``record.py`` copies them into each CSV, so the recordings always
carry the values the firmware actually used instead of a copy that went stale.
"""

from __future__ import annotations

import random
from pathlib import Path

# ----------------------------------------------------------------- seed ------
SEED = 20261109  # date of the lab; one constant, referenced everywhere


def seed_everything(seed: int = SEED) -> int:
    """Seed every RNG the pipeline can reach. Import-safe when a package is absent.

    numpy / scikit-learn are imported lazily: the recorder must run with nothing but
    pyserial installed, and step 5 will pull them in later.
    """
    random.seed(seed)
    try:  # pragma: no cover - optional at recording time
        import numpy as np
    except ImportError:
        np = None
    if np is not None:
        np.random.seed(seed)
    try:  # pragma: no cover - optional at recording time
        from sklearn.utils import check_random_state  # noqa: F401
    except ImportError:
        pass
    return seed


# ---------------------------------------------------------------- paths ------
REPO_ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = REPO_ROOT / "data"
RAW_DIR = DATA_DIR / "raw"  # recordings; gitignored, never committed (rule 16)

# --------------------------------------------------------------- serial ------
DEFAULT_PORT = "COM4"  # HARDWARE.md: CH9102 bridge on UART0
DEFAULT_BAUD = 115200

# A session shorter than this is not usable for a 2.5 s window with overlap.
MIN_SESSION_SECONDS = 10.0
TARGET_SESSION_SECONDS = 45.0

# The recorder measures the delivered rate from the board's own timestamps; warn if
# it drifts this far from the rate the firmware was told to run at.
RATE_TOLERANCE_HZ = 5.0

# --------------------------------------------------------------- classes -----
# name -> (LED colour, what it means, which stage records it)
CLASSES: dict[str, tuple[str, str, int]] = {
    "not_attached": ("blue", "board loose: in the hand, in the pocket, or lying still on the table", 1),
    "shaking": ("magenta", "board shaken by hand, several styles and strengths", 1),
    "calm_breathing": ("green", "board on the chest, slow regular breathing", 2),
    "fast_breathing": ("red", "board on the chest, fast breathing", 2),
}

# Stage 1 (the first graded demo) needs only these two; the chest pair arrives with step 7.
STAGE1_CLASSES: tuple[str, ...] = tuple(n for n, v in CLASSES.items() if v[2] == 1)
CHEST_CLASSES: tuple[str, ...] = tuple(n for n, v in CLASSES.items() if v[2] == 2)

# Sessions we never train on — the held-out material for the abstention extension
# (PLAN.md step 10). Recorded now because they cannot be invented afterwards.
NOVEL_CLASSES: tuple[str, ...] = ("novel_tapping", "novel_rolling", "novel_walking")

# Everything the recorder will accept. The chest classes opened once stage-1 data was in
# the bag, so that a mis-recorded class could not contaminate the first demo.
RECORDABLE_CLASSES: tuple[str, ...] = tuple(CLASSES) + NOVEL_CLASSES

# What to say on the console for the chest classes, where the wiring of the *subject*
# matters more than the wiring of the board.
CHEST_HINTS: dict[str, str] = {
    "calm_breathing": "hold/tape the board flat against the chest, breathe slowly and "
                      "regularly through the nose, don't talk or cough",
    "fast_breathing": "same placement, breathe fast and shallow on purpose, keep the "
                      "board from sliding",
}
