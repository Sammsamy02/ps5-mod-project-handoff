# PREENTRY-REVIEW — pre-entry execution window map + twelve-edit attribution + exoneration challenge

Worker: worker-preentry-review, 2026-10-05. Independent read-only review; no edits to
sources, no builds, no console contact. All paths under
`outputs/2026-10-04-zcode-team/worker-preentry-review/` (this file is the only write).

Subject: the staged candidate compose `64d8c6e7` (bare-sdk ELF `8ede717f`, staged fself
30839416 B) fails at ploader injection with zero evidence of module start. Task: map
everything that executes between PRX mapping and the first
`diagnostic_event("module_start_entered")`, classify the twelve
`seven_edit_fold_2026_10_05` edits against that window, and challenge the controller's
static exonerations.

---

## 0. Candidate identity and the TRUE binary delta (foundation for attribution)

The staged-to-console failing artifact chain (all verified from run receipts):

| stage | failing candidate | last console-good (pid 700) |
|---|---|---|
| composed object | `64d8c6e7` | `a0dacb3a` |
| bridge packaging TU object | `309676bf` (bridge f9747267, edits 3/7/11) | `18047a07` (pre-fold bridge) |
| combined partial-link object | `4a218ac3` | `3e1dafe2` |
| bare-sdk inner ELF | `8ede717f` (5 determinism re-runs byte-identical) | `2b700bdd` |
| staged fself | 30,839,416 B (readback byte-identical, run 2026-10-05T20:07:37Z) | `07ac3e1e` 30,849,159 B |

Critical fact #1: pid 700's console-proven module (fself `07ac3e1e`, INDEX-pid700-fullfix)
was produced by THIS SAME launch11-combined pipeline at 18:36 (aside-1791247296
artifacts-receipt). So "prior compose from the same pipeline ran fine" is exact, and the
delta good→failing is precisely the edits that landed 18:36→20:00.

Critical fact #2: build-object diff between aside-1791247296 (good) and
aside-1791248544 (failing): ALL ten frozen bootstrap units are byte-IDENTICAL — including
`runtime_module_entry.o` (the entry shim) and `diagnostic.o` (the diagnostic sink).
Only `runtime_start_bridge.o` and the composed object changed. The entire pre-entry
machinery is bit-for-bit the machinery that loaded, started, and ran at pid 700.

Critical fact #3: symbol-surface diff of the two combined objects: strong-UND sets
identical (205 == 205, zero diff); defined global FUNC delta = exactly
`+bridge_kernel_range_verdict`. `syscall` is DEFINED in BOTH objects (27-byte FUNC,
`.text.syscall` — the libc trampoline was already statically present in the pid-700-good
module), which defuses any "new unresolved syscall import" theory.

Critical fact #4: the staging run (worker-staging-next 20261005-200730-stage-c) verified
readback byte-identical (`l10-readback-verdict.json` ok:true), ploader.ini byte-equal
pre/post and never written (pinned `1216aab6…`), pre-image saved + on-console `.bak`
(`l10-go-nogo.json` all 8 checks PASS). Staging/transfer integrity is proven; the
console demonstrably received `8ede717f`'s fself. There is no later run dir that could
have overwritten the slot before launch.

---

## 1. The pre-entry execution window (from the actual sources)

Sources read: `worker-comp6-driver/staging/lineage-overlay-2-aside-1791248544/lifecycle/
runtime_module_entry.c` (entry shim, byte-identical to the good build),
`worker-diagnostic-sink/staging/diagnostic.c` (f5e96c41, gated),
`worker-prx-link-gaps/package_prx.py` (frozen, e6fbe6c5) + driver
`build_runtime_composition6_l11.py` package_phase (in-memory injection of entry-unit
swap, diagnostic-staging swap, `-std=c++20` for clang++), Packager semantics
(explicit DT_INIT/DT_FINI, loader-owned arrays zeroed), overlay `generated/probe_config.h`,
pinned ploader analysis (`worker-ploader-imports/PLOADER-IMPORT-ANALYSIS.md`,
`.superpowers/native-dependency-spike/ploader-return-message-1/REPORT.md`,
`ploader-cleanup-candidate-1/original/main.c`), and the failing vs good bare-sdk ELFs
(parsed directly).

### W-1..W0 — before the module is touched (ploader-side, per pinned main.c)

- **W-1 arm**: ploader reads `ploader.ini` ONCE at arm time. Ini order (staging run
  `l10-ploader.ini.pre`, byte-verified): `/data/plugins/modbridge_supplier_composition.prx = 2`
  (line 3) comes BEFORE `/data/plugins/modbridge_path_test.prx = 2` (line 7). The
  composition is injected FIRST, path_test second.
- **W-2 readiness scan** (`deps_ready`/`wait_stable`, main.c:509-559): gates on
  libkernel.sprx + libc.prx + libSceFios2.prx present, LoadStartModule resolvable, and a
  loaded-module count (6 early / 16 late). NOTE: this gate does NOT name
  libSceLibcInternal or any other import PROVIDER of the composition's 196-import set.
- **W-3 per entry**: mode-2 wait up to 25 s for the file; PT_ATTACH + ucred jack;
  target-side `access()`; mmap + copyin of the PRX; in-target
  `sceKernelLoadStartModule(path,0,0,0,0,NULL)` via ptrace register hijack + INT3.
  **`pRes` is NULL — the module initializer's return value is never collected**
  (pinned analysis §1; ploader-return-message-1 REPORT). FAIL = `rc <= 0` of
  `inject_prx`: its own `-1` (access/resolve/mmap/copyin) or the collected
  sceKernelLoadStartModule return. The FAIL notification CANNOT say which load substage
  refused; only a locally built diagnostic ploader adds `rc=0x%08x`, and the user's
  installed binary is not matched to either local build.

### W1..W6 — kernel-side load (sceKernelLoadStartModule), zero module instructions

- **W1 shape gates**: program-header scan (PT_SCE_MODULE + module note + SDK stamp
  0x11000000), "turns away a module while scanning program headers if it names a dynamic
  table without the fourth (SCE dynamic) segment" (DynamicWriter.cs header comment).
- **W2 segment map**: 14 phdrs; PT_TLS present but EMPTY (filesz=0, memsz=0) in BOTH the
  good and failing ELFs — no TLS image, no TLS risk at all.
- **W3 dynamic processing**: 20428 RELA entries (DT_RELASZ 490272) — IDENTICAL count in
  the good and failing modules; DT_PLTRELSZ 96 identical; dynamic tag set identical,
  only address-valued tags shifted by the constant 0x1040 (content growth). Loader-side
  relocation processing writes R_X86_64_RELATIVE values (incl. the 235 `.init_array`
  slots and GOT) — pure data manipulation, no module code.
- **W4 import binding**: 196 NID imports, exact `<nid>#<library>#<module>` identity match
  against modules resident in the GAME process at that instant. Strong (GLOBAL) UND that
  cannot bind aborts the load; weak binds to 0. The import TABLE is byte-equivalent in
  set/identity/count to the pid-700-good module (suffix histogram identical:
  A#B 126 / B#C 19 / C#C 44 / D#D 4 / E#E 3) — but BINDING is evaluated against
  whatever providers happen to be resident at THIS injection's W4 instant.
- **W5 arrays**: loader-owned DT_INIT_ARRAY/DT_FINI_ARRAY/DT_PREINIT_ARRAY = 0 (verified
  in both ELFs) — the loader runs NO static constructors. The 235 `.init_array` slots
  are inert data until the entry shim walks them.
- **W6 DT_INIT call**: DT_INIT = `module_start` @ vaddr 5536 (same in both),
  DT_FINI = `module_stop` @ 7680. The loader calls module_start exactly once.

### W7..W9 — module_start up to the first event (the only module code in the window)

From `runtime_module_entry.c` (byte-identical to the good build):

1. `__atomic_test_and_set(entry_guard)` (BSS) → `-20` on contention (no writes anywhere).
2. lifecycle.state checks (BSS, PROBE_NEW at load) → `0`/`-21`.
3. `diagnostic_ready()` — three volatile reads of import-stub addresses; NO calls.
4. `diagnostic_open_channel()` — **the FIRST kernel call of the module**:
   `sceKernelOpen("/data/townfall_runtime_aligned_entry.log", O_WRONLY|O_CREAT|O_APPEND|
   O_NOFOLLOW, 0600)`. The sdiag sink is LAZY: `channel_fd = -1` at static init
   (diagnostic.c:37), opened only here (diagnostic.c:87-92); the supplier jsonl sink
   `supplier_fd` is likewise lazy on the first event (diagnostic.c:184, 272-289).
   **No file I/O exists anywhere at static-init time**; diagnostic.c emits no
   constructors (constant-initialized statics only).
5. If channel < 0: `write_ue4ss_log_line("diag_channel_open_failed", …)` — which itself
   early-returns when `!diagnostic_ready()`, and otherwise tries a SECOND
   `sceKernelOpen("/data/plugins/UE4SS.log", …)`; if that open also fails the function
   returns silently — then `module_start` returns `-22` with NOTHING written to any
   channel. This is the one complete zero-evidence module-code path.
6. `diagnostic_event("module_start_entered", 0)` — first event, dual-channel write
   (entry text log + `/data/modbridge-supplier-diag.jsonl`).

### W10+ — everything the twelve edits actually touch

- `entry_incoming_sp_mod16` / `entry_call_sp_mod16` / `constructor_count` events, then
  `probe_lifecycle_start` walks `__init_array_start..__init_array_end` — the 235 static
  constructors (owned by the composed UE4SS units; the count is 235 in BOTH the good and
  failing modules — unchanged). EACH constructor is individually wrapped with
  `constructor_begin` / `constructor_return` events (runtime_module_entry.c:103-110), so
  even the first crashing constructor would have left durable evidence. Bridge code
  (`modbridge_runtime_start_with_activation`) and the initializer's pool wait loop run
  only after a clean constructor run. The LuaMod/program/String.hpp code runs at mod
  load/print time, much later.

**Window conclusion: between mapping and the first event there is NO module code except
the six steps in W7-W9. No TU of the twelve edits executes a single instruction in that
window. Every constructor, every string conversion, every file I/O of the edited units
is post-first-event. Zero events therefore localizes the failure to W1-W6 (kernel load
refusal), W3' (ploader-side -1), or W7 step 4-5 (first-open failure path).**

---

## 2. Per-edit classification (twelve edits of seven_edit_fold_2026_10_05)

Legend: PRE-ENTRY? = executes in W-1..W9. KILL? = has a mechanism to produce
"inject FAIL + zero evidence".

| # | edit | object/source delta | PRE-ENTRY? | KILL? | mechanism / closest reach |
|---|---|---|---|---|---|
| 1 | SeedValidation.hpp v5 (final-units, b3d81b0f→db07ac60) | header; consumer = initializer TU (+fixtures) | NO | NO | publish-policy + FailureClass + resolve_listing_gap; all inline/constexpr; runs inside the initializer constructor (W10). PoolShapeCheck 24→32 B is a stack-local aggregate there. init slots 235→235 (no new ctors). |
| 2 | SeedValidation v5 mirror (initializer staging) | byte-identical mirror of 1 | NO | NO | same |
| 3 | bridge +kernel_range_verdict (faec2336→ec53442b) | runtime_start_bridge TU | NO | NO | called from initializer pool loop (W10); `static unsigned char vec[4096]` function-local BSS (+4096 of the +4160 BSS growth — no relocs, no ctor); `::syscall` statically defined in BOTH good and failing objects. |
| 4 | initializer one-shot resolve_listing_gap consult (22f06286→e7f349af) | initializer TU | NO | NO | function-local `static bool gap_probed_this_boot` + `static const char* gap_note_cached` — constant-initialized, no init_array delta, no TLS. W10 only. |
| 5 | program setup_mods widen_ascii (7b5d4636→68b8bd18) | UE4SSProgram TU | NO | NO | setup_mods runs in the runtime init thread (W10+). No load-surface change. |
| 6 | LuaMod modbridge:: qualification ×8 (3c39a86b→c92c92ff) | LuaMod TU | NO | NO | call-site binding change; UND set identical (540==540 prebuild); the dropped dead shadow `RC::normalize_path_for_lua` was never referenced. W10+ (mod load). |
| 7 | bridge errno-convention fix (ec53442b→59db9dcd) | runtime_start_bridge TU | NO | NO | return-value mapping only; fails-safe unchanged; W10. |
| 8 | SeedValidation warning hardening (db07ac60→8a9a1c83) | header ×3 copies | NO | NO | default member initializer `failure_class = FailureClass::None` (uint8_t enum) — aggregate 24→32 B; single consuming TU; bridge does NOT include the header; ODR-mix impossible in this compose (single staged header, stage-header pin refreshed to 8a9a1c83 at 19:52:09). |
| 9 | program start/reinstall widen (→cb6cb14a) | UE4SSProgram TU | NO | NO | start_mods<LuaMod> / queue_reinstall_mod — runtime paths, W10+. |
| 10 | String.hpp to_generic_string PROSPERO byte-widen (→c76be502) | header | NO | NO | inline constrained template (COMDAT per instantiation); all TUs in this compose see the ONE staged header (the 7c983db4 regression-guard pin was a manifest refresh item, re-pinned per MANIFEST-REPOINT). Even a worst-case ODR pick changes print-pipeline behavior only — post-entry by minutes. The historical "inline lost at link" hazard required TWO header copies in one link; none exists here. |
| 11 | bridge errno-macro compile fix (→f9747267) | runtime_start_bridge TU | NO | NO | compile-time-only syntax fix (errno is a macro); no runtime delta beyond 3/7. |
| 12 | gate accessor allow-list (+bridge_kernel_range_verdict to linker_provided/reconciliation) | driver + fixed edition scripts | NO | NO | build-gate bookkeeping for the composed object's extern reference; the bridge defines it; pre-proof + accessor verification receipts exist (receipts/accessor-verification-*.json, 20:03). |

**No edit classifies as PRE-ENTRY or KILL.** The only load-image deltas the fold produced
are: +1 defined global function, +4160 B (seg0 +1104, seg1 +1232, BSS +4160), and the
constant 0x1040 shift of dynsym/strtab/rela/jmprel — all inside a shape that the SAME
loader accepted at pid 700 with identical phdr/dynamic/import/RELA structure.

---

## 3. Challenge of the controller's static exonerations

| exonerated surface | my independent verdict |
|---|---|
| phdrs | **CONFIRMED** — all 14 phdrs identical in type/flags/align file-by-file between good (`2b700bdd`) and failing (`8ede717f`); PT_TLS empty in both; PT_SCE_MODULE + both PT_NOTEs (36 B / 24 B) identical sizes. |
| imports (196 contracted) | **CONFIRMED for the table** — 196 decoded NID rows, zero unknown NIDs, suffix histogram identical, strong-UND set of the combined object identical (205==205), no new import of any kind (`syscall` statically defined in BOTH). **HOLE (scope, not error):** the exoneration covers the import TABLE, not import BINDING TIME. The kernel binds these 196 at W4 against providers resident in the game process at that instant; the ploader readiness gate (W-2) names only libkernel/libc/libSceFios2 — libSceLibcInternal (qsort's provider) and every other provider are unguarded. Same table, different boot, different residency → different outcome. This is exactly the class the static program cannot see. |
| SDK note | **CONFIRMED** — PT_SCE_MODULE 32 B + SDK stamp 0x11000000 verified in the failing ELF (static check sdk_stamp_0x11000000 PASS; same stamp class console-proven to fix 0x80020063). |
| dynamic tags | **CONFIRMED** — identical tag set and counts; DT_INIT/DT_FINI same vaddrs (5536/7680); loader-owned arrays 0; only +0x1040 address shifts; DT_RELASZ byte-count identical (490272 = 20428 entries) — the loader's relocation workload is identical in size to the module it already accepted. |

**The real hole in the exoneration program is not one of its four verdicts — it is what
the program never captured:**

1. **The FAIL rc was never captured.** Per the pinned ploader sources the popup carries a
   raw value (diagnostic builds add `rc=0x%08x`; the base build prints the plain FAIL of
   `rc <= 0`). `0xffffffff` ⇒ ploader-internal (-1: access/resolve/mmap/copyin);
   `0x8002xxxx` ⇒ kernel load refusal class (ESDKVERSION `0x80020063` already excluded by
   the stamp). The task notes the popup format is pending from the user. Until it exists,
   every module-content theory is subordinate — and every static exoneration, while
   correct, is logically incapable of EXPLAINING the FAIL; it can only narrow it.
2. **The post-FAIL file state was never archived.** No post-launch pull exists (the only
   post-20:07 pull is the plumbing test to port 9, failed by design). Worse, the staging
   run's own PRE-launch baselines ALL failed (rc=3, four files recorded absent —
   `l10-baselines.json` "present: false" ×4): the console log state was cleaned before
   this launch. So "sdiag byte-identical to the prior run's" is a report against an
   EARLIER pull (e.g., launch10-evidence l11run2/3), not an archived comparison inside
   the failing run. The zero-evidence triad is currently UNVERIFIED evidence.
3. **Timing/residency classes are untested** — see the import-binding hole above.
4. **The -22 zero-evidence path is real but the pinned analysis caps it**: with `pRes`
   NULL the initializer's return is never collected and the pinned report explicitly
   writes "No assumption is made that changing module_start's return must change
   Ploader's OK/FAIL notification." So a module that ran and returned -22 should surface
   as OK-with-dead-mod, not FAIL — unless the specific kernel build fails the load on a
   negative init result. If the popup rc ever shows such a class, the -22-with-both-
   opens-failing path (fios2 not yet writable for the game at composition's DT_INIT,
   recovered by path_test's turn) becomes the ONLY module-reachable explanation — but
   note it involves ZERO of the twelve edits (entry shim + diagnostic.c are
   byte-identical to the pid-700 build).

---

## 4. Alternative explanations for "inject FAIL + zero evidence" (not module-content)

| # | explanation | fits zero-evidence? | fits path_test OK? | fits pid-700 prior good? | rate |
|---|---|---|---|---|---|
| A | **Kernel load refusal — import binding at early injection** (provider-residency race; composition is FIRST in ini order and binds 196 imports incl. libSceLibcInternal/qsort, unguarded by the readiness gate) | YES — module code never runs, no events/log/status by construction | YES — path_test's tiny import set binds trivially seconds later | WEAK — same bind-set succeeded 5+ prior boots; but boot-to-boot residency timing is exactly the uncontrolled variable | **HIGH (top)** |
| B | **inject_prx -1 — mmap/copyin of the 30.8 MB fself failed under target memory pressure** (or an access() race on the just-uploaded file; readback makes the race near-zero) | YES — pre-entry by definition | YES | WEAK — same size class booted fine repeatedly | **MEDIUM-HIGH** |
| C | **Kernel propagates a negative module_start rc as load failure; module ran and took the -22 path** (first sceKernelOpen AND UE4SS.log open both failed → silent -22; /data/fios2 write-availability timing at composition's DT_INIT) | YES — the one complete zero-evidence module path | YES — /data writable again by path_test | WEAK — identical entry/diagnostic bytes ran fine at pid 700; never observed in 5+ boots; also contradicts the pinned "init result never collected" reading unless this kernel fails load on negative init | LOW-MEDIUM (jumps to top IF the popup rc shows an init-failure class) |
| D | **Stale slot / post-stage overwrite or wrong file ran** (slot not `8ede717f` at launch) | YES | YES (path_test untouched) | NO — contradicts byte-identical readback 20:07:37 + no later writes in any run dir | VERY LOW — close it by re-RETR of the slot hash at failure time |
| E | **Observation error on the zero-evidence triad** (sdiag compared against a wrong/absent baseline; file recreated; wrong path pulled) | n/a | n/a | n/a | LOW but MUST be closed: pull all four files fresh (entry log, supplier-diag jsonl, UE4SS.log, runtime-status) before any code action |
| F | module crash before first event (W7 steps 1-4) | YES (no events) | **NO** — a crash in the game process during load would kill the game; path_test ran after | — | VERY LOW (excluded modulo injector isolation, which pinned main.c does not do) |
| G | double-invocation of module_start (DT_INIT + NID export call) → -20 | YES (silent) | YES | NO — identical wiring at pid 700; loader calls DT_INIT once | VERY LOW |

Discriminators already available: popup rc value (A: kernel class; B: 0xffffffff; C:
kernel init class), popup TIMING (B and G: instant; A: fast; the 25 s mode-2 shape only
applies to missing-file), a fresh pull of the four evidence files (closes D/E), and one
retry launch after full boot warm-up (A predicts possible PASS; C/B predict stable FAIL).

---

## 5. Ranked most-likely causes (top 3)

1. **Kernel load refusal of the composition module at import-binding time (provider
   residency / boot-timing class), composition-first ini order.** Mechanism: W4 binding
   of the 196-import table fails against not-yet-resident providers; the loader aborts
   before W6; not one instruction of the module runs — zero events, zero log, zero
   status by construction; path_test (second, trivial imports) succeeds. Not
   attributable to any of the twelve edits (identical import surface to the pid-700
   module); attributable to boot-timing variance the pipeline has never controlled.
   Discriminator: popup rc (kernel error class) + retry-after-warm-boot.
2. **inject_prx -1 on the 30.8 MB copyin/mmap under target memory pressure at that
   boot.** Mechanism: W-3 target-side allocation/copy fails; FAIL instantly; module
   never mapped; zero evidence; path_test (small) succeeds. Discriminator: rc=0xffffffff,
   instant FAIL timing.
3. **The -22 first-open failure path IF (and only if) this kernel build fails the load
   on a negative initializer result** — module ran, both /data opens failed at its W7
   step 4-5 instant (fios2 write-availability), silent -22, loader reports FAIL.
   Mechanism verified in source (entry shim lines 342-344 + write_ue4ss_log_line
   early-outs); requires an exceptional kernel semantics AND a never-before-seen /data
   timing miss; involves zero of the twelve edits. Discriminator: popup rc class;
   next-launch behavior of the entry log channel.

No module-content cause ranks: the twelve edits have no mechanism in the W-1..W9 window,
and the entire pre-entry machinery (entry shim, diagnostic sink, all ten bootstrap
units) is byte-identical to the module that loaded and ran at pid 700.

---

## 6. Evidence-first next steps (no builds, no console writes)

1. Get the popup text/rc from the user (diagnostic ploader builds exist locally; the
   installed binary identity is itself unverified — treat the rc as the single most
   decisive datum).
2. Fresh-connection pull of the four evidence files + the slot hash at failure time
   (readback law: plain files byte-exact; fself PT_LOAD law) — closes D and E and
   grounds the zero-evidence triad in archived bytes.
3. One user-driven retry launch after a full warm boot, unchanged staging — A predicts
   possible PASS; B/C predict stable FAIL. Any retry gets a NEW run id; no silent
   re-stage.
4. If A survives: re-order or re-time injection (ini order is a controller-owned,
   content-anchored single edit with .bak law) or add provider modules to the readiness
   consideration — decisions for the controller, not this worker.
