"""Mac-only: are the engine names the GUI backend needs present in a game's own
executable (UHT registration strings, ASCII or UTF-16)? Source 1 of 2 (source 2
is the run-time reflection lookup). Generic: reads only the names list and the
game file. Prints items_checked; fails on zero or on any missing name."""
import json
import sys

names = json.load(open(sys.argv[1]))
data = open(sys.argv[2], "rb").read()
items, missing = 0, []
for group in ("classes", "functions", "parameters", "struct_members", "properties"):
    for n in names[group]:
        items += 1
        found = n.encode() in data or n.encode("utf-16-le") in data
        if not found:
            missing.append(f"{group}:{n}")
print(f"engine_names_check items_checked={items} missing={len(missing)} {' '.join(missing)}")
sys.exit(2 if items == 0 else (1 if missing else 0))
