#!/usr/bin/env python3
"""simulate_ploader_ini.py — host-side re-implementation of the PINNED ploader
ini parser (.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/
original/main.c, read_ini/trim/is_blank_or_comment/parse_section/parse_mode/
normalize_path/get_or_create_slot/register_plugin) with the exact constants
MAX_PLUGINS=10, MAX_TITLES=10, MAX_PATH_LEN=255, MAX_TITLE_ID_LEN=16.

Purpose: prove the COMBINED 6-entry ploader.ini parses to the intended 5
registered plugins in file order with the mode-0 line skipped, under all
layout variants (canonical, trailing-newline-absent, comments, reordered).
No native code executed; pure host simulation of pinned C logic."""
import sys

MAX_PLUGINS = 10
MAX_TITLES = 10
MAX_PATH_LEN = 255
MAX_TITLE_ID_LEN = 16


def trim(s: str) -> str:
    s = s.lstrip(" \t")
    end = len(s) - 1
    # C trim mutates in place stripping trailing ' ', '\t', '\r', '\n'
    while end > 0 and s[end] in " \t\r\n":
        end -= 1
    # C loop: while (end > s && ...) *end-- = 0  -> strips while end>0 index
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
    # atoi semantics: optional ws, optional sign, digits until non-digit
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
        line = raw.split("\n")[0][:1024]  # fgets(line,1024) per line incl \n
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
        if mode <= 0:  # -1 invalid, 0 disabled: SKIPPED (main.c L672)
            dropped.append((key.strip(), mode))
            continue
        normalized = normalize_path(key)
        if normalized is None:
            dropped.append((key.strip(), mode))
            continue
        # get_or_create_slot
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
        # register_plugin
        if len(slot.paths) >= MAX_PLUGINS:
            dropped.append((normalized, mode))
            continue
        slot.paths.append(normalized[:MAX_PATH_LEN + 1])
        slot.modes.append(mode)
    return plugins, dropped


CANONICAL = """[PPSA33286]
/data/plugins/modbridge_runtime_aligned_entry.prx = 2
/data/plugins/modbridge_path_test.prx = 2
/data/plugins/modbridge_ingame_facts.prx = 0
/data/plugins/modbridge_supplier_composition.prx = 2
/data/plugins/modbridge_qh_driver.prx = 2
/data/plugins/modbridge_runtime_aligned_observer.prx = 2
"""

EXPECT_ORDER = [
    "/data/plugins/modbridge_runtime_aligned_entry.prx",
    "/data/plugins/modbridge_path_test.prx",
    "/data/plugins/modbridge_supplier_composition.prx",
    "/data/plugins/modbridge_qh_driver.prx",
    "/data/plugins/modbridge_runtime_aligned_observer.prx",
]

VARIANTS = {
    "canonical": CANONICAL,
    "no_trailing_newline": CANONICAL.rstrip("\n"),
    "crlf": CANONICAL.replace("\n", "\r\n"),
    "blank_lines_comments": "[PPSA33286]\n\n; oracle line kept\n"
        "/data/plugins/modbridge_runtime_aligned_entry.prx = 2\n"
        "# qdrv1 ridealong\n"
        "/data/plugins/modbridge_path_test.prx = 2\n"
        "/data/plugins/modbridge_ingame_facts.prx = 0\n"
        "/data/plugins/modbridge_supplier_composition.prx = 2\n"
        "/data/plugins/modbridge_qh_driver.prx = 2\n"
        "/data/plugins/modbridge_runtime_aligned_observer.prx = 2\n",
    "ridealongs_first": "[PPSA33286]\n"
        "/data/plugins/modbridge_qh_driver.prx = 2\n"
        "/data/plugins/modbridge_runtime_aligned_observer.prx = 2\n"
        "/data/plugins/modbridge_runtime_aligned_entry.prx = 2\n"
        "/data/plugins/modbridge_path_test.prx = 2\n"
        "/data/plugins/modbridge_ingame_facts.prx = 0\n"
        "/data/plugins/modbridge_supplier_composition.prx = 2\n",
    "duplicate_line_hazard": CANONICAL
        + "/data/plugins/modbridge_qh_driver.prx = 2\n",
    "no_stray_paths": CANONICAL + "/data/plugins/modbridge_ingame_facts.prx = 2\n",
    "invalid_mode_3": CANONICAL + "/data/plugins/modbridge_bad.prx = 3\n",
}


def main() -> int:
    failures = []
    for name, text in VARIANTS.items():
        plugins, dropped = read_ini(text)
        assert len(plugins) <= MAX_TITLES
        if not plugins:
            failures.append(f"{name}: no title slot parsed")
            continue
        e = plugins[0]
        reg = list(zip(e.paths, e.modes))
        print(f"variant={name} registered={len(reg)} dropped={dropped}")
        for p, m in reg:
            print(f"   mode={m} {p}")
        # registered set check: mode-0 line never appears
        if name != "no_stray_paths" and any(
                p.endswith("modbridge_ingame_facts.prx") for p, _ in reg):
            failures.append(f"{name}: mode-0 entry was registered (parser violation)")
        if name in ("invalid_mode_3",):
            if any(p.endswith("modbridge_bad.prx") for p, _ in reg):
                failures.append(f"{name}: mode-3 entry registered (must be rejected)")
            continue
        if name in ("no_stray_paths",):
            if len(reg) != 6:
                failures.append(f"{name}: expected 6 registered if igf flipped to 2, got {len(reg)}")
            if len(reg) > MAX_PLUGINS:
                failures.append(f"{name}: exceeds MAX_PLUGINS={MAX_PLUGINS}")
            continue
        if name in ("duplicate_line_hazard",):
            qdrv = [1 for p, _ in reg if p.endswith("modbridge_qh_driver.prx")]
            if len(qdrv) != 2:
                failures.append(f"{name}: expected duplicate registration x2, got {len(qdrv)}")
            print(f"   HAZARD CONFIRMED: ploader registers duplicates -> "
                  f"duplicate injection calls ({len(qdrv)}x qdrv1)")
            continue
        if name == "ridealongs_first":
            if [p for p, _ in reg] != [
                    "/data/plugins/modbridge_qh_driver.prx",
                    "/data/plugins/modbridge_runtime_aligned_observer.prx",
                    "/data/plugins/modbridge_runtime_aligned_entry.prx",
                    "/data/plugins/modbridge_path_test.prx",
                    "/data/plugins/modbridge_supplier_composition.prx"]:
                failures.append(f"{name}: reorder registration != file order")
            continue
        if [p for p, _ in reg] != EXPECT_ORDER:
            failures.append(f"{name}: order/set mismatch: {[p for p, _ in reg]}")
        if not all(m == 2 for _, m in reg):
            failures.append(f"{name}: expected all modes == 2")
    if failures:
        print("FAIL")
        for f in failures:
            print(" -", f)
        return 1
    print("SIMULATION_PASSED: all variants within MAX_PLUGINS=10/MAX_TITLES=10; "
          "mode-0 skipped at parse; order=registration=registration-file-order")
    return 0


if __name__ == "__main__":
    sys.exit(main())
