# SEED-TABLE-FORMAT.md — the SEED-TABLE.json contract (authoritative, v2)

Worker-initializer-integration, 2026-10-04. This document is the **authoritative
format spec** for `SEED-TABLE.json` as consumed by the merged staged
`UnrealInitializer.cpp`. worker-seed-table-derivation delivers the file; this
worker consumes it. No address may reach the merged unit except through a table
conforming to this spec (in either accepted dialect, §2) or through the
FNAME-VERDICT constants named below. **Never inline guesses.**

Sources of authority consumed here:

- `worker-required-slots-feasibility/SLOTS-FEASIBILITY.md` §4b/§5 (slot ids,
  evidence standard, config/DoScan gating)
- `worker-fname-ctor-audit/FNAME-VERDICT.md` §4/§5 (ctor/pool/base labels,
  pool structural offsets, heap span, validation chain, behavioral probe)
- `worker-seed-table-derivation/SEED-TABLE.json` + `SEED-INTEGRATION.md` (the
  landed values: slots 0/2/4 seeded, slot 3 unseeded, tick correction record)
- `worker-all-signatures-inventory/SIGNATURES-INVENTORY.md` (FLAG-1/3/4 gates)
- run-3 receipt (`ue4ss-native-supplier-composition-4/run-3/receipt.json`),
  unit `unreal-unrealinitializer` (compile argv authority)

## 1. File identity

- Canonical delivered path: `worker-seed-table-derivation/SEED-TABLE.json`.
- Encoding: UTF-8 JSON, one object, no comments, no trailing commas.
- The file pins one exact image. A table whose `image.sha256` does not equal
  the sha256 of the retained `decrypted-eboot.bin`
  (`3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`) is
  **refused at generation time** by `gen_seed_table.py` (never at runtime).
- `SEED-TABLE.conformed.json` (this directory) is the canonical-form snapshot
  of the landed table; raw provenance stays in the derivation worker's file.

## 2. Dialects

`gen_seed_table.py` accepts and normalizes exactly two shapes:

1. **canonical** — the schema below (`format_version: 2`, bare slot keys,
   `present`/`label` fields, derivation triples).
2. **derivation** — the shape worker-seed-table-derivation delivered
   (`N_`-prefixed slot keys, `seeded`/`linked` fields, `image.translation_rule`
   carrying the base, no `format_version`). The normalizer maps
   `seeded→present`, `linked→label`, derives `base.observed` from
   `translation_rule`, defaults `fname_remedy` to the FNAME-VERDICT constants
   (checked against them), accepts slot 6/10 records as **documentation-only**
   (seeding them refuses: slot 6 is settings-owned, tick is vtable-fallback-owned),
   and preserves each slot's full derivation object inside the generated
   provenance string.

Both dialects must produce identical `SeedTable.h` values for the same table —
verified for the landed table (dialect run vs `SEED-TABLE.conformed.json`).

## 3. Canonical schema (version 2)

```json
{
  "format_version": 2,
  "image": { "path": "...", "sha256": "3b6cfad4…", "bytes": 194241424 },
  "base": { "observed": "0x400000", "provenance": "…" },
  "fname_remedy": {
    "pool_label": "0x9365ec0",
    "heap_span": { "lo": "0x8018c000", "hi": "0x100118000" },
    "provenance": "FNAME-VERDICT.md section 4"
  },
  "slots": {
    "g_uobject_array":                  { "slot_id": 0, "kind": "data", "present": true,  "label": "0x93ba110", "runtime_expected": "0x97ba110", "evidence": "…", "derivation": { "anchor_bytes": "…", "rip_arithmetic": "…", "uniqueness": "…" } },
    "fname_ctor_wchar":                 { "slot_id": 2, "kind": "code", "present": true,  "label": "0x23d590",  "runtime_expected": "0x63d590",  "evidence": "…", "derivation": { "…": "…" } },
    "gmalloc":                          { "slot_id": 3, "kind": "data", "present": false, "label": null, "runtime_expected": null, "evidence": "under-determined: …" },
    "static_construct_object_internal": { "slot_id": 4, "kind": "code", "present": true,  "label": "0x490aa0",  "runtime_expected": "0x890aa0",  "evidence": "…", "derivation": { "…": "…" } }
  }
}
```

Field rules (unchanged from v1):

- `slots` keys are exactly the four REQUIRED address slots. Slot 6
  (`engine_version`) is **not representable** — the settings route
  (`[EngineVersionOverride] MajorVersion=5 MinorVersion=6`, exact bytes in
  `worker-seed-table-derivation/UE4SS-settings.ini.seed-proposal`, restage
  re-pins the manifest sha to `5c3af1a4…`) owns it. Tick (slot 10) is not
  representable either — the vtable fallback owns it.
- `label` is the **file-image vaddr**; `runtime_expected` is informational
  (`label + base.observed`) and never compiled in.
- `present: false` requires `label: null` plus evidence naming WHY
  (the landed table's gmalloc entry: 0/14 idiom patterns, string anchors
  absent, DeleteFile-route function disassembled with no GMalloc shape,
  `derivation.json gmalloc_rejection_note`).
- Every `present: true` slot carries `derivation` with at least
  `anchor_bytes`, `rip_arithmetic`, `uniqueness` (SLOTS §4b evidence standard).
- `fname_remedy` and `base` are mandatory whenever any slot is present.

## 4. Consumption + arming semantics (v2 — SHAPE A safe default, binding)

`gen_seed_table.py` emits `SeedTable.h` (quoted include, staging-local,
same-dir resolution — the R3-proven rule) with per-slot
`SlotSeed{present, data_kind, label, name, derivation}` constants plus
`kBaseObserved`, `kFNamePoolLabel`, `kHeapLo`, `kHeapHi`,
`kRequiredSetComplete`, `kGenerated`.

Runtime policy in `ScanGame()` (compiled from the constants; clang folds them
even at -O0, so the armed shape is compile-time — see §7):

| state | runtime behavior |
|---|---|
| `SeedTable.h` absent / `kGenerated == false` | legacy path, byte-for-byte: ps_scan arm runs and dies as today |
| table present, any of the four REQUIRED slots missing (the landed {0,2,4} table: gmalloc unseeded) | **SHAPE A — the compiled ScanGame IS the named loud refusal**, listing present/missing by name, BEFORE any exec touch: no seed binds, no scan runs, no vtable walk, no detour |
| table present, all four present | **armed**: the seeded slots' config flags clear (required mask empties), the ps_scan loop and both Lua DoScan arms are skipped, slots bind after data-side validation (§5) |

Why all four gate arming (LAUNCH9-PREDICTION §1/§2-I3/§4-#2): continuing past
an unseeded gmalloc ("Shape B") binds {0,2,4} and then walks straight into the
first vtable walk (LoadMap, `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL` →
`ASMHelper.cpp:57-69` unconditional text READ — the igf6 crash class) and past
that the tick detour WRITE, before the honest PostInitialize GMalloc throw
could fire. A crash-shaped death is not a loud failure. The upstream
`PostInitialize` throw ("GMalloc is uninitialized.") stays intact untouched as
defense-in-depth; under Shape A the ScanGame refusal is the terminal.

v2 history: v1 was all-or-nothing; a per-slot continuation variant (Shape B)
was built and then withdrawn in favor of Shape A after the launch-9 safety
analysis. The landing of a derived gmalloc constant is a pure table update:
one regeneration + one recomposition, no merged-unit change.

## 5. Runtime validation contract (what refutes a wrong constant)

Ordered, all data-side (no code byte reads) — implemented in
`SeedArming::consume_required_slot_seeds` + the pure `SeedValidation.hpp`
(host-tested, `host-tests/`, ALL PASS):

1. **Base check** — `ModuleSource->Views().array[MainExe].image_base` must
   equal `kBaseObserved` when non-null; mismatch = image moved = refuse (loud).
2. **Pool shape** (FNAME-VERDICT §5) — u32 `CurrentBlock` @ `pool+0x8` ≥ 1;
   u64 `Blocks[0]` @ `pool+0x10` nonzero inside `[kHeapLo, kHeapHi)`. Retried
   within the `SecondsToScanBeforeGivingUp` budget while the pool is not yet
   initialized (interned ids still zero also retries); once the shape holds,
   mismatches are terminal.
3. **Interned strings** — u32 ids @ `pool+0x14040` ("None") and
   `pool+0x14044` ("ByteProperty") resolve via `Blocks[id>>16] +
   (id&0xFFFF)*2` to entries whose u16 header (bIsWide bit0 / Len bits15:6,
   port `NameTypes.hpp`) decodes to the exact ASCII string; block index and
   current-block offsets bounded by `CurrentBlock`/`CurrentByteOffset`.
4. **Binding + behavioral probe** (FNAME-VERDICT §5) —
   `FName::ConstructorInternal.assign_address(base + ctor_label)`; then
   `FName(STR("ByteProperty"), FNAME_Find)` (**never** `FNAME_Add` — pool
   mutation guard) must return a nonzero id equal to the independently
   pool-read `+0x14044` id. Zero-fill or divergence ⇒ `reset_address()` +
   throw ("refuted, no replay"). This is also the FLAG-1 answer: the detour
   verifier (`VerifyFNameConstructor`) is **skipped while seed-bound**,
   unconditionally — not gated behind the exec-writability verdict — so the
   seeded FName path works under EITHER verdict.
5. **Data-slot probes** (admission-grade, logged "identity and ABI unverified"):
   slot 0 — non-null 8-aligned; slot 3 (when seeded) — `[addr]` non-null
   aligned FMalloc*.

Accepted deref class: a wrong-but-mapped address is refuted by the chain; an
unmapped read faults (sha-pinned constants, same class as every pointer deref
in the module). Faulting is launch-blocking, not silent.

## 6. Latent-trap and hook gates carried by the merged unit

- **FLAG-4** (`ftext_fstring` never assigned → second-pass DoScan arm
  unconditionally live): on Prospero the field is removed from that gate;
  armed seeds close the whole arm. Non-Prospéro keeps the original
  expression (byte-preserving legacy).
- **FLAG-1** (detour = code READ+WRITE): the seed-bound ctor skips
  `VerifyFNameConstructor` unconditionally; the tick vtable fallback comment
  records that tick is not seeded (runtime Tick = base + linked 0x2fac8c0 =
  0x33ac8c0 per the derivation worker's byte-evidenced correction; the
  once-circulated 0x37AC8C0 was a runtime-mislabeled-as-linked convention
  error) and that the eventual tick detour patch remains verdict-dependent
  and fails loud (ERROR_ID throw).
- **FLAG-3 / A-PI**: the ProcessInternal/ProcessLocalScriptFunction Zydis
  derivation is honored via the retained parse path
  (`[Hooks] HookProcessInternal` / `HookProcessLocalScriptFunction` ini →
  `SettingsManager.cpp:279-280` → `SettingsManager.hpp:116-117` (default
  true) → `UE4SSProgram.cpp:1222-1223` → staged `:1497` condition). The ini
  keys decision is worker-settings-v3-package's; when set, the code-read
  derivation is skipped with zero initializer changes.

## 7. Compile-time arming consequence (verified)

The arming booleans are `inline constexpr`, so clang folds them **even at
-O0**: with the shipped partial table ({0,2,4}, gmalloc unseeded) the compiled
`ScanGame` is the named refusal and nothing else — the seed-binding body, the
ps_scan loop, the DoScan lambdas and every downstream vtable path are
compile-time unreachable and absent from the object (verified: ScanGame folds
to 0x4a4 bytes; 296 extern symbols drop out; `ps_scan` import disappears).
Composed-object import set unaffected: every dropped import (ps_scan,
DoScan targets, query-harness, wide-string helpers) is defined by another
composed unit, and the one added import (`fmt::vformat<char>`) resolves
against the composed fmt unit — **composed undefined-delta identical to
run-3's** (`added: [sceKernelGetModuleInfo]`, `baseline_extended: true`,
nothing removed), so the composition gate passes unchanged. When a future
complete table (gmalloc derived) is regenerated, the same source compiles to
the full armed path — that launch inherits FLAG-2 (vtable-walk read) and
FLAG-1 (tick detour write) as its open verdicts.

## 6b. OPTIONAL exec-writability verdict gating (designed, NOT armed)

The IGW probe (`modbridge_exec_write_probe_armed.prx`) runs its full ladder
synchronously in its own `module_start`. If the ridealongs plan orders its
ploader.ini line BEFORE the composition's, its verdict records exist by this
module's start. Design (extension point in the merged unit,
`SeedArming::kExecWritabilityVerdictGating`, currently `false`):

- When the ridealongs plan pins the verdict jsonl path + record schema, flip
  the constant and add a bounded read at ScanGame entry:
  - `{X->RX ok}` — post-arm vtable walks (ASMHelper text read, FLAG-2 class)
    may be allowed;
  - `{R->RW ok}` — a vtable-swap tick backend (data-side, replaces the PLH
    patch) may be allowed;
  - absent / unreadable / both-fail — **stay SHAPE A** (the compiled default):
    no walk, no detour.
- Until that contract lands, the constant stays false and nothing reads any
  verdict file. The ini ordering itself is worker-ridealongs' (they own the
  ini layout); the consumer contract on this side is the constant + the
  verdict semantics above.

## 8. Change control

- Format changes bump `format_version`; the generator refuses unknown
  versions. The generator, not the consumer, validates the schema — the
  runtime consumer sees only the generated constants.
- Seed VALUES are owned by worker-seed-table-derivation; this worker owns the
  format, the generator, and the arming policy. A future gmalloc derivation
  lands as one more present slot in the table + one regeneration + one
  recomposition — no merged-unit change.
