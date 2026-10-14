#!/usr/bin/env python3
"""release_audit.py -- PHASE A (agent GI): Rule-10 blind-test law audit.

Proves (or finds violations of) the no-game-names law for a candidate release
tree: given game names / title-ids, list EVERY textual occurrence in the tree,
classified by the Rule-10 boundary proposed in FIRST-LAUNCH-AUTOMATION.md
section 4:

  RUNTIME          code that compiles into the shipped PRX (src/, include/,
                   generated_include/, profile units). Game names/title-ids/
                   offsets here are VIOLATIONS -- they must go.
  PROFILE-TABLE    shipped per-version layout tables + generated profile
                   artifacts. Engine-level facts allowed; per-GAME facts are
                   violations (a generated per-game profile JSON is allowed
                   ONLY under a game-scoped profile slot path, never inside
                   the tool's shared tables).
  DIAGNOSTIC       receipts, probe/test scripts, docs, configs that name the
                   CURRENT development game. ALLOWED for the CURRENT game
                   only; a FUTURE/BLIND-TEST game name here is a violation
                   (it would leak test expectations into the repo).
  IGNORE           binary files, .git, archives, logs harvested from the
                   console (their game names came from the game itself, not
                   our sources).

Static, read-only, Mac-local. No console contact. Exit codes:
  0 = clean (zero violations)   1 = warnings only   2 = VIOLATIONS found
  3 = usage/IO error

Usage (placeholder terms -- the tool itself must audit clean, so no real
game name is hardcoded or exemplified anywhere in this file):
  python3 release_audit.py --game "GameFoo|Game Foo|PPSA99999" \
                           [--game "GameBar|PPSA88888"] \
                           [--current-game "GameFoo|Game Foo|PPSA99999"] \
                           [--root WHAT_DIR] [--scope runtime|full] [--json]
"""

import argparse
import json
import os
import re
import stat
import sys
from pathlib import Path

WHAT_DEFAULT = Path(__file__).resolve().parents[3]

# Scopes. "runtime" is the Rule-10-auditable core: everything that compiles
# into the shipped PRX plus the layout tables that ship beside it. "full"
# adds the development tree (diagnostics/tools classification).
RUNTIME_DIRS = [
    "outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src",
    "outputs/2026-10-08-l26-runtime/typescan-r4/candidate/include",
    "outputs/2026-10-08-l26-runtime/typescan-r4/candidate/generated_include",
    # generated profile headers are injected as include dirs by
    # build_variants.py (declared_include_directories) -- they compile in:
    "outputs/2026-10-08-l26-runtime/profile/units",
    # shipped per-version layout tables (engine-level; no game facts):
    "outputs/2026-10-08-l25-prep/tables",
]

# File classes the audit refuses to treat as evidence (binary/console-harvest).
IGNORE_DIR_PARTS = {".git", "__pycache__", "node_modules", ".codex"}
# Full-scope pruning: vendored/research/build trees that can never ride a
# release (research spikes, SDK builds, toolchains, build copies). The
# runtime scope never enters them; the full scope skips them for speed and
# because their contents are third-party or machine-generated, not authored
# project sources.
FULL_SCOPE_PRUNE = {
    ".superpowers",                        # vendored deps + research spikes
    "outputs/ps5-mod-bridge/native-host/build",   # built SDK/toolchain trees
    "outputs/2026-10-08-l26-runtime/snapshots",   # build snapshots (copies)
    "outputs/harvest",
}
# Directory NAMES pruned at any depth in full scope (machine-generated copy
# trees; a name hit is always a build/copy tree in this worktree).
FULL_SCOPE_PRUNE_NAMES = {"build", "builds", "snapshots", "artifacts",
                          "vendor", "third_party", "deps"}
# Full-scope size cap: authored diagnostics are small; anything larger is
# console-harvest data / logs (IGNORE class anyway). Runtime scope keeps the
# 8 MiB cap for completeness on the small release tree.
MAX_TEXT_BYTES_FULL = 512 * 1024
# Harvest/log receipts: game names inside them came from console output, not
# authored sources. Classified IGNORE when full-scope walks them at all.
IGNORE_SUFFIXES = (".zip", ".tar", ".png", ".jpg", ".elf", ".self", ".bin",
                   ".o", ".a", ".dylib", ".so")

MAX_TEXT_BYTES = 8 * 1024 * 1024  # skip monster files; they are logs/data


def classify_path(rel: str) -> str:
    """Rule-10 class for a path inside the audited scope."""
    p = rel.lower()
    parts = Path(rel).parts
    if any(part in IGNORE_DIR_PARTS for part in parts):
        return "IGNORE"
    if p.endswith(IGNORE_SUFFIXES):
        return "IGNORE"
    if rel.startswith("outputs/2026-10-08-l26-runtime/profile/units"):
        return "PROFILE-TABLE"
    if "/harvest" in p or p.endswith(".log") or "klog" in p or "console-" in p \
            or "/logs/" in p or "/snapshots/" in p:
        return "IGNORE"          # console-harvested evidence, not authored
    if any(rel.startswith(d) for d in RUNTIME_DIRS[:3]):
        return "RUNTIME"
    if any(rel.startswith(d) for d in RUNTIME_DIRS[4:]):
        return "PROFILE-TABLE"
    # phasea-tools and every other authored file: diagnostics/tools. Tools may
    # name the CURRENT development game only (blind-test law).
    return "DIAGNOSTIC"


def iter_files(root: Path, scope: str, term_bytes=()):
    """Yield (relpath, text) for candidate files. Speed contract: a raw
    bytes prefilter (any term substring, ascii + utf-16le forms) skips the
    decode entirely for the overwhelming majority of files, so a full-scope
    walk over a large worktree stays minutes-fast without third-party
    tools. Authored sources are far below the size cap; the cap exists to
    keep console-harvest logs from dominating."""
    base_dirs = RUNTIME_DIRS if scope == "runtime" else [""]  # "" = whole tree
    size_cap = MAX_TEXT_BYTES if scope == "runtime" else MAX_TEXT_BYTES_FULL
    for base in base_dirs:
        d = root / base if base else root
        if not d.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(d):
            rel_dir = os.path.relpath(dirpath, root)
            pruned = []
            for x in sorted(dirnames):
                if x in IGNORE_DIR_PARTS:
                    continue
                if scope == "full":
                    child = os.path.join(rel_dir, x) if rel_dir != "." else x
                    if child in FULL_SCOPE_PRUNE or x in FULL_SCOPE_PRUNE_NAMES:
                        continue
                pruned.append(x)
            dirnames[:] = pruned
            for fn in sorted(filenames):
                full = Path(dirpath) / fn
                try:
                    st = full.stat()
                    # REGULAR FILES ONLY: clr-debug-pipe FIFOs exist under
                    # outputs/*/tmp/ and read_bytes() on a FIFO blocks
                    # forever (found live during the 2026-10-10 retry run).
                    if not stat.S_ISREG(st.st_mode):
                        continue
                    if st.st_size > size_cap:
                        continue
                    blob = full.read_bytes()
                except OSError:
                    continue
                if term_bytes:
                    low = blob.lower()   # ASCII-case fold; regex pass is the
                    # authority -- prefilter is advisory-only and sound for
                    # ASCII-range names (all title-ids are).
                    if not any(b in low for b in term_bytes):
                        continue
                # NUL byte in the first 4 KiB => treat as binary, skip
                if b"\x00" in blob[:4096]:
                    continue
                try:
                    text = blob.decode("utf-8")
                except UnicodeDecodeError:
                    try:
                        text = blob.decode("utf-16-le")
                    except UnicodeDecodeError:
                        continue
                yield str(full.relative_to(root)), text


def audit(root: Path, games, current_game, scope):
    game_res = {g: {"terms": re.split(r"\|", g), "hits": []} for g in games}
    compiled = []
    term_bytes = []
    for g, spec in game_res.items():
        for t in spec["terms"]:
            t = t.strip()
            if t:
                compiled.append((t, re.compile(re.escape(t), re.IGNORECASE),
                                 g))
                lo = t.lower()
                term_bytes.append(lo.encode("utf-16-le"))
                if lo.isascii():
                    term_bytes.append(lo.encode("ascii"))

    counts = {"RUNTIME": 0, "PROFILE-TABLE": 0, "DIAGNOSTIC": 0,
              "IGNORE": 0}
    scanned = 0
    for rel, text in iter_files(root, scope, term_bytes):
        scanned += 1
        cls = classify_path(rel)
        if cls == "IGNORE":
            continue
        lines = text.splitlines()
        for term, rx, gname in compiled:
            hits = [(i + 1, l.strip()[:200]) for i, l in enumerate(lines)
                    if rx.search(l)]
            if not hits:
                continue
            counts[cls] += len(hits)
            game_res[gname]["hits"].append({
                "term": term, "class": cls, "path": rel,
                "occurrences": len(hits),
                "lines": [{"n": n, "text": t} for n, t in hits[:12]],
            })

    # Verdict per the proposed Rule-10 boundary (FIRST-LAUNCH-AUTOMATION.md
    # section 4):
    #   RUNTIME / PROFILE-TABLE hits for ANY audited game => VIOLATION
    #     (hard gate: shipping code/tables carry zero game facts).
    #   DIAGNOSTIC hits for the CURRENT game              => allowed (info).
    #   DIAGNOSTIC hits for a NON-current (blind-test) game => REVIEW warning:
    #     legitimate only in session receipts / blind-test mission docs;
    #     NEVER acceptable in tools (names enter via CLI only), per-game
    #     symbol sidecars, or anything that could ride a release. The tool
    #     reports; the human/Claude gate decides each REVIEW item.
    violations, warnings = [], []
    current_terms = {x.strip().lower() for x in current_game.split("|")} \
        if current_game else set()
    for g, spec in game_res.items():
        for h in spec["hits"]:
            if h["class"] in ("RUNTIME", "PROFILE-TABLE"):
                violations.append((g, h))
            else:  # DIAGNOSTIC
                if h["term"].strip().lower() in current_terms:
                    continue                    # current game: allowed
                warnings.append((g, h))         # blind-test REVIEW item
    return {"games": game_res, "counts": counts, "files_scanned": scanned,
            "violations": violations, "warnings": warnings}


def main(argv=None):
    ap = argparse.ArgumentParser(
        prog="release_audit.py",
        description="Rule-10 no-game-names release gate (agent GI, phasea)")
    ap.add_argument("--game", action="append", required=True,
                    help="game spec 'Name1|Name2|TITLEID' (repeatable)")
    ap.add_argument("--current-game", default=None,
                    help="terms of the CURRENT development game (its "
                         "diagnostic mentions are allowed); default: every "
                         "--game's diagnostic mentions become REVIEW items")
    ap.add_argument("--root", default=str(WHAT_DEFAULT),
                    help="worktree root to audit")
    ap.add_argument("--scope", choices=["runtime", "full"], default="runtime",
                    help="runtime = PRX-compiled sources + profile units + "
                         "shipped tables; full = whole worktree (adds "
                         "diagnostics classification)")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)

    root = Path(args.root).resolve()
    if not root.is_dir():
        print("root not found: %s" % root, file=sys.stderr)
        return 3

    res = audit(root, args.game, args.current_game, args.scope)

    if args.json:
        out = {
            "tool": "release_audit.py 1.0 (agent GI, 2026-10-08-l25-prep)",
            "root": str(root), "scope": args.scope,
            "files_scanned": res["files_scanned"],
            "class_counts": res["counts"],
            "games": {
                g: {
                    "terms": s["terms"],
                    "total_occurrences": sum(h["occurrences"]
                                             for h in s["hits"]),
                    "by_class": {
                        c: sum(h["occurrences"] for h in s["hits"]
                               if h["class"] == c)
                        for c in ("RUNTIME", "PROFILE-TABLE", "DIAGNOSTIC")},
                    "files": s["hits"],
                } for g, s in res["games"].items()},
            "violations": [
                {"game": g, "term": h["term"], "class": h["class"],
                 "path": h["path"], "occurrences": h["occurrences"],
                 "sample": h["lines"][:3]}
                for g, h in res["violations"]],
            "warnings": [
                {"game": g, "term": h["term"], "class": h["class"],
                 "path": h["path"], "occurrences": h["occurrences"]}
                for g, h in res["warnings"]],
        }
        print(json.dumps(out, indent=1))
    else:
        print("release_audit.py 1.0 (agent GI)  root=%s  scope=%s  "
              "files_scanned=%d" % (root, args.scope, res["files_scanned"]))
        print("class counts: " + ", ".join("%s=%d" % kv
                                           for kv in res["counts"].items()))
        for g, spec in res["games"].items():
            print("\n== game %r ==" % g)
            if not spec["hits"]:
                print("  CLEAN: zero occurrences in audited scope")
                continue
            for h in spec["hits"]:
                print("  [%s] %-70s %3dx  term=%r"
                      % (h["class"], h["path"][:70], h["occurrences"],
                         h["term"]))
                for ln in h["lines"][:4]:
                    print("        L%-6d %s" % (ln["n"], ln["text"][:120]))
        print("\nviolations=%d  review-warnings=%d"
              % (len(res["violations"]), len(res["warnings"])))
    return 2 if res["violations"] else (1 if res["warnings"] else 0)


if __name__ == "__main__":
    sys.exit(main())
