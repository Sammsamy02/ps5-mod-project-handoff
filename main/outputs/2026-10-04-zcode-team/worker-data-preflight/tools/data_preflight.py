#!/usr/bin/env python3
"""
data_preflight.py -- local staging preflight for the PS5 UE4SS mod tree.

Verifies every file a modforge loadlist declares against the runtime's own
checks, re-implemented 1:1 from the runtime SOURCE (see SOURCE PINS below).
Both live launch-10 data-gate failures were staging-side preventable:

  run1: ue4ss/PS5Pad.ini contained two UTF-8 em-dashes (U+2014) ->
        "PS5Pad.ini must be ASCII text without control bytes"
        (runtime_start_failed code=3, run1-ue4ss.log)
  run2: loadlist recorded 2960 bytes for ue4ss/PS5Pad.ini, sanitized file was
        2958 -> "size file is shorter than the load list says
        ue4ss/PS5Pad.ini" (mfa_activate_failed code=-8, run2-ue4ss.log)

This tool catches both classes (and every other loadlist-declared invariant)
BEFORE a console launch. Read-only by default. --fix writes sanitized copies
plus a diff to a separate output directory; it never modifies the input tree.

Modes:
  (default)          verify only
  --fix --fix-out D  write fixed copies (ASCII-folded PS5Pad.ini, size-synced
                     loadlist) + FIXES-DIFF.txt under D, then re-verify them
  --selftest         build fixture trees in a temp dir and check every
                     failure class + the fixer round-trip + the two real
                     launch-10 incidents from the evidence copies
  --json FILE        also write machine-readable findings

Exit codes: 0 clean (no errors; warnings reported), 1 findings, 2 usage/IO.

SOURCE PINS (all under .superpowers/native-dependency-spike/):
  [MFA]  native-coherent-runtime-1/source/activation/modforge_activation.c
         (byte-identical copies: claude-takeover-43a96969/source/runtime/,
          native-coherent-runtime-1/source-reconstruction-check/,
          native-coherent-runtime-1/source-reconstruction-final-4/,
          native-coherent-runtime-1/source-generations/composition-1/ --
         all md5 9ce907127c6de918a500757b0289ba66)
  [LL]   native-coherent-runtime-1/source/activation/modforge_loadlist.h
         (byte-identical to the vendored copy in
          outputs/2026-10-04-zcode-team/worker-f-r2-fix/composition/vendored/)
  [AMS]  native-retained-cppmods-1/consumers-final/source/bootstrap/activated_mod_source.cpp
  [NMS]  ue4ss-unreal-native-build-1/port/ue4ss/src/Platform/NativeModSource.cpp
  [PAD]  ue4ss-native-pad-config-1/src/PS5PadConfig.cpp (+ PS5PadConfig.hpp:7)
  [KEY]  ue4ss-unreal-native-build-1/port/first/src/Input/KeyDef.cpp
         (identical to ue4ss-native-input-port-1/port/first copy, md5
          552d65a87e44bc4cf3d36508e7838705; 167 key names)
"""

import argparse
import difflib
import hashlib
import json
import os
import re
import shutil
import sys
import tempfile

# .../what/outputs/2026-10-04-zcode-team/worker-data-preflight/tools/<this file>
_HERE = os.path.dirname(os.path.abspath(__file__))
WORKSPACE = os.path.abspath(os.path.join(
    _HERE, "..", "..", "..", ".."))  # -> .../what
DEFAULT_TREE = os.path.join(
    WORKSPACE, "outputs", "2026-10-04-zcode-team",
    "worker-settings-v3-package", "package-v3")
DEFAULT_TITLE = "PPSA33286"

# ---------------------------------------------------------------------------
# Runtime constants (from source, pinned)
# ---------------------------------------------------------------------------

# [PAD] PS5PadConfig.hpp:7 -- inline constexpr size_t PS5PadConfigLimit = 16*1024
PAD_CONFIG_LIMIT = 16 * 1024
# [PAD] PS5PadConfig.cpp:91 -- at most 128 bindings
PAD_MAX_BINDINGS = 128
# [PAD] PS5PadConfig.cpp:41-46 -- button name table
PAD_BUTTONS = {
    "L3", "R3", "OPTIONS", "UP", "RIGHT", "DOWN", "LEFT", "L2", "R2", "L1",
    "R1", "TRIANGLE", "CIRCLE", "CROSS", "SQUARE", "TOUCH_PAD",
}
# [PAD] PS5PadConfig.cpp:109 -- key modifiers
PAD_MODIFIERS = {"SHIFT", "CONTROL", "ALT"}
# [KEY] KeyDef.cpp:12-176 -- lowercase_string_to_key names (167 entries);
# string_to_key lowercases its input [KEY :193].
UE4SS_KEYS = frozenset("""
a add apps attn b backspace browser_back browser_favorites browser_forward
browser_home browser_refresh browser_search browser_stop c cancel caps_lock
clear crsel d decimal del divide down_arrow e eight end ereof escape execute
exsel f f1 f10 f11 f12 f13 f14 f15 f16 f17 f18 f19 f2 f20 f21 f22 f23 f24 f3
f4 f5 f6 f7 f8 f9 five four g h help home i ime_accept ime_convert ime_final
ime_hanguel ime_hangul ime_hanja ime_junja ime_kana ime_kanji ime_modechange
ime_nonconvert ime_off ime_on ime_process ins j k l launch_app1 launch_app2
launch_mail launch_media_select left_arrow left_mouse_button left_win m
media_next_track media_play_pause media_prev_track media_stop
middle_mouse_button multiply n nine noname num_eight num_five num_four
num_lock num_nine num_one num_seven num_six num_three num_two num_zero o
oem_102 oem_clear oem_comma oem_eight oem_five oem_four oem_minus oem_one
oem_period oem_plus oem_seven oem_six oem_three oem_two one p pa1 packet
page_down page_up pause play print print_screen q r reserved_start_of_enum
return right_arrow right_mouse_button right_win s scroll_lock select separator
seven six sleep space subtract t tab three two u up_arrow v volume_down
volume_mute volume_up w x xbutton_one xbutton_two y z zero zoom
""".split())

# [MFA] modforge_activation.c:337-340 -- mfa_limits defaults
MFA_MAX_FILES = 4096
MFA_MAX_FILE_BYTES = 8 << 20          # 8 MiB per preloaded file
MFA_MAX_TOTAL_BYTES = 64 << 20        # 64 MiB preloaded total
MFA_MAX_LOADLIST_BYTES = 1 << 20      # 1 MiB loadlist
MFA_PATH_MAX = 1024                   # [MFA] :21; rel check :371 is n+2 > 1024
# [MFA] :281 -- default preloaded roles; [AMS] :54-55 additionally allows
# native_plugin for ue4ss/ paths.
PRELOAD_ROLES = {"lua_mod", "config", "script"}
UE4SS_ROLES = {"lua_mod", "config", "script", "native_plugin"}


# ---------------------------------------------------------------------------
# Findings
# ---------------------------------------------------------------------------

class Preflight:
    def __init__(self):
        self.findings = []
        self.file_hashes = {}

    @property
    def errors(self):
        return [f for f in self.findings if f["severity"] == "error"]

    @property
    def warnings(self):
        return [f for f in self.findings if f["severity"] == "warn"]

    def add(self, fid, severity, path, detail, source, fixable=False):
        self.findings.append({
            "id": fid, "severity": severity, "path": path,
            "detail": detail, "source": source, "fixable": fixable,
        })


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


# ---------------------------------------------------------------------------
# [LL] modforge_loadlist.h mirror
# ---------------------------------------------------------------------------

def mf_utf8_ok(s):
    """[LL] mf_utf8_ok :82-103 -- strict UTF-8, no overlongs/surrogates."""
    i, n = 0, len(s)
    while i < n:
        c = s[i]
        if c < 0x80:
            i += 1
            continue
        lo, hi = 0x80, 0xBF
        if 0xC2 <= c <= 0xDF:
            need = 1
        elif c == 0xE0:
            need, lo = 2, 0xA0
        elif c == 0xED:
            need, hi = 2, 0x9F
        elif 0xE1 <= c <= 0xEF:
            need = 2
        elif c == 0xF0:
            need, lo = 3, 0x90
        elif 0xF1 <= c <= 0xF3:
            need = 3
        elif c == 0xF4:
            need, hi = 3, 0x8F
        else:
            return False
        if n - i - 1 < need:
            return False
        if not (lo <= s[i + 1] <= hi):
            return False
        for k in range(2, need + 1):
            if not (0x80 <= s[i + k] <= 0xBF):
                return False
        i += need + 1
    return True


def mf_path_ok(s):
    """[LL] mf_path_ok :109-125 -- relative, '/', no backslash/control/./.."""
    if len(s) == 0 or s[0:1] == b"/":
        return False
    seg = 0
    for i in range(len(s) + 1):
        if i == len(s) or s[i:i + 1] == b"/":
            ln = i - seg
            if ln == 0:
                return False
            part = s[seg:i]
            if part == b"." or part == b"..":
                return False
            seg = i + 1
        else:
            c = s[i]
            if c == 0x5C or c < 0x20 or c == 0x7F:  # backslash or control
                return False
    return True


def mf_parse_u64(s):
    """[LL] mf_parse_u64 :127-140 -- digits only, <=20 chars, overflow-checked."""
    if len(s) == 0 or len(s) > 20:
        return None
    v = 0
    for ch in s:
        if not (0x30 <= ch <= 0x39):
            return None
        d = ch - 0x30
        if v > (0xFFFFFFFFFFFFFFFF - d) // 10:
            return None
        v = v * 10 + d
    return v


class Record:
    __slots__ = ("kind", "line", "fields", "size")

    def __init__(self, kind, line, fields, size=0):
        self.kind, self.line, self.fields, self.size = kind, line, fields, size


def parse_loadlist(data, pf):
    """Mirror of [LL] mf_next :144-206. Stops at the first malformed line,
    exactly like the runtime (mfa pass 1 fails with MFA_E_LOADLIST)."""
    records = []
    lines = data.split(b"\n")
    if lines and lines[-1] == b"":
        lines.pop()                     # trailing newline is not an extra line
    seen_header = False
    for lineno, raw in enumerate(lines, 1):
        line = raw[:-1] if raw.endswith(b"\r") else raw   # [LL] :153 CRLF ok
        if not mf_utf8_ok(line):                          # [LL] :154
            pf.add("LOADLIST_PARSE", "error", "loadlist.txt",
                   "line %d: malformed loadlist.txt (invalid UTF-8) -- runtime "
                   "fails with MFA_E_LOADLIST" % lineno,
                   "[LL] modforge_loadlist.h:154 + [MFA] modforge_activation.c:395-399")
            return records
        if len(line) == 0 or line[0:1] == b"#":           # [LL] :155
            continue
        parts = line.split(b"\t")                          # [LL] :157-168
        if len(parts) > 5:                                 # [LL] :162
            pf.add("LOADLIST_PARSE", "error", "loadlist.txt",
                   "line %d: malformed loadlist.txt (too many fields, max 5) -- "
                   "runtime fails with MFA_E_LOADLIST" % lineno,
                   "[LL] modforge_loadlist.h:162 + [MFA] :395-399")
            return records
        if not seen_header:                                # [LL] :170-175
            if len(parts) != 2 or parts[0] != b"modforge-loadlist" or parts[1] != b"1":
                pf.add("LOADLIST_PARSE", "error", "loadlist.txt",
                       "line %d: malformed loadlist.txt (header must be exactly "
                       "'modforge-loadlist<TAB>1') -- runtime fails with "
                       "MFA_E_LOADLIST" % lineno,
                       "[LL] modforge_loadlist.h:170-175 + [MFA] :395-399")
                return records
            seen_header = True
            continue
        kind_name = parts[0]
        fields = parts[1:]
        size = 0
        table = ((b"title", "title", 1), (b"engine", "engine", 1),
                 (b"sdk", "sdk", 1), (b"framework", "framework", 1),
                 (b"mod", "mod", 2), (b"order", "order", 1),
                 (b"file", "file", 3))
        kind = "unknown"
        want = len(fields)
        for name_lit, k, w in table:                       # [LL] :190-202
            if kind_name == name_lit:
                kind, want = k, w
                break
        if kind != "unknown" and len(fields) != want:
            pf.add("LOADLIST_PARSE", "error", "loadlist.txt",
                   "line %d: malformed loadlist.txt ('%s' row needs exactly %d "
                   "field(s) after the kind, got %d) -- runtime fails with "
                   "MFA_E_LOADLIST" % (lineno, kind_name.decode("latin-1"),
                                       want, len(fields)),
                   "[LL] modforge_loadlist.h:190-202 + [MFA] :395-399")
            return records
        if kind == "file":                                 # [LL] :196-199
            if not mf_path_ok(fields[1]):
                pf.add("LOADLIST_PARSE", "error", "loadlist.txt",
                       "line %d: malformed loadlist.txt (file path %r is not a "
                       "valid relative key: must be non-empty, relative, '/', no "
                       "backslash/control byte, no empty '.'/'..' segment) -- "
                       "runtime fails with MFA_E_LOADLIST"
                       % (lineno, fields[1].decode("latin-1")),
                       "[LL] modforge_loadlist.h:198,109-125 + [MFA] :395-399")
                return records
            sz = mf_parse_u64(fields[2])
            if sz is None:
                pf.add("LOADLIST_PARSE", "error", "loadlist.txt",
                       "line %d: malformed loadlist.txt (file size %r is not a "
                       "decimal uint64) -- runtime fails with MFA_E_LOADLIST"
                       % (lineno, fields[2].decode("latin-1")),
                       "[LL] modforge_loadlist.h:198,127-140 + [MFA] :395-399")
                return records
            size = sz
        records.append(Record(kind, lineno, fields, size))
    if not seen_header:                                    # [LL] :205
        pf.add("LOADLIST_PARSE", "error", "loadlist.txt",
               "empty or headerless loadlist.txt -- runtime fails with "
               "MFA_E_LOADLIST (empty input is an error)",
               "[LL] modforge_loadlist.h:205 + [MFA] :395-399")
    return records


# ---------------------------------------------------------------------------
# ASCII fold for --fix (PS5Pad.ini only)
# ---------------------------------------------------------------------------

FOLD_MAP = {
    b"\xe2\x80\x94": b"--",   # U+2014 em dash   (the exact run1 bytes)
    b"\xe2\x80\x93": b"-",    # U+2013 en dash
    b"\xe2\x80\x98": b"'",    # U+2018 left single quote
    b"\xe2\x80\x99": b"'",    # U+2019 right single quote
    b"\xe2\x80\x9c": b'"',    # U+201C left double quote
    b"\xe2\x80\x9d": b'"',    # U+201D right double quote
    b"\xe2\x80\xa6": b"...",  # U+2026 ellipsis
    b"\xc2\xa0": b" ",        # U+00A0 no-break space
}


def ascii_fold_pad(data):
    """Fold bytes the [PAD] :56-58 gate rejects into printable ASCII.

    Em dash -> '--' reproduces the real launch-10 sanitization byte-for-byte.
    Returns (folded_bytes, [descriptions of replacements]).
    """
    out = bytearray()
    notes = []
    i, n = 0, len(data)
    while i < n:
        b = data[i]
        if b <= 0x7E and (b >= 0x20 or b in (0x0A, 0x0D, 0x09)):
            out.append(b)
            i += 1
            continue
        if b >= 0xC2:
            for seq, rep in FOLD_MAP.items():
                if data[i:i + len(seq)] == seq:
                    out += rep
                    notes.append("offset %d: UTF-8 %s -> %r"
                                 % (i, seq.hex(), rep.decode()))
                    i += len(seq)
                    break
            else:
                if 0xC2 <= b <= 0xDF:
                    ln = 2
                elif 0xE0 <= b <= 0xEF:
                    ln = 3
                elif 0xF0 <= b <= 0xF4:
                    ln = 4
                else:
                    ln = 1
                seq = data[i:i + ln]
                ok = len(seq) == ln and all(0x80 <= c <= 0xBF for c in seq[1:])
                out.append(0x3F)
                notes.append("offset %d: UTF-8 %s -> '?'"
                             % (i, seq.hex() if ok else bytes([b]).hex()))
                i += ln if ok else 1
            continue
        out.append(0x3F)
        if b < 0x20:
            notes.append("offset %d: control byte 0x%02x -> '?'" % (i, b))
        else:
            notes.append("offset %d: byte 0x%02x (>=0x7f) -> '?'" % (i, b))
        i += 1
    return bytes(out), notes


# ---------------------------------------------------------------------------
# [PAD] PS5PadConfig.cpp parse_ps5_pad_config :52-124
# ---------------------------------------------------------------------------

def _pad_trim(line):
    return line.strip(b" \t\r")            # [PAD] :10-15


def check_pad_config(data, rel, pf):
    """Full mirror of [PAD] parse_ps5_pad_config. The runtime throws at the
    first violation; this checker records the first ASCII/size violation and
    keeps collecting structural errors (superset, exit-code identical)."""
    src = "[PAD] PS5PadConfig.cpp"
    syntax_errors = 0

    def err(detail, pin):
        nonlocal syntax_errors
        syntax_errors += 1
        pf.add("PAD_SYNTAX", "error", rel, detail + " -- runtime throws",
               src + ":" + pin)

    if len(data) == 0 or len(data) > PAD_CONFIG_LIMIT:                     # :54-55
        pf.add("PAD_SIZE_LIMIT", "error", rel,
               "PS5Pad.ini must contain 1..%d bytes (got %d) -- runtime throws "
               "'PS5Pad.ini must contain 1..16384 bytes'"
               % (PAD_CONFIG_LIMIT, len(data)),
               src + ":54-55 + PS5PadConfig.hpp:7")
        return
    for i, b in enumerate(data):                                           # :56-58
        if b > 0x7E or (b < 0x20 and b not in (0x0A, 0x0D, 0x09)):
            pf.add("PAD_ASCII", "error", rel,
                   "byte 0x%02x at offset %d -- runtime throws 'PS5Pad.ini must "
                   "be ASCII text without control bytes' (the launch-10 run1 "
                   "gate: em-dash UTF-8 E2 80 94 trips this)" % (b, i),
                   src + ":56-58", fixable=True)
            return  # the runtime throws here before any parsing
    section = None                                     # None | 'pad' | 'bindings'
    pad_seen = bindings_seen = user_seen = False                           # :60
    bindings = []                                                          # :61
    lines = data.split(b"\n")
    if lines and lines[-1] == b"":
        lines.pop()
    for lineno0, raw in enumerate(lines, 1):
        line = _pad_trim(raw)
        if len(line) == 0 or line[0:1] in (b"#", b";"):                    # :69
            continue
        if line[0:1] == b"[":                                              # :70-75
            up = line.upper()
            if up == b"[PS5PAD]" and not pad_seen:
                pad_seen, section = True, "pad"
                continue
            if up == b"[BINDINGS]" and not bindings_seen:
                bindings_seen, section = True, "bindings"
                continue
            err("line %d: unknown or repeated section" % lineno0, "74")
            continue
        if line.count(b"=") != 1:                                          # :77-78
            err("line %d: expected exactly one '='" % lineno0, "77-78")
            continue
        eq = line.index(b"=")
        left = _pad_trim(line[:eq])
        right = _pad_trim(line[eq + 1:])
        if len(left) == 0 or len(right) == 0:                              # :80
            err("line %d: empty name or value" % lineno0, "80")
            continue
        if section == "pad":                                               # :81-89
            if left.upper() != b"USER" or user_seen:                       # :83
                err("line %d: unknown or repeated PS5Pad option" % lineno0, "83")
                continue
            user_seen = True
            ok = re.fullmatch(rb"-?[0-9]+", right) is not None
            if ok:
                val = int(right)
                ok = -2**31 <= val < 2**31 and val >= -1                   # :86-87
            if not ok:
                err("line %d: User must be -1 or a nonnegative signed integer"
                    % lineno0, "86-87")
            continue
        if section != "bindings":                                          # :90
            err("line %d: binding requires [Bindings] section" % lineno0, "90")
            continue
        if len(bindings) == PAD_MAX_BINDINGS:                              # :91
            err("line %d: at most 128 bindings are allowed" % lineno0, "91")
            continue
        buttons = 0
        bad = False
        for comp in left.split(b"+"):                                      # :93-98
            name = comp.upper()
            if len(name) == 0:                                             # :32
                err("line %d: empty chord/key component" % lineno0, "32")
                bad = True
                break
            if name.decode("latin-1") not in PAD_BUTTONS:                  # :95
                err("line %d: unknown controller button %r"
                    % (lineno0, comp.decode("latin-1")), "95")
                bad = True
                break
            bit = 1 << sorted(PAD_BUTTONS).index(name.decode("latin-1"))
            if buttons & bit:                                              # :96
                err("line %d: repeated button in chord" % lineno0, "96")
                bad = True
                break
            buttons |= bit
        if bad:
            continue
        comps = right.split(b"+")                                          # :99-114
        key_name = comps[-1].upper()
        key_lcase = key_name.lower().decode("latin-1")
        mods = set()
        bad = False
        for comp in comps[:-1]:
            m = comp.upper().decode("latin-1")
            if m not in PAD_MODIFIERS:                                     # :110
                err("line %d: unknown key modifier %r"
                    % (lineno0, comp.decode("latin-1")), "110")
                bad = True
                break
            if m in mods:                                                  # :112
                err("line %d: repeated key modifier" % lineno0, "112")
                bad = True
                break
            mods.add(m)
        if bad:
            continue
        if key_lcase not in UE4SS_KEYS:                                    # :105
            err("line %d: unknown UE4SS key %r ('%s' not in the string_to_key "
                "table)" % (lineno0, key_name.decode("latin-1"), key_lcase),
                "105 + [KEY] KeyDef.cpp:12-176")
            continue
        if key_lcase == "reserved_start_of_enum":                          # :106
            err("line %d: reserved key is not bindable" % lineno0, "106")
            continue
        for old_btn, _k in bindings:                                       # :115-120
            if old_btn == buttons:                                         # :117
                err("line %d: duplicate controller chord" % lineno0, "117")
                bad = True
                break
            if (old_btn & buttons) == old_btn or \
                    (old_btn & buttons) == buttons:                        # :118-119
                err("line %d: controller chord cannot contain another "
                    "configured chord" % lineno0, "118-119")
                bad = True
                break
        if bad:
            continue
        bindings.append((buttons, key_name))
    if len(bindings) == 0 and syntax_errors == 0:                          # :123
        pf.add("PAD_SYNTAX", "error", rel,
               "PS5Pad.ini requires at least one binding -- runtime throws",
               src + ":123")


# ---------------------------------------------------------------------------
# [NMS] NativeModSource.cpp validate_path :12-47 (UE4SS key layer)
# ---------------------------------------------------------------------------

def check_ue4ss_key(rel, pf):
    if not rel.startswith("ue4ss/"):
        # [AMS] :38 -- non-ue4ss keys are skipped by the UE4SS layer (still
        # preloaded by [MFA]); not a failure, but never consumed.
        pf.add("KEY_NOT_UE4SS", "warn", rel,
               "path is outside ue4ss/ -- preloaded by mfa but skipped by the "
               "UE4SS mod source", "[AMS] activated_mod_source.cpp:38")
        return
    extra = ""
    if len(rel) > 4096:                                   # [NMS] :14
        extra = "key longer than 4096"
    elif rel.endswith("/"):                               # [NMS] :14
        extra = "key ends with '/'"
    else:
        for ch in rel:
            if ord(ch) < 0x20 or ord(ch) == 0x7F or ch == "\\" or ch == ":":  # :16-18
                extra = "key contains invalid character %r" % ch
                break
    if not extra:
        for part in rel.split("/"):                       # [NMS] :37-46
            if part in ("", ".", ".."):
                extra = "key is not canonical (empty/'.'/'..' segment)"
                break
    if extra:
        pf.add("PATH_KEY_INVALID", "error", rel,
               extra + " -- runtime throws 'Native mod file requires a relative "
               "key below ue4ss/' / '...invalid character' / '...not canonical'",
               "[NMS] NativeModSource.cpp:12-47")


# ---------------------------------------------------------------------------
# mods.txt rules ([NMS] :157-177)
# ---------------------------------------------------------------------------

def check_mods_txt(data, rel, pf):
    text = data
    if text.startswith(b"\xef\xbb\xbf"):                  # [NMS] :160
        text = text[3:]
    if b"\x00" in text:                                   # [NMS] :161
        pf.add("MODS_TXT_FORMAT", "error", rel,
               "Native mods.txt contains NUL -- runtime throws",
               "[NMS] NativeModSource.cpp:161")
        return
    for raw in text.split(b"\n"):
        line = raw[:-1] if raw.endswith(b"\r") else raw
        if b";" in line:                                  # [NMS] :168 (skipped)
            continue
        line = line.replace(b" ", b"").replace(b"\t", b"")  # [NMS] :169
        if len(line) == 0:
            continue
        colon = line.find(b":")
        if (colon == -1 or colon == 0 or colon + 2 != len(line)
                or line[-1:] not in (b"0", b"1")):        # [NMS] :172-174
            pf.add("MODS_TXT_FORMAT", "error", rel,
                   "malformed enabled row %r -- runtime throws 'Native mods.txt "
                   "contains a malformed enabled row'" % line.decode("latin-1"),
                   "[NMS] NativeModSource.cpp:172-174")
            return


# ---------------------------------------------------------------------------
# Main verification
# ---------------------------------------------------------------------------

def verify(tree, loadlist_path, expected_title=DEFAULT_TITLE):
    pf = Preflight()
    tree = os.path.abspath(tree)

    # [MFA] :343 read_loadlist / :221-224 presence / :236,:264-267 limit
    if not os.path.isfile(loadlist_path):
        pf.add("LOADLIST_MISSING", "error", "loadlist.txt",
               "no loadlist.txt (nothing installed) -- runtime fails with "
               "MFA_E_NO_LOADLIST (-2)", "[MFA] modforge_activation.c:221-224")
        return pf, []
    if os.path.getsize(loadlist_path) > MFA_MAX_LOADLIST_BYTES:
        pf.add("LOADLIST_LIMIT", "error", "loadlist.txt",
               "loadlist.txt is too large (> %d bytes) -- runtime fails with "
               "MFA_E_LIMIT (-5)" % MFA_MAX_LOADLIST_BYTES,
               "[MFA] modforge_activation.c:236-237,264-267")
        return pf, []
    with open(loadlist_path, "rb") as fh:
        data = fh.read()

    records = parse_loadlist(data, pf)

    # [MFA] pass 1 :346-406 -- field control, per-file and total limits
    file_rows = []
    n_title = n_engine = n_sdk = 0
    preload_total = 0
    for rec in records:
        if rec.kind == "unknown":
            continue                                        # [MFA] :349
        checked = False
        for f in rec.fields:                                # [MFA] :350-355
            for ch in f:
                if ch < 0x20 or ch == 0x7F:
                    pf.add("LOADLIST_FIELD_CONTROL", "error", "loadlist.txt",
                           "line %d: control character 0x%02x in a field -- "
                           "runtime fails with MFA_E_LOADLIST ('control character "
                           "in a field')" % (rec.line, ch),
                           "[MFA] modforge_activation.c:137-144,350-355")
                    checked = True
                    break
            if checked:
                break
        if rec.kind == "title":
            n_title += 1
        elif rec.kind == "engine":
            n_engine += 1
        elif rec.kind == "sdk":
            n_sdk += 1
        elif rec.kind == "file":
            if len(file_rows) + 1 > MFA_MAX_FILES:          # [MFA] :366-370
                pf.add("LOADLIST_LIMIT", "error", "loadlist.txt",
                       "too many files (>%d) -- runtime fails with MFA_E_LIMIT"
                       % MFA_MAX_FILES, "[MFA] modforge_activation.c:366-370")
            role, rel_b, size = rec.fields[0], rec.fields[1], rec.size
            rel = rel_b.decode("latin-1")
            if len(rel_b) + 2 > MFA_PATH_MAX:               # [MFA] :371-375
                pf.add("LOADLIST_LIMIT", "error", rel,
                       "path too long (>%d bytes) -- runtime fails with "
                       "MFA_E_LIMIT" % (MFA_PATH_MAX - 2),
                       "[MFA] modforge_activation.c:371-375")
            if role.decode("latin-1") in PRELOAD_ROLES:     # [MFA] :376-389
                if size > MFA_MAX_FILE_BYTES:
                    pf.add("LOADLIST_LIMIT", "error", rel,
                           "file too large to preload (%d > %d) -- runtime fails "
                           "with MFA_E_LIMIT" % (size, MFA_MAX_FILE_BYTES),
                           "[MFA] modforge_activation.c:377-382")
                preload_total += size + 1
                if preload_total > MFA_MAX_TOTAL_BYTES:
                    pf.add("LOADLIST_LIMIT", "error", rel,
                           "preloaded files exceed the total limit (%d > %d) -- "
                           "runtime fails with MFA_E_LIMIT"
                           % (preload_total, MFA_MAX_TOTAL_BYTES),
                           "[MFA] modforge_activation.c:383-389")
            file_rows.append((rec.line, role.decode("latin-1"), rel, size))

    if n_title != 1 or n_engine != 1 or n_sdk > 1:          # [MFA] :401-406
        pf.add("LOADLIST_COUNTS", "error", "loadlist.txt",
               "loadlist.txt needs exactly one title and one engine row, at most "
               "one sdk (got title=%d engine=%d sdk=%d) -- runtime fails with "
               "MFA_E_LOADLIST" % (n_title, n_engine, n_sdk),
               "[MFA] modforge_activation.c:401-406")

    title_rows = [r.fields[0].decode("latin-1") for r in records if r.kind == "title"]
    if title_rows and title_rows[0] != expected_title:      # [MFA] :454-459
        pf.add("TITLE_MISMATCH", "error", "loadlist.txt",
               "load list is for a different game (%r != %r) -- runtime fails "
               "with MFA_E_TITLE (-4)" % (title_rows[0], expected_title),
               "[MFA] modforge_activation.c:454-459")

    engine_rows = [r.fields[0].decode("latin-1") for r in records if r.kind == "engine"]
    fw_rows = [r.fields[0].decode("latin-1") for r in records if r.kind == "framework"]
    if engine_rows:                                         # [AMS] :27-29
        if engine_rows[0] != "unreal":
            pf.add("ENGINE_GATE", "error", "loadlist.txt",
                   "activation engine is %r, the UE4SS path requires 'unreal' -- "
                   "runtime throws 'Activation does not request the native UE4SS "
                   "framework'" % engine_rows[0],
                   "[AMS] activated_mod_source.cpp:27-29")
        if "ue4ss-ps5" not in fw_rows:
            pf.add("FRAMEWORK_GATE", "error", "loadlist.txt",
                   "no 'framework<TAB>ue4ss-ps5' row -- runtime throws "
                   "'Activation does not request the native UE4SS framework'",
                   "[AMS] activated_mod_source.cpp:27-29")

    # duplicates: [MFA] :461-472 (all file rows, bytewise)
    seen = {}
    for line, role, rel, size in file_rows:
        if rel in seen:
            pf.add("FILE_DUPLICATE", "error", rel,
                   "path listed twice (lines %d and %d) -- runtime fails with "
                   "MFA_E_DUPLICATE (-12)" % (seen[rel], line),
                   "[MFA] modforge_activation.c:461-472")
        else:
            seen[rel] = line

    # per-file checks
    for line, role, rel, size in file_rows:
        if role not in UE4SS_ROLES and rel.startswith("ue4ss/"):  # [AMS] :54-55
            pf.add("ROLE_UNSUPPORTED", "error", rel,
                   "unsupported UE4SS activation file role %r -- runtime throws "
                   "'Unsupported UE4SS activation file role'" % role,
                   "[AMS] activated_mod_source.cpp:54-55")
        check_ue4ss_key(rel, pf)
        abs_path = os.path.join(tree, rel)
        if not os.path.isfile(abs_path):                    # [MFA] :170-174
            pf.add("FILE_MISSING", "error", rel,
                   "cannot open listed file (missing on disk) -- runtime fails "
                   "with MFA_E_OPEN (-6)", "[MFA] modforge_activation.c:170-174")
            continue
        actual = os.path.getsize(abs_path)
        if actual < size:                                   # [MFA] :185-189
            pf.add("FILE_SIZE_SHORT", "error", rel,
                   "file is shorter than the load list says (loadlist line %d: "
                   "listed %d, actual %d) -- runtime fails with MFA_E_SIZE (-8); "
                   "this is the exact launch-10 run2 gate" % (line, size, actual),
                   "[MFA] modforge_activation.c:185-189", fixable=True)
        elif actual > size:                                 # [MFA] :200-204
            pf.add("FILE_SIZE_LONG", "error", rel,
                   "file is longer than the load list says (loadlist line %d: "
                   "listed %d, actual %d) -- runtime fails with MFA_E_SIZE (-8)"
                   % (line, size, actual),
                   "[MFA] modforge_activation.c:200-204", fixable=True)
        try:
            pf.file_hashes[rel] = sha256_file(abs_path)
        except OSError:
            pass

        if rel == "ue4ss/PS5Pad.ini":                       # [PAD] pad path gate
            with open(abs_path, "rb") as fh:
                check_pad_config(fh.read(), rel, pf)
        elif rel == "ue4ss/Mods/mods.txt":
            with open(abs_path, "rb") as fh:
                check_mods_txt(fh.read(), rel, pf)

    # [NMS] :98-126 lua mod discovery invariants over the declared tree
    scripts_casings = {}
    for _line, _role, rel, _size in file_rows:
        m = re.match(r"^ue4ss/Mods/([^/]+)/([Ss]cripts)/.+", rel)
        if m:
            scripts_casings.setdefault(m.group(1), set()).add(m.group(2))
    declared_paths = {rel for _l, _r, rel, _s in file_rows}
    for name, casings in sorted(scripts_casings.items()):
        if len(casings) > 1:                                # [NMS] :113-115
            pf.add("MOD_LAYOUT", "error", "ue4ss/Mods/%s" % name,
                   "ambiguous Scripts/scripts directories (%s) -- runtime throws "
                   "'Native mod has ambiguous Scripts/scripts directories'"
                   % ", ".join(sorted(casings)),
                   "[NMS] NativeModSource.cpp:113-115")
        if name.lower() == "shared":                        # [NMS] :117-121
            pf.add("MOD_LAYOUT", "warn", "ue4ss/Mods/%s" % name,
                   "'shared' is never a loadable lua mod (runtime skips it) -- "
                   "its files still preload",
                   "[NMS] NativeModSource.cpp:117-121")
        elif not any(p.startswith("ue4ss/Mods/%s/Scripts/main.lua" % name)
                     or p.startswith("ue4ss/Mods/%s/scripts/main.lua" % name)
                     for p in declared_paths):
            pf.add("MOD_MAIN_LUA_MISSING", "warn", "ue4ss/Mods/%s" % name,
                   "no main.lua row under Scripts/ -- runtime silently does not "
                   "register this mod (it would not start)",
                   "[NMS] NativeModSource.cpp:117-126")

    mod_rows = [r.fields[0].decode("latin-1") for r in records if r.kind == "mod"]
    for name in mod_rows:
        if name.lower() == "shared":                        # [NMS] :117-121
            pf.add("MOD_LAYOUT", "error", "ue4ss/Mods/%s" % name,
                   "mod id 'shared' is reserved -- its directory is never used "
                   "as a lua mod root", "[NMS] NativeModSource.cpp:117-121")

    return pf, records


# ---------------------------------------------------------------------------
# --fix
# ---------------------------------------------------------------------------

def loadlist_sync_sizes(data, tree):
    """Rewrite every 'file' row's size to the on-disk size in `tree`
    (byte-identical otherwise). Returns (new_bytes, [changes])."""
    out_lines = []
    changes = []
    lines = data.split(b"\n")
    for idx, raw in enumerate(lines):
        line = raw
        stripped = raw[:-1] if raw.endswith(b"\r") else raw
        parts = stripped.split(b"\t")
        if idx > 0 and len(parts) == 4 and parts[0] == b"file":
            rel = parts[2].decode("latin-1")
            p = os.path.join(tree, rel)
            if os.path.isfile(p):
                actual = os.path.getsize(p)
                new = b"\t".join([parts[0], parts[1], parts[2],
                                  str(actual).encode("ascii")])
                if stripped.endswith(b"\r"):
                    new += b"\r"
                if new != raw:
                    changes.append("loadlist line %d: size for %s %s -> %d"
                                   % (idx + 1, rel,
                                      parts[3].decode("ascii"), actual))
                line = new
        out_lines.append(line)
    return b"\n".join(out_lines), changes


def unified_diff(a_bytes, b_bytes, a_name, b_name):
    a = a_bytes.decode("utf-8", "replace").splitlines(keepends=True)
    b = b_bytes.decode("utf-8", "replace").splitlines(keepends=True)
    return "".join(difflib.unified_diff(a, b, fromfile=a_name, tofile=b_name))


def run_fix(tree, loadlist_path, fix_out, expected_title=DEFAULT_TITLE):
    """Write fixed copies under fix_out (never touching the input tree).
    Returns (pf_before, fix_log)."""
    pf, records = verify(tree, loadlist_path, expected_title)
    fix_out = os.path.abspath(fix_out)
    if fix_out == os.path.abspath(tree):
        raise SystemExit("--fix-out must differ from the input tree")
    if os.path.exists(fix_out):
        shutil.rmtree(fix_out)
    os.makedirs(fix_out)

    # 1. copy every declared file byte-identical first
    for rec in records:
        if rec.kind != "file":
            continue
        rel = rec.fields[1].decode("latin-1")
        src = os.path.join(tree, rel)
        dst = os.path.join(fix_out, rel)
        if os.path.isfile(src):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)

    fix_log = []
    # 2. ASCII-fold ue4ss/PS5Pad.ini if the pad gate would reject it
    pad_rel = "ue4ss/PS5Pad.ini"
    pad_src = os.path.join(tree, pad_rel)
    if os.path.isfile(pad_src):
        with open(pad_src, "rb") as fh:
            pad = fh.read()
        if any(b > 0x7E or (b < 0x20 and b not in (0x0A, 0x0D, 0x09))
               for b in pad):
            folded, notes = ascii_fold_pad(pad)
            with open(os.path.join(fix_out, pad_rel), "wb") as fh:
                fh.write(folded)
            fix_log.append("folded %s: %s" % (pad_rel, "; ".join(notes)))
            fix_log.append("DIFF %s:\n%s" % (pad_rel, unified_diff(
                pad, folded, pad_rel + " (original)", pad_rel + " (fixed)")))

    # 3. sync the loadlist's file sizes against the FIXED tree
    with open(loadlist_path, "rb") as fh:
        ll = fh.read()
    fixed_ll, changes = loadlist_sync_sizes(ll, fix_out)
    with open(os.path.join(fix_out, "loadlist.txt"), "wb") as fh:
        fh.write(fixed_ll)
    if fixed_ll != ll:
        fix_log.extend(changes)
        fix_log.append("DIFF loadlist.txt:\n%s" % unified_diff(
            ll, fixed_ll, "loadlist.txt (original)", "loadlist.txt (fixed)"))

    with open(os.path.join(fix_out, "FIXES-DIFF.txt"), "w") as fh:
        fh.write("\n".join(fix_log) + "\n")
    return pf, fix_log


# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------

def print_report(pf, tree, loadlist_path, out=sys.stdout):
    print("data_preflight: tree=%s" % tree, file=out)
    print("               loadlist=%s" % loadlist_path, file=out)
    print("               declared files hashed: %d" % len(pf.file_hashes),
          file=out)
    if not pf.findings:
        print("VERDICT: CLEAN (0 errors, 0 warnings)", file=out)
        return
    for f in pf.findings:
        print("[%s] %s %s: %s" % (f["severity"].upper(), f["id"], f["path"],
                                  f["detail"]), file=out)
        print("        source: %s" % f["source"], file=out)
    print("VERDICT: FAILED (%d error(s), %d warning(s))"
          % (len(pf.errors), len(pf.warnings)), file=out)


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

def selftest(out=sys.stdout):
    ws = WORKSPACE
    pkg = os.path.join(ws, "outputs", "2026-10-04-zcode-team",
                       "worker-settings-v3-package", "package-v3")
    ev = os.path.join(ws, "outputs", "2026-10-04-zcode-controller",
                      "launch10-evidence")
    for d in (pkg, ev):
        if not os.path.isdir(d):
            print("FAIL selftest prerequisites missing: %s" % d, file=out)
            return 1
    results = []
    tmp = tempfile.mkdtemp(prefix="data-preflight-selftest-")

    def write(path, data):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as fh:
            fh.write(data)

    def fixture(name):
        d = os.path.join(tmp, name)
        shutil.copytree(pkg, d)
        return d

    def run(name, ok, detail=""):
        results.append((name, ok))
        if not isinstance(detail, str):
            detail = str(detail)
        print("%s %s%s" % ("PASS" if ok else "FAIL", name,
                           (" -> " + detail) if detail else ""), file=out)

    def check(name, tree, expect_ids, absent_ids=(), loadlist=None,
              expect_detail=None):
        ll = loadlist or os.path.join(tree, "loadlist.txt")
        pf, _ = verify(tree, ll)
        got = {f["id"] for f in pf.findings}
        ok = all(e in got for e in expect_ids) and \
            all(a not in got for a in absent_ids)
        if expect_detail:
            ok = ok and any(expect_detail in f["detail"] for f in pf.findings)
        run(name, ok, sorted(got))
        return pf

    def roundtrip(name, tree, expect_ids, allow_post=()):
        ll = os.path.join(tree, "loadlist.txt")
        pf0, _ = verify(tree, ll)
        pre = {f["id"] for f in pf0.findings}
        ok = all(e in pre for e in expect_ids)
        fix_dir = tree + "-fixed"
        run_fix(tree, ll, fix_dir)
        pf1, _ = verify(fix_dir, os.path.join(fix_dir, "loadlist.txt"))
        post = {f["id"] for f in pf1.findings}
        unexpected = post - set(allow_post)
        ok = ok and not unexpected
        run(name, ok, "pre=%s post=%s"
            % (sorted(pre), sorted(post) if post else "CLEAN"))
        return fix_dir

    def sync_ll(tree):
        """Rewrite <tree>/loadlist.txt sizes to the tree's on-disk sizes."""
        with open(os.path.join(tree, "loadlist.txt"), "rb") as fh:
            ll = fh.read()
        new_ll, _ = loadlist_sync_sizes(ll, tree)
        with open(os.path.join(tree, "loadlist.txt"), "wb") as fh:
            fh.write(new_ll)

    with open(os.path.join(pkg, "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"), "rb") as fh:
        lua = fh.read()
    with open(os.path.join(pkg, "ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"), "rb") as fh:
        uh = fh.read()

    good_ll = (b"modforge-loadlist\t1\n"
               b"title\tPPSA33286\n"
               b"engine\tunreal\n"
               b"framework\tue4ss-ps5\n"
               b"mod\tTownfallItemSpawner\t1.0.0\n"
               b"file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/Scripts/main.lua\t"
               + str(len(lua)).encode() + b"\n"
               b"file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/enabled.txt\t0\n"
               b"file\tlua_mod\tue4ss/Mods/shared/UEHelpers/UEHelpers.lua\t"
               + str(len(uh)).encode() + b"\n")

    # T01 clean baseline
    t = fixture("t01-clean")
    write(os.path.join(t, "loadlist.txt"), good_ll)
    write(os.path.join(t, "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"), lua)
    write(os.path.join(t, "ue4ss/Mods/TownfallItemSpawner/enabled.txt"), b"")
    write(os.path.join(t, "ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"), uh)
    check("T01 clean minimal tree passes", t, (),
          ("FILE_MISSING", "LOADLIST_PARSE", "FILE_SIZE_SHORT", "FILE_SIZE_LONG"))

    # T02 real incident 1: package-v3 PS5Pad.ini (em-dash file run1 rejected)
    t = fixture("t02-incident1-emdash")
    check("T02 incident-1 em-dash pad -> PAD_ASCII", t, ("PAD_ASCII",),
          expect_detail="must be ASCII text without control bytes")
    fx = roundtrip("T03 incident-1 fix round-trip (sanitize+resync)", t,
                   ("PAD_ASCII",))
    with open(os.path.join(ev, "PS5Pad-sanitized.ini"), "rb") as fh:
        san = fh.read()
    with open(os.path.join(fx, "ue4ss/PS5Pad.ini"), "rb") as fh:
        got = fh.read()
    run("T04 --fix pad output byte-identical to real PS5Pad-sanitized.ini",
        got == san, "fixed=%d sanitized=%d bytes" % (len(got), len(san)))

    # T05 real incident 2: sanitized pad (2958) + loadlist still saying 2960
    t = fixture("t05-incident2-size")
    shutil.copyfile(os.path.join(ev, "PS5Pad-sanitized.ini"),
                    os.path.join(t, "ue4ss/PS5Pad.ini"))
    check("T05 incident-2 size mismatch -> FILE_SIZE_SHORT", t,
          ("FILE_SIZE_SHORT",),
          expect_detail="file is shorter than the load list says")
    fx = roundtrip("T06 incident-2 fix round-trip (size sync)", t,
                   ("FILE_SIZE_SHORT",))
    with open(os.path.join(fx, "loadlist.txt"), "rb") as fh:
        ll_fixed = fh.read()
    with open(os.path.join(ev, "loadlist-sizefixed.txt"), "rb") as fh:
        ll_expect = fh.read()
    run("T07 --fix loadlist output byte-identical to real loadlist-sizefixed.txt",
        ll_fixed == ll_expect,
        "fixed=%d sizefixed=%d bytes" % (len(ll_fixed), len(ll_expect)))

    # T08 missing declared file
    t = fixture("t08-missing")
    os.remove(os.path.join(t, "ue4ss/MemberVariableLayout.ini"))
    check("T08 missing file -> FILE_MISSING", t, ("FILE_MISSING",),
          expect_detail="MFA_E_OPEN")

    # T09 longer than listed
    t = fixture("t09-longer")
    with open(os.path.join(t, "ue4ss/VTableLayout.ini"), "ab") as fh:
        fh.write(b"xx")
    check("T09 longer file -> FILE_SIZE_LONG", t, ("FILE_SIZE_LONG",),
          expect_detail="file is longer than the load list says")
    roundtrip("T10 longer-file fix round-trip", t, ("FILE_SIZE_LONG",))

    # T11 duplicate path
    t = fixture("t11-duplicate")
    write(os.path.join(t, "loadlist.txt"),
          good_ll + good_ll.splitlines()[-1] + b"\n")
    check("T11 duplicate path -> FILE_DUPLICATE", t, ("FILE_DUPLICATE",),
          expect_detail="MFA_E_DUPLICATE")

    # T12 bad header
    t = fixture("t12-header")
    write(os.path.join(t, "loadlist.txt"),
          good_ll.replace(b"modforge-loadlist\t1", b"modforge-loadlist\t2"))
    check("T12 bad header -> LOADLIST_PARSE", t, ("LOADLIST_PARSE",))

    # T13 two titles
    t = fixture("t13-two-titles")
    write(os.path.join(t, "loadlist.txt"), good_ll + b"title\tPPSA00000\n")
    check("T13 two title rows -> LOADLIST_COUNTS", t, ("LOADLIST_COUNTS",),
          expect_detail="MFA_E_LOADLIST")

    # T14 control char in a non-file field (file paths are caught earlier by
    # mf_path_ok, mirrored)
    t = fixture("t14-field-control")
    write(os.path.join(t, "loadlist.txt"),
          good_ll.replace(b"mod\tTownfallItemSpawner\t1.0.0",
                          b"mod\tTownfall\x01Item\t1.0.0"))
    check("T14 control byte in field -> LOADLIST_FIELD_CONTROL", t,
          ("LOADLIST_FIELD_CONTROL",))

    # T15 invalid UTF-8 loadlist line
    t = fixture("t15-bad-utf8")
    write(os.path.join(t, "loadlist.txt"), good_ll + b"note\tx \x80y\n")
    check("T15 invalid UTF-8 -> LOADLIST_PARSE", t, ("LOADLIST_PARSE",),
          expect_detail="invalid UTF-8")

    # T16 backslash in file path (mf_path_ok layer)
    t = fixture("t16-backslash")
    write(os.path.join(t, "loadlist.txt"),
          good_ll.replace(
              b"ue4ss/Mods/TownfallItemSpawner/enabled.txt\t0",
              b"ue4ss\\Mods\\TownfallItemSpawner\\enabled.txt\t0"))
    check("T16 backslash path -> LOADLIST_PARSE", t, ("LOADLIST_PARSE",))

    # T17 '..' segment in file path
    t = fixture("t18-dotdot")
    write(os.path.join(t, "loadlist.txt"),
          good_ll.replace(b"ue4ss/Mods/TownfallItemSpawner/enabled.txt\t0",
                          b"ue4ss/Mods/../Mods/x.txt\t0"))
    check("T18 '..' segment -> LOADLIST_PARSE", t, ("LOADLIST_PARSE",))

    # T19 colon in key (passes [LL], fails [NMS] validate_path)
    t = fixture("t17-colon")
    write(os.path.join(t, "loadlist.txt"),
          good_ll.replace(b"enabled.txt", b"ena:bled.txt"))
    write(os.path.join(t, "ue4ss/Mods/TownfallItemSpawner/ena:bled.txt"), b"")
    check("T19 colon in key -> PATH_KEY_INVALID", t, ("PATH_KEY_INVALID",),
          expect_detail="invalid character")

    # T20 unknown role
    t = fixture("t20-role")
    write(os.path.join(t, "loadlist.txt"),
          good_ll.replace(
              b"file\tlua_mod\tue4ss/Mods/shared/UEHelpers/UEHelpers.lua\t"
              + str(len(uh)).encode(),
              b"file\tbanana\tue4ss/Mods/shared/UEHelpers/UEHelpers.lua\t"
              + str(len(uh)).encode()))
    check("T20 unknown role -> ROLE_UNSUPPORTED", t, ("ROLE_UNSUPPORTED",),
          expect_detail="Unsupported UE4SS activation file role")

    # T21 preload per-file limit (8 MiB): exactly the limit is allowed [MFA
    # :377 is strictly '>'], one byte over fails
    t = fixture("t21-oversize")
    write(os.path.join(t, "loadlist.txt"),
          good_ll.replace(
              b"file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/Scripts/main.lua\t"
              + str(len(lua)).encode(),
              b"file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/Scripts/main.lua\t"
              + str((8 << 20) + 1).encode()))
    check("T21 preload >8MiB -> LOADLIST_LIMIT", t, ("LOADLIST_LIMIT",),
          expect_detail="file too large to preload")

    # T22 wrong title id
    t = fixture("t22-title")
    write(os.path.join(t, "loadlist.txt"),
          good_ll.replace(b"title\tPPSA33286", b"title\tPPSA00000"))
    check("T22 wrong title -> TITLE_MISMATCH", t, ("TITLE_MISMATCH",),
          expect_detail="MFA_E_TITLE")

    # T23 engine/framework gates
    t = fixture("t23-engine")
    write(os.path.join(t, "loadlist.txt"),
          good_ll.replace(b"engine\tunreal", b"engine\tunity")
                 .replace(b"framework\tue4ss-ps5", b"framework\tother-fw"))
    check("T23 engine/framework gate -> ENGINE_GATE + FRAMEWORK_GATE", t,
          ("ENGINE_GATE", "FRAMEWORK_GATE"))

    # T24 pad structural errors
    t = fixture("t24-pad-syntax")
    pad = (b"[PS5Pad]\nUser = -1\n\n[Bindings]\n"
           b"OPTIONS = NOT_A_KEY\n"        # unknown UE4SS key
           b"BANANA = F7\n"                # unknown controller button
           b"OPTIONS = F8\n"               # valid; adds chord
           b"OPTIONS = F9\n"               # duplicate chord
           b"L1+L1 = F10\n"                # repeated button in chord
           b"CROSS+OPTIONS = F11\n"        # chord contains configured chord
           b"UP = SHIFT+MYSTERY+F12\n"     # unknown modifier
           b"= F13\n"                      # empty name
           b"DOWN = A=B\n"                 # two '='
           b"LEFT = SHIFT+SHIFT+F14\n")    # repeated modifier
    write(os.path.join(t, "ue4ss/PS5Pad.ini"), pad)
    sync_ll(t)
    pf = check("T24 pad syntax class -> PAD_SYNTAX", t, ("PAD_SYNTAX",),
               absent_ids=("PAD_ASCII", "PAD_SIZE_LIMIT"))
    n = len([f for f in pf.findings if f["id"] == "PAD_SYNTAX"])
    run("T25 pad syntax caught >=8 distinct errors", n >= 8, "got %d" % n)

    # T26 pad size limits
    t = fixture("t26a-pad-empty")
    write(os.path.join(t, "ue4ss/PS5Pad.ini"), b"")
    sync_ll(t)
    check("T26a empty pad -> PAD_SIZE_LIMIT", t, ("PAD_SIZE_LIMIT",),
          ("PAD_ASCII",))
    t = fixture("t26b-pad-oversize")
    write(os.path.join(t, "ue4ss/PS5Pad.ini"),
          b"[Bindings]\nOPTIONS = F7\n" + b"# pad\n" * 2760)  # 16585 > 16384
    sync_ll(t)
    check("T26b >16384-byte pad -> PAD_SIZE_LIMIT", t, ("PAD_SIZE_LIMIT",),
          ("PAD_ASCII",))

    # T27 NUL/DEL control bytes in pad
    t = fixture("t27-pad-control")
    write(os.path.join(t, "ue4ss/PS5Pad.ini"),
          b"[Bindings]\nOPTIONS = F7\x00\nUP\x7f = F8\n")
    sync_ll(t)
    check("T27 NUL/DEL in pad -> PAD_ASCII", t, ("PAD_ASCII",))
    # The '?' fold makes the file ASCII, but '? ' is not a valid UE4SS key, so
    # the post-fix tree must show no PAD_ASCII while PAD_SYNTAX (garbage
    # binding) is still loudly reported -- never silently laundered.
    fx = roundtrip("T28 control-byte pad fix -> ASCII clean, binding error kept",
                   t, ("PAD_ASCII",), allow_post=("PAD_SYNTAX",))
    with open(os.path.join(fx, "ue4ss/PS5Pad.ini"), "rb") as fh:
        fixed = fh.read()
    run("T29 folded pad now passes the [PAD] byte gate",
        all(b <= 0x7E and (b >= 0x20 or b in (0x0A, 0x0D, 0x09))
            for b in fixed))

    # T30 Scripts/scripts ambiguity
    t = fixture("t30-ambiguous")
    write(os.path.join(t, "loadlist.txt"),
          good_ll + b"file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/scripts/main.lua\t"
          + str(len(lua)).encode() + b"\n")
    write(os.path.join(t, "ue4ss/Mods/TownfallItemSpawner/scripts/main.lua"), lua)
    check("T30 Scripts+scripts ambiguity -> MOD_LAYOUT", t, ("MOD_LAYOUT",),
          expect_detail="ambiguous Scripts/scripts")

    # T31 main.lua missing (warn only)
    t = fixture("t31-nomain")
    write(os.path.join(t, "loadlist.txt"),
          good_ll.replace(
              b"file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/Scripts/main.lua\t"
              + str(len(lua)).encode(),
              b"file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/Scripts/other.lua\t3"))
    write(os.path.join(t, "ue4ss/Mods/TownfallItemSpawner/Scripts/other.lua"),
          b"---")
    check("T31 no main.lua -> MOD_MAIN_LUA_MISSING (warn)", t,
          ("MOD_MAIN_LUA_MISSING",))

    # T32 malformed mods.txt
    t = fixture("t32-mods-txt")
    write(os.path.join(t, "loadlist.txt"),
          good_ll + b"file\tlua_mod\tue4ss/Mods/mods.txt\t13\n")
    write(os.path.join(t, "ue4ss/Mods/mods.txt"), b"TownfallItem\n")
    check("T32 malformed mods.txt -> MODS_TXT_FORMAT", t, ("MODS_TXT_FORMAT",),
          expect_detail="malformed enabled row")

    # T33 non-ue4ss path (skipped by UE4SS layer, still preloaded)
    t = fixture("t33-nonue4ss")
    write(os.path.join(t, "loadlist.txt"),
          good_ll + b"file\tconfig\tdata/extra.ini\t3\n")
    write(os.path.join(t, "data/extra.ini"), b"hi\n")
    check("T33 non-ue4ss path -> KEY_NOT_UE4SS warn (no error)", t,
          ("KEY_NOT_UE4SS",))

    ok_all = all(ok for _n, ok in results)
    print("\nSELFTEST %s: %d/%d checks passed"
          % ("PASS" if ok_all else "FAIL",
             sum(1 for _n, ok in results if ok), len(results)), file=out)
    shutil.rmtree(tmp, ignore_errors=True)
    return 0 if ok_all else 1


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Preflight a local PS5 UE4SS mod tree against the runtime's "
                    "own data-file gates before a launch (SOURCE PINS in the "
                    "module docstring).")
    ap.add_argument("--tree", default=DEFAULT_TREE,
                    help="local mod-tree dir (default: settings-v3 package tree)")
    ap.add_argument("--loadlist", default=None,
                    help="loadlist file (default: <tree>/loadlist.txt)")
    ap.add_argument("--title", default=DEFAULT_TITLE,
                    help="expected game title id (default %s)" % DEFAULT_TITLE)
    ap.add_argument("--fix", action="store_true",
                    help="write fixed copies + FIXES-DIFF.txt under --fix-out")
    ap.add_argument("--fix-out", default=None,
                    help="output dir for --fix (default: <tree>-preflight-fixed)")
    ap.add_argument("--json", default=None, help="also write findings as JSON")
    ap.add_argument("--selftest", action="store_true",
                    help="run the built-in fixture self-test")
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()

    tree = os.path.abspath(args.tree)
    loadlist_path = os.path.abspath(
        args.loadlist or os.path.join(tree, "loadlist.txt"))
    if not os.path.isdir(tree):
        print("error: tree dir not found: %s" % tree, file=sys.stderr)
        return 2

    if args.fix:
        fix_out = os.path.abspath(args.fix_out or tree + "-preflight-fixed")
        pf0, fix_log = run_fix(tree, loadlist_path, fix_out, args.title)
        print("== BEFORE (input tree) ==")
        print_report(pf0, tree, loadlist_path)
        print("\n== FIX APPLIED -> %s ==" % fix_out)
        for entry in fix_log:
            print("  " + entry.replace("\n", "\n  "))
        pf1, _ = verify(fix_out, os.path.join(fix_out, "loadlist.txt"),
                        args.title)
        print("\n== AFTER (fixed copy) ==")
        print_report(pf1, fix_out, os.path.join(fix_out, "loadlist.txt"))
        pf = pf1
        tree_out = fix_out
    else:
        pf, _ = verify(tree, loadlist_path, args.title)
        print_report(pf, tree, loadlist_path)
        tree_out = tree

    if args.json:
        payload = {
            "tree": tree_out,
            "loadlist": (os.path.join(tree_out, "loadlist.txt")
                         if args.fix else loadlist_path),
            "verdict": "CLEAN" if not pf.findings else "FAILED",
            "errors": len(pf.errors), "warnings": len(pf.warnings),
            "findings": pf.findings,
            "files": {rel: {"sha256": h}
                      for rel, h in sorted(pf.file_hashes.items())},
        }
        with open(args.json, "w") as fh:
            json.dump(payload, fh, indent=2)
            fh.write("\n")
        print("json findings written: %s" % args.json)
    return 0 if not pf.errors else 1


if __name__ == "__main__":
    sys.exit(main())
