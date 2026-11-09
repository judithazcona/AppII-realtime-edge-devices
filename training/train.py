#!/usr/bin/env python
"""Train the classifier that will run on the ESP32-S3, and export it as a C header.

    uv run python training/train.py                        # whatever classes have data
    uv run python training/train.py --classes not_attached shaking
    uv run python training/train.py --export                # also write the C header

**The split is by whole session, before any windowing** (AGENTS.md rule 9). Neighbouring
windows overlap by 50 % and are near-duplicates of each other, so if you window first and
split after, the same physical half-second sits in train and in test and the test score means
nothing. Every number printed here is on sessions the model has never seen at all, which is
also the only honest predictor of the demo, where the board meets a *new* performance.

Window 2.5 s, stride 1.25 s - read out of `firmware/config.h` rather than repeated here, so
the training side cannot drift away from the ring buffer on the chip.

Seeds come from `config.SEED` (rule 8): the shuffle that assigns sessions, the MLP weight
init, and the solver's randomness. Re-running on the same recordings must print the same
confusion matrix; if it does not, that is a bug to fix here, not noise to tolerate.
"""

from __future__ import annotations

import argparse
import json
import random
import re
import sys
from collections import Counter
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))

from config import SEED, CLASSES, NOVEL_CLASSES, RAW_DIR, seed_everything  # noqa: E402
from features import (  # noqa: E402
    FEATURE_NAMES,
    N_FEATURES,
    apply_standardisation,
    log_amplitudes,
    standardisation_params,
    window_features,
)

REPO_ROOT = Path(__file__).resolve().parents[1]
CONFIG_H = REPO_ROOT / "firmware" / "config.h"
SPLIT_FILE = REPO_ROOT / "training" / "split_assignment.json"
HEADER_OUT = REPO_ROOT / "firmware" / "model_weights.h"

MIN_SESSIONS_PER_CLASS = 2  # with 1 session there is no held-out session to be honest about


# --------------------------------------------------------------- config.h ----
def read_firmware_config() -> dict[str, int]:
    """Window size, stride and rate are defined once, on the firmware side."""
    text = CONFIG_H.read_text()
    out: dict[str, int] = {}
    for key in ("WINDOW_SAMPLES", "WINDOW_STRIDE_SAMPLES", "MPU_SAMPLE_RATE_HZ"):
        m = re.search(rf"#define\s+{key}\s+(\d+)", text)
        if not m:
            raise SystemExit(f"{key} not found in {CONFIG_H} - where does the window come from?")
        out[key] = int(m.group(1))
    return out


# ------------------------------------------------------------------ data -----
def load_session(path: Path) -> tuple[dict[str, str], np.ndarray, np.ndarray]:
    """Return (metadata, t_us, signal[N,6]) from one recording."""
    meta: dict[str, str] = {}
    rows: list[list[float]] = []
    for line in path.read_text().splitlines():
        if line.startswith("#"):
            parts = line[1:].split(None, 1)
            if len(parts) == 2:
                meta[parts[0]] = parts[1]
            continue
        fields = line.split(",")
        if len(fields) < 7:
            continue
        try:
            rows.append([float(fields[0])] + [float(v) for v in fields[1:7]])
        except ValueError:
            continue  # the column-name row
    if not rows:
        raise SystemExit(f"{path.name}: no data rows - a truncated write?")
    arr = np.asarray(rows, dtype=np.float64)
    return meta, arr[:, 0].astype(np.int64), arr[:, 1:7]


def contiguous_runs(t_us: np.ndarray, rate_hz: float) -> list[tuple[int, int]]:
    """Sample ranges with no dropped sample inside, so a window never straddles a gap."""
    if t_us.size < 2:
        return [(0, t_us.size)]
    period = 1e6 / rate_hz
    steps = np.diff(t_us)
    breaks = np.nonzero(steps > 1.75 * period)[0] + 1
    edges = np.concatenate(([0], breaks, [t_us.size]))
    return [(int(a), int(b)) for a, b in zip(edges, edges[1:]) if b > a]


def window_signal(sig: np.ndarray, runs: list[tuple[int, int]], size: int,
                  stride: int) -> np.ndarray:
    out = [sig[a:b][i:i + size] for a, b in runs for i in range(0, max(1, b - a - size + 1), stride)]
    return np.asarray([w for w in out if w.shape[0] == size]) if out else np.empty((0, size, 6))


def discover_sessions() -> dict[str, list[Path]]:
    """Usable recordings per class. Smoke tests and novel_* never enter training."""
    by_class: dict[str, list[Path]] = {}
    if not RAW_DIR.exists():
        raise SystemExit(f"{RAW_DIR} does not exist - record something first (data/README.md)")
    for path in sorted(RAW_DIR.glob("*session*.csv")):
        if "smoke_test" in path.name:
            continue
        m = re.match(r"^(?:novel_)?session\d+_(.+?)__", path.name)
        if not m:
            continue
        cls = m.group(1)
        if cls in NOVEL_CLASSES:
            continue
        by_class.setdefault(cls, []).append(path)
    return by_class


# ----------------------------------------------------------------- split -----
def assign_splits(by_class: dict[str, list[Path]], seed: int) -> dict[str, str]:
    """Whole-session train/val/test, seeded, and **committed** so it is stable forever.

    An existing `training/split_assignment.json` is honoured: a session that already has a
    split keeps it, so growing the dataset later cannot move a session from test to train -
    which would quietly invalidate every number quoted in the README.
    """
    existing: dict[str, str] = {}
    if SPLIT_FILE.exists():
        existing = json.loads(SPLIT_FILE.read_text())["sessions"]

    assignment = dict(existing)
    rng = random.Random(seed)
    for cls, paths in sorted(by_class.items()):
        names = [p.name for p in paths]
        fresh = [n for n in names if n not in existing]
        rng.shuffle(fresh)  # seeded: the same sessions land in the same split every run
        n = len(names)
        want = {"test": max(1, round(n * 0.25)), "val": max(1, round(n * 0.25))}
        have = Counter(assignment[n2] for n2 in names if n2 in assignment)
        need_test = max(0, want["test"] - have["test"])
        need_val = max(0, want["val"] - have["val"])
        for i, name in enumerate(fresh):
            if i < need_test:
                assignment[name] = "test"
            elif i < need_test + need_val:
                assignment[name] = "val"
            else:
                assignment[name] = "train"
    return assignment


def save_splits(assignment: dict[str, str], seed: int) -> None:
    payload = {
        "_comment": "Whole-session split assigned before windowing (AGENTS.md rule 9). "
                    "Committed so every experiment is evaluated on the same held-out sessions.",
        "seed": seed,
        "sessions": dict(sorted(assignment.items())),
    }
    SPLIT_FILE.write_text(json.dumps(payload, indent=2) + "\n")


# --------------------------------------------------------------- features ----
def extract(paths: list[Path], size: int, stride: int, rate: float) -> np.ndarray:
    mats = []
    for path in paths:
        _, t_us, sig = load_session(path)
        wins = window_signal(sig, contiguous_runs(t_us, rate), size, stride)
        if len(wins):
            mats.append(np.vstack([window_features(w) for w in wins]))
    return np.vstack(mats) if mats else np.empty((0, N_FEATURES))


# --------------------------------------------------------------- metrics -----
def confusion_matrix(y: np.ndarray, yhat: np.ndarray, labels: list[str]) -> list[list[int]]:
    idx = {c: i for i, c in enumerate(labels)}
    m = [[0] * len(labels) for _ in labels]
    for t, p in zip(y, yhat):
        m[idx[t]][idx[p]] += 1
    return m


def print_confusion(title: str, matrix: list[list[int]], labels: list[str]) -> None:
    width = max(max(len(c) for c in labels), 5)
    print(f"\n{title}   (rows = true session, cols = predicted)")
    print(" " * (width + 2) + "".join(f"{c[:width]:>{width + 2}}" for c in labels)
          + f"{'recall':>{width + 2}}")
    for i, row in enumerate(matrix):
        total = sum(row)
        rec = f"{100 * row[i] / total:.0f}%" if total else "  -"
        print(f"{labels[i]:<{width + 2}}" + "".join(f"{v:>{width + 2}}" for v in row)
              + f"{rec:>{width + 2}}")
    correct = sum(matrix[i][i] for i in range(len(labels)))
    n = sum(sum(r) for r in matrix)
    print(f"{'accuracy':<{width + 2}}{correct}/{n} = {100 * correct / n if n else 0:.1f}%")


# ------------------------------------------------------------------ export ---
def export_header(model, mean: np.ndarray, std: np.ndarray, labels: list[str],
                  cfg: dict[str, int], metrics: dict[str, float], path: Path) -> None:
    """Write the trained MLP + preprocessing as a C header the firmware compiles in.

    Weights are stored transposed for row-major dot products, at float32 - the same type the
    chip uses, so the header cannot be more accurate than the firmware that reads it.
    """
    w1, b1 = model.coefs_[0], model.intercepts_[0]
    w2, b2 = model.coefs_[1], model.intercepts_[1]
    if w2.shape[1] == 1:
        # sklearn fits ONE logistic unit for a two-class problem, not two softmax units.
        # Widening to the pair (-z/2, +z/2) makes softmax reproduce sigmoid(z) EXACTLY
        # (softmax(-z, +z) would be sigmoid(2z) - a factor 2 error that the export check
        # below caught at 9e-3 on real data). One softmax path in the firmware then covers
        # both the 2-class and the 4-class model.
        w2 = np.hstack([-0.5 * w2, 0.5 * w2])
        b2 = np.concatenate([-0.5 * b2, 0.5 * b2])

    def arr(name: str, a: np.ndarray, per_line: int = 8) -> list[str]:
        flat = np.asarray(a, dtype=np.float32).ravel()
        vals = ", ".join(f"{v:.7g}f" for v in flat)
        out, chunk = [f"static const float {name}[{flat.size}] = {{", ], vals.split(", ")
        for i in range(0, len(chunk), per_line):
            out.append("    " + ", ".join(chunk[i:i + per_line]) + ("," if i + per_line < len(chunk) else ""))
        out.append("};")
        return out

    hidden = w1.shape[1]
    lines = [
        "// model_weights.h - GENERATED by training/train.py. Do not edit by hand.",
        "//",
        f"// classes: {', '.join(labels)}",
        f"// held-out-session accuracy at export time: {100 * metrics['test_acc']:.1f}% "
        f"on {metrics['test_windows']:.0f} windows",
        f"// preprocessing: log1p on amplitude-like features, then (x - mean) / std, "
        f"both fitted on TRAIN sessions only",
        f"// seed {int(metrics['seed'])}  //  window {cfg['WINDOW_SAMPLES']} samples "
        f"@ {cfg['MPU_SAMPLE_RATE_HZ']} Hz, stride {cfg['WINDOW_STRIDE_SAMPLES']}",
        "",
        "#pragma once",
        "",
        f"#define MODEL_N_FEATURES {N_FEATURES}",
        f"#define MODEL_N_HIDDEN {hidden}",
        f"#define MODEL_N_CLASSES {len(labels)}",
        f"#define MODEL_WINDOW_SAMPLES {cfg['WINDOW_SAMPLES']}",
        f"#define MODEL_WINDOW_STRIDE {cfg['WINDOW_STRIDE_SAMPLES']}",
        f"#define MODEL_SAMPLE_RATE_HZ {cfg['MPU_SAMPLE_RATE_HZ']}",
        "",
        f"static const char *const MODEL_CLASS_NAMES[{len(labels)}] = {{"
        + ", ".join(f'"{c}"' for c in labels) + "};",
        "",
    ]
    lines += arr("MODEL_LOG_MASK", np.array([1.0 if n in _log_set() else 0.0 for n in FEATURE_NAMES]))
    lines += [""] + arr("MODEL_FEAT_MEAN", mean) + [""] + arr("MODEL_FEAT_STD", std)
    lines += [""] + arr("MODEL_W1", w1) + [""] + arr("MODEL_B1", b1)
    lines += [""] + arr("MODEL_W2", w2) + [""] + arr("MODEL_B2", b2)
    lines += [
        "",
        "// Indexing, row-major, exactly as written to memory:",
        "//   h[j]      = MODEL_B1[j] + sum_i x[i] * MODEL_W1[i*MODEL_N_HIDDEN + j]",
        "//   logits[k] = MODEL_B2[k] + sum_j h[j] * MODEL_W2[j*MODEL_N_CLASSES + k]",
        "// Activation: relu on layer 1, softmax on layer 2. x is the log1p'd, standardised",
        "// feature vector in MODEL_FEATURE_NAMES order.",
        "",
        "static const char *const MODEL_FEATURE_NAMES[" + str(N_FEATURES) + "] = {"
        + ", ".join(f'"{n}"' for n in FEATURE_NAMES) + "};",
        "",
    ]
    path.write_text("\n".join(lines), newline="\n")


def _log_set() -> frozenset:
    from features import LOG_FEATURES
    return LOG_FEATURES


def verify_export(model, logged_test: np.ndarray, mean: np.ndarray, std: np.ndarray,
                  header_text: str) -> float:
    """Re-read the header arrays and forward-propagate them in numpy.

    This proves the *serialisation*: the numbers that reached the header reproduce sklearn's
    probabilities. It does NOT prove the C code computes the same features - that is checked on
    the chip in step 6, and only there.
    """
    def grab(name: str) -> np.ndarray:
        m = re.search(rf"static const float {name}\[\d+\] = \{{(.*?)\}};", header_text, re.S)
        return np.array([float(v.rstrip(",f")) for v in m.group(1).split()])

    mean_h, std_h = grab("MODEL_FEAT_MEAN"), grab("MODEL_FEAT_STD")
    hidden = grab("MODEL_B1").size
    n_cls = grab("MODEL_B2").size
    w1 = grab("MODEL_W1").reshape(N_FEATURES, hidden)
    w2 = grab("MODEL_W2").reshape(hidden, n_cls)

    x = (logged_test - mean_h) / std_h
    h = np.maximum(0.0, x @ w1 + grab("MODEL_B1"))
    logits = h @ w2 + grab("MODEL_B2")
    logits -= logits.max(axis=1, keepdims=True)
    p = np.exp(logits)
    p /= p.sum(axis=1, keepdims=True)
    return float(np.max(np.abs(p - model.predict_proba(x))))


# -------------------------------------------------------------------- main ---
def main() -> int:
    ap = argparse.ArgumentParser(description="train the on-chip classifier")
    ap.add_argument("--classes", default="", help="comma separated; default = every class "
                                                  "with at least two usable sessions")
    ap.add_argument("--hidden", type=int, default=16)
    ap.add_argument("--alpha", type=float, default=1e-3)
    ap.add_argument("--epochs", type=int, default=3000)
    ap.add_argument("--export", action="store_true", help="write firmware/model_weights.h")
    ap.add_argument("--seed", type=int, default=SEED)
    args = ap.parse_args()

    seed_everything(args.seed)
    cfg = read_firmware_config()
    rate = float(cfg["MPU_SAMPLE_RATE_HZ"])

    found = discover_sessions()
    if args.classes:
        wanted = [c.strip() for c in args.classes.split(",") if c.strip()]
        missing = [c for c in wanted if not found.get(c)]
        if missing:
            raise SystemExit(f"no usable sessions for: {', '.join(missing)}. Present: "
                             + ", ".join(f"{k}({len(v)})" for k, v in sorted(found.items())))
        labels = wanted
    else:
        labels = [c for c, v in sorted(found.items()) if len(v) >= MIN_SESSIONS_PER_CLASS]
    if len(labels) < 2:
        raise SystemExit("need at least two classes with data to train anything")
    usable = {c: found[c] for c in labels}
    thin = {c: len(v) for c, v in usable.items() if len(v) < MIN_SESSIONS_PER_CLASS}
    if thin:
        raise SystemExit(f"too few sessions for a held-out split: {thin} "
                         f"(need >= {MIN_SESSIONS_PER_CLASS})")

    assignment = assign_splits(usable, args.seed)
    save_splits(assignment, args.seed)

    print(f"rate {rate:.0f} Hz from config.h   window {cfg['WINDOW_SAMPLES']} samples "
          f"({cfg['WINDOW_SAMPLES'] / rate:.1f} s)   stride {cfg['WINDOW_STRIDE_SAMPLES']} "
          f"({cfg['WINDOW_STRIDE_SAMPLES'] / rate:.2f} s)   features {N_FEATURES}")
    print(f"seed {args.seed}\n")
    print(f"{'session':<48}{'class':<16}{'split':<8}{'windows':>8}")
    parts: dict[str, list[np.ndarray]] = {"train": [], "val": [], "test": []}
    labels_of: dict[str, list[str]] = {"train": [], "val": [], "test": []}
    for cls in labels:
        for path in usable[cls]:
            _, t_us, sig = load_session(path)
            wins = window_signal(sig, contiguous_runs(t_us, rate), cfg["WINDOW_SAMPLES"],
                                 cfg["WINDOW_STRIDE_SAMPLES"])
            split = assignment[path.name]
            if len(wins):
                parts[split].append(np.vstack([window_features(w) for w in wins]))
                labels_of[split] += [cls] * len(wins)
            print(f"{path.name:<48}{cls:<16}{split:<8}{len(wins):>8}")
    if not len(parts["train"]) or not len(parts["test"]):
        raise SystemExit("the split left train or test empty - too few sessions per class")

    xtr_raw = np.vstack(parts["train"])
    xva_raw = np.vstack(parts["val"]) if len(parts["val"]) else None
    xte_raw = np.vstack(parts["test"])
    ytr = np.asarray(labels_of["train"])
    yva = np.asarray(labels_of["val"]) if xva_raw is not None else None
    yte = np.asarray(labels_of["test"])

    xtr_log = log_amplitudes(xtr_raw)
    mean, std = standardisation_params(xtr_log)  # TRAIN ONLY - scaling test data would leak
    xtr = apply_standardisation(xtr_log, mean, std)
    xva = apply_standardisation(log_amplitudes(xva_raw), mean, std) if xva_raw is not None else None
    xte = apply_standardisation(log_amplitudes(xte_raw), mean, std)

    from sklearn.neural_network import MLPClassifier
    from sklearn.linear_model import LogisticRegression

    counts = Counter(ytr.tolist())
    # MLPClassifier has no class_weight argument at all, so imbalance goes in through
    # per-window sample weights (fit() does accept those). Only switched on when a class is
    # genuinely short: weighting a balanced set just makes the loss noisier.
    imbalanced = max(counts.values()) > 1.5 * min(counts.values())
    sample_w = None
    if imbalanced:
        per_class = {c: len(ytr) / (len(counts) * n) for c, n in counts.items()}
        sample_w = np.asarray([per_class[c] for c in ytr])
    cw = "balanced via sample weights" if imbalanced else "none (classes balanced)"
    model = MLPClassifier(hidden_layer_sizes=(args.hidden,), activation="relu", alpha=args.alpha,
                          max_iter=args.epochs, random_state=args.seed)
    model.fit(xtr, ytr, sample_weight=sample_w)
    floor = LogisticRegression(max_iter=4000, random_state=args.seed,
                               class_weight="balanced" if imbalanced else None)
    floor.fit(xtr, ytr)

    print(f"\nmodel MLP {N_FEATURES}-{args.hidden}-{len(labels)} relu alpha={args.alpha} "
          f"class_weight={cw}   iterations {model.n_iter_}, loss {model.loss_:.4g}")
    print(f"train windows {len(xtr)}  val {0 if xva is None else len(xva)}  test {len(xte)}"
          f"   (weights {model.coefs_[0].size + model.coefs_[1].size} floats "
          f"= {(model.coefs_[0].size + model.coefs_[1].size) * 4 / 1024:.1f} KB)")
    if xva is not None:
        print_confusion("VALIDATION (seen in no gradient step, used only to pick the model)",
                        confusion_matrix(yva, model.predict(xva), labels), labels)
    print_confusion("TEST - held-out SESSIONS, never trained on",
                    confusion_matrix(yte, model.predict(xte), labels), labels)
    print(f"\nlogistic-regression floor on the same features/test: "
          f"{100 * (floor.predict(xte) == yte).mean():.1f}% "
          f"- the MLP has to beat this to be worth its bytes")

    metrics = {"test_acc": float((model.predict(xte) == yte).mean()), "test_windows": float(len(xte)),
               "seed": float(args.seed)}
    if args.export:
        export_header(model, mean, std, labels, cfg, metrics, HEADER_OUT)
        delta = verify_export(model, log_amplitudes(xte_raw), mean, std, HEADER_OUT.read_text())
        size = HEADER_OUT.stat().st_size
        print(f"\nwrote {HEADER_OUT.relative_to(REPO_ROOT)}  ({size / 1024:.1f} KB)")
        print(f"export check: re-reading the header and forward-propagating it in numpy "
              f"reproduces sklearn probabilities to max |dp| = {delta:.2e} "
              f"({'OK' if delta < 1e-4 else 'WRONG - do not flash this'})")
        print("note: that verifies the weights survived serialisation, NOT that firmware/"
              "features.h computes the same features - that check happens on the chip.")
    print("\nCAVEAT worth reading twice: with "
          f"{sum(len(v) for v in usable.values())} sessions the test set is a couple of "
          "sessions per class. Treat the accuracy as 'the chain works', not as a performance "
          "claim; more sessions per class is the only cure.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
