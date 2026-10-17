# LATENCY-ANALYSIS — launch-9 injection path (PPSA33286, pid 273)

Worker: worker-injection-latency · 2026-10-05 · read-only analysis, proposal-only patches.
Provenance for every claim: `CLAIM.json` (S## = pinned ploader source, L## = log evidence).

Source basis: pinned ploader `main.c`
(`yourphonelinging0x0/ps5-plugin-loader@1409a94`, sha256 `2b521cd7…`, byte-verified at
`.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`).
All `main.c:L` references below are into that exact file.

---

## 0. Headline answers (coordinator questions)

**Q1 — Does ploader serialize on each module_start's RETURN? YES, as a fact of source.**

- Plugins of a title run one at a time in ini order (`main.c:776-820`, order set by the
  ini parser `main.c:642-683`).
- The wait point is `pt_call` (`main.c:260-290`): `PT_CONTINUE` at `main.c:277`, then
  `waitpid_timeout(pid, &status, 30000)` at `main.c:280`. The injected shellcode ends in
  `INT3` **after** `call r15` (`main.c:93-96`), i.e. the SIGTRAP ploader waits for fires
  only when `sceKernelLoadStartModule` — and therefore `module_start` — has RETURNED.
  Called from `inject_prx` (`main.c:447`) inside the per-plugin loop (`main.c:806-814`).
- So one plugin's `module_start` blocks the START and the OK of every subsequent plugin
  for `min(module_start_duration, 30 s)` per attempt. There is no parallelism and no
  overlap of injections.
- **The 30 s is a cap, not a fixed wait**: if `module_start` returns in 200 ms, `pt_call`
  returns in ~200 ms (`main.c:280` returns as soon as the SIGTRAP arrives).
- On timeout, ploader does NOT wait for the run-away `module_start` (no join; it restores
  regs best-effort at `main.c:281`, detaches at `main.c:817`, emits FAIL at `main.c:814`,
  and proceeds). The module keeps executing **on the game thread the shellcode hijacked**,
  overlapping everything else — including the game itself and the menu.

**Did the entry's ~6 min add serially to the user-visible wait?**

- To the ploader's own progress: only ~30 s (one attempt, capped at `main.c:280`); each
  pid is processed exactly once per invocation (`main.c:755-764`), so there was no retry
  pass behind it.
- To the GAME: effectively yes, ~5-6 min. The whole harness ran inside one
  `module_start` on a game thread (log block 36, L07/L08/L14), and ploader's OK/FAIL came
  at 30 s while the thread stayed occupied for the full harness duration. Which thread
  the kernel stopped for the shellcode is not observable from our logs — but the user's
  "10-15 min before menu" is consistent with a hijacked init/main thread being
  unavailable for ~6 min. Flagged as inference, not measurement.

**Q2 — launch-10 projection under the 2-plugin config (`1216aab6…`)**: see §4.

---

## 1. Measured launch-9 per-stage table

Clock discipline: the entry log has NO timestamps; the table uses within-writer facts
(structure, byte offsets) plus wall-clock anchors (xwp jsonl mtime 11:52, entry log still
growing at 11:58, mirror final ~12:05). No cross-writer stamp comparison is used.

| # | Stage | Writer/source | Measured / bound | Evidence |
|---|---|---|---|---|
| 1 | ploader game detection | source | ≤50 ms poll + one sysctl proc scan per pass; game pid appears as NEW pid only if ploader started first | `main.c:755-764, 825`; L04 |
| 2 | xwp (probe, first ini entry) | xwp jsonl | exactly ONE 15-record pass (module_start→summary); complete by 11:52 | L03 |
| 3 | fc1pool / qh_driver / observer / carrier (missing) | — | each cost ~0 s in `inject_prx` (`access` fails instantly) but each STILL paid the per-plugin readiness wait + attach first: ~1.6 s + ≤0.3-7 s each ≈ **~2 s × 4 ≈ 8 s** per invocation | S09, S08, S10; L01 |
| 4 | supplier_composition module_start (30 MB, VMMAP walk 10,019 + lease registration + fail-closed refusal) | — | **UNMEASURED** (no stamps; runtime-status is single-slot). Bounded ≲ ~1-2 min by ini order + the 11:52 anchors | L10, L13; §3 R3 |
| 5 | entry: readiness + attach before injection | source | ~1.6 s readiness (mode 2) + attach ≤ ~0.3-7 s | S08, S10 |
| 6 | entry: ploader wait on module_start | source+log | capped at **30 s** (`pt_call` timeout) → FAIL toast; harness kept running | S12; L07 |
| 7 | entry: actual harness duration (ONE run, block 36) | entry log | **~5-6 min** (started after xwp 11:52, last write ~11:58; 541 lines, ~65.8 KB, 232 ctors → sleeps → Lua VM → done) | L07, L08, L14 |
| 8 | entry: sleep mechanics | entry log | 3 sleep attempts: 2× EINTR (errno=4) + 1 success; same pattern in ~all historical blocks | L05 |
| 9 | path_test (68 KB, last ini entry) | UE4SS mirror | ONE PATH_TEST stamp for the pid-273 epoch; module_start trivial (2 log lines) — its START was delayed behind entry's `min(6 min, 30 s)` + FAIL path | L02 |
| 10 | invocation tail after FAILs | source | outer loop idles (50 ms scans, all pids known) until the **120 s** cap, then "Timeout" | S03, S04 |
| 11 | runtime init after injection | source | none | S14 |

Reconciliations worth recording (correcting two natural misreadings):

- **"2 entry blocks in the launch-9 window" is wrong.** The +130,726 B delta over the
  session-eve baseline (2,213,030 B) spans the tail of block 35 AND block 36. But the
  baseline byte position lands ~23 lines INTO block 35 (block-35 marker at byte
  2,210,392), i.e. block 35 was already running when the baseline was captured →
  block 35 is the session-eve run. Launch-9 contains exactly ONE entry run (block 36).
  (L06, L07, L09.)
- **"Missing files cause a retry storm" is wrong for one invocation.** A failed plugin
  does mark the title un-injected, but the one-shot `known`-list semantics
  (`main.c:755-764`) mean the title is never re-attempted within that invocation; the
  loop merely idles to the 120 s cap. Re-attempts only happen if ploader is invoked
  again (36 historical blocks ≈ many invocations across sessions). (S04.)

## 2. Where the user's 10-15 minutes went (launch-9 attribution)

Ordered, with confidence:

1. **Entry harness inside `module_start`, one game thread, ~5-6 min** (L07/L08) —
   measured; largest single addressable chunk. ploader only waited 30 s of it; the rest
   ran while the user stared at "loading".
2. **Game's own boot-to-menu** — not ours, not measured here.
3. **Composition module_start** — unmeasured, bounded ≲ ~1-2 min (stage 4). Includes the
   30 MB image read from local /data (seconds-class) + VMMAP walk + lease registration +
   refusal.
4. **ploader fixed overheads**: ≥1.6 s readiness per plugin × 8 ≈ ≥13 s; attach retries
   worst ~7 s; missing-file entries ~8 s of wasted readiness+attach (stage 3); idle tail
   up to ~120 s after the last FAIL (userspace only — does not block the game, but
   matches the user watching toasts).
5. **Repeated invocations** (if the user relaunched the game/ploader during the session):
   each adds its own window; 36 historical entry runs show many invocations happened
   across sessions (L04).

## 3. Ranked reduction candidates + expected savings

| Rank | Candidate | Mechanism (evidence) | Expected saving |
|---|---|---|---|
| R1 | **Entry disabled in launch-10 config** — ALREADY DONE by controller (`1216aab6…`) | removes the ~5-6 min in-`module_start` harness from the boot path (L07/L08, L13) | **~5-6 min** vs launch-9 whenever this config is used |
| R2 | If the entry is ever re-enabled: move the harness body into a worker thread and return from `module_start` immediately | module_start currently runs 232 ctors + sleeps + Lua VM serially on a game thread (L14); ploader's shellcode blocks that thread for the full duration (S11, S12) | **~5-6 min** of game-thread occupancy each time the entry loads |
| R3 | **Measure composition's module_start** before trusting the 2-plugin projection: the OK/FAIL toast already carries `dt` (`main.c:806-814`); better, apply PATCH ploader-03 step log | C_comp is the last unmeasured boot-path stage; if C_comp > 30 s, `pt_call` FAILs it (S12) and every later invocation re-pays it | measurement-enabling; if C_comp > 30 s, worker-offload of the VMMAP/lease walk saves up to ~C_comp per launch |
| R4 | Missing-file early-skip (PATCH ploader-01) | missing entries currently pay readiness+attach before the instant `access` failure (S09 before S05/S08); mark dead for the invocation | ~2 s per missing entry per invocation (~8 s in a 9-entry ini; 0 in the 2-plugin ini) + cleaner toasts |
| R5 | Hoist readiness wait to once per title (PATCH ploader-02) | readiness is charged per plugin (`main.c:784` in the per-plugin loop), min ~1.6 s each (S08) | ~1.6 s × (plugins−1) per invocation: ~11 s at 8 plugins, **~1.6 s at 2 plugins** |
| R6 | Quiescent exit (PATCH ploader-04) | after the game pid is processed once, the loop idles to the 120 s cap doing nothing (S03, S04) | user-invisible (toasts only); hygiene |
| R7 | NOT candidates: per-boot transfer, per-file timeout | ploader has no network code (S02, S16); missing files fail instantly (S09) | n/a — removing them from the hypothesis space is itself the deliverable |

Also noted, no patch proposed: `wait_stable`'s mode-2 parameters (1000 ms stable +
600 ms settle, `main.c:475`) could be trimmed for ~1.1 s/plugin, but the stability gate
protects against injecting into a still-morphing module space; not worth the risk.

## 4. Launch-10 projection under the 2-plugin config (sha 1216aab6…, supplier_composition=2, path_test=2)

Transfer: **0 at boot** — both PRXs are already staged on /data (staging law runs over
FTP 2121 / manager 8084 before launch, L13); ploader reads local files only (S16).

| Stage | Cost model | Estimate |
|---|---|---|
| ploader detects game pid | 50 ms poll + sysctl scan (`main.c:755-764, 825`) | ≤ ~0.1 s |
| composition: readiness (mode 2) | ≥1.6 s, ≤25 s if module space still settling (S06-S08) | 1.6-3 s |
| composition: attach | 1 attempt typical (`main.c:176-190`) | 0.3-2 s |
| composition: 30 MB read from /data by the kernel loader | storage-speed bound | ~0.5-3 s |
| composition: module_start (VMMAP walk 10,019 + lease registration + refusal) | **C_comp — UNMEASURED**; memory-bound in principle, must be verified via toast `dt` or step log | unknown, `pt_call` cap 30 s |
| path_test: readiness + attach + module_start | ≥1.6 s + attach; module_start writes 2 log lines | ~2-3 s |
| tail | both OK → `injected=1` → "Finished"; no idle tail (no FAILs) | ~0 |

**Projected mod-attributable boot wait: ~5-10 s + C_comp (≤30 s unless C_comp exceeds
the cap).** Against launch-9's measured ~6 min entry occupancy + unmeasured composition,
this is roughly a **10-70× reduction** on the mod-attributable portion. The game's own
boot-to-menu is unchanged and dominant in what remains.

**Residual risks (launch-10):**
1. C_comp > 30 s → composition FAILed despite working; with 2 plugins the invocation
   still finishes quickly (path_test proceeds; no storm), but every subsequent
   invocation re-pays it. Mitigation: R3 measurement first, then decide.
2. Entry accidentally re-enabled → the ~6 min cost returns (R2 is the durable fix).
3. If ploader is invoked while the game is already running, the game pid is in the
   initial `known` seed and is never injected (`main.c:739-740, 755-764`) — not a
   latency issue, but a footgun for manual retries.
