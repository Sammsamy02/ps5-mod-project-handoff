# BOOT-PROTOCOL.md — warm/cold boot discrimination for the loading regression

Worker: worker-boot-protocol · 2026-10-05 · Design + read-only console baseline ONLY.
This doc stages nothing, writes nothing to the console, launches nothing, and makes
no game-verdict claims. All console contact was read-only (FTP LIST/RETR, HTTP GET).
Raw probe evidence: `probe/` (this directory).

Companion documents (NOT superseded by this one):
- rc capture: `outputs/2026-10-04-zcode-team/worker-loader-instrument/STAGE-CARD.md`
  (diagnostic ploader, decision table §4) — **owns the rc → action mapping**
- launch law: `outputs/2026-10-04-zcode-team/worker-test-matrix/TEST-MATRIX.md`
  (LAUNCH-A rc observation; LAUNCH-B2 cold-boot repro §3.2; single-variable law §0)
- ini-order/slot-contrast test: TEST-MATRIX LAUNCH-B1 (being prepared separately)

---

## 0. Why a boot protocol exists at all

The loading regression: `sceKernelLoadStartModule` rejects the ~30.8 MB composition
module in ~85 ms while `modbridge_path_test.prx` loads OK in the same launch.
All disk-state variables (game update, slot bytes, ini, loadlist, settings) have been
eliminated on the disk side. The two remaining cause classes are **kernel-state,
per-boot properties**:

1. **Boot-state import-binding / provider-residency race** — module accept/reject
   depends on kernel or provider state established earlier in THIS boot.
2. **mmap/copyin failure of the ~30.8 MB image under memory pressure** — kernel
   memory state accumulated over the boot refuses the mapping.

Both classes predict the SAME warm-relaunch behavior (fail again) and differ only on
a **cold boot**. A console reboot is a user action, costs zero disk changes, and
resets kernel state. It is the cheapest complementary discriminator alongside the
diagnostic loader's rc capture. This document makes it rigorous: what counts as cold,
how to PROVE which boot a launch ran on, and what each outcome means.

---

## 1. Current console facts — read-only baseline taken 2026-10-05 ~20:52–21:05 host-local

### 1.1 Reachability and exposed services (all confirmed live this probe)

| probe | command | result |
|---|---|---|
| pldmgr manager | `GET http://ps5-host-not-configured.invalid:8084/list_payloads` | **200**, 28,486 B catalog (`probe/list_payloads.json`); catalog includes `kstuff.elf`, `kstuff-lite_v1.11.elf`, `etaHEN-13.60.elf`, `OnionHEN.elf` as armable payloads |
| ftpsrv | LIST `ftp://ps5-host-not-configured.invalid:2121/` | OK, 1,572 B root listing (`probe/ftp-root-list.txt`) |
| websrv | not probed beyond presence in catalog/autoload | autoload-listed; not contacted further (out of bounded scope) |

pldmgr config + autoload (`probe/console-autoload.txt`, `probe/console-pldmgr_config.txt`,
mtimes Oct 03 15:29 / Oct 04 18:44 FTP-tz): `AUTOLOAD_ENABLED=1`, `AUTOLOAD_DELAY=5`;
autoload list = `kstuff-lite_v1.11.elf, nanoDNS_0.4.elf, ftpsrv-ps5.elf, elfldr_v0.26.elf,
PKG-Manager_v1.4.1.elf, ps5upload_v5.41.0.elf, websrv_v0.34.elf, shadowmountplus.elf,
OnionHEN.elf`. **etaHEN is NOT in the autoload list; OnionHEN is.**

### 1.2 /proc probes — THE NEGATIVE RESULT (documented exactly, per task)

ftpsrv does **not** expose procfs. Every /proc attempt is refused with FTP 550
("Server denied you to change to the given directory", curl exit 9):

| attempt | exact outcome |
|---|---|
| LIST `/proc/` | curl (9) 550 — denied (`probe/` run log, this doc §1.2 note) |
| RETR `/proc/uptime` | curl (9) 550 — denied, 0 bytes |
| RETR `/proc/meminfo` | curl (9) 550 — denied, 0 bytes |
| RETR `/proc/1/status` (`--ftp-method singlecwd`) | 0 bytes (denied) |

Consequences, stated plainly:
- **MemFree-at-idle is UNAVAILABLE on this setup.** The task's hoped-for
  "MemFree low at idle ⇒ cause 2 gains weight" evidence **cannot be obtained through
  ftpsrv**. Memory-pressure reasoning stays indirect (uptime hours, launch counts, §1.4)
  unless the controller later opens a read-only channel that can see procfs. No such
  channel exists in the permitted toolset (FTP RETR/LIST, HTTP GET only).
- ftpsrv/websrv process start times are likewise **not listable** (no /proc). Boot-time
  evidence was recovered by other means (§1.4).

### 1.3 kstuff / etaHEN presence

| evidence | file / source | reading |
|---|---|---|
| etaHEN 13.60-port data dir | `/data/etaHEN/` (LIST, mtime Oct 03 04:05) | present; contains `bootstrap-1360-port.log`, `startup-1360-port.log`, `etaHEN_util_daemon.log`, `config-1360-port.ini` (all RETR'd into `probe/etahen-*`) |
| etaHEN version (self-reported) | startup log line `{"stage":"Decrypted Version: etaHEN Beta 2.5 (13.600.007 )"}` | etaHEN Beta 2.5 for fw 13.600.007 |
| kstuff residency per boot (HISTORICAL, Oct 03 window) | startup log generations: `kstuff state=1 readable=1 native=0xffffff80… compat=0xffffff80… crypt=deb7/deb7` → `resident kstuff found` | kstuff kernel payload resident at every recorded bootstrap generation; the `native=`/`compat=` kernel addresses DIFFER between generations (14eb3ef0 / 15cb3ef0 / 1ae03ef0 / 1307bef0 / 1b4b7ef0 / 1a4c7ef0 / 0e8d7ef0) — **kernel-side layout is per-boot**; no in-tree kstuff unload/reload path exists anywhere |
| etaHEN re-run is a NO-OP | startup log lines `resident etaHEN detected: other pid=X` | re-triggering the etaHEN payload inside a boot does NOT re-init or reset kernel state — **re-running HEN payloads is not a cold-boot equivalent** |
| etaHEN on CURRENT boot | last `startup-1360-port.log` append is FTP-mtime Oct 03 04:06; no bootstrap generation since | etaHEN was NOT auto-spawned on the current boot (autoload carries OnionHEN instead). Current-boot HEN-userland state is not directly observable read-only |
| kstuff-family liveness on CURRENT boot | ftpsrv (2121) + pldmgr (8084) answering NOW | these are injected ELF payloads (autoload list); their liveness shows the kernel payload infrastructure IS resident this boot |
| kstuff config marker | `/data/.kstuff_noautomount` (0 B, Oct 03 15:30) | present; kstuff automount-disable marker |

### 1.4 Boot identity and uptime — recovered WITHOUT procfs (the key finding)

The composition runtime publishes a per-session status file. Current read:

```
RETR /data/modbridge/roots/PPSA33286/runtime-status.txt   (68 B)
modforge-runtime-status	1
session	934	95140156
state	1	activated	0
```

- pid 700 (the 18:40 OK launch): `session	700	86714471` at wall
  `2026-10-05 18:40:10` (self-stamped) → **24 h 05 m 14 s uptime** at the last OK.
  (source: `outputs/2026-10-04-zcode-controller/launch10-evidence/pid700-fullfix/EVIDENCE.md`,
  which names the third column "status monotonic ms")
- pid 934 (launch at probe time): `session	934	95140156` → **26 h 25 m 40 s**.
- Δ = 95,140,156 − 86,714,471 = **8,425,685 ms = 2 h 20 m 26 s**;
  18:40:10 + 2:20:26 = **21:00:36** — exactly the self-stamp of the fresh
  `/data/plugins/UE4SS.log` (1,579 B) captured during this probe
  (`probe/console-UE4SS.log`). Three-way agreement, same monotonic epoch.

Derived boot facts (cross-validated on three independent clocks):

| fact | value | basis |
|---|---|---|
| boot origin | **≈ 2026-10-04 18:34:55 wall** | 18:40:10 − 86,714,471 ms; corroborated by `/dev/random` creation mtime Oct 04 23:34 FTP-tz (= 18:34 wall) and `pldmgr/` dir mtime Oct 04 18:44 (payload arming ~9 min post-boot) |
| continuous uptime at probe | **≥ 26.4 h** | monotonic epoch continuous from pid700 through pid934 |
| reboots between the 18:40 OK and now | **ZERO — proven** | a reboot resets the epoch; 95,140,156 ms after a reset would be impossible, and the wall-clock delta (2:20:26) matches the monotonic delta exactly |
| cold-boot discriminator status | **UNRUN** | every observation of this regression (18:40 OK, all FAIL launches incl. Launch 5, the 21:00 launch) happened on ONE ≥26 h boot |

Clock caveat (three clocks in play; never subtract across clocks):
(a) console wall clock ≈ host local (log self-stamps); (b) FTP LIST mtimes =
console wall **+5 h** (verified: UE4SS.log self-stamp 21:00:36 ↔ FTP mtime Oct 06 02:00);
(c) per-boot monotonic clock (runtime-status ms, PATH_TEST `monotonic-ns`) — the only
tz-free clock, and the one that defines boot identity.

### 1.5 Live state drift observed DURING the probe window (recorded raw — controller owns this)

The console was mid-flight in a controller-driven cycle while this probe ran. Facts as
observed (FTP mtimes in +5h clock):

- `/data/plugins/modbridge_supplier_composition.prx` now **30,837,400 B**, mtime Oct 06 01:57 —
  **NOT** the TEST-MATRIX §1 pin `34eafe5d` / 30,849,624 B.
- Three rollback files present: `.prx.20261005-200539.bak` (30,849,624 B),
  `.prx.20261005-200733.bak` (30,849,624 B), `.prx.20261005-diagswap.bak` (30,849,624 B,
  mtime 01:56) — a "diagswap" staging happened minutes before this probe.
- Fresh game sandbox `PPSA33286_000` (mtime 01:58) + `pfsmnt` (01:57);
  fresh `/data/plugins/UE4SS.log` (1,579 B) self-stamped `2026-10-05 21:00:36`
  showing UE4SS core init + `[Seeds] slot 0 g_uobject_array bound …` /
  `slot 3 gmalloc bound …` lines, and **no PATH_TEST line yet** (log captured mid-run).
- `runtime-status.txt` says `session 934 … state 1 activated`.
- `/data/plugins/ploader.ini` RETR → sha256
  `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0` — **exactly the
  frozen 2-plugin pin**, observed 440 B on console (note: STAGE-CARD §2.6 labels the
  same hash "263 B"; the hash is the law-carrying identity, the byte-count label
  disagrees with the console listing — flag for reconciliation, do not act on it here).

**No verdicts are drawn from the in-flight launch.** This snapshot is stale by the time
it is read; every protocol step below therefore begins from the TEST-MATRIX pre-flight
law (RETR-re-pin everything; any drift → STOP + reconcile, never a silent re-pin).

### 1.6 Warm-relaunch history on this boot (why warm evidence alone is exhausted)

Same-boot game/launch activity visible today: pid 355/370 runs (earlier epoch, Oct 4),
pid 273 (launch-9), pid 700 OK 18:40, the Launch 5 FAIL and subsequent FAIL attempts,
pid 934 at 21:00 — plus `NPXS40093_000..044` sandbox instance dirs (a system/UI app,
identity unverified) with strictly increasing mtimes spanning Oct 04 19:12 → Oct 05
20:57 wall. Warm relaunches have been done many times and fail. **The kernel state
every one of them hit is the same ≥26 h boot.** The cold-boot outcome is genuinely
unknown — it has never been tried on this regression.

---

## 2. The warm/cold protocol (user actions; zero disk changes)

### 2.1 Definitions on THIS setup

| action | what it is | kernel-state effect | evidence basis |
|---|---|---|---|
| **WARM** | close the game fully (user confirms the title is closed on screen), then relaunch from the UI | NONE on the kernel boot state | monotonic epoch continuity across pid 273 → 700 → 934 (§1.4, §1.6); every warm relaunch tonight hit the same boot |
| **COLD** | full console power-off, then power-on (the console's Restart/Power Off→On, user's choice of the two; both create a new bootstrap generation) | full kernel reset: new monotonic epoch, new kstuff generation, fresh kernel VMAs/pools | etaHEN startup log: new bootstrap generation with NEW kstuff `native=` addresses per generation; per-boot kernel layout proven §1.3 |
| NOT cold: re-running etaHEN/kstuff-lite/OnionHEN payloads | payload re-trigger | NO documented reset; etaHEN re-run is a logged NO-OP when resident; kstuff-lite re-inject behavior on a resident kernel payload is undocumented | §1.3; using it would also be a second variable in one launch (forbidden) |
| NOT cold: pldmgr app relaunch | re-fires autoload after `AUTOLOAD_DELAY=5` | same objection as above — autoloading a kernel payload into a live kernel is not a kernel reset | §1.1 |
| NOT provably cold: rest-mode wake | unknown whether the monotonic/kernel state resets | UNVERIFIED either way on this setup | if rest mode is involved, the boot-identity receipt (§2.3) decides — a "cold" is only credited if the monotonic epoch demonstrably reset |

**Only a real power cycle counts as cold.** Anything else muddies the single-variable
law (TEST-MATRIX §0).

### 2.2 The one launch that must precede the cold boot: LAUNCH-A (rc capture)

Per STAGE-CARD (worker-loader-instrument) and TEST-MATRIX LAUNCH-A: stage the
diagnostic ploader (`279c1453…` or `009445ab…`, never mix), ONE launch, capture the
popup rc verbatim. The rc names the kernel's rejection class on the CURRENT (warm,
≥26 h) boot — without it, a cold-boot-OK outcome can never be attributed between
cause class 1 and 2. Sequence discipline is STAGE-CARD's; this doc only adds the boot
bookkeeping around it.

### 2.3 Boot-identity receipt (mandatory bookkeeping, free, read-only)

Every launch in this protocol records the boot it ran on, BEFORE and AFTER the launch:

1. `RETR /data/modbridge/roots/PPSA33286/runtime-status.txt` → record `session <pid> <ms>`.
   - The `<ms>` column IS the boot's uptime at that session: ms ≈ 9.5×10⁷ = the old
     boot; ms in the 10⁴–10⁶ range = a fresh boot.
   - Known limit: this file is written by the composition runtime (LATEST-STATE-ONLY).
     On a launch where the composition FAILS to load it may go stale — the fallback
     boot-identity source is the control module's `PATH_TEST: … writable at <ns>
     monotonic-ns` line in `/data/plugins/UE4SS.log` (path_test loads even when the
     composition fails; TEST-MATRIX §2.2 already harvests it every launch).
2. For a cold boot specifically: the post-reboot receipt MUST show a monotonic value
   orders of magnitude below the pre-reboot one (fresh boot = minutes, not ~10⁸ ms),
   and a wall-clock gap consistent with a power cycle. **No epoch reset in the receipt
   ⇒ the reboot did not happen or did not take effect ⇒ the launch result is not a
   cold result.** (Same-check law already in TEST-MATRIX §2.1 step 7 and §4 item 6,
   now with a concrete instrument.)

### 2.4 Outcome semantics (the discriminator table)

| cold-boot outcome | meaning | consequence |
|---|---|---|
| **cold boot, then OK** — composition loads after a verified power cycle | kernel-state **drift**, transient class: some per-boot accumulated state (import/provider residency race OR memory pressure) was the trigger; disk bytes exonerated (again) | cause classes 1 and 2 both remain plausible — attribute with the rc from LAUNCH-A: memory-class rc + cold-boot-OK ⇒ **cause 2** (pressure accumulates over boot uptime); import-class rc + cold-boot-OK ⇒ **cause 1** (boot-state race that eventually wedges); no rc captured ⇒ treat as unresolved transient, rc capture remains the next spend |
| **cold boot, still FAIL (same ~85 ms signature)** | **deterministic per-boot condition** present even on a fresh kernel — deepens the mystery; accumulated-drift classes 1-and-2-as-transient are dead | rc capture becomes MORE essential, not less: import/format/auth-class rc routes to TEST-MATRIX LAUNCH-B1/B3 as designed; a **memory-class rc even on a fresh boot** ⇒ structural memory ceiling for a 30.8 MB image ⇒ host-side truncated-composition/split-module build decision (a build-worker question, outside this protocol's and the launch-matrix's authority) |
| cold boot unverifiable (no epoch reset in receipt) | launch result is UNINTERPRETABLE for this discriminator | do not count the launch toward either row; re-do the power cycle with receipts |

Warm relaunches add no new information by themselves (proven §1.6: many warm attempts,
one boot) and are no longer part of the protocol except as the natural re-entry state
after staging work.

---

## 3. Recommended combined sequence (one variable per launch)

Ordered; each step keys on the previous step's observed result. Zero disk changes are
made by steps marked (user).

1. **Reconcile first (controller).** The §1.5 drift (diagswap + fresh pid-934 session)
   must be reconciled and re-pinned under the TEST-MATRIX pre-flight law before any
   protocol launch. This doc's snapshot is a baseline, not a pin source.
2. **LAUNCH-A — stage the diagnostic loader, ONE launch** (controller stages per
   STAGE-CARD; user launches). Single question: what rc does the kernel return?
   Record the boot-identity receipt before/after (§2.3). This rc is captured on the
   CURRENT warm boot deliberately — that is the state in which the regression lives.
3. **Classify the rc** with STAGE-CARD §4 (authoritative table). Then:
   - **memory-class rc** (`0x8002000C` ENOMEM / ENOSPC / EAGAIN family) →
     **Step 4 (cold boot) is the single next launch**: identical bytes, unchanged ini,
     user power-cycles, boot-identity receipt proves the new epoch, ONE relaunch.
     (This is TEST-MATRIX LAUNCH-B2 with this doc's receipts around it.)
   - **import-class rc** (`0x80020064` ESTART / unresolved strong-import family) →
     the ini-order/slot-contrast test (LAUNCH-B1, prepared separately) is the single
     next launch; the cold boot is deferred and spent only if B1 is inconclusive.
   - **other classes** → STAGE-CARD §4 rows (format/auth: B3; `-1`: no launch, reconcile).
4. **Cold boot test (user, zero writes) — default next single question whenever the rc
   is inconclusive or the selected B-launch didn't close the question.** Procedure:
   a. record pre-reboot receipt (runtime-status `session <pid> ≈9.5×10⁷ ms` or latest);
   b. user performs full power-off → power-on (NOT rest-mode-only, NOT payload re-runs);
   c. verify reachability (G0 `list_payloads` 200), verify pins (RETR law);
   d. record post-reboot receipt — monotonic MUST be fresh-small (§2.3), else STOP;
   e. ONE launch, unchanged bytes (diagnostic ploader stays installed so any FAIL
      still names its rc — if LAUNCH-A was rolled back, its rollback must first be
      undone as its own verified step, TEST-MATRIX §3.2);
   f. read the outcome in §2.4's table.
5. **Never two variables in one launch** — the cold boot ITSELF is the only variable of
   its launch (zero staged bytes); the diagnostic ploader is already-installed state
   that must be identical across the compared launches (TEST-MATRIX §3.2 wording).

Launch-count discipline: unchanged from TEST-MATRIX §6 — LAUNCH-A plus ONE
outcome-keyed follow-up; the cold-boot launch IS that follow-up when memory-class or
inconclusive. Rest-mode-only wakes do not qualify as the cold step unless the §2.3
receipt shows a real epoch reset.

---

## 4. Evidence index (all reads this worker made; raw files in `probe/`)

| artifact | file |
|---|---|
| pldmgr catalog (200, 28,486 B) | `probe/list_payloads.json` |
| FTP root listing | `probe/ftp-root-list.txt` |
| /proc denial (LIST + RETR ×3, FTP 550 each) | `probe/proc-denials.log` (curl wrote no body on denial; exact commands + outcomes recorded there) |
| `/data` listing (etaHEN/, OnionHEN/, .kstuff_noautomount) | `probe/ftp-data-list.txt` |
| `/data/etaHEN/` listing + logs (bootstrap/startup/util-daemon) | `probe/ftp-etahen-list.txt`, `probe/etahen-bootstrap-1360-port.log`, `probe/etahen-startup-1360-port.log`, `probe/etahen-etaHEN_util_daemon.log` |
| `/mnt`, `/mnt/sandbox` listings (PPSA33286_000, NPXS40093_000..044) | `probe/ftp-mnt-list.txt`, `probe/ftp-sandbox-list.txt` |
| `/dev` listing (boot-corroborating node mtimes) | `probe/ftp-dev-list.txt` |
| pldmgr dir + autoload + config | `probe/ftp-pldmgr-list.txt`, `probe/console-autoload.txt`, `probe/console-pldmgr_config.txt` |
| `/data/plugins` listing (slot bytes, .baks, ini) | `probe/ftp-plugins-list.txt` |
| current `ploader.ini` (sha `1216aab6…` verified) | `probe/console-ploader.ini` |
| current `UE4SS.log` (1,579 B, self-stamp 21:00:36) | `probe/console-UE4SS.log` |
| current `runtime-status.txt` (session 934 / 95140156 ms) | `probe/console-runtime-status.txt` |

Cross-referenced (host-retained, not re-fetched): pid700 `session 700 86714471` at
wall 18:40:10 — `outputs/2026-10-04-zcode-controller/launch10-evidence/pid700-fullfix/EVIDENCE.md`;
Launch 5 transcription — `outputs/2026-10-04-zcode-controller/incident-loading-regression/POPUP-TRANSCRIPTIONS.md`.

## 5. Honest limits

- **procfs is closed on this setup**: no MemFree, no /proc uptime, no process start
  times. The task's hoped-for MemFree-at-idle cause-2 evidence does not exist on this
  channel; all uptime/pressure reasoning here is monotonic-clock-derived and indirect.
- Boot identity rests on (a) runtime-status monotonic-ms, (b) UE4SS.log self-stamps,
  (c) FTP mtime deltas — three consistent clocks, but (a) is written by the
  composition runtime and can go stale on FAIL-only launches (fallback: PATH_TEST
  monotonic-ns; §2.3). The monotonic↔wall cross-check was performed once, at probe
  time; repeat it at every protocol step (§2.3) rather than trusting this snapshot.
- The NPXS40093 instance-counter "no reset across the day" observation is a HINT
  (boot-counter semantics unverified), used only as corroboration, never as proof.
- etaHEN log generations predate the current boot; kstuff residency on the CURRENT
  boot is inferred from payload-service liveness (ftpsrv/pldmgr answering), not
  directly observed.
- The §1.5 mid-flight observations (diagswap, pid-934 activation, Seeds lines) are
  recorded raw with timestamps; NO verdicts about that launch are made or implied.
- The "263 B vs 440 B" ini byte-count label disagreement between STAGE-CARD §2.6 and
  the console listing is flagged, not reconciled, by this worker (hash-level identity
  is what the staging law gates on).
- No game-state, load-outcome, or quality claims of any kind are made by this
  document. The cold-boot discriminator's outcome is genuinely unknown and unrun.
