"""Durable host-side parser for /data/modbridge-ingame-facts.jsonl.

Design contract (from dispatch):
  * handles the v1/v2 missing-comma malformation deterministically
  * validates the envelope per build_id
  * extracts per-experiment verdicts (rc/errno/fields/needs_base/candidates)
  * never silently drops a line: unrepairable lines are preserved verbatim
    and force a nonzero exit in parse mode

PROVENANCE NOTE (honest): the two real console logs the repair rules were
supposed to be derived from were absent from this workspace at build time
(see ../CLAIM.md). The repair below therefore implements the documented
malformation *class* -- a complete JSON value immediately followed by another
value/key with no separating comma -- as a deterministic, fully logged
transformation. When the real logs surface, run the CLI on them; if any line
is unrepairable the tool exits nonzero with the raw line preserved, which is
the signal to refine `_repair_missing_commas` from real evidence.

VENDORED PATCH (worker-parser-validation, 2026-10-04, after running against
ALL real logs in outputs/2026-10-04-zcode-controller/console/):
  1. Envelope seq alias: owned-dll console logs (modbridge.owned-dll marker,
     48 real lines) use `sequence`, not `seq`. Envelope validation now accepts
     either key (`seq` preferred, `sequence` fallback); lines with neither are
     still invalid-envelope. Never a silent mutation of the record.
  2. entry/startup presence: real igf logs encode lifecycle experiments as
     `"exp":"entry"` / `"exp":"startup"`; `exp` added to FLAG_EVENT_KEYS so
     entry_present/startup_present reflect reality.
Original upstream: outputs/2026-10-04-zcode-team/worker-ingame-log-parser/
(deltas limited to the two items above; everything else byte-identical).
"""
from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple

_VALUE_START = set('"[{') | set("-0123456789") | set("tfn")
_LITERAL_ENDS = ("true", "false", "null")
_MAX_REPAIRS = 64

FLAG_KEYS = ("entry", "startup")
FLAG_EVENT_KEYS = ("event", "kind", "type", "phase", "stage", "exp")

SEQ_KEYS = ("seq", "sequence")


def _envelope_seq(obj: Dict[str, Any]) -> Tuple[Optional[int], Optional[str]]:
    """Effective envelope sequence value: `seq`, falling back to the
    modbridge.owned-dll `sequence` variant. Returns (value, key_used), or
    (None, None) when neither key holds a plain integer."""
    for key in SEQ_KEYS:
        v = obj.get(key)
        if v is not None and not isinstance(v, bool) and isinstance(v, int):
            return v, key
    return None, None


class UnrepairableLineError(ValueError):
    """Raised when a line cannot be repaired into valid JSON."""


def _prev_ends_value(s: str, pos: int) -> bool:
    """True if a complete JSON value ends just before whitespace-run ending at pos."""
    i = pos - 1
    while i >= 0 and s[i] in " \t\r\n":
        i -= 1
    if i < 0:
        return False
    c = s[i]
    if c in '"}]' or c.isdigit():
        return True
    head = s[: i + 1]
    return any(head.endswith(lit) for lit in _LITERAL_ENDS)


def _repair_missing_commas(line: str) -> Tuple[str, List[int]]:
    """Repair the v1/v2 missing-comma malformation.

    Deterministically insert ',' wherever the JSON parser hits
    "Expecting ',' delimiter" and a new value starts where a previous value
    just ended. Returns (repaired_line, insertion_offsets). Offsets index into
    the *repaired* string (each insert shifts later offsets by +1 cumulative;
    offsets are recorded as inserted, in order, so consumers can reconstruct).
    Raises UnrepairableLineError for anything else (truncated line, invalid
    token, top-level "Extra data", ...). Never silent.
    """
    cur = line
    offsets: List[int] = []
    for _ in range(_MAX_REPAIRS):
        try:
            json.loads(cur)
            return cur, offsets
        except json.JSONDecodeError as e:
            if e.msg != "Expecting ',' delimiter":
                raise UnrepairableLineError(
                    "%s at column %d" % (e.msg, e.pos + 1)
                ) from None
            j = e.pos
            if j >= len(cur) or cur[j] not in _VALUE_START:
                k = j
                while k < len(cur) and cur[k] in " \t\r\n":
                    k += 1
                if k < len(cur) and cur[k] in _VALUE_START and _prev_ends_value(cur, k):
                    j = k
                else:
                    raise UnrepairableLineError(
                        "no new value starts at error column %d "
                        "(truncated line or unsupported malformation)" % (e.pos + 1)
                    ) from None
            if not _prev_ends_value(cur, j):
                raise UnrepairableLineError(
                    "no complete value ends before column %d" % (j + 1)
                ) from None
            cur = cur[:j] + "," + cur[j:]
            offsets.append(j)
    raise UnrepairableLineError(
        "exceeded %d comma repairs on one line" % _MAX_REPAIRS
    )


@dataclass
class ExperimentVerdict:
    name: str
    rc: Any
    errno: Any = None
    fields: Optional[Dict[str, Any]] = None
    needs_base: Optional[bool] = None
    candidates: Optional[List[Any]] = None
    source_line: int = -1

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "rc": self.rc,
            "errno": self.errno,
            "fields": self.fields,
            "needs_base": self.needs_base,
            "candidates": self.candidates,
            "source_line": self.source_line,
        }


@dataclass
class LineOutcome:
    line_no: int
    raw: str
    record: Optional[Dict[str, Any]]
    repairs: List[int] = field(default_factory=list)
    repaired_line: Optional[str] = None
    anomalies: List[str] = field(default_factory=list)  # invalid-envelope / dup / unknown-build


@dataclass
class ParseReport:
    path: str = ""
    total_lines: int = 0
    blank_lines: int = 0
    ok_lines: int = 0
    repaired_lines: int = 0
    unrepairable_lines: int = 0
    invalid_envelope_lines: int = 0
    build_ids: Dict[str, Dict[str, Any]] = field(default_factory=dict)
    duplicate_seqs: List[Dict[str, Any]] = field(default_factory=list)
    unknown_build_lines: List[int] = field(default_factory=list)
    entry_present: bool = False
    startup_present: bool = False
    experiments: List[ExperimentVerdict] = field(default_factory=list)
    unrepairable: List[Dict[str, Any]] = field(default_factory=list)

    @property
    def fatal(self) -> bool:
        return self.unrepairable_lines > 0 or self.invalid_envelope_lines > 0


def _collect_experiments(obj: Any, parent_key: Optional[str],
                         out: List[Tuple[Optional[str], Dict[str, Any]]]) -> None:
    if isinstance(obj, dict):
        rc = obj.get("rc")
        if rc is not None and not isinstance(rc, bool) and isinstance(rc, (int, float)):
            out.append((parent_key, obj))
        for k, v in obj.items():
            _collect_experiments(v, k, out)
    elif isinstance(obj, list):
        for v in obj:
            _collect_experiments(v, parent_key, out)


def _scan_flags(obj: Any, flags: set) -> None:
    if isinstance(obj, dict):
        for k, v in obj.items():
            kl = str(k).lower()
            if kl in FLAG_KEYS:
                flags.add(kl)
            if (kl in FLAG_EVENT_KEYS and isinstance(v, str)
                    and v.lower() in FLAG_KEYS):
                flags.add(v.lower())
            _scan_flags(v, flags)
    elif isinstance(obj, list):
        for v in obj:
            _scan_flags(v, flags)


def parse_line(line_no: int, raw_line: str) -> LineOutcome:
    raw = raw_line.rstrip("\r\n")
    if not raw.strip():
        return LineOutcome(line_no=line_no, raw=raw, record=None, anomalies=["blank"])
    try:
        obj, repairs, repaired = _attempt_parse(raw)
    except UnrepairableLineError as e:
        return LineOutcome(
            line_no=line_no, raw=raw, record=None,
            anomalies=["unrepairable: %s" % e],
            repaired_line=None,
        )
    out = LineOutcome(line_no=line_no, raw=raw, record=obj,
                      repairs=repairs, repaired_line=repaired if repairs else None)
    if not isinstance(obj, dict):
        out.anomalies.append("invalid-envelope: top-level value is %s, not object"
                             % type(obj).__name__)
    else:
        build_id = obj.get("build_id")
        if not isinstance(build_id, str) or not build_id.strip():
            out.anomalies.append("invalid-envelope: missing/empty build_id")
        seq, _seq_key = _envelope_seq(obj)
        if seq is None:
            out.anomalies.append(
                "invalid-envelope: missing/non-integer seq (and sequence)")
    return out


def _attempt_parse(raw: str) -> Tuple[Any, List[int], str]:
    try:
        return json.loads(raw), [], raw
    except json.JSONDecodeError:
        pass
    repaired, offsets = _repair_missing_commas(raw)
    return json.loads(repaired), offsets, repaired


def parse_lines(lines: Iterable[str], path: str = "",
                expect_build_ids: Optional[Sequence[str]] = None
                ) -> Tuple[List[LineOutcome], ParseReport]:
    outcomes: List[LineOutcome] = []
    report = ParseReport(path=path)
    expected = set(expect_build_ids) if expect_build_ids else None
    seq_seen: Dict[Tuple[str, int], int] = {}
    flags: set = set()
    exp_idx = 0

    for line_no, raw_line in enumerate(lines, start=1):
        report.total_lines += 1
        oc = parse_line(line_no, raw_line)
        outcomes.append(oc)
        if "blank" in oc.anomalies:
            report.blank_lines += 1
            continue
        if any(a.startswith("unrepairable") for a in oc.anomalies):
            report.unrepairable_lines += 1
            report.unrepairable.append({
                "line_no": line_no, "raw": oc.raw, "error": oc.anomalies[0],
            })
            continue
        if oc.repairs:
            report.repaired_lines += 1
        else:
            report.ok_lines += 1
        assert oc.record is not None
        env_bad = [a for a in oc.anomalies if a.startswith("invalid-envelope")]
        if env_bad:
            report.invalid_envelope_lines += 1
            continue
        build_id = oc.record["build_id"]
        seq, _seq_key = _envelope_seq(oc.record)
        b = report.build_ids.setdefault(build_id, {
            "count": 0, "seq_min": seq, "seq_max": seq, "repair_lines": 0,
        })
        b["count"] += 1
        b["seq_min"] = min(b["seq_min"], seq)
        b["seq_max"] = max(b["seq_max"], seq)
        if oc.repairs:
            b["repair_lines"] += 1
        key = (build_id, seq)
        seq_seen[key] = seq_seen.get(key, 0) + 1
        if expected is not None and build_id not in expected:
            oc.anomalies.append("unknown-build: %r" % build_id)
            report.unknown_build_lines.append(line_no)
        _scan_flags(oc.record, flags)
        pairs: List[Tuple[Optional[str], Dict[str, Any]]] = []
        _collect_experiments(oc.record, None, pairs)
        for parent_key, obj in pairs:
            name = None
            for nk in ("experiment", "name", "id", "exp"):
                v = obj.get(nk)
                if isinstance(v, str) and v.strip():
                    name = v
                    break
            if name is None and parent_key not in (None, "experiments", "results", "verdicts"):
                name = parent_key
            if name is None:
                name = "exp#L%d#%d" % (line_no, exp_idx)
            exp_idx += 1
            fields = obj.get("fields")
            candidates = obj.get("candidates")
            report.experiments.append(ExperimentVerdict(
                name=name,
                rc=obj["rc"],
                errno=obj.get("errno"),
                fields=fields if isinstance(fields, dict) else None,
                needs_base=obj.get("needs_base") if isinstance(obj.get("needs_base"), bool) else None,
                candidates=candidates if isinstance(candidates, list) else None,
                source_line=line_no,
            ))

    report.entry_present = "entry" in flags
    report.startup_present = "startup" in flags
    report.duplicate_seqs = [
        {"build_id": bid, "seq": seq, "count": n}
        for (bid, seq), n in sorted(seq_seen.items()) if n > 1
    ]
    return outcomes, report


def parse_file(path: str, expect_build_ids: Optional[Sequence[str]] = None
               ) -> Tuple[List[LineOutcome], ParseReport]:
    with open(path, "r", encoding="utf-8", errors="surrogateescape") as f:
        return parse_lines(f, path=path, expect_build_ids=expect_build_ids)
