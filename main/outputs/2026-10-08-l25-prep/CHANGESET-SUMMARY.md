# CHANGESET SUMMARY — l24 built runtime vs l25 tree (Claude packet)

Author: agent BB (2026-10-08). READ-ONLY enumeration of the l25 runtime tree;
this file + `agent-BB-changeset.md` are the only writes.

Baseline ("l24 built runtime") = what the parent composition
`outputs/2026-10-08-l24-build-r2` actually consumed, per its SOURCE-SELECTION.json
(one-hop law, Z9/O): typescan-r3 candidates, l23 thread sources, l23 profile
units, r5 ScriptHookThunks, port-root UObjectArray/ClassListener/UnrealVirtual506/
FMalloc FunctionBodies. Every CHANGES row's `original` is exactly such a path;
all 24 originals were re-hashed during this audit and are byte-identical to their
pins (l24/l23/r5/port-root trees untouched).

Inputs enumerated (complete): `2026-10-08-l25-runtime/typescan-r4/CHANGES.json`
(20 rows), `profile/CHANGES.json` (4 rows), `thread/CHANGES.json` (0 rows,
placeholder), plus the copyin-probe overlay (agent Q) wired by
`build_variants.py` (2 patch targets: composed second-hop UnrealInitializer.cpp +
packaging-side runtime_start_bridge.cpp). Total 26 entries; 24 declared CHANGES
rows (matches PLAN.json `changes=24`), 12 of which carry content deltas vs l24.

Row format: `file — original_sha8 -> candidate_sha8 [delta lines] WHAT; WHY
(owner: report)`. "carried" = candidate byte-identical to the l24-consumed
original (zero l25 delta; row exists only to re-pin the l24-consumed path onto
the l25 tree so the composition keeps consuming it).

## Theme 1 — Crash root-cause fixes (profile offsets, FMalloc vtable map, scan-crash containment)

- `profile/profiles/townfall-build-facts.json` — 056cf45a -> 95d22dbe [79] Adds
  required_members FMapProperty::KeyProp=112/8, ValueProp=120/8,
  FSetProperty::ElementProp=112/8 and promoted totals FMap UEP_TotalSize=160 /
  FSet=144 (item 5, generic-platform compiler agreement + static image), THEN
  corrects 5 FUObjectArray rows in place from engine-binary disassembly
  (CreateListeners 104->88, DeleteListeners 120->104, PrimarySerialNumber
  176->144, bShouldRecycle 180->148, UEP_TotalSize 184->152; old pins pointed
  into adjacent BSS because l23 assumed S=0x28 pthread critical section, the
  shipping Prospero build uses the 0x18 lazy-mutex form) (earlier-batch worker +
  agent AS applying agent AA values; AS §1, AA receipt sha 9d73f4a5, AE
  corroboration).
- `profile/units/CompiledNativeLayoutDeclarations.hpp` — 900bc7ac -> bf9419dd
  [6] INI text the runtime parses: new `[FMapProperty] KeyProp=0x70
  ValueProp=0x78 UEP_TotalSize=0xa0`, `[FSetProperty] ElementProp=0x70
  UEP_TotalSize=0x90` sections + corrected `[FUObjectArray]` values + new
  embedded digest; why: binds the runtime to the corrected offsets (digest gate
  green on l25 units, red on l23) (earlier-batch + AS; agent-M §3).
- `profile/units/RuntimeBuildProfile.hpp` — 7a289aba -> 09a14712 [4] Regenerated
  profile (same generator mechanism as generate_overlay.py): kDigest + the 5
  corrected kRequiredMembers pins; why: the compiled-in pins must match the
  facts/INI or preflight refuses startup (AS §1 item 3).
- `profile/units/JoinedLayoutPreflight.hpp` — 32c9856c -> 1646a99e [8] Rows
  75-77 make the three container members startup-required (group
  startup_core_map_set_members); why: refuse boot rather than run with missing
  container offsets (earlier-batch; agent-M §3).
- `generated_include/FunctionBodies/5_06_VTableOffsets_FMalloc_FunctionBody.cpp`
  (original = port root) — 95488869 -> 2f20d64d [94] FIX 1: every non-dtor
  FMalloc vtable default-map entry shifted +8 to the binary-verified true slots
  (Malloc 0x28->0x30 ... Realloc 0x38->0x40 ... GetTotalFreeCachedMemorySize
  0xF0->0xF8); why: the compiled-in default map was one slot low (latent — l24
  dispatched through the staged table; a staging failure would route Realloc to
  the TryMalloc default wrapper 0xb8a20 with ptr-as-size) (agent AU from agent-AB
  §4/§5 audit, AH edge reconciliation; AU §FIX 1).
- `src/VersionedContainer/UnrealVirtualImpl/UnrealVirtual506.cpp` (original =
  port root) — 8e407b89 -> 8e407b89 [carried, 0] Mechanism row, deliberately
  byte-identical; why: build_runtime.py only recompiles a unit when its unit
  source appears in replacements — this row forces unit
  unreal-src-versionedcontainer-unrealvirtualimpl-unrealvirtual506 into
  affected_units so the corrected FunctionBodies include (routed via the
  candidate generated_include -I shadow) is actually consumed; a bare row on the
  generated include alone would be a silent no-op (Z9 risk 1) (agent AU).
- `typescan-r4/candidate/src/TypeChecker.cpp` (original = l24 typescan-r3
  candidate) — dcebc6e6 -> c05b06a7 [153] Scan loop rewired to the Checked
  (noexcept, recording) guard reads (item 1), per-object catch records
  [PropertyPointer] refusals with reason/field/class from the new detail trailer
  and ends only that object's chain (item 2), ScanCollectionScope RAII ends scan
  mode on any unwind (item 3, fixes a real l24 dangling-scan_log hazard);
  KnownPropertyClassBootstrap compares pre-computed known-name FName indices
  first, bounds the index by the verified pool, before any ToString suffix test
  (item 4a); plus review fixes F1 (RefusalReason +LayoutMissing=4/Unknown=5, all
  three noexcept Checked wrappers catch layout_offset throws — closes the
  std::terminate regression l24 introduced by removing missing_keys.attempt),
  F2 (trailer marker-order bounds check before substr), F3 (wide-suffix margin
  comment) (earlier-batch worker items 1-3; agent N applying agent-K findings
  F1-F3; typescan-r4 REPORT + CHANGES scope + review_fixes[0]).
- `typescan-r4/candidate/src/UnrealInitializer.cpp` (in part) — 78c16fb7 ->
  cac95e48 [74 total; this theme's hunk = 1 call] Single
  PublishVerifiedPoolAddress call after pool admission (item 4b): publishes the
  SeedArming-verified pool address the bootstrap bounds-check reads; why: the
  FName index bound must not trust unpinned globals (earlier-batch worker;
  CHANGES scope). Other hunks of this file belong to themes 2/3/4 (see below);
  at build time the whole file is superseded by Q's composed second hop (theme
  4).

## Theme 2 — Listener ABI + registration hardening (agents V, O, Z19, AT)

- `typescan-r4/candidate/include/Unreal/UObjectArray.hpp` (original = port root)
  — b01bf4ef -> 69a2391a [28] ABI FIX (item 3): added `virtual size_t
  GetAllocatedSize() const { return 0; }` as the LAST virtual of
  RC::Unreal::FUObjectDeleteListener; why: UE 5.6.1 engine dispatches slot-4
  GetAllocatedSize (engine UObjectArray.h:950 via GetDeleteListenersAllocatedSize
  :1471) and our vtables ended at slot 3, so the engine read one word past the
  vtable — a root-cause-grade crash defect; FUObjectCreateListener verified
  already correct (agent O on agent-V ABI ground truth; review_fixes[2]).
- `typescan-r4/candidate/src/UObjectArray.cpp` (original = port root) —
  171560a9 -> ea7221e8 [433; layered O -> Z19 -> AD -> AT -> AU] (a) Item 2
  hardening: Add/RemoveUObjectDeleteListener sanity gate (Num in [0,Max],
  Max<=256, alignment, no wrap, null-Data only when empty) with clean refusal
  instead of throw on duplicate/missing-layout, Data/Num/Max dumps pre/post,
  file-local mutex over writers, GMalloc-bound check before growth, per-event
  RISK logging of the residual realloc race (engine-locked registration path
  proven unreachable: COREUObject_API unresolvable in the static PS5 image) —
  why: the l24 path could throw/derail init on a corrupt or racing header; (b)
  E1 compile fix: ill-formed `const_cast<void*>(GetData())` -> `static_cast`
  (Z19, from Z13 finding E1); (c) refusal-6 ImplausibleDataForGrowth
  heap-plausibility screen, later hoisted ABOVE Contains so it is a true
  pre-read guard — Contains dereferences Data, so a non-canonical live buffer
  would fault before refusing (AT, from AK finding 2; 103/103 + 115/115
  harness); (d) GMalloc slot reads on both log lines per the final AB §7 spec
  (AU FIX 2 — see theme 4). Owner chain: O (review_fixes[2]) -> Z19
  (review_fixes[3]) -> AD (review_fixes[4]) -> AT (review_fixes[6]) -> AU
  (review_fixes[7]); final sha ea7221e8 verified in tree.
- `typescan-r4/candidate/src/ClassListener.cpp` (original = port root) —
  a579b272 -> a8e9b6f4 [114] Throttled (first call + >=10s) engine-dispatch
  proof markers in FClassDeleteListener::NotifyUObjectDeleted and
  OnUObjectArrayShutdown, mirrored to supplier-diag events (l25_dl_*); why: the
  repaired slot-4 vtable must be observably dispatched before the listener path
  is trusted (agent O item 3; review_fixes[2]; verified agent Z12).
- `typescan-r4/candidate/src/UnrealInitializer.cpp` (in part) — the PostInitialize
  pre/post registration markers with elapsed ms around both delete-listener
  registrations (agent O item 2); behavior change vs l24: registration refusal
  no longer aborts init.

## Theme 3 — Variant machinery (B gates, switches)

- `typescan-r4/candidate/src/UnrealInitializer.cpp` (in part) — both
  delete-listener registrations wrapped in `#if
  !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)` with the mandated skip log
  (agent O), and (AO G3) the whole bUseUObjectArrayCache block (searcher pools +
  AddUObjectCreateListener + class-delete registration) also suppressed under
  variant B with a forced-off log line; why: variant B must isolate the
  delete-listener subsystem without ini changes, while variant A stays
  behavior-identical (agent AO from agent-AC audit ranks G1/G3;
  review_fixes[5]).
- `typescan-r4/candidate/src/LuaMod.cpp` (in part) — FLuaObjectDeleteListener
  registration (first statement of LuaMod::on_program_start) wrapped in the same
  `#if` with its skip log (AO G1, audit rank 1 mandatory); why: the Lua-held
  listener registration is the other half of the A/B isolation. Whole-file sha
  transition under theme 4 (P's tags dominate the delta); AO also converted all
  7 R3-marshal GetFullName() print sites to FName-index+pointer form (G2) in
  both variants — removes the ProcessEvent(Conv_NameToString)+GMalloc FString
  cluster from the marshal path.
- Driver-level (not a CHANGES row): `build_variants.py` builds twin variants
  A (no define) / B (-DMODBRIDGE_L25_SKIP_DELETE_LISTENER) with gates G4/G5
  (token present; define confined to units luamod + unreal-unrealinitializer),
  G9 (variants differ), G10 (object deltas confined to define units); variant-B
  carries the standing UNSAFE-for-long-term marker (agent R).

## Theme 4 — Instrumentation (phase tags, GMalloc logging, copyin liveness)

- `typescan-r4/candidate/src/UE4SSProgram.cpp` (original = l23 thread source,
  the l24-consumed pin) — 030407d7 -> 352da81b [14] l25 item 6 phase tags:
  "[l25] on_program_start entered" + "[l25] event loop started" each paired
  with a diagnostic_event; why: crash sessions never proved these phases ran —
  the tags bracket the exact stall region (agent P; review_fixes[1]; verified
  agent Z4 34/34).
- `typescan-r4/candidate/src/LuaMod.cpp` (original = l23 thread source) —
  fc9589db -> cef8d02f [107; P then AO] Phase tags: once-only "[l25] marshal
  hook first invocation (carrier fired)" + r3_marshal_first_invoke at the top
  of funcptr_marshal_hook, >=1s-throttled allocation-free r3_marshal_pumped
  after the IsInGameThreadRaw gate (Claude's exact recommendation), >=1s
  "[l22] ExecuteInGameThread queued depth=N" + eigt_queue_depth at the enqueue
  site; why: the R3 pump arms at on_program_start which is unreachable while
  init stalls — the cheapest liveness probe for the single root cause; existing
  R3-arm and per-closure EIGT lines deliberately not duplicated
  (agent P review_fixes[1]; + AO G1/G2 hunks, theme 3).
- `typescan-r4/candidate/src/UObjectArray.cpp` (in part) — item 4 GMalloc
  logging: pre-Append "header pre:" extended with growth_required, GMalloc
  variable address/value/instance/vptr (safe byte-wise reads behind a
  plausibility screen) + post-Append "GMalloc post:" snapshot + refusal code 6
  + l25_dl_gmalloc_pre/post diag tags (AD, PENDING-ALIGNMENT form), superseded
  by AU FIX 2 per AB §7: both lines extended in place with raw vtable slot
  reads +0x28/+0x30/+0x38/+0x40/+0xd0 (guarded, never called through);
  interpretation table in-code (+0x38==0x4b8a20 is the pre-fix mis-dispatch
  signature); why: a changed instance/vptr across Append is direct evidence
  for/against the GMalloc-virtual-call fault hypothesis (AD review_fixes[4];
  AU review_fixes[7] on AB §7 / AH).
- copyin-probe overlay, UE4SS side (agent Q; wired by build_variants.py
  `compose_copyin_probe`): base = the typescan-r4 candidate; Q's recorded base
  c4364dd7 was stale after the O/AO revisions, so the pure-addition patch was
  rebased onto the revised base — composed second hop
  `2026-10-08-l25-build-variants/staging-copyin-probe/UnrealInitializer.cpp` =
  cac95e48 + 96 added lines -> sha 12c99461 (verified in staging + PLAN; mode
  "patch-rebased-onto-revised-base"; Q's ready candidate 125a0471 kept as
  immutable deliverable). WHAT: one-time post-admission probe-state line
  ([Seeds][L25] copyin probe state / copyin_probe_liveness_state=3) +
  throttled copyin_probe_fire_post_admission census + note_probe_call on the
  admission-ops side; WHY: prove kernel-write copyin dispatch actually STOPPED
  after pool admission (the GATE-1 oracle context) without changing probe
  behavior (agent-Q MANIFEST/WIRING; observation-only, applies to BOTH
  variants; second-hop override of unit unreal-unrealinitializer, no import
  delta — diagnostic_event already undefined in the baseline object).
- copyin-probe overlay, bridge side (agent Q; wired at packaging by
  `l25_packaging_driver.py` outer_bridge substitution after pin-checking the
  l24 base): `lineage/lifecycle/runtime_start_bridge.cpp` 89e4b643 -> f510c014
  [+72 lines] one-time bridge-side counters (gate_armed, bank_retired,
  parent_fd/sinks_open, nonce_used, attempts_total, gate_released,
  attempts_late_total, init-phase fire census) + throttled post-retirement fire
  events inside the existing exported bridge functions; WHY: the bridge owns
  the probe lifetime, so retirement must be proven from its side too; all
  additions static/local, no new imports (agent-Q MANIFEST File 2).

## Theme 5 — Hygiene (carried pins, CHANGES discipline, E1/E3 build fixes)

- 9 container rows, all "carried" (byte-identical, zero l25 delta; originals =
  l24 typescan-r3 candidates): `MemberVariableLayout_HeaderWrapper_FMapProperty.hpp`
  457dc903->457dc903, `MemberVariableLayout_SrcWrapper_FMapProperty.hpp`
  3170f072->3170f072, `src/Property/FMapProperty.cpp` 68139f5e->68139f5e,
  `MemberVariableLayout_HeaderWrapper_FSetProperty.hpp` e466e8ca->e466e8ca,
  `MemberVariableLayout_SrcWrapper_FSetProperty.hpp` 2bbaeae5->2bbaeae5,
  `src/Property/FSetProperty.cpp` 7a294926->7a294926,
  `MemberVariableLayout_HeaderWrapper_FArrayProperty.hpp`
  661b69c1->661b69c1, `MemberVariableLayout_SrcWrapper_FArrayProperty.hpp`
  a2cc1956->a2cc1956, `src/Property/FArrayProperty.cpp`
  0657a2ef->0657a2ef; WHY rows exist at all: the one-hop law pins every
  l24-consumed path the l25 build must keep consuming onto the l25 tree; the
  wrappers expose GetKeyProp/GetValueProp/GetElementProp resolving through the
  corrected profile offsets (item 5 consumer side; agent-M §3).
- `src/CustomProperty.cpp` — 66be0042 -> 66be0042 [carried] l24 content (the
  TU-local owned-object writer for the two Array::GetInner assignments);
  re-pinned only (earlier l23-r2 work retained).
- `src/ScriptHookThunks.cpp` (original = r5-source-integration pin) — ecec9159
  -> ecec9159 [carried, staged only] deliberately unedited: no l25 item required
  a change once P's non-duplication rule was honored (the SetFuncPtr swap
  carrier markers already exist); staged so the l25 tree owns the unit row
  (agent P review_fixes[1] note).
- `thread/CHANGES.json` — 0 rows; placeholder scope rewritten l25-accurate
  (item 7 CHANGES discipline: stale l24 text removed; prescribes the exact
  original paths any future thread row must key on) (earlier-batch worker;
  agent-M §1).
- E1 (compile hygiene): fixed inside the UObjectArray.cpp row above (Z13
  finding, Z19 one-line static_cast fix; host + prospero syntax verified).
- E3 (header-routing hygiene, build machinery, not a row):
  `build_variants.py` `header_route_check` gate G12 proves every reachable
  include spelling of every replaced header resolves to the l25 candidate (never
  the port-root original), G2r proves no CHANGES row is a silent no-op (every
  original classified unit-source / dep-universe / overlay-target /
  alias-name-digest / lineage-data), G2s proves unit-source rows land in the
  affected set, G12b proves every consumer of every replaced header recompiles,
  G7 proves the raw import set is unchanged vs l24 (agent R driver; Z13 E3;
  Z9 risk framework).

## Row inventory and totals

- Declared CHANGES rows: 24 = 20 (typescan-r4) + 4 (profile) + 0 (thread).
  Matches PLAN.json `changes=24`, `added_headers=4` (NativePropertyPointerGuard
  .hpp/.inl, NativePropertyReadBackend.inl, NativeTypeScanPolicy.hpp — new
  guard/scan-support headers, earlier-batch), `header_aliases=27`.
- Content deltas vs the l24 built runtime: 12 CHANGES rows (8 typescan-r4:
  TypeChecker, UnrealInitializer, UE4SSProgram, LuaMod, UObjectArray.hpp,
  UObjectArray.cpp, ClassListener, FMalloc FunctionBody; 4 profile) + 2 Q
  overlay targets (composed UnrealInitializer second hop — supersedes the row's
  candidate at build time — and the packaging-side bridge). 12 rows are
  byte-identical carried pins (9 container + CustomProperty + ScriptHookThunks
  + UnrealVirtual506; the last is a deliberate compile-forcing row, theme 1).
- Every changed candidate hash in both trees was re-hashed during this audit:
  all match CHANGES.json (typescan-r4 8/8, profile 4/4); all 24 originals match
  their pins; the composed overlay matches 12c99461.

## Unit-count reconciliation (12 / 162 / 374)

- 12 units carry direct unit-source rows (PLAN `unit_row_units`, gate G2s):
  unreal-flat-typechecker, unreal-unrealinitializer, unreal-uobjectarray,
  unreal-flat-classlistener, luamod, program, native-script-hook-thunks,
  lua-custom-property, unreal-src-property-fmapproperty,
  unreal-src-property-fsetproperty, unreal-src-property-farrayproperty,
  unreal-src-versionedcontainer-unrealvirtualimpl-unrealvirtual506. Three of
  these are byte-identical rows (custom-property, script-hook-thunks,
  unrealvirtual506) — they still force fresh recompiles (unrealvirtual506 by
  design for the FMalloc shadow; the property units re-verify the l24 objects
  against l25 headers).
- 162 units recompile fresh per variant per pass (PLAN `affected_units=162`;
  374 selected total, 212 retained). Decomposition: 12 direct + 150 pulled in
  solely as consumers of replaced headers. Consumer census (PLAN
  `header_consumers`, gate G12b): UObjectArray.hpp = 162 (the ENTIRE affected
  set — the slot-4 vtable ABI repair forces a whole-fleet recompile to avoid a
  mixed-header ODR fleet), MemberVariableLayout_HeaderWrapper_{Map,Set,Array}
  = 125 each, SrcWrapper_* = 1 each, RuntimeBuildProfile.hpp = 2,
  CompiledNativeLayoutDeclarations.hpp = 1, JoinedLayoutPreflight.hpp = 1.
  Since UObjectArray.hpp alone covers all 162 and the affected set is exactly
  162, every other header's consumers are a subset — "12 affected units + 162
  consumer recompiles" is fully explained: 12 row-driven + 150 ABI-header
  consumers.
- Gate G2t additionally proves the 4 listener-instrumentation units are fresh
  (unreal-uobjectarray, unreal-flat-classlistener, unreal-unrealinitializer by
  rows; lua-luauobject via the UObjectArray.hpp name+digest alias), closing the
  Z9/O silent-no-op risk. Q's second hop redirects only
  unreal-unrealinitializer's effective source (no unit-count change). Variant B
  adds the define to exactly luamod + unreal-unrealinitializer (G5/G10).

## What did NOT change (verified by hash during this audit)

- Port root untouched: the five port-root originals (UObjectArray.hpp
  b01bf4ef, UObjectArray.cpp 171560a9, ClassListener.cpp a579b272,
  UnrealVirtual506.cpp 8e407b89, FMalloc FunctionBody 95488869) are
  byte-identical to their pins; the corrected FMalloc table and listener ABI
  live only in the l25 candidate tree (shadowing -I + unit row).
- l23 originals pinned: all four l23 profile originals (056cf45a, 7a289aba,
  900bc7ac, 32c9856c) and the l23 thread sources (UE4SSProgram 030407d7,
  LuaMod fc9589db) unchanged; l24 typescan-r3 originals (dcebc6e6, 78c16fb7)
  unchanged; r5 ScriptHookThunks pin ecec9159 unchanged.
- Engine files unmodified: no engine binary/self is touched by any l25 part;
  the profile fixes are value corrections consumed at runtime startup (image
  sha 3b6cfad5... unchanged); no console contact anywhere in the l25 tree
  (every receipt: console_contact=false).
- The Lua mod main.lua is untouched: no l25 part writes or stages any .lua;
  the main.lua copies under outputs/ are frozen console-test evidence of the
  unmodified Nexus mod.
- Verified-but-unpublished offsets stay unpublished (UObjectDeleteListenersCritical
  0x78, ObjObjectsCritical 0x30, ObjAvailableList 0x48): no schema/binding rows
  exist for them and NativeLayoutBinding throws on unknown members — recorded
  in profile CHANGES `listener_unpublished_offsets` instead (AS §2).
- Copyin-probe GATING deliberately NOT wired (Z15 l26 proposal only; l25 keeps
  observation-only liveness in both variants), variant-C (AE) not adopted as a
  variant, kstuffSharp/quarantined machinery not involved.

## Tree-level additions that are NOT runtime rows (for completeness)

`build_runtime.py` (base driver), `build_variants.py` (A/B twin driver, Q
wiring, E3 gates), `l25_packaging_driver.py` (bridge substitution, staged
packaging), `gating-proposal/` (Z15, l26 proposal), `variant-c-proposal/` (AE),
`profile-fix-template/` (AM: aa-values-engine-binary.json +
apply_engine_binary_fix.py), `typescan-r4/target_compile.py`, `tests/`
(item-6 suites, repointed by S; 2/2), `profile/tests/` (preflight/digest gate,
PASS_HOST_ONLY_MAC). Status at snapshot: PLAN.json (16:26) green on all
verifiable gates; variant-A execute started 16:27 (pass1 objects compiling);
l25-B, twin receipts, packaging and any console proof still pending — no claim
of a built or tested l25 runtime is made here.

## Provenance index (owner -> report, all under outputs/2026-10-08-l25-prep/)

earlier-batch worker (items 1-5, 7): typescan-r4/REPORT.md + CHANGES scope;
agent-M-l25-inventory.md. Review fixes: agent-K-l25-review.md (findings) ->
agent-N-review-fixes.md. Phase tags: agent-P-phase-tags.md (verified
agent-Z4-phasetag-verification.md). Listener ABI/hardening:
agent-V-listener-abi.md -> agent-O-listener-instrumentation.md (verified
agent-Z12-listener-verification.md, layout agent-Z8-listener-layout.md). Item-4
GMalloc logging: agent-AD-item4-logging.md (audit agent-AB-gmalloc-audit.md,
edge agent-AH-gmalloc-edge-verify.md, verification agent-AK-item4-verification.md).
Code-6 guard: agent-AT-code6-guard.md. FMalloc vtable map + logspec:
agent-AU-vtable-map-fix.md. Gate extensions: agent-AC-container-audit.md ->
agent-AO-gate-extensions.md. Profile fix: agent-AA-offset-verify.md (values) +
agent-AM-profile-template.md (mechanism) -> agent-AS-profile-fix.md (applied;
consumers agent-AL-offset-consumers.md). Copyin liveness:
agent-Q-copyin-probe.md + copyin-probe/{MANIFEST,WIRING}.md. E1/E3:
agent-Z13-combined-smoke.md -> agent-Z19-e1-fix.md; driver gates agent-R-twin-build.md
(+ agent-Z9-edit-path.md one-hop law). Twin/packaging drivers:
agent-R-twin-build.md, l25_packaging_driver.py.
