"""Summary table + machine JSON for parsed modbridge-ingame-facts files."""
from __future__ import annotations

from typing import Any, Dict, List

from .parser import ParseReport


def summary_dict(reports: List[ParseReport]) -> Dict[str, Any]:
    files = []
    for r in reports:
        files.append({
            "path": r.path,
            "lines": {
                "total": r.total_lines, "blank": r.blank_lines,
                "ok": r.ok_lines, "repaired": r.repaired_lines,
                "unrepairable": r.unrepairable_lines,
                "invalid_envelope": r.invalid_envelope_lines,
            },
            "build_ids": r.build_ids,
            "unknown_build_lines": r.unknown_build_lines,
            "duplicate_seqs": r.duplicate_seqs,
            "entry_present": r.entry_present,
            "startup_present": r.startup_present,
            "experiments": [e.to_dict() for e in r.experiments],
            "unrepairable": r.unrepairable,
        })
    return {"tool": "modbridge_facts", "files": files,
            "any_fatal": any(r.fatal for r in reports)}


def _fmt(v: Any, width: int) -> str:
    s = "-" if v is None else str(v)
    return s[: width - 1].ljust(width)


def summary_table(reports: List[ParseReport]) -> str:
    out: List[str] = []
    out.append("== modbridge-ingame-facts summary ==")
    for r in reports:
        out.append("")
        out.append("file: %s" % (r.path or "<stdin>"))
        out.append(
            "lines: %d total | ok %d | repaired %d | unrepairable %d | "
            "invalid-envelope %d | blank %d"
            % (r.total_lines, r.ok_lines, r.repaired_lines,
               r.unrepairable_lines, r.invalid_envelope_lines, r.blank_lines)
        )
        if r.build_ids:
            parts = []
            for bid, b in sorted(r.build_ids.items()):
                parts.append("%s (n=%d, seq %s..%s, repaired %d)"
                             % (bid, b["count"], b["seq_min"], b["seq_max"],
                                b["repair_lines"]))
            out.append("build_ids: " + "; ".join(parts))
        else:
            out.append("build_ids: (none)")
        if r.unknown_build_lines:
            out.append("unknown build_id lines (vs --expect-build-id): %s"
                       % r.unknown_build_lines)
        if r.duplicate_seqs:
            out.append("duplicate seqs: %s"
                       % ", ".join("(%s,%d)x%d" % (d["build_id"], d["seq"], d["count"])
                                   for d in r.duplicate_seqs))
        out.append("entry present: %s | startup present: %s"
                   % ("yes" if r.entry_present else "NO",
                      "yes" if r.startup_present else "NO"))
        out.append("")
        if r.experiments:
            out.append("  %-22s %5s %7s %6s %10s  %s"
                       % ("experiment", "rc", "errno", "flds", "needs_base", "candidates"))
            for e in r.experiments:
                cands = "-"
                if e.candidates is not None:
                    names = [str(c) for c in e.candidates]
                    cands = "%d %s" % (len(names), "[" + ", ".join(names[:4]) +
                                       (", ..." if len(names) > 4 else "") + "]")
                out.append("  %-22s %5s %7s %6s %10s  %s" % (
                    _fmt(e.name, 22), _fmt(e.rc, 5), _fmt(e.errno, 7),
                    _fmt(len(e.fields) if e.fields is not None else None, 6),
                    _fmt(e.needs_base, 10), cands))
        else:
            out.append("experiments: (none found)")
        if r.unrepairable:
            out.append("")
            out.append("UNREPAIRABLE LINES (%d) — preserved verbatim, never dropped:"
                       % len(r.unrepairable))
            for u in r.unrepairable:
                raw = u["raw"]
                shown = raw if len(raw) <= 90 else raw[:87] + "..."
                out.append("  line %d: %s" % (u["line_no"], shown))
                out.append("    error: %s" % u["error"])
    return "\n".join(out)
