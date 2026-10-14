"""cap11 static lint (Mac-only): staged runtime/SDK code contains no
code-patching primitive, no game/title name and no hard-coded engine address
(hex literals of 6+ digits other than ABI magics, CRC/PNG/bit-pattern
constants listed explicitly). Prints items_checked; fails on zero."""
import re
import sys
from pathlib import Path

cap = Path(sys.argv[1])
files = [p for p in sorted((cap / "PATCH").rglob("*")) if p.suffix in (".h", ".hpp", ".inc", ".cpp", ".diff")]
forbidden = re.compile(r"PLH::|x64Detour|IatHook|mprotect|VirtualProtect|kernel_mprotect|WriteProcessMemory|\bDetour[A-Z]\w*\(")
names = re.compile(r"Townfall|Silent ?Hill|Stellar|PPSA\d|Leon|Konami", re.I)
allowed_hex = {"0x314d424d", "0x31495547", "0xedb88320", "0xffffffff", "0x000fffffffffffff", "0x3ff0000000000000", "0x1ff8000000000000"}
items, fails = 0, []
for p in files:
    text = p.read_text(errors="replace").splitlines()
    if p.suffix == ".diff":
        text = [l[1:] for l in text if l.startswith("+") and not l.startswith("+++")]
    code = [l for l in text if not l.strip().startswith(("//", "*", "/*"))]
    for what, bad in (
        ("no-detour", [l for l in code if forbidden.search(l)]),
        ("generic", [l for l in text if names.search(l)]),
        ("no-address", [h for l in code for h in re.findall(r"0x[0-9A-Fa-f]{6,}u?l*l*", l) if h.lower().rstrip("ul") not in allowed_hex]),
    ):
        items += 1
        ok = not bad
        print(("  pass " if ok else "  FAIL ") + f"{what} {p.relative_to(cap)}" + ("" if ok else f": {str(bad[0]).strip()[:90]}"))
        if not ok:
            fails.append(str(p))
# [fixer] CHANGES-ROWS.json must match the staged files byte for byte, and the
# staged runtime code must not hold a thread_local with a destructor (B2).
import hashlib, json
rows = json.loads((cap / "PATCH/CHANGES-ROWS.json").read_text())
team = cap.parent
for r in rows.get("files_rows_replace", []) + rows.get("added_files", []):
    src = Path(r["from"]) if Path(r["from"]).is_absolute() else team / r["from"]
    want = r.get("sha256", r.get("candidate_sha256"))
    ok = src.exists() and hashlib.sha256(src.read_bytes()).hexdigest() == want
    items += 1
    print(("  pass " if ok else "  FAIL ") + f"row sha {src.name} {want[:8]}")
    if not ok:
        fails.append(f"row {src.name}")
tl_bad = re.compile(r"\bthread_local\b")
for p in files:
    if p.parts[-3:-1] == ("sdk", "src") or "examples" in p.parts:
        continue  # the mod side links its own runtime; the gate is for the runtime PRX
    hits = [l for l in p.read_text(errors="replace").splitlines() if tl_bad.search(l.split("//")[0]) and "constinit" not in l]
    items += 1
    print(("  pass " if not hits else "  FAIL ") + f"no non-constinit thread_local {p.relative_to(cap)}")
    if hits:
        fails.append(str(p))
print(f"lint_patch items_checked={items} files={len(files)} failed={len(fails)}")
sys.exit(2 if not files else (1 if fails else 0))
