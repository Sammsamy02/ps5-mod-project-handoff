# Agent CQ — l26 merged-tree integrity + combined compile smoke (2026-10-08)

Read-only verification of `outputs/2026-10-08-l26-runtime/typescan-r4/` after CN
(hardening, 5 rows) and CL (logwriter, 4 rows) landed concurrently in the same
CHANGES.json. CP's FindFirstOf row was expected in-flight and DID land mid-check
(row 29, UObjectGlobals.cpp, 20:02:40) — treated as in-flight and re-verified in
the final snapshot. Work artifacts: `/tmp/l26-integrity/` (pins-check.txt,
COMBINED-SMOKE.json, P2FIX.json, logs/, lw/ harness copy). No tree file was
written; CL's harness was re-run from a /tmp copy with VDIR repointed.

## VERDICT: MERGED-CLEAN

Two standing non-blocking observations for the parent are listed in §5.

## 1. CHANGES.json integrity (merged state)

Two snapshots: A = 19:55 state (29 rows, CN+CL merged), B = 20:02:40 state
(30 rows, after CP's row landed).

| check | snapshot A (29 rows) | snapshot B (30 rows) |
|---|---|---|
| JSON validity | VALID | VALID |
| files[] pins vs disk | 87/87 OK | 90/90 OK |
| changes[] pins vs disk | 87/87 OK | 90/90 OK |
| added_header_hashes (4) | 4/4 OK | 4/4 OK |
| source_selection pin | OK | OK |
| duplicate originals | NONE | NONE |
| mismatched/missing files | 0 / 0 | 0 / 0 |

Every `original_sha256`, `candidate_sha256`, `copied_original_sha256` in BOTH
arrays recomputes clean against disk. CN's rows (24–28: LuaUObject.cpp,
UObject.cpp, NameTypes.cpp, NameTypes.hpp, LuaUObject.hpp) and CL's rows
(20–23: FileDevice.hpp, Output.cpp, PosixFile.cpp, LuaLibrary.cpp) all verify;
their `changes[]` entries match their report tables exactly. CP row 29
(UObjectGlobals.cpp, keyed on l22-diagnostics per the CM convention, symmetric
files==changes) landed mid-check and verifies clean — final state 30 rows.

**files==changes consistency**: the arrays are NOT deep-equal — 22 rows differ
in `original`/`original_sha256` ONLY; candidate/copied_original fields are
identical in every row and the row sets align 1:1 by index and candidate.
Classification (verified, not guessed): this is the documented, intentional CM
`l26_row_sync` — files[] rows re-keyed on the identities the l25-A BASE build
consumed (incl. the two LuaUObject re-key notes), while changes[] retains the
per-agent provenance originals. Proof it predates CN/CL: the l25 CHANGES.json
is symmetric (files==changes, 20 rows, no l26_row_sync key); CN's and CL's own
9 rows are internally symmetric in both arrays; the 22 divergent rows are all
carried rows + CM's two documented LuaUObject re-keys. NOT concurrency damage.
Strict-equality consumers of files[]==changes[] will see the split (see §5.1).

## 2. Combined compile smoke (Z13 recipe /tmp/l25-combined-smoke/)

Environment passes: p1 = each owner's proven recipe (Z13 argvs repointed
l25-tree→l26-tree only; CL receipt argvs adapted per compile-matrix.sh; CN argvs
verbatim), p2 = merged l26 environment (l26 candidate include first via -I +
single CN overlay `/tmp/l26-harden-smoke/headers-l26.json`). Modes:
`-fsyntax-only` + `-c`. Variant B = `-DMODBRIDGE_L25_SKIP_DELETE_LISTENER`
(LuaMod, UnrealInitializer only). Host /usr/bin/clang++, `-D__PROSPERO__
-D__SCE__ -fshort-wchar`, Z13 stubs.

| TU group | units | compiles | result |
|---|---|---|---|
| CN touched (merged recipe) | lua-luauobject, unreal-uobject, unreal-nametypes | 6 (3×syntax+obj) | **6/6 PASS** |
| CL touched, p1 (CL recipe, l24 overlay) | output, posix-file, lua-library | 6 | **6/6 PASS** |
| CL touched, p2 (merged env) | same 3 | 6 | **6/6 PASS** |
| Siblings, p2 merged env, variant A | luamod, program, unreal-unrealinitializer, unreal-flat-typechecker, unreal-uobjectarray, unreal-flat-classlistener | 12 | **12/12 PASS** |
| Siblings, p2 merged env, variant B | luamod, unreal-unrealinitializer | 4 | **4/4 PASS** |
| Tree-source bonus, merged env | UObjectArray.cpp (real source) | 1 | **PASS** |
| Cross-agent canary (all 4 headers in one TU) | — | 1 | **PASS** |

**p1 sibling failures (12/12, expected class, NOT a CN/CL conflict)**: with only
`-I` prefixing of the l26 candidate include (no overlay mapping), `<Unreal/
NameTypes.hpp>` resolves to CN's candidate while port-root headers (e.g.
FPrimaryAssetId.hpp via a relative `NameTypes.hpp` include) still reach the
port-root copy → redefinition errors (NAME_SIZE, EFindName, ENameCase,
FNameEntryId, ...). This reproduces exactly CN's documented E3-lesson parent
obligation §5.1: the two new header replacements must reach all consumers via
the vfs overlay; `headers-l26.json` is the exact recipe. With it (p2), every
sibling compiles clean. Logs: `/tmp/l26-integrity/logs/*-p1-*.log`,
`*-p2fix-*.log`.

**No cross-agent ODR/record-layout conflict found**: all 12 TUs compile against
one identical merged header universe (CN's NameTypes.hpp + LuaUObject.hpp + CL's
FileDevice.hpp + carried headers), and a canary TU including all four headers
compiles rc=0 with every one resolving to the l26 candidate.

Disclosures:
- My first p2 runner mangled `-I` pairs (dropped a path value, left the flag),
  which made typechecker/unrealinitializer pick the stale l24-tree guard —
  harness bug, fixed; the corrected p2fix runs all pass. Logs kept for both.
- Per the Z13 recipe, luamod/uobjectarray argvs compile Z13's /tmp SHIMMED
  sources (host shims). Tree UObjectArray.cpp also passes directly; tree
  LuaMod.cpp non-shim host compile fails at l25 identically (same error,
  LuaMod.cpp:1396 std:: constructor) — pre-existing host limitation, the reason
  the shim exists; prospero is the authority for that unit.

## 3. Header routing (-H proofs, current tree)

| new header | representative consumer TU | rc | -H trace resolves to |
|---|---|---|---|
| Unreal/NameTypes.hpp (row 27) | unreal-nametypes (NameTypes.cpp) | 0 | `…l26-runtime/typescan-r4/candidate/include/Unreal/NameTypes.hpp` |
| LuaType/LuaUObject.hpp (row 28) | lua-luauobject (LuaUObject.cpp) | 0 | `…l26-runtime/typescan-r4/candidate/include/LuaType/LuaUObject.hpp` |
| DynamicOutput/FileDevice.hpp (row 20) | lua-library (LuaLibrary.cpp) | 0 | `…l26-runtime/typescan-r4/candidate/include/DynamicOutput/FileDevice.hpp` (depth 4; -I-order routed, no overlay entry needed) |

The `output` TU does not reach FileDevice.hpp under this host recipe —
consistent with CL's dep-universe note (lua-library carries the FileDevice edge;
16 dep-universe units consume the header). CL's harness internal -H proof also
re-confirmed candidate FileDevice.hpp for the fixed binary vs original for
orig/sim. Canary TU: all four headers route to l26 candidates, rc=0.

## 4. Behavioral spot re-runs (current merged tree)

- **CN `tests/actual_ufunc_harden.cpp`** (rebuilt from the current tree into
  /tmp): plain rc=0 `ALL L26 HARDENING FIXTURE CHECKS PASSED`; fatal
  ASan+UBSan rc=0, same. Still green post-merge.
- **CL logwriter harness** (build_and_run.sh copied to /tmp/l26-integrity/lw,
  VDIR repointed, original artifacts untouched; recompiles the REAL staged
  Output.cpp/PosixFile.cpp + port units from the current tree):
  - deployed-NUL sim: 847 corrupted lines, 79600 NUL bytes (console byte
    pattern reproduced); orig race: 173 corrupted (real race present);
  - **candidate: 0 corrupted, 400/400 [Lua], 800/800 [l22], 400/400 full
    tails, 0 NUL bytes**; parity serialized orig-vs-candidate byte-IDENTICAL
    (35640 B);
  - `VERDICT: … PASS`. (Corrupted-count deltas vs CL's original run — 847 vs
    925, 173 vs 284 — are race-timing variance; all pass criteria identical.)

## 5. Standing notes for the parent (non-blocking)

1. **files[]/changes[] split**: 22 rows now carry different `original`
   identities between the arrays (documented CM re-key). Any consumer assuming
   files[]==changes[] must treat files[] as the replacement authority or ask CM
   to mirror the re-key into changes[].
2. **E3 routing requirement, reproduced**: the l26 build MUST ship the two CN
   header mappings (port-root LuaUObject.hpp + NameTypes.hpp → l26 candidates)
   in its vfs overlay (`/tmp/l26-harden-smoke/headers-l26.json` is the exact
   recipe). A -I-prefix-only composition of the l26 tree fails 6 sibling TUs
   with NameTypes redefinition errors (CN §5.1 obligation, now with reproduction
   logs in /tmp/l26-integrity/logs/).
3. CP row 29 (UObjectGlobals.cpp FindFirstOf work) landed at 20:02:40 and pins
   verify; it was NOT part of the 12-TU smoke scope (in-flight per task) — the
   parent's l26 composition should re-smoke it with its own recipe.
