#!/usr/bin/env python3
"""find_root_v2 — hardened, versioned workspace-root resolution.

Replaces the marker-walk in the FROZEN packaging script
    outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py
    (_find_root(), lines 74-83, frozen sha256
     e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313)

Incident this hardens (see worker-stray-root-audit AUDIT.md/REPORT.md):
    a stray bare `.superpowers/` dir staged at
    outputs/2026-10-04-zcode-team/ by a botched vendor build.py staging copy
    (build b33f79f5..., worker-seh64-trampoline-fix, 2026-10-05 05:42:47)
    shadowed root resolution for every driver loading the frozen file; two
    packaging runs failed closed with 12 lineage gaps (walkguard run-a,
    r3-armed run-a). Same bug class had already fired 2026-10-04 17:08
    (worker-crt-integrate vendor misroot).

Differences vs frozen _find_root():
    frozen                                      v2
    ----                                        ----
    marker: any dir named `.superpowers`        STRONG marker required:
                                                `.superpowers/native-observations`
                                                AND >= 1 corroborating marker
                                                (ledger file | README-RETURN.md | sdd/)
    walks up 6 levels                           walks up to 8 ancestors
    silent first-match (shadowable)             ambiguity FAILS LOUD:
                                                  - multiple strong candidates
                                                  - any weak-marker-only match
                                                    (bare `.superpowers`, or
                                                    native-observations without
                                                    corroboration) on the walked
                                                    path
                                                --root override honored
    raises RuntimeError("workspace root         raises RootResolutionError,
    containing .superpowers not found")         a RuntimeError SUBCLASS with the
                                                candidates attached -- catch-
                                                compatible with the frozen
                                                contract.

Frozen contract matched exactly (call sites package_prx.py:85 `ROOT =
_find_root()`, consumed at :86 SPIKE and :89-123 PIN):
    * no-arg call -> pathlib.Path (resolved) to the workspace root
    * failure raises RuntimeError
Default `start` mirrors the frozen mechanism (the loading module's own
directory). For byte-exact parity with what the frozen file would resolve,
pass the FROZEN FILE's directory explicitly:
    find_root(start=PKG_FIXED.parent)

CLI (the --root override):
    python3 find_root_v2.py [--start DIR] [--root DIR]
                            [--max-ancestors N] [--weak-markers fail|warn|ignore]
                            [--allow-weak-override] [--json]

LOCAL-ONLY; stdlib only; read-only on the filesystem it inspects.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Iterable, List, Optional, Sequence, Tuple

__version__ = "2.0.0"
__find_root_edition__ = "v2-strong-marker-8-ancestors"

# ---------------------------------------------------------------- markers
# Enumerated 2026-10-05 at the true workspace root
# /WORKSPACE/.codex/worktrees/translation-core/what:
#   .superpowers/{native-dependency-spike, native-observations,
#                 native-read-at-review-scratch, native-read-at-spike,
#                 native-sdk-build-check, sdd}
#   .superpowers/native-observations/transport-experiments-progress.json
#   README-RETURN.md
# The 05:42 stray had ONLY native-dependency-spike -> matches none of the
# strong rules below.

# Required deep marker: exists only at the true root (the stray lacked it).
STRONG_MARKER_REL = ".superpowers/native-observations"
WEAK_MARKER_REL = ".superpowers"

# Corroborating markers: a STRONG root needs >= CORROBORATION_MIN of these.
# Ordered by stability: the shared ledger is the load-bearing artifact of the
# whole spike/packaging lineage; README-RETURN.md marks the workspace root
# itself; .superpowers/sdd is a long-lived planning dir.
CORROBORATING_MARKERS: Tuple[Tuple[str, str], ...] = (
    (".superpowers/native-observations/transport-experiments-progress.json", "file"),
    ("README-RETURN.md", "file"),
    (".superpowers/sdd", "dir"),
)
CORROBORATION_MIN = 1


class RootResolutionError(RuntimeError):
    """Loud, candidate-listing root-resolution failure.

    Subclasses RuntimeError so any driver that today catches the frozen
    _find_root()'s RuntimeError keeps working unchanged.
    """

    def __init__(
        self,
        message: str,
        *,
        kind: str,
        candidates: Sequence[Path] = (),
        weak_decoys: Sequence[Tuple[Path, str]] = (),
        walked: Sequence[Path] = (),
    ) -> None:
        super().__init__(message)
        self.kind = kind              # 'no-root' | 'ambiguous-multiple' | 'weak-decoy' | 'bad-override'
        self.candidates = list(candidates)
        self.weak_decoys = list(weak_decoys)
        self.walked = list(walked)


# ---------------------------------------------------------------- classification
def _marker_missing(p: Path, rel: str, kind: str) -> bool:
    q = p / rel
    return not (q.is_dir() if kind == "dir" else q.is_file())


def classify_candidate(p: Path) -> Tuple[Optional[str], List[str], Optional[str]]:
    """Classify one directory.

    Returns (verdict, corroborating_markers_present, weak_reason) where
    verdict is "strong", "weak", or None (not a marker match at all).
    """
    if not (p / WEAK_MARKER_REL).is_dir():
        return None, [], None
    if not (p / STRONG_MARKER_REL).is_dir():
        return "weak", [], f"bare {WEAK_MARKER_REL}/ without {STRONG_MARKER_REL}/"
    present = [rel for rel, _kind in CORROBORATING_MARKERS if not _marker_missing(p, rel, _kind)]
    if len(present) < CORROBORATION_MIN:
        missing = ", ".join(rel for rel, _ in CORROBORATING_MARKERS)
        return ("weak", [], f"{STRONG_MARKER_REL}/ present but no corroborating "
                            f"marker (need >= {CORROBORATION_MIN} of: {missing})")
    return "strong", present, None


# ---------------------------------------------------------------- resolution
def find_root(
    start: Optional[Path] = None,
    *,
    override: Optional[Path] = None,
    max_ancestors: int = 8,
    weak_markers: str = "fail",
    allow_weak_override: bool = False,
) -> Path:
    """Resolve the workspace root; drop-in for frozen `_find_root()`.

    start          directory to walk up FROM; default = this file's directory
                   (mirrors the frozen BASE mechanism). For exact frozen-file
                   parity pass the frozen file's parent, e.g.
                   `find_root(start=PKG_FIXED.parent)`.
    override       --root escape hatch: return this path after verification
                   (fails loud unless it classifies strong, or
                   allow_weak_override=True is set deliberately).
    max_ancestors  at most this many parent hops above `start` are inspected
                   (v2 default 8; frozen inspected 6 nodes total).
    weak_markers   what to do when a weak-marker-only dir (bare `.superpowers`,
                   or native-observations without corroboration) sits on the
                   walked path: "fail" (default; the stray condition),
                   "warn" (stderr note, keep resolving), "ignore".
    """
    if weak_markers not in ("fail", "warn", "ignore"):
        raise ValueError(f"weak_markers must be fail|warn|ignore, got {weak_markers!r}")

    # --root override: honored, but verified so it cannot silently re-create
    # the misroot it is escaping.
    if override is not None:
        ov = Path(override).expanduser().resolve()
        verdict, present, reason = classify_candidate(ov)
        if verdict == "strong":
            return ov
        if allow_weak_override:
            print(f"[find_root_v2] WARNING: --root override accepted WITHOUT the "
                  f"strong-marker test ({reason}): {ov}", file=sys.stderr)
            return ov
        raise RootResolutionError(
            f"find_root_v2: --root override rejected: {ov}\n"
            f"  reason: {reason or f'no {WEAK_MARKER_REL}/ dir present at all'}\n"
            f"  hint: pass --allow-weak-override to accept deliberately.",
            kind="bad-override", candidates=[ov],
        )

    start_dir = Path(start).expanduser().resolve() if start is not None \
        else Path(__file__).resolve().parent

    # start itself + up to max_ancestors hops (frozen checked 6 nodes; v2 >= frozen).
    nodes: List[Path] = [start_dir] + list(start_dir.parents)[:max_ancestors]

    strongs: List[Path] = []
    corroboration: dict = {}
    weaks: List[Tuple[Path, str]] = []
    for node in nodes:
        verdict, present, reason = classify_candidate(node)
        if verdict == "strong":
            strongs.append(node)
            corroboration[node] = present
        elif verdict == "weak":
            weaks.append((node, reason))

    if not strongs:
        lines = [
            "find_root_v2: workspace root NOT found (no strong-marker candidate).",
            f"  required: dir {STRONG_MARKER_REL}/ + >= {CORROBORATION_MIN} of "
            f"{[rel for rel, _ in CORROBORATING_MARKERS]}",
            f"  walked from {start_dir} up to {max_ancestors} ancestor(s):",
            *[f"    - {n}" for n in nodes],
        ]
        if weaks:
            lines.append("  weak-marker-only decoys seen (NOT trusted):")
            lines += [f"    - {p}  ({why})" for p, why in weaks]
        lines.append("  hint: pass --root DIR to override deliberately.")
        raise RootResolutionError("\n".join(lines), kind="no-root",
                                  weak_decoys=weaks, walked=nodes)

    if weaks and weak_markers == "fail":
        raise RootResolutionError(
            "find_root_v2: AMBIGUOUS root resolution — weak-marker-only dir(s) "
            "on the walked path (the 2026-10-05 05:42 stray condition). "
            "Refusing to guess.\n"
            + "".join(f"  weak decoy: {p}\n              ({why})\n" for p, why in weaks)
            + "".join(f"  strong candidate: {p}  (corroboration: {corroboration[p]})\n"
                      for p in strongs)
            + f"  walked from {start_dir} up to {max_ancestors} ancestor(s)\n"
            "  hint: quarantine/rename the stray (keep bytes), or pass --root DIR.",
            kind="weak-decoy", candidates=strongs, weak_decoys=weaks, walked=nodes,
        )
    if weaks and weak_markers == "warn":
        for p, why in weaks:
            print(f"[find_root_v2] WARNING: weak-marker dir on walked path, "
                  f"ignored by policy: {p} ({why})", file=sys.stderr)

    if len(strongs) > 1:
        raise RootResolutionError(
            "find_root_v2: AMBIGUOUS root resolution — multiple strong-marker "
            "candidates on the walked path. Refusing to guess.\n"
            + "".join(f"  candidate: {p}  (corroboration: {corroboration[p]})\n"
                      for p in strongs)
            + ("".join(f"  weak decoy: {p}  ({why})\n" for p, why in weaks))
            + f"  walked from {start_dir} up to {max_ancestors} ancestor(s)\n"
            "  hint: pass --root DIR to pick one deliberately.",
            kind="ambiguous-multiple", candidates=strongs, weak_decoys=weaks,
            walked=nodes,
        )

    return strongs[0].resolve()


# ---------------------------------------------------------------- CLI
def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="find_root_v2: hardened workspace-root resolution "
                    "(strong-marker, loud-ambiguity, --root override).")
    ap.add_argument("--start", type=Path, default=None,
                    help="directory to walk up from (default: this file's dir)")
    ap.add_argument("--root", type=Path, default=None, dest="override",
                    help="override the resolved root (verified; see "
                         "--allow-weak-override)")
    ap.add_argument("--max-ancestors", type=int, default=8)
    ap.add_argument("--weak-markers", choices=("fail", "warn", "ignore"),
                    default="fail")
    ap.add_argument("--allow-weak-override", action="store_true",
                    help="accept --root even if it lacks the strong marker")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    args = ap.parse_args(argv)

    try:
        root = find_root(
            start=args.start, override=args.override,
            max_ancestors=args.max_ancestors,
            weak_markers=args.weak_markers,
            allow_weak_override=args.allow_weak_override,
        )
    except RootResolutionError as e:
        if args.json:
            print(json.dumps({
                "ok": False, "edition": __find_root_edition__,
                "kind": e.kind,
                "candidates": [str(p) for p in e.candidates],
                "weak_decoys": [{"path": str(p), "reason": why}
                                for p, why in e.weak_decoys],
                "walked": [str(p) for p in e.walked],
                "error": str(e),
            }, indent=2))
        else:
            print(f"FATAL: {e}", file=sys.stderr)
        return 2

    if args.json:
        _, present, _ = classify_candidate(root)
        print(json.dumps({
            "ok": True, "edition": __find_root_edition__,
            "root": str(root),
            "corroboration": present,
        }, indent=2))
    else:
        print(root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
