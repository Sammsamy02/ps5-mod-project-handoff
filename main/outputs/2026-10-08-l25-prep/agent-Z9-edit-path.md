# Agent Z9 — sanctioned edit path for l25 listener files (READ-ONLY determination)

Date: 2026-10-08. No file outside this report was written. All paths relative to
`/WORKSPACE/.codex/worktrees/translation-core/what/` unless absolute.

VERDICT UP FRONT: **mechanism (a) — agent P's staged pin-identical pattern in a
consumed part (`typescan-r4`), with the row's `original` pinned to the EXACT path
the l24 build actually consumed.** Not (b), not (c). One critical exception the
checklist must honor: **LuaUObject.hpp's consumed root is the l22-diagnostics copy,
NOT the port root.**

---

## 1. What build_runtime.py actually consumes

`outputs/2026-10-08-l25-runtime/build_runtime.py` (BASE=`outputs/2026-10-08-l24-build-r2`,
OUT=`outputs/2026-10-08-l25-build-r2`):

1. **The l24 composition, not the port root.** It never walks the port tree. It reads
   `BASE/SOURCE-SELECTION.json` and `BASE/pass{1,2}/COMPOSITION-RECEIPT.json`
   (374 units each; unit = name, source, source_sha256, argv, dependency .d, object).
2. **Changes come ONLY from the hardcoded parts list** (line 38):
   `for part in ['profile','typescan-r4','thread']:` → `<part>/CHANGES.json` keys
   `files` (rows), `added_headers`, `include_directories`. **`copyin-probe/` is NOT in
   the list** — nothing there is ever read by build_runtime.py.
3. **Row contract** (lines 41-47): for each row, `original` must still hash to
   `original_sha256`, `candidate` must hash to `candidate_sha256`, `candidate` must be
   under `outputs/2026-10-08-l25-runtime/` (`candidate.is_relative_to(HERE)`), and the
   same `original` may not map to two different candidates (`copied_original` is
   audit-only; the legacy `changes` key is not read either — only `files`).
4. **One-hop replacement model** (agent Q's term, WIRING.md + build_runtime.py:41-47,
   :106): `replacements[original]=candidate` is resolved ONCE per unit via
   `replacements.get(str(Path(u['source']).resolve()))`. There is no chaining: a row
   whose `original` is an l25 candidate (second hop) is dead; a row whose `original` is
   a path no l24 unit compiled (e.g. the port-root `UnrealInitializer.cpp`) is a
   **silent no-op**. Headers ride the VFS overlay instead: rows with a header
   `original` enter `header_specs` as `(basename, original_sha256, candidate)` and
   alias EVERY file in the l24 dependency universe — and every inherited
   `-ivfsoverlay` target — matching that **(name, digest)** pair (lines 56-59, 81-88).
5. **Assertions, loud vs silent:**
   - LOUD: all 14,646 source + 408 receipt + 403 artifact pins of l24 must still match
     (line 36) — this is why a port-root edit (option c) kills the build outright;
     per-unit `source_sha256`/`object_sha256` and `.d` pins (lines 51, 104); duplicate
     original→different candidate (line 45); `unapproved include graph additions`
     (lines 115-116: a newly-seen include must live under l25-runtime or be an
     already-pinned (name,sha) header); 391 link inputs, each object once, fixed-input
     pins (lines 137-139); **undefined-symbol set must equal the l24 baseline**
     (lines 143-144 — any new import fails the build); runtime twins byte-identical
     and per-unit object hashes equal across passes (lines 152-154); fresh
     `OUT.mkdir(exist_ok=False)` (line 32).
   - SILENT: a row that hashes fine but keys on a root the l24 build did not consume
     — the unit is simply `retained` (line 105: `if u['name'] not in affected: return
     u,None`), the build succeeds, and the fix is absent.
6. Agent R's real driver `outputs/2026-10-08-l25-runtime/build_variants.py` mirrors the
   same parts list and asserts (its `compute_selection`), builds variants A (normal)
   and B (`-DMODBRIDGE_L25_SKIP_DELETE_LISTENER` on the token's translation units),
   and is the ONLY consumer of `copyin-probe/`: it composes Q's patch as a second hop
   in its own staging dir on top of the CURRENT typescan-r4 candidate (or adopts Q's
   ready-made candidate if the base is still byte-identical; no-fuzz/no-rej enforced;
   plan mode `tolerate_drift=True` only REPORTS pin drift, `--execute` hard-fails).

## 2. Where the l24 build got each file (from `2026-10-08-l24-build-r2/pass1/COMPOSITION-RECEIPT.json` + dep-universe scan)

| File | Consumed by l24 from (sole occurrence in dep universe) | sha256 (head) | Units touching it |
|---|---|---|---|
| UnrealInitializer.cpp | `outputs/2026-10-08-l24-runtime/typescan-r3/candidate/src/UnrealInitializer.cpp` | 78c16fb7… | 1 (`unreal-unrealinitializer`) |
| UObjectArray.cpp | PORT ROOT `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UObjectArray.cpp` | 171560a9… | 1 (`unreal-uobjectarray`) |
| UObjectArray.hpp | PORT ROOT `…/port/unreal/include/Unreal/UObjectArray.hpp` | b01bf4ef… | 162 |
| ClassListener.hpp | PORT ROOT `…/port/unreal/include/Unreal/ClassListener.hpp` | 94b4274a… | 3 |
| ClassListener.cpp | PORT ROOT `…/port/unreal/src/ClassListener.cpp` | a579b272… | 1 (`unreal-flat-classlistener`) |
| LuaUObject.hpp | **`outputs/2026-10-08-l22-runtime/diagnostics/include/LuaType/LuaUObject.hpp`** — NOT the port root | 1b6e2ab7… | 33 (`lua-luauobject` + 32) |
| LuaUObject.cpp | `outputs/2026-10-08-l22-runtime/diagnostics/src/LuaUObject.cpp` | d31f4e51… | 1 (`lua-luauobject`) |
| UnrealInitializer.hpp | PORT ROOT `…/port/unreal/include/Unreal/UnrealInitializer.hpp` | 4d5f1d84… | 33 |

The port-root `LuaUObject.hpp` (sha 89cb7390…) is pinned in l24 history but consumed
by NO unit: both inherited overlays (`2026-10-08-l22-build-r3/headers.vfs.json` and
`2026-10-08-l24-build-r2/headers.vfs.json`) redirect the port path to the
l22-diagnostics copy. Port root currently matches every l24 pin (verified — pristine).

## 3. Sanctioned mechanism for the listener changes (= Q3 answer)

**(a) — stage into `typescan-r4` exactly as agent P did**, because:

- build_runtime.py:38 + build_variants.py:77 make `typescan-r4/CHANGES.json` `files`
  rows the ONLY source-level input; line 106 applies them one-hop to the l24 unit
  sources; lines 56-59/81-88 apply header rows by (name, digest) across the whole dep
  universe and inherited overlays — one row per header recompiles every consumer.
- Option (b) (copyin-probe-style overlay dir) is not a mechanism: parts are hardcoded;
  Q's dir works only because R's build_variants.py special-cases it (`compose_copyin_probe`).
  A new overlay dir for the listener files would be silently dead.
- Option (c) (port-root edit) is prohibited and self-defeating: line 36 (and G1 in
  build_variants.py) asserts every l24 pin still matches — the port files ARE pinned
  there — so the build fails before compiling anything, and it would mutate history
  that l22/l23/l24 receipts rely on ("no historical header or source is edited in
  place", build_runtime.py:53-54).

**The one rule that makes or breaks it: the row's `original` must be the path (and
digest) from section 2, i.e. what the l24 build consumed — which for LuaUObject.hpp is
the l22-diagnostics copy, and for UnrealInitializer.cpp is the typescan-r3 candidate,
NOT the port-root files that also exist.**

## 4. Divergence risks found

1. **SILENT — wrong-root LuaUObject.hpp row (highest risk).** A row keyed on the port
   root (89cb7390) passes every assert (file exists, hash matches pin) but aliases
   nothing: the only dep-universe copy is 1b6e2ab7. Build green, fix absent. Same
   silent shape for any row whose `original` is not an l24 unit source or dep-universe
   path (e.g. port-root `UnrealInitializer.cpp`).
2. **SILENT — overlay-dir orphaning.** Anything under a non-part directory
   (`copyin-probe/` itself, or any new overlay dir) is invisible to build_runtime.py.
3. **Live drift window (observed).** During this audit agent O edited
   `typescan-r4/candidate/src/UnrealInitializer.cpp` (14:37:26) and updated the row
   (14:38:05); mid-window the row pointed at c4364dd7 while disk held efdef253 — a
   build in that window dies at line 43 (loud, but coordinated builds must snapshot).
   Row/candidate updates must land together; single owner per file.
4. **Q's patch rebase point.** Q's copyin-probe candidate/patch is based on c4364dd7;
   O's candidate is now efdef253. R's driver auto-rebases (strict no-fuzz/no-rej) or
   hard-fails in `--execute` demanding a manual rebase. Adopting Q's ready-made
   125a0471 file as-is would REVERT O's listener edits — must not be copied over.
5. **Duplicate-original rule.** Same `original` in two parts with different candidates
   → loud assert (line 45). Keep one owner per original across profile/typescan-r4/thread.
6. **Loud-but-planned failures to expect:** new includes must live under l25-runtime or
   be already-pinned (name,sha) (line 116); undefined-symbol set must equal baseline
   (line 144 — `diagnostic_event` is already in the baseline, so diagnostics-only
   additions are safe); 391-input link shape; fresh OUT dirs.
7. **Header alias is name+digest only.** UObjectArray.hpp/ClassListener.hpp/
   UnrealInitializer.hpp each have exactly ONE consumed copy, so one row each is
   complete; LuaUObject.hpp is the only two-content name (see risk 1).
8. **Retained-unit false comfort.** Units not in `affected_units` keep their l24
   objects by design; verify `unreal-uobjectarray`, `unreal-flat-classlistener`,
   `unreal-unrealinitializer`, `lua-luauobject` all appear in the produced
   `CHANGE-SELECTION.json` `affected_units` before packaging.
9. **build_variants.py plan mode tolerates drift** (`tolerate_drift=True` reports G2
   failures without stopping) — a green plan is NOT pin health; only `--execute` enforces.

## 5. Checklist for agent O (listener fix) and agent R (build)

Agent O — for each file, stage under `outputs/2026-10-08-l25-runtime/typescan-r4/`
(single owner; edit candidate + row in one step; keep `original`/`copied_original`
byte-exact; keep legacy `changes` key in sync; never touch port root, l22/l23/l24
outputs, copyin-probe/, or build_runtime.py):

1. `UObjectArray.hpp` — DONE (row exists): original=port root `…/port/unreal/include/Unreal/UObjectArray.hpp`
   (b01bf4ef) → candidate `typescan-r4/candidate/include/Unreal/UObjectArray.hpp` (69a2391a). Reaches 162 units.
2. `UObjectArray.cpp` — DONE: original=port root `…/port/unreal/src/UObjectArray.cpp`
   (171560a9) → candidate (668b2682). Unit `unreal-uobjectarray`.
3. `ClassListener.cpp` — DONE: original=port root `…/port/unreal/src/ClassListener.cpp`
   (a579b272) → candidate (a8e9b6f4). Unit `unreal-flat-classlistener`.
4. `UnrealInitializer.cpp` — DONE (row updated to efdef253): original MUST stay
   `outputs/2026-10-08-l24-runtime/typescan-r3/candidate/src/UnrealInitializer.cpp`
   (78c16fb7); do NOT re-key to the port root or to an l25 path (both are silent no-ops).
5. `ClassListener.hpp` — PENDING: original=port root `…/port/unreal/include/Unreal/ClassListener.hpp`
   (94b4274a) → `typescan-r4/original/…` + edited `typescan-r4/candidate/include/Unreal/ClassListener.hpp` + row.
6. `LuaUObject.hpp` — PENDING, CRITICAL: original=**`outputs/2026-10-08-l22-runtime/diagnostics/include/LuaType/LuaUObject.hpp`
   (1b6e2ab7)** (copy pin-identical into `typescan-r4/original/include/LuaType/`),
   candidate under `typescan-r4/candidate/…`. NEVER key this row on the port-root copy.
7. If new headers are introduced: list them in `added_headers` (+ `include_directories`
   if needed) so line 98 adds their `-I` dirs; new includes must otherwise be
   already-pinned headers. Keep the variant-B token `MODBRIDGE_L25_SKIP_DELETE_LISTENER`
   spellings exactly as build_variants.py expects.

Agent R:

1. Do NOT edit part trees; verify every `files` row matches disk (both `original_sha256`
   and `candidate_sha256`) and that no duplicate-original conflicts exist across
   profile/typescan-r4/thread; confirm rows 5-6 above landed before the final build.
2. Confirm the four listener units appear in `affected_units` of the produced
   CHANGE-SELECTION.json (esp. `lua-luauobject` — proves the LuaUObject.hpp row was
   keyed on the l22-diagnostics root, not the port root).
3. Run `build_variants.py` (twin variants; Q's copyin-probe composed automatically —
   accept `patch-rebased-onto-revised-base`, never overwrite the typescan-r4 candidate
   with Q's 125a0471). `--execute` enforces the hard pins; plan mode only reports drift.
4. Use fresh OUT dirs (`l25-build-r2` must not exist for build_runtime.py; variant dirs
   must not exist for build_variants.py); expect the import-delta assertion to hold
   (diagnostics-only additions use `diagnostic_event`, already in baseline).
5. Bridge-side copyin-probe changes ride packaging (`l25_packaging_driver.py` outer
   bridge substitution), never the runtime-object build.
