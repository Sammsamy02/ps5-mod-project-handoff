# REPORT — worker-seed-table-derivation (2026-10-04)

Status: **DELIVERED (offline derivation + proposals; no console contact, no frozen or
wiring file modified, no nested subagents).** Claim written first (`CLAIM.json`);
this report written last. Working dir:
`W:outputs/2026-10-04-zcode-team/worker-seed-table-derivation/` (`W:` = workspace root
`/WORKSPACE/.codex/worktrees/translation-core/what`). ~60 tool calls.

## Verdict up front

The staged-initializer seed table recommended by SLOTS-FEASIBILITY §4b/§5 is
**3/4 derivable offline** against the sha-pinned retained image
(`decrypted-eboot.bin`, sha256 `3b6cfad4…622fc`, verified before every scan):

| slot | semantic | runtime seed | status |
|---|---|---|---|
| 0 | GUObjectArray | **0x97ba110** (linked 0x93ba110, phdr-7 BSS) | seeded — live-validated in-game (71959 objects, read+write) + 1565 offline code refs |
| 2 | FName ctor wchar | **0x63d590** (linked 0x23d590, code) | seeded — independent offline derivation matching 3 prior cross-checked worker records; FC-1 validator built, in-game run pending |
| 3 | GMalloc | **UNSEEDED** | under-determined: all shipped routes dead on this image (details below); fails loudly per the binding standard |
| 4 | StaticConstructObject_Internal | **0x890aa0** (linked 0x490aa0, code) | seeded — dominant terminal call in 744/1918 anchor windows + prologue byte-identical to the independently recorded expected bytes |
| 6 | engine version | settings route | `[EngineVersionOverride] MajorVersion=5 MinorVersion=6` (exact bytes + sha in SEED-INTEGRATION §5) |
| 10 | tick | no seed | vtable fallback unchanged; **convention correction recorded** (below) |

Deliverables: `CLAIM.json`, `SEED-TABLE.json`, `SEED-INTEGRATION.md` (diffs 1-6,
ini bytes, argv, validation design), `derivation.json` + `derive_seed_constants.py` +
`derive2.py` (machine-recorded evidence), `UE4SS-settings.ini.seed-proposal`
(833 B, sha256 `5c3af1a4…fb733`).

## Method (honest notes)

- Local read-only parse of the already-retained image only. ELF64 EXEC phdr table
  re-derived from the file (phdrs 0/1/3/7/8; the "LOAD7 BSS" record name is the real
  phdr index — LOAD7 spans linked [0x913c000, 0x9633418) with BSS past 0x93085f2).
- Shipped ps_scan ELF routes were replicated offline with full read access
  (capstone 5.0.7 for instruction-boundary-true decoding; no guessed addresses; every
  candidate additionally cross-checked against independently recorded worker evidence).
- The vaddr↔file-offset arithmetic (delta 0x4000 for LOAD0/LOAD1) was validated
  against worker-anchor-validation's recorded extraction offsets (tick 0x2fb08c0,
  SCO 0x494aa0) and byte-identical prologues.

## Findings worth the controller's attention

1. **Tick convention correction (byte-evidenced).** `worker-tick-address-validation`'s
   prediction "runtime = 0x400000 + 0x33ac8c0 = 0x37AC8C0" mislabels the recorded
   runtime as a linked vaddr. Bytes: linked 0x2fac8c0 carries the exact canonical
   prologue `554889e54157415641554154534881ec` (file off 0x2fb08c0 = the recorded
   extraction offset); linked 0x33ac8c0 decodes mid-instruction (contains `ud2`).
   Runtime Tick = **0x33ac8c0** (the c2-alt-correspondence / vaddr-semantics /
   anchor-probe convention). My independent game_loop.rs-route derivation agrees:
   the unique UTF-16 `causeevent=` xref at 0x2facbae sits in the function starting
   0x2fac8c0. No shipped behavior changes (tick is not a seeded slot).
2. **Slot 3 GMalloc is honestly unseedable today.** Re-verified: 0 hits for the idiom
   patterns; `/proc/meminfo` and the root-privileges string absent. The
   `DeleteFile %s` anchor exists (linked 0x79202d6, exactly 1 lea xref at 0xb368a)
   but its containing function (root 0xb3670) was disassembled: no GMalloc shape —
   the rip-relative global it touches is the stack-protector guard slot 0x9131f08
   (guard load → save → compare → `call 0x7471e60` + `ud2`). A find_global
   recursion attempt produced only linear-sweep artifacts (one bogus candidate
   0x9131f38 is **rejected and documented** in `derivation.json`
   `gmalloc_rejection_note`). Per the evidence standard: no derivation → no seed →
   the proposal gates it off so PostInitialize throws
   "GMalloc is uninitialized." quickly instead of a 120 s timeout loop.
3. **FName ctor derived independently** via the shipped 5-anchor route (cluster of
   all five module-name xrefs; `mov edx,1`/`e8 rel32` windows; consensus 3/4 →
   0x23d590; prologue `554889e553504889f04889fb…`), agreeing byte-for-byte with
   worker-fname-ctor-audit, worker-fname-fc1-materialize (9 rel32-verified call
   sites), and worker-fname-runtime-probe fixtures — and my xref census
   (10284/5/2/18/2) matches the materialize diagnostics exactly.
4. **SCO derived via population statistic** where the shipped strict first-6 pair
   intersection is empty (mixed window shapes): 0x490aa0 is the dominant terminal
   near-call in 744/1918 `NewObject with empty name…` anchor windows and the second
   half of the dominant consecutive pair (params-ctor 0x493530 → SCO), which is
   exactly the shipped route's documented sequence; prologue equals the recorded
   expected bytes extracted by another worker from the same image at the same file
   offset. Runtime 0x890aa0 equals the recorded igf5/igf6 candidate.
5. **GUA corroborated offline**: 1565 rip-relative code refs target linked 0x93ba110
   exactly (first sites cluster at 0x2b1e57-0x2b329e); 0 mov-imm32 refs. The
   in-game live validation remains the primary evidence.

## What was NOT done / non-claims

- No constants were guessed; slot 3 stayed unseeded. No edited
  UnrealInitializer.cpp was staged (the diffs are proposals; the controller applies
  them to a copy and pins it via `--replace-receipt`).
- Seeds 2/4 remain "identity and ABI unverified" until FC-1/sco_anchor run in-game;
  per-slot arming booleans let the controller gate each seed behind its validator.
- No claim that the composed runtime reaches PostInitialize with these diffs — only
  that the failure mode stays loud and the required-mask problem is closed for
  {0,2,4,6}.
- No unrelated files touched; all writes confined to this worker directory.
