# ORACLE — launch-10 acceptance evidence table (worker-launch10-acceptance-criteria)

Mission: the TEST ORACLE the controller reads launch-10 evidence against. Launch-10 runs ONE
rebuilt `supplier_composition` PRX (geometry fix + ARMED SeedTable 4/4 + GMalloc V1/V2
validators + read-path guard; r14 lease hardening if it landed compile-clean) + `path_test`
only (2-plugin ini `1216aab6…`; aligned_entry and all probes `=0`). All predictions are
source-derived, verbatim where the source is literal, pattern-marked where a runtime value
fills the line. Honest bounds: nothing here is an outcome promise; only the console launch
is evidence.

Source pins (short names used below; every pin is `FILE:LINE` against the on-disk staging
copies — see CLAIM.json `sources` for absolute paths):

| short | file |
|---|---|
| `UI` | `worker-launch10-readpath-guard/staging/UnrealInitializer.cpp` (2100 lines) |
| `SV` | `worker-launch10-readpath-guard/staging/SeedValidation.hpp` |
| `ST` | `worker-launch10-readpath-guard/staging/SeedTable.h` |
| `BG` | `worker-facts-geometry-fix/staging/runtime_start_bridge_r13.geometrydiag.cpp` |
| `GF` | `worker-facts-geometry-fix/staging/live_module_facts.geometryfix.cpp` |
| `BR14` | `worker-lease-coverage-fix/runtime_start_bridge_r14.cpp` (CONDITIONAL: only if the r14 hardening landed in the launch-10 rebuild) |
| `EN` | `worker-module-start-wiring/runtime_module_entry.c` (r4/r7 freshness variant: `runtime_module_entry_r4.c`) |
| `ENH` | `worker-module-start-wiring/runtime_start_bridge.h` |
| `PT` | `worker-minimal-path-test/src/path_test_entry.c` + `src/generated/pt_config.h` |
| `MFA` | `worker-f-r2-fix/composition/vendored/modforge_activation.c` |
| `OWN` | runtime-ack owner (`worker-runtime-ack/host-test/apply/.../runtime_owner.cpp`) |
| `PB` | `worker-first-run-playbook/FIRST-RUN-PLAYBOOK.md` (indirect pins into UE4SSProgram.cpp / LuaMod.cpp — secondhand, marked) |
| `GEO` | `worker-geometry-reference/GEOMETRY.md` (span facts, coordinator addition) |
| `LAT` | `worker-injection-latency/LATENCY-ANALYSIS.md` (ploader cap) |
| `INIL` | `worker-final-ini-layout/FINAL-INI-LAYOUT.md` (ini order) |

Diagnostic-event sink for launch-10 (controller decision was opportunistic, `worker-diagnostic-sink/REPORT.md:1-40`):
- If the compile-gated supplier sink shipped: every `diagnostic_event(tag, value)` ALSO appends
  `{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":N,"pid":P,"tag":"…","value":D,"value_hex":"0x…","t_ms":MS}`
  to `/data/modbridge-supplier-diag.jsonl` (durable, pid+t_ms stamped). Read THIS first.
- Else: the legacy shared channel only — `/data/townfall_runtime_aligned_entry.log`, lines
  `[PPSA33286] modbridge-runtime-aligned-entry/v1 build=f6530c79… <tag> value=0x…`
  (`worker-diagnostic-sink/ANALYSIS.md:38-47`) — append-only across ALL launches, no
  timestamps, marker collision documented; attribute via ordering + value shapes only.

Runtime-status sink `/data/modbridge/roots/PPSA33286/runtime-status.txt` (`MFA:602-618`,
`PB:§1.3`): `mfa_status_open` = `O_TRUNC` — every open REPLACES the file; one file per
session, LAST-WRITER-WINS across sessions; header
`modforge-runtime-status\t1\nsession\t<pid>\t<monotonic-ms-at-open>`; state lines
`state\t<seq>\t<state>\t<code>\t<detail>`; states exactly `activated`, `engine-ready`,
`mods-started`, `failed`, `stopped` (`OWN:171-176,187`); detail ASCII-sanitized (`?` for
control bytes), 512-byte line cap. Freshness: session ms is CLOCK_MONOTONIC — compare
against the launch monotonic epoch / PATH_TEST stamp (`post_launch_check.py:97-98,348`).
Two independent openers exist (ack sink inside prepare; entry terminal
`write_status_failed` `EN:198-219`) — a terminal entry write TRUNCATES earlier ack lines of
the same launch.

UE4SS.log sink `/data/plugins/UE4SS.log`, append-only: baseline lines (PATH_TEST /
ACTIVATION_TEST / DIAG) precede everything from this launch; composition entry failure
lines are freshness-prefixed `[t=<ms>ms pid=<pid>] modbridge module_start failed: <stage>
code=<n> detail=<sanitized>` (`EN4:198-206`, regex `post_launch_check.py:83`); runtime
`Output::send` lines follow the program banner.

---

## 1. Ordered state table

Class key: **EXPECTED-SUCCESS** (the launch-10 win path) · **EXPECTED-REFUSAL** (a NEW named
refusal that is itself diagnostic info) · **REGRESSION** (recurrence of a known blocker or
wrong-build evidence — controller STOP) · **DORMANT** (path compiled out or unreachable on
the armed build; its appearance is an identity discriminator) · **CONDITIONAL** (only if the
r14 hardening shipped / only on the failure branch).

Order is the real execution order. Thread tags: **L** = loader thread inside
composition `module_start`; **I** = serialized init thread; **E** = UE4SS event-loop thread;
**PL** = ploader invocation context.

### Phase A — ploader + ini traversal (PL)

| # | Class | Sink | Expected evidence | Pin | Meaning |
|---|---|---|---|---|---|
| A0 | gate | console | `ploader.ini` sha `1216aab6…` (2-plugin: `supplier_composition=2`, `path_test=2`, all else 0) fetched BEFORE launch; STOP on drift | `RETURN-SESSION-COMMANDS.md:5-8,41-42` | composition identity precondition |
| A1 | gate | — | ini order: composition BEFORE path_test (path_test = last entry, TERMINATOR) | `INIL:74,78,138-142` | PATH_TEST lines prove full traversal AFTER composition `module_start` returned (or timed out, see S-30S) |
| A2 | EXPECTED-SUCCESS | toast | OK toast for composition with `dt` = C_comp; per-invocation OK/FAIL toasts carry dt (`main.c:806-814`) | `LAT:130` | first measured C_comp (VMMAP walk + lease registration now on L) |

### Phase B — composition entry prologue (L) — diagnostic sink

| # | Class | Sink | Expected evidence (verbatim unless `<pat>`) | Pin | Meaning / plausible values |
|---|---|---|---|---|---|
| B1 | EXPECTED-SUCCESS | diag | `module_start_entered` value=0x00000000 — EXACTLY ONE per launch | `EN:305` | the per-launch counter (O1) |
| B2 | EXPECTED-SUCCESS | diag | `entry_incoming_sp_mod16`, `entry_call_sp_mod16`, `constructor_count` `<pat> 0x<…>` | `EN:311-314` | preserved probe prologue; constructor_count > 0 (comp6 ctor set; exact count unpinned) |
| B3 | EXPECTED-SUCCESS | diag | per ctor: `constructor_begin` value=0x<idx> then `constructor_return` value=0x00000000 ×constructor_count | `EN:101-106` | every UE4SS unit ctor exception-caught, result 0 each |
| B4 | REGRESSION | diag | `probe_lifecycle_result` value=0x<nonzero> + `runtime_start_skipped` | `EN:318,322` | a shipped ctor/worker refused — the rebuild is dead BEFORE any launch-10 fix is exercised → verdict FAIL(rebuild-dead), reasons on observer only |
| B5 | EXPECTED-SUCCESS | diag | `runtime_wiring_begin` value=0x00000000 | `EN:234` | wiring attempted exactly once |
| B6 | EXPECTED-SUCCESS | diag | `mfa_activated_low` + `mfa_activated_high` `<pat>` low/high pair | `EN:270-273` | activation preloaded; bytes ≈ sum of 7 loadlist files ≈ 35,890 (0x8c32) + loadlist overhead — PATTERN, do not hard-gate |
| B7 | EXPECTED-REFUSAL | UE4SS.log + status | `[t=<ms>ms pid=<pid>] modbridge module_start failed: mfa_activate_failed code=<mfa> detail=<MFA_NAME> <message> <path>`; status = single-line session `state 1 failed <mfa> …`; diag `mfa_activate_failed` (+ `mfa_activate_os_error`) | `EN4:198-206`, `EN:266-268`, `MFA:626-646` | loadlist/roots drifted; new info names the offending file; game unmodded. Verdict: FAIL (nothing downstream ran) with named cause |

### Phase C — bridge stage 1: capture, VMMAP walk, anchors, lease registration (L)

| # | Class | Sink | Expected evidence | Pin | Meaning / plausible values |
|---|---|---|---|---|---|
| C1 | EXPECTED-SUCCESS | diag | `bridge_vmentry_entries` `<pat>` 0 < v ≤ 16384 (buffer `kBridgeVmmapEntries=16384`); launch-9-era walks measured ~10,019 records in-save | `BG:188,289` | kernel VMMAP snapshot parsed |
| C2 | EXPECTED-SUCCESS | diag | `bridge_vm_walk_stop` = 0 (clean end) or 2 (truncated — honest lower bound, consumer-supported) | `BG:290,296-299` | 1 = malformed record → query returns −6, fail loudly |
| C3 | REGRESSION | diag | `bridge_vm_walk_stop` = 1 and NO later capture events | `BG:296` | insane kve_structsize — the proven walker contract broke |
| C4 | EXPECTED-SUCCESS | diag | `bridge_kve_distinct0..3`, `bridge_kve_eboot` = 0/1 `<pat>` | `BG:351-355` | r9 mount-layout discovery; launch-9 shape: anchored exec record path region ZERO, eboot_hit pattern unpinned |
| C5 | EXPECTED-SUCCESS | diag | `anchor_cov` / `anchor_match` for w=0..5 (order E,D,B,C,C2,A): E,D,B covered=1 AND match=1 (required); C xor C2 covered=1 AND match=1 (either-site); A logged-only (may refuse `anchor_mm`/uncovered without dropping others) | `BG:159-167,381-407` | B&&D&&E&&(C‖C2) is the identity gate at base 0x400000 |
| C6 | REGRESSION | diag | any REQUIRED anchor uncovered (`anchor_cov` bit0=0 for w=0/1/2) or `anchor_c_either`=0 | `BG:405-407` | image identity gate refused → conversion dies; wrong image or moved base |
| C7 | EXPECTED-SUCCESS | diag | `bridge_exepath_source` = 2 (configured `/app0/eboot.bin` accepted as existing regular file; launch-9 shape: kve_path route empty) or 1 (kernel-observed); if 0: r10 fork events `stat_app0[_ec]`, `stat_ownmod[_ec]`, `stat_rootdir[_ec]` then refusal −1 | `BG:429-477` | source 0 → conversion refuses GameExecutablePathMissing (EXPECTED-REFUSAL if seen: /data mount shape changed) |
| C8 | EXPECTED-SUCCESS | diag | `bridge_lease_reg_spans` = **6** (readable spans ∩ extent; launch-9 measured 6, `GEO` fact 6) | `BR14:667` / `BG:532` | cap input for the 128-region registry |
| C9 | EXPECTED-SUCCESS | diag | `bridge_lease_reg` = **6** (registered; `spans == reg`, ≤ 128) | `BR14:668` / `BG:533` | launch-9 predicted spans==6; exec-only text never registered |
| C10 | REGRESSION | diag | `bridge_lease_reg` = 0xffffffff (query failed, r13 shape) or `spans > 128` with reg == 128 (capped) or `bridge_lease_cap`=1 | `BG:498-499`, `BR14:657,667-668` | registry cannot cover the lease — r14 L3 must refuse pre-conversion (see C12) |
| C11 | CONDITIONAL DORMANT | diag | r14-only tags ABSENT on success: `bridge_lease_order`, `bridge_lease_cap`, `bridge_anchor_refuse`(+`bridge_lease_ref_lo/hi/len`, `bridge_lease_near_*`) | `BR14:588-591,630,657,714` | any of these = the L-gates bit — named, fail-closed |
| C12 | CONDITIONAL EXPECTED-REFUSAL | UE4SS.log + status | `runtime_start_failed code=1 detail=lease coverage gap: requested [0x<lo>,0x<lo>+0x<len>)` (+ diag `bridge_lease_gap_req`, `bridge_lease_ref_lo/hi/len`) | `BR14:753-764` | r14's loud pre-lease refusal — new info naming the uncovered span |
| C13 | DORMANT | UE4SS.log + status | `runtime_start_failed code=2 detail=Owned read range includes unregistered bytes` | launch-9 evidence (`RETURN-SESSION-COMMANDS.md:17-19`), `PB:F4/F5` codes | the launch-9 composition killer. DORMANT per launch-10 scope: with r14 shipped it is structurally impossible (frozen-snapshot registration L1-L5); without r14 it stays throw-only-if-crossed. RECURRENCE = REGRESSION, verdict ≤ MINIMAL-ACCEPT + mandatory r14 reconciliation |

### Phase D — conversion + geometry (THE launch-10 fix) (L)

Geometry inputs (coordinator additions, `GEO:51,139,162-164` + `worker-anchor-cap-analysis/REPORT.md`): extent = `[0x400000, 0xc0602c8)`
(0xc0602c8, NOT the 0xfc602c8 comment typo); uniform slide runtime = file + 0x400000 for all
PT_LOADs; 7 kernel spans tile the image (6 readable + 1 exec-only); the six readable records
are pairwise ADJACENT so the adapter coalesces them — expected diagnostic pair is
`lmf_intervals_found=1` with `bridge_lease_reg_spans=6` (raw, uncoalesced).

| # | Class | Sink | Expected evidence | Pin | Meaning / plausible values |
|---|---|---|---|---|---|
| D1 | EXPECTED-SUCCESS | diag | `lmf_extent_base_low`=0x00400000, `lmf_extent_base_high`=0x00000000 | `BG:711-714` | resolved_image_base = 0x400000 |
| D2 | EXPECTED-SUCCESS | diag | `lmf_extent_end_low`=0x0c0602c8, `lmf_extent_end_high`=0x00000000 | `BG:715-718` | extent end = 0x400000+0xbc602c8 = 0xc0602c8 |
| D3 | EXPECTED-SUCCESS | diag | `lmf_anchor_count` = **7** (TRUE pre-filter intersecting count; igf3 shape). Interpretation table (`worker-anchor-cap-analysis/REPORT.md:53-62`): 7 = igf3 shape reproduced; 8 = one extra fragment (LOAD8 materializing?) — with the worktree cap `kMaxAnchorMappings=8` (`live_module_facts.hpp:88`) 8 is the LAST zero-margin warning and 9+ is UNREADABLE (count pins at 8; silent-truncation signature: anchor_count=8 AND intervals missing top-of-image coverage AND/OR downstream GUA/required-slot refusal while lease_reg_spans>6); controller directed the 8→16 fold at build time (margin wide) — CONFIRM the folded cap at the composition gate: with 16 shipped, 9+ reads honestly instead | `BG:719-720`, `GF:462-492`, `CAP REPORT:11-14,53-62` | the geometry fix: containing-base-only rule (1 exec-only span) replaced by intersect-all |
| D4 | EXPECTED-SUCCESS | diag | `lmf_intervals_found` = **1** (adapter COALESCED readable_ranges count). THE PREDICTED PAIR is **intervals=1 next to bridge_lease_reg_spans=6**: on the pinned igf3-shaped boot the six readable records are pairwise ADJACENT (LOAD1 R--; LOAD3 R-- RELRO-demoted page-exact; LOAD7 split into 4 fragments RW/R--/RW/RW ending [0x970c000,0x9a34000)) and the adapter coalesces adjacency. `lmf_intervals_found=6` would indicate PAGE GAPS the pinned records do not show — an investigation trigger, NOT automatic failure. Divergence note (pinned): the geometry-fix FIXTURE SUITE assumed page gaps (intervals==6) — a live intervals=1 is NOT a fix failure (`worker-anchor-cap-analysis/REPORT.md:63-67`). MUST be > 0 — the zero-interval refusal class is the launch-9 geometry death | `BG:721`, `CAP REPORT:63-67` | >0 remains the MINIMAL-ACCEPT gate; the exact expected value is 1, not 6 |
| D5 | EXPECTED-SUCCESS | diag | `bridge_cfg_gamedir`=1, `bridge_cfg_workdir`=1 | `BG:703-704` | configured roots attached — conversion SUCCEEDED (success-path-only events) |
| D6 | REGRESSION (THE launch-9 death, must NEVER recur) | UE4SS.log + status | `[t=<ms>ms pid=<pid>] modbridge module_start failed: runtime_start_failed code=1 detail=no observed mapping interval intersects the module image extent with valid geometry`; diag `lmf_intervals_found`=0 + refusal-path fan-out `lmf_vmentry_cap`, `lmf_vm_scan_cap`, `lmf_linkmap_cap`, `lmf_provider_code`, `lmf_identity_source` | refusal text: `worker-facts-geometry-fix/ANALYSIS.md:56` + host-test `test_geometry_fix_host.cpp:275`; events `BG:664-690` | geometry fix failed on live input → verdict FAIL (task definition) |
| D7 | EXPECTED-REFUSAL | UE4SS.log + status | `runtime_start_failed code=1 detail=<other adapter refusal text>` with the same diag fan-out | `BG:659-692` | any OTHER named conversion refusal (e.g. GameExecutablePathMissing, snapshot NotCaptured) = new info; verdict ≥ MINIMAL-ACCEPT only if D1-D2/D5-family events prove the geometry path itself ran |

### Phase E — context build + runtime start (L)

| # | Class | Sink | Expected evidence | Pin | Meaning |
|---|---|---|---|---|---|
| E1 | EXPECTED-REFUSAL | UE4SS.log + status | `runtime_start_failed code=2 detail=<exception what()>` | `EN:289-290`, `ENH:38`, `BG:764-772` | MakeNativeProgramContext threw — named new info (missing PRX file, refused facts). Verdict ≥ MINIMAL-ACCEPT (conversion passed) |
| E2 | EXPECTED-REFUSAL | UE4SS.log + status | `runtime_start_failed code=3 detail=<exception what()>` or `detail=StartActivatedRuntimeWithAck returned null owner` | `ENH:39`, `BG:744-761` | prepare/start threw (pad profile, mod source, sink open) — named new info; verdict ≥ MINIMAL-ACCEPT |
| E3 | EXPECTED-SUCCESS | diag | `runtime_start_ok` value=0x00000000 | `EN:286` | owner started; init thread spawned |
| E4 | EXPECTED-SUCCESS | status | `modforge-runtime-status\t1` / `session\t<pid>\t<ms>` / `state\t1\tactivated\t0\t` | `MFA:602-618,626-646`, `OWN:171` | session = pid (launch-9 pid 273 precedent — a NEW pid is expected per boot); file replaced (O_TRUNC) |
| E5 | EXPECTED-SUCCESS | diag | `module_start_return` value=0x00000000; entry returned 0 to the kernel | `EN:330-331` | loader thread done; game continues loading UNAFFECTED from here — everything below is thread I |

### Phase F — initializer on the init thread (I) — UE4SS.log + status

Program banner first (thread L during prepare, before `runtime_start_ok`): `Native console,
GUI, crash dumping and filesystem watching are unavailable` → `UE4SS - v<maj>.<min>.<patch>…`
→ `Timezone: UTC…` → `UE4SS Build Configuration: …` → path block with `root directory:
/data/plugins`, `working directory: /data/plugins` → `Setting up mods...` — indirect pins
`PB:§2 #13-14` (program:316-435,1739). Then thread I:

| # | Class | Sink | Expected evidence (verbatim from `UI`) | Pin | Meaning / values |
|---|---|---|---|---|---|
| F1 | EXPECTED-SUCCESS | UE4SS.log | `[Seeds] Required-slot seed table armed (slots 0/2/3/4); the ps_scan arm and both Lua override scans are skipped` | `UI:983` | THE ARMED MARKER — composition carries the 4/4 table |
| F2 | EXPECTED-SUCCESS | UE4SS.log | `[Seeds] Unbound by design: fname_tostring (Kismet fallback owns it), gnatives, fuobject_hash_tables_get; engine version via EngineVersionOverride settings` | `UI:984` | documented degradations, not failures |
| F3 | DORMANT (identity discriminator) | UE4SS.log | `PS Scan attempt N (Phase M)` / `PS scan successful` / `Lua Scan attempt N (Phase M)` / `Running first pass of Lua override scans` / `Running second pass of Lua override scans` | `UI:1017,1028,1095,1241,1341` | must NOT appear on the armed build; if seen, the WRONG (non-armed) composition shipped → REGRESSION |
| F4 | DORMANT (identity discriminator) | UE4SS.log | `[Seeds] Required-slot seed table incomplete: present [ ] missing [ ]. Arming requires all four REQUIRED slots (SHAPE A safe default, LAUNCH9-PREDICTION): …` | `UI:964` | compiled out (`ST:35 kRequiredSetComplete=true`); seeing it = wrong SeedTable shipped → REGRESSION |
| F5 | pattern (settings) | UE4SS.log | one of `Build configuration: debug|test|shipping, forced by settings` / `Build configuration: shipping, detected` / `Build configuration: not detected, assuming shipping. Set DebugBuild in UE4SS-settings.ini to override.` (Warning) AND one of `Stats: on|off, forced by settings` / `Stats: <on|off>, detected independently of build configuration` / `Stats not detected; native layout publication requires an explicit Stats setting` (Warning) | `UI:1140-1212` | v3-settings dependent; a Warning here is honest, not fatal |
| F6 | EXPECTED-SUCCESS | UE4SS.log | `[Seeds] slot 0 g_uobject_array bound at 0x97ba110 (data; identity and ABI unverified)` | `UI:766-767`; address = 0x400000+`ST:31` label 0x93ba110 | slot 0 bound after the read-path guard class check passed DataReadable |
| F7 | EXPECTED-REFUSAL (read-path guard, new info) | status failed detail | `[Seeds] REFUTED: slot 0 g_uobject_array 0x<addr>: seed class <class> (expected data-readable); refusing reads. Terminal for this launch; derive afresh with new evidence, never replay.` where `<class>` ∈ `data-readable` / `exec-only (call-only)` / `unmapped/prot-none gap (refused)` | `UI:757-759`; class names `SV:188-196` | seed sat on the wrong span class — read-path guard did its job; derive afresh |
| F8 | EXPECTED-REFUSAL | status failed detail | `[Seeds] REFUTED: slot 0 g_uobject_array: seeded address is null.` or `…seeded address is not 8-aligned.` | `UI:761-763`; `SV:198-208` | admission probe failed |
| F9 | EXPECTED-SUCCESS | UE4SS.log | `[Seeds] slot 3 gmalloc bound at 0x970add8 -> 0x97089b8 (data; V1 instance + V2 vptr match; identity and ABI unverified)` | `UI:817-818`; addresses = 0x400000+`ST:33` label 0x930add8 and 0x400000+`ST:21` instance 0x93089b8 | GMalloc V1 AND V2 PASSED — pointer-to-known-BSS identity shape + RELRO vptr equality |
| F10 | EXPECTED-REFUSAL (GMalloc validator, new info) | status failed detail | `[Seeds] REFUTED: slot 3 gmalloc V1 at 0x970add8: [0x<malloc>] != derived instance 0x97089b8: [GMalloc] does not equal the derived FMallocBinned3 instance address. …` | `UI:804-807`; V1 string `SV:225-232` | [GMalloc] holds a live-but-foreign heap pointer — rank-1 arming rule correctly refuses |
| F11 | EXPECTED-REFUSAL (GMalloc validator, new info) | status failed detail | `[Seeds] REFUTED: slot 3 gmalloc V2 at 0x970add8: instance 0x<malloc> vptr 0x<vptr> != derived vtable 0x82c26e0: FMallocBinned3 instance vptr does not equal the derived vtable address. …` | `UI:809-814`; V2 string `SV:237-244`; runtime vtable = 0x400000+0x7ec26e0 | vptr mismatch class — RELRO constant compared, never dereferenced |
| F12 | EXPECTED-REFUSAL (read-path guard) | status failed detail | `[Seeds] REFUTED: slot 3 gmalloc 0x970add8: seed class <class> (expected data-readable); refusing reads. …` | `UI:777-779` | slot 3 guard variant |
| F13 | EXPECTED-REFUSAL | status failed detail | `[Seeds] REFUTED: slot 3 gmalloc: [0x970add8] is not a live aligned FMalloc*: <probe.failure>. …` | `UI:787-791` | weak live-pointer probe (pre-V1) |
| F14 | EXPECTED-SUCCESS (retries SILENT within budget) | UE4SS.log | `[Seeds] FName pool validated at 0x9765ec0 (CurrentBlock <n>, Blocks[0] 0x<hex>; 'None' id 0x<id>, 'ByteProperty' id 0x<id>)` | `UI:868-869`; pool = 0x400000+`ST:12` label 0x9365ec0 | `<n>` = CurrentBlock ≥ 1; Blocks[0] inside [0x8018c000,0x100118000); both ids nonzero; ByteProperty id must equal the fc1pool `byteprop_id_for_find_crosscheck` record (D3 VALIDATED) |
| F15 | EXPECTED-REFUSAL (**vintage-transient heap**, coordinator addition; VALUE-CARRYING v2 format, initializer launch10-b) | status failed detail | terminal shape: `[Seeds] REFUTED: FName pool at 0x9765ec0: <shape.failure> (observed CurrentBlock <n>, Blocks[0] 0x<hex> vs span [0x8018c000,0x100118000)). Terminal for this launch; derive afresh with new evidence, never replay.` — exact failure strings: `CurrentBlock < 1 (pool not initialized or wrong pool/base)` (`SV:70`), `Blocks[0] is null` (`SV:73`), `heap span is empty` (`SV:78`), `Blocks[0] outside the readable heap span` (`SV:82`); the v2 parenthetical is the format at `UI:846-847` `…{} (observed CurrentBlock {}, Blocks[0] 0x{:X} vs span [0x{:X},0x{:X}))` with args pool_runtime, shape.failure, words.current_block, words.blocks0, kHeapLo, kHeapHi | `UI:843-848`; `SV:63-85` | kHeapLo/kHeapHi = [0x8018c000, 0x100118000) (`ST:13-14`) is VINTAGE-TRANSIENT non-image memory: real at igf/igf2, ABSENT from the igf3 7-span walk (`GEO:120,137`). Interpretation: the LIVE heap layout moved — the refusal itself is the diagnostic. VALUE-CARRYING (supersedes the earlier value-blind format): the observed CurrentBlock and Blocks[0] ARE printed on refusal; if it fires at launch-10 the observed Blocks[0] directly feeds the launch-11 runtime-bounds design (worker-fname-heap-runtime-bounds). Verdict cap: GOOD |
| F16 | EXPECTED-REFUSAL | status failed detail | `[Seeds] REFUTED: FName pool entry id 0x<id>: entry block index exceeds FNameMaxBlocks.` / `…entry block index at or past CurrentBlock.` / `…entry extends past CurrentByteOffset of its block.` / `…pool entry id 0x<id>: Blocks[<n>] is null.` | `UI:854,719`; `SV:98,102,108` | pool bounds chain |
| F17 | EXPECTED-REFUSAL | status failed detail | `[Seeds] REFUTED: FName pool entry id 0x<id> (expected 'None'): entry is wide; ASCII expected.` / `…entry length does not match expected string length.` / `…entry payload exceeds the readable bytes.` / `…entry characters differ from the expected string.` (same for expected 'ByteProperty') | `UI:859`; `SV:139,143,147,153` | interned-content mismatch — terminal, never retried once shape holds |
| F18 | EXPECTED-SUCCESS | UE4SS.log | `[Seeds] slot 2 FName ctor bound at 0x63d590 and behaviorally verified (ByteProperty probe id == pool id 0x<id>); detour verification skipped` | `UI:893-894`; ctor = 0x400000+`ST:32` label 0x23d590 | THE task-pinned line — behavioral ByteProperty cross-check passed; detour verifier skipped by seed-bound flag |
| F19 | EXPECTED-REFUSAL (read-path guard, code class) | status failed detail | `[Seeds] REFUSAL SHAPES: slot 2 FName ctor 0x63d590: seed class <class> (expected exec-only call-only); refusing bind. …` / `slot 4 static_construct_object 0x890aa0: seed class <class> (expected exec-only call-only); refusing bind. …` | `UI:879-881,905-907` | the two task-named guard refusals — a data-classified code seed or a refused gap |
| F20 | EXPECTED-REFUSAL | status failed detail | `[Seeds] REFUTED: slot 2 FName ctor behavioral probe at 0x63d590 (probe id 0x<pid> vs pool id 0x<bid>): behavioral probe returned NAME_None (ctor call did not take effect).` or `…: behavioral probe id diverges from the pool-read ByteProperty id.` | `UI:886-890`; `SV:249-260` | ctor call did not take effect / diverged — ctor label refuted |
| F21 | EXPECTED-SUCCESS | UE4SS.log | `[Seeds] slot 4 static_construct_object_internal bound at 0x890aa0 (code; identity and ABI unverified)` | `UI:910-911`; SCO = 0x400000+`ST:34` label 0x490aa0 | second task-pinned line — SCO consumed call-only |
| F22 | EXPECTED-REFUSAL (per-launch base) | status failed detail | `[Seeds] REFUTED: module MainExe view base 0x<derived> != seed table base_observed 0x400000 (image moved). …` | `UI:745-749`; base `ST:11` | Phase-1 base check — ASLR/second-load bias moved |
| F23 | DORMANT | UE4SS.log | `Phase 1: Skipping FName verification` / `Phase 2: Verifying FName constructor...` / `FName constructor verified at 0x…` / `FName verification callback retired; forwarding hook retained for host lifetime` | `UI:1362,1372,1410,207` | detour verifier skipped when seed-bound (`UI:1480-1484`); any of these = wrong build |
| F24 | EXPECTED-SUCCESS | UE4SS.log | `Using engine version: <maj>.<min>` (profile townfall-ps5-ue56 → plausible `5.6`; PATTERN) | `UI:1492` | consumed via EngineVersionOverride settings |
| F25 | EXPECTED-SUCCESS | UE4SS.log | `Waiting for object construction...` then (per 400 attempts, Warning) `Still waiting for object construction, <n> objects constructed so far` → proceeds at ≥ 10000 | `UI:1499-1514` | object-array population wait |
| F26 | EXPECTED-SUCCESS | UE4SS.log | `Locating KismetStringLibrary...` → `KismetStringLibrary located: 0x<hex> [n: <hex>]` → `Locating KismetStringLibrary:Conv_NameToString...` → `KismetStringLibrary:Conv_NameToString located: 0x<hex> [n: <hex>]` → `Locating KismetStringLibrary CDO...` → `KismetStringLibrary CDO located: 0x<hex> [n: <hex>]` (+ per-40-attempt Warning patterns `Still searching for KismetStringLibrary…` / `Still waiting for the KismetStringLibrary CDO…`) | `UI:1518-1567` | reflected-query prerequisites; Warnings = slow construction, not refusals |
| F27 | EXPECTED-SUCCESS | UE4SS.log | `Constructed [<k> / <N>]: <ObjectName>` (one per required object found) + Warning patterns `Still waiting for required object '<name>' after <s> seconds` / `Timed out waiting for required object '<name>' to construct; continuing without it` | `UI:1648,1661,1679` | required-object ladder; timeout Warnings are bounded-continue, not fatal |
| F28 | EXPECTED-REFUSAL (named terminal) | UE4SS.log + status | `Finding '<ClassName>'` (+ optional `Could not find '<ClassName>', falling back to '<CDO>'`), then `Object CDO: 0x<hex> [n: <hex>]`, `Struct CDO: …`, `GameEngine CDO/Instance: …`, `GameMode CDO/Instance: …`, `Actor CDO: …`, `GameViewportClient CDO/Instance: …` (+ Warning `Post-initialization: Was unable to find 'Engine.Default__GameViewportClient'…` with `bHookGameViewportClientTick=false` fallback) | `UI:1686-1744` | core-object inventory; throws here are named (`UI:1700,1707,1714,1726,1734`) and surface per F30 |
| F29 | EXPECTED-SUCCESS (first vtable walks — the launch-9 crash site) | UE4SS.log | `Finding GameEngine::LoadMap` → `GameEngine::LoadMap address 0x<hex>` → `Finding GameEngine::Tick` → `GameEngine::Tick address (vtable: 0x33ac8c0; scan: <null-form>)` → tick resolve line: `Scan failed, falling back to vtable address for GameEngine::Tick` OR `Using vtable address for GameEngine::Tick` (settings-dependent) → `Finding GameModeBase::InitGameState` → `GameModeBase::InitGameState address 0x<hex>` → `Finding AActor::BeginPlay/EndPlay/Tick` + address lines → `Finding GameViewportClient::Tick` → `Finding ProcessEvent` → `ProcessEvent address 0x<hex>` → `Finding ProcessConsoleExec` → `Finding UStruct::Link` + address lines | `UI:1748-1752,1757-1761,1795-1839,1865-1934` | CP2 note: controller pin says `scan: 0x33ac8c0` (`RETURN-SESSION-COMMANDS.md:56`) — on the ARMED build the scan arm is structurally dead and `scan_address` is NULL (`UI:1798-1800` comment); expected `scan:` renders null (exact rendering of the null pointer unpinned: `0x0`/`0`). If evidence shows `scan: 0x33ac8c0` the composition ran a tick scan arm → composition-identity flag, not automatically a failure. A vtable/scan mismatch would add the WARNING block `WARNING: VTable and scan addresses differ…` (`UI:1765-1791`) — unreachable with null scan |
| F30 | EXPECTED-REFUSAL (named terminal, status-visible) | status | `state\t1\tactivated\t0\t` then `state\t2\tfailed\t1\t<what()>` where `<what>` is any named throw from the armed path (a `[Seeds] REFUTED: …` text, `Native game-thread initializer callback registration failed` `UI:1846`, `Query harness tick dispatch callback registration failed` `UI:1857`, a PostInitialize `throw`, `TypeChecker: Was unable to find some or all of the required core objects` `UI:1954`, `Fatal error! The following objects were never constructed:` `UI:1942` + empty-string throw `UI:1948`) | owner ack `OWN:176`; playbook F7 | the init thread threw BEFORE engine-ready; `what()` lands verbatim (sanitized, 512-B line cap) in status detail; UE4SS.log ends mid-module-lines. This is THE row where every seed_refused variant actually becomes durable evidence |
| F31 | EXPECTED-SUCCESS | UE4SS.log | `Initializing global FName variables` → `Constructed <k> of <N> objects` → `Initializing type system` → (Verbose) `UnrealConfig.FExecVTableOffsetInLocalPlayer: <hex>` → `Post-initialization: GMalloc: 0x970add8 -> 0x97089b8` | `UI:1936-2046,401` | GMalloc line doubles as the V1/V2 echo: GMalloc ptr = slot-3 address, *GMalloc = derived instance |
| F32 | EXPECTED-SUCCESS | UE4SS.log | `Initializing searcher pools` → `Adding GUObjectArray listeners` → `Adding shutdown listener` → `Starting callback garbage collector!` → `Hooking StaticConstructObject for searcher pools` → size lines `FAssetData Size: 0x…`, `FVector Size: 0x…`, `FQuat Size: 0x…`, `FFieldVariant Size: 0x…`, `FText Size: 0x…`, `FName Size: 0x…`, `FName Alignment: 0x…` (+ Warning `[UE4SS.StaticConstructObject.ObjectSearcherPoolHook] StaticConstructObject is set to return nullptr, not adding to ObjectSearcherPool!` possible) | `UI:416-518,491` | PostInitialize tail — engine setup COMPLETE |
| F33 | DORMANT | UE4SS.log | `Unhooking FName constructor` / `Unable to retrieve or synchronize ID of game thread` | `UI:210,175` | non-Prospero branch / publication failure shapes |
| F34 | EXPECTED-SUCCESS | status | `state\t2\tengine-ready\t0\t` | `OWN:172`; site `PB:§2 #21` (program:654-661) | setup_unreal done. NOTE: proves init-thread completion only — NOT game-thread evidence (playbook honesty note) |
| F35 | EXPECTED-SUCCESS | status | `state\t3\tmods-started\t0\t` immediately after engine-ready | `OWN:173`; `PB:§2 #22` | ack points 2+3 complete; written BEFORE main.lua runs |

### Phase G — deferred Lua startup (E thread) + mod load attempt

| # | Class | Sink | Expected evidence | Pin | Meaning |
|---|---|---|---|---|---|
| G1 | EXPECTED-SUCCESS | UE4SS.log | `start_mods<LuaMod>` per startup mod; Lua VM prepared (indirect pins `PB:§2 #25`, program:1946-1956, luamod:6440) | `PB` | deferred startup reached |
| G2 | EXPECTED-SUCCESS (BEST marker) | UE4SS.log | `[Lua] [TownfallItemSpawner] <msg>` — `print` → `[Lua] ` prefix, `log()` → `[TownfallItemSpawner] ` prefix (main.lua:5-6) | `PB:§2 #26`; `worker-itemspawner-bundle/.../main.lua:5-6` | THE mod loaded and its own prints ran |
| G3 | EXPECTED-SUCCESS-ADJACENT (still BEST) | UE4SS.log | mod-level error lines: `Error loading script: <msg>` / `Error executing script: <msg>` / `Exception in load_and_execute_script: …` / `Failed to execute main script: <path>` / `Main script 'main.lua' not found in scripts directory: …` / `Exception during mod startup: …` | `PB:F9` (luamod:6413-6478) | a broken mod script does NOT remove mods-started; status still ends mods-started; session alive, only the mod is dead |
| G4 | CONDITIONAL DORMANT (launch-10 armed goal stops here) | UE4SS.log | Item Spawner activation evidence (OPTIONS→F7 menu, spawn) | `PB:§5 rungs 4-5`; `RETURN-SESSION-COMMANDS.md:59` | user tests OPTIONS only AFTER mods-started evidence exists; visible spawn is NOT a launch-10 verdict input — do not claim |

### Phase H — path_test terminator (L of path_test PRX)

| # | Class | Sink | Expected evidence | Pin | Meaning |
|---|---|---|---|---|---|
| H1 | EXPECTED-SUCCESS | UE4SS.log | `PATH_TEST: /data/plugins/UE4SS.log writable at <monotonic-ns> monotonic-ns` (19-digit ns ≈ boot-epoch 1.79e18; `… CLOCK_FAILED` variant if clock failed) | `PT:171-176`; `PT generated/pt_config.h` (PT_LOG_PATH) | freshness stamp; traversal reached the last ini entry |
| H2 | EXPECTED-SUCCESS | UE4SS.log | `ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: 6d6f64666f7267652d6c6f61646c6973` (= "modforge-loadlis"; `<pat>` short-read/errno variants) | `PT:125-146`; PT_LOADLIST_PATH | activation path readable; expected hex from `worker-loadlist-final/final-package/loadlist.txt` head |
| H3 | EXPECTED-REFUSAL | UE4SS.log | `ACTIVATION_TEST: … OPEN_FAILED errno=<n>` or `OPEN_OK_READ_FAILED errno=<n>`; PATH_TEST fallback `PATH_TEST: /data/plugins/UE4SS.log OPEN_FAILED errno=<n> at <ns> monotonic-ns` (+ same-file fallback `modbridge_path_test.fallback.log`) | `PT:129-154,178-190` | new info: mount shape changed; game unmodded |

### Phase S — stop/unload (any time after E3)

| # | Class | Sink | Expected evidence | Pin | Meaning |
|---|---|---|---|---|---|
| S1 | EXPECTED-SUCCESS | diag + status | `runtime_stop_begin` → `runtime_stop_result` value=0x00000000; status last line `state\t<k>\tstopped\t0\t` (single) | `EN:341-348`; `OWN:187` | clean unload; join succeeded |
| S2 | EXPECTED-REFUSAL | UE4SS.log + status | `…runtime_stop_failed code=<n> detail=owner stop reported failure`; stop rc = 1 (not stopped) or 5 (`MRS_ERR_STOP_EXN`, exception escaped stop) | `ENH:45`, `EN:345-347`, `BG:536-556` | bounded, named; reconcile without replay |

### Special row S-30S — the 30-second ploader cap

| # | Class | Evidence | Pin | Meaning |
|---|---|---|---|---|
| S-30S | SPECIAL (own row, per task) | ploader `pt_call` waits `module_start` capped at **30 s** → FAIL toast + detach; the module's work keeps running (launch-9 measured behavior) | `LAT:63,130` | The cap bounds ONLY Phase B–E on thread L (entry prologue + mfa_activate + capture + conversion + context + prepare + start-spawn). EVERYTHING in Phase F/G (seed consumption, engine-ready, mods) happens on init thread I AFTER `module_start` returned — no 30 s pressure. Special outcome: user sees a composition FAIL toast at dt≈30 s WHILE the status/UE4SS.log sinks later show `runtime_start_ok` + the full `[Seeds]` set. DECLARATION LAW: judge by the durable sinks, never the toast; if sinks show completion, apply the verdict ladder normally and record "C_comp > 30 s" as a launch-11 latency item (worker-injection-latency R3), not a functional failure |

### NO-CRASH invariant (independent of every verdict)

- The game process must SURVIVE launch-10 in every acceptable outcome. Every designed failure
  is loud + fail-closed: wiring failures return 0 from `module_start` (game runs UNMODDED,
  `EN:35-38`); seed refusals are throws caught on the init thread (F30); the entry never kills
  the game.
- CRASH = verdict FAIL regardless of prior progress. Crash-shaped signature (launch-9 T-RD
  class): game dies; `runtime_start_ok` present or status frozen at `activated`; UE4SS.log
  ends mid-module-lines (e.g. right after `Finding GameEngine::LoadMap`) with NO
  `failed` state line and NO entry failure line; observer/jsonl stops mid-stream.
- A silent no-op (no new `module_start_entered`, no entry failure line, no status change —
  F0) is ALSO fail: the PRX never ran, and no launch-10 claim is possible.

---

## 2. Launch-10 ACCEPTANCE VERDICTS (controller declarations, honestly bounded)

Evidence is read in this order: runtime-status.txt (session truth) → supplier-diag.jsonl /
observer events (stage attribution) → UE4SS.log (initializer detail). All sinks must agree
on the session (fresh `session\t<pid>\t<ms>`; no FAIL-line/progress conflicts —
`post_launch_check.py:470-498`).

### BEST — seeds bound, engine-ready, mod load attempted

ALL of:
1. Conversion+context clean: D1+D2+D3(anchor_count>0)+D4(intervals>0)+D5(cfg events)+E3
   (`runtime_start_ok`)+E4(`activated`); no D6/D7/E1/E2/C13.
2. ALL FIVE armed-path seed lines in UE4SS.log: F1 (armed), F6 (slot 0 bound),
   F14 (pool validated), F9 (slot 3 `V1 instance + V2 vptr match`), F18 (slot 2 behaviorally
   verified), F21 (slot 4 bound) — note: six lines including the armed marker; the five
   `[Seeds] slot/pool` bind lines are the requirement.
3. `state 2 engine-ready 0` (F34) AND `state 3 mods-started 0` (F35).
4. Mod load ATTEMPTED: G1 (`start_mods<LuaMod>`) and (G2 prints OR G3 mod-error lines —
   a mod that errors AFTER load still satisfies this clause).
5. NO-CRASH invariant holds (game alive at menu).
Note: F30-class failures after `mods-started` do not demote BEST (init-throw can only precede
engine-ready by construction; post-ready failures surface as stopped/failed acks — record,
do not demote).

### GOOD — conversion+context pass; seeds consume past V1/V2; loud NAMED refusal later

ALL of:
1. Clause BEST-1 (conversion+context clean, `activated` present).
2. F1 + F6 + F14 + F9 present — the seed consumption got PAST the GMalloc V1/V2 gate (slot 3
   bound) — optionally + F18/F21.
3. A NAMED later-stage refusal, durable in status failed detail per F30 (any `[Seeds]
   REFUTED:` at pool/ctor/SCO phase — F15/F16/F17/F19/F20 classes — or a named throw:
   callback-registration failure, PostInitialize throw, TypeChecker/required-object fatal),
   OR C12 (`lease coverage gap…`) with r14 shipped.
4. NO engine-ready (F34 absent). NO-CRASH holds.
F15 (vintage-transient heap-shape refusal) caps the verdict at GOOD and carries the
interpretation note + the value-blind diagnostic gap (REPORT.md R4).

### MINIMAL-ACCEPT — geometry fixed; conversion passes; a NEW named refusal may appear after

ALL of:
1. D1 + D2 present (extent events emitted), D3 anchor_count > 0, D4 intervals_found > 0,
   D5 cfg events present, E4 `activated` present.
2. The geometry refusal D6 did NOT occur.
3. NO-CRASH holds.
Everything after conversion may be ANY named failure (E1/E2/F7-F22/F30/C13-recurrence) and
the verdict still holds — but: a C13 recurrence (`Owned read range includes unregistered
bytes`) or any C10/C11 registry anomaly additionally raises a REGRESSION flag requiring r14
reconciliation before launch-11 (the geometry claim itself stays satisfied).

### FAIL — geometry refusal recurs or crash

ANY of:
1. D6: `… code=1 detail=no observed mapping interval intersects the module image extent with
   valid geometry` appears (the geometry fix failed on live input).
2. CRASH in the NO-CRASH invariant sense (including the igf6-class death at the first
   vtable walk, F29 site).
3. B4 (probe lifecycle failed — the rebuild never reached wiring; no launch-10 claim
   possible) or B7-adjacent silent no-op (F0: composition never entered).
4. Sinks conflict irreconcilably (fresh session absent + no new entry lines + no events =
   nothing ran).

Verdict-claim guard: rung advancement requires the observations above from the real console
launch. Host tests, drafted logs, stale baseline lines, and this oracle never advance a
verdict. Never claim >"BEST" and never upgrade a verdict by re-reading unchanged evidence.

---

## 3. Quick reference — every seed_refused variant (terminal wrapper `UI:657-663`:
`[Seeds] REFUTED: <why>. Terminal for this launch; derive afresh with new evidence, never
replay.`; all reach durable evidence ONLY through the F30 status-failed-detail row; UE4SS.log
does NOT carry them)

| variant (the `<why>` payload, verbatim template) | pin | class |
|---|---|---|
| `module MainExe view base 0x{derived:X} != seed table base_observed 0x400000 (image moved)` | UI:749 | EXPECTED-REFUSAL |
| `slot 0 g_uobject_array 0x{addr:X}: seed class {class} (expected data-readable); refusing reads` | UI:759 | EXPECTED-REFUSAL (read-path guard) |
| `slot 0 g_uobject_array: {probe.failure}` (null / not 8-aligned) | UI:763 | EXPECTED-REFUSAL |
| `slot 3 gmalloc 0x{addr:X}: seed class {class} (expected data-readable); refusing reads` | UI:779 | EXPECTED-REFUSAL (read-path guard) |
| `slot 3 gmalloc: {probe.failure}` | UI:783 | EXPECTED-REFUSAL |
| `slot 3 gmalloc: [0x{addr:X}] is not a live aligned FMalloc*: {probe.failure}` | UI:791 | EXPECTED-REFUSAL |
| `slot 3 gmalloc V1 at 0x{addr:X}: [0x{loaded:X}] != derived instance 0x97089b8: [GMalloc] does not equal the derived FMallocBinned3 instance address` | UI:806-807 + SV:229 | EXPECTED-REFUSAL (GMalloc V1) |
| `slot 3 gmalloc V2 at 0x{addr:X}: instance 0x{m:X} vptr 0x{v:X} != derived vtable 0x82c26e0: FMallocBinned3 instance vptr does not equal the derived vtable address` | UI:813-814 + SV:241 | EXPECTED-REFUSAL (GMalloc V2) |
| `slot 1 fname pool 0x{addr:X}: seed class {class} (expected data-readable); refusing reads` | UI:830 | EXPECTED-REFUSAL (read-path guard) |
| `FName pool at 0x{addr:X}: interned ids still zero after the wait budget (pool never initialized)` | UI:845 | EXPECTED-REFUSAL (timing: pool never initialized within SecondsToScanBeforeGivingUp) |
| `FName pool at 0x{addr:X}: {shape.failure}` — `CurrentBlock < 1 (pool not initialized or wrong pool/base)` / `Blocks[0] is null` / `heap span is empty` / **`Blocks[0] outside the readable heap span`** (vintage-transient heap window — see F15). v2 VALUE-CARRYING wrapper (`UI:846-847`, initializer launch10-b): `…{shape.failure} (observed CurrentBlock {n}, Blocks[0] 0x{hex} vs span [0x8018c000,0x100118000))` — the observed values feed worker-fname-heap-runtime-bounds | UI:846-847 + SV:70,73,78,82 | EXPECTED-REFUSAL (heap-shape class is new-info, value-carrying; verdict cap GOOD) |
| `pool entry id 0x{id:X}: Blocks[{n}] is null` | UI:719 | EXPECTED-REFUSAL |
| `FName pool entry id 0x{id:X}: {bounds.failure}` (exceeds FNameMaxBlocks / at or past CurrentBlock / extends past CurrentByteOffset) | UI:854 + SV:98,102,108 | EXPECTED-REFUSAL |
| `FName pool entry id 0x{id:X} (expected 'None'|'ByteProperty'): {match.failure}` (wide / length / payload / characters) | UI:859 + SV:139-153 | EXPECTED-REFUSAL |
| `slot 2 FName ctor 0x{addr:X}: seed class {class} (expected exec-only call-only); refusing bind` | UI:881 | EXPECTED-REFUSAL (read-path guard, task-named) |
| `slot 2 FName ctor behavioral probe at 0x{addr:X} (probe id 0x{p:X} vs pool id 0x{b:X}): {probe.failure}` (NAME_None / diverges) | UI:889-890 + SV:253,257 | EXPECTED-REFUSAL |
| `slot 4 static_construct_object 0x{addr:X}: seed class {class} (expected exec-only call-only); refusing bind` | UI:907 | EXPECTED-REFUSAL (read-path guard, task-named) |
| `slot '{name}' is not present in the seed table (arming is all-or-nothing)` | UI:734 | DORMANT (table is 4/4; seeing it = wrong table shipped → REGRESSION) |
| `[Seeds] Required-slot seed table incomplete: present [{present}] missing [{missing}]. …` (the Shape-A throw, not a REFUTED wrapper) | UI:964 | DORMANT (compiled out; seeing it = REGRESSION) |

Expected success bind lines (for contrast): UI:766 (slot 0), UI:817 (slot 3), UI:868 (pool),
UI:893 (slot 2), UI:910 (slot 4) — rows F6/F9/F14/F18/F21.

## 4. Timing summary

- ploader waits `module_start` (composition), cap 30 s (`pt_call`) → FAIL toast + detach,
  module keeps running (`LAT:63`); outer invocation loop idles to the 120 s cap after FAILs
  (`LAT:67`). path_test is trivial (2 log lines) and runs after composition in ini order.
- The composition should COMPLETE or REFUSE within 30 s of `module_start` entry; the
  S-30S special row governs the over-cap case. Seed consumption, pool retry budget
  (`SecondsToScanBeforeGivingUp`, UI:1008), object/Kismet/required-object waits, engine-ready
  and mods-started are all POST-detach (init thread) and are bounded by their own Warning-
  instrumented loops, not by ploader.

---

<!-- MERGED 2026-10-05 by controller: ORACLE-SUPPLEMENT (final-build sync) into the oracle.
     Extent figure confirmed correct in the supplement: runtime extent [0x400000,0xc0602c8).
     v4 IS folded (initializer 4a2b9400, SeedValidation f09ec9b3, bridge rename publish_readable_spans). -->

# SUPPLEMENT (final-build sync — merged verbatim below)

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
