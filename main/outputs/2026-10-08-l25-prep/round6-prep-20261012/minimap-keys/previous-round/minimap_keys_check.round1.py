#!/usr/bin/env python3
"""minimap-keys track checker (Mac-only, read-only; stdlib only).

Run:  python3 -I minimap_keys_check.py [--root WORKTREE] [--write-candidate]

Default mode only READS files and writes check-result.json next to this
script. --write-candidate additionally (re)writes the future-only candidate
PS5Pad.FUTURE-minimap-DO-NOT-UPLOAD.ini next to this script. Nothing else is
ever written. No network, no console contact.

Every check prints how many items it checked; a check that checks zero items
FAILS, and any failed check makes the exit status non-zero.

Checks:
  1 pins        minimap main.lua + PS5Pad.ini + loadlist match rowcheck-pins.json
  2 minimap     RegisterKeyBind-family calls (expect 0), engine key polls, key
                tables, settings.txt keys, show-by-default facts (file:line)
  3 padini      PS5Pad.ini parsed with a mirror of PS5PadConfig.cpp rules;
                launch-card rows present
  4 freechords  legal new chords under the containment law (all 16 buttons)
                and under the MAPPING.md policy (TOUCH_PAD L1 R1 L2 R2 only)
  5 gates       composed pad/input sources match the l25-A composition pins and
                the exact delivery-gate lines say what the report cites
  6 inject      engine-input injection call scan over every composed source
                + the l26 candidate tree (expect 0 hits)
  7 subscribed  UE4SS key subscriptions of every pinned Lua mod; the
                minimap's keys must be absent
  8 candidate   future-only candidate: deterministic bytes, mirror-parse,
                containment, size/sha256, implied loadlist row change
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
    "comp1": "outputs/2026-10-08-l25-build-variants/l25-A/pass1/COMPOSITION-RECEIPT.json",
    "comp2": "outputs/2026-10-08-l25-build-variants/l25-A/pass2/COMPOSITION-RECEIPT.json",
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

RESULT = {"checks": {}, "ok": True}


def sha(b):
    return hashlib.sha256(b).hexdigest()


def record(name, checked, failures, data=None):
    ok = checked > 0 and not failures
    RESULT["checks"][name] = {"checked": checked, "failures": failures, "pass": ok, "data": data or {}}
    if not ok:
        RESULT["ok"] = False
    status = "PASS" if ok else "FAIL"
    print(f"[{status}] {name}: checked {checked} item(s), {len(failures)} failure(s)")
    for f in failures:
        print(f"        - {f}")
    if checked == 0:
        print("        - zero items checked: a check that scans nothing is a failed check")
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
    # Show-by-default facts with exact lines.
    facts = [
        (78, "enabled = true", "S.enabled defaults to true"),
        (149, 'id = "blankMap"', "blankMap row present"),
        (149, 'def = "on"', "blankMap default on"),
        (245, '"\\\\settings.txt"', "settings path ends in \\settings.txt"),
        (1996, "trusted = true", "default layout is trusted"),
        (4043, "pc:IsInputKeyDown", "main hotkeys polled from engine key state"),
        (4045, "S.pc = nil", "a failing IsInputKeyDown drops the controller"),
        (4084, "if not pollKeys(pc) then setShown(false); return end", "poll failure hides the map"),
        (4181, "M.defaultLayout()", "default layout used before any map snapshot"),
        (4188, "local show = S.enabled and (covered and S.hasSnapshot or S.blank or W.active) and hudOk", "show formula"),
        (4213, "toast(HINT_TEXT, HINT_TICKS, true)", "keyboard hint toast on first show"),
        (4268, "LoopInGameThreadWithDelay(TICK_MS, safeTick)", "50 ms tick loop"),
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
    return record("4 freechords", n, failures, data)


# ---------------------------------------------------------------- 5 gates
def load_units(root):
    units = {}
    for rel in (REL["comp1"], REL["comp2"]):
        r = json.loads((root / rel).read_text())
        for u in r["selected_units"]:
            if "source" in u:
                units.setdefault(u.get("name", u["source"]), u)
    return units


def resolve(root, p):
    q = Path(p)
    return q if q.is_absolute() else root / q


def check_gates(root):
    units = load_units(root)
    failures, data, n = [], {}, 0
    names = ["native-pad-PS5PadSource", "pad-config", "native-pad-input", "input-handler",
             "input-keydef", "native-pad-PS5PadNative"]
    srcs = {}
    for nm in names:
        u = units.get(nm)
        n += 1
        if not u:
            failures.append(f"unit {nm} missing from the l25-A composition receipt")
            continue
        p = resolve(root, u["source"])
        b = p.read_bytes()
        srcs[nm] = b.decode("latin1")
        ok = sha(b) == u.get("source_sha256")
        data[nm] = {"source": u["source"].split("/what/")[-1], "sha256": sha(b), "matches_pin": ok}
        if not ok:
            failures.append(f"{nm} source sha does not match its composition pin")
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
    units = load_units(root)
    pat = re.compile(rb"InputKey\s*\(|FInputKeyParams|UPlayerInput|InputAxis\s*\(|FSlateApplication|"
                     rb"ProcessKeyDownEvent|OnKeyDown|FKeyEvent|InjectInput|inject_key|SendInput|keybd_event")
    files = [resolve(root, u["source"]) for u in units.values()]
    files += [p for p in (root / REL["l26_candidate"]).rglob("*") if p.is_file()]
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
    return record("6 inject", n, failures, {"files_scanned": n, "hits": hits})


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
    prog = prog_path.read_bytes().decode("latin1")
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=str(HERE.parents[3]), help="worktree root")
    ap.add_argument("--write-candidate", action="store_true")
    a = ap.parse_args()
    root = Path(a.root).resolve()
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
    run("8 candidate", check_candidate, root, a.write_candidate)
    RESULT.pop("_rows", None)
    RESULT["_keydef_count"] = len(RESULT.pop("_keydef", set()))
    total = sum(c["checked"] for c in RESULT["checks"].values())
    RESULT["total_items_checked"] = total
    (HERE / "check-result.json").write_text(json.dumps(RESULT, indent=1, default=sorted) + "\n")
    print(f"TOTAL: {total} item(s) checked across {len(RESULT['checks'])} checks; "
          f"overall {'PASS' if RESULT['ok'] and total > 0 else 'FAIL'}")
    sys.exit(0 if RESULT["ok"] and total > 0 else 1)


if __name__ == "__main__":
    main()
