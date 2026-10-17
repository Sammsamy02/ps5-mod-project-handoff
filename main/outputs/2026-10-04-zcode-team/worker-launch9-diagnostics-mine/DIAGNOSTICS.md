# Launch-9 diagnostics mine — entry-log delta (worker-launch9-diagnostics-mine)

Local-only extraction. Source: `outputs/2026-10-04-zcode-controller/launch9-evidence/aligned-entry-log-snapshot.log`
(2,343,756 B). Delta extracted = last 133,450 B = bytes **[2210306, 2343756)** — which begins exactly at a
`module_start_entered` line (clean attempt boundary). All 1,097 delta lines parsed; full ordered table with byte
offsets in `timeline.tsv`.

Boundary note (honesty): the stated session-eve baseline (2,213,030 B) and the stated delta size (133,450 B) are
mutually inconsistent by 2,724 B. The baseline offset falls **inside** the delta's first attempt (mid constructor
chain, between `constructor_begin` @2212935 and its `constructor_return` @2213056). The 133,450 B tail used here is
the clean attempt boundary and is a superset of the baseline-relative delta.

Every line in the delta carries the shared-channel prefix
`[PPSA33286] modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0` — the diagnostic_event
stream of the composition-family bridge code flows into this shared aligned-entry log (established since launch-1).

---

## Finding 1 — Complete launch-9 diagnostic sequence

The delta contains **two complete module_start invocations** plus interleaved worker-thread events:

| attempt | offsets | shape |
|---|---|---|
| B (r13 entry start) | 2210306–2277722 | probes → **235 ctors (0xeb)** → worker → `runtime_wiring_begin` → 3 VMMAP rounds + lease registration + stat probes + anchor windows + `lmf_*` → **`runtime_start_failed value=0x1`** @2276081 → `module_start_return` → worker Lua harness (all zero) |
| C (self-contained harness) | 2277845–2343755 | probes → **232 ctors (0xe8)** → FProperty/FField trace phases (0x100…0x401) → `full_runtime_constructors_complete` → **`module_start_return value=0x0`** @2342257 (no wiring stage, no failure) → worker Lua harness (all zero) |

Attempt C matches LAUNCH9-RESULT's harness description verbatim ("232 constructors, module_start returned 0,
live Lua VM, 100% PASS" — `lua_load_result`/`lua_call_result`/`lua_result_match`/`lua_state_closed`/
`lua_memory_check_passed`/`buffered_lua_result` all `0x0`, twice).

Tag counts in the delta (distinct = 63): constructor_begin/return 467 each (235+232), bridge_vmentry_entries 3,
bridge_lease_reg_spans **1**, bridge_lease_reg **1**, lmf_* 1 each, runtime_start_failed **1**, anchor_cov 6,
anchor_match 5, fproperty_phase 10, ffield_* 14 each. Full per-line table: `timeline.tsv`.

---

## Finding 2 — DECISIVE: lease registration values (cause A vs B/C/D)

Verbatim, from the entry's r13 wiring stage (attempt B):

```
@2270807  bridge_lease_reg_spans value=0x00000006
@2270933  bridge_lease_reg      value=0x00000006
```

**spans = 6, registered = 6 → equal. The 128-region cap was NOT hit. Cause A (cap saturation) is excluded.**

Context events immediately before (attempt B):

```
@2269696  bridge_vmentry_entries value=0x00002723   (10019 entries parsed)
@2269822  bridge_vm_walk_stop    value=0x00000000   (0 = clean end, off==len; not 1=malformed, not 2=truncated)
@2270339  bridge_kve_distinct0..3 value=0x0          (no non-zero kve_path prefixes in ANY record)
@2270661  bridge_kve_eboot       value=0x00000000   (no 'eboot' bytes in any path region)
@2270561  bridge_exepath_valid   value=0x00000000
@2270685  bridge_exepath_sig     value=0x00000000
```

Sub-case assessment:

- **Cause B (exec-only text pages inside the lease range) — favored.** r12 source (line 113) states outright:
  *"The eboot header page is EXEC-ONLY on this target (VMMAP [0x400000, …)"*, and r13's registration comment
  (lines 470–478): *"Exec-only text is NOT registered readable — the D2/text_base question stays open upstream."*
  The registration filter is `protection & kVmProtRead`; live VMMAP protection decode from the xwp probe rows
  confirms **R=1, W=2, X=4** on this firmware (0x4=X, 0x5=RX, 0x7=RWX, 0x3=RW, 0x1=R; UE4SS mirror lines 32–43).
  Any lease page with prot=0x4 inside the image extent is unregistered by construction.
- **Cause C (unmapped gaps)** — possible: registration clamps each readable span to [extent_begin, extent_end)
  and quantizes outward per span (r11, kPage=0x4000); nothing registers the gaps between spans. A lease walk
  crossing an inter-span gap throws the same way. Not directly observable in-log (no per-gap events exist).
- **Cause D (protection-bit semantics)** — disfavored: the xwp rows show the kernel reporting the expected
  bit pattern on real records in this very process.
- VMMAP walk health is not in question: 10,019 entries, clean stop, three consecutive rounds identical.

Independent corroboration from the adjacent pre-delta attempt (see Finding 3b): the composition's attempt
registered **6** regions as well (`bridge_lease_reg value=0x00000006` @2204570, no spans tag — pre-r13 build).
Two independent instances, same count 6, cap never approached. This matches the r13 prediction comment:
"launch-7 evidence predicts spans == 6 on this target" — confirmed exactly.

**Conclusion for the lease root-cause worker:** the unregistered bytes are a *coverage semantics* problem
(exec-only text and/or inter-span gaps inside the derived lease ranges), not a cap problem. Registration
evidence is present in this channel; per the REPORT the composition's own registration for launch-9 is
attributed by adjacency, not by a durable per-plugin sink.

---

## Finding 3 — The entry's code=1 geometry refusal, in context

Terminal chain of attempt B (verbatim tail):

```
@2274097  anchor_cov    value=0x00000001   window 0: covered=1
@2274211  anchor_match  value=0x00000001   window 0: matched=1
@2274327  anchor_cov    value=0x00010001   window 1: covered=1
@2274441  anchor_match  value=0x00010001   window 1: matched=1
@2274557  anchor_cov    value=0x00020001   window 2: covered=1
@2274671  anchor_match  value=0x00020001   window 2: matched=1
@2274787  anchor_cov    value=0x00030001   window 3: covered=1
@2274901  anchor_match  value=0x00030001   window 3: matched=1
@2275017  anchor_cov    value=0x00040001   window 4: covered=1
@2275131  anchor_match  value=0x00040001   window 4: matched=1
@2275247  anchor_cov    value=0x00050000   window 5: covered=0 (no match event)
@2275361  anchor_c_either value=0x00000001
@2275480  lmf_vmentry_cap    value=0x00000002   ProbeResult::Ok
@2275599  lmf_vm_scan_cap    value=0x00000002   ProbeResult::Ok
@2275718  lmf_linkmap_cap    value=0x00000001   ProbeResult::Failed (DT_DEBUG route refused in-game)
@2275837  lmf_provider_code  value=0xfffff05f   int32 -4001 = kRefusalExecHandleRawZeroIngame
@2275958  lmf_identity_source value=0x00000002  IdentitySource::VmScan
@2276081  runtime_start_failed value=0x00000001  (MRS_ERR_CONVERSION)
@2276205  module_start_return  value=0x00000000
```

Value decodes (from source, cited in CLAIM.json): `ProbeResult {NotBound=0, Failed=1, Ok=2}`;
`IdentitySource {None=0, LinkMap=1, VmScan=2}`; `0xfffff05f` = −4001 =
`LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame` — the known in-game g1 exec_handle raw-0 refusal,
recorded as "last nonzero OS return"; it is NOT the scan's own failure code.
`MRS_ERR_CONVERSION=1`, `MRS_ERR_CONTEXT=2` (runtime_start_bridge.h lines 37–38) — so code=1 is the
conversion gate and code=2 is the context-build throw (where the lease ReadLease throw lands).

The refusal site is `lmf_to_native_loader_facts.cpp:228-231`: `derive_readable_ranges()` returned false —
its filter (line 84–96) drops non-readable mappings and anything not intersecting [extent.base, extent.end).
**The contradiction that defines the bug:** in the same attempt, minutes of log apart, the bridge's own
registration query (same `bridge_query_vmentry`, same candidate constants) found **6 readable spans
intersecting the extent** (@2270807), while the conversion's snapshot derived **zero** intersecting intervals.
The anchor windows prove real image memory at the candidate base (5/5 verified). So the conversion-side inputs
differ — extent derivation (VmScan provenance: `chosen_base + PT_LOAD-vaddr chosen_extent`, mode-1 bias) and/or
the supplier's retained `anchor_mappings` content — exactly the "standalone inputs must differ" hypothesis in
LAUNCH9-RESULT. Root-causing belongs to worker-facts-geometry-fix; these are the numbers it needs.

**Configured-roots markers:** `bridge_cfg_gamedir` / `bridge_cfg_workdir` are **ABSENT from the entire delta**
(and from the adjacent composition attempt). For the entry this is structurally expected — the attach point
(r13 lines 668–671) sits after the conversion gate that refused. Consequence: the launch-9 log contains **no
direct Resolution-B attach evidence** for either plugin; LAUNCH9-RESULT's "passed configured roots" for the
composition rests on the runtime-status detail text, not on these tags (they don't exist pre-r12, and the
adjacent composition attempt's build is pre-r12 by tag evidence).

**Mappings-count / extent values logged:** none numerically for the conversion side (no tag emits the
conversion's extent or its anchor_mapping_count; only the bridge-side `bridge_vmentry_entries=10019` exists).
Adding those would make the next geometry failure self-diagnosing.

### Attempt sequence mapping (all 9 `runtime_start_failed` in the full log ↔ UE4SS mirror lines, in order)

| # | entry-log offset | value | UE4SS-log-final.txt line | failure text |
|---|---|---|---|---|
| 1 | 1303207 | 0x1 | 6  | no image anchor (status NoImageAnchor); refusing conversion; dlopen(NULL) is never a fallback |
| 2 | 1431014 | 0x1 | 9  | (same NoImageAnchor text) |
| 3 | 1558821 | 0x1 | 12 | (same) |
| 4 | 1686985 | 0x1 | 15 | (same) |
| 5 | 1815648 | 0x1 | 18 | observed executable path is empty; refusing to fabricate game_executable_path |
| 6 | 1945333 | 0x2 | 21 | Native module and executable paths must name existing regular files |
| 7 | 2076253 | 0x1 | 24 | observed executable path is empty; refusing to fabricate game_executable_path |
| 8 | 2208542 | 0x2 | 27 | **Owned read range includes unregistered bytes** |
| 9 | 2276081 | 0x1 | 45 | **[t=44372082ms pid=273] no observed mapping interval intersects the module image extent with valid geometry** |

Row 9 is the launch-9 entry terminal state (mirror correlation given verbatim in the mirror line). Row 8 is the
only in-log code=2 wiring sequence and is the adjacent composition attempt (Finding 3b) — its refusal text
matches LAUNCH9-RESULT's session-234 observation verbatim; the alternative attribution to launch-8 (same text)
cannot be excluded from local evidence alone because the mirror line carries no pid/timestamp stamp.

### Finding 3b — the adjacent composition attempt (pre-delta, launch attribution inferred)

```
@2203459  bridge_vmentry_entries value=0x00002712   (10002 entries — different snapshot/session from B's 10019)
@2203585  bridge_vm_walk_stop    value=0x00000000
@2204570  bridge_lease_reg       value=0x00000006   (NO bridge_lease_reg_spans line → pre-r13 build)
@2205801  bridge_exepath_source  value=0x00000003   (configured-fallback route served)
@2205926  bridge_exepath_sig     value=0x7461642f   ('/dat' — path beginning "/data…")
@2206048..2207159 third VMMAP round (10002, clean)
@2207159..2208423 anchor windows 0-4 covered+matched, window 5 covered=0
@2208423  anchor_c_either value=0x00000001              (NO lmf_* failure events → conversion SUCCEEDED)
@2208542  runtime_start_failed value=0x00000002        (MRS_ERR_CONTEXT — the lease-read throw)
@2208666  module_start_return  value=0x00000000
@2208789..2210183 worker Lua harness, all zero
```

Build fingerprint: `bridge_lease_reg` without `bridge_lease_reg_spans` → pre-r13; no `bridge_cfg_*` after a
successful conversion → pre-r12 (tags introduced in r12); r10 vs r11 indistinguishable (identical tag sets).
So the composition binary is r10/r11-lineage — it PREDATES the r13 spans counter, meaning **the launch-9
composition build never logged spans-vs-registered itself**; only the entry's r13 instance did (6/6).
Sequence shape: conversion succeeded → context build → lease read threw code=2. This is the fail the r11
quantization was supposed to fix, persisting in a quantized-or-older build — consistent with the cause-B/C
conclusion above (quantization fixes only the end-partial-page case, not exec-only text or gaps).

---

## Raw-extract index

- Full ordered table (1,097 rows): `timeline.tsv` (order, abs_byte_offset, delta_line, component, build, tag, key=value)
- Per-finding file:byte-offset provenance: `CLAIM.json`
- Verbatim extracts above are bounded to the diagnostic tag lines; constructor begin/return pairs
  (467 lines of repetition) are omitted from prose and present in timeline.tsv.
