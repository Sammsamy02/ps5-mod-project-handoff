# Fail-closed native GUI coverage gate — Mac-only

The machine-readable `DECLARED-COVERAGE.json` explicitly marks the actual native ImGui surface blocked. `coverage_gate.py` compares it against cap09/ACTUAL-SURFACE.json. It returns 1 for missing/unimplemented APIs and 2 for an empty required surface. Only the literal status `implemented` satisfies a required API; design mappings and Lua analogues cannot pass.

```sh
python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap11/coverage_gate.py outputs/2026-10-12-caps-prep-chatgpt/cap09/ACTUAL-SURFACE.json outputs/2026-10-12-caps-prep-chatgpt/cap11/DECLARED-COVERAGE.json
```

Current expected result: exit 1, 76 APIs missing. This is a successful detection of the gap, not a capability PASS. The gate checks declarations only; changing JSON labels cannot establish actual implementation or console support. A future implemented entry needs real linked code, semantic tests and console evidence. Supporting structs, draw-list methods, ImGui internal APIs and renderer/context behavior require separate review, beyond the ImGui:: call-name set.
