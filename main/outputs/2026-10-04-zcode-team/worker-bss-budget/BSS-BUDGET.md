# BSS-BUDGET — launch-12 capture capacity 16,384 → 32,768 vs PRX BSS headroom

Worker: worker-bss-budget · 2026-10-05 · local-only ELF parsing + pinned-SDK/doc
audit; no console contact, no engine writes. All byte counts are exact (measured
with a Python ELF64 struct parser; no lief needed).

## Verdict: **AFFORDABLE** (with one honest unknown, stated in §4)

The 32,768-record bump adds **+20,553,728 B (+19.60 MiB)** BSS to a shipped PRX
that already measures **21,558,752 B (20.56 MiB)** of BSS and boots on target.
Result: **42,112,480 B (40.16 MiB)** BSS in a 16 KiB-aligned **40,188,672 B**
writable segment, **71,505,414 B-class (71.51 MiB)** total PT_LOAD memsz image —
815× under the only documented machine cap and 1.378× the total-image memsz of a
runtime class with six clean boots behind it. No identified constraint binds.

---

## 1. The shipped PRX, measured (task 1)

Artifact:
`outputs/2026-10-04-zcode-controller/launch10-build/prx-candidate-l10-2/bare-sdk/modbridge_supplier_composition.prx`
sha256 `ff1055b2…13cbb`, 30,834,648 B. ELF64 LSB, x86-64, `e_type 0xFE08`
(prospero PRX), 5 × PT_LOAD, 27 sections.

| PT_LOAD | flags | filesz | memsz | note |
|---|---|---|---|---|
| 0 | R–E | 18,788,391 | 18,788,391 | .text 17.9 MiB |
| 1 | R–  | 11,266,436 | 11,266,436 | rodata/eh_frame 10.7 MiB |
| 2 | RW  |    218,784 |    218,784 | .got/.data.rel.ro |
| 4 | RW  |     23,456 | 21,582,208 | **.data 0x5BA0 + .bss 0x148F5E0** |
| 9 | R–  |    501,104 |    501,104 | dynamic-link info |

- **.bss (SHT_NOBITS) = 0x148F5E0 = 21,558,752 B = 20.56 MiB** — carried
  entirely by PT_LOAD[4] (memsz − filesz = .bss exactly).
- PT_LOAD sums: filesz 32,855,914 B (31.33 MiB), **memsz 54,425,186 B
  (51.90 MiB)**; image vaddr span 0x31EF6F0 (49.94 MiB); p_align 0x4000.
- l10-1's bare-sdk twin is byte-layout-identical in BSS (same
  .bss/PT_LOAD sizes; only SDK-version bytes differ per its `.sdk.json`).

### Composed object contribution

`prx-candidate-l10-2/build/combined-supplier.o`: NOBITS total
**21,553,867 B**; the other linked objects add 92 B; the PRX's 4,793 B excess
is link alignment. (`runtime_start_bridge.o` at 19,796,470 B NOBITS is the
same translation unit's standalone compile — not in the l10-2 link set.)

Named capacity arrays inside the composed object:

| symbol | entries × sizeof | bytes |
|---|---|---|
| `(anon)::g_bridge_vmmap` | 16,384 × 1,160 (`kinfo_vmentry`, `kve_sizeof_` pinned 1160) | **19,005,440** (18.12 MiB) |
| `g_spanpub_records` | 16,384 × 24 (`VmSpanRecord` = u64,u64,u32+pad) | 393,216 (384 KiB) |
| `window_anchor_verify()::mappings` | 16,384 × 24 (`lmf::OsVmMapping`) | 393,216 (384 KiB) |
| `lmf::RetainedLiveModuleFactsSupplier::capture()::mappings` | **1,024** × 24 | **24,576 (24 KiB)** |
| arrays sum | | 19,816,448 (18.90 MiB) |
| other BSS (NAME_None, lease/image tables, Unreal class-registrar statics, …) | | 1,742,304 (1.66 MiB) |
| **total .bss** | | **21,558,752 (20.56 MiB)** |

**Finding:** `kMaxVmMappings` is still **1024** in the shipped artifact — the
EDITION.json edit-4 staged header override (1024→16384) did **not** reach this
composition. That is precisely the verification step CAPTURE-CAPACITY.md §5.1
requires before launch-12; the launch-12 fold is 1024→32,768, not 16384→32,768.

## 2. Loading limits (task 2)

Documented, pinned SDK
(`outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/`):

- `target/include/machine/vmparam.h:55-70`:
  **MAXTSIZ = DFLDSIZ = MAXDSIZ = 32 GiB** (34,359,738,368 B), DFLSSIZ 8 MiB,
  **MAXSSIZ 512 MiB** — the FreeBSD-derived per-process text/data/stack caps.
  Nothing PRX-specific; no per-module BSS, per-segment memsz, or
  total-module-memory cap exists anywhere in the pinned headers, ldscript (no
  MEMORY/LENGTH caps), or docs. `SceKernelModuleSegmentInfo.size` is `uint32_t`
  (a 4 GiB *reporting* field, not a cap).
- `native-module-metadata/upstream/libkernel.h`: `sceKernelLoadStartModule`
  loads the PRX; no size parameter, no documented caps.

**If undocumented, bounded empirically:**

- The frozen_prx baseline (`native-full-runtime-aligned-entry-1/build-1`,
  fSELF 30,151,623 B, sha `3b998f09…`, "loads in Townfall every time") parses
  as PT_LOAD memsz sum **32,197,730 B (30.71 MiB)** but **.bss = 71,960 B
  (70.3 KiB)**. It proves total-image loading is not the binding constraint —
  and *nothing about BSS*: the "~30 MB PRX loads fine" fact is about the wrong
  axis. The BSS axis is proven by the current class instead.
- The current 20.56 MiB-BSS runtime class has **six real console boots**
  (worker-verdict-first-real runs 1/2/3, pids 317/341/355; launch10-prep run5
  baselines, session 370, three boots): `module_start_entered` +
  `module_start_return=0` and 235/235 ctor pairs every time; the bridge walked
  `bridge_vmentry_entries` = 22761/22761/10028/10028/9993/9993 with
  `bridge_vm_walk_stop=0` — i.e. the sysctl filled and the walker touched the
  full 19,005,440 B `g_bridge_vmmap` on every boot (≥19.5 MiB of BSS physically
  resident and exercised). Every observed refusal was config-class (PS5Pad
  ASCII, loadlist size, pad phase=6), never memory-class.
- What is therefore binding: nothing identified at this operating point. The
  candidate constraint classes are (a) per-segment memsz caps — none documented,
  815× margin to MAXDSIZ; (b) physical residency during the walk — already
  proven at ~19.5 MiB resident, worst case doubles to ~40 MiB (~0.3% of a
  ~12.5 GiB title-class budget); (c) kernel-internal loader policy — see §4.

## 3. The launch-12 budget (task 3)

All four capacity arrays to 32,768 (`kBridgeVmmapEntries` ×3, lmf fold 1024→32,768):

| array | today | after | Δ |
|---|---|---|---|
| `g_bridge_vmmap` (1,160 B/rec) | 19,005,440 | 38,010,880 (36.25 MiB) | +19,005,440 |
| `g_spanpub_records` (24 B) | 393,216 | 786,432 (768 KiB) | +393,216 |
| `window_anchor_verify::mappings` (24 B) | 393,216 | 786,432 (768 KiB) | +393,216 |
| lmf `capture::mappings` (24 B) | 24,576 | 786,432 (768 KiB) | +761,856 |
| spanpub top-16 table | 384 | 384 | **0** (bounded at kMaxPublishedSpans=16; saturation is orthogonal — fixture C3) |
| **total** | **19,816,448** | **40,380,928** | **+20,553,728 (+19.60 MiB)** |

Cross-check vs the mission's phrasing: "+768 KiB lmf BSS" is the lmf array's
after-value (768 KiB, from a shipped 24 KiB); "~36.3 MiB g_bridge_vmmap" is
exactly 38,010,880 B = 36.25 MiB. CAPTURE-CAPACITY.md §5.3's list omits the
bridge's own `window_anchor_verify` array — it also scales and is budgeted here.

**Post-bump totals vs limits:**

| quantity | value | limit | margin |
|---|---|---|---|
| new .bss | 42,112,480 B (40.16 MiB) | — | — |
| writable segment memsz (16 KiB-aligned) | 40,188,672 B | MAXDSIZ 32 GiB (documented) | **815×** |
| new total PT_LOAD memsz | 74,978,914 B (71.51 MiB) | console-proven 54,425,186 B | 1.378× |
| new BSS | 42,112,480 B | console-proven resident ≥19.5 MiB | 2.05× |
| per-segment | 40,188,672 B | SceKernelModuleSegmentInfo.size u32 (4 GiB report field) | ~106× |

**Verdict: AFFORDABLE.** The documented caps are three orders of magnitude
away; the empirical envelope that actually constrains (the booted class) is
doubled but doubles along an axis with six boots and zero memory-class signals.

## 4. Honest unknown

**No PS5 dynlib/PRX-loader-specific cap is documented in the pinned SDK.** The
kernel's internal behavior is unverifiable from here: (a) whether the prospero
loader imposes an undocumented per-module/per-segment memsz ceiling, and (b)
how the kernel sizes its sysctl `KERN_PROC_VMMAP` temp buffer when asked for
38,010,880 B (today's 19,005,440 B request has served ~19 MiB of real map data,
6/6 boots; a sane kernel allocates by actual map size, not request size, but
that is source-inference, not target evidence). Both unknowns are
*class-unchanged* by a 2× bump and have produced zero negative signal across
six boots. The launch-12 verification pass (existing scoped-verifier pattern)
is the right place to retire them with one real boot.

## 5. Alternative lever (bounded proposal — not required by this verdict)

Keep `kBridgeVmmapEntries = 16384`. The walk **already parses every record**
(`bridge_vmentry_entries=22761`); only the SNAPSHOT copy is bounded. Feed
`bridge_spanpub_publish` from the walk stream instead of the 16,384-record
prefix so interest-spans above the prefix cut (the tail / true heap top)
register into the existing bounded top-16 table, and define the published
envelope over the full walk. Zero added BSS.

Contract warning: this rewrites pin D1 semantics ("envelope = max end over the
CAPTURED prefix"; omitted ⇔ at/above envelope; T5's honest-unmapped-below-cut).
A full-walk envelope eliminates the truncation class for span queries, so the
capture-truncated refusal and fixtures E4/H5/T1–T5 must be re-derived — a NEW
bounded revision with distinct IDs, host-fixture first, never a loosening of a
refused launch.

## 6. Sources (all local, verified reads)

- PRX + objects: `outputs/2026-10-04-zcode-controller/launch10-build/prx-candidate-l10-2/` (parsed), `prx-candidate-l10-1/bare-sdk/` (cross-check)
- Arrays: `launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp` (:240-241, :420, :708), `final-units/live_module_facts.geometryfix.cpp` (:206), `worker-1-live-supplier/src/live_module_os_ops.hpp` (:33), `worker-comp6-driver/staging/lineage-overlay-2/SeedValidation.hpp` (:151)
- Limits: `sdk-v0.43/.../machine/vmparam.h` (:55-70), `native-module-metadata/upstream/libkernel.h` (:45-83, :156)
- Console evidence: `worker-verdict-first-real/{REPORT.md,LAUNCH10-RESULTS.md,CLAIM.json,classifier-outputs/}`, `launch10-prep/{runtime-status,sdiag}-baseline-run5.*`
- Capacity plan: `worker-capture-22k-fixture/CAPTURE-CAPACITY.md` §5
- Frozen baseline: `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/build-1/` (parsed), `worker-import-audit-weakify/AUDIT-RESULTS.md` (:17)
