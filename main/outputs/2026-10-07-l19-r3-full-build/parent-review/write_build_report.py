#!/usr/bin/env python3
"""Render the final report from completed, verified local build receipts."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[3]
OWN = Path(__file__).resolve().parent.parent
def load(p): return json.loads(Path(p).read_text())
def link(label, p): return f'[{label}]({Path(p).resolve()})'
pipeline = load(OWN/'FULL-PIPELINE-RECEIPT.json')
assert pipeline['pass'] and pipeline['composition_deterministic'] and pipeline['prx_deterministic']
src = ROOT/'outputs/2026-10-07-l19-local-profile-r3'
focused = load(src/'reports/FINAL-RECEIPT.json')
assert focused['pass']
first, second = pipeline['builds']
unknown = pipeline['unresolved_profile_facts']
assert len(unknown['members']) == 2 and len(unknown['vtables']) == 2
report = f'''# l19 R3 build report

**Mac-only: profile fix built and packaged twice with identical output. Upload HOLD. Full layout resolution remains incomplete; this build is not ready to show the original mod's menu or spawn an item.**

No console, account or network contact occurred. No mod, configuration, game executable, system or kernel file was changed. The console remains on l18; its saved pool admission and ByteProperty round-trip are earlier console evidence, not l19 results.

## Artifact and pipeline

- PRX: **{second['bytes']:,} bytes**, SHA256 **`{second['sha256']}`**.
- SELF magic `{second['magic']}`; SDK `{second['sdk']}`.
- Composition: {second['composition_bytes']:,} bytes; SHA256 `{second['composition_sha256']}`.
- Both linked compositions and packaged PRXs match byte-for-byte. Each has {second['selected_units']} selected compilation units and {second['inputs']} hash-checked composition inputs.
- Final artifact gates pass; each package passes {second['static_checks']['count']} static checks. The first candidate's initially pending twin gate was settled by the second build.
- All **206 typed imports** are unchanged against saved final l18: no added/removed names, changed symbol types, libraries or module identities.
- Exact strong layout provider/consumer linkage, actual helper dependencies, original LuaMod object comparator, ODR, entry exports, receipt coverage and full selected-source/object hashes remain checked.

Receipts: {link('full pipeline',OWN/'FULL-PIPELINE-RECEIPT.json')}; {link('import delta',OWN/'IMPORT-DELTA-VS-L18.json')}; {link('independent source review',ROOT/'outputs/2026-10-07-l19-final-audit/r3/REVIEW_FINAL.json')}.

## Profile and collect-all behavior

Shipping comes from the authenticated PT_SCE PATH ending `Townfall-PS5-Shipping.self`. Stats-off comes from the independent saved-image detector; Shipping never implies Stats-off. Both bind to image SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, the expected base, native target, engine revision and build identity. Wrong, missing or conflicting declarations still refuse before later engine calls.

The final {link('per-game profile',src/'profiles/townfall-build-facts.json')} supplies **67/69 required member values and 5/7 engine slots**. The {link('complete fact table',OWN/'inputs/final-report-documents/FACT-TABLE.md')} preserves all 125 original inventory rows in 138 joined rows, with current values, evidence, confidence, supply status, and dormant/dynamic distinctions. Its evidence join checks 1,603 native instruction windows. It includes the newly proved `UClass.Interfaces=0x1D8`/16 and `UEngine.Tick` slot `0x310`/8.

All **54 required non-total accessor widths** now have compiler-checked providers. Ten active public-getter types handle unavailable or inappropriate legacy types. Native FField Owner storage remains 8 bytes; the existing port's by-value wrapper copies 16 bytes, so staging uses that conservative access footprint. This does not establish complete native Variant ABI equivalence or future game behavior.

## Remaining refusals

| Required target fact | Status |
|---|---|
| Exact FField total | Unproved; 0x30 candidate not promoted |
| Exact FFieldClass total | Unproved; 0x40 placement/extent candidate not promoted |
| UEngine.LoadMap slot | Unproved |
| AGameModeBase.InitGameState slot | Unproved |

Saved declarations still contain only 17 member values and one virtual slot: **52 required member declarations and 6 virtual declarations are absent**. These counts overlap the four unknown target facts; they are separate completeness checks, not extra native discoveries. No expanded configuration was generated or installed. Host integration reproduces refusal for this saved case and logs all failures before throwing.

## Focused verification

- {focused['actual_initializer_scenarios']} extracted actual initializer cases; {focused['actual_binding_host_scenarios']} actual binding/parser cases.
- All 54 target width values and native minimum-width checks; ten public getter controls; selected initializer/binding target objects with the exact undefined/strong provider pair.
- {focused['schema_negative_scenarios']} schema refusal cases; profile plain and fatal ASan/UBSan runs.
- Independent review freshly executed 56 profile/initializer/binding binaries, all passing. The earlier nine unchanged XOM/cache/copyin/FName/lifecycle suites are retained by exact 28 unit hashes, explicitly not claimed rerun.
- V1 and R2 freezes remain intact. R2 stopped before compilation at a missing output directory and has no full artifact claim. R3 creates the required output directories and retains all gates.

## Plain XOM answer

**No successful game-instruction write is identified as a prerequisite for the first menu/spawn.** Eager StaticConstructObject and deferred LoadMap inline-hook attempts occur on the path, but the current game-text guard refuses them before patching and the normal error path continues. Full original LoadMap reset behavior across maps remains unproved under XOM.

The required game-thread carrier changes a **UFunction.Func data pointer**, not game instructions. Its writable coverage, lifetime, real firing, input delivery, UMG rendering and spawn behavior remain unproved. {link('Full XOM source audit',OWN/'parent-review/XOM-AUDIT.md')} records exact callsites. Pad and code-verification paths are unchanged.

## Startup speed: analysis only

The saved l18 timeline places roughly **65 s before PRX entry**, medium confidence because kernel and user clock epochs are not explicitly equated. Entry to runtime acknowledgement is **4.082 s**. Cache **2.130 s** is included in admission **2.297 s** and FName **2.355 s**, rather than added to them. Later engine-readiness polls were not reached. Rendered popup timestamps and individual loader polls were not captured.

Ranked proposals: (1) prove deployed loader lineage, then enforce real wall-time-bounded mode-2 readiness; historical potential about 39.5 s per exhausted burn, medium risk; (2) reuse only proven stable dependency state for the second plugin, historical potential about 62–70 s, medium risk; (3) diagnostic batching has no isolated savings measurement and cannot explain a minute before entry; (4) no evidence justifies changing unreached engine polling or weakening image authentication. Mode 1 has high timing/dependency risk and is not recommended for this path. **No speedup was applied.**

{link('Full timeline',OWN/'inputs/final-report-documents/TIMELINE.md')}; {link('additional ranked analysis',OWN/'inputs/final-report-documents/TIMING-PROPOSALS-ADDENDUM.md')}.

## Next action

Complete the four authenticated native facts and prepare a complete profile-derived layout declaration with refusal tests before proposing a console launch. A packaged artifact is insufficient evidence for the menu, spawn, all Unreal games, or general DLL compatibility.

{link('Persistent progress',OWN/'PROGRESS.md')} contains verified results, unresolved issues and the next steps. Console changes remain on hold until a separate exact approval.
'''
destination = OWN/'BUILD-REPORT.md'
assert not destination.exists()
destination.write_text(report)
print(f'Build report written: {destination}; status Mac-only, upload HOLD.')
