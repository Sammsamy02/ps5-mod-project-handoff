# TEST-MATRIX.md — minimal launch sequence for the loading regression

Worker: worker-test-matrix, 2026-10-05. **Design only.** This doc stages nothing,
writes nothing to the console, launches nothing, and makes no game claims. The
controller executes each launch only after user approval, and only one launch at
a time, each keyed on the previous launch's observed result.

The regression being instrumented: identical console bytes that loaded at
18:40 (pid 700, `runtime_start_ok`) now fail — `sceKernelLoadStartModule`
rejects the ~30MB composition module in ~85 ms, while `modbridge_path_test.prx`
loads in the same launch ("The second plugin said OK"). The missing observation
is the kernel's rejection code. LAUNCH-A exists to capture exactly that code;
each LAUNCH-B variant exists to split one hypothesis class per launch.

---

## 0. The law every launch obeys

1. **Single question first.** Before any staging, the run log records ONE
   sentence: "This launch's single question is: …". A launch that cannot state
   its question does not happen.
2. **Single variable.** Exactly one staged byte-state differs from the previous
   launch's pinned state. Observation-only fetches (RETR) are unlimited and free.
3. **No re-staging unchanged files.** If the bytes on console already hash-equal
   the intended bytes, the STOR does not happen (record "already-pinned, skipped").
4. **Pre-image before overwrite.** Every overwritten target gets a RETR'd
   pre-image (hash recorded) plus an on-console `.bak` BEFORE any write.
5. **Readback or it did not happen.** Every write is verified by a fresh-connection
   RETR + hash; the FSELF readback class (decrypted inner ELF + ftpsrv SHT-strip
   artifacts, per CONSOLE-LOG items 11–12) is compared under that law and never
   widened on a live refusal.
6. **No blind retry.** A failed staging is never replayed unchanged; negative
   evidence is evidence. Any hash drift at pre-flight halts the launch for
   reconciliation — nothing is "fixed" silently to make a launch happen.
7. **Transcription preserved raw.** Popup text is preserved exactly as the user
   transcribes it; interpretation happens only in a separate analysis note.

## NEVER list (hard)

- NEVER a two-variable launch.
- NEVER re-stage bytes that are already verified on console.
- NEVER launch without the single question recorded first.
- NEVER launch with any pre-flight hash drifted from its pin.
- NEVER arm the ploader while a staged change is pending verification.
- NEVER interpret a popup rc before the verbatim transcription is archived.
- NEVER stage the diagnostic ploader and a new composition/ini in one launch.

---

## 1. Current pinned state (baseline all launches measure against)

Console facts (all prior packets): `FTP=ftp://ps5-host-not-configured.invalid:2121` (anonymous),
`HTTP=http://ps5-host-not-configured.invalid:8084` (pldmgr arming only). Game CLOSED, ploader NOT
armed, before every launch.

| console target | pinned bytes | sha256 (prefix) | source of pin |
|---|---|---|---|
| `/data/plugins/ploader.ini` | 440 | `1216aab6` | staging run `20261005-200730-stage-c` ini pre==post |
| `/data/plugins/modbridge_supplier_composition.prx` | 30,849,624 | `34eafe5d` | `l10-composition.prx.pre.sha256` (FTP readback of the restored slot) |
| ploader payload `/data/pldmgr/payloads/ploader/ploader.elf` | — | `edf61245` | incident Launch-5 restore record; no local byte-match has ever been established (precedent record) |
| `$R/ue4ss/loadlist.txt` | — | `c656740a` | incident Launch-5 restore record |
| UE4SS-settings.ini | — | `b980cea7` | incident Launch-5 restore record |

Current ini content (pinned `1216aab6`, authoritative only via Step-0 RETR):

```ini
[PPSA33286]
/data/plugins/modbridge_exec_write_probe_armed.prx = 0
/data/plugins/modbridge_fc1pool.prx = 0
/data/plugins/modbridge_supplier_composition.prx = 2
/data/plugins/modbridge_qh_driver.prx = 0
/data/plugins/modbridge_runtime_aligned_observer.prx = 0
/data/plugins/modbridge_runtime_aligned_entry.prx = 0
/data/plugins/modbridge_path_test.prx = 2
/data/plugins/modbridge_ingame_facts.prx = 0
/data/plugins/modbridge_carrier.prx = 0
```

Local artifact shelf (all hash-verified on disk this session):

| artifact | path (under workspace) | bytes | sha256 prefix | magic |
|---|---|---|---|---|
| LAUNCH-A ploader (diagnostic rc build) | `outputs/2026-10-04-zcode-team/worker-loader-instrument/build-diagnostic/ploader.elf` | — | `279c1453` | bare ELF |
| secondary ploader (filelog build; see §5) | `outputs/2026-10-04-zcode-team/worker-loader-instrument/build-filelog/ploader.elf` | — | `9dccfef1` | bare ELF |
| 18:40-OK build (receipt-pinned) | `outputs/2026-10-04-zcode-controller/launch11-combined/prx-candidate-l11-1-aside-1791243375/modbridge_supplier_composition.prx` | 30,849,159 | `07ac3e1e` | FSELF `4f153d1d` |
| l11-1 current top-level (FSELF variant) | `outputs/2026-10-04-zcode-controller/launch11-combined/prx-candidate-l11-1/modbridge_supplier_composition.prx` | 30,851,495 | `d6922807` | FSELF `4f153d1d` |
| l11-1/l11-2 bare-sdk (identical builds) | `outputs/2026-10-04-zcode-controller/launch11-combined/prx-candidate-l11-1/bare-sdk/modbridge_supplier_composition.prx` | 30,839,416 | `8ede717f` | bare ELF `7f454c46` |
| fc1pool probe module | `outputs/2026-10-04-zcode-team/worker-fc1-validator-probe/modbridge_fc1pool.prx` | 22,733 | `d2431bf8` | FSELF |
| path_test control module | `outputs/2026-10-04-zcode-team/worker-minimal-path-test/modbridge_path_test.prx` | 8,365 | `b24f67e5` | (on console, currently loading OK) |

Open provenance item (must be reconciled, not assumed): the receipt-pinned
18:40-OK build is `07ac3e1e` (EVIDENCE.md, pid700-fullfix), while the Launch-5
restored slot read back `34eafe5d` / 30,849,624 B. These are not the same local
file, and an FSELF slot readback is the decrypted-inner+SHT-strip view, so the
reconciliation must follow the readback law (§0 rule 5) — the pre-flight slot
RETR in every launch doubles as the drift check for this item.

---

## 2. LAUNCH-A — the rc observation (primary, after diagnostic ploader staged)

**Single question: "What rc does the kernel return when the composition module
is loaded under the current console state?"**

**Variable (exactly one): the ploader payload** at
`/data/pldmgr/payloads/ploader/ploader.elf` is replaced by the diagnostic
build `279c1453…` (worker-loader-instrument; FAIL line now carries
`rc=0x%08x` of `inject_prx`'s return — the collected `sceKernelLoadStartModule`
result, or `-1` for that function's own early failures). Everything else —
slot bytes, ini, loadlist, settings — is frozen at the §1 pins.

### 2.1 Pre-flight checklist (all RETR-only, zero writes; any failure = NO-GO)

1. Game CLOSED; ploader not armed; `GET $HTTP/list_payloads` answers (G0).
2. Slot RETR → sha == `34eafe5d` (30,849,624 B). Drift → STOP, reconcile first.
3. ini RETR → sha == `1216aab6`, byte-identical to the §1 listing.
4. ploader payload RETR → sha == `edf61245` (the rollback pre-image; STOR it
   to `/data/pldmgr/payloads/ploader/ploader.elf.$TAG.bak` too).
5. loadlist RETR → sha == `c656740a`; settings RETR → sha == `b980cea7`
   (observation-only pins; drift halts).
6. Local diagnostic ploader receipt complete: instrument worker's report pins
   `279c1453…`, its base source, and the exact FAIL format string
   `[PPSA33286] FAIL (<ms>ms, rc=0x%08x): <file>`; **if the instrument's
   rc→action decision table has landed, it supersedes §2.3 below.**
7. Record console boot/session identity (fresh boot or continuous with the
   failing launches) — this answers §3.2's cold-boot conditionality for free.

### 2.2 Expected observations

Popup sequence (transcribed verbatim by the user, all lines, in order):

| pattern | meaning |
|---|---|
| `[ploader] Injector started` | diagnostic ploader alive |
| `[PPSA33286] Injecting 2 plugin(s)` | ini parsed unchanged (composition + path_test) |
| `[PPSA33286] OK (<ms>ms): modbridge_path_test.prx` | control: injection pathway healthy in the same launch |
| `[PPSA33286] FAIL (<ms>ms, rc=0x%08x): modbridge_supplier_composition.prx` | **THE observation** |
| `[ploader] Finished` | clean end |

Baseline timings: composition FAIL ≈ 85 ms (both failing launches); the measured
predecessor metadata refusal (ESDKVERSION `0x80020063`) was ≈ 75 ms. A materially
different latency is itself evidence about which gate refuses.

Free rider observations (fetch after the launch, no state changes):
- fresh `/data/plugins/UE4SS.log` fetch: a NEW `PATH_TEST:` line expected (control);
- `/data/modbridge-supplier-diag.jsonl` (cumulative sink): record the record
  count before/after. **A new pid epoch appearing after a FAIL popup is a
  major finding** — it would mean `module_start` actually ran (load accepted,
  `rc<=0` came from cleanup), contradicting a kernel rejection;
- `/data/townfall_supplier_composition.log`: expected absent/unchanged on refusal.

Negative-space expectations: no new sdiag epoch, no composition log lines, no
`bridge_spanpub_*`/conversion events. Absence is the expected evidence of a
pre-`module_start` refusal.

### 2.3 rc → next-action decision table (DRAFT from in-tree evidence)

Supersede with worker-loader-instrument's table the moment it lands. Codes from
the retained `ploader-rc-80020063-1/public-orbis-error.h`. Honest limit: the
kernel's exact code for an unresolvable strong import is established nowhere in
the retained sources (PLOADER-IMPORT-ANALYSIS §6.4), so the import class is a
residual bucket, not a mapped value.

| rc (popup) | class | next action |
|---|---|---|
| `rc=0xffffffff` (`-1`) | ploader-side early failure (access/NID-resolve/mmap/copyin), NOT a kernel rejection | No LAUNCH-B. Re-verify slot placement/readability from the game's context; reconcile with the instrument worker; only then re-plan |
| `rc=0x00000000` | API returned 0 (FAIL predicate is `rc>0`); or cleanup-only FAIL | Check for a new sdiag epoch + composition log first; treat as "load accepted, downstream refused" — route to post-promotion-runtime triage T2/T5, not the B matrix |
| `rc>0` (nonzero handle) | load actually succeeded; FAIL string was the old build's cleanup fields | Harvest logs immediately; module may be alive — this contradicts the rejection premise |
| `rc=0x80020063` ESDKVERSION | metadata/SDK-stamp refusal (the one measured precedent, 75 ms) | No console experiment. Host-side static-check reconciliation of the staged bytes; re-stage only after a verified local cause |
| `rc=0x8002000C` ENOMEM / `0x8002001C` ENOSPC / `0x80020023` EAGAIN | memory/resource class | **LAUNCH-B2** (cold-boot repro with unchanged bytes) |
| `rc=0x80020008` ENOEXEC / `0x80020016` EINVAL / `0x8002004F` EFTYPE | format class | **LAUNCH-B3** (FSELF-form variant) |
| `rc=0x80020050` EAUTH / `0x80020060` EICV / `0x8002005D` ENOTCAPABLE / `0x80020001` EPERM / `0x8002000D` EACCES | auth/trust class | **LAUNCH-B3**, with the `07ac3e1e` re-stage as the preferred first probe (see §3.3 note) |
| `rc=0x80020002` ENOENT | file-not-found from the game process | Placement problem contradicting the FTP readback — no B launch; reconcile placement evidence |
| `rc=0x80020064` ESTART / any other `0x800200xx` | unresolved — plausibly the import/dependency class (no in-tree mapping) | **LAUNCH-B1** (ini-slot contrast experiment) |
| no rc line at all (`Attach failed` / `case died` / timeout lines instead) | ploader attach lost — the launch yielded no rc | Record as a new observation class; do not count as a kernel rejection; reconcile with the instrument worker before any relaunch |

### 2.4 Rollback (game CLOSED, ploader not armed)

STOR the saved pre-image back over `/data/pldmgr/payloads/ploader/ploader.elf`,
fresh-connection RETR → sha == `edf61245`. Nothing else was touched; verify the
§1 pins once to close the run. The console `.bak` is the second artifact.

---

## 3. LAUNCH-B — exactly ONE, chosen by LAUNCH-A's rc class

Each candidate is fully designed; the controller stages only the one its
outcome selects. If LAUNCH-A's rc lands in the "no LAUNCH-B" rows of §2.3,
no B launch happens at all.

### 3.1 LAUNCH-B1 — ini-slot contrast (import/dependency class)

**Single question: "Does an independent small modbridge module load via its own
ini slot under the current console state?"** If yes, the kernel pathway and the
FSELF class are healthy and the failure is composition-specific (imports/format
of that one module); if it fails with the same rc, the failure is systemic
(environment/state), not composition-specific.

Variable (exactly one state = the ini, TWO one-byte edits inside it, per the
documented freeze law):

- `/data/plugins/modbridge_fc1pool.prx = 0` → `= 2` (one byte `0`→`2`)
- `/data/plugins/modbridge_supplier_composition.prx = 2` → `= 0` (one byte `2`→`0`)

`fc1pool` is receipt-built (`fc1pool1-78508a2e2940`), 22,733 B FSELF,
logs `FC1POOL: …` to `/data/plugins/UE4SS.log` and its own channel
`/data/modbridge-fc1pool-facts.jsonl`. Both ini edits stay within
`MAX_PLUGINS=10` (9 entries).

**Ini freeze cycle (exact, per ROLLBACK-RUNBOOK §4.2–4.4 + STAGING-PLAN-NEXT
step 9 — unfreeze→modify→refreeze→re-pin):**

1. RETR pre-image → `ploader.ini.pre`, sha must equal `1216aab6` (the pin).
2. STOR pre-image to `/data/plugins/ploader.ini.$TAG.bak` (rollback artifact).
3. Local edit on the pre-image copy: exactly the two single-character mode
   changes above; `diff` must show exactly two changed lines, nothing else —
   no reorder, no whitespace, no comment drift, header untouched.
4. STOR the edited ini; fresh-connection RETR `diff` against the edited copy
   must be empty (`INI-B1-OK`).
5. **Re-pin**: record the new ini sha256 as the active pin in the run record;
   the doc-level pins in §1 remain valid only for the restore direction.
6. Slot bytes untouched this launch; path_test stays `=2` (control).

Expected observations: popup `Injecting 2 plugin(s)` (fc1pool + path_test);
expected `[PPSA33286] OK (<ms>ms): modbridge_fc1pool.prx` plus a `FC1POOL:`
line in UE4SS.log (file is already on console — verify via RETR sha `d2431bf8`
in pre-flight; launch9 evidence showed it MISSING once, so presence is checked,
not assumed); composition produces NO popup line at all (`=0` entries are
skipped at parse, never attached).

Rollback: game CLOSED, ploader not armed → STOR `ploader.ini.pre` back,
fresh RETR diff empty, re-pin sha `1216aab6`. The slot was never touched.

### 3.2 LAUNCH-B2 — cold-boot repro (memory class)

**Single question: "Do the identical bytes load after a console power-cycle?"**

The smallest meaningful memory-class probe is **no new bytes at all**:
path_test already proves small loads, no intermediate-size artifact exists on
disk (composition builds are all ~30MB; a size ladder would need a new build,
which is outside this matrix's authority), and the launch-10 runs are recorded
as happening on ONE console boot — accumulated in-process state is a live
memory-class hypothesis that a cold boot tests for free.

Variable: none staged. Pre-flight = §2.1 steps 1–5 (all pins must still hold),
plus the recorded fact of an actual power-cycle (not a game close) since the
last failing launch. If §2.1 step 7 already shows the failing launches happened
after a fresh boot, this experiment is already answered — do not spend the
launch; the follow-up is a host-side truncated-composition build decision,
which is a build-worker question, not a launch-matrix question.

Expected observations: identical popup protocol as LAUNCH-A (diagnostic ploader
may stay installed — it was already the staged state; if it was rolled back,
LAUNCH-A's rollback must be undone first so the rc remains observable).
Outcome reading: loads after cold boot ⇒ transient memory/resource state;
same rc ⇒ memory-class rc was structural, move to the host-side size-ladder
decision with the rc as evidence.

Rollback: nothing to restore (no writes); re-verify pins to close.

### 3.3 LAUNCH-B3 — FSELF-form variant (auth/format class)

**Single question: "Does the same composition code in its console-proven FSELF
wrapper class load where the current slot bytes fail?"**

Primary staged artifact (per task): l11-1 top-level
`modbridge_supplier_composition.prx`, sha `d6922807…`, 30,851,495 B, magic
`4f153d1d` — instead of the bare-sdk class currently associated with the slot.

Variable (exactly one): the slot file. Ini untouched (composition stays `=2`),
ploader stays the LAUNCH-A diagnostic build (rc must remain observable — if
LAUNCH-A was rolled back, re-stage it first as its own verified step).

Console FSELF history (verified from receipts — yes, FSELF forms have run):
composition-5 fself `04869ab3` staged and launched ("constructors 232+ all
OK", CONSOLE-LOG item 16); r3 `0d0d1feb`; r11 `0911af0c` ("Production test ALL
PASS", item 19); and the 18:40-OK build `07ac3e1e` is FSELF (magic verified
locally). Auth/format is therefore not the leading hypothesis from history —
which is exactly why the rc must come first.

Provenance note: for an EAUTH/EICV/ENOTCAPABLE outcome specifically, the
sharper first probe is re-staging the receipt-pinned 18:40 bytes `07ac3e1e`
(console-proven FSELF, exact pid-700 build) rather than the newer `d6922807` —
pick ONE of the two, never both in one launch; `d6922807` tests the current
l11-1 code, `07ac3e1e` tests pure reproducibility of a console-proven build.
Controller chooses per the rc; state the choice in the single-question line.

Expected observations: LAUNCH-A popup protocol unchanged; readback follows the
FSELF law (decrypted inner + SHT-strip comparison against the matching local
bare-sdk inner `2b700bdd…` for `07ac3e1e`, or `8ede717f`-class for the l11-1
rebuild — per the artifacts-receipt of whichever file is staged). On success:
new sdiag epoch + composition log markers appear (module_start reached).

Rollback: game CLOSED, ploader not armed → STOR the saved slot pre-image
(`34eafe5d` bytes) back, fresh RETR sha == `34eafe5d`, ini re-checked at
`1216aab6`.

---

## 4. Per-launch observation harvest (every launch, no exceptions)

1. Verbatim popup transcription (all lines, order preserved, timings included).
2. Fresh `/data/plugins/UE4SS.log` RETR (PATH_TEST / FC1POOL / DIAG markers).
3. sdiag record count before/after (new-epoch detector).
4. `/data/townfall_supplier_composition.log` presence/size (absence recorded).
5. FAIL latency in ms vs the 85 ms / 75 ms baselines.
6. Console boot/session identity (fresh boot vs continuous).
7. All §1 pins re-verified at close; any drift becomes its own incident, never
   a silent re-pin.

## 5. Secondary instrument (on the shelf, explicitly NOT launch one)

worker-loader-instrument's second build (`9dccfef1…`) appends a machine-exact
line to `/data/plugins/ploader-results.log`
(`OK|FAIL PPSA33286 path=… rc=0x%08x dt=<ms>ms loader=ploader-diag-filelog
built=<date time>`) — durable, fetchable evidence that removes transcription
risk. Its builder marks it "NOT for first observation launch"; the matrix
honors that. If the controller wants durable rc capture on a B launch, that is
a single-variable launch of its own (ploader payload swap only) and must state
that question first.

## 6. Launch-count discipline

Minimal sequence to the next decision point: **two launches** — LAUNCH-A, then
one outcome-keyed LAUNCH-B. Nothing in this matrix schedules more; the rc and
the B result together select the next host-side decision (reconcile, rebuild,
or triage route), which is outside this document.

---

## Provenance

| fact | source |
|---|---|
| Launch-5 popup transcription + restore pins (`edf61245`/`34eafe5d`/`c656740a`/`b980cea7`) | `outputs/2026-10-04-zcode-controller/incident-loading-regression/POPUP-TRANSCRIPTIONS.md` |
| 18:40 OK build identity `07ac3e1e` (pid 700) | `outputs/2026-10-04-zcode-controller/launch10-evidence/pid700-fullfix/EVIDENCE.md` + `prx-candidate-l11-1-aside-1791243375/artifacts-receipt.json` |
| rc semantics (inject_prx return; `-1` early failures; `pRes` NULL) | `ploader-return-message-1/REPORT.md`; `worker-ploader-imports/PLOADER-IMPORT-ANALYSIS.md` §1 |
| error-code table incl. `0x80020063` ESDKVERSION, `0x80020050` EAUTH, `0x8002000C` ENOMEM, `0x80020064` ESTART | `ploader-rc-80020063-1/public-orbis-error.h` |
| measured 80020063 refusal (75 ms, log_probe) + SDK-stamp fix | `ploader-rc-80020063-1/return-path-review.json`; `worker-prx-packaging/STAGING-CHECKLIST.md:30` |
| strong-import bind refusal rc not established in-tree; import-class analysis | `worker-ploader-imports/PLOADER-IMPORT-ANALYSIS.md` §3, §5, §6 |
| current ini content + staging law | `worker-staging-next/runs/20261005-200730-stage-c/l10-ploader.ini.{pre,post}`; `worker-staging-next/STAGING-PLAN-NEXT.md` |
| ini edit freeze law (value edit, one byte, `.bak`, readback) | `worker-rollback-runbook/ROLLBACK-RUNBOOK.md` §4.2–4.6 |
| diagnostic ploader builds + FAIL format `rc=0x%08x` | `worker-loader-instrument/build-diagnostic/main.c:814`, `build-filelog/main.c:147-152` (shas computed this session) |
| FSELF console history (comp5 `04869ab3`, r3 `0d0d1feb`, r11 `0911af0c` ran) | `outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md` items 12–19 |
| fc1pool identity/behavior | `worker-fc1-validator-probe/REPORT.md`; launch9 missing-file evidence `2026-10-04-zcode-controller/launch9-evidence/LAUNCH9-RESULT.md` |
| FSELF readback law (decrypted inner + SHT-strip) | `outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md` items 11–12 |

## Honest limits

- Design-only: no console was contacted; every pin above is from retained
  receipts and local hashes, and §1 pins must be re-proven by RETR at pre-flight.
- The `34eafe5d` ↔ `07ac3e1e` provenance gap (§1 open item) is unresolved here;
  the matrix is written so that no launch decision depends on resolving it
  silently — pre-flight drift detection handles it loudly.
- The §2.3 table is a draft from the in-tree error header plus one measured
  refusal; worker-loader-instrument's decision table supersedes it when it
  lands. The kernel's import-class refusal code is genuinely unknown in-tree.
- No local byte-match for the installed ploader (`edf61245`) exists; LAUNCH-A's
  diagnostic build additionally differs in wait policy from the 05r lineage
  (83 diff lines vs `worker-staging-next/ploader-build/main.c`). The rc
  semantics of the injected call are unchanged; latency comparisons carry that
  caveat.
- No game-state or quality claims of any kind are made by this document.
