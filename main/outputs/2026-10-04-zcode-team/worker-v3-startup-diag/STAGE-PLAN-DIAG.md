# STAGE-PLAN-DIAG — minimal startup diagnostic (igd1-fc26d6c03038)

Worker: worker-v3-startup-diag, 2026-10-04. Purpose: localize the v3
(igf3-de0c0cb644d3) zero-records result. The cause is UNCONFIRMED; this
variant is designed so the FIRST staged launch decides between
"module loads+starts" and "module fails before module_start".

## 1. Frozen payload

- File: `outputs/2026-10-04-zcode-team/worker-v3-startup-diag/
  diag/determinism/run1/modbridge_ingame_facts.prx`
- Build id: `igd1-fc26d6c03038` (records in the shared log are identifiable
  by `"build_id":"igd1-fc26d6c03038"` — never confusable with igf2/igf3).
- fself: **12173 B**, sha256
  `13eeb7c55e82a83b4aee3b90d36231ca7738c5431d4974ed9785d4555dd28754`
- Inner ELF (bare-sdk): 68880 B, sha256
  `78dbf702f486255b6239ff8a9c9e76f4100d29cbf242370981145deaebc39ecd`
- Contract sha256 `c5c482a75423fab0ce24f21710f24e1a6f496f53cc93d0c8f02b741b1171e17f`
  (same 7 verbatim frozen rows).
- Two-run determinism: `determinism/run1` == `run2` byte-identical at fself,
  inner ELF (original + sdk-stamped), combined object, contract
  (DETERMINISM-PASSED, see build log in this worker dir).
- module_start (275 B) does EXACTLY: guard → `sceKernelOpen(IG_LOG_PATH,
  O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600)` → ONE seq=1 `"entry"` record
  (v3 writer, full 64-attempt bounded write retry) → `sceKernelClose` →
  return 0. NO experiments, NO VMMAP, NO predicates, NO base scan, NO ctor
  walk, NO C++ anywhere in the binary. On open failure it ALSO returns 0
  (controller directive: the nonzero-entry-return variable is removed — v3
  returned -22 there; kernel-side propagation of a nonzero entry return is
  unknown). module_stop = guard + return 0.

## 2. Staging (same-file replacement, no ini change)

1. Back up the current v3 file first (reversible-op rule):
   `/data/plugins/modbridge_ingame_facts.prx` ->
   `/data/plugins/modbridge_ingame_facts.igf3.bak` (if not already present).
2. Upload the fself above via FTP (2121, binary) over
   `/data/plugins/modbridge_ingame_facts.prx` (same path, same
   `ploader.ini` line — module name and path are UNCHANGED).
3. One launch, per the standing ritual: game fully closed -> ploader armed ->
   user launches Townfall (never automated). ploader shows "injecting 2
   plugins" then OK/FAIL. `igd1-` records appear (if at all) ~1 min in.
4. Collect afterwards: ploader surface text (exact OK/FAIL line + ms),
   `/data/modbridge-ingame-facts.jsonl` size/mtime/tail, sibling
   `/data/townfall_runtime_aligned_entry.log` mtime.

## 3. Upload verification — CORRECTED ftpsrv oracle expectations

STAGE-PLAN-V3's oracle (exact 109776 B / c52e582d) was WRONG: it contradicts
the channel behavior documented in STAGE-PLAN-V2 and observed on v1. The
channel's decryption readback is the segment-covered prefix of the inner ELF
(section-header tail removed); across v1, v3, and this build the readback
size ALWAYS equals (bare-sdk size − exactly 1560):

- Expected readback size: **67320** (= 68880 − 1560; v1: 98240→96680; v3:
  109776→108216 — same law).
- Readback begins `\x7fELF` (stored host fself begins `4f 15 3d 1d`).
- Full-hash equality through this channel is IMPOSSIBLE by construction
  (SHT tail stripped; e_shnum zeroed in the embedded header). Acceptance =
  ELF magic + the size law + header-page comparison per STAGE-PLAN-V2 §1
  (payload beyond the header page byte-identical to bare-sdk except sparse
  header/SHT-rebuild bytes).

## 4. Outcome interpretation matrix (decide on FIRST launch)

| # | Observation | Meaning | Next action |
|---|-------------|---------|-------------|
| 1 | `igd1-` entry record PRESENT, game runs | Module loads AND module_start runs; open+write path works. The v3 failure is in the EXPERIMENT CODE (runs after the marker in v3). | Incremental bisection: igd2 = diag + `ig_collect_vmmap`/`vmmap_walk.c`; igd3 = igd2 + experiments.c V1/V2 (VMMAP + base hunt); igd4 = + predicates/C++ (v3-equivalent minus V3/V4/V5). One launch each, distinct build ids. |
| 2 | `igd1-` record ABSENT, game runs normally, ploader OK | Module accepted by the loader but module_start either never ran or could not open/write /data — with rc=0 always, ploader cannot distinguish. Packaging/build-level delta is the suspect (NOT experiment code). | Compare loader-visible structure vs v2 (the only console-proven build): size 12173 vs 30701 fself, .bss 0x14000-vs-5.6MB, GLOB_DAT count 4-vs-7. Control: restage the v2 fself (`igf2` backup) to re-prove the slot still works, then bisect build flags (e.g., link vmmap_walk.c only). |
| 3 | `igd1-` record ABSENT + game CRASHES | A 12 KB 7-import module faults the system: PRX format/import interaction itself. | Halt bisection; investigate fself/imports vs the aligned sibling; restore `igf3.bak`/`igf2.bak`. |
| 4 | `igd1-` record PRESENT but MALFORMED (not strict JSON) | Writer bug in the shared record path (v3 record_sink) — would localize v3's failure to the first write. | Fix record_sink, rebuild diag, relaunch. |

Limitation (by design, frozen scope): outcome 2 cannot separate
"module_start never ran" from "open failed" — no second signal channel
exists in the frozen spec. If the controller needs that split, a follow-up
variant writing an alternate-path fallback line on open failure is required
(new build id, new staging).

## 5. ploader semantics (from retained source)

`.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`:
`inject_prx()` runs remote `sceKernelLoadStartModule` (NID `wzvqT4UqKX8`) in
the title; `int ok = (rc > 0)` (line 810) — **"OK" means LoadStartModule
returned a positive module id; module_start's own return value goes to the
kernel's start step and is NEVER visible to ploader.** A v3-style -22 (open
failure) would only surface as FAIL if the kernel propagates nonzero entry
returns into LoadStartModule's result — unknown from this source. The
observed "OK (~1207ms)" on both v3 launches therefore proves loader
ACCEPTANCE (imports resolved, segments mapped), not that module_start
reached the marker.

## 6. Rollback

Restore `/data/plugins/modbridge_ingame_facts.igf3.bak` (or the known-good
v2 `igf2` backup) over the staged file. The diagnostic module stays INERT
after its single marker attempt (no threads, no scans, closes its fd,
returns 0).
