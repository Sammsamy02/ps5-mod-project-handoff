# ORACLE-SUPPLEMENT — launch-10 FINAL-build deltas (worker-oracle-final-sync)

PATCH PROPOSAL for the controller to merge into
`outputs/2026-10-04-zcode-team/worker-launch10-acceptance-criteria/ORACLE.md` at
staging time. ORACLE.md itself was NOT touched by this worker. The oracle's 83
predictions were written against the PRE-fold unit set; the FINAL launch-10
build (EDITION `launch10-final-a` + walkguard edit 5) has since gained exactly
the five deltas below. Everything else in the oracle stands.

Basis (all verified against the on-disk FINAL artifacts, 2026-10-05):

- Edition: `outputs/2026-10-04-zcode-controller/launch10-build/EDITION.json`
  (`launch10-final-a`, 4 tagged edits + `bridge_cli`) PLUS walkguard edit 5 =
  the driver's hardcoded retained replacement
  `--replace-runtime-unit asmhelper-asmhelper=…/worker-launch10-walkguard-prep/staging/asmhelper-guard/ASMHelper.guarded.cpp`
  (`build_runtime_composition6_l10.py:351`).
- Composed-run proof of the fold: run receipt
  `.superpowers/…/ue4ss-native-supplier-composition-6/run-l10-1[-aside-*]/receipt.json`
  — `asmhelper-asmhelper` source `694af016…`, object **`81dd832b…`** = the
  fold-0 guarded object (0 `syscall` instructions, WST-measured); composed
  runtime `bef38f16…` (117,855,744 B).
- Class key of the oracle is reused verbatim (EXPECTED-SUCCESS /
  EXPECTED-REFUSAL / REGRESSION / DORMANT / CONDITIONAL).

New source pins used below (absolute; short names as in CLAIM.json `sources`):

| short | file |
|---|---|
| `WG` | `worker-launch10-walkguard-prep/staging/asmhelper-guard/ASMHelper.guarded.cpp` (`694af016…`, the shipped unit) |
| `UVC` | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtualBaseVC.hpp` |
| `INI-F` | `outputs/2026-10-04-zcode-controller/launch10-build/final-units/UnrealInitializer.cpp` (`c3a1c9eb…` = EDITION edit 2; 2170 lines — supersedes the oracle's `UI` staging pins for line numbers late in the file) |
| `CB` | `outputs/2026-10-04-zcode-controller/launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp` **as built = sha `fc06fe52…`**; the surviving byte-exact copy is `worker-comp6-driver/staging/lineage-overlay-2-aside-1791208925/lifecycle/runtime_start_bridge.cpp` (see REPORT drift note before re-pinning the live copy) |
| `HDR` | `outputs/2026-10-04-zcode-controller/launch10-build/staging/live_module_facts.hpp` (`67170b21…` = EDITION edit 4) |
| `SV-F` | `outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp` (`6e8f8c4a…`) |
| `DRV` | `outputs/2026-10-04-zcode-controller/launch10-build/build_runtime_composition6_l10.py` |
| `UD` | `.superpowers/…/ue4ss-native-supplier-composition-6/run-l10-1-aside-1791209468/undefined-delta.json` (the completed spanpub-merge receipt state) |

---

## S1. WALKGUARD FOLD (edit 5) — new observable at the pump-arm site

Fold state shipped: **fold-0 — the guard unit is ACTIVE in the build (wrapping
every walk read) but the RX-walk fold (`RC_ASM_WALK_RX_GUARD=1`) is NOT
active**: the define is ABSENT from the real 148-arg unit argv, so the shipped
object `81dd832b…` contains ZERO mprotect text and `enter()` refuses EVERY read
loudly before any syscall (`WG:57-59,132-135,215-223`; WST probes 0/1 measured
0 vs 1 `syscall` instructions). The activation flip is verdict-gated
(IGW D2) and controller-owned — not part of this build.

Coverage and call route: `get_first_instruction_at_address` is the single
innermost text read of every resolve path (`WG:28-34`); the scoped guard wraps
it at `WG:286`. The initializer's walk route is pinned:
`OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL` (`UVC:118-134`) reads the LIVE
object's vtable from serviced memory (`UVC:132` — a data read, unaffected),
then calls `ASM::resolve_function_address_from_potential_jmp`
(`WG:325-345`) → `get_first_instruction_at_address` → the guarded read.

Pump-arm sites in `INI-F` (supersedes the oracle's `UI` line pins):

- `INI-F:1819` — LoadMap walk `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(UEngine,
  LoadMap, GameEngine)`, inside `if (UnrealConfig.bHookLoadMap)` whose
  `Finding GameEngine::LoadMap` line prints FIRST (`INI-F:1816-1818`). This is
  the FIRST live guarded read on the armed path (no earlier call site in the
  file: the only resolve/walk calls are at `:1819, :1828, :1851, :1936, :1945,
  :1954, :1963, :1972, :1981, :2049, :2087`).
- `INI-F:1828` + `:1890-1894` — the Tick vtable route; `:1892` is the fallback
  line `Scan failed, falling back to vtable address for GameEngine::Tick`
  (oracle F29). On the final build this site is only reachable if the LoadMap
  walk is settings-disabled; otherwise the :1819 refusal is terminal and this
  site is unreachable-by-construction in the same pass.

### S1.1 Every new log/refusal string the guarded walk can emit

| # | Class | Sink | Expected evidence (verbatim from `WG`) | Pin | Meaning |
|---|---|---|---|---|---|
| W1 | EXPECTED-REFUSAL (new info; the fold-0 refusal) | UE4SS.log (Error) | `[WalkRxGuard] REFUSED: NO read attempted; walk text reads require the IGW RX verdict (LAUNCH10-MATRIX D2). Fold RC_ASM_WALK_RX_GUARD=1 only for W_WORKS/W_FAILS_R_WORKS rows; reason in thrown message.` | `WG:234` | emitted by `refuse_and_throw` BEFORE any read, on every guarded walk at fold-0; zero syscalls |
| W2 | EXPECTED-REFUSAL (new info; THE pump-arm terminal) | status failed detail (via the F30 row) | `[WalkRxGuard] refused: guard_off_no_rx_verdict (walk text read not permitted; no read attempted; detail=0)` | reason `WG:135`; message template `WG:235-237` | the thrown `std::runtime_error`; the init thread catch lands `what()` verbatim (sanitized, 512-B cap) in status `state 2 failed 1 …` — oracle F30 shape. `Finding GameEngine::LoadMap` (`INI-F:1818`) is the last initializer progress line before it |
| W3 | REGRESSION (impossible at fold-0) | UE4SS.log + status | same W1/W2 shapes with reason `anomaly_latched_earlier_restore_failed` | `WG:121` | the latch is only set by `exit()` after an ACTIVE transition; seeing it proves a folded-ON guard object shipped (composition identity drift) |
| W4 | REGRESSION (impossible at fold-0) | UE4SS.log + status | same shapes with reason `page_range_invalid` | `WG:127` | requires len==0 / non-power-of-2 page / address overflow — unreachable for the fixed 16-byte walk reads |
| W5 | REGRESSION (impossible at fold-0) | UE4SS.log + status | same shapes with reason `guard_depth_exceeded` | `WG:152` | depth only increments under the folded-ON transition path (`WG:149-164`) |
| W6 | REGRESSION (impossible at fold-0) | UE4SS.log + status | same shapes with reason `rx_transition_denied` | `WG:160` | the mprotect call at `WG:156` is only reached folded-ON; at fold-0 `sys_mprotect` is a `-1` stub that is never invoked (`WG:215-223`) |
| W7 | REGRESSION (impossible at fold-0) | UE4SS.log (Error) | `[WalkRxGuard] ANOMALY: restore mprotect failed rc=0x<n>; page left RX (still executable); every further walk read now refuses loudly` | `WG:260` | destructor anomaly — requires an active in-flight transition; fold-0 `exit()` no-ops (`WG:170-175`) |

### S1.2 Observable difference vs the unguarded crash

- UNGUARDED (the oracle's F29 crash-site note / igf6 T-RD class): the naked
  ≤16-byte text read lands on the EXEC-ONLY game text span (VMMAP prot 0x4) →
  in-page fault → the game process DIES mid-walk; status frozen at
  `activated`; UE4SS.log ends mid-module-lines with no `failed` line. Oracle
  verdict FAIL (NO-CRASH invariant violated).
- FINAL build (guarded, fold-0): the FIRST walk refuses LOUDLY — W1 Error line
  + W2 thrown message — with NO read attempted and ZERO syscalls. The throw is
  caught by the serialized init thread (fail-closed, oracle F30); the game
  process SURVIVES. **The guard refuses loudly; no crash — the NO-CRASH
  invariant is preserved at exactly the site that killed igf6.** A crash at
  this site despite the guard = REGRESSION (the unguarded unit shipped).
- Oracle row updates: F29's success ladder is unreachable past
  `Finding GameEngine::LoadMap` whenever W2 fires (all later address/`GameEngine::Tick address (…)`/InitGameState/BeginPlay/… lines do not appear
  in that pass); F30 gains W2 as a named-throw class; the verdict reading is
  unchanged in structure — W2 after the seed gates is a NAMED later-stage
  refusal (GOOD clause 3 class), and while it stands, BEST's walk-dependent
  rows (F29 → F34/F35 engine-ready/mods-started) cannot be reached. Honest
  bound: this is source-derived (fold-0 refusal is unconditional in `WG`);
  console behavior is still only proven by the real launch.

## S2. CONSOLIDATED BRIDGE (`fc06fe52`) — r14-CONDITIONAL tags resolved + spanpub

The merged bridge carries ALL THREE layers: r14's lease hardening L1-L5 with
its lease tags, the geometrydiag `lmf_*` events, and the spanpub publication
markers + accessors (`CB:84-89`). The oracle's `BR14` CONDITIONAL tag set is
therefore no longer conditional — r14 shipped.

### S2.1 Oracle row resolutions (Phase C)

- C8/C9 (`bridge_lease_reg_spans`/`bridge_lease_reg`): CONDITIONAL →
  unconditional EXPECTED-SUCCESS, values **spans = 6, reg = 6** on the pinned
  igf3 shape (`CB:675-695`: registration from the FROZEN capture snapshot
  inside the scan wrapper — the first op `capture()` drives).
- C11: the r14-only tags are now LIVE gates whose SUCCESS-PATH absence is the
  unconditional expectation; ANY emission is named new info: `bridge_lease_order`
  (`CB:651`), `bridge_lease_cap` (`CB:690`, saturation deferred to the gate),
  `bridge_anchor_refuse` + `anchor_owned_ref` (`CB:812`, `CB:451` — L4
  window-anchor owned-coverage), `bridge_lease_ref_lo/hi/len` +
  `bridge_lease_near_lo/hi/len` (`CB:609-623`).
- C12 resolves to the merged L3 gate shape: gap events fire DURING capture
  (inside the scan wrapper, `CB:993/1006` → `CB:845-882`) and the start refuses
  BEFORE conversion: `runtime_start_failed code=1 detail=lease coverage gap:
  requested [0x<lo>,0x<lo>+0x<len>) outside registered readable spans; nearest
  registered [0x<b>..0x<e>)` (`MRS_ERR_CONVERSION`=1, `runtime_start_bridge.h:37`;
  detail assembled `CB:855-881`, 192-B bound). Because the gate returns before
  `TryMakeNativeLoaderFactsFromSupplier`, a C12 refusal carries NO
  `lmf_extent/anchor/intervals` events — that absence is diagnostic, not drift.
- C13 stays DORMANT with the same recurrence = REGRESSION rule (r14 L1-L5 make
  the launch-8 two-snapshot race structurally impossible).

### S2.2 Expected diag tag sequence for the merged bridge (igf3-shaped boot)

In emission order (success path):

1. `bridge_vmentry_entries`, `bridge_vm_walk_stop` (oracle C1/C2 unchanged).
2. `bridge_kve_distinct0..3`, `bridge_kve_eboot` (C4 unchanged).
3. `bridge_exepath_valid`, `bridge_exepath_sig`, `bridge_exepath_source`
   (C7 unchanged; r10 `stat_app0[_ec]`/`stat_ownmod[_ec]`/`stat_rootdir[_ec]`
   only on the source=0 fork).
4. `anchor_cov` / `anchor_match` per w, `anchor_c_either` (C5 unchanged); NEW
   r14 L4 rider: `anchor_owned_ref` (with `bridge_anchor_refuse` +
   `bridge_lease_ref_*`/`near_*`) if a window-anchor read is not owned-covered.
5. `bridge_lease_reg_spans` = **6**, `bridge_lease_reg` = **6**
   (`bridge_lease_cap`=1 only if a record saturates the 128-region registry).
6. NEW spanpub publication markers, same wrapper, same frozen snapshot
   (`CB:982-986` call; emissions `CB:736-740`): `bridge_spanpub_count`
   (published non-image readable spans, ≤ `rc_seed::kMaxPublishedSpans`=16),
   `bridge_spanpub_total` (qualifying count), `bridge_spanpub_cap` (0/1),
   `bridge_spanpub_env_low`, `bridge_spanpub_env_high` (max captured record
   end — the truncation-honest envelope). "if any emit" bound: see REPORT —
   the AS-BUILT `fc06fe52` TU failed packaging compile, so these five tags are
   prediction-pinned but not yet live-evidenced; the repaired bridge form
   emits them here.
7. Coverage gate: SILENT on success (no tags); on gap the C12 set above.
8. Conversion success: `bridge_cfg_gamedir`=1, `bridge_cfg_workdir`=1
   (`CB:1079-1080`), THEN the geometrydiag block from the adapter's resolved
   record (`CB:1084-1098`): `lmf_extent_base_low`=0x00400000,
   `lmf_extent_base_high`=0x00000000, `lmf_extent_end_low`=0x0c0602c8,
   `lmf_extent_end_high`=0x00000000, `lmf_anchor_count` = **7**,
   `lmf_intervals_found` = **1**.
   (Conversion refusal instead: `lmf_vmentry_cap`, `lmf_vm_scan_cap`,
   `lmf_linkmap_cap`, `lmf_provider_code`, `lmf_identity_source` fan-out
   (`CB:1040-1044`) then the same extent/anchor quartet with
   `lmf_intervals_found`=0 (`CB:1057-1066`) — oracle D6/D7 unchanged.)

Predicted values (unchanged from the oracle's D1-D4, now unconditional on the
merged bridge): **anchor_count=7, spans=6, reg=6, intervals_found=1**; extent
[0x400000, 0xc0602c8).

### S2.3 Spanpub accessors — initializer-side observable updates (delta 2 tail)

The pool-bound v2 initializer (EDITION edit 2, `INI-F`) consumes the accessors
(`INI-F:625-633` decl; `:864-939` consumer; UND-proven in the composed run via
UD `linker_provided`). Oracle row updates:

- F14 success line SUPERSEDED (`INI-F:938-939`): `[Seeds] FName pool validated
  at 0x9765ec0 (CurrentBlock <n>, Blocks[0] 0x<hex> inside a published
  non-image readable span of <k>; 'None' id 0x<id>, 'ByteProperty' id 0x<id>)`
  — the frozen [0x8018c000,0x100118000) envelope is RETIRED; Blocks[0] is
  admitted against the BRIDGE-PUBLISHED spans of this invocation.
- F15 refusal format SUPERSEDED (`INI-F:915-916`): `…: <shape.failure>
  (observed CurrentBlock <n>, Blocks[0] 0x<hex>; published non-image readable
  spans <k>)` — still value-carrying, still verdict-cap GOOD.
- NEW refusal class, EXPECTED-REFUSAL (new info; bridge side): `FName pool
  admission at 0x<addr>: span table unavailable (bridge published 0 non-image
  readable spans; Blocks[0] cannot be bounded; frozen envelope retired)`
  (`INI-F:867`) and the none-survived variant (`INI-F:884`) — the bridge
  published no usable span table; never a fallback to the frozen envelope.
- NEW `SV-F` shape-failure strings (supersede the oracle's `heap span is
  empty` / `Blocks[0] outside the readable heap span` pair):
  `span table unavailable (bridge published no non-image readable spans this
  invocation)` (`SV-F:254`), `Blocks[0] inside the image extent (wrong-base/
  wrong-pool arithmetic; pool blocks are heap allocations)` (`SV-F:271`),
  `snapshot truncated: cannot prove heap coverage at or past the captured
  envelope (omitted mapping, not proven unmapped)` (`SV-F:283`), `Blocks[0]
  not covered by any published readable span (unmapped or unreadable at this
  address; capture complete below the envelope)` (`SV-F:286`). The
  CurrentBlock<1 / null / not-8-aligned structural strings are unchanged.

## S3. CAPACITY CHANGE (HDR `67170b21`) — complete capture now EXPECTED

- `kMaxVmMappings` 1024 → **16384** (`HDR:85-91`; EDITION edit 4, staged via
  `--stage-header …=67170b21…` overriding the driver-pinned default
  `f114f876`). The lmf snapshot now holds the WHOLE observed walk
  (~10,019 records in-save; heap mappings ~record 9,500) in a process-lifetime
  static buffer (384 KiB BSS). Same edit folds `kMaxAnchorMappings` 8 → 16
  (`HDR:94-99`) — the oracle D3 "folded cap" confirmation: 16 SHIPPED, so 9+
  anchor counts read honestly instead of pinning at 8.
- NEW OBSERVABLE — complete capture is the EXPECTED case: the bridge query
  buffer matches end-to-end (`kBridgeVmmapEntries`=16384, `CB:240-241`), so a
  full walk ends clean: `bridge_vm_walk_stop` = **0** (the stop=2 honest
  lower-bound path `CB:350` should NOT fire), `capabilities.vm_mappings_truncated`
  = false, and the spanpub envelope covers the heap (a valid pool can no longer
  be mis-refused by a truncated snapshot — the launch10-c rationale).
- NEW TRUNCATION REFUSAL CLASS: `vm_total > 16384` → `bridge_vm_walk_stop` = 2
  (oracle C2 honest lower bound) AND `vm_mappings_truncated`=true AND, at the
  pool check, an admission miss at/above the captured envelope refuses
  `snapshot truncated: cannot prove heap coverage at or past the captured
  envelope (omitted mapping, not proven unmapped)` (`SV-F:283`) instead of the
  'unmapped' class (`SV-F:286`). At 16384 this class firing at all = the walk
  grew past capacity = new info, not a geometry failure.

## S4. LAYOUT FRESHEN ({cppmod,luamod,settings} recompiled) — ZERO oracle impact

The l10 tagged edit recompiles `luamod`, `cppmod` and `settings` from their
UNCHANGED attested sources against current headers (`DRV:346-350`, comment
"recompile the three stale-inline definer units from their UNCHANGED attested
sources against current headers"), with `freshen-deps.txt` (1322 files)
seeding the replace-receipt freshness. Sources unchanged ⇒ by construction
ZERO new log/refusal/diag strings from these units. **Oracle impact: none —
no row is added, changed, or re-classed by this delta.** Any NEW line
attributable to luamod/CppMod/SettingsManager means different sources were
compiled — composition identity drift, handled by the oracle's existing
REGRESSION discipline, not by a new row.

## S5. DELTA `linker_provided` — one line for the evidence reader

`undefined-delta.json['linker_provided'] = ["bridge_capture_envelope",
"bridge_readable_span_at", "bridge_readable_span_count"]` (UD; `['added']` =
`["sceKernelGetModuleInfo"]` only): these are exactly the 3 spanpub accessors
the composed runtime references and the packaging bridge TU defines, so their
ABSENCE from `['added']` is EXPECTED, not drift. (Bookkeeping note: a later
re-run of the same byte-identical composed object `bef38f16…` reclassifies the
same 3 symbols into `['added']` with `linker_provided` null — same
expectation: the symbols must resolve at the packaging link; neither shape
alone is a failure signal.)
