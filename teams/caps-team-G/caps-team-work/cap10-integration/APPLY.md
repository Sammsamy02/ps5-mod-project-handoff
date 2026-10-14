# APPLY — exact integrator steps for capability #10 (Team G)

## CURRENT (2026-10-13 rebase): apply on the live tree R16d + #11 follow-ups

This section supersedes the patch name and base in §0–§2 below; everything else
(markers §3, gate roots §4, corlib note §5, post-package check §5b, build §6)
is unchanged.

- Patch: `PATCH/cap10-only-on-live-R16d-fu.patch`
  sha256 `784baed517ca59e6354b09d1b5920445b78c279b264f5f34a1f6c4cd63bca629`,
  47 text files (43 new + 4 changed), `patch -p1` from the worktree root.
  Receipt: `PATCH/PATCH-RECEIPT-R16d-fu.json`.
- Binaries: the same 4 pinned files, `PATCH/binaries/outputs/…` (unchanged shas).
- Base shas the patch expects (check before applying; refuse on any mismatch):
  | file | live sha256 (at rebase) |
  |---|---|
  | `outputs/2026-10-08-l26-runtime/build_variants.py` | `37018de76572a6d3…` |
  | `outputs/2026-10-08-l26-runtime/l26_packaging_driver.py` | `0e2e2213c7a0df8d…` |
  | `outputs/2026-10-08-l26-runtime/typescan-r4/CHANGES.json` | `b0876aa27dc94956…` |
  | `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp` | `44499a0aaeb617c8…` |
  None of the 43 new files exists in the live tree (no `cap10/`, no `cap10_integration.py`).
- Drift handled (the #10 hunks themselves needed no change; 0 offset, 0 fuzz):
  - `build_variants.py` (after7 `1942e11a` → live `37018de7`): the reviewed #7 changed only
    the `pakmount` NEW_UNIT source pin, away from every #10 hunk.
  - `typescan-r4/CHANGES.json` (after7 `3c1e8761` → live `b0876aa2`): new rows
    `r16_row_sync`, `r16b_row_sync`, `r16c_row_sync`, `cap11_fu_row_sync` and new
    LuaMod/UnrealInitializer files[] pins; #10 still changes only the UE4SSProgram
    files[] pin (`44499a0a` → `b6a68e3f`) and adds `cap10_row_sync`.
  - `l26_packaging_driver.py`, `UE4SSProgram.cpp`: not drifted.
  - The R16 / #11-follow-up files (LuaMod.cpp, PakRequestProducer.hpp, NativeGuiHost.hpp,
    NativeModHostCore.hpp, NativeGuiEngineBackend.inc, NativeModEngineServices.inc) are
    not touched by #10; the target-compiled `program` object is byte-identical before and
    after the rebase (`c97979a1`), so they do not reach it.
- Re-measured against R16d (`runtime-l26-A.o` `02c5f4ce`, PRX `3b4c202b`): raw undefined
  210 → 361 (+146 strong, +5 weak, −0), 0 `sceNet*`, link inputs 401, `.ctors` 1896 B
  unchanged, no TLS, the 17 fixed link inputs identical, R16d's 207 identities equal
  the pinned baseline set. So G6 401 / G7 361 / P7 350 / P8 240-2 stand.
- Re-pinned content (not hunks): `cap10/ADMITTED-IMPORTS.json` (measurement note),
  `cap10/INTEGRATION-INPUTS.json` (its pin, `094f6120…`), `CHANGES.json`
  `cap10_row_sync.cap10_inputs`. Regenerate with `tools/refresh_pins_r16.py` if the
  tree moves again (base build for the tools: `CAP10_BASE_RUNTIME_SHA`/`_BYTES`).
- Apply:
  ```
  cd <worktree root>
  shasum -a 256 <the 4 base files>        # must equal the table
  patch -p1 --dry-run < .../PATCH/cap10-only-on-live-R16d-fu.patch && patch -p1 < .../PATCH/cap10-only-on-live-R16d-fu.patch
  rsync -a .../PATCH/binaries/outputs/ outputs/
  ```
  Then §3 markers, §4 gate roots, one build (§6), the §5b post-package check, and the launch card.

---

#10 is **last** in the queue and goes on top of **R15e + #7 step 2**. Do this
only after #7 step 2 has its own build and launch. One build, one launch.

## 0. Inputs (all under this folder, `caps-team-G/caps-team-work/cap10-integration/`)

- `PATCH/cap10-only-on-R15e+cap07.patch` — 47 text files, `-p1` from the worktree root.
  (sha in `PATCH/PATCH-RECEIPT.json`; regenerate with `tools/refresh_pins.py` after any fix.)
- `PATCH/binaries/outputs/2026-10-08-l26-runtime/cap10/…` — the 4 pinned binaries
  (the text patch cannot carry them):
  - `lib/libmonosgen-2.0.a` `24eebde5…` (the **v2** runtime, NOT the historical `38d03421…`)
  - `lib/mscorlib.full-interp.ps5.o` `a9f0a856…`
  - `managed/Probe.dll` `c328dca1…`
  - `runtime-assets/lib/mono/4.5/mscorlib.dll` `86364b78…`
- The accepted signal pair stays together and unchanged: `MonoOwner.cpp` `03edcc27…`,
  `MonoOwner.hpp` `8121c4ce…`, `libmonosgen-2.0.a` `24eebde5…`.

## 1. Rebase check

The patch was built on **live R15e sources + `intake/.../cap07-only-on-R15e.patch`**.
Before applying, confirm the one drift-prone pin still holds:
- `typescan-r4/candidate/src/UE4SSProgram.cpp` is at `4fab1e61…` **after** #7
  applies (GPT's base) — Team G's patch rewrites it to `b6a68e3f…`.
If the tree moved (a reviewed #7 landed differently, or other caps merged),
re-apply onto that tree in a scratch copy and re-run `tools/refresh_pins.py`,
which rewrites the `UE4SSProgram.cpp` candidate sha, the `cap10_row_sync`
record, every cap10 pin in `INTEGRATION-INPUTS.json`, and the patch itself.
`tools/target_compile.py` and the host tests must be re-run after any rebase.

## 2. Apply

```
cd <worktree root with outputs/>
patch -p1 < .../cap10-integration/PATCH/cap10-only-on-R15e+cap07.patch
# copy the 4 binaries (exact shas) into the live tree:
rsync -a .../cap10-integration/PATCH/binaries/outputs/ outputs/
# verify the 4 shas + the signal pair (PATCH-RECEIPT.json lists them)
```
There is one cosmetic reject to expect in `typescan-r4/CHANGES.json` if the live
file's trailing-newline state differs — it is the EOF-only hunk INTAKE flagged;
the real content hunks apply. Never copy a staged `CHANGES.json` over the live
one (it would revert R15e's `integrator_change_slate_units` note and the
`9ade8394` pin); always apply hunks.

## 3. Markers (before the build)

Add these to `outputs/2026-10-08-l25-prep/harvest/markers-declared.json`
(schema `markers-declared/1`). `diagnostic_event` tags land UTF-8; the `[CS]`
lines are wide `Output::send` (UTF-16LE). Ready-to-append rows:

```json
{"id":"cap10-stage","canonical_marker":"CAP10_STAGE","required":[{"text":"CAP10_STAGE","encodings":["utf-8"],"source":"cap10/native/MonoOwner.cpp emit_result"}]},
{"id":"cap10-image","canonical_marker":"CAP10_IMAGE","required":[{"text":"CAP10_IMAGE","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-resolve","canonical_marker":"CAP10_RESOLVE","required":[{"text":"CAP10_RESOLVE","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-run","canonical_marker":"CAP10_RUN","required":[{"text":"CAP10_RUN","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-throw","canonical_marker":"CAP10_THROW","required":[{"text":"CAP10_THROW","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-recover","canonical_marker":"CAP10_RECOVER","required":[{"text":"CAP10_RECOVER","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-done","canonical_marker":"CAP10_DONE","required":[{"text":"CAP10_DONE","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-cookie1","canonical_marker":"CAP10_COOKIE1","required":[{"text":"CAP10_COOKIE1","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-cookie2","canonical_marker":"CAP10_COOKIE2","required":[{"text":"CAP10_COOKIE2","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-r1","canonical_marker":"CAP10_R1_CALLS","required":[{"text":"CAP10_R1_CALLS","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-r2","canonical_marker":"CAP10_R2_CALLS","required":[{"text":"CAP10_R2_CALLS","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-t","canonical_marker":"CAP10_T_CALLS","required":[{"text":"CAP10_T_CALLS","encodings":["utf-8"],"source":"MonoOwner.cpp"}]},
{"id":"cap10-net-refused","canonical_marker":"CAP10_NET_REFUSED","required":[{"text":"CAP10_NET_REFUSED","encodings":["utf-8"],"source":"cap10/native/Cap10OfflineNet.cpp"}]},
{"id":"cs-probe-requested","canonical_marker":"[CS]","required":[{"text":"[CS] C# probe requested by an enabled C# mod: starting the C# runtime","encodings":["utf-16le"],"source":"candidate/src/UE4SSProgram.cpp"}]},
{"id":"cs-not-started","canonical_marker":"[CS]","required":[{"text":"[CS] C# runtime not started: no enabled C# mod requested it","encodings":["utf-16le"],"source":"candidate/src/UE4SSProgram.cpp"}]}
```

## 4. Pipeline gate roots (INTAKE §5 + Team G)

Add to the no_detour gate call in `run_labelled_build.sh`:
`--root outputs/2026-10-08-l26-runtime/typescan-r4/include --root outputs/2026-10-08-l26-runtime/cap10/native`.
(field_gate and fs_throw already scan the candidate tree; the cap10 code adds no
field reads and no throwing fs calls — verified.)

## 5. Build-composition choice (important — the 13.4 MiB corlib)

Making Mono init **lazy** keeps Mono from *running* with no C# mod, but it does
**not** remove the Mono archive + static corlib from a PRX that compiles #10 in:
the owner references `mono_*` and `mono_aot_module_mscorlib_info`, so the linker
pulls them (≈13.4 MiB corlib + Mono members, resident all session). That is
acceptable for the #10 launch. It must **not** ride into other builds:
- The #10 patch is **additive and self-contained**. Any build that does **not**
  apply it has zero Mono (no define, no units, no artifacts, imports back to 207).
- Therefore build Stellar Blade and the clean rollback **without** the #10 patch.
  Do not carry #10 forward into the Stellar Blade blind test.

## 5b. Post-package check (review finding 2)

After packaging, confirm the PRX still exports exactly `module_start` and
`module_stop` (as R15e) and has **0 `sceNet*`** rows in either direction (no
import, no export). The offline stubs end up default-visibility in the target
object (toolchain flag), which is harmless only while the packager exports
nothing else.

## 6. Build once, then launch

Run the gates, build once with `run_labelled_build.sh`. Expect:
G6 census **401**, G7 the declared cap10 raw delta, P7/P11 **350** identities
(== R15e 207 + the 143 admitted rows), P8 **240/2**. Then follow `LAUNCH-CARD.md`
for the console steps (each needs the user's OK).

## 7. Design option for later (not implemented): Mono as its own PRX

**Idea.** Package owner + boundary + offline-net + static corlib + Mono archive
as a separate module, loaded on demand by the #9 C++-mod loader
(`sceKernelLoadStartModule`, nested load from module_start is console-proven by
#9) only when a C# mod is enabled. The main runtime would stay at R15e's 207
imports, 240/2 constructors and current size in every build.

**Feasibility:** yes in principle. #9 already loads PRXs and hands them a
validated host-services table (ABI v1, `mbm_mod_entry_v1`); the Mono module
could be an ABI-v1 module whose entry runs the probe. GPT's owner resolves the
probe's two native exports through same-image function pointers, so those
exports and the observation window would move into the Mono module with it.

**Blockers:**
1. Packaging: the pipeline makes exactly one PRX (package_prx_l19.py, 20 static
   checks, twins, verify_build manifest, decrypt-off readback, upload rules). A
   second PRX needs all of that, plus its own fixed import list (the 143) and its
   own G6/G7/P7/P8 contracts.
2. A second static libc/libc++ copy in the process (own heap, own emutls keys,
   own constructors); has to be reviewed for coexistence with the main runtime.
3. Call-backs into the main runtime: today `UE4SSProgram` owns the admission
   scope and `diagnostic_event` lives in the bootstrap. Both must reach the
   module through the ABI-v1 services table, not direct symbols (R15e exports
   only module_start/module_stop).
4. Placement and discovery: the #9 loader wants a mod folder with `dlls/`; the
   Mono module needs a generic runtime location under the title root and a
   matching rule (no mod names).
5. Signal ownership and the runtime-file checks stay as they are, just moved.

**Rough effort:** about 3–5 Mac days (split units, ABI-v1 entry shim, second
packaging configuration with its gates and tests; packaging is the largest
part), then one review and one console launch. The upside: #10 no longer
changes the main runtime at all.
