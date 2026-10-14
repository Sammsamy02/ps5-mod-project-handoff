"""Mac-only check that compiling Dear ImGui with mbm_imconfig.h + the unity
wrapper adds NO new C import to the runtime PRX: undefined C symbols of the
optimised unity object (Mac nm, leading underscore stripped; C++/runtime-internal
symbols and Darwin-only memset lowerings excluded) minus symbols the object
defines itself, compared with the R11 import names. Compiled with -fmath-errno
to keep ELF-like libm call semantics. The integrator confirms on the target
link (supplier contract). Prints items_checked; fails on zero or on any new
import."""
import subprocess
import sys

r11 = set(open(sys.argv[1]).read().split())
DARWIN_ONLY = {"bzero", "memset_pattern16", "memset_pattern8", "memset_pattern4"}
und, defined = set(), set()
for o in sys.argv[2:]:
    for line in subprocess.run(["nm", o], capture_output=True, text=True).stdout.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        kind, sym = parts[-2], parts[-1]
        sym = sym[1:] if sym.startswith("_") else sym
        (und if kind == "U" else defined).add(sym)
cands = sorted(s for s in und - defined if not s.startswith(("_", "Z")) and s not in DARWIN_ONLY)
new = [s for s in cands if s not in r11]
print(f"import_delta items_checked={len(cands)} already_imported={len(cands)-len(new)} new={len(new)} darwin_only_ignored={len(und & DARWIN_ONLY)}")
print("c_symbols: " + " ".join(cands))
print("new_c_imports: " + (" ".join(new) if new else "(none)"))
sys.exit(2 if not cands else (1 if new else 0))
