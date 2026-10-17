"""#11 coverage gate v2 (Mac-only). Two levels per required ImGui API:
  host:    the API is called by the NativeGuiProbe source AND was executed across
           the module boundary in this run (count > 0 in the [NGUI] coverage-list
           line printed by test_gui_sdk_mods), against the host's real Dear
           ImGui 1.92.1 context;
  console: pending until the #11 launch (never set here).
Required set = the imgui-family calls of the real mods' inventory (reproduced).
Writes COVERAGE-HOST.json. Exit 0 only when the host level has zero gaps.
Prints items_checked; fails on zero."""
import json
import re
import sys

inv = json.load(open(sys.argv[1]))
probe_src = open(sys.argv[2]).read()
log = open(sys.argv[3]).read()
out_path = sys.argv[4]
required = sorted({r["symbol"] for r in inv["calls"] if r["family"] == "imgui"})
m = re.search(r"COVERAGE-LINE .*?\[NGUI\] coverage-list (.*)", log)
counts = {}
if m:
    for tok in m.group(1).split():
        if "=" in tok:
            k, v = tok.rsplit("=", 1)
            counts[k] = int(v)
rows = {}
missing = []
for name in required:
    in_src = bool(re.search(r"NG\(" + re.escape(name) + r"\b", probe_src) or re.search(r"ImGui::" + re.escape(name) + r"\(", probe_src))
    ran = counts.get(name, 0)
    host = "exercised-cross-module" if in_src and ran > 0 else "missing"
    rows[name] = {"host": host, "calls_in_run": ran, "in_probe_source": in_src, "console": "pending-#11-launch"}
    if host == "missing":
        missing.append(name)
res = {
    "label": "Mac-only. host = real Dear ImGui 1.92.1 linked in host and mod, shared context, executed in test_gui_sdk_mods; console = pending",
    "required_count": len(required),
    "host_missing": missing,
    "console_proven": 0,
    "apis": rows,
}
json.dump(res, open(out_path, "w"), indent=1)
print(f"coverage_gate_v2 items_checked={len(required)} host_exercised={len(required)-len(missing)} host_missing={len(missing)} console_proven=0")
if not required:
    sys.exit(2)
sys.exit(1 if missing else 0)
