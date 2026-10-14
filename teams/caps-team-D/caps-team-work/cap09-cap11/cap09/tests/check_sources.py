"""cap09 static checks (Mac-only). Prints items_checked and fails on zero.

1. the SDK's copy of NativeModAbi.h is byte-identical to the host copy;
2. the SDK's generated Input/KeyDef.hpp has exactly the port's key values;
3. pinned base files (diff bases + loader) still have the recorded sha256;
4. no-detour lint: staged runtime code contains no code-patching primitive;
5. genericity lint: staged runtime code / SDK contain no game or title name
   and no hard-coded engine address (hex literal of 6+ digits other than the
   documented ABI magics).
6. [B1] the staged UE4SSProgram.cpp carries the 4 startup hunks, and the
   startup order setup_mods -> install_cpp_mods -> start_cpp_mods in
   start_native_mods() is unchanged; full-file candidate copies are linted
   through their diffs only (added lines).
7. [B2] no thread_local of a type with a destructor in staged runtime code
   (it would emit __cxa_thread_atexit = a new weak import).
"""
import hashlib
import json
import re
import sys
from pathlib import Path

cap = Path(sys.argv[1])
loader_src = Path(sys.argv[2])
port_keydef = Path(sys.argv[3])
items = 0
fails = []


def check(cond, what):
    global items
    items += 1
    print(("  pass " if cond else "  FAIL ") + what)
    if not cond:
        fails.append(what)


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


# 1
a = cap / "PATCH/new/include/Mod/NativeModAbi.h"
b = cap / "PATCH/sdk/include/mbm/NativeModAbi.h"
check(a.read_bytes() == b.read_bytes(), "SDK NativeModAbi.h identical to host copy")

# 2
src = port_keydef.read_text()
m = re.search(r"enum Key : uint8_t\s*\{(.*?)\};", src, re.S)
port = dict((n, int(v, 16)) for n, v in re.findall(r"^\s*([A-Z_][A-Z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+),", m.group(1), re.M))
sdk_text = (cap / "PATCH/sdk/include/Input/KeyDef.hpp").read_text()
m2 = re.search(r"enum Key : uint8_t\s*\{(.*?)\};", sdk_text, re.S)
sdk = dict((n, int(v, 16)) for n, v in re.findall(r"^\s*([A-Z_][A-Z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+),", m2.group(1), re.M))
check(len(port) > 100 and port == sdk, f"SDK key enum equals port KeyDef.hpp ({len(port)} keys)")
check(sha(port_keydef) in sdk_text, "SDK KeyDef.hpp records the port source sha256")

# 3
pins = json.loads((cap / "PATCH/PINS.json").read_text())
for row in pins["files"]:
    p = Path(row["path"])
    check(p.exists() and sha(p) == row["sha256"], f"pin {p.name} {row['sha256'][:8]} ({row['role']})")

# 4 + 5
FULL_COPIES = {"UE4SSProgram.cpp", "LuaMod.cpp"}  # live candidate files: linted via diffs/<name>.diff
runtime_files = sorted((cap / "PATCH/new").rglob("*.*")) + [p for p in sorted((cap / "PATCH/candidate").rglob("*.*")) if p.name not in FULL_COPIES]
sdk_files = sorted((cap / "PATCH/sdk").rglob("*.*"))
diff_files = sorted((cap / "PATCH/diffs").rglob("*.diff"))
forbidden = re.compile(r"PLH::|x64Detour|IatHook|mprotect|VirtualProtect|sceKernelMprotect|kernel_mprotect|WriteProcessMemory|\bDetour[A-Z]\w*\(")
names = re.compile(r"Townfall|Silent ?Hill|Stellar|PPSA\d|Leon|Konami", re.I)
allowed_hex = {"0x314D424D", "0x31495547"}


def runtime_lines(p):
    if p.suffix == ".diff":
        return [l[1:] for l in p.read_text(errors="replace").splitlines() if l.startswith("+") and not l.startswith("+++")]
    return p.read_text(errors="replace").splitlines()


scanned = 0
for p in runtime_files + diff_files:
    if p.suffix not in (".h", ".hpp", ".inc", ".cpp", ".diff"):
        continue
    scanned += 1
    lines = runtime_lines(p)
    bad = [l for l in lines if forbidden.search(l) and not l.strip().startswith(("//", "*", "/*"))]
    check(not bad, f"no-detour lint {p.name}" + (f": {bad[0].strip()[:80]}" if bad else ""))
    hexes = [h for l in lines for h in re.findall(r"0x[0-9A-Fa-f]{6,}", l) if h.upper().replace("X", "x") not in {x.upper().replace("X", "x") for x in allowed_hex}]
    check(not hexes, f"no hard-coded address literal {p.name}" + (f": {hexes[0]}" if hexes else ""))
for p in runtime_files + diff_files + sdk_files:
    if p.suffix not in (".h", ".hpp", ".inc", ".cpp", ".diff"):
        continue
    scanned += 1
    hits = [l for l in runtime_lines(p) if names.search(l)]
    check(not hits, f"genericity lint {p.relative_to(cap)}" + (f": {hits[0].strip()[:80]}" if hits else ""))
check(scanned > 0, f"lint scanned {scanned} files")

# 6
prog = (cap / "PATCH/candidate/src/UE4SSProgram.cpp").read_text()
check(prog.count("[CAP09 B1]") == 5, f"staged UE4SSProgram.cpp carries the B1 hunks ({prog.count('[CAP09 B1]')} markers, expect 5)")
check("Platform::NativeMods::discover_cpp_mods(" in prog and "std::make_unique<CppMod>(*this, std::move(name), std::move(mod_dir))" in prog,
      "setup_mods snapshot branch plans and creates CppMods")
# [F2] one try/catch per candidate; the start-order row is written before the module is kept
i3 = prog.find("for (const auto& candidate : inventory.start_order)")
loop = prog[i3:i3 + 2600]
li = [loop.find(x) for x in ("try\n", "native_cpp_start_order().push_back(candidate.name);", "std::make_unique<CppMod>(", "m_mods.emplace_back(std::move(mod));",
                             "catch (const std::exception& error)", "catch (...)")]
check(i3 > 0 and all(x > 0 for x in li) and li == sorted(li), "[F2] per-candidate try: order row -> create -> keep, then catch std::exception and (...)")
check(prog.count("[CAP09 F2]") == 2, "[F2] markers present (2)")
# [F3] hot reload refused while native C++ mods are loaded, before anything is uninstalled
i5 = prog.find("auto UE4SSProgram::queue_reinstall_mods() -> void")
body5 = prog[i5:i5 + 3000]
check(i5 > 0 and "[CAP09 F3]" in body5 and 0 < body5.find("refused: hot reload") < body5.find("uninstall_mods();"),
      "[F3] queue_reinstall_mods refuses (logged) before uninstall_mods while C++ mods are present")
# [F1] staged port header: error_code overload only, and CppMod.cpp passes the out-parameter
ncmp = (cap / "PATCH/candidate/include/Platform/NativeCppModPath.hpp").read_text()
check("std::filesystem::status(path, ec)" in ncmp and "is_regular_file(" not in ncmp.split("namespace RC::Platform", 1)[1], "[F1] staged NativeCppModPath.hpp probes with status(path, ec) only (no throwing is_regular_file)")
for cm in (cap / "PATCH/candidate/src/Mod/CppMod.cpp", cap.parent / "cap11/PATCH/candidate/src/Mod/CppMod.cpp"):
    t = cm.read_text()
    check("FindNativeCppModModule(m_dlls_path, Unicode::EncodeUtf8(m_mod_name), &lookup_error)" in t and "if (lookup_error)" in t,
          f"[F1] {cm.parent.parent.parent.parent.name} CppMod.cpp passes the error_code out-parameter and refuses on error")
check("if constexpr (std::is_same_v<ModType, CppMod>)\n                start_native_cpp_mods_in_order();" in prog, "start_mods<CppMod> starts them under the snapshot")
i = prog.find("auto UE4SSProgram::start_native_mods() -> void")
body = prog[i:i + 2500]
order = [body.find(x) for x in ("setup_mods();", "install_cpp_mods();", "start_cpp_mods(IsInitialStartup::Yes);")]
check(i > 0 and all(o > 0 for o in order) and order == sorted(order), "start_native_mods: setup_mods -> install_cpp_mods -> start_cpp_mods (unchanged order)")
for name in sorted(FULL_COPIES):
    check((cap / "PATCH/diffs" / f"{name}.diff").exists(), f"{name}: diff present for the added-lines lint")

# 6b rows
rows = json.loads((cap / "PATCH/CHANGES-ROWS.json").read_text())
for r in rows["files_rows"] + rows["files_rows_refresh"]:
    src = cap.resolve().parent / r["from"]
    check(src.exists() and sha(src) == r["candidate_sha256"], f"row sha {src.name} {r['candidate_sha256'][:8]}")
for r in rows["files_rows_refresh"]:
    check(sha(r["candidate"]) == r["base_sha256_live"], f"live base unchanged since staging: {Path(r['candidate']).name} {r['base_sha256_live'][:8]}")
for r in rows["added_files"]:
    src = cap.resolve().parent / r["from"]
    check(src.exists() and sha(src) == r["sha256"], f"row sha {src.name} {r['sha256'][:8]}")

# 7
tl_bad_type = re.compile(r"\bthread_local\b[^;]*\b(std::|vector|string|map|function|unique_ptr|shared_ptr)\b")
tl_hits = []
for p in runtime_files + diff_files:
    if p.suffix not in (".h", ".hpp", ".inc", ".cpp", ".diff"):
        continue
    for l in runtime_lines(p):
        code = l.split("//")[0]
        if re.search(r"\bthread_local\b", code) and ("constinit" not in code or tl_bad_type.search(code)):
            tl_hits.append(f"{p.name}: {l.strip()[:90]}")
check(not tl_hits, "no thread_local with a destructor (only constinit trivially-destructible TLS)" + (f": {tl_hits[0]}" if tl_hits else ""))

print(f"check_sources items_checked={items} failed={len(fails)}")
if items == 0:
    print("FAIL: zero items")
    sys.exit(2)
sys.exit(1 if fails else 0)
