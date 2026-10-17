# PER-VERSION-TABLE — UE4→5.6 layout table seeds (Blind-test prep, priority (a))

Agent GJ, 2026-10-08. Machine table: `per-version-table.json` (same dir).
Mac-only prep, battery mode: no builds, no console, no fabricated measurements.

## 0. What this table IS and IS NOT

- IS: the engine-version-keyed knowledge the profile GENERATOR consults when a
  new game's decrypted eboot is classified. Rule 10 law baked in: the table is
  keyed by engine version only — no game names, no per-game offsets, no
  image-sha special cases (a sha appears ONLY inside the measured 5.6 entry's
  provenance block, never as a matching key).
- IS NOT: a source of trustable offsets for any version except 5.6. Exactly
  ONE row is measured (5.6, on the Townfall image). Every other row is a
  TRAINING-KNOWLEDGE seed at confidence 0.0 carrying *scan hypotheses* +
  expected deltas + a confirm_when list. The generator must never EMIT a seed
  value as a pin; seeds only steer scanning, and emitted pins come solely from
  anchor/consensus measurement against the actual binary.

## 1. Coverage matrix (honest accounting)

| Version band | Status | Confidence | Values |
|---|---|---|---|
| **5.6 (5.6.1)** | **LOCAL-MEASURED** | 1.0 | full: FNamePool shape, GUObjectArray/FUObjectItem, FName ctor slot-2 pattern+ABI, ToStringInternal, SCO, listener ops, FMalloc vtable order, labels, image facts |
| 5.4-5.5 | seed | 0.0 | deltas + scan hypotheses |
| 5.0-5.3 | seed | 0.0 | deltas + scan hypotheses |
| 4.23-4.27 | seed | 0.0 | FNamePool-era UE4 deltas (ToString slot = UNRESOLVED-by-seed; FMalloc without Try* pair) |
| 4.12-4.22 | seed | 0.0 | pre-FNamePool TNameEntryArray deltas |
| 4.7-4.11 | seed | 0.0 | marker-table floor; likely FFixedUObjectArray |
| 5.7-5.8 | seed | 0.0 | forward rows so the version axis never falls off the table |

Measured-row provenance (all receipts local):
- Engine version pin: **UE 5.6.1-release**, Epic commit `6978b63c8951e57d97048d8424a0bebd637dde1d` — `outputs/2026-10-07-l19-r4-official-engine-source/SOURCE-RECEIPT.json` + `dependencies/` (Field.h/Field.cpp, NameTypes.h, ObjectMacros.h, CoreTypes.h…). Profile `engine_revision` "5.6.1-0+++UE5" + axes classification "5.6" agree; the vendored official-source receipts ARE the local engine headers that pin the minor version.
- Labels/pins: `outputs/2026-10-08-l26-package-A.preDA/lineage/SeedTable.h` (slots 0/2/3/4 + validators), `RuntimeBuildProfile.hpp` (69 members + 24 vtables), `CodeSignatureProfile.hpp` (6 code pins), `EngineToStringProfile.hpp`, `EngineListenerOpsProfile.hpp`, `FNameAbiProfile.hpp`.
- Pool decoder shape: agent-ER pool decoder (block=id>>16, offset=(id&0xFFFF)*2, pool+0x8/+0xC cursors, +0x10 Blocks[] u64 stride 8, u16 header wide=bit0/len=>>6, block capacity 0x20000, 8192 max blocks, EName table +0x14040).
- FMalloc order: l21 allocator candidate-table (17 measured slots, listed byte offsets; slots 0-47/88/96/136 explicitly UNMEASURED — never pin).

## 2. Key generalized patterns (scan seeds; the only "portable" bytes)

FName ctor wchar slot (from the Townfall 5.6 32-byte prefix, wildcards on the
two short-jump displacements; see JSON for exact + wildcarded forms):

```
55 48 89 E5 53 50 48 89 F0 48 89 FB 48 85 F6 74 ?? 0F B7 38 48 89 C6 31 C9 66 85 FF 74 ??
```

Semantics that make it a ctor detector on ANY UE version (SysV): sret out in
RDI saved to RBX, UTF-16 pointer in RSI tested non-null, first UTF-16 unit
loaded (`0F B7 38`), sret-only result handoff. AdmisSON still requires the
consensus machinery (UTF-16 module-string cluster → xref census →
`mov edx,1` + nearest `e8 rel32` consensus → call-site re-verification),
exactly the SeedTable derivation route — a bare pattern hit is a candidate,
never a pin.

GUObjectArray (9 patterns) / GMalloc (5 patterns) anchor sets: unchanged from
`offline_anchor_scan.py` / patternsleuth resolvers; the runner runs ALL of
them against every binary (they are version-agnostic code shapes), and joins
hits to the table row the version axis selected.

## 3. Deltas that matter across the table (summary; full text in JSON)

1. FName system: TNameEntryArray (≤4.22) → FNamePool (4.23+) → 5.x packings.
   The whole pool decoder + ToString slot are version-dependent; UE4.23-4.27
   has NO known ToStringInternal shape (slot unresolved until measured).
2. FUObjectItem: 24 B total measured on 5.6 (Object@0, Flags@8, remainder
   unpinned). ClusterRootIndex presence varies (late-UE4 yes; 5.x varies) —
   seeds must not pin either way.
3. Reflection: UProperty (pre-4.25) vs FProperty (4.25+); 5.6's FField
   owner-variant tag-bit0 encoding is a 5.6 measurement only.
4. FMalloc: TryMalloc/TryRealloc believed 5.0+; their absence in UE4 shifts
   EVERY later slot — only the ORDER hypotheses ship in seeds; offsets are
   measured per-image from RELA vtable reconstruction (worker-gmalloc route).
5. Base: labels are image-relative; live base is observed per-image at
   runtime (Townfall 0x400000 is one observation, not a law for other games;
   PS5 eboots are ET_SCE_EXEC_ASLR). The generator emits `base_status` and
   refuses to treat an unobserved base as fact.

## 4. Confirming a seed (the upgrade path)

A seed row is upgraded to measured only when ALL of: (1) a decrypted eboot
classified by ref_axes to that version exists (per decrypt-off acquisition
law); (2) anchor probes multi-hit with agreement and static-layout windows
fix values with two-source agreement; (3) for runtime pins, live admission on
console passes. Until then every seed field stays confidence 0.0 and the
generator REFUSES rather than emitting guesses (the Astro Bot refusal path is
the model: 0 markers → no profile, no pins).

## 5. Alignment

- Consumer: `run-blade-profile.py` (this dir) loads the JSON table and joins
  the classified version to its row.
- agent-GI's FIRST-LAUNCH-AUTOMATION.md: NOT landed at time of writing. The
  table's schema (version-keyed entries, per-field status/confidence,
  confirm_when, base_policy) is designed to be GI's shipped-with-the-tool
  table per addendum A5 ("measured once on the Mac per Unreal version");
  field names get reconciled to GI's schema when it lands — the LAWS block in
  the JSON is the invariant contract.
- A2/A5 law preserved: table ships with the tool; profile+self-test run on
  PS5 at first launch; zero per-game human work; no Blade names/offsets
  anywhere (grep-clean: the only Blade strings in this dir are documentation
  of the acquisition path, never in runtime/generator logic).
