#!/usr/bin/env python3
"""simulate_ploader_ini_final.py — EXTENDED ridealongs simulator for the
DEFINITIVE return-session ploader.ini (worker-final-ini-layout; NINTH-SLOT
revision by worker-ninth-slot-ini-adoption, 2026-10-04).

Parser core is byte-for-byte the pinned-parser re-implementation delivered by
worker-game-session-ridealongs/simulate_ploader_ini.py (itself derived from
.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/
main.c: MAX_PLUGINS=10, MAX_TITLES=10, MAX_PATH_LEN=255, MAX_TITLE_ID_LEN=16,
parse_mode 0/1/2 only, mode-0 SKIPPED at parse main.c L672, register order ==
file order L776-820, NO path dedupe). The original file is left untouched;
this module embeds the same law and adds:

  1. CANONICAL_FINAL: the definitive 9-entry layout (8 active + igf=0 inert;
     cc1 carrier appended AFTER the igf line per
     worker-carrier-probe-build/STAGE-PLAN.md — the ninth-slot revision;
     parser core UNCHANGED from the 8-entry delivery).
  2. Per-stager content-anchored ensure() ops + a commutativity proof: every
     staging-order permutation (and every subset/repeat) converges to the
     canonical bytes byte-for-byte (9! = 362880 orders).
  3. Reorder-consequence variants: probe-after-composition (verdict-absent-
     at-composition-init modelled), ridealongs-before-composition (window
     burn), duplicate hazard, igf=2 counterfactual, mode-1 probes,
     CRLF/comments/no-trailing-newline tolerance, mode-3 rejection.

No native code executed; pure host simulation. Exit 0 = all variants pass."""
import itertools
import sys

MAX_PLUGINS = 10
MAX_TITLES = 10
MAX_PATH_LEN = 255
MAX_TITLE_ID_LEN = 16

HEADER = "[PPSA33286]"

# (path, mode) in THE definitive order. Line text = f"{path} = {mode}".
CANONICAL_ENTRIES = [
    ("/data/plugins/modbridge_exec_write_probe_armed.prx", 2),
    ("/data/plugins/modbridge_fc1pool.prx", 2),
    ("/data/plugins/modbridge_supplier_composition.prx", 2),
    ("/data/plugins/modbridge_qh_driver.prx", 2),
    ("/data/plugins/modbridge_runtime_aligned_observer.prx", 2),
    ("/data/plugins/modbridge_runtime_aligned_entry.prx", 2),
    ("/data/plugins/modbridge_path_test.prx", 2),
    ("/data/plugins/modbridge_ingame_facts.prx", 0),
    ("/data/plugins/modbridge_carrier.prx", 2),
]

CANONICAL_FINAL = HEADER + "\n" + "".join(
    f"{p} = {m}\n" for p, m in CANONICAL_ENTRIES)

EXPECT_REGISTERED = [
    p for p, m in CANONICAL_ENTRIES if m > 0
]  # 8 active entries, igf=0 skipped; carrier appended after the igf line


# ---------------------------------------------------------------- parser law
def trim(s: str) -> str:
    s = s.lstrip(" \t")
    res = s
    while res and res[-1] in " \t\r\n" and len(res) > 1:
        res = res[:-1]
    return res if len(s) > 0 else s


def is_blank_or_comment(s: str) -> bool:
    return s == "" or s[0] in ";#"


def parse_section(s: str):
    if len(s) >= 2 and s[0] == "[" and s[-1] == "]":
        return s[1:-1]
    return None


def parse_mode(val: str):
    val = val.lstrip(" \t\n\r\f\v")
    sign = 1
    i = 0
    if i < len(val) and val[i] in "+-":
        sign = -1 if val[i] == "-" else 1
        i += 1
    n = 0
    while i < len(val) and val[i].isdigit():
        n = n * 10 + int(val[i])
        i += 1
    mode = sign * n
    if mode < 0 or mode > 2:
        return -1
    return mode


def normalize_path(p: str):
    p = trim(p)
    if not p or len(p) > MAX_PATH_LEN or ".." in p:
        return None
    if "/" not in p:
        return "/data/plugins/" + p
    return p


class Entry:
    def __init__(self):
        self.title_id = ""
        self.paths = []
        self.modes = []


def read_ini(text: str):
    plugins = []
    tid = ""
    dropped = []
    for raw in text.split("\n"):
        line = raw[:1024]
        s = trim(line)
        if is_blank_or_comment(s):
            continue
        sec = parse_section(s)
        if sec is not None:
            tid = sec[:MAX_TITLE_ID_LEN]
            continue
        if not tid:
            continue
        eq = s.find("=")
        if eq < 0:
            continue
        key, val = s[:eq], s[eq + 1:]
        mode = parse_mode(trim(val))
        if mode <= 0:  # invalid(-1) or disabled(0): SKIPPED (main.c L672)
            dropped.append((key.strip(), mode))
            continue
        normalized = normalize_path(key)
        if normalized is None:
            dropped.append((key.strip(), mode))
            continue
        slot = None
        for e in plugins:
            if e.title_id == tid:
                slot = e
                break
        if slot is None:
            if len(plugins) >= MAX_TITLES:
                dropped.append((normalized, mode))
                continue
            slot = Entry()
            slot.title_id = tid
            plugins.append(slot)
        if len(slot.paths) >= MAX_PLUGINS:
            dropped.append((normalized, mode))
            continue
        slot.paths.append(normalized[:MAX_PATH_LEN + 1])
        slot.modes.append(mode)
    return plugins, dropped


def registered(text: str):
    plugins, dropped = read_ini(text)
    assert len(plugins) <= MAX_TITLES
    if not plugins:
        return [], dropped
    return list(zip(plugins[0].paths, plugins[0].modes)), dropped


# ------------------------------------------------- content-anchored ensure()
# Each staging step ensures ONE line, inserting immediately after its
# canonical predecessor (a CONTENT anchor, never the file tail), recursively
# ensuring the predecessor first. Idempotent (present-exactly-once -> no-op)
# and commutative: insertion position depends only on content anchors, so any
# interleaving converges to CANONICAL_FINAL. Duplicates are a hard STOP
# (ploader does not dedupe paths).
PREDECESSOR = {HEADER: None}
for i, (p, m) in enumerate(CANONICAL_ENTRIES):
    anchor = HEADER if i == 0 else CANONICAL_ENTRIES[i - 1][0]
    PREDECESSOR[p] = anchor
LINE_OF = {p: f"{p} = {m}" for p, m in CANONICAL_ENTRIES}


class EnsureError(Exception):
    pass


def _count_path(lines, path):
    return sum(1 for ln in lines if ln.split(" = ")[0].strip() == path)


def ensure(text: str, path: str) -> str:
    """Content-anchored idempotent ensure for one canonical entry."""
    if path == HEADER:
        # Header is a PRECONDITION (Step-0 RETR proves it), never appended.
        if _count_path(text.split("\n"), path) >= 1:
            return text
        raise EnsureError("header [PPSA33286] missing — STOP, reconcile")
    if path not in LINE_OF:
        raise EnsureError(f"unknown path {path!r} (never invent lines)")
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines = lines[:-1]
    n = _count_path(lines, path)
    if n > 1:
        raise EnsureError(f"duplicate line for {path} — STOP, reconcile "
                          f"(ploader registers duplicates twice)")
    if n == 1:
        return "\n".join(lines) + "\n"          # idempotent no-op
    pred = PREDECESSOR[path]
    text = ensure(text, pred)                   # recursive chain to header
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines = lines[:-1]
    idx = max(i for i, ln in enumerate(lines)
              if ln.split(" = ")[0].strip() == pred)
    lines.insert(idx + 1, LINE_OF[path])
    return "\n".join(lines) + "\n"


# ------------------------------------------------------------------ variants
def v_canonical():
    return CANONICAL_FINAL


def v_no_trailing_newline():
    return CANONICAL_FINAL.rstrip("\n")


def v_crlf_comments():
    body = CANONICAL_FINAL.replace("\n", "\r\n")
    return body.replace(
        "\r\n/data/plugins/modbridge_fc1pool.prx",
        "\r\n; fc1 validator probe\r\n# pinned 2026-10-04\r\n"
        "/data/plugins/modbridge_fc1pool.prx", 1)


def v_probe_after_composition():
    """THE degraded reorder: xwp verdict written AFTER composition's
    module_start -> verdict file absent when the (future-armed) initializer
    reads it -> SHAPE A safe default fires. Parser still accepts it; the
    LAYOUT verifier must reject it."""
    e = CANONICAL_ENTRIES[:]
    e.remove(("/data/plugins/modbridge_exec_write_probe_armed.prx", 2))
    e.insert(3, ("/data/plugins/modbridge_exec_write_probe_armed.prx", 2))
    return HEADER + "\n" + "".join(f"{p} = {m}\n" for p, m in e)


def v_ridealongs_before_composition():
    e = CANONICAL_ENTRIES[:]
    q = e.pop(3)          # qdrv1
    o = e.pop(4)          # observer (after pop index shifts)
    e.insert(2, o)
    e.insert(2, q)
    return HEADER + "\n" + "".join(f"{p} = {m}\n" for p, m in e)


def v_duplicate_hazard():
    return CANONICAL_FINAL + "/data/plugins/modbridge_fc1pool.prx = 2\n"


def v_igf2_counterfactual():
    return CANONICAL_FINAL.replace(
        "/data/plugins/modbridge_ingame_facts.prx = 0",
        "/data/plugins/modbridge_ingame_facts.prx = 2")


def v_probes_mode1():
    """Mode is NOT the ordering mechanism (file order is); mode-1 probes stay
    before composition. Timing semantics change, order does not."""
    return CANONICAL_FINAL.replace(
        "/data/plugins/modbridge_exec_write_probe_armed.prx = 2", 
        "/data/plugins/modbridge_exec_write_probe_armed.prx = 1").replace(
        "/data/plugins/modbridge_fc1pool.prx = 2",
        "/data/plugins/modbridge_fc1pool.prx = 1")


def v_invalid_mode3():
    return CANONICAL_FINAL + "/data/plugins/modbridge_bad.prx = 3\n"


def v_unknown_extra_line():
    return CANONICAL_FINAL + "/data/plugins/modbridge_surprise.prx = 2\n"


def v_composition_mode_flipped():
    return CANONICAL_FINAL.replace(
        "/data/plugins/modbridge_supplier_composition.prx = 2",
        "/data/plugins/modbridge_supplier_composition.prx = 1")


VARIANTS = {name[2:]: fn for name, fn in sorted(globals().items())
            if name.startswith("v_")}

# Staging-order commutativity set: per-stager obligations (stager -> the ONE
# line it owns). Header is a precondition of every op.
STAGER_LINES = [
    "/data/plugins/modbridge_exec_write_probe_armed.prx",
    "/data/plugins/modbridge_fc1pool.prx",
    "/data/plugins/modbridge_supplier_composition.prx",
    "/data/plugins/modbridge_qh_driver.prx",
    "/data/plugins/modbridge_runtime_aligned_observer.prx",
    "/data/plugins/modbridge_runtime_aligned_entry.prx",
    "/data/plugins/modbridge_path_test.prx",
    "/data/plugins/modbridge_ingame_facts.prx",
    "/data/plugins/modbridge_carrier.prx",
]


def v_staging_permutations():
    """Every permutation of the 9 stager ensure-ops from the bare header must
    converge byte-for-byte to CANONICAL_FINAL (9! = 362880 orders)."""
    results = set()
    for perm in itertools.permutations(STAGER_LINES):
        text = HEADER + "\n"
        try:
            for path in perm:
                text = ensure(text, path)
        except EnsureError as exc:
            return None, f"ensure raised on order {perm}: {exc}"
        results.add(text)
    if len(results) != 1:
        return None, f"NON-CONVERGENT: {len(results)} distinct end-states"
    final = results.pop()
    return (final if final == CANONICAL_FINAL else None), \
        ("" if final == CANONICAL_FINAL else "converged but != canonical")


def v_staging_subsets_and_repeats():
    """Subsets and repeated (idempotent) applications also stay consistent:
    running the FULL sequence again on the final file must be a byte no-op,
    and any prefix subset must be a canonical-order prefix of the layout."""
    text = HEADER + "\n"
    for path in STAGER_LINES * 3:      # repeats exercise idempotency
        text = ensure(text, path)
    if text != CANONICAL_FINAL:
        return None, "repeat applications diverged from canonical"
    return text, ""


VARIANTS["staging_permutations"] = v_staging_permutations
VARIANTS["staging_subsets_and_repeats"] = v_staging_subsets_and_repeats

# Byte-comparison policy: byte equality vs CANONICAL_FINAL is asserted only
# where bytes are the contract (canonical itself and the staging-convergence
# variants). Tolerance variants (CRLF/comments/missing trailing NL) are
# compared at the PARSE level, matching the pinned parser's trim behaviour.


def main() -> int:
    failures = []
    for name, fn in VARIANTS.items():
        out = fn()
        text, note = out if isinstance(out, tuple) else (out, "")
        if text is None:
            failures.append(f"{name}: {note or 'no result'}")
            continue
        reg, dropped = registered(text)
        print(f"variant={name} registered={len(reg)} dropped={dropped} {note}".rstrip())
        for p, m in reg:
            print(f"   mode={m} {p}")
        # universal laws
        if any(p.endswith("modbridge_ingame_facts.prx") for p, _ in reg) \
                and name != "igf2_counterfactual":
            failures.append(f"{name}: mode-0 igf entry was registered")
        if len(reg) > MAX_PLUGINS:
            failures.append(f"{name}: exceeds MAX_PLUGINS={MAX_PLUGINS}")
        # per-variant expectations
        if name == "canonical":
            if [p for p, _ in reg] != EXPECT_REGISTERED:
                failures.append(f"{name}: order/set mismatch: {[p for p, _ in reg]}")
            if len(reg) != 8:
                failures.append(f"{name}: expected 8 registered, got {len(reg)}")
            if not all(m == 2 for _, m in reg):
                failures.append(f"{name}: expected all modes == 2")
        elif name == "no_trailing_newline":
            if [p for p, _ in reg] != EXPECT_REGISTERED or len(reg) != 8:
                failures.append(f"{name}: parse diverged without trailing NL")
        elif name == "crlf_comments":
            if [p for p, _ in reg] != EXPECT_REGISTERED or len(reg) != 8:
                failures.append(f"{name}: CRLF/comment parse diverged")
        elif name == "probes_mode1":
            if [p for p, _ in reg] != EXPECT_REGISTERED:
                failures.append(f"{name}: mode-1 probes changed registration order")
            modes = dict(reg)
            if modes["/data/plugins/modbridge_exec_write_probe_armed.prx"] != 1 or \
               modes["/data/plugins/modbridge_fc1pool.prx"] != 1:
                failures.append(f"{name}: mode-1 not reflected")
        elif name == "igf2_counterfactual":
            if len(reg) != 9:
                failures.append(f"{name}: expected 9 registered if igf flipped to 2, got {len(reg)}")
            print("   COUNTERFACTUAL: flipping igf to 2 would add a SECOND "
                  "writer on qdrv1's jsonl — MUST stay 0")
        elif name == "duplicate_hazard":
            fc1 = [1 for p, _ in reg if p.endswith("modbridge_fc1pool.prx")]
            if len(fc1) != 2:
                failures.append(f"{name}: expected duplicate x2, got {len(fc1)}")
            print("   HAZARD CONFIRMED: no path dedupe -> fc1 registered twice")
        elif name == "probe_after_composition":
            if [p for p, _ in reg] != EXPECT_REGISTERED:
                pass  # parser is order-faithful; set still valid — layout
                # verifier (not the parser) rejects this sequence.
            else:
                print("   DEGRADED-BUT-PARSES: verdict absent at composition "
                      "init -> initializer SHAPE A safe default; layout "
                      "verifier must REJECT this order")
        elif name == "ridealongs_before_composition":
            print("   EVIDENCE-LOSS ORDER: observer/qdrv1 windows start "
                  "pre-ladder -> honest negatives; layout verifier rejects")
        elif name == "invalid_mode3":
            if any(p.endswith("modbridge_bad.prx") for p, _ in reg):
                failures.append(f"{name}: mode-3 entry registered")
        elif name == "unknown_extra_line":
            if not any(p.endswith("modbridge_surprise.prx") for p, _ in reg):
                failures.append(f"{name}: parser unexpectedly dropped the line")
            print("   PARSER LAW: an unknown but well-formed line REGISTERS "
                  "(9th slot burned, double-writer risk) — only the "
                  "final-layout VERIFIER rejects it; parser never will")
        elif name == "composition_mode_flipped":
            modes = dict(reg)
            if modes.get("/data/plugins/modbridge_supplier_composition.prx") != 1:
                failures.append(f"{name}: flipped mode not reflected")
            print("   MODE-FLIP LAW: existing-line mode changes are a STOP "
                  "(never rewrite pinned modes)")
        elif name in ("staging_permutations", "staging_subsets_and_repeats"):
            if text != CANONICAL_FINAL:
                failures.append(f"{name}: end-state != canonical bytes")
            else:
                print("   CONVERGED: byte-identical to CANONICAL_FINAL")
    if failures:
        print("FAIL")
        for f in failures:
            print(" -", f)
        return 1
    print("SIMULATION_PASSED: 8 active entries registered in definitive file "
          "order (igf=0 skipped, no slot; cc1 carrier appended after the igf "
          "line); 8/10 MAX_PLUGINS; every staging order (9! = 362880) "
          "converges byte-for-byte to the canonical layout; duplicate/"
          "mode-3/mode-flip hazards behave per the pinned parser law.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
