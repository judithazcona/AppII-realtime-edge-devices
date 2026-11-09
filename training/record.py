#!/usr/bin/env python
"""Record one labelled MPU-6050 session from the board into ``data/raw/``.

Run it through uv (AGENTS.md rule 3 — never a bare ``python``)::

    uv run python training/record.py                       # asks for everything
    uv run python training/record.py --probe               # check the link, write nothing
    uv run python training/record.py --class shaking --session hand_vigorous --duration 45
    uv run python training/record.py --list                # what is already on disk

The board must be running ``firmware/data_recorder`` (see firmware/README.md for the
exact compile + upload commands). Nothing here is wired to a hard-coded sample rate:
the firmware sends its rate, ranges and scales in its stream header, and they are copied
into the CSV, so a recording always carries the settings that produced it.

Recordings land in ``data/raw/`` only — that directory is gitignored, and this script
refuses to write anywhere else (the repository is public, rule 16).

Randomness: the recorder itself draws no random numbers, so nothing here *needs* a seed.
It still calls :func:`config.seed_everything` and stamps ``pipeline_seed`` into every
file, so a recording can be tied to the pipeline constant that will later split and
train on it, and one constant stays in charge (rule 8).
"""

from __future__ import annotations

import argparse
import math
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

from config import (
    CLASSES,
    DEFAULT_BAUD,
    DEFAULT_PORT,
    MIN_SESSION_SECONDS,
    NOVEL_CLASSES,
    RATE_TOLERANCE_HZ,
    RAW_DIR,
    STAGE1_CLASSES,
    TARGET_SESSION_SECONDS,
    SEED,
    seed_everything,
)

import serial  # noqa: E402  (pyserial; added with `uv add pyserial`)

# Keys the firmware emits periodically; they are status, not metadata.
_VOLATILE_HEADER_KEYS = {"progress"}


class LineReader:
    """Line reader over a serial port that tolerates interleaved ``#`` lines."""

    def __init__(self, ser: serial.Serial) -> None:
        self._ser = ser
        self._buf = bytearray()

    def line(self, timeout: float = 1.0) -> str | None:
        """Next newline-terminated line without its newline, or None after `timeout`."""
        deadline = time.monotonic() + timeout
        while True:
            nl = self._buf.find(b"\n")
            if nl >= 0:
                raw = bytes(self._buf[: nl + 1])
                del self._buf[: nl + 1]
                return raw.decode("ascii", "replace").rstrip("\r\n")
            if time.monotonic() > deadline:
                return None
            waiting = self._ser.in_waiting
            chunk = self._ser.read(waiting if waiting else 1)
            if chunk:
                self._buf.extend(chunk)

    def flush(self) -> None:
        self._ser.reset_input_buffer()
        self._buf.clear()


def ask(prompt: str, default: str | None = None, choices: tuple[str, ...] | None = None) -> str:
    if not sys.stdin.isatty():
        raise SystemExit(
            "no terminal to ask questions from (piped input?). Run it in a real terminal, or "
            "pass everything: --class <name> --session <name> --person <who> --placement <where>"
        )
    hint = f" [{default}]" if default else ""
    if choices:
        hint += " (" + " / ".join(choices) + ")"
    while True:
        answer = input(f"{prompt}{hint}: ").strip()
        if not answer and default is not None:
            return default
        if not choices or answer in choices:
            return answer
        print(f"  please answer one of: {', '.join(choices)}")


def slugify(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def next_index(cls: str, prefix: str = "session") -> int:
    """Next free session number for this class, so files never overwrite each other."""
    pattern = re.compile(rf"^{prefix}(\d+)_{re.escape(cls)}__.*\.csv$")
    used = [
        int(m.group(1))
        for p in RAW_DIR.glob(f"{prefix}*_{cls}__*.csv")
        if (m := pattern.match(p.name))
    ]
    return (max(used) + 1) if used else 1


def read_firmware_header(reader: LineReader, timeout: float = 5.0) -> dict[str, str]:
    """Collect the ``# key value`` header the board sends when asked with 'H'.

    Data rows arriving during the collection are ignored on purpose: the board has been
    streaming since it booted, and we only care about the metadata block here.
    """
    meta: dict[str, str] = {}
    deadline = time.monotonic() + timeout
    while "columns" not in meta:
        line = reader.line(timeout=1.0)
        if line is None:
            if time.monotonic() > deadline:
                raise SystemExit(
                    "no header from the board within "
                    f"{timeout:.0f} s. Check: is the port free (close the Arduino monitor)?\n"
                    "  Is firmware/data_recorder the flashed sketch? Wrong COM port?\n"
                    "  Try: arduino-cli board list"
                )
            continue
        line = line.strip()
        if not line or not line.startswith("#"):
            continue
        parts = line[1:].split(None, 1)
        key = parts[0]
        if key not in _VOLATILE_HEADER_KEYS:
            meta[key] = parts[1] if len(parts) > 1 else ""
    return meta


def parse_row(line: str) -> tuple[int, float, float, float] | None:
    """Return (t_us, |a|, ax, az) for a data row, or None if it is not one."""
    fields = line.split(",")
    if len(fields) != 13:
        return None
    try:
        t_us = int(fields[0])
        ax, ay, az = float(fields[1]), float(fields[2]), float(fields[3])
    except ValueError:
        return None
    return t_us, math.sqrt(ax * ax + ay * ay + az * az), ax, az


def record(
    ser: serial.Serial, duration: float
) -> tuple[list[str], list[tuple[int, float, float, float]], dict[str, str]]:
    reader = LineReader(ser)
    ser.write(b"H\n")  # ask for the header instead of hoping we caught the boot one
    ser.flush()
    print("asking the board for its settings ...")
    meta = read_firmware_header(reader)
    if "ERROR" in meta:
        raise SystemExit(f"the board reported an error: {meta['ERROR']}")

    reader.flush()  # drop anything queued during the countdown
    ser.write(b"B\n")
    ser.flush()
    started = False
    deadline = time.monotonic() + 3.0
    while not started and time.monotonic() < deadline:
        line = reader.line(1.0)
        if line and line.strip().startswith("# begin"):
            started = True
    if not started:
        raise SystemExit(
            "the board never answered the 'B' start marker. Is firmware/data_recorder the\n"
            "  sketch that is flashed? (bench_test does not answer it.)"
        )

    rows: list[str] = []
    samples: list[tuple[int, float, float, float]] = []
    end = time.monotonic() + duration
    while time.monotonic() < end:
        line = reader.line(1.0)
        if line is None:
            continue
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parsed = parse_row(line)
        if parsed is None:
            continue
        rows.append(line)
        samples.append(parsed)
    return rows, samples, meta


def statistics_summary(samples: list[tuple[int, float, float, float]]) -> dict[str, float]:
    ts = [s[0] for s in samples]
    dts = sorted((b - a) / 1000.0 for a, b in zip(ts, ts[1:]))  # ms
    span_s = (ts[-1] - ts[0]) / 1e6 if len(ts) > 1 else 0.0
    period_ms = dts[len(dts) // 2] if dts else 0.0
    return {
        "measured_hz": (len(ts) - 1) / span_s if span_s > 0 else 0.0,
        "median_dt_ms": period_ms,
        "p99_dt_ms": dts[int(0.99 * (len(dts) - 1))] if dts else 0.0,
        "gaps": sum(1 for d in dts if period_ms and d > 1.75 * period_ms),
        "span_s": span_s,
        "a_max": max((s[1] for s in samples), default=0.0),
        "ax_min": min((s[2] for s in samples), default=0.0),
        "ax_max": max((s[2] for s in samples), default=0.0),
        "az_min": min((s[3] for s in samples), default=0.0),
        "az_max": max((s[3] for s in samples), default=0.0),
    }


def list_sessions() -> None:
    files = sorted(RAW_DIR.glob("*session*.csv")) if RAW_DIR.exists() else []
    if not files:
        print(f"no sessions recorded yet in {RAW_DIR}")
        return
    print(f"{'file':<52} {'samples':>8} {'length s':>9} {'Hz':>6}  usable")
    tally: dict[str, int] = {}
    for path in files:
        meta: dict[str, str] = {}
        ts: list[int] = []
        for line in path.read_text().splitlines():
            if line.startswith("#"):
                parts = line[1:].split(None, 1)
                if len(parts) == 2:
                    meta[parts[0]] = parts[1]
                continue
            parsed = parse_row(line)
            if parsed:
                ts.append(parsed[0])
        span = (ts[-1] - ts[0]) / 1e6 if len(ts) > 1 else 0.0
        hz = (len(ts) - 1) / span if span > 0 else 0.0
        # A take is only usable if it is long enough and was not a link smoke test.
        usable = span >= MIN_SESSION_SECONDS and "smoke_test" not in path.name
        if usable:
            tally[meta.get("class", "?")] = tally.get(meta.get("class", "?"), 0) + 1
        flag = "yes" if usable else f"no ({span:.0f} s smoke test)"
        print(f"{path.name:<52} {len(ts):>8} {span:>9.1f} {hz:>6.1f}  {flag}")
    print("\nsessions per class that count towards the dataset:")
    for cls in sorted(tally):
        print(f"  {cls:<16} {tally[cls]}")
    if not tally:
        print("  (none yet - the smoke tests above are link checks, not data)")


def probe(args: argparse.Namespace) -> int:
    """Open the board for 3 s, report the delivered rate, write nothing.

    Needs no label: it is the "is the link alive and at the right rate" check you run
    before committing anyone to a 45-second take.
    """
    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        rows, samples, fw_meta = record(ser, 3.0)
    if not samples:
        raise SystemExit("no samples arrived. Is the port free and data_recorder flashed?")
    stats = statistics_summary(samples)
    fw_rate = float(fw_meta.get("rate_hz", 0) or 0)
    off = abs(stats["measured_hz"] - fw_rate) > RATE_TOLERANCE_HZ
    print(f"\nprobe: {len(samples)} samples in {stats['span_s']:.2f} s "
          f"-> {stats['measured_hz']:.2f} Hz measured (firmware asked for {fw_rate:.0f} Hz)")
    print(f"timing  median dt {stats['median_dt_ms']:.2f} ms   "
          f"p99 {stats['p99_dt_ms']:.2f} ms   gaps {stats['gaps']}")
    print(f"|a| max {stats['a_max']:.3f} g   az {stats['az_min']:.3f}..{stats['az_max']:.3f} g")
    print("first rows:")
    for line in rows[:3]:
        print("  " + line)
    print("WARN delivered rate is off by more than "
          f"{RATE_TOLERANCE_HZ} Hz - fix that before recording real sessions" if off
          else "OK the delivered rate matches what the firmware was configured for")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="record one labelled session from the board")
    ap.add_argument("--port", default=DEFAULT_PORT)
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    ap.add_argument(
        "--class",
        dest="cls",
        choices=sorted(STAGE1_CLASSES + NOVEL_CLASSES),
        help="the two stage-1 classes, or a never-trained-on 'novel_*' one for the "
             "abstention extension",
    )
    ap.add_argument("--session", help="short name for the condition, e.g. table_still")
    ap.add_argument("--person", default="", help="who performed the motion")
    ap.add_argument("--placement", default="", help="where the board was / how it was held")
    ap.add_argument("--condition", default="", help="anything a reader should know about this take")
    ap.add_argument("--duration", type=float, default=TARGET_SESSION_SECONDS)
    ap.add_argument("--probe", action="store_true", help="read 3 s, print the rate, write nothing")
    ap.add_argument("--list", action="store_true", help="list recorded sessions and exit")
    args = ap.parse_args()

    if args.list:
        list_sessions()
        return 0

    seed_everything(SEED)

    if args.probe:
        return probe(args)

    allowed = tuple(sorted(STAGE1_CLASSES + NOVEL_CLASSES))
    cls = args.cls or ask("class to record", choices=allowed)
    if cls not in allowed:
        raise SystemExit(
            "stage 1 records " + ", ".join(STAGE1_CLASSES) + " plus the held-out "
            + ", ".join(NOVEL_CLASSES) + "; the chest classes come with step 7"
        )
    novel = cls in NOVEL_CLASSES
    session = args.session or ask("session name (short, describes the take)", default="take_1")
    person = args.person if args.session else ask("person", default="judith")
    placement = args.placement if args.session else ask("placement", default="")

    duration = args.duration
    if duration < MIN_SESSION_SECONDS:
        raise SystemExit(f"{duration}s is too short: a 2.5 s window needs at least "
                         f"{MIN_SESSION_SECONDS}s of signal to be worth anything")

    if novel:
        colour, meaning = "none", "never trained on — held out to show the device abstaining"
    else:
        colour, meaning, _stage = CLASSES[cls]
    print(f"\nclass     {cls}  (LED {colour}) - {meaning}")
    print(f"session   {session}")
    print(f"duration  {duration:.0f} s\n")
    print("3")
    for n in ("2", "1"):
        time.sleep(1.0)
        print(n)
    print("GO - start the motion now and keep it going for the whole session.")

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        rows, samples, fw_meta = record(ser, duration)

    if not samples:
        raise SystemExit("no samples arrived. Is the port free and data_recorder flashed?")

    stats = statistics_summary(samples)
    fw_rate = float(fw_meta.get("rate_hz", 0) or 0)
    rate_off = abs(stats["measured_hz"] - fw_rate) > RATE_TOLERANCE_HZ

    # --- write the CSV, in data/raw only -------------------------------------
    RAW_DIR.mkdir(parents=True, exist_ok=True)
    prefix = "novel_session" if novel else "session"
    name = f"{prefix}{next_index(cls, prefix):03d}_{cls}__{slugify(session)}.csv"
    dest = (RAW_DIR / name).resolve()
    if dest.parent != RAW_DIR.resolve():
        raise SystemExit(f"refusing to write outside {RAW_DIR}")

    header_lines = [
        f"# session_name {session}",
        f"# class {cls}",
        f"# trainable {'no' if novel else 'yes'}",
        f"# held_out {'never trained on - material for the abstention extension' if novel else 'no'}",
        f"# led_colour {colour}",
        f"# person {person or 'unstated'}",
        f"# placement {placement or 'unstated'}",
        f"# condition {args.condition or 'unstated'}",
        f"# recorded_at {datetime.now().astimezone().isoformat(timespec='seconds')}",
        f"# samples {len(samples)}",
        f"# recorded_span_s {stats['span_s']:.3f}",
        f"# measured_rate_hz {stats['measured_hz']:.2f}",
        f"# median_dt_ms {stats['median_dt_ms']:.3f}",
        f"# gaps_gt_1_75_period {stats['gaps']}",
        f"# abs_accel_max_g {stats['a_max']:.3f}",
        f"# pipeline_seed {SEED}",
        "# recorder training/record.py + firmware/data_recorder.ino",
        "# --- reported by the board itself in its stream header ---",
    ]
    header_lines += [f"# fw_{k} {v}".rstrip() for k, v in sorted(fw_meta.items())]
    columns = fw_meta.get("columns", "t_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,"
                                   "ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw")
    header_lines.append(columns)  # last non-# line before the data: the column names
    body = "\n".join(header_lines + rows) + "\n"
    tmp = dest.with_suffix(".csv.part")
    tmp.write_text(body, newline="\n")
    tmp.replace(dest)

    ignored = subprocess.run(
        ["git", "check-ignore", "-q", str(dest)], capture_output=True
    ).returncode == 0

    print(f"\nsaved   {dest.relative_to(RAW_DIR.parents[0])}  ({dest.stat().st_size/1024:.1f} KB)")
    print(f"samples {len(samples)} in {stats['span_s']:.2f} s  ->  {stats['measured_hz']:.2f} Hz measured")
    print(f"timing  median dt {stats['median_dt_ms']:.2f} ms   p99 {stats['p99_dt_ms']:.2f} ms   "
          f"gaps {stats['gaps']}")
    print(f"range   |a| max {stats['a_max']:.2f} g   ax {stats['ax_min']:.2f}..{stats['ax_max']:.2f} g   "
          f"az {stats['az_min']:.2f}..{stats['az_max']:.2f} g")
    print(f"git     {name} ignored by .gitignore: {'yes' if ignored else 'NO - STOP, this would publish raw data'}")
    if rate_off:
        print(f"WARN    delivered {stats['measured_hz']:.1f} Hz but the firmware asked for "
              f"{fw_rate:.0f} Hz - more than {RATE_TOLERANCE_HZ} Hz apart, find out why before "
              f"recording the real sessions")
    if len(samples) and stats["a_max"] > 7.9:
        print("WARN    the accelerometer saturated (|a| > 7.9 g of a +-8 g range): peaks are clipped")
    if stats["span_s"] < MIN_SESSION_SECONDS:
        print(f"WARN    {stats['span_s']:.1f} s is short for a 2.5 s window; consider re-recording")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
