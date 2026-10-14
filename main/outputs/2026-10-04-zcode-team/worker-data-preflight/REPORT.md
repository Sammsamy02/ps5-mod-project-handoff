# REPORT.md — worker-data-preflight, 2026-10-05 (written last)

Local-only, host-only. No console contact, no engine writes, no changes to
`console_batch.py` or any other worker's output. All work and deliverables
live under `outputs/2026-10-04-zcode-team/worker-data-preflight/`.

## Delivered

- `tools/data_preflight.py` — standalone preflight tool (verify / `--fix` /
  `--selftest` / `--json`; exit 0 clean, 1 findings, 2 usage). Read-only by
  default; `--fix` never writes into the input tree.
- `TEST-RECEIPT.txt` — full receipt: 33/33 selftest checks, both real
  incident reproductions with the exact run1/run2 log lines, fixer round-trip
  evidence, input-tree safety hashes.
- `CURRENT-TREE-REPORT.md` + `current-staged-tree-report.json` — the current
  staged tree state verification: **CLEAN**.
- `CLAIM.json` — every check pinned to runtime source file:line, plus
  explicit non-claims.
- `current-staged-tree/` — the verified local mirror of the current staged
  payload (package-v3 + sanitized pad + sizefixed loadlist).
- `demo-fix-from-package-v3/` — `--fix` output on the default tree
  (byte-identical payload to the mirror, plus `FIXES-DIFF.txt`).

## Both launch-10 data gates are now caught before a launch

1. **run1 (em-dash pad)**: the tool re-flags the exact file — `PAD_ASCII`
   byte 0xe2 at offset 19, pinned to
   `.superpowers/native-dependency-spike/ue4ss-native-pad-config-1/src/PS5PadConfig.cpp:56-58`.
   `--fix` folds em-dash → `--` and the result is **byte-identical** to the
   real `PS5Pad-sanitized.ini` the controller shipped.
2. **run2 (size mismatch)**: `FILE_SIZE_SHORT` ("file is shorter than the
   load list says"), pinned to
   `.../native-coherent-runtime-1/source/activation/modforge_activation.c:185-189`
   — the exact `MFA_E_SIZE (-8)` path. `--fix` resyncs the loadlist and the
   result is **byte-identical** to the real `loadlist-sizefixed.txt`.

Beyond these, the tool enforces every other loadlist-declared invariant found
in the runtime sources: loadlist shape (header, UTF-8, field counts, u64
sizes, path rules), one title / one engine / ≤1 sdk, title-vs-game match,
engine=unreal + framework=ue4ss-ps5, duplicate paths, role whitelist, UE4SS
key canonicality (no `:`, `\`, control bytes, non-canonical segments), 4096
files / 8 MiB per preloaded file / 64 MiB total / 1022-byte path limits,
mods.txt row grammar, Scripts/scripts ambiguity, main.lua discovery, and the
complete PS5Pad.ini grammar (sections, User, 128-binding cap, button table,
modifier rules, the 167-name UE4SS key table, chord duplicate/containment).

## Self-test

`--selftest` builds 33 fixture checks in a temp dir: every failure class
above, fixer round-trips (sanitize/resync → re-verify CLEAN), the two real
incidents from the actual evidence copies, and negative controls. 33/33 PASS,
exit 0. One deliberate semantics check: folding NUL/DEL garbage inside a
binding makes the file ASCII (byte gate passes) but the now-invalid key is
still loudly reported (`PAD_SYNTAX`) — the fixer never launders a broken
binding into a passing one.

## Current staged tree: CLEAN, with one loud warning

The mirror of the current staged state (sanitized pad + sizefixed loadlist +
the rest of package-v3) verifies **CLEAN, 0 errors, 0 warnings**. Payload
hashes match the settings-v3 report, so the mirror is exactly what passed the
data gates in launch-10 run 3.

**Warning:** the DEFAULT tree (`worker-settings-v3-package/package-v3`)
still contains the ORIGINAL em-dash PS5Pad.ini and the 2960 loadlist row —
the tool correctly reports it FAILED. Re-staging from `package-v3/` as-is
would re-create run1's death. Stage from
`worker-data-preflight/current-staged-tree/` (or `demo-fix-from-package-v3/`,
byte-identical) or re-run `--fix` on the intended staging tree. `package-v3/`
itself was not modified (hashes re-verified, `TEST-RECEIPT.txt` section 5).

## Honest scope limits

- Does NOT clear launch-10 run3's `PS5Pad input unavailable … error=-2137915388`
  — that is a pad-provider runtime failure, not a data-file gate.
- UE4SS-settings.ini / layout .ini CONTENT is not validated beyond
  presence/size/path/role: the spike sources contain no hard content gate for
  them (their parse acceptance lives in worker-settings-v3-package's host
  tests).
- native_plugin rows: role/path/size checks implemented, but no such rows
  exist in this loadlist, so that path is only fixture-covered at the
  role/path layer.
- Verifies the LOCAL mirror only; it cannot see console-side drift.
