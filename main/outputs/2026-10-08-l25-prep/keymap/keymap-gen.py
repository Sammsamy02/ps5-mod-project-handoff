#!/usr/bin/env python3
# keymap-gen.py -- AUTO CONTROLLER MAPPING generator for the PS5 UE4SS port.
#
# Parses each mod's main.lua for RegisterKeyBind / RegisterKeyBindAsync uses
# (direct calls AND table-driven `["Key"] = Key.X, ["ModifierKeys"] = {...}`
# entries, e.g. the Keybinds mod), then emits a ue4ss/PS5Pad.ini that maps
# free DualSense buttons/chords to the still-unmapped keys.
#
# Laws implemented (verified against the RETAINED parser the payload runs):
#   .superpowers/native-dependency-spike/ue4ss-native-pad-config-1/src/PS5PadConfig.cpp
#   (parse_ps5_pad_config) and KeyDef.cpp (string_to_key):
#   - sections [PS5Pad] (User = -1) then [Bindings]; each at most once
#   - rows `CHORD = [MODIFIERS+]KEY`, exactly one '=' per data line
#   - buttons (case-insensitive): L3 R3 OPTIONS UP RIGHT DOWN LEFT L2 R2 L1 R1
#     TRIANGLE CIRCLE CROSS SQUARE TOUCH_PAD; no repeated button in a chord
#   - modifiers: SHIFT CONTROL ALT (each at most once, prefix the key)
#   - keys: exact UE4SS KeyDef names (case-insensitive), see KEY_NAMES below
#   - at most 128 bindings; at least one binding required
#   - NO chord may contain another configured chord (bitmask subset, either
#     direction); no duplicate chord
#   - file must be 1..16384 bytes of ASCII (bytes <= 0x7e; of controls only
#     \n \r \t allowed). ';' or '#' line comments only -- NO trailing comments
#     on data lines, NO non-ASCII (an em-dash once nearly killed a launch).
#   - the file is MFA SIZE-EXACT admitted: its byte length is pinned in the
#     modforge loadlist row `file<TAB>config<TAB>ue4ss/PS5Pad.ini<TAB><bytes>`;
#     a size mismatch fails activation all-or-nothing (mfa_activate_failed
#     code=-8). Regenerating with a different mod set changes the size =>
#     re-pin the row (wc -c the staged file) before upload, readback cmp after.
#
# Reserved (never auto-assigned; assumptions documented in MAPPING.md):
#   OPTIONS            game pause (deliberately unbound since the l12 rebind)
#   CROSS CIRCLE SQUARE TRIANGLE   game face actions
#   UP RIGHT DOWN LEFT d-pad (game menu navigation; carry pinned rows only)
#   L3 R3              stick clicks; L3+R3 is the UNIVERSAL MOD-MENU chord
# Pinned console-proven rows (preserved verbatim, from --pin-pad):
#   L3+R3 = F7, UP = UP_ARROW, DOWN = DOWN_ARROW, CROSS = RETURN,
#   LEFT = LEFT_ARROW, RIGHT = RIGHT_ARROW   (TownfallItemSpawner 6 binds)
#
# Free-button ladders (antichains: no member contains another, and none
# intersects a pinned chord, so the containment law always holds):
#   ergonomic (7): TOUCH_PAD; L1+R1; L2+R2; L1+R2; L2+R1; L1+L2; R1+R2
#   pairs10   (10): all 2-button pairs over {TOUCH_PAD,L1,R1,L2,R2}
# There is no back button / gyro / touch-swipe in the parser's 16-bit button
# space; pairs10 is the maximum antichain over the 5 free buttons
# (C(5,2) = 10), so 10 is the hard ceiling for auto-assigned rows.
#
# Stability: slot = ladder[sha256("FV1|" + key_identity) mod len(ladder)];
# collisions resolve by forward linear probing (deterministic suffix).
# The hash is over the KEY IDENTITY ONLY (modifiers+key), which subsumes the
# "same mod+key -> same button" rule: a row never moves when unrelated mods
# are enabled/disabled; only a probed collision against a new key, or pool
# exhaustion, can shift/drop an assignment, and both are reported.
#
# Usage:
#   keymap-gen.py --mod NAME=PATH [--mod NAME=PATH ...]
#                 [--pin-pad PINNED_PS5Pad.ini] [--ladder ergonomic|pairs10]
#                 [--out OUT.ini] [--strict] [--quiet]
#   keymap-gen.py --check PAD_INI --mod NAME=PATH [...] [--expect-size N]
#                 [--pin-pad PINNED_PS5Pad.ini] [--strict]
#
# --mod order IS the assignment priority order (mods listed first win slots
# first when the pool runs short). Exit codes: 0 clean, 1 warnings, 2 error.

import argparse
import hashlib
import json
import re
import sys

VERSION = "keymap-gen.py 1.0 (agent FV, 2026-10-08-l25-prep/keymap)"

# ---------------------------------------------------------------------------
# Parser-faithful tables
# ---------------------------------------------------------------------------

BUTTONS = ["L3", "R3", "OPTIONS", "UP", "RIGHT", "DOWN", "LEFT",
           "L2", "R2", "L1", "R1",
           "TRIANGLE", "CIRCLE", "CROSS", "SQUARE", "TOUCH_PAD"]

MODIFIERS = ["SHIFT", "CONTROL", "ALT"]          # emitted in this order

RESERVED_BUTTONS = ["OPTIONS",
                    "CROSS", "CIRCLE", "SQUARE", "TRIANGLE",
                    "UP", "RIGHT", "DOWN", "LEFT",
                    "L3", "R3"]

# UE4SS KeyDef names accepted by string_to_key (KeyDef.cpp:12-171), uppercased.
KEY_NAMES = set("""
LEFT_MOUSE_BUTTON RIGHT_MOUSE_BUTTON CANCEL MIDDLE_MOUSE_BUTTON XBUTTON_ONE
XBUTTON_TWO BACKSPACE TAB CLEAR RETURN PAUSE CAPS_LOCK IME_KANA IME_HANGUEL
IME_HANGUL IME_ON IME_JUNJA IME_FINAL IME_HANJA IME_KANJI IME_OFF ESCAPE
IME_CONVERT IME_NONCONVERT IME_ACCEPT IME_MODECHANGE SPACE PAGE_UP PAGE_DOWN
END HOME LEFT_ARROW UP_ARROW RIGHT_ARROW DOWN_ARROW SELECT PRINT EXECUTE
PRINT_SCREEN INS DEL HELP ZERO ONE TWO THREE FOUR FIVE SIX SEVEN EIGHT NINE
A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
LEFT_WIN RIGHT_WIN APPS SLEEP NUM_ZERO NUM_ONE NUM_TWO NUM_THREE NUM_FOUR
NUM_FIVE NUM_SIX NUM_SEVEN NUM_EIGHT NUM_NINE MULTIPLY ADD SEPARATOR SUBTRACT
DECIMAL DIVIDE
F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12 F13 F14 F15 F16 F17 F18 F19 F20 F21
F22 F23 F24
NUM_LOCK SCROLL_LOCK BROWSER_BACK BROWSER_FORWARD BROWSER_REFRESH BROWSER_STOP
BROWSER_SEARCH BROWSER_FAVORITES BROWSER_HOME VOLUME_MUTE VOLUME_DOWN VOLUME_UP
MEDIA_NEXT_TRACK MEDIA_PREV_TRACK MEDIA_STOP MEDIA_PLAY_PAUSE LAUNCH_MAIL
LAUNCH_MEDIA_SELECT LAUNCH_APP1 LAUNCH_APP2
OEM_ONE OEM_PLUS OEM_COMMA OEM_MINUS OEM_PERIOD OEM_TWO OEM_THREE OEM_FOUR
OEM_FIVE OEM_SIX OEM_SEVEN OEM_EIGHT OEM_102 IME_PROCESS PACKET ATTN CRSEL
EXSEL EREOF PLAY ZOOM NONAME PA1 OEM_CLEAR
""".split())

PS5PAD_CONFIG_LIMIT = 16 * 1024
MAX_BINDINGS = 128

LADDERS = {
    "ergonomic": ["TOUCH_PAD",
                  "L1+R1", "L2+R2", "L1+R2", "L2+R1", "L1+L2", "R1+R2"],
    "pairs10": ["TOUCH_PAD+L1", "TOUCH_PAD+R1", "TOUCH_PAD+L2", "TOUCH_PAD+R2",
                "L1+R1", "L1+R2", "L1+L2", "R1+L2", "R1+R2", "L2+R2"],
}

# ---------------------------------------------------------------------------
# Lua scanning
# ---------------------------------------------------------------------------

def strip_lua_comments(src: str) -> str:
    """Remove -- line comments and --[[ ]] / --[=*[ ]=*] block comments.
    Heuristic (mods here put no '--' inside strings); comment text is dropped
    so references like '-- RegisterKeyBind :2118' cannot create false binds
    (zzdiag-v10 main.lua:7 has exactly such a line)."""
    out = []
    i, n = 0, len(src)
    while i < n:
        if src[i] == '"' or src[i] == "'":
            quote = src[i]
            out.append(quote)
            i += 1
            while i < n and src[i] != quote:
                out.append(src[i])
                i += 1
            if i < n:
                out.append(quote)
                i += 1
            continue
        if src.startswith("--", i):
            m = re.match(r"--\[(=*)\[", src[i:])
            if m:
                close = "]" + m.group(1) + "]"
                j = src.find(close, i + m.end())
                i = n if j < 0 else j + len(close)
                out.append(" ")
                continue
            j = src.find("\n", i)
            i = n if j < 0 else j
            out.append(" ")
            continue
        out.append(src[i])
        i += 1
    return "".join(out)


MODIFIER_TOKEN_RE = re.compile(r"ModifierKey\.([A-Za-z0-9_]+)")
DIRECT_BIND_RE = re.compile(
    r"\bRegisterKeyBind(?:Async)?\s*\(\s*Key\.([A-Za-z0-9_]+)\s*"
    r"(?:,\s*(\{[^{}]*\}|ModifierKey\.[A-Za-z0-9_]+))?")
TABLE_KEY_RE = re.compile(r'\[\s*"?Key"?\s*\]\s*=\s*Key\.([A-Za-z0-9_]+)')
TABLE_MODS_RE = re.compile(r'\[\s*"?ModifierKeys"?\s*\]\s*=\s*(\{[^{}]*\})')


def parse_modifiers(blob) -> list:
    """Extract an ordered unique modifier list from a modifier argument."""
    if not blob:
        return []
    mods = []
    for m in MODIFIER_TOKEN_RE.findall(blob):
        u = m.upper()
        if u not in MODIFIERS:
            raise ValueError(f"unknown ModifierKey.{m} (expected SHIFT/CONTROL/ALT)")
        if u not in mods:
            mods.append(u)
    return sorted(mods, key=MODIFIERS.index)


def extract_binds(path: str) -> list:
    """Return a list of (lineno, key, mods_tuple) binds in one main.lua."""
    raw = open(path, "r", encoding="utf-8", errors="replace").read()
    line_starts = [0]
    for m in re.finditer(r"\n", raw):
        line_starts.append(m.end())
    stripped = strip_lua_comments(raw)
    binds = []

    def lineno(pos):
        import bisect
        return bisect.bisect_right(line_starts, pos)

    for m in DIRECT_BIND_RE.finditer(stripped):
        key = m.group(1).upper()
        mods = parse_modifiers(m.group(2))
        binds.append((lineno(m.start()), key, tuple(mods)))

    # Table-driven bindings, e.g. the Keybinds mod:
    #   ["ObjectDumper"] = {["Key"] = Key.J, ["ModifierKeys"] = {ModifierKey.CONTROL}},
    # Guard: only chunks that are NOT a RegisterKeyBind argument position.
    for m in TABLE_KEY_RE.finditer(stripped):
        before = stripped[max(0, m.start() - 80):m.start()]
        if re.search(r"RegisterKeyBind(?:Async)?\s*\(\s*$", before):
            continue  # already captured by DIRECT_BIND_RE shape
        window = stripped[m.start():m.start() + 300]
        mm = TABLE_MODS_RE.search(window)
        mods = parse_modifiers(mm.group(1) if mm else None)
        binds.append((lineno(m.start()), m.group(1).upper(), tuple(mods)))
    return binds


def key_identity(key: str, mods) -> str:
    return "+".join(list(mods) + [key])


# ---------------------------------------------------------------------------
# PS5Pad.ini parsing (mirror of parse_ps5_pad_config, for --check and
# self-validation)
# ---------------------------------------------------------------------------

class IniError(Exception):
    pass


def parse_pad_ini(text: str):
    """Return list of rows: (line, chord_str, frozenset(buttons), key, mods).
    Raises IniError('line N: ...') on any parser-rule violation."""
    if not text:
        raise IniError("must contain 1..16384 ASCII bytes")
    for ch in text:
        if ord(ch) > 0x7e or (ord(ch) < 0x20 and ch not in "\n\r\t"):
            raise IniError("must be ASCII text without control bytes")
    if len(text.encode("ascii")) > PS5PAD_CONFIG_LIMIT:
        raise IniError("must contain 1..16384 ASCII bytes")
    section, pad_seen, bindings_seen, user_seen = None, False, False, False
    rows = []
    for number, raw_line in enumerate(text.splitlines(), 1):
        line = raw_line.strip(" \t\r")
        if not line or line[0] in ";#":
            continue
        if line[0] == "[":
            up = line.upper()
            if up == "[PS5PAD]" and not pad_seen:
                pad_seen, section = True, "pad"
                continue
            if up == "[BINDINGS]" and not bindings_seen:
                bindings_seen, section = True, "bindings"
                continue
            raise IniError(f"line {number}: unknown or repeated section")
        if line.count("=") != 1:
            raise IniError(f"line {number}: expected exactly one '='")
        left, right = (s.strip(" \t\r") for s in line.split("="))
        if not left or not right:
            raise IniError(f"line {number}: empty name or value")
        if section == "pad":
            if left.upper() != "USER" or user_seen:
                raise IniError(f"line {number}: unknown or repeated PS5Pad option")
            user_seen = True
            try:
                if int(right) < -1:
                    raise ValueError
            except ValueError:
                raise IniError(f"line {number}: User must be -1 or nonnegative")
            continue
        if section != "bindings":
            raise IniError(f"line {number}: binding requires [Bindings] section")
        if len(rows) == MAX_BINDINGS:
            raise IniError(f"line {number}: at most 128 bindings are allowed")
        buttons = []
        for name in left.split("+"):
            if not name:
                raise IniError(f"line {number}: empty chord component")
            u = name.upper()
            if u not in BUTTONS:
                raise IniError(f"line {number}: unknown controller button")
            if u in buttons:
                raise IniError(f"line {number}: repeated button in chord")
            buttons.append(u)
        parts = right.split("+")
        mods = []
        for name in parts[:-1]:
            u = name.upper()
            if u not in MODIFIERS:
                raise IniError(f"line {number}: unknown key modifier")
            if u in mods:
                raise IniError(f"line {number}: repeated key modifier")
            mods.append(u)
        key = parts[-1].upper()
        if key not in KEY_NAMES:
            raise IniError(f"line {number}: unknown UE4SS key")
        mask = frozenset(buttons)
        for old in rows:
            if old[2] == mask:
                raise IniError(f"line {number}: duplicate controller chord")
            if old[2] <= mask or mask <= old[2]:
                raise IniError(f"line {number}: chord cannot contain another configured chord")
        rows.append((number, left, mask, key, tuple(sorted(mods, key=MODIFIERS.index))))
    if not rows:
        raise IniError("requires at least one binding")
    return rows


# ---------------------------------------------------------------------------
# Generation
# ---------------------------------------------------------------------------

def stable_slot_index(identity: str, ladder_len: int) -> int:
    digest = hashlib.sha256(("FV1|" + identity).encode("ascii")).digest()
    return int.from_bytes(digest[:8], "big") % ladder_len


def collect(mods: list):
    """mods: list of (name, path) in priority order.
    Returns (groups, per_mod, errors) where groups maps key_identity ->
    {key, mods, owner} in first-request order."""
    groups, errors = {}, []
    for name, path in mods:
        try:
            binds = extract_binds(path)
        except OSError as e:
            errors.append(f"{name}: cannot read {path}: {e}")
            continue
        for lineno, key, mod_set in binds:
            if key not in KEY_NAMES:
                errors.append(f"{name} main.lua:{lineno}: unknown Key.{key} "
                              "(not in KeyDef.cpp string_to_key)")
                continue
            ident = key_identity(key, mod_set)
            g = groups.setdefault(ident, {"key": key, "mods": mod_set, "wanters": []})
            if name not in g["wanters"]:
                g["wanters"].append(name)
    return groups, errors


def assign(groups: dict, ladder, pinned_keys: set):
    """Deterministic assignment. groups is ordered by first request (priority).
    Keys already served by pinned rows never consume a slot."""
    out, unmapped, shared, notes = [], [], [], []
    taken = set()
    for ident, g in groups.items():
        if (g["key"], g["mods"]) in pinned_keys or g["key"] in pinned_keys:
            shared.append(g)
            continue
        start = stable_slot_index(ident, len(ladder))
        slot = None
        for off in range(len(ladder)):
            cand = (start + off) % len(ladder)
            if cand not in taken:
                slot = cand
                break
        if slot is None:
            unmapped.append(g)
            notes.append(f"pool exhausted: {ident} wanted by {', '.join(g['wanters'])}")
        else:
            taken.add(slot)
            g["slot"] = slot
            out.append(g)
    return out, unmapped, shared, notes


def render(mods, pin_rows, pin_sha, ladder_name, ladder, assigned, unmapped,
           shared, groups_errors, out_path):
    lines = []
    inputs_digest = hashlib.sha256()
    for name, path in mods:
        fh = hashlib.sha256(open(path, "rb").read()).hexdigest()
        inputs_digest.update(f"{name}|{path}|{fh}\n".encode("ascii"))
    a = lines.append
    a("; ue4ss/PS5Pad.ini -- AUTO-GENERATED by keymap-gen.py (agent FV).")
    a("; DO NOT HAND-EDIT: regenerate via keymap-gen.py so the MFA loadlist")
    a("; size pin (file<TAB>config<TAB>ue4ss/PS5Pad.ini<TAB><bytes>) can be")
    a("; recomputed. ASCII only; no trailing comments on data lines; LF; <= 16384 B.")
    a(f"; generator: {VERSION}")
    a(f"; inputs-sha256: {inputs_digest.hexdigest()}")
    if pin_rows is not None:
        a(f"; pinned-source-sha256: {pin_sha}")
    a(f"; ladder: {ladder_name} ({len(ladder)} free slots)")
    a(";")
    a("; Reserved, never auto-assigned (Townfall control-scheme assumptions,")
    a("; see MAPPING.md): OPTIONS=pause; CROSS/CIRCLE/SQUARE/TRIANGLE=game")
    a("; actions; d-pad=game menus; L3+R3=UNIVERSAL MOD-MENU CHORD (= F7).")
    a("; UE4SS keybinds are non-exclusive: every mapped button still reaches")
    a("; the game; mods must compensate (SetIgnoreMoveInput/SetIgnoreLookInput).")
    a("")
    a("[PS5Pad]")
    a("User = -1")
    a("")
    a("[Bindings]")
    if pin_rows:
        a("; -- pinned console-proven rows, preserved verbatim (l23 profile,")
        a(";    md5 5510caf11fb9307931fe26602245205d) -- never regenerate these --")
        for _, left, _, key, m in pin_rows:
            a(f"{left} = {key_identity(key, m)}")
    if assigned:
        a("; -- auto-assigned: slot = ladder[sha256('FV1|key') mod N], linear-probe")
        a(";    suffix on collision; consuming mods listed per row --")
        for g in sorted(assigned, key=lambda g: g["slot"]):
            a(";" + key_identity(g["key"], g["mods"]) + " <- " + ", ".join(g["wanters"]))
            a(f"{ladder[g['slot']]} = {key_identity(g['key'], g['mods'])}")
    if shared:
        a("; -- keys already served by pinned rows above (shared, no new row) --")
        for g in shared:
            a(";" + key_identity(g["key"], g["mods"]) + " <- " + ", ".join(g["wanters"]))
    if unmapped:
        a("; -- WARN: unmapped, free-slot pool exhausted (raise ladder or drop")
        a(";    mods from the enabled set; these binds stay unreachable) --")
        for g in unmapped:
            a(";" + key_identity(g["key"], g["mods"]) + " <- " + ", ".join(g["wanters"]))
    if groups_errors:
        a("; -- WARN: extraction errors --")
        for e in groups_errors:
            a("; " + e)
    a("; -- free slots still available: "
      + (", ".join(ladder[i] for i in range(len(ladder))
                   if i not in {g.get('slot') for g in assigned}) or "none"))
    text = "\n".join(lines) + "\n"
    return text


# ---------------------------------------------------------------------------
# Modes
# ---------------------------------------------------------------------------

def parse_mod_args(pairs):
    mods = []
    for p in pairs:
        if "=" not in p:
            raise SystemExit(f"--mod expects NAME=PATH, got {p!r}")
        name, path = p.split("=", 1)
        mods.append((name, path))
    return mods


def cmd_generate(args):
    mods = parse_mod_args(args.mod)
    if not mods:
        raise SystemExit("generate: at least one --mod NAME=PATH required")
    ladder = LADDERS[args.ladder]
    pin_rows, pin_sha, pinned_keys = None, None, set()
    if args.pin_pad:
        pin_text = open(args.pin_pad, "r", encoding="ascii").read()
        pin_sha = hashlib.sha256(pin_text.encode("ascii")).hexdigest()
        pin_rows = parse_pad_ini(pin_text)  # parser-verified pin source
        for _, _, _, key, m in pin_rows:
            pinned_keys.add(key)
            pinned_keys.add((key, m))
    groups, errors = collect(mods)
    assigned, unmapped, shared, notes = assign(groups, ladder, pinned_keys)
    text = render(mods, pin_rows, pin_sha, args.ladder, ladder,
                  assigned, unmapped, shared, errors, args.out)
    # Self-validation: generated text must parse under the retained rules and
    # must not auto-assign reserved buttons.
    rows = parse_pad_ini(text)
    for _, left, mask, _, _ in rows:
        if pin_rows and left in {r[1] for r in pin_rows}:
            continue
        for b in mask:
            if b in RESERVED_BUTTONS:
                raise SystemExit(f"internal error: reserved button {b} auto-assigned")
    data = text.encode("ascii", "strict")
    if len(data) > PS5PAD_CONFIG_LIMIT:
        raise SystemExit(f"internal error: output {len(data)} B exceeds 16384 B")
    if args.out:
        with open(args.out, "wb") as f:
            f.write(data)
    if not args.quiet:
        sys.stderr.write(
            f"wrote {args.out or '<stdout>'}: {len(data)} bytes, "
            f"{len(rows)} bindings "
            f"({len(pin_rows or [])} pinned + {len(assigned)} auto, "
            f"{len(shared)} shared-key mods, {len(unmapped)} unmapped, "
            f"{len(errors)} extraction errors)\n")
        if len(data) != 3718:
            sys.stderr.write(
                "NOTE: size differs from the current pinned 3718 B loadlist"
                " row -- wc -c this file and re-pin "
                "file\\tconfig\\tue4ss/PS5Pad.ini\\t<bytes> before upload.\n")
    warns = len(unmapped) + len(errors)
    if warns and args.strict:
        return 1
    return 0


def cmd_check(args):
    if not args.mod:
        raise SystemExit("check: at least one --mod NAME=PATH required")
    mods = parse_mod_args(args.mod)
    problems, warnings = [], []
    text = open(args.check, "r", encoding="ascii", errors="replace").read()
    try:
        rows = parse_pad_ini(text)
    except IniError as e:
        print(f"VIOLATION parser: {args.check}: {e}")
        return 2
    data = text.encode("utf-8", "replace")
    print(f"parsed {args.check}: {len(rows)} bindings, {len(data)} bytes "
          f"(limit {PS5PAD_CONFIG_LIMIT})")
    if args.expect_size is not None and len(data) != args.expect_size:
        problems.append(f"size {len(data)} != pinned loadlist {args.expect_size} "
                        "(MFA admission would fail with mfa_activate_failed code=-8)")
    pinned_left = set()
    if args.pin_pad:
        pin_text = open(args.pin_pad, "r", encoding="ascii").read()
        try:
            for _, left, _, key, m in parse_pad_ini(pin_text):
                pinned_left.add(left)
        except IniError as e:
            problems.append(f"pin source unreadable: {e}")
    groups, errors = collect(mods)
    warnings.extend(errors)
    served = {}
    for _, left, mask, key, m in rows:
        served[(key, m)] = served.get((key, m), []) + [left]
    # unmapped mod keys
    for ident, g in groups.items():
        if (g["key"], g["mods"]) not in served and g["key"] not in served:
            warnings.append(f"unmapped: {ident} wanted by {', '.join(g['wanters'])}")
    # orphan rows / reserved buttons on non-pinned rows
    wanted = {g["key"] for g in groups.values()}
    for _, left, mask, key, m in rows:
        if key not in wanted and left not in pinned_left:
            warnings.append(f"orphan row {left} = {key_identity(key, m)} "
                            "(no enabled mod wants this key)")
        if left not in pinned_left:
            for b in mask:
                if b in RESERVED_BUTTONS:
                    problems.append(f"reserved button {b} used by non-pinned row "
                                    f"{left} = {key_identity(key, m)}")
    # shared-key co-fire disclosure
    for ident, g in groups.items():
        if len(g["wanters"]) > 1:
            via = served.get((g["key"], g["mods"]), served.get(g["key"], []))
            print(f"shared: {ident} fires for {', '.join(g['wanters'])}"
                  + (f" via {', '.join(via)}" if via else ""))
    for w in warnings:
        print("WARN " + w)
    for p in problems:
        print("VIOLATION " + p)
    print(f"check: {len(problems)} violations, {len(warnings)} warnings")
    if problems:
        return 2
    return 1 if (warnings and args.strict) else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mod", action="append", default=[], metavar="NAME=PATH",
                    help="mod name and main.lua path; repeatable; ORDER = priority")
    ap.add_argument("--pin-pad", metavar="INI",
                    help="pinned console-proven PS5Pad.ini carried verbatim")
    ap.add_argument("--ladder", choices=sorted(LADDERS), default="ergonomic",
                    help="free-slot ladder (default ergonomic=7; pairs10=10 max)")
    ap.add_argument("--check", metavar="INI", help="validate INI against --mod set")
    ap.add_argument("--expect-size", type=int, default=None,
                    help="loadlist-pinned byte count to verify in --check")
    ap.add_argument("--out", default=None, help="output path (default stdout)")
    ap.add_argument("--strict", action="store_true",
                    help="exit 1 on warnings (unmapped keys, orphans)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    if not args.quiet:
        sys.stderr.write(VERSION + "\n")
    if args.check:
        sys.exit(cmd_check(args))
    sys.exit(cmd_generate(args))


if __name__ == "__main__":
    main()
