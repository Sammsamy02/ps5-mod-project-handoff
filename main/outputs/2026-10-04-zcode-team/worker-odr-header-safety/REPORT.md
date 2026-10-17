# REPORT — worker-odr-header-safety (composition-6 ODR hazard audit)

**Verdict: the comp6 design is ODR-sound ONLY as a complete set — `program` must be
replaced, and the D2 ctor-gate headers must land IN PLACE (the drybuild intercept
chain provably does not reach the construction site). Additionally, the dry1 PRX
candidates embed a real mixed-vintage by-value/sret ABI mismatch and must never be
launched. Full tables and evidence in ODR-SAFETY.md.**

## What was audited

- Baseline: composition-4 run-3 (status `NATIVE_…_COMPOSED`), 368 units = 15 fresh
  + 353 retained. All 368 `dependency_file` walks read (0 missing) — the retained
  units' walks come from their original build receipts, which is the correct
  closure for "what they were compiled against".
- Headers: `NativeProgramContext.hpp`, `NativeLoaderContext.hpp`,
  `lmf_to_native_loader_facts.hpp`, `PsMappedImageCaller.hpp`,
  `OwnedMappedImageCaller.hpp`.
- 24 of 368 units include at least one audited header; the other 344 retained
  units have zero contact.

## Headline answers to the four tasks

1. **Hit list** (full table in ODR-SAFETY.md §1): NativeProgramContext.hpp — 24
   units (14 retained incl. `program`, 10 fresh); NativeLoaderContext.hpp — 3, all
   fresh; lmf_to_native_loader_facts.hpp — 1, fresh; D2 headers — 9 (8 fresh + the
   retained collector). The design claim "retained units never name
   NativeLoaderFacts" is **VERIFIED** against all 353 retained walks.
2. **Classification**: 6 affected = FRESH; `program` = REPLACED-in-comp6 (design
   §3.2 — must actually happen: `UE4SSProgram.cpp` on disk is still the old
   vintage); everything else RETAINED. The 13 non-program retained units including
   NativeProgramContext.hpp are **BENIGN**: 12 never name the type at all (include
   is transitive via UE4SSProgram.hpp), and the collector only uses D2 inline
   functions whose bodies comp6/D2 do not change. `luamod`/`settings`/
   `program-signatures` odr-use inline `native_mod_source()`, which is offset-safe
   ONLY because both new fields insert after `mod_source` — recorded as a
   load-bearing invariant with a post-build emitter check.
3. **D2 audit**: Diff B/C are code-only (no layout change). The sole construction
   site in the composition is `OwnedMappedScanSource.cpp:10,15` (fresh
   `native-ownedmappedscansource`). **The drybuild staging-d1 intercept chain does
   NOT reach it**: dry1 run-2 `.d` shows only `lmf-loader-adapter` resolving the
   patched headers; the ctor site resolves the frozen old satellite copies (and
   14/15 fresh units were bit-identical to run-3). In that shape the new
   text_base=0 flow would still hit the OLD ctor throw at runtime — D2 silently
   defeated, plus a strict ODR violation. Diff B/C must land in the satellite
   headers themselves (read-lease variant too).
4. **Remediation** (ODR-SAFETY.md §6): 7 items — land `UE4SSProgram.cpp` + in-place
   D2 headers, one-vintage pin regeneration, port-vs-overlay sha gate, extend the
   `.d`-resolution proof to all 15 fresh TUs, symbol-emitter checks, and quarantine
   of the dry1 candidates.

## LOUD findings

- **FINDING-1 (do-not-launch):** the dry1 PRX candidates
  (`prx-candidate-composition6-dry1-a/-b`) mix vintages across the adapter↔bridge
  by-value boundary (`LmfConversionResult` sret + `LmfConversionOptions` const-ref,
  `NativeLoaderFacts` by-value): adapter = staging-d1 new headers, bridge = overlay
  old headers (`18c76cdc…`/`a5bc3b70…`). That is an out-of-bounds sret write waiting
  for a target run; static gates cannot see it. Dry1 was offline-only
  (`target_executed: False`, verified) — keep it that way.
- **FINDING-2:** D2 ctor gates are not compiled into their only odr-user under the
  drybuild staging shape (evidence above).
- **FINDING-4:** `program` is the single largest stale-layout contact (stale
  `ValidateNativeProgramContext` — run-3 map shows program.o as its only emitter —
  plus stale `m_native_context` derivations and the by-value ctor). The design's
  replace-mechanics already own this; it is restated as blocking.
- The landing moved under this audit (port header + W/include + adapter/loadercontext
  sources changed 21:41–21:42): current tree has the new vintages landed for the
  W/include and port-header files, while `UE4SSProgram.cpp`, both satellite D2
  headers, and the read-lease variant are still old, and several frozen pins still
  name old vintages. The one-pass build must not start until §6 items 1–3 are done.

## Method notes / limits

- Evidence: run-3 + dry1 receipts and `.d` walks, runtime.map emitter extraction,
  `nm -C` on fresh objects, source greps over the canonical trees (port,
  notify-cancel, tick-file-owner, jmap, collector, candidate-selection, staging
  copies). No console contact, no nested subagents, no writes outside
  `outputs/2026-10-04-zcode-team/worker-odr-header-safety/` (CLAIM.json first,
  ODR-SAFETY.md, this REPORT.md last). ~40 tool calls.
- No retained object was disassembled byte-for-byte; the benign verdicts for the 12
  port TUs rest on zero-source-use greps plus the weak-symbol map evidence. If any
  of those TUs were later found to odr-use an inline function through a macro or
  template not visible to grep, the emitter-set check in §6 item 6 would catch it
  post-build.
- Scores: N/A (audit, not a quality-domain rating). No completion or parity claims.
