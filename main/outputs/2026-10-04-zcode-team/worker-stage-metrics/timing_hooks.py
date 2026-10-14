#!/usr/bin/env python3
"""timing_hooks.py — reusable stage timer for the PS5 UE4SS runtime iterate loop.

Import from the preflight runner (or any host-side driver) to time its own
stages with the SAME JSON schema as
outputs/2026-10-04-zcode-team/worker-stage-metrics/results/STAGE-TIMING.md
(and results/stage-timing-l11run3.json, an emitted example).

Canonical stages: tests, compilation, packaging, staging, injection,
initialization. Custom stage names are allowed (prefix custom ones clearly).
Host-side stages only: this module uses a local monotonic clock and never
talks to the console. Console-side per-event numbers (injection/initialization
on the target) come from worker-durable-events/tools/epoch_view.py; record
them here via record_external() so a cycle document is complete.

Usage:
    from timing_hooks import StageTimer
    t = StageTimer(cycle="run-l11-4", actor="preflight-runner")
    t.begin("compilation"); ...build...; t.end("compilation")
    t.begin("packaging");  ...package...; t.end("packaging")
    t.begin("staging");    ...upload...;  t.end("staging")
    # console-side events, measured elsewhere:
    t.record_external("injection", 1250.0, "measured",
                      "source: ploader dispatch receipt utc")
    doc = t.doc()
    t.write("stage-timing-run-l11-4.json")

JSON schema (one document per cycle):
{
  "marker": "modbridge.stage-timing/v1",
  "schema": 1,
  "cycle": "<run label>",
  "actor": "<runner name>",
  "clock": "time.perf_counter_ns (monotonic, host-local)",
  "utc_start": "...", "utc_end": "...",
  "stages": [
    {"stage": "compilation", "duration_ms": 22216.8,
     "t_start_ms": 0.0, "t_end_ms": 22216.8,
     "status": "measured",              # measured|estimated|not_reached|skipped
     "source": "run-l11-1 file mtimes ...",
     "notes": []},
    ...
  ],
  "totals": {"measured_ms": ..., "all_ms": ..., "slowest_stage": "..."}
}

Rules honored from the studio contract: measurements only, no game claims;
a stage that did not run is recorded as "not_reached"/"skipped", never 0.
"""

import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

if sys.version_info >= (3, 10):
    _OPT_LIST = list | None
else:  # py3.9 compat (host runners may be older)
    _OPT_LIST = list

MARKER = "modbridge.stage-timing/v1"
SCHEMA = 1
CANONICAL_STAGES = ("tests", "compilation", "packaging", "staging",
                    "injection", "initialization")


def _now_ms() -> float:
    return time.perf_counter_ns() / 1e6


def _utc() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%fZ")


class StageTimer:
    def __init__(self, cycle: str, actor: str = "unnamed-runner"):
        self.cycle = cycle
        self.actor = actor
        self.t0 = _now_ms()
        self.utc_start = _utc()
        self._open: dict = {}          # stage -> (t_start_ms, notes-so-far)
        self.stages: list = []         # finished/external stage records

    # --- host-side measured stages -------------------------------------
    def begin(self, stage: str) -> None:
        if stage in self._open:
            raise ValueError(f"stage already open: {stage}")
        self._open[stage] = {"t_start_ms": _now_ms(), "notes": []}

    def mark(self, stage: str, note: str) -> None:
        """Attach a note to an open stage (e.g. sub-step boundaries)."""
        if stage not in self._open:
            raise ValueError(f"stage not open: {stage}")
        self._open[stage]["notes"].append(
            {"at_ms": round(_now_ms() - self._open[stage]["t_start_ms"], 3),
             "note": note})

    def end(self, stage: str, source: str, status: str = "measured") -> None:
        if stage not in self._open:
            raise ValueError(f"stage not open: {stage}")
        o = self._open.pop(stage)
        t_end = _now_ms()
        self.stages.append({
            "stage": stage,
            "duration_ms": round(t_end - o["t_start_ms"], 3),
            "t_start_ms": round(o["t_start_ms"] - self.t0, 3),
            "t_end_ms": round(t_end - self.t0, 3),
            "status": status,
            "source": source,
            "notes": o["notes"],
        })

    # --- stages measured elsewhere / not run ----------------------------
    def record_external(self, stage: str, duration_ms: float, status: str,
                        source: str, notes: _OPT_LIST = None) -> None:
        """Record a stage whose duration was measured by another instrument
        (e.g. epoch_view.py per-event t_ms deltas) — keep the schema uniform."""
        if status not in ("measured", "estimated", "not_reached", "skipped"):
            raise ValueError(f"bad status: {status}")
        self.stages.append({
            "stage": stage,
            "duration_ms": (None if status in ("not_reached", "skipped")
                            else round(float(duration_ms), 3)),
            "t_start_ms": None,
            "t_end_ms": None,
            "status": status,
            "source": source,
            "notes": notes or [],
        })

    def skip(self, stage: str, reason: str) -> None:
        self.record_external(stage, None, "skipped", reason)

    def not_reached(self, stage: str, reason: str) -> None:
        self.record_external(stage, None, "not_reached", reason)

    # --- output ----------------------------------------------------------
    def doc(self) -> dict:
        open_leak = sorted(self._open)
        if open_leak:
            raise ValueError(f"stages still open: {open_leak}")
        measured = [s["duration_ms"] for s in self.stages
                    if s["status"] == "measured" and s["duration_ms"]]
        slowest = None
        if self.stages:
            ok = [s for s in self.stages if s["duration_ms"] is not None]
            if ok:
                slowest = max(ok, key=lambda s: s["duration_ms"])["stage"]
        return {
            "marker": MARKER,
            "schema": SCHEMA,
            "cycle": self.cycle,
            "actor": self.actor,
            "clock": "time.perf_counter_ns (monotonic, host-local)",
            "utc_start": self.utc_start,
            "utc_end": _utc(),
            "stages": self.stages,
            "totals": {
                "measured_ms": round(sum(measured), 3),
                "all_ms": round(sum(s["duration_ms"] or 0
                                    for s in self.stages), 3),
                "slowest_stage": slowest,
            },
        }

    def write(self, path) -> dict:
        d = self.doc()
        p = Path(path)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(d, indent=1) + "\n")
        return d


def load_cycle(path) -> dict:
    """Read back a cycle document and assert the schema marker."""
    d = json.loads(Path(path).read_text())
    if d.get("marker") != MARKER or d.get("schema") != SCHEMA:
        raise ValueError(f"not a stage-timing document: {path}")
    return d


if __name__ == "__main__":
    # self-check: emit a tiny synthetic cycle to stdout
    t = StageTimer(cycle="self-check", actor="timing_hooks.__main__")
    t.begin("tests"); time.sleep(0.01); t.end("tests", "self-check sleep")
    t.not_reached("initialization", "no console contact in self-check")
    print(json.dumps(t.doc(), indent=1))
