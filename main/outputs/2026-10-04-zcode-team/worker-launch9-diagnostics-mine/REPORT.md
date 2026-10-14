# REPORT — worker-launch9-diagnostics-mine (2026-10-05)

Honest summary of what the launch-9 entry-log delta does and does not contain. Local-only work; no network
calls, no engine writes, no files outside the deliverables directory.

## Headline answers

**1. Timeline extracted — complete.** The last 133,450 bytes (`aligned-entry-log-snapshot.log` bytes
2210306–2343756) parse into 1,097 diagnostic lines, zero unparsed. All carry the shared
`modbridge-runtime-aligned-entry/v1 build=f6530c79…` channel prefix. They form two complete module_start
invocations (the r13 wiring attempt that refused code=1, and the 232-constructor self-contained harness that
passed 100%) with every tag, value, and byte offset in `timeline.tsv`.

**2. Decisive lease question answered — the 128-span cap was NOT hit.** The events ARE present in the delta
(said explicitly, per instruction):

```
@2270807  bridge_lease_reg_spans value=0x00000006
@2270933  bridge_lease_reg      value=0x00000006
```

spans (6) == registered (6) → cause A (cap saturation) excluded; 128 was never approached. The surrounding
events favor cause B (exec-only text pages inside the lease range — r12 source states the eboot header page is
EXEC-ONLY on this target; xwp probe rows confirm live prot bits R=1 W=2 X=4), with cause C (inter-span gaps)
possible and cause D (protection-bit semantics) disfavored. VMMAP walk itself is healthy: 10,019 entries,
clean stop reason 0, identical across three rounds.

**3. Code=1 geometry failure fully framed.** Refusal site = `derive_readable_ranges()` returning zero
intersecting intervals (`lmf_to_native_loader_facts.cpp:228-231`). Terminal chain captured verbatim: 5/5 data
windows verified+matched at the candidate base (window 5 not covered), `lmf_vmentry_cap=2(Ok)`,
`lmf_vm_scan_cap=2(Ok)`, `lmf_linkmap_cap=1(Failed)`, `lmf_provider_code=0xfffff05f` (= −4001,
`kRefusalExecHandleRawZeroIngame` — the known in-game g1 exec_handle refusal, not a scan failure),
`lmf_identity_source=2 (VmScan)`, `runtime_start_failed=0x1` @2276081. The defining contradiction: the same
attempt's bridge registration had found 6 readable spans intersecting the extent, while the conversion's
snapshot derived zero — the conversion-side extent/anchor-mapping inputs differ, which is the facts-geometry
worker's lead. `bridge_cfg_gamedir`/`bridge_cfg_workdir` are ABSENT from the whole delta (structurally
expected for the entry: its refusal precedes the attach point). All 9 historical `runtime_start_failed`
values in the log map 1:1 (in order) to the 9 UE4SS-mirror failure texts; the launch-9 entry state is #9,
mirror-stamped `[t=44372082ms pid=273]`.

## What the delta does NOT contain (explicit)

- **No second `bridge_lease_reg` pair.** The composition plugin's own launch-9 registration is not separately
  visible in this channel for launch-9 as an r13-instrumented pair. The adjacent pre-delta attempt
  (`runtime_start_failed=0x2` @2208542, `bridge_lease_reg=6` @2204570 with NO spans tag) is attributed to the
  composition (session 234) by adjacency + exact failure-text match with LAUNCH9-RESULT — an inference, not a
  channel-stamped fact. Its build is pre-r13 (no spans counter) and pre-r12 (no cfg tags after a successful
  conversion); r10 vs r11 cannot be distinguished in-log (identical tag sets). Launch-8 produced the same
  failure text, so the mirror's single unregistered-bytes line (UE4SS-log-final.txt line 27, no pid stamp)
  cannot be launch-attributed from local evidence alone. If per-plugin durable registration counts are
  required going forward, they need a sink that stamps plugin identity (this channel stamps only the channel
  owner's prefix).
- **No numeric conversion-side extent or anchor_mapping_count events.** The conversion's extent and interval
  count are never emitted; only the bridge-side `bridge_vmentry_entries` (10019 / 10002) exists. Recommend the
  facts-geometry fix add `lmf_extent_base/extent_end/anchor_count` tags so the next run self-diagnoses.
- **No Resolution-B attach evidence in-log.** `bridge_cfg_*` absent everywhere in the launch-9 window.
- No `runtime_start_ok` in the whole log (no plugin ever fully started; consistent with LAUNCH9-RESULT).

## Caveats / honesty

- Baseline discrepancy: stated baseline 2,213,030 B vs delta size 133,450 B are inconsistent by 2,724 B; the
  baseline offset falls mid-constructor-chain inside the delta's first attempt. Extraction used the clean
  attempt boundary at 2210306. All offsets in deliverables are absolute file offsets.
- Attempt C attribution (the 232-constructor pass) matches LAUNCH9-RESULT's harness description numerically;
  whether it was the entry's fallback re-entry or the path_test DIAG module_start cannot be proven from the
  shared-prefix channel — both are consistent with the line evidence.
- The composition build being r10/r11-lineage while the entry is r13 means the launch-9 "composition passed
  registration with the r11 fix" claim rests on its failure occurring AFTER registration (code=2 is
  MRS_ERR_CONTEXT, post-conversion), not on span instrumentation — the instrumented spans counter did not
  exist in that binary.
- I decoded enum values (ProbeResult, IdentitySource, MRS_ERR_*, −4001) from local source copies
  (comp6-integration, worker-composition-prep staging, worker-module-start-wiring); citations in CLAIM.json.

## Deliverables

- `CLAIM.json` — per-finding file:byte-offset provenance with evidence citations
- `DIAGNOSTICS.md` — the three findings with bounded verbatim extracts and the 9-attempt mapping table
- `timeline.tsv` — 1,097 rows: order, abs_byte_offset, delta_line, component, build, tag, key=value
- `REPORT.md` — this file
