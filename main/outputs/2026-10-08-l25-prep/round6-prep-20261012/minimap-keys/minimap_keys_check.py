#!/usr/bin/env python3
"""minimap-keys track checker (Mac-only, read-only; stdlib only).

Run:  python3 -I minimap_keys_check.py [--root WORKTREE] [--result FILE]
                                       [--write-candidate]

Default mode only READS files and writes check-result.json next to this
script (--result picks another file name; it must stay inside this script's
folder). --write-candidate additionally (re)writes the future-only candidate
PS5Pad.FUTURE-minimap-DO-NOT-UPLOAD.ini next to this script. Nothing else is
ever written. No network, no console contact, no console logs.

Every check prints how many items it checked; a check that checks zero items
FAILS, and any failed check makes the exit status non-zero. Items that are
generated in memory (button combinations) are counted separately from items
read from files, and a check whose file inputs are missing checks zero items.

Checks:
  1 pins        minimap main.lua + PS5Pad.ini + loadlist match rowcheck-pins.json
  2 minimap     RegisterKeyBind-family calls (expect 0), engine key polls, key
                tables, settings.txt keys, show-path gates (file:line)
  3 padini      PS5Pad.ini parsed with a mirror of PS5PadConfig.cpp rules;
                launch-card rows present
  4 freechords  legal new chords under the containment law (all 16 buttons)
                and under the MAPPING.md policy (TOUCH_PAD L1 R1 L2 R2 only);
                needs the pinned rows from check 3, else zero items
  5 gates       provenance chain of the B4 PRX (d044cf0b): frozen receipt ->
                l26-package-A BUILD-INPUTS -> l26-A pass1/pass2 composition
                receipts; pad/input unit sources match those pins; the exact
                delivery-gate lines say what the report cites; PRX symbols
                contain the pad parser and poll
  6 inject      engine-input injection call scan over every B4-composed source
                + the l26 candidate tree (expect 0 hits)
  7 subscribed  UE4SS key subscriptions of every pinned Lua mod; the
                minimap's keys must be absent
  8 candidate   future-only candidate: deterministic bytes, mirror-parse,
                containment, size/sha256, implied loadlist row change
  9 shimlog     what a "[SHIM] io-open resolved=...settings.txt->..." marker
                can and cannot prove about the minimap showing (main.lua write
                paths + LuaPathShim.cpp marker lines, B4-pinned source)
"""
import argparse
import hashlib
import itertools
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CANDIDATE_NAME = "PS5Pad.FUTURE-minimap-DO-NOT-UPLOAD.ini"

REL = {
    "pins": "outputs/2026-10-08-l25-prep/morning-launch-20261012/rowcheck-pins.json",
    "loadlist": "outputs/2026-10-08-l25-prep/morning-launch-20261012/loadlist.txt",
    "minimap": ".superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1/"
               "minimap-v3.1/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua",
    "pad": "outputs/2026-10-08-l25-prep/ueselftest/PS5Pad-round4-proposed.ini",
    "pad_alt": "outputs/2026-10-08-l25-prep/launch-manifests/launch1-PS5Pad.ini",
    # B4 (d044cf0b) provenance chain. The frozen folder is read-only on disk.
    "b4_receipt": "outputs/2026-10-08-l26-frozen/B4-shim-final-stack-d044cf0b/FINAL-RECEIPT-VARIANT-A.json",
    "b4_prx": "outputs/2026-10-08-l26-frozen/B4-shim-final-stack-d044cf0b/modbridge_supplier_composition.prx",
    "b4_prx_sha": "outputs/2026-10-08-l26-frozen/B4-shim-final-stack-d044cf0b/modbridge_supplier_composition.prx.sha256",
    "b4_symbols": "outputs/2026-10-08-l26-frozen/B4-shim-final-stack-d044cf0b/modbridge_supplier_composition.prx.symbols.json",
    "ledger": "outputs/2026-10-08-l25-prep/BUILD-LEDGER-20261012.md",
    # BUILD-INPUTS moves to archive-r6 when the next build starts (ledger line 9).
    "build_inputs": ["outputs/2026-10-08-l26-package-A/BUILD-INPUTS.json",
                     "outputs/2026-10-08-l26-build-variants/archive-r6/B4-shim-final-stack/"
                     "2026-10-08-l26-package-A/BUILD-INPUTS.json"],
    "comp_archive": "outputs/2026-10-08-l26-build-variants/archive-r6/B4-shim-final-stack/l26-A",
    # Older l25-A receipts: cross-check only (not the launched build's provenance).
    "l25_comp1": "outputs/2026-10-08-l25-build-variants/l25-A/pass1/COMPOSITION-RECEIPT.json",
    "l25_comp2": "outputs/2026-10-08-l25-build-variants/l25-A/pass2/COMPOSITION-RECEIPT.json",
    "l26_candidate": "outputs/2026-10-08-l26-runtime/typescan-r4/candidate",
    "l26_changes": "outputs/2026-10-08-l26-runtime/typescan-r4/CHANGES.json",
    "upload": "outputs/2026-10-08-l25-prep/morning-launch-20261012/upload",
    "spool": "outputs/2026-10-08-l25-prep/evidence/morning-readonly-20261012T114008Z/spool/rows",
}

# PS5PadConfig.cpp:41-46 button names; KeyDef.cpp (input-keydef unit) key names.
BUTTONS = ["L3", "R3", "OPTIONS", "UP", "RIGHT", "DOWN", "LEFT", "L2", "R2",
           "L1", "R1", "TRIANGLE", "CIRCLE", "CROSS", "SQUARE", "TOUCH_PAD"]
POLICY_FREE = ["TOUCH_PAD", "L1", "R1", "L2", "R2"]      # MAPPING.md section 3
PAIRS10 = ["TOUCH_PAD+L1", "TOUCH_PAD+R1", "TOUCH_PAD+L2", "TOUCH_PAD+R2",
           "L1+R1", "L1+R2", "L1+L2", "R1+L2", "R1+R2", "L2+R2"]  # keymap-gen.py LADDERS
MODIFIERS = ["SHIFT", "CONTROL", "ALT"]

# Minimap engine key name -> UE4SS KeyDef name (KeyDef.cpp line in the comment).
MINIMAP_TO_UE4SS = [
    ("menu", "F5", "F5"),             # KeyDef.cpp:115
    ("fullmap", "F6", "F6"),          # KeyDef.cpp:116
    ("toggle", "Insert", "INS"),      # KeyDef.cpp:52
    ("side", "Delete", "DEL"),        # KeyDef.cpp:53
    ("zoomOut", "PageDown", "PAGE_DOWN"),  # KeyDef.cpp:41
    ("zoomIn", "PageUp", "PAGE_UP"),  # KeyDef.cpp:40
    ("bigger", "Home", "HOME"),       # KeyDef.cpp:43
    ("smaller", "End", "END"),        # KeyDef.cpp:42
]
WHAT = {
    "menu": "open/close the minimap settings menu (pauses the game)",
    "fullmap": "open/close the Expanded Dual map",
    "toggle": "show/hide the minimap (saved to settings.txt)",
    "side": "cycle map side front/back/split/auto",
    "zoomOut": "zoom out", "zoomIn": "zoom in",
    "bigger": "map size one step bigger", "smaller": "map size one step smaller",
}

# B4-shim-final-stack PRX. Two independent sources, both re-read by check 5:
# BUILD-LEDGER-20261012.md line 9 and the frozen .prx.sha256 file; check 5 also
# hashes the frozen PRX itself and reads the frozen FINAL-RECEIPT.
B4_PRX_SHA = "d044cf0bf736b5f4399081eea764f59673d1d798a530572ab6897f8879c37599"
PAD_UNITS = ["native-pad-PS5PadSource", "pad-config", "native-pad-input", "input-handler",
             "input-keydef", "native-pad-PS5PadNative"]

RESULT = {"checks": {}, "ok": True}


def sha(b):
    return hashlib.sha256(b).hexdigest()


def record(name, checked, failures, data=None, synthetic=0):
    """checked = all items; synthetic = how many of them were generated in
    memory (not read from a file). A check with zero items read from files
    is a failed check even if it generated items."""
    ok = checked > 0 and (checked - synthetic) > 0 and not failures
    RESULT["checks"][name] = {"checked": checked, "synthetic": synthetic, "failures": failures,
                              "pass": ok, "data": data or {}}
    if not ok:
        RESULT["ok"] = False
    status = "PASS" if ok else "FAIL"
    extra = f" ({synthetic} generated in memory, {checked - synthetic} from files)" if synthetic else ""
    print(f"[{status}] {name}: checked {checked} item(s){extra}, {len(failures)} failure(s)")
    for f in failures:
        print(f"        - {f}")
    if checked - synthetic <= 0:
        print("        - zero items checked from files: a check that scans nothing is a failed check")
    return ok


def line_of(text, n):
    lines = text.splitlines()
    return lines[n - 1] if 0 < n <= len(lines) else ""


# ---------------------------------------------------------------- 1 pins
def check_pins(root):
    pins = json.loads((root / REL["pins"]).read_text())
    want = [("ue4ss/Mods/TownfallMiniMap/Scripts/main.lua", REL["minimap"]),
            ("ue4ss/PS5Pad.ini", REL["pad"]),
            ("ue4ss/PS5Pad.ini", REL["pad_alt"]),
            ("loadlist.txt", REL["loadlist"])]
    failures, data, n = [], {}, 0
    for pin_key, rel in want:
        p = pins.get(pin_key)
        b = (root / rel).read_bytes()
        n += 1
        got = {"size": len(b), "sha256": sha(b)}
        data[rel] = {"pin": pin_key, **got}
        if not p or p["size"] != got["size"] or p["sha256"] != got["sha256"]:
            failures.append(f"{rel} does not match pin {pin_key}: {got} vs {p}")
    return record("1 pins", n, failures, data)


# ---------------------------------------------------------------- 2 minimap
def check_minimap(root):
    text = (root / REL["minimap"]).read_bytes().decode("latin1")
    failures, data = [], {}
    n = 0
    # UE4SS key-bind API usage (expect none).
    api = re.compile(r"RegisterKeyBind(?:Async)?\s*\(|IsKeyBindRegistered\s*\(|\bKey\.[A-Z_0-9]+|\bModifierKey\.")
    api_hits = [(i, l.strip()) for i, l in enumerate(text.splitlines(), 1) if api.search(l)]
    n += 1
    data["ue4ss_keybind_api_hits"] = api_hits
    if api_hits:
        failures.append(f"expected 0 UE4SS key-bind API uses, found {len(api_hits)}")
    # Engine key polling calls.
    poll = re.compile(r"(IsInputKeyDown|WasInputKeyJustPressed|GetInputAnalogKeyState)\s*\(")
    polls = [(i, m.group(1)) for i, l in enumerate(text.splitlines(), 1) for m in poll.finditer(l)]
    n += len(polls)
    data["engine_key_polls"] = polls
    if not polls:
        failures.append("expected engine key polls (IsInputKeyDown...), found none")
    # Key tables: KEYS (main), M.KEYS (settings menu), W.KEYS (expanded map).
    tables = {}
    for label, pat in (("KEYS", r"^local KEYS = (\{.*?\})\s*$"),
                       ("M.KEYS", r"^M\.KEYS = (\{.*?\})\s*$"),
                       ("W.KEYS", r"^W\.KEYS = (\{.*?\})\s*$")):
        m = re.search(pat, text, re.M | re.S)
        if not m:
            failures.append(f"key table {label} not found")
            continue
        pairs = re.findall(r'\{\s*"([A-Za-z0-9]+)"\s*,\s*"([A-Za-z0-9]+)"\s*\}', m.group(1))
        start = text[:m.start()].count("\n") + 1
        tables[label] = {"line": start, "pairs": pairs}
        n += len(pairs)
    data["key_tables"] = tables
    expected_main = [(a, e) for a, e, _ in MINIMAP_TO_UE4SS]
    if tables.get("KEYS", {}).get("pairs") != expected_main:
        failures.append(f"KEYS table differs from the expected 8 actions: {tables.get('KEYS')}")
    # settings.txt keys read by loadSettings.
    ls = re.search(r"local function loadSettings\(\)(.*?)\nend\n", text, re.S)
    keys = sorted(set(re.findall(r'k == "([A-Za-z_]+)"', ls.group(1)))) if ls else []
    n += len(keys)
    data["settings_txt_keys"] = keys + ["show.<row id>"]
    if not keys:
        failures.append("loadSettings keys not found")
    # Show-by-default facts and every gate on the show path, with exact lines.
    # Order = the order the 50 ms tick meets them (tick() starts at 4062).
    facts = [
        (9, "local TICK_MS, COPY_EVERY = 50, 5", "tick period 50 ms"),
        (14, "HINT_TICKS = 50, 140", "hint lasts 140 ticks (about 7 s)"),
        (78, "enabled = true", "S.enabled defaults to true"),
        (149, 'id = "blankMap"', "blankMap row present"),
        (149, 'def = "on"', "blankMap default on"),
        (245, '"\\\\settings.txt"', "settings path ends in \\settings.txt"),
        (1996, "trusted = true", "default layout is trusted"),
        (4268, "LoopInGameThreadWithDelay(TICK_MS, safeTick)", "50 ms tick loop"),
        # gate 1: a local PlayerController; FindAllOf whenever S.pc is not valid
        (305, "if valid(S.pc) then return S.pc end", "gate 1: cached controller reused while valid"),
        (307, 'FindAllOf("PlayerController")', "gate 1: full PlayerController search when S.pc is nil/invalid"),
        (308, "pc:IsLocalController()", "gate 1: must be the local controller"),
        (4064, "local pc = playerController()", "gate 1 runs every tick"),
        (4068, "setShown(false); return", "gate 1 fail hides the map"),
        # gate 2: all 8 engine key polls succeed
        (4043, "pc:IsInputKeyDown", "gate 2: main hotkeys polled from engine key state"),
        (4045, "S.pc = nil", "gate 2: a failing IsInputKeyDown drops the controller (next tick searches again)"),
        (4084, "if not pollKeys(pc) then setShown(false); return end", "gate 2 fail hides the map"),
        # gate 3: the pawn has a valid MapRenderComponent
        (313, "local pawn = pc.Pawn", "gate 3: needs a pawn"),
        (315, "pawn.MapRenderComponent", "gate 3: needs pawn.MapRenderComponent"),
        (4090, "if S.noPawnTicks % 20 == 0 then S.pc = nil end", "gate 3 fail: controller search again about once a second"),
        (4091, "setShown(false)", "gate 3 fail hides the map"),
        # gate 4: the widget is built and in the viewport
        (4116, "if not buildWidget(pc) then return end", "gate 4: widget rebuild after an error"),
        (4119, "if not buildWidget(pc) then return end", "gate 4: widget (re)build when missing / not in viewport"),
        (1118, 'cls("/Script/Townfall.MapUserWidget")', "gate 4: buildWidget needs the game's MapUserWidget class"),
        # gate 5: unprotected calls; an error aborts the tick
        (4136, "local open = comp:IsMapOpen()", "gate 5: unprotected call (an error aborts the tick)"),
        (4140, "local gs, hs = gameState(pc), hudState(pc)", "gate 5: game/HUD state read"),
        (4242, "if pcall(tick) then return end", "gate 5: a tick error is caught here"),
        (4245, "S.needsRebuild = true", "gate 5: after a tick error the next tick destroys and rebuilds the widget"),
        # gate 6: HUD state and the show formula
        (72, "PLAY_STATES = { [4] = true, [7] = true, [11] = true, [21] = true }", "gate 6: play states"),
        (73, "MAP_STATES = { [9] = true, [32] = true }", "gate 6: map states"),
        (74, "NO_HUD = 3", "gate 6: HUD-hidden state"),
        (4173, "local hudOk = (gs == nil or PLAY_STATES[gs] or MAP_STATES[gs]) and hs ~= NO_HUD", "gate 6: hudOk"),
        (4181, "M.defaultLayout()", "default layout used before any map snapshot"),
        (4182, "S.blank = not covered", "blank paper map when not over a snapshot"),
        (4183, "S.cal.trusted and S.frame ~= nil", "blank map needs a trusted layout and a frame"),
        (4188, "local show = S.enabled and (covered and S.hasSnapshot or S.blank or W.active) and hudOk", "show formula"),
        (4211, "if not S.hintShown then", "first show only (hintShown persisted in settings.txt)"),
        (4213, "toast(HINT_TEXT, HINT_TICKS, true)", "keyboard hint toast on first show"),
        (4214, "saveSettings()", "first show writes settings.txt"),
        (4238, "setShown(show or toastOnly or M.open)", "visibility applied"),
    ]
    for ln, needle, why in facts:
        n += 1
        if needle not in line_of(text, ln):
            failures.append(f"main.lua:{ln} expected {needle!r} ({why}); got {line_of(text, ln).strip()!r}")
    data["facts_checked"] = [f"main.lua:{ln} {why}" for ln, _, why in facts]
    return record("2 minimap", n, failures, data)


# ---------------------------------------------------------------- 3 padini (mirror parser)
class IniError(Exception):
    pass


def parse_pad(text):
    """Mirror of parse_ps5_pad_config (PS5PadConfig.cpp:52-125)."""
    b = text.encode("latin1")
    if not b or len(b) > 16384:
        raise IniError("must contain 1..16384 bytes")
    for c in b:
        if c > 0x7E or (c < 0x20 and c not in (0x0A, 0x0D, 0x09)):
            raise IniError("must be ASCII text without control bytes")
    section, pad_seen, bind_seen, user_seen = None, False, False, False
    rows = []
    for no, raw in enumerate(text.split("\n"), 1):
        line = raw.strip(" \t\r")
        if not line or line[0] in "#;":
            continue
        if line[0] == "[":
            u = line.upper()
            if u == "[PS5PAD]" and not pad_seen:
                pad_seen, section = True, "pad"
                continue
            if u == "[BINDINGS]" and not bind_seen:
                bind_seen, section = True, "bind"
                continue
            raise IniError(f"line {no}: unknown or repeated section")
        if line.count("=") != 1:
            raise IniError(f"line {no}: expected exactly one '='")
        left, right = (s.strip(" \t\r") for s in line.split("="))
        if not left or not right:
            raise IniError(f"line {no}: empty name or value")
        if section == "pad":
            if left.upper() != "USER" or user_seen:
                raise IniError(f"line {no}: unknown or repeated PS5Pad option")
            user_seen = True
            if not re.fullmatch(r"-?[0-9]+", right) or int(right) < -1:
                raise IniError(f"line {no}: bad User")
            continue
        if section != "bind":
            raise IniError(f"line {no}: binding requires [Bindings] section")
        if len(rows) == 128:
            raise IniError(f"line {no}: at most 128 bindings")
        mask = set()
        for comp in left.split("+"):
            name = comp.strip(" \t\r").upper()
            if not name:
                raise IniError(f"line {no}: empty chord component")
            if name not in BUTTONS:
                raise IniError(f"line {no}: unknown controller button {name}")
            if name in mask:
                raise IniError(f"line {no}: repeated button")
            mask.add(name)
        parts = [p.strip(" \t\r").upper() for p in right.split("+")]
        if any(not p for p in parts):
            raise IniError(f"line {no}: empty key component")
        mods = parts[:-1]
        for m in mods:
            if m not in MODIFIERS:
                raise IniError(f"line {no}: unknown key modifier {m}")
        if len(set(mods)) != len(mods):
            raise IniError(f"line {no}: repeated key modifier")
        key = parts[-1]
        fm = frozenset(mask)
        for old in rows:
            if old["mask"] == fm:
                raise IniError(f"line {no}: duplicate controller chord")
            if old["mask"] <= fm or fm <= old["mask"]:
                raise IniError(f"line {no}: chord contains / is contained in another chord")
        rows.append({"line": no, "chord": left, "mask": fm, "key": key, "mods": mods})
    if not rows:
        raise IniError("requires at least one binding")
    return rows


def check_padini(root, keydef_names):
    text = (root / REL["pad"]).read_bytes().decode("latin1")
    failures, data = [], {}
    try:
        rows = parse_pad(text)
    except IniError as e:
        return record("3 padini", 1, [f"pinned PS5Pad.ini failed the mirror parser: {e}"])
    n = len(rows)
    for r in rows:
        if r["key"] not in keydef_names:
            failures.append(f"line {r['line']}: key {r['key']} not in KeyDef.cpp names")
    card = [("L3+R3", "F7", "spawner menu"), ("UP", "UP_ARROW", "d-pad up"),
            ("DOWN", "DOWN_ARROW", "d-pad down"), ("LEFT", "LEFT_ARROW", "d-pad left"),
            ("RIGHT", "RIGHT_ARROW", "d-pad right"), ("CROSS", "RETURN", "Cross = Enter"),
            ("L1+R1", "F12", "self-test")]
    found = {frozenset(r["chord"].upper().split("+")): r for r in rows}
    confirmed = []
    for chord, key, why in card:
        r = found.get(frozenset(chord.split("+")))
        if not r or r["key"] != key or r["mods"]:
            failures.append(f"launch-card row {chord} = {key} ({why}) not in the pinned file")
        else:
            confirmed.append(f"PS5Pad.ini:{r['line']} {r['chord']} = {r['key']} ({why})")
    if len(rows) != len(card):
        failures.append(f"expected exactly {len(card)} rows, found {len(rows)}")
    data["rows"] = [f"line {r['line']}: {r['chord']} = {'+'.join(r['mods'] + [r['key']])}" for r in rows]
    data["launch_card_confirmed"] = confirmed
    data["minimap_keys_in_file"] = [k for _, _, k in MINIMAP_TO_UE4SS if any(r["key"] == k for r in rows)]
    if data["minimap_keys_in_file"]:
        failures.append(f"unexpected minimap keys already mapped: {data['minimap_keys_in_file']}")
    RESULT["_rows"] = rows
    return record("3 padini", n, failures, data)


# ---------------------------------------------------------------- 4 freechords
def check_freechords():
    rows = RESULT.get("_rows") or []
    if not rows:
        # Fail closed: without the pinned rows (check 3) there is nothing real to
        # check against; the generated combinations alone prove nothing.
        return record("4 freechords", 0, ["no pinned PS5Pad.ini rows available (check 3 read nothing)"])
    masks = [r["mask"] for r in rows]
    legal_all = 0
    legal_policy = []
    n = 0
    for k in range(1, len(BUTTONS) + 1):
        for combo in itertools.combinations(BUTTONS, k):
            s = frozenset(combo)
            n += 1
            if any(s <= m or m <= s for m in masks):
                continue
            legal_all += 1
            if s <= set(POLICY_FREE):
                legal_policy.append("+".join(b for b in BUTTONS if b in s))
    pairs_free = [p for p in PAIRS10 if frozenset(p.split("+")) not in masks]
    failures = []
    if len(pairs_free) != 9:
        failures.append(f"expected 9 free pairs10 slots, found {len(pairs_free)}")
    data = {"subsets_examined": n, "legal_new_chords_any_button": legal_all,
            "legal_policy_free_chords": legal_policy, "free_pairs10_slots": pairs_free,
            "note": "any chord containing UP/DOWN/LEFT/RIGHT/CROSS, or both L3+R3, or both L1+R1, "
                    "or equal to L3/R3/L1/R1 alone, is refused by PS5PadConfig.cpp:115-120"}
    print(f"        legal new chords (all 16 buttons): {legal_all}; policy-free legal: {len(legal_policy)}; "
          f"free pairs10 slots: {len(pairs_free)}")
    # items = the pinned rows used as constraints (from a file) + generated subsets
    return record("4 freechords", n + len(rows), failures, data, synthetic=n)


# ---------------------------------------------------------------- 5 gates
def resolve(root, p):
    """Receipt paths are absolute paths into this worktree; re-anchor them at
    --root so a run against another root never reads the real tree."""
    s = str(p)
    marker = "/translation-core/what/"
    if marker in s:
        return root / s.split(marker, 1)[1]
    q = Path(s)
    return q if q.is_absolute() else root / q


def units_of(receipt):
    return {u.get("name", u["source"]): u for u in receipt["selected_units"] if "source" in u}


_CHAIN = {}


def b4_chain(root):
    """Provenance of the launched B4 PRX, resolved once:
    PRX sha (ledger line + frozen .sha256 + hash of the frozen PRX + frozen
    receipt) -> runtime object sha -> BUILD-INPUTS (current or archived) ->
    its two composition receipts (sha-pinned) -> unit sources."""
    if "v" in _CHAIN:
        return _CHAIN["v"]
    failures, data, n = [], {}, 0
    ledger_lines = (root / REL["ledger"]).read_text().splitlines()
    n += 1
    hit = [i for i, l in enumerate(ledger_lines, 1) if "B4-shim-final-stack" in l and B4_PRX_SHA in l]
    if not hit:
        failures.append("BUILD-LEDGER has no B4-shim-final-stack row carrying the B4 PRX sha")
    data["ledger_row"] = f"BUILD-LEDGER-20261012.md:{hit[0]}" if hit else None
    sha_file = (root / REL["b4_prx_sha"]).read_text().split()[0]
    n += 1
    if sha_file != B4_PRX_SHA:
        failures.append(f"frozen .prx.sha256 says {sha_file}, expected {B4_PRX_SHA}")
    prx_sha = sha((root / REL["b4_prx"]).read_bytes())
    n += 1
    if prx_sha != B4_PRX_SHA:
        failures.append(f"frozen B4 PRX hashes to {prx_sha}, expected {B4_PRX_SHA}")
    rec = json.loads((root / REL["b4_receipt"]).read_text())
    n += 1
    if rec.get("prx", {}).get("sha256") != B4_PRX_SHA:
        failures.append("frozen FINAL-RECEIPT prx.sha256 is not the B4 sha")
    obj = rec.get("runtime_object_sha256")
    bi, bi_rel = None, None
    for rel in REL["build_inputs"]:
        p = root / rel
        if p.is_file():
            cand = json.loads(p.read_text())
            if obj and cand.get("runtime_object", {}).get("sha256") == obj:
                bi, bi_rel = cand, rel
                break
    n += 1
    if bi is None:
        raise RuntimeError(f"no BUILD-INPUTS.json with runtime object {obj} in {REL['build_inputs']}")
    receipts = []
    for ent in bi["runtime_composition_receipts"]:
        pass_dir = Path(ent["path"]).parent.name  # pass1 / pass2
        found = None
        for p in (resolve(root, ent["path"]), root / REL["comp_archive"] / pass_dir / "COMPOSITION-RECEIPT.json"):
            if p.is_file():
                b = p.read_bytes()
                if sha(b) == ent["sha256"]:
                    found = (p, json.loads(b))
                    break
        n += 1
        if not found:
            raise RuntimeError(f"no composition receipt with the pinned sha {ent['sha256']} for {pass_dir}")
        receipts.append((pass_dir, found[0], found[1]))
    if len(receipts) != 2:
        failures.append(f"expected 2 composition receipts (pass1/pass2), found {len(receipts)}")
    u1, u2 = units_of(receipts[0][2]), units_of(receipts[-1][2])
    for nm in PAD_UNITS:
        n += 1
        a, b = u1.get(nm), u2.get(nm)
        if not a or not b:
            failures.append(f"unit {nm} missing from a B4 composition receipt")
        elif a["source_sha256"] != b["source_sha256"] or a["source"] != b["source"]:
            failures.append(f"unit {nm} differs between pass1 and pass2")
    # Cross-check only: the older l25-A receipts pin the same pad-unit sources.
    l25 = {}
    for rel in (REL["l25_comp1"], REL["l25_comp2"]):
        p = root / rel
        if p.is_file():
            for nm, u in units_of(json.loads(p.read_text())).items():
                if nm in PAD_UNITS:
                    l25.setdefault(nm, set()).add(u["source_sha256"])
    data["l25A_cross_check"] = {nm: (sorted(l25.get(nm, [])) == [u1[nm]["source_sha256"]] if nm in u1 else False)
                                for nm in PAD_UNITS}
    data["b4_prx_sha256"] = prx_sha
    data["runtime_object_sha256"] = obj
    data["build_inputs"] = bi_rel
    data["composition_receipts"] = [{"pass": d, "path": str(p).split("/what/")[-1], "sha256": sha(p.read_bytes())}
                                    for d, p, _ in receipts]
    data["units_composed"] = len(u1)
    v = {"u1": u1, "u2": u2, "rec": rec, "failures": failures, "data": data, "n": n}
    _CHAIN["v"] = v
    return v


def check_gates(root):
    chain = b4_chain(root)
    units = chain["u1"]
    failures, data, n = list(chain["failures"]), {"b4_chain": chain["data"]}, chain["n"]
    srcs = {}
    for nm in PAD_UNITS:
        u = units.get(nm)
        n += 1
        if not u:
            failures.append(f"unit {nm} missing from the B4 composition receipt")
            continue
        p = resolve(root, u["source"])
        b = p.read_bytes()
        srcs[nm] = b.decode("latin1")
        ok = sha(b) == u.get("source_sha256")
        data[nm] = {"source": u["source"].split("/what/")[-1], "sha256": sha(b), "matches_pin": ok}
        if not ok:
            failures.append(f"{nm} source sha does not match its B4 composition pin")
    # The B4 PRX's own symbol table contains the pad parser, the pad poll and
    # the shim's io.open wrapper; the sidecar is the one the receipt pins.
    sym_pin = chain["rec"].get("symbols_sidecar", {})
    sym_b = (root / REL["b4_symbols"]).read_bytes()
    n += 1
    if sha(sym_b) != sym_pin.get("sha256") or sym_pin.get("prx_sha256") != B4_PRX_SHA:
        failures.append("frozen symbols sidecar does not match the FINAL-RECEIPT pin")
    for needle in (b"RC::Input::parse_ps5_pad_config(", b"RC::Input::PS5PadSource::poll(",
                   b"RC::LuaPathShim::(anonymous namespace)::io_open_wrapper("):
        n += 1
        if needle not in sym_b:
            failures.append(f"B4 PRX symbols lack {needle.decode()}")
    data["b4_symbols_checked"] = ["parse_ps5_pad_config", "PS5PadSource::poll", "LuaPathShim io_open_wrapper"]
    del sym_b
    expect = [
        ("native-pad-PS5PadSource", 154, "(data.buttons & b.buttons) == b.buttons", "chord = all its buttons held"),
        ("native-pad-PS5PadSource", 164, "allow && subscribed[b.key]", "event only for a key some mod subscribed"),
        ("native-pad-PS5PadSource", 168, "m_events.push_back({b.key,b.modifiers})", "event = UE4SS key + modifiers"),
        ("input-handler", 113, "m_platform_handler->process_event(this)", "handler pulls pad events"),
        ("input-handler", 155, "m_key_set.key_data.find(event.key)", "lookup in RegisterKeyBind table"),
        ("input-handler", 162, "callbacks_to_call.emplace_back(key_data.callback)", "only key-bind callbacks run"),
        ("input-handler", 191, "m_subscribed_keys[key] = true", "subscription set by register_keydown_event"),
        ("input-handler", 208, "m_subscribed_keys[key] = true", "subscription set by register_keydown_event (mods)"),
        ("pad-config", 69, "line.front()=='#' || line.front()==';'", "full-line comments only"),
        ("pad-config", 77, "line.find('=',split+1)", "exactly one '=' per line"),
        ("pad-config", 117, "duplicate controller chord", "no duplicate chord"),
        ("pad-config", 119, "controller chord cannot contain another configured chord", "containment law"),
        ("native-pad-input", 21, 'files.read("ue4ss/PS5Pad.ini")', "file read at startup"),
        ("native-pad-input", 24, "parse_ps5_pad_config(file->bytes)", "parse errors throw"),
        ("input-keydef", 40, 'STR("page_up")', "PAGE_UP"), ("input-keydef", 41, 'STR("page_down")', "PAGE_DOWN"),
        ("input-keydef", 42, 'STR("end")', "END"), ("input-keydef", 43, 'STR("home")', "HOME"),
        ("input-keydef", 52, 'STR("ins")', "INS"), ("input-keydef", 53, 'STR("del")', "DEL"),
        ("input-keydef", 115, 'STR("f5")', "F5"), ("input-keydef", 116, 'STR("f6")', "F6"),
    ]
    cited = []
    for nm, ln, needle, why in expect:
        n += 1
        t = srcs.get(nm, "")
        if needle not in line_of(t, ln):
            failures.append(f"{nm}:{ln} expected {needle!r} ({why}); got {line_of(t, ln).strip()!r}")
        else:
            cited.append(f"{data.get(nm, {}).get('source', nm)}:{ln} {why}")
    h = srcs.get("input-handler", "")
    cnt = h.count("m_subscribed_keys[key] = true")
    n += 1
    if cnt != 2:
        failures.append(f"Handler.cpp: expected the subscription write only in the 2 register_keydown_event overloads, found {cnt}")
    ch = (root / REL["l26_changes"]).read_text()
    n += 1
    touched = [w for w in ("PS5Pad", "Handler.cpp", "KeyDef", "NativePadInput", "PadConfig") if w in ch]
    if touched:
        failures.append(f"l26 CHANGES.json mentions pad/input units: {touched}")
    data["cited_lines"] = cited
    data["l26_changes_touch_pad_units"] = touched
    keydef = set(re.findall(r'STR\("([a-z0-9_]+)"\)', srcs.get("input-keydef", "")))
    RESULT["_keydef"] = {k.upper() for k in keydef}
    return record("5 gates", n, failures, data)


# ---------------------------------------------------------------- 6 inject
def check_inject(root):
    chain = b4_chain(root)
    pat = re.compile(rb"InputKey\s*\(|FInputKeyParams|UPlayerInput|InputAxis\s*\(|FSlateApplication|"
                     rb"ProcessKeyDownEvent|OnKeyDown|FKeyEvent|InjectInput|inject_key|SendInput|keybd_event")
    srcs = {u["source"] for u in chain["u1"].values()} | {u["source"] for u in chain["u2"].values()}
    files = [resolve(root, s) for s in sorted(srcs)]
    cand = root / REL["l26_candidate"]
    if not cand.is_dir():
        raise RuntimeError(f"l26 candidate tree missing: {cand}")
    seen = {f.resolve() for f in files}
    files += [p for p in sorted(cand.rglob("*")) if p.is_file() and p.resolve() not in seen]
    n, hits, missing = 0, [], []
    for f in files:
        try:
            b = f.read_bytes()
        except OSError:
            missing.append(str(f))
            continue
        n += 1
        for m in pat.finditer(b):
            line_no = b[:m.start()].count(b"\n") + 1
            hits.append(f"{f}:{line_no}")
    failures = [f"engine-input injection call found: {h}" for h in hits]
    failures += [f"composed source not readable: {m}" for m in missing]
    return record("6 inject", n, failures, {"files_scanned": n, "b4_composed_sources": len(srcs),
                                             "hits": hits,
                                             "limit": "scans .cpp unit sources + every file of the l26 candidate "
                                                      "tree; headers outside that tree are not scanned"})


# ---------------------------------------------------------------- 7 subscribed
def check_subscribed(root):
    pins = json.loads((root / REL["pins"]).read_text())
    roots = [root / REL["upload"], root / REL["spool"]]
    mm_dir = (root / REL["minimap"]).parents[4]
    roots.append(mm_dir)
    bind = re.compile(r"RegisterKeyBind(?:Async)?\s*\(\s*Key\.([A-Z_0-9]+)")
    subs, failures, n = {}, [], 0
    for rel, p in pins.items():
        if not rel.endswith(".lua"):
            continue
        copy = None
        for r in roots:
            f = r / rel
            if f.is_file():
                b = f.read_bytes()
                if sha(b) == p["sha256"] and len(b) == p["size"]:
                    copy = b
                    break
        n += 1
        if copy is None:
            failures.append(f"no verified local copy of pinned {rel}")
            continue
        text = re.sub(r"--\[\[.*?\]\]|--[^\n]*", "", copy.decode("latin1"), flags=re.S)
        for k in bind.findall(text):
            subs.setdefault(k, set()).add(rel.split("/")[2])
    # UE4SS core's own binds (l26 UE4SSProgram.cpp) and the settings keys they use.
    prog_path = root / REL["l26_candidate"] / "src/UE4SSProgram.cpp"
    prog_b = prog_path.read_bytes()
    prog = prog_b.decode("latin1")
    prog_unit = b4_chain(root)["u1"].get("program")
    n += 1
    if not prog_unit or prog_unit["source_sha256"] != sha(prog_b):
        failures.append("candidate UE4SSProgram.cpp is not the source the B4 PRX composed (unit 'program')")
    builtin = [(prog[:m.start()].count("\n") + 1, m.group(1))
               for m in re.finditer(r"register_keydown_event\(\s*Input::Key::([A-Z_0-9]+)", prog)]
    setting_refs = [(prog[:m.start()].count("\n") + 1, m.group(1))
                    for m in re.finditer(r"register_keydown_event\(\s*settings_manager\.\w+\.(\w+)", prog)]
    n += len(builtin) + len(setting_refs)
    ini_pin = pins.get("ue4ss/UE4SS-settings.ini")
    ini_text = None
    for r in (root / REL["spool"], root / REL["upload"]):
        f = r / "ue4ss/UE4SS-settings.ini"
        if f.is_file() and ini_pin:
            b = f.read_bytes()
            if sha(b) == ini_pin["sha256"] and len(b) == ini_pin["size"]:
                ini_text = b.decode("latin1")
                break
    n += 1
    builtin_info = {"literal": [f"UE4SSProgram.cpp:{ln} CONTROL+{k}" for ln, k in builtin], "from_settings": []}
    if ini_text is None:
        failures.append("no verified local copy of pinned ue4ss/UE4SS-settings.ini")
    else:
        for ln, name in setting_refs:
            m = re.search(r"^[ \t]*" + re.escape(name) + r"[ \t]*=[ \t]*([^\r\n;]*)", ini_text, re.M | re.I)
            val = (m.group(1).strip() if m else "<absent>")
            builtin_info["from_settings"].append(f"UE4SSProgram.cpp:{ln} CONTROL+{name} (settings value {val!r})")
            if val.upper() in {u for _, _, u in MINIMAP_TO_UE4SS}:
                failures.append(f"UE4SS core binds a minimap key via {name}={val}")
    minimap_ue = {u for _, _, u in MINIMAP_TO_UE4SS} | {"TAB"}
    for ln, k in builtin:
        if k in minimap_ue:
            failures.append(f"UE4SS core binds minimap key {k} at UE4SSProgram.cpp:{ln}")
    subs = {k: sorted(v) for k, v in sorted(subs.items())}
    clash = [u for _, _, u in MINIMAP_TO_UE4SS if u in subs] + (["TAB"] if "TAB" in subs else [])
    if clash:
        failures.append(f"minimap keys subscribed by some mod (would change the verdict): {clash}")
    print(f"        UE4SS keys subscribed by pinned mods: {subs}")
    print(f"        UE4SS core binds (all need CONTROL, none is a minimap key): {builtin_info}")
    return record("7 subscribed", n, failures, {"subscribed_keys": subs, "minimap_keys_subscribed": clash,
                                                 "ue4ss_core_binds": builtin_info})


# ---------------------------------------------------------------- 8 candidate
def stable_slot(identity, n):
    return int.from_bytes(hashlib.sha256(("FV1|" + identity).encode("ascii")).digest()[:8], "big") % n


def build_candidate(pinned_text):
    rows = parse_pad(pinned_text)
    taken = {PAIRS10.index(r["chord"].upper()) for r in rows if r["chord"].upper() in PAIRS10}
    assigned = []
    for action, engine, ue in MINIMAP_TO_UE4SS:
        start = stable_slot(ue, len(PAIRS10))
        for off in range(len(PAIRS10)):
            c = (start + off) % len(PAIRS10)
            if c not in taken:
                taken.add(c)
                assigned.append((action, engine, ue, PAIRS10[c], off))
                break
        else:
            raise SystemExit(f"pool exhausted for {ue}")
    pinned_sha = sha(pinned_text.encode("latin1"))
    data_rows = [l for l in pinned_text.split("\n")
                 if l.strip() and l.strip()[0] not in ";#[" and "=" in l and not l.strip().upper().startswith("USER")]
    L = [
        "; ue4ss/PS5Pad.ini -- FUTURE-ONLY CANDIDATE. DO NOT UPLOAD. (minimap-keys track, 2026-10-12)",
        "; INERT with the current runtime (1e49188b, B1-B4): the pad bridge only feeds",
        "; UE4SS RegisterKeyBind callbacks (PS5PadSource.cpp:164, Handler.cpp:155-162),",
        "; but TownfallMiniMap reads the ENGINE key state (main.lua:4043,",
        "; PlayerController:IsInputKeyDown) and registers no key binds at all.",
        "; The 8 minimap rows below do nothing until a generic runtime feature",
        "; also feeds mapped keys into the engine input path (not a config change).",
        "; Built by minimap_keys_check.py --write-candidate, not by hand:",
        f"; base = pinned 1736 B profile sha256 {pinned_sha}",
        "; (its 7 rows carried verbatim) + 8 rows placed by the keymap-gen.py FV1",
        "; rule (pairs10 ladder, linear probe, L1+R1 already F12).",
        "; ASCII only; no trailing comments on data lines; LF.",
        "",
        "[PS5Pad]",
        "User = -1",
        "",
        "[Bindings]",
        "; -- rows carried verbatim from the pinned 1736 B profile --",
    ]
    L += data_rows
    L.append("; -- TownfallMiniMap keys (main.lua:66-67), engine names in comments --")
    for action, engine, ue, chord, off in assigned:
        L.append(f";{ue} = minimap {action} (engine key {engine})")
        L.append(f"{chord} = {ue}")
    free = [p for i, p in enumerate(PAIRS10) if i not in taken]
    L.append("; -- free pairs10 slots still available: " + (", ".join(free) or "none"))
    text = "\n".join(L) + "\n"
    return text, assigned, free


def check_candidate(root, write):
    pinned_text = (root / REL["pad"]).read_bytes().decode("latin1")
    text, assigned, free = build_candidate(pinned_text)
    b = text.encode("ascii")
    failures, n = [], 0
    try:
        rows = parse_pad(text)
        n += len(rows)
    except IniError as e:
        failures.append(f"candidate failed the mirror parser: {e}")
        rows = []
    keydef = RESULT.get("_keydef") or set()
    for r in rows:
        if r["key"] not in keydef:
            failures.append(f"candidate key {r['key']} not a KeyDef name")
    reserved = {"OPTIONS", "CROSS", "CIRCLE", "SQUARE", "TRIANGLE", "UP", "RIGHT", "DOWN", "LEFT", "L3", "R3"}
    for action, engine, ue, chord, off in assigned:
        if set(chord.split("+")) & reserved:
            failures.append(f"{ue} landed on a reserved button: {chord}")
    out = HERE / CANDIDATE_NAME
    if write:
        out.write_bytes(b)
        print(f"        wrote {out.name}")
    if out.exists():
        if out.read_bytes() != b:
            failures.append(f"{out.name} on disk differs from the deterministic build (re-run with --write-candidate)")
    else:
        failures.append(f"{out.name} not present (run with --write-candidate)")
    ll = (root / REL["loadlist"]).read_bytes()
    old_row = b"file\tconfig\tue4ss/PS5Pad.ini\t1736\n"
    new_row = f"file\tconfig\tue4ss/PS5Pad.ini\t{len(b)}\n".encode("ascii")
    n += 1
    if ll.count(old_row) != 1:
        failures.append("loadlist does not contain exactly one PS5Pad.ini row of 1736 B")
    new_ll = ll.replace(old_row, new_row)
    data = {
        "file": CANDIDATE_NAME, "size": len(b), "sha256": sha(b), "rows": len(rows),
        "assigned": [{"key": ue, "engine_key": engine, "action": action, "chord": chord, "probe": off}
                     for action, engine, ue, chord, off in assigned],
        "free_pairs10_after": free,
        "loadlist_row_old": old_row.decode().strip("\n").replace("\t", "<TAB>"),
        "loadlist_row_new": new_row.decode().strip("\n").replace("\t", "<TAB>"),
        "ps5pad_size_delta_bytes": len(b) - 1736,
        "loadlist_size_old": len(ll), "loadlist_size_new": len(new_ll),
        "loadlist_sha256_old": sha(ll), "loadlist_sha256_new": sha(new_ll),
        "status": "FUTURE-ONLY, INERT today, DO NOT UPLOAD",
    }
    print(f"        candidate {CANDIDATE_NAME}: {len(b)} B sha256 {sha(b)}; "
          f"loadlist row 1736 -> {len(b)} (delta {len(b) - 1736:+d} B); loadlist {len(ll)} -> {len(new_ll)} B")
    return record("8 candidate", n, failures, data)


# ---------------------------------------------------------------- 9 shimlog
FUNC_DEF = re.compile(r"^(?:local )?function ([A-Za-z_][\w.:]*)\(")


def enclosing(lines, ln):
    for i in range(ln, 0, -1):
        m = FUNC_DEF.match(lines[i - 1])
        if m:
            return m.group(1)
    return "<top level>"


def callers(lines, name):
    # direct calls name(...) and function values passed on, e.g. pcall(name, x)
    pat = re.compile(r"(?<![\w.:])" + re.escape(name) + r"(?=\s*\(|\s*[,)])")
    out = []
    for i, l in enumerate(lines, 1):
        code = l.split("--", 1)[0]
        if pat.search(code) and not FUNC_DEF.match(l):
            out.append((i, enclosing(lines, i)))
    return out


def check_shimlog(root):
    """What a '[SHIM] io-open resolved=<mod>\\settings.txt-><target>' marker in
    a decoded launch log can prove. Mac-only code reading; reads no log."""
    text = (root / REL["minimap"]).read_bytes().decode("latin1")
    lines = text.splitlines()
    failures, data, n = [], {}, 0
    # (a) the minimap opens exactly one file, settings.txt: read at load, write in saveSettings.
    opens = [(i, l.strip()) for i, l in enumerate(lines, 1) if re.search(r"\bio\.(open|lines)\s*\(", l)]
    n += len(opens)
    want = {251: 'io.open(S.settingsPath, "r")', 293: 'io.open(S.settingsPath, "w")'}
    if {i for i, _ in opens} != set(want):
        failures.append(f"expected io.open only at main.lua:251 and :293, found {opens}")
    for ln, needle in want.items():
        if needle not in line_of(text, ln):
            failures.append(f"main.lua:{ln} expected {needle!r}")
    # (b) call graph into saveSettings: every path except tick() needs an engine
    # key-down edge (onKey) or the open F5 menu (M.open is set only by M.openMenu).
    expect = {
        "saveSettings": {(3427, "onKey"), (3435, "onKey"), (3443, "onKey"), (3535, "M.changed"),
                         (3830, "M.togglePause"), (4214, "tick")},
        "M.changed": {(3528, "M.set"), (3556, "M.pick"), (3561, "M.pick")},
        "M.set": {(3564, "M.pick")},
        "M.pick": {(3546, "M.change"), (3886, "M.mouse")},
        "M.change": {(3423, "onKey"), (3884, "M.mouse"), (3901, "M.key"), (3902, "M.key"), (3903, "M.key")},
        "M.togglePause": {(3863, "M.mouse")},
        "M.key": {(3982, "M.poll"), (3987, "M.poll")},
        "M.mouse": {(3998, "M.poll")},
        "M.poll": {(4233, "tick"), (4028, "M.frame")},
        "M.openMenu": {(3414, "onKey")},
        "onKey": {(4049, "pollKeys")},
    }
    graph = {}
    for name, exp in expect.items():
        got = set(callers(lines, name))
        n += len(got)
        graph[name] = sorted(f"main.lua:{i} in {f}" for i, f in got)
        if got != exp:
            failures.append(f"callers of {name}: expected {sorted(exp)}, found {sorted(got)}")
    opens_menu = [i for i, l in enumerate(lines, 1) if re.search(r"\bM\.open\s*(,[^=]*)?=\s*true", l)]
    n += 1
    if opens_menu != [3916] or enclosing(lines, 3916) != "M.openMenu":
        failures.append(f"M.open = true expected only at main.lua:3916 in M.openMenu, found {opens_menu}")
    gated = [(4224, "if M.open then"), (4021, "if not M.open and not W.active then return end"),
             (4024, "if W.active then"), (4026, "return"), (4211, "if not S.hintShown then"), (4214, "saveSettings()"),
             (275, 'elseif k == "hintShown" then S.hintShown = v == "true"')]
    for ln, needle in gated:
        n += 1
        if needle not in line_of(text, ln):
            failures.append(f"main.lua:{ln} expected {needle!r}")
    data["minimap_io_opens"] = [f"main.lua:{i} {l}" for i, l in opens]
    data["call_graph_into_saveSettings"] = graph
    # (c) LuaPathShim.cpp (the source the B4 PRX composed): the marker prints the
    # sandbox target for write modes and the mod-root path for read modes.
    unit = b4_chain(root)["u1"].get("lua-pathshim")
    n += 1
    if not unit:
        raise RuntimeError("unit lua-pathshim missing from the B4 composition receipt")
    sb = resolve(root, unit["source"]).read_bytes()
    if sha(sb) != unit["source_sha256"]:
        failures.append("LuaPathShim.cpp does not match its B4 composition pin")
    shim = sb.decode("latin1")
    shim_facts = [
        (92, "constexpr int kMaxMarkers = 10;", "at most 10 io-open markers per Lua state"),
        (163, "int resolved_markers = 0;", "marker counter lives in the per-state context"),
        (676, 'std::strpbrk(mode, "wa+")', "write mode = any of w, a, +"),
        (851, "const std::string target = sandbox_target(*context, key);", "write: target = per-mod writable sandbox"),
        (855, 'Output::send(STR("[SHIM] io-open resolved={}->{}\\n"), ensure_str(logical), ensure_str(target));',
         "write marker prints the sandbox target"),
        (871, "const std::string modroot = modroot_physical(*context, key);", "read: first try the mod root"),
        (875, 'Output::send(STR("[SHIM] io-open resolved={}->{}\\n"), ensure_str(logical), ensure_str(modroot));',
         "read marker prints the mod-root path, even when the read is later served from the sandbox"),
        (881, "const std::string sandbox = sandbox_of_key(*context, key);", "read falls back to the sandbox copy"),
        (1653, "if (context_is_anchored(L))", "one shim context per Lua state"),
        (1687, ": derive_writable_root(context->modbridge_root);", "writable root derived from the modbridge root"),
    ]
    for ln, needle, why in shim_facts:
        n += 1
        if needle not in line_of(shim, ln):
            failures.append(f"LuaPathShim.cpp:{ln} expected {needle!r} ({why}); got {line_of(shim, ln).strip()!r}")
    data["shim_source"] = {"path": unit["source"].split("/what/")[-1], "sha256": sha(sb)}
    data["shim_facts"] = [f"LuaPathShim.cpp:{ln} {why}" for ln, _, why in shim_facts]
    data["decision_rule_for_driver"] = [
        "Apply only to THIS launch's decoded log (harvest/decode_session_log.py); never to an earlier session.",
        "Marker 'io-open resolved=ue4ss/Mods/TownfallMiniMap\\settings.txt-><target>' whose target is under the "
        "writable root (the root the '[SHIM] write-area TownfallMiniMap -> ...' line names) = a WRITE open "
        "(LuaPathShim.cpp:851-855). With no keyboard, the only write path is the first-show block "
        "(main.lua:4211-4214). Reaching it means that, in that tick: a local PlayerController was found, all 8 "
        "IsInputKeyDown polls returned without error, pawn.MapRenderComponent was valid, the widget was built, "
        "and show was true (main.lua:4064-4188).",
        "A target under the mod root (not the writable root) = a READ open (loadSettings, main.lua:251): proves "
        "only that the mod loaded.",
        "No write marker does NOT prove the map failed: once a settings.txt with hintShown=true exists in the "
        "writable area, the first-show write never happens again (main.lua:275, 4211); the marker budget is "
        "10 per Lua state (LuaPathShim.cpp:92).",
        "Even a write marker does not prove the map stayed visible later or that there was no stutter.",
    ]
    return record("9 shimlog", n, failures, data)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=str(HERE.parents[3]), help="worktree root")
    ap.add_argument("--result", default="check-result.json",
                    help="result file name, written inside this script's folder only")
    ap.add_argument("--write-candidate", action="store_true")
    a = ap.parse_args()
    root = Path(a.root).resolve()
    result_path = (HERE / a.result).resolve()
    if HERE not in result_path.parents:
        print(f"refusing --result outside {HERE}")
        sys.exit(2)
    print(f"minimap_keys_check.py (Mac-only, read-only) root={root}")

    def run(name, fn, *args):
        try:
            fn(*args)
        except Exception as e:  # fail closed: an exception is a failed check with 0 items
            record(name, 0, [f"exception: {type(e).__name__}: {e}"])

    run("1 pins", check_pins, root)
    run("2 minimap", check_minimap, root)
    run("5 gates", check_gates, root)            # loads KeyDef names used by check_padini
    run("3 padini", lambda r: check_padini(r, RESULT.get("_keydef") or set()), root)
    run("4 freechords", check_freechords)
    run("6 inject", check_inject, root)
    run("7 subscribed", check_subscribed, root)
    run("8 candidate", check_candidate, root, a.write_candidate and root == HERE.parents[3])
    run("9 shimlog", check_shimlog, root)
    RESULT.pop("_rows", None)
    RESULT["_keydef_count"] = len(RESULT.pop("_keydef", set()))
    total = sum(c["checked"] for c in RESULT["checks"].values())
    synthetic = sum(c.get("synthetic", 0) for c in RESULT["checks"].values())
    from_files = total - synthetic
    RESULT["root"] = str(root)
    RESULT["total_items_checked"] = total
    RESULT["items_generated_in_memory"] = synthetic
    RESULT["items_from_files"] = from_files
    ok = RESULT["ok"] and from_files > 0
    RESULT["overall"] = "PASS" if ok else "FAIL"
    result_path.write_text(json.dumps(RESULT, indent=1, default=sorted) + "\n")
    print(f"TOTAL: {total} item(s) checked across {len(RESULT['checks'])} checks "
          f"({from_files} from files, {synthetic} button combinations generated in memory); "
          f"overall {RESULT['overall']}; result -> {result_path.name}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
