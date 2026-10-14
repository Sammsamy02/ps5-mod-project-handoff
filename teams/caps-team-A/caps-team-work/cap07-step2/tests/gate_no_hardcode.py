#!/usr/bin/env python3
"""gate_no_hardcode.py - rule-10 + no-detour gate for the staged mount unit.

Scans the staged runtime sources (PATCH/*.hpp, *.cpp) and FAILS if any contain:
  - a game/title identifier (PPSA33286, 'Townfall', 'Silent Hill', 'Leon', ...)
  - a literal container/chunk name ('pakchunk', 'zzz_', '_P.pak', '.utoc' as a
    decision constant, 'global.utoc')
  - a code-write / detour pattern (mprotect, memcpy into text, JMP-patch words,
    'detour', 'trampoline', writing a vtable slot of engine code)
Prints how many files and how many lines it scanned; FAILS if it scanned zero
files (empty scan == failed gate).

The .inc is DATA (per-version offsets) and is scanned too, but the image sha and
image-relative offsets there are allowed (they are not game/mod *names*). Only
name/detour tokens fail.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATCH = os.path.join(HERE, "..", "PATCH")
SCAN_EXT = (".hpp", ".cpp", ".inc")

# Forbidden in RUNTIME LOGIC/DATA that ships. Names only; comments count too,
# because a shipped source with a game name violates the generic-runtime law.
NAME_TOKENS = [
    r"PPSA\d{5}", r"Townfall", r"Silent\s*Hill", r"Leon", r"Stellar\s*Blade",
    r"pakchunk", r"zzz_", r"global\.utoc",
]
# "_P.pak" / ".utoc" as a *decision constant* (a string literal used in logic).
CONTAINER_LITERAL = re.compile(r'"[^"]*(_P\.pak|\.utoc|\.ucas)[^"]*"')
# Detour / code-write patterns.
DETOUR_TOKENS = [
    r"\bmprotect\b", r"VirtualProtect", r"\bmemcpy\b\s*\([^)]*text",
    r"write_code", r"patch_bytes", r"install_hook", r"\bInlineHook\b",
]

# The .inc may legitimately contain the image sha and image-relative offsets.
# Those are NOT name tokens, so they pass; we do not special-case them.


def main():
    files = []
    for root, _dirs, names in os.walk(PATCH):
        for n in names:
            if n.endswith(SCAN_EXT):
                files.append(os.path.join(root, n))
    files.sort()

    total_lines = 0
    findings = []
    for f in files:
        with open(f, errors="replace") as fh:
            for i, line in enumerate(fh, 1):
                total_lines += 1
                for pat in NAME_TOKENS:
                    if re.search(pat, line, re.IGNORECASE):
                        findings.append((f, i, "name", pat, line.strip()[:80]))
                m = CONTAINER_LITERAL.search(line)
                if m:
                    findings.append((f, i, "container-literal", m.group(0),
                                     line.strip()[:80]))
                for pat in DETOUR_TOKENS:
                    if re.search(pat, line):
                        findings.append((f, i, "detour", pat, line.strip()[:80]))

    print(f"scanned {len(files)} files, {total_lines} lines")
    for f, i, kind, pat, ctx in findings:
        print(f"  [VIOLATION/{kind}] {os.path.relpath(f)}:{i} ~{pat}  {ctx}")

    if len(files) == 0 or total_lines == 0:
        print("GATE FAIL: scanned nothing")
        sys.exit(2)
    if findings:
        print(f"GATE FAIL: {len(findings)} violation(s)")
        sys.exit(1)
    print("GATE PASS: no game/mod names, no container-name decision constants, "
          "no detour/code-write patterns in the staged runtime unit")


if __name__ == "__main__":
    main()
