# PAIRED-STAGING — one user-coordinated probe window carrying BOTH probes

Worker: worker-probe-pairing-prep · 2026-10-05 · PROPOSAL ONLY. LOCAL ONLY.
No console contact was made; every console step below is the CONTROLLER's, the
launch is USER-COORDINATED, and the window must NEVER overlap launch-10's
window. Every console constant is [TARGET] until the Step-0 RETR proves it.

## 0. The two probes being paired (inventory, all verified on disk today)

| | probe 1 — IGW row-0 exec-write | probe 2 — fc1pool Blocks[0] capture |
|---|---|---|
| worker of origin | `worker-igw-row0-rebuild` | `worker-fc1-validator-probe` |
| local fself | `worker-igw-row0-rebuild/modbridge_exec_write_probe_armed.prx` — 20013 B, sha256 `e1541af612f97bb450b5f61888ae0c2fc93dea72454723dc9e0ffafabd6002df` (re-verified on disk this session) | `worker-fc1-validator-probe/modbridge_fc1pool.prx` — 22733 B, sha256 `d2431bf842fdba85321a6a08b2c1b6fdc9426c971fbc7cddfcd2b14fb49f0d96` (re-verified on disk this session) |
| build id | `igd1-e030a09eb4d1` (REBUILD of launch-9's `igd1-6ebab07f2111`; delta = exactly 1 semantic byte — row 0 `confirmed` 0→1 — plus build-id churn, BINARY-DELTA.md) | `fc1pool1-78508a2e2940` |
| bare-sdk ELF | 69464 B, sha `cc544d6e…`; SHT-strip readback **67832 B**, body sha `e3f13d19…` (recomputed with the exact batch-tool law this session: prefix == 67832, body-sha match TRUE) | 69408 B, sha `54f8335e…`; SHT-strip readback **67848 B**, body sha `7e2dce27…` (recomputed with the exact batch-tool law this session: prefix == 67848, body-sha match TRUE) |
| console path | `/data/plugins/modbridge_exec_write_probe_armed.prx` | `/data/plugins/modbridge_fc1pool.prx` |
| ini entry (pinned canonical position) | entry 1, `= 2`, anchored to the `[PPSA33286]` header | entry 2, `= 2`, anchored to the xwp line |
| own output channel | `/data/modbridge-exec-write-probe.jsonl` (envelope marker `modbridge.execwriteprobe`) | `/data/modbridge-fc1pool-facts.jsonl` (envelope marker `modbridge.ingamefacts`, disambiguated by its own path + `fc1pool1-` build_id) |
| UE4SS.log writes | best-effort write-through MIRROR of every record to `/data/plugins/UE4SS.log` | exactly ONE `FC1POOL: …` line per launch (`O_APPEND`, ≤256 B) |
| memory behavior | one-shot synchronous `module_start`; row 0 = `sceKernelMprotect(0x7878000, 1 page, 0x4→0x5)` + read test + restore to 0x4 + post-restore VMMAP re-proof; rows 1–5 skip (`confirmed=0`, zero attempts); budget 8, 1–2 consumed; ALWAYS returns 0 | one-shot synchronous `module_start`; plain map-gated READS only (GUA `0x97ba110` → base `0x400000` → pool `0x9765ec0` → ids `+0x14040/+0x14048` → Blocks[0] chase); NO mprotect, 0 `syscall` instructions (build-gated); ≤8×1 s grace; ALWAYS returns 0 |

The fc1pool probe is the ONLY instrument in the tree that can capture the real
`Blocks[0]` u64 (worker-blocks0-provenance verdict: NO CAPTURE EXISTS; the
`[0x970c000,0x9a34000)` figure is mapping-class only). The xwp rebuild is the
only staged instrument that can produce the IGW T2 verdict
(`W_WORKS`/`W_FAILS_R_WORKS`). That is why they pair.

## 1. The ini decision — Option A vs the probe-pair trim (Option P)

**The window's declared composition law: PROBES ONLY — the composition is
excluded.** This immediately constrains the choice, because the preserved
9-entry canonical (`fd9c7393…`, 440 B, FINAL-INI-OK verified) pins
`modbridge_supplier_composition.prx = 2` at entry 3 — Option A's bytes CANNOT
exclude the composition. Flipping a pinned line's mode or deleting lines is
verifier-REJECT / STOP class under the layout law. So there is NO byte-exact,
law-ready ini that stages probes only; any probes-only ini is a NEW layout.

### Option P (RECOMMENDED for this window): 3-entry probe-pair ini — NEW pinned-bytes proposal

Target bytes for `/data/plugins/ploader.ini` (host-computed TARGET only; ASCII,
LF, trailing newline):

```ini
[PPSA33286]
/data/plugins/modbridge_exec_write_probe_armed.prx = 2
/data/plugins/modbridge_fc1pool.prx = 2
/data/plugins/modbridge_path_test.prx = 2
```

- **149 B, sha256 `d6770f44f5908e9188e981175b4797917f524c0404a8c75cb92f069312084598`** (computed this session).
- Content-anchored chain (FINAL-INI-LAYOUT §3.3 pattern): header → xwp → fc1pool → path_test. The first two anchors are IDENTICAL to the canonical chain (xwp anchored to the header, fc1pool anchored to the xwp line — same as `console_batch.py` `FINAL_INI.anchor_of`); path_test re-anchors to fc1pool because its canonical predecessor (aligned_entry) is intentionally absent here. Convergence is trivially unique (3 inserts, one predecessor each).
- Why path_test stays: it is the traversal TERMINATOR — its two `PATH_TEST:`/`ACTIVATION_TEST:` lines can only appear after every earlier `module_start` completed, so "fresh PATH_TEST lines for this launch" proves full-list traversal (3/3 OK notification lines, zero FAIL). It is already on console and enabled today (`1216aab6…` enables it), so the presence guard needs no new upload.

**Honest law cost (this is the recommendation's price, stated up front):**
`verify_final_ini_layout.py` pins byte-exact canonical bytes and WILL REJECT
these 149 B — correctly, for the law it pins. Option P therefore REQUIRES the
layout owner's NEW pinned-bytes decision (a NEW adoption worker/id, the exact
precedent set by `worker-ninth-slot-ini-adoption` adopting the 9-entry
revision) BEFORE anything is staged. This proposal supplies the candidate
bytes for that pin; it does not pin them itself. Never an in-place edit of
`1216aab6…` (which launch-10 freezes byte-exact), never staging unpinned bytes.

Why Option P despite the pin cost:

1. **It is the only shape that matches the window's purpose** (probes only).
2. **Smallest blast radius**: 2 uploads + 3 presence proofs, vs Option A's 5 uploads + 8 presence proofs.
3. **No composition ladder in the window.** The composition's ladder carries fatal-attempt classes; a mid-ladder title death after position 3 would be noise the probe window does not need (both probes' records would survive, but the window's title-state and post-launch forensics would not).
4. **No ridealong threads** (qdrv1 ~2 min window, observer ~60 s window, cc1) writing concurrent evidence during the probes' quiet measurement.
5. **Cleanest possible pool measurement**: fc1pool at entry 2 measures the earliest-possible pool state, with NO composition-side FName activity ever having run (under Option A the composition also never ran before entry 3, but the trim removes the question entirely).

### Option A (RECORDED FALLBACK — law-ready today, NOT probes-only): the preserved 9-entry canonical

- Bytes: `outputs/2026-10-04-zcode-controller/launch9-evidence/ploader-ini-9entry-restore.txt` — 440 B, sha256 `fd9c73937b2b1e45d0cc22721c3e03d977cf027a7a335e5599577b2b7f7c3507`, FINAL-INI-OK verified against `verify_final_ini_layout.py` (rc=0) — re-verified byte-identical on disk this session against `worker-final-ini-layout/ploader.ini.canonical`.
- Both probes already sit at entries 1–2 in exactly the wanted order; the canonical IS a paired ini — it just also enables 6 more modules (composition, qh_driver, observer, aligned_entry, path_test, carrier; igf=0 skipped).
- Honest costs if used for this window:
  - **Not probes-only**: the composition loads and runs its ladder (entry 3).
  - **4 of the 8 enabled files are MISSING on console** (fc1pool, qh_driver, observer, carrier were 0 B / not injected at launch-9 — the incident that created the files-first-config-last law). All four must be uploaded + law-verified first, plus the xwp REBUILD (the launch-9 bytes under that name are superseded history — see §3 hazard H-1). That is 5 uploads and 8 presence proofs.
  - Ridealong thread windows (qdrv1, observer, cc1) run as measurement noise.
- When Option A is the right call: the controller/user want the window to run BEFORE the layout owner pins Option P, and accept the composition + ridealongs. The probes' evidence value is identical (entries 1–2 run first either way; their records land before anything else can fail).

**Recommendation: Option P, pending the layout owner's pin; Option A is the
only lawful fallback until that pin exists.** This worker did NOT pin, edit,
or write any ini anywhere; both byte-strings above are proposals.

## 2. Sequencing — ONE launch is safe (no split needed)

**Verdict: both probes run in ONE game launch, in one ini, xwp at entry 1 and
fc1pool at entry 2. No two-launch spec is required. No reordering is
permitted.**

Interference analysis, mechanism by mechanism:

1. **Temporal exclusion.** ploader injects sequentially in file order (pinned parser: registration order == file order; each mode-2 injection is a synchronous `module_start` + settle). xwp's entire one-shot chain (mprotect → read test → restore → post-restore re-proof → records flushed synchronously) completes BEFORE fc1pool's `module_start` begins. There is no concurrency to interfere.
2. **Address-space disjointness.** xwp row 0 touches exactly `[0x7878000, +0x1000)` — a text-class page far below every fc1pool target. fc1pool reads: GUA anchor `0x97ba110` (96-byte header admission), pool `0x9765ec0` (+0x20 header window, `+0x14040/+0x14048` bootstrap ids), the Blocks[0] pointer chased into the engine heap, and two 40-byte entry windows in the heap. Zero overlap with the xwp page.
3. **Direction of the prot flip is safe even in the pathological case.** Row 0 flips 0x4 (X-only) → 0x5 (R|X): a permission WIDENING (adds R). xwp always restores to 0x4 before returning (`restore` record, outcome 10 on success, then a fresh-VMMAP `post_restore` re-proof). Even a hypothetical failed restore leaves the page MORE permissive (0x5) — it cannot block any fc1pool read.
4. **Map scans do not couple.** Both probes run fresh per-invocation `sysctl KERN_PROC_VMMAP` scans, but fc1pool gates only spans containing ITS OWN addresses; xwp's transient 1-page prot change (restored before fc1pool starts anyway) is invisible to fc1pool's gating.
5. **No shared state.** Neither probe reads the other's outputs. Disjoint own channels (`/data/modbridge-exec-write-probe.jsonl` vs `/data/modbridge-fc1pool-facts.jsonl`). Both touch `/data/plugins/UE4SS.log` append-only at strictly ordered times (path_test precedent class; xwp mirrors records, fc1pool writes exactly one ≤256 B line). Neither writes game memory (fc1pool: reads only, 0 syscall instructions; xwp: the one bounded mprotect+read on its own row, no writes at to_prot 0x5 — no W bit).
6. **Budget fits.** xwp ≈1.6 s stable+settle + a 1–2-attempt bounded ladder; fc1pool ≤8 s grace worst case; both inside the 25 s per-plugin cap; 3 sequential injections ≈ 5–13 s, far inside the 120 s injector timeout.

Ordering contract: the xwp line MUST precede the fc1pool line (both options
preserve canonical positions 1–2). Benefit: the xwp verdict records exist on
disk before fc1pool starts (evidence-before discipline), and fc1pool still
measures the pool before any composition activity (which, in the trim, never
runs at all).

**Contingency only**: if the window's evidence ever showed fc1pool refusing
pool-side in the same launch where xwp succeeded (e.g. `pool_window_uncovered`),
that is a reconcile-first datum, NOT proof of interference — both refusals and
verdicts are terminal evidence for that launch; a retry is a NEW window with
reconciled constants, never a re-rating or blind relaunch.

## 3. Files-first-config-last staging order (controller executes; user launches)

Gate (all before ANY console write):
- G-0. Launch-10's window is fully closed and its frozen byte-equality pre/post state is recorded and undisturbed; this probe window is a separate user-coordinated session that never overlaps it. If launch-10 has not terminated its flow: DO NOT stage.
- G-1. Standing reachability gate (manager 8084, never 9223) + Step-0 RETR pre-image of `/data/plugins/ploader.ini` + sha. Expected pre-image: `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0` (the 2-plugin shortest config). ANY divergence = STOP + preserve + reconcile; never blind overwrite.
- G-2. Console `.bak` STOR under a distinct fresh TAG (`ploader.ini.<TAG>.bak`).
- G-3. If Option P was chosen: the layout owner's pinned-bytes decision for the 149 B `d6770f44…` layout is ON RECORD (adoption receipt, NEW id). If it is not: STOP — either obtain the pin or fall back to Option A's `fd9c7393…` bytes explicitly.

Files FIRST:
- F-1. STOR the xwp REBUILD fself to `/data/plugins/modbridge_exec_write_probe_armed.prx` (same filename as launch-9's — see hazard H-1). Fresh RETR readback; verify with the exact SHT-strip law against `e1541af6…` / `cc544d6e…` / 67832 / `e3f13d19…`. Any FAIL → ONE re-STOR from the verified local fself, re-verify; second failure → abort staging (console keeps prior state) + reconcile.
- F-2. STOR the fc1pool fself to `/data/plugins/modbridge_fc1pool.prx`; RETR readback; verify against `d2431bf8…` / `54f8335e…` / 67848 / `7e2dce27…`. Same one-retry bound.
- F-3. (Option A only) also upload + law-verify qh_driver, observer, carrier per their own owners' constants — the presence guard will refuse otherwise.

Config LAST:
- C-1. Pre-publish presence guard over EVERY enabled entry of the chosen ini (Option P: 3 entries; Option A: 8) — each file present + non-empty via fresh RETR. This is the launch-9-incident guard; it is what makes either option safe.
- C-2. STOR the ini bytes verbatim (Option P: 149 B `d6770f44…`; Option A: 440 B `fd9c7393…`) + byte-exact RETR diff + `verify_final_ini_layout.py` verdict recorded (OK for A; for P the recorded verdict is the NEW pin's own check — the 8-entry verifier's FAIL is expected and documented by the pin, not a staging error).
- C-3. On any failure: ONE bounded restore of the `1216aab6…` `.bak` (the standing single-restore law) and STOP.

Launch:
- L-1. USER launches the game manually (never automated). No claims about outcomes beyond the pinned mechanics in EVIDENCE-CONTRACT.md.

Fetch + restore (after the session ends, game fully closed):
- R-1. RETR both channels + `/data/plugins/UE4SS.log` tail + ploader notification lines; archive under the controller's evidence dir with the session TAG.
- R-2. END-OF-WINDOW RESTORE (obligatory): re-stage the `1216aab6…` 2-plugin ini (byte-exact STOR + RETR diff) so launch-10's frozen baseline stays undisturbed. The two uploaded .prx files MAY stay on disk (unreferenced by the restored ini; presence is harmless, launch-9 precedent) — record their presence in the session archive. Never delete owned evidence jsonl.
- R-3. If any FAIL notification line appeared at launch: STOP + `rollback` path only; never replay a failed activation without a fresh preflight and a distinct TAG.

### Hazard register (staging-specific)

- **H-1 — WRONG-BUILD hazard (top hazard).** `console_batch.py`'s `XWP_ARMED` constant still pins the LAUNCH-9 build (`worker-writability-probe-build`, `igd1-6ebab07f2111`, fself `48949316…`). A batch flow that stages "xwp" as-is would upload the OLD bytes and self-verify them PASS against its own pinned constants — a consistent-but-wrong half-state under the same console filename; the window would reproduce launch-9's all-skip evidence instead of the row-0 verdict. Either the batch-tool owner updates the constants under NEW ids first, or the controller stages the rebuild explicitly from `worker-igw-row0-rebuild/modbridge_exec_write_probe_armed.prx` (F-1 above) outside that constant. The runbook MUST record the on-console build identity actually staged (`igd1-e030a09eb4d1` expected in the seq-1 `entry` line's `build_id`).
- **H-2 — duplicate lines.** ploader does NOT dedupe: a duplicated probe line registers and injects the module TWICE. The Step-0 pre-image check and the post-STOR byte-exact diff are the guards.
- **H-3 — `= 0` mode semantics.** Mode 0 is skipped at parse and consumes no slot: it is NOT a way to "keep an eye on" the composition. If the composition must not run, its line must be ABSENT (Option P) — never flipped to 0 on the pinned canonical.
- **H-4 — fc1pool duplicate-line guard.** fc1 STAGE-PLAN §2 preflight: FAIL if a `modbridge_fc1pool` line ALREADY exists in the pre-image (it does not in `1216aab6…`).

## 4. What this window does NOT do

- It does NOT stage the launch-10 rebuild composition (that milestone keeps its own gating: stray-root disposition, r14 lease fix, gmalloc validator diff).
- It does NOT arm verdict gating (`kExecWritabilityVerdictGating` stays `false`); with the composition excluded there is no same-launch consumer anyway — this window is pure evidence production for the L11 matrix's D2/D3 dimensions and the v4 calibration.
- It does NOT re-rate or replay launch-9 evidence; both probes' launch-9 outputs (xwp all-skip; fc1pool absent) remain on record unchanged.
