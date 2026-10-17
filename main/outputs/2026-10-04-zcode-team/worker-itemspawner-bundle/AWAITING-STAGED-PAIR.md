# AWAITING-STAGED-PAIR — the two layout keys this bundle deliberately does not carry

worker-itemspawner-bundle, 2026-10-04. This is the honest completion marker for
the actual bundle in `bundle/` (see `bundle.json` →
`layout_keys_awaiting_staged_pair`). **There are no synthetic bytes and no
placeholder bytes for these keys anywhere in the actual bundle.**

## The two keys

| Key | Role | Status |
|---|---|---|
| `ue4ss/MemberVariableLayout.ini` | config | **awaiting staged pair (no bytes)** |
| `ue4ss/VTableLayout.ini` | config | **awaiting staged pair (no bytes)** |

## Why they are required and why they are absent

- `UE4SSProgram::load_unreal_offsets_from_file()` reads BOTH keys from the mod
  source and **throws** `"Native activation requires both member and vtable
  layout files"` when either is absent (UE4SSProgram.cpp:899-904), inside
  `setup_unreal()` — i.e. BEFORE the engine scan and BEFORE any Lua runs.
- The bytes must be validated Townfall layout facts (BUNDLE-SPEC 1.5):
  identity `format=1`, `target=prospero-x86_64-sysv`, `pointer_size=8`,
  `engine_major>=4`, non-empty revision/build id, positive `UEP_TotalSize` per
  member class, offsets inside declared size, vtable offsets 8-aligned,
  accessor widths known. **No retained validated source contains these bytes**
  (worker-activation-bundle LAYOUT-GATE.md; worker-itemspawner-dryrun
  FIRST-RUN-CONFIG.md hard gate). This worker does not synthesize them.
- The `--layout-placeholders` / `stage-root-full` mode that exists in
  worker-activation-bundle's build is a HOST LOAD-PATH TEST FIXTURE ONLY. Its
  comment-only bytes are not layout facts and must never be shipped or staged.

## What completes the bundle (the moment the staged pair emits)

Source: `outputs/2026-10-04-zcode-team/worker-staged-pair-emission/EMISSION-RUNBOOK.md`
→ gated assembler → `$OUT/emission-live/`. Emission is REFUSED until the anchor
probe verdicts and real identity facts land; never hand-write a `Validated`,
never ship the `SYNTHETIC-PREPARATION-NOT-A-SCAN-FACT` bytes.

Completion steps (controller-owned, in order):

1. Verify the emitted pair is real: identity header present
   (`format=1`, `target=prospero-x86_64-sysv`, `pointer_size=8`)
   and EngineRevision/BuildId are REAL scan facts — not `SYNTHETIC-…`.
2. Copy the two emitted files into `bundle/ue4ss/` under their exact keys
   `ue4ss/MemberVariableLayout.ini` and `ue4ss/VTableLayout.ini`.
3. Append exactly two rows to `bundle/loadlist.txt` (TAB-separated) with the
   MEASURED byte sizes, then rewrite `loadlist.txt` last if re-staging:
   `file	config	ue4ss/MemberVariableLayout.ini	<size>`
   `file	config	ue4ss/VTableLayout.ini	<size>`
4. Update `bundle.json` (move both entries from
   `layout_keys_awaiting_staged_pair` into `files` with measured size + sha256).
5. Update this worker's host test T5 (currently asserts both keys are ABSENT)
   to instead assert both keys activate and match the emitted bytes, and re-run
   `sh run_host_test.sh`. A changed bundle must re-pass through
   `mfa_activate` before it is installable.
6. Then, and only then, follow INSTALL-PACKAGE.md for the console-side install.

Until step 1 lands, the bundle is otherwise complete and frozen; the only
remaining runtime-chain blockers are the layout data and the runtime chain
itself (acceptance ladder rungs).
