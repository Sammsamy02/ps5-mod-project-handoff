# STATE — L26 WAVE (2026-10-08 night → 2026-10-09 early UTC)

Written by agent ED (ZCode), 2026-10-08 ~23:15 CDT. This is the comprehensive
state record for the full l26 arc: l25 approval → console proof of the crash
fix → the mod-blocker hunt → the converged diagnosis → the 5ea2c845 fix build →
the partial console prep → the pending decisive test. Read this before ANY
console or code action. Companion: `outputs/2026-10-06-recovery-fakesign/
PS5-MODDING-MISSION.md` (rules + prior history; an L26-WAVE appendix was added
there tonight).

ONE-LINE STATUS: crash family SOLVED and console-proven; the remaining blocker
(walk-finds-nothing, root-caused to Find-mode FName index mismatch with a
verified-query/string-fallback fix) is BUILT, SIGNED, and UPLOADED to the
console (PRX 5ea2c845, decrypt-off readback verified) — but the launch prep
FAILED at ploader-arming and the PS5 fell asleep. Wake → finish prep (runbook
§6) → ONE user launch, in-world, L3+R3. The decisive in-world test has never
been run with the fix resident.

All times console-local CDT (UTC-5) unless marked Z. Mac local time == CDT.

---

## 1. How we got here — the arc, with citations

### 1.1 l25 packet approved (Claude review 3)

- Claude review 3 (mission file `PS5-MODDING-MISSION.md` "CLAUDE REVIEW 3"
  entry; packet `outputs/2026-10-08-l25-prep/packet/REVIEW-FOR-CLAUDE-l25.md` +
  review dirs): l25 packet APPROVED. Launch order: **B-first** (skip-registration
  diagnostic), **A second**, with A gated on GMalloc reconciliation
  (0x970add8 vs engine 0x9350950-file/0x9750950-runtime) — **AB2 cleared it**
  (`agent-AB2-gmalloc-reconcile.md`, mission "GMALLOC RECONCILED"): our seed
  binding 0x970add8 IS the primary Binned3 allocator variable; the engine's
  second variable is a lazily-seeded alias holding the identical singleton →
  VARIANT A GO AS-IS. **Variant C approved as l26 production shape** (engine's
  own locked Add at 0x4841A0 with prologue-signature verification, fail-closed,
  no B-skip). Q copyin-gating folded into l26 as a labelled change.
- l25 twins: A runtime 599906eb / B e9dad66f; PRXes 2ee46478 (31,414,407 B) /
  4f6ad852 (31,376,167 B), both fake-signed 4f153d1d.

### 1.2 Console round: the crash family is SOLVED (proof)

Five runs 18:36–19:10 CDT (`CONSOLE-RESULTS-20261008.md`; logs
`harvest/harvest-l25-B-final-*`, `/tmp/l25A-live2.log`, `/tmp/l25A-live3.log`,
`/tmp/zzdiag1.log`, `/tmp/zzdiag2.log`):

| run | window | outcome |
|---|---|---|
| B (orig settings) | 18:36:26–18:40:33 | init past the l24 crash point, registration SKIPPED by design, mod started, clean quit |
| A (orig settings) | 18:43:17–18:45:29 | 2 real Adds (Num 5→6→7, 11 ms), engine CALLED our listeners (calls=2), clean |
| A (cache-on) | 18:50:53–18:52:49 | 3 Adds, engine calls up to calls=12 on one object, clean |
| A+cache+zzdiag v1 | 18:58:33–18:59:54 | full probe cycle, clean |
| A+cache+zzdiag v2 | 19:09:34–19:09:53 | CE-108255-1 (the removed FindFunction probe — see §1.3) |

Proofs: `[L25][DeleteListener] header pre/post` shows the REAL array
(Data=0x1280174d00, Num 5→8, growth_required=0), not the l24 crit-fast-state
misread; klog 0 fatal/copyin in B and A taps; live GMalloc values match AB2's
binary analysis exactly (`GMallocVar=0x84dbef38 GMalloc=0x970add8
GMallocInst=0x97089b8 vptr=0x82c26e0`, true Realloc slot +0x40 staged layout
live). Full pipeline works end-to-end: admission → init → mods → keybinds →
game-thread pump (R3 marshal) → Lua execution → EIGT drain 46/46. Pump timing
adequate (in-game drains ≤369 ms bounds, one 15 ms direct; 9–12 s one-time boot
warm-up). L3+R3 → F7 chain fires every time.

### 1.3 The mod blocker hunt (the night's real detective story)

1. **The UFunction-nullptr MISREAD.** The l25 finding doc
   (`evidence/UFUNCTION-NULLPTR-FINDING.md`) claimed "UFunction <is a>
   nullptr". **CA overturned the framing** (`agent-CA-ufunc-trace.md` §0): the
   decoded console text is "Tried calling a member function but the **UObject
   instance** is nullptr" (LuaUObject.hpp:517) — no UFunction-null string
   exists anywhere in the port. **CC confirmed** (`agent-CC-upstream-diff.md`)
   the resolution chain is byte-identical to upstream UE4SS 3.0.1 — do NOT
   replace it. The null originated one layer earlier.
2. **The diag's `db == nil` FALSE-POSITIVE.** FindFirstOf never returns Lua nil
   — a miss is wrapped as a NON-NIL null-wrapper (`auto_construct_object`,
   LuaUObject.cpp:342-348/441-444). zzdiag's `db ~= nil` check therefore passed
   on a null remote ("A3 ok"), and CALLING it produced the error above. **CD's
   host harness reproduced the exact console error from the verbatim port
   chain** under exactly that condition (`agent-CD-ufunc-harness.md`,
   /tmp/l26-ufunc-harness/). Only `pcall o:IsValid()` is a valid resolution
   test (CX's v7 token design).
3. **The FNAME_Add GC-storm crash and the revert.** l26-v1 (first l26
   composition, OLD logwriter, 19:34:38–19:39:09) carried CN's fix-2a:
   FNAME_Add construction for MEMBER names (LuaUObject.hpp:821). During the
   night's only save/level load this mutated the FName pool from inside the GC
   storm — thousands of listener-driven Lua member accesses, engine-called
   delete counts 6,181 → **13,970** (~100–128/s) — a GATE-1-family crash; the
   log tail died at 19:38:57. Root REVERTED member names to upstream FNAME_Find
   on ALL platforms with a loud NAME_None gate (`[UF] nofn [n:0]`); revert text
   at `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/include/LuaType/
   LuaUObject.hpp:821-829`. The ONLY source row that changed vs the never-run
   20:46 build (CZ §1, CHANGE-SELECTION diff). Post-revert build e13b1d0b
   (21:18): l26-v2 saw only 2→349 calls (~6.4/s menu churn) — revert held.
   CZ's klog caveat: pid 110 actually ended via the NORMAL quit path (the
   "crash" was log-tail loss under the old writer + the storm); the night's
   only REAL crash was an effectively unmodded 20 s boot SIGSEGV at 20:58:56
   (pid 305, RTHeartBeat, libkernel+0x10ec) with ploader dead — never attribute
   it to a PRX. PRX 23c8483d (20:46 build) never executed on console.
4. **The menu-timing RED HERRING.** CZ (`agent-CZ-session-analysis.md` /
   `SESSION-L26V2-ANALYSIS.md`): EVERY FindXOf probe in EVERY session before
   22:41 was MENU-TIMED (no save loaded; delete churn ~6/s). With CDOs
   excluded by `IsValidObjectForFindXOf`, db/ks=invalid, N1–N4=-1, and
   all-nil FindAllOf are the PREDICTED menu-state results even with a healthy
   FindXOf. The l25-era "FindFirstOf WORKS" line was retracted (the null-wrapper
   false-positive). No in-game datapoint existed in either direction — until:
5. **The definitive in-world confirmation (22:49).** zzdiag v7 run with a save
   loaded (`/tmp/now3.log`, session 22:47:09–22:49:26; the CX token set,
   `agent-CX-v7-probe.md`): engine-called delete churn **calls=35419**
   (Index=68563 at 22:48:59 — save/level load genuinely live), and three F7
   cycles (22:49:05/06/25) printed **F1r n, F2r n, A1r n** (and U3r n) —
   FindFirstOf("PlayerController")/("GameEngine") invalid and
   FindAllOf("PlayerController") nil **in-world**. THE WALK IS GENUINELY
   BROKEN, not a menu artifact. Additional hard facts from the same log:
   `S1e/S2e/S3e/U2e` — every `StaticFindObject("/full/path")` probe ERRORS
   under pcall (a distinct port bug — see open item EC), while
   `U1 y v3` (UEHelpers require works) and `Z1 true` (keybind bridge works).

### 1.4 The converged diagnosis (six-way)

CR + CW + CT + CU + CS + CB, all read-only over the port tree and the shipping
decrypted eboot (sha 3b6cfad5…):

- **CR** (`agent-CR-object-reads.md`): every per-object read the walk performs
  (R1–R11: item Object +0x0, Flags +0x8 bit 28, stride 0x18, ClassPrivate
  +0x10, NamePrivate +0x18, ObjectFlags +0x8 bits 0x10/0x20, SuperStruct
  +0x40, GUObjectArray anchors) is binary-confirmed from the engine's own code
  at multiple sites. Failure-mode ranking: **N1 rank-1** — the walk's INPUT
  FName: runtime-constructed FNAME_Find ComparisonIndex ≠ the stored
  NamePrivate index for the same textual name.
- **CW** (`agent-CW-guobjectarray.md`): init type-scan and runtime FindXOf end
  in the SAME ForEachUObject_Chunked over the SAME seed-bound GUObjectArray
  (0x97BA110; hash-table arm dead on PS5 — compiled unavailable). The one
  difference: init compares STRINGS (GetFullName, immune to id derivation);
  runtime compares a Find-constructed INDEX. **The l25→l26 delta is exactly
  Add→Find query construction**: the l25 build (Add-mode query) FOUND
  ItemDatabaseSubsystem in-game; the l26 build (Find-first, CP) returns nil for
  every class with zero [FFO] markers (Find "hits", Equals misses). Not a
  GUObjectArray bug; do NOT touch the walk/seeds/binding.
- **CT** (`agent-CT-uobject-offsets.md`): every l26 profile row for the target
  members matches the binary (UObjectBase {8,C,10,18,20}, FUObjectItem
  {0,8,stride 0x18}, UClass::ClassCastFlags 0xD8 live). No offset mismatch.
- **CU** (`agent-CU-isa-uclass.md`): IsA<UClass> exclusion cannot silently
  over-exclude — pointer-equality SuperStruct walk, canonicals resolved at
  init from string compares (core-ready=true logged), a broken canonical
  THROWS (no crash observed). Filter exonerated.
- **CS** (`agent-CS-walk-harness.md`): host harness with console-parity mocks
  reproduced the all-zero symptom and ranked the misreads; (a)-family named
  forms cannot produce every-class-zero on a full array.
- **CB** (`agent-CB-fname-machinery.md`): the FName machinery decode — the
  bound ctor 0x23d590→0x23d600→0x242470 dispatches **Add → Store 0x23c0b0**
  vs **Find → shard-Find 0x23c340**, different engine paths sharing an id
  space only on paper; **runtime FNAME_Find of an asset/BP-registered name was
  never proven on console** — precisely the unverified link. (Also corrects
  FNAME-VERDICT.md's swapped Add/Find callee labels.)

Converged sentence: names RESOLVE (Find-mode hit, non-None, zero [FFO]
add-fallback), indices never MATCH stored, all reads are binary-correct, the
machinery is upstream-identical — the defect is the constructed query index,
the l25→l26 delta = Add→Find construction.

### 1.5 The fix stack in build 5ea2c845

Build: twin passes byte-identical, 22:41–22:55 CDT;
`outputs/2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx`,
sha256 `5ea2c845ba4a01dde4599c08f1930262b394e76279d1bfbaa235a19499dbfc51`,
31,412,727 B, SELF magic 4f153d1d, 20/20 static checks each pass, 207 exact
import identities UNCHANGED vs the l24 baseline (import_delta none),
.init_array 238. Variant A (normal registration). Receipts:
`FINAL-RECEIPT-VARIANT-A.json`, `PREPARATION-RECEIPT.json`,
`outputs/2026-10-08-l26-build-variants/EXECUTION-RECEIPT.json` (34 changes, 176
affected units). Contents:

1. **CP Find-first** (`agent-CP-findfirstof-fix.md`): every FindXOf string
   overload constructs `FName(ClassName, FNAME_Find)` first, Add fallback only
   on IsNone, with the `[FFO] add-fallback [n:X]` marker (the port's own
   Hooks.cpp:511-518 precedent).
2. **DA verified-query + string fallback** (`agent-DA-verified-query-fix.md`):
   `FindFirstOfVerified/FindAllOfVerified` round-trip the constructed FName via
   ToString (`String::iequal`) against the query; on MISMATCH run a
   string-compare walk mirroring the init-proven GetFullName mechanism. Marker
   **`[FFO] query-fallback [n:X]`** (deduped, 128 cap). Harness 7/7 PASS
   (DA2_N1_CONSOLE_SYMPTOM reproduced pre-fix; DA2B inverted post-fix),
   host+prospero compiles clean, CHANGES rows synced. `__PROSPERO__`-guarded;
   host bodies upstream-identical.
3. **CN hardening** (`agent-CN-resolver-impl.md`): null-guards, loud gates
   (`[UF] nofn`), Equals-index-only member compare per CB — after the fix-2a
   revert, member names stay FNAME_Find.
4. **CL logwriter F1–F4** (`agent-CL-logwriter-impl.md`, CE's proposal): the
   wide-char Lua log writer fix — **l26-v2 was the FIRST FULLY-READABLE UE4SS
   session ever** (0 corrupted/truncated lines vs 284–925 baseline), which is
   why everything above is legible.
5. **Staging import fix (build-time)**: the packaging staging was repointed and
   reconciled for the rebuild — CM's l26 driver repoint/row-sync
   (`agent-CM-l26-drivers.md`: BASE=l25-A, l25 layer_rank preference, l26map of
   the frozen l24 pins) plus DA's refreshed candidate rows, so the build's
   every-row-against-disk assertions and the import-identity contract passed
   (imports_unchanged=true, 207 identities, supplier-contract
   missing_strong=0). The zzdiag-v8 side of staging (console loadlist import)
   is staged locally at `/tmp/loadlist-v8.txt` (545 B, 9 rows, zzdiag main.lua
   row = 1488) — uploaded at wake per §6.
6. Carried in the binary from the 21:18 e13b1d0b build: the fix-2a revert
   (§1.3.3). Q's copyin liveness overlay wired in both variants (import delta
   none — diagnostic_event already undefined).

### 1.6 Console prep (23:08–23:09 CDT) — PARTIAL

`harvest/LAUNCH-PREP-FAILED-A-20261009T040918Z.md` (prep_modded_launch.py,
mode live):

- Steps 1–5 ALL PASS: daemons up (kstuff=87, pldmgr=85, ftpsrv=89, klogsrv=96),
  no game running, no resident ploader; resident PRX backed up decrypt-off
  (e13b1d0b…, 31,393,095 B); **5ea2c845 uploaded, decrypt-off readback
  byte-exact (magic 4f153d1d, sha+size match)**; settings uploaded = the
  ORIGINAL 1947 B b980cea7 (cache-OFF) with byte-exact readback; ploader.ini
  enabled-state uploaded (440 B, 3eab9510) with byte-exact readback.
- **STEP 6 FAILED**: the single loadpayload dispatch returned rc 0 / body OK
  but NO fresh payload.elf pid appeared within 10 s (pre-arm set [99]). →
  **ploader is NOT armed. Do NOT launch.** The dispatch is one-shot; never
  duplicate-send; re-run prep from STEP 1 only after understanding the refusal.
- After the failed prep, a cache-on settings re-upload
  (/tmp/l25-settings-cacheon-1947.ini, sha 5847362e…) was attempted and
  INTERRUPTED (the PS5 went to sleep mid-flow). Last VERIFIED settings state =
  cache-off original (readback b980cea7 at 23:09). → settings state UNCERTAIN;
  VERIFY at wake (§6 step 3).

---

## 2. CURRENT STATE (as of sleep, 2026-10-08 ~23:10 CDT)

- **PRX on console: 5ea2c845 (verified-query build), uploaded + decrypt-off
  readback VERIFIED.** Resident and next to load at game launch — once ploader
  is armed.
- **Settings: UNCERTAIN.** Verified last state = 1947 B cache-OFF original
  (b980cea7). The cache-on re-upload was interrupted. The wake runbook checks
  and applies cache-on (1947 B, bUseUObjectArrayCache=true, admission-legal)
  if needed.
- **ploader: NOT armed** (step-6 failure).autoload.txt still has ploader last
  line; re-arm via ONE /loadpayload dispatch at wake after verifying no
  resident payload.elf.
- **PS5: ASLEEP.** No game running at sleep. Daemons were up at last contact.
- **Decisive in-world test: PENDING** — never run with the fix resident.
  Expected on success: `[FFO] query-fallback [n:X]` markers (the fix working —
  one per probed class if N1 is real in that pool state), FindFirstOf/
  FindAllOf returning valid objects in-world (zzdiag v8 `T<n> db=v pc=v`), and
  the mod's actual menu on L3+R3. If `[FFO] add-fallback` fires for known
  classes instead → GATE-1/pool-admission family, not N1. If NEITHER marker
  fires and probes still return nil → N1 falsified; re-read CR §4 (A2
  wrapper/validity layer next).
- Rollback: e13b1d0b backed up locally
  (/tmp/l25-variant-A/backup-resident.decrypt-off.prx) and the original
  settings (/tmp/l24-settings-backup.ini).

## 3. Console identity card

- Console ps5-host-not-configured.invalid (HTTP 8084 / FTP 2121 / klog TCP 3232). Game: Silent
  Hill Townfall PPSA33286. Image sha 3b6cfad5…, base 0x400000, UE 5.6.1.
- PRX path on console: /data/plugins/modbridge_supplier_composition.prx.
- Mod root: /data/modbridge/roots/PPSA33286/ (loadlist.txt admission manifest —
  SIZE-EXACT law; ue4ss/UE4SS-settings.ini; ue4ss/Mods/{TownfallItemSpawner,
  zzdiag}/Scripts/main.lua).
- ploader: /data/pldmgr/payloads/ploader/ploader.elf; last line of autoload.txt;
  exits per game-launch cycle → must be re-armed per boot via
  `http://ps5-host-not-configured.invalid:8084/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fploader%2Fploader.elf`.
- Permanent PS5 rules in force (AGENTS.md): decrypt-off `-Q "SITE SELF"` for
  every executable readback; never upload unsigned/modified game executables;
  one launch per test, user launches; klog captures to NEW files; no
  rapid-fire launch tests; quarantined kstuffSharp untouched.

## 4. What is NOT yet proven (honesty box)

- No console execution of 5ea2c845. No [FFO] marker, working lookup, mod menu,
  or spawn has ever been observed. The N1 mechanism is the rank-1 hypothesis
  with a fix that harness-inverts the symptom — not a console-verified repair.
- StaticFindObject full-path probes ERROR (S1e/S2e/S3e/U2e in every v7 session,
  menu AND in-world) — a separate unresolved port bug (EC, §5).
- Variant C and Q gating are approved-but-unwired l26 production items (EE).
- B variant never to be left resident (diagnostic-only; replaced by l26).

## 5. Open items (in flight at sleep — check for their reports before redoing)

| id | item | notes |
|---|---|---|
| EB | FName ADAPTER ROOT-FIX | fix the id derivation at the FName adapter so bound-ctor Find results equal stored ids (CW fix-shape 2; CB decoded sites 0x23c340 vs 0x23c0b0) — the narrow complement to DA's fallback |
| EC | StaticFindObject GetPackageNameFromLongName bug | every full-path probe throws under pcall (S1e/S2e/S3e/U2e, /tmp/now3.log); suspect the long-name package-split helper on this port |
| EE | variant C + Q gating integration | engine Add 0x4841A0 registration re-route (Claude-approved l26 production) + Z15 copyin gating as labelled changes |
| EA | l26 Claude packet | review packet for the l26 round (user said fix-first without Claude; call for review when needed) |
| — | GetEngine-invalid anomaly | U3r n IN-WORLD with a save live (now3.log): UEHelpers.GetEngine()=FindFirstOf("Engine") invalid although GameEngine 0x19000390A0 was located at init — unexplained; retest after DA fix lands |
| — | marshaling ambers | CI's table (`agent-CI-marshaling-audit.md`): rows 3–24 AMBER (first-live-verification items), #8 AMBER-CRITICAL (FName→string via Conv_NameToString — engine ProcessEvent class never executed on console; schedule it as the FIRST instrumented engine call after resolution lands) |
| — | carried l26 list | compiled-in FMalloc vtable default-map correction (latent; defense in depth), full item-6 suite re-run, CD's ClassCastFlags 0xD0-vs-0xD8 note (staged 0xD8 live, no traversal issue observed) |

## 6. WAKE-UP RUNBOOK (exact commands — run when the PS5 is awake)

Script form (preferred; does all of the below with checks):
`bash /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/wakeup_prep.sh`

Manual equivalent:

```bash
FTPR="ftp://ps5-host-not-configured.invalid:2121"
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what

# 1. console state: no game running; daemons present
curl -s --max-time 8 "http://ps5-host-not-configured.invalid:8084/processes_list" | grep -c eboot   # must be 0
curl -s --max-time 8 "http://ps5-host-not-configured.invalid:8084/processes_list" | grep -c payload.elf  # 0 = not armed

# 2. PRX on console must be the verified-query build (decrypt-off!)
curl -s --max-time 15 -Q "SITE SELF" "$FTPR/data/plugins/modbridge_supplier_composition.prx" -o /tmp/wake-prx-check.prx
shasum -a 256 /tmp/wake-prx-check.prx   # MUST start 5ea2c845; magic 4f153d1d
# mismatch -> STOP, re-run prep_modded_launch.py from step 1 (never partial-patch)

# 3. settings -> cache-on (1947 B, bUseUObjectArrayCache=true). VERIFY FIRST (state uncertain):
curl -s --max-time 12 "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini" -o /tmp/wake-settings.ini
wc -c /tmp/wake-settings.ini; grep bUseUObjectArrayCache /tmp/wake-settings.ini
# if not 1947 bytes with =true, apply:
curl -s --max-time 15 -T /tmp/l25-settings-cacheon-1947.ini "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini"
curl -s --max-time 12 "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini" | cmp /tmp/l25-settings-cacheon-1947.ini - && echo CACHE-ON APPLIED

# 4. zzdiag v8 (auto-prober, 1488 B) + its loadlist row (size-exact admission)
curl -s --max-time 15 -T /tmp/zzdiag-v8/main.lua "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua"
curl -s --max-time 15 -T /tmp/loadlist-v8.txt "$FTPR/data/modbridge/roots/PPSA33286/loadlist.txt"
curl -s --max-time 12 "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua" | cmp /tmp/zzdiag-v8/main.lua - && echo V8 LIVE

# 5. arm ploader ONCE (skip if a payload.elf is already resident)
curl -s --max-time 20 "http://ps5-host-not-configured.invalid:8084/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fploader%2Fploader.elf"
sleep 5; curl -s --max-time 8 "http://ps5-host-not-configured.invalid:8084/processes_list" | grep -c payload.elf  # must be >= 1
# (last attempt returned OK but no pid within 10 s and the console then slept; if it repeats, STOP and reconcile — never duplicate-send)

# 6. klog tap (NEW file; 2 h window)
nohup python3 $WHAT/outputs/2026-10-07-l18-full-build/console-test-20261007T205350Z-f1310cc9/klog_capture.py \
  --out-dir /tmp/l25-variant-A --window-seconds 7200 > /tmp/l25-variant-A/wake-tap-stderr.log 2>&1 &
```

Then tell the user (ONE launch, user-launched):

1. LAUNCH Townfall. 2. Load the SAME save as the l24 crash run. 3. Get
IN-WORLD (controlling the character). 4. Wait ~10 s. 5. Press **L3+R3 ONCE**
(OPTIONS stays the game's pause). 6. CLEAN EXIT via in-game quit to dashboard —
never a power event. If it crashes: note code + WHEN (menu / save-load /
L3+R3), harvest immediately, REBOOT before any next launch.

What to watch (v8 auto-reports every 6 s from mod start, no button needed):
`[zz] T<n> db=<v|n|i|e> pc=<...>` — db=v/pc=v once the save is live means the
lookups WORK. On F7: the full probe set. In UE4SS.log: `[FFO] query-fallback
[n:X]` = the DA fix engaging (expected, good); `[FFO] add-fallback` = GATE-1
family; neither + still-nil = N1 falsified (CR §4 A2 next). Mod-side success
ladder: spawner scan builds an item list (no "Scan failed: … Item database not
ready"), menu opens on L3+R3, item spawns (ACCEPTANCE-LADDER.md).

Post-run: `python3 $WHAT/outputs/2026-10-08-l25-prep/harvest/harvest.py --label l26-a-<n>`
(read-only harvest BEFORE any relaunch).

## 7. Key file map (tonight's provenance)

- This wave's reports: `outputs/2026-10-08-l25-prep/agent-{CA..CY}-*.md`
  (CA ufunc-trace, CB fname-machinery, CC upstream-diff, CD ufunc-harness,
  CE logwriter-proposal, CG findfunction-crash, CI marshaling-audit,
  CL logwriter-impl, CN resolver-impl, CO walk-audit, CP findfirstof-fix,
  CQ l26-integrity, CR object-reads, CS walk-harness, CT uobject-offsets,
  CU isa-uclass, CW guobjectarray, CX v7-probe, CY fix-templates,
  CZ session-analysis), `CONSOLE-RESULTS-20261008.md`,
  `SESSION-L26V2-ANALYSIS.md`, `CLAUDE-REPORT-20261008.md` (+ claude-review 1/2),
  `evidence/UFUNCTION-NULLPTR-FINDING.md` (with CA amendment),
  `evidence/SETTINGS-DEVIATION-NOTE.md`, `RUNBOOK-MOD-MENU-UP.md`,
  `ACCEPTANCE-LADDER.md`.
- Build: `outputs/2026-10-08-l26-package-A/` (5ea2c845 receipts),
  `outputs/2026-10-08-l26-build-variants/` (twin + execution receipts),
  `outputs/2026-10-08-l26-runtime/` (typescan-r4 candidates, drivers,
  fix-templates, variant-c-proposal, gating-proposal).
- Session logs (raw, /tmp): l26v2.log, v6-run.log, l26-crash.log, v7-run.log,
  now.log/now2.log (menu-state v7), **now3.log (the 22:49 in-world proof)**,
  live-1..6.log (tails), zzdiag1/2.log (l25 round). Normalized copies *.norm.log.
- Klogs: /tmp/l25-variant-A/klog-20261009-*.log, /tmp/l25-variant-B/.
- Prep artifacts: `harvest/LAUNCH-PREP-FAILED-A-20261009T040918Z.md`,
  `harvest/wakeup_prep.sh`, `harvest/prep_modded_launch.py`, `harvest/harvest.py`,
  /tmp/l25-settings-cacheon-1947.ini, /tmp/zzdiag-v8/main.lua (1488 B),
  /tmp/loadlist-v8.txt (545 B), /tmp/l24-settings-backup.ini (b980cea7),
  /tmp/ploader-ini-backup-20261008.ini.

## 8. Reading rules that bit us tonight (do not relearn)

- "CRASH KILL_END" in klog = the NORMAL quit path; "SIGSEGV: normal" lines =
  Mono/PSM banner. Never mine klogs for bare "CRASH"/"SIGSEGV".
- FindFirstOf/StaticFindObject wrappers are NEVER Lua nil — only
  `pcall o:IsValid()` tests resolution; FindAllOf returns real nil.
- The launch gate must assert a LIVE ploader immediately before each user
  launch (tonight's 20:58 ghost crash + PRX 23c8483d never-executed lesson).
- Settings/loadlist edits are SIZE-EXACT (modforge MFA admission): 1947 B
  settings, loadlist row byte-counts must match the staged file exactly.
- No FindFunction/class-children probes from Lua (zzdiag-v2 crash); v8/v7
  contain none.

— agent ED (ZCode), 2026-10-08. Sources cited inline; raw logs untouched.

---

## 9. 2026-10-10 MENU-UP session + Claude round-3 directives

Written by agent FP (ZCode), 2026-10-10 early UTC, while the console sleeps and
agents FJ–FO run offline. Sections 1–8 above are ED's record, untouched. This
section carries the wave forward from the round-2 build (§ run of PRX efbfc656)
through Claude's round-3 orders. Companion docs written in this pass:
`WAKE-RUNBOOK-20261010.md` (exact wake sequence) and
`CLAUDE-ROUND3-RECEIPT.md` (directive→agent→artifact receipt).

### 9.1 THE MILESTONE: mod MENU opened on PS5 for the first time

Unmodified TownfallItemSpawner, session 2026-10-10 00:53–00:56 (log time),
PRX **efbfc656** (build id `l26-20261010T043709Z`, frozen snapshot 24025929 —
the Claude round-2 build: bridge + parameter marshaling + [FFO2] oracle +
[BUILD] stamp + toolchain probes, 13/13 manifest markers verified in the PRX
before upload). Full session log:
`evidence/claude2-menu-session-20261010.log` (mixed narrow/wide encoding —
ALWAYS `grep -a`). Round-2 report to Claude:
`CLAUDE-MENU-UP-SPAWN-BLOCKED.md`.

Evidence lines, in order (verbatim from the log):

```text
[Console log excerpt omitted from the private source handoff.]
```

- `[LUA-FN] id=0x31AAD` for PlayerController is EXACTLY the [ST] self-test
  index Claude specified — the string seam is FIXED at the FName layer, and
  both round-2 toolchain probes ([ST] wcslen-16units, [ST] wstring-rt) PASS:
  wcslen walks 2-byte wchar correctly at runtime; the "prebuilt libc++ 4-byte
  stride" theory is dead for wcslen/wstring themselves. Remaining work is
  call-path routing, not a global toolchain defect.
- The mod's own ladder all passed: DB scan found 24 spawnable pickups
  (Pages=3), UMG Create + StaticConstructObject(TextBlock) + SetText(FText)
  all worked (DRY-4/5/6), and the F7 menu was VISIBLE IN GAME. The full path
  engine init → slot-2 ctor → decoder → bridge → FindFirstOf → UMG → mod scan
  → menu is console-proven end-to-end.

### 9.2 The one remaining blocker: ItemId reverse read at spawn configure

User browses the menu and spawns: the actor SPAWNS, the PickupInteractable
component check PASSES, then configure fails:

```
[2026-10-10 00:55:47] [Lua] [TownfallItemSpawner] Failed at configure pickup.
  See UE4SS.log. ...main.lua:317: Spawned pickup has no valid inventory ID.
```

The failing line is `str(pickup.PickupInventoryItem.ItemId)` — a REVERSE
property read (engine FName/FString property → Lua string), wrapped in a
swallowing pcall in the mod, so the real failure mode (throw vs "None") is not
yet visible. Smoking-gun refusal line, fired ONCE (once-per-reason notify) at
exactly the item-scan moment (00:54:35 = "24 spawnable pickups found"):

```
[2026-10-10 00:54:35] [FN] seam refused: reason='embedded' len=4 at=1
  (NAME_None produced; embedded NUL in a wide buffer is the
  stride-corruption signature)
```

len=4 with embedded NUL at 1 = e.g. 'X\0Y\0' — 2-byte-chars read as
4-byte-units; correctly REFUSED (NAME_None) instead of crashing.

Reverse-family zero-fire evidence ([LFB-M] first-10 marshaling families): the
FORWARD families fired and capped (fname-narrow ×10, fstring-fwd ×10,
ftext-fwd ×10 — all ok=1), while the REVERSE families **fstring-rev and
ftext-rev have ZERO lines in the whole session** (grep-verified: 0 matches).
The ItemId read is exactly a reverse-direction read → reads return through a
DIFFERENT, unrouted converter that still carries the seam (hypothesis H3,
supported; H1 throw-inside-pcall and H2 ToString-hits-seam remain live until
the probe names the hop).

### 9.3 Claude round-3 directives (verbatim-summarized)

1. **Q1 — zzdiag v11 pickup probe (agent FJ).** Instrument zzdiag, NOT the
   mod: a v11 pickup probe that reproduces the ItemId read path with verbose
   logging (type/raw tostring/pcall error text per hop), so the exact failing
   hop is named on the CURRENT build. Instrument-first: the probe launch rides
   the RESIDENT efbfc656 before any new PRX is deployed.
2. **Q2 — refusal logging (agent FK).** Harden the [FN] seam-refusal
   instrumentation: first-10 logging per reason, call-site tag on every
   refusal, and hex units of the offending buffer (so the len=4 embedded-NUL
   source is identifiable, not just counted).
3. **Q3 — reverse-site routing (agents FL/FM).** Full conversion-site
   inventory (every engine→Lua string conversion site), then route ALL reverse
   sites (FName/FString/FText property reads: NativePropertyReadBackend /
   CustomProperty / LuaUObject __index return path) through the bridge's
   push_fstring_to_lua/from_fname — same treatment as round-2's forward sites.
4. **Post-spawn sequence (agents FN/FO).** Once spawn works: freeze the
   known-good state (FN, with an identity doc for the byte-identical ORIGINAL
   mod), rerun the byte-identical original mod as the MENU+SPAWN acceptance,
   then start the 12-mod generality ladder (FO's schema; staged candidates in
   `multimod-prep/`).

Root, while the console sleeps (offline): sync CHANGES rows, rebuild, package,
and `verify_build.sh --manifest` the new routed build; sha lands in the build
receipts under `outputs/2026-10-08-l26-package-A/` when verified.

### 9.4 Agent map (round-3 wave)

| agent | assignment | artifact (expected) |
|---|---|---|
| FJ | zzdiag v11 pickup probe (instrument zzdiag, not the mod) | `zzdiag-v11/` (main.lua + EXPECTED-MARKERS.txt) + agent-FJ report |
| FK | refusal logging: first-10 + call-site tag + hex units | agent-FK report + staged refusal-logging patch |
| FL | full conversion-site inventory (all engine→Lua string sites) | agent-FL inventory report |
| FM | route ALL reverse sites through the bridge | agent-FM report + CHANGES rows (root builds) |
| FN | freeze known-good + byte-identical original-mod identity doc | agent-FN identity/freeze doc |
| FO | 12-mod ladder schema | agent-FO ladder schema |
| FP | docs/state only (this section, wake runbook, round-3 receipt) | STATE §9, `WAKE-RUNBOOK-20261010.md`, `CLAUDE-ROUND3-RECEIPT.md`, `agent-FP-runbook.md` |

(FJ–FO were still running in parallel when this section was written; their
artifacts land tonight offline. Consumed at wake per the runbook.)

### 9.5 Console state at sleep (2026-10-10 early UTC)

- **PS5: REST MODE** until the user wakes it. ALL current work is offline.
- **PRX efbfc656 RESIDENT** (fake-signed 4f153d1d, decrypt-off readback
  verified). Next-to-load at game launch once ploader is armed.
- **zzdiag v10 loaded** on console (auto-probe + M1–M6 + F7 rerun).
- **Settings: ORIGINAL cache-off** (1947 B original).
- ploader was armed fresh before sleep, but its resident window is ≤120 s
  (Z2 law: "Timeout, shutting down.") → it is EXPECTED to be gone at wake;
  re-arm once per the wake runbook; never double-dispatch.
- klogsrv died in a sleep cycle tonight and was restarted; EXPECT daemon
  deaths again at wake. `harvest/wakeup_prep.sh` CHECKS daemons but does NOT
  start them — the klogsrv restart command is in the wake runbook (§1).
- Wake-time sequence (user + root): `WAKE-RUNBOOK-20261010.md`. Order:
  wake/daemons → zzdiag-v11 deploy + PROBE launch on resident efbfc656 →
  deploy verified routed build → byte-identical original-mod acceptance →
  12-mod ladder.

— agent FP (ZCode), 2026-10-10. Evidence lines quoted verbatim from
`evidence/claude2-menu-session-20261010.log` (grep -a; reverse-family counts
re-verified 0 by grep). Raw logs untouched.

---

## 10. 2026-10-11 ROUND-4 SPAWN-PROVEN session + Claude round-8 directives

Written by agent IJ (ZCode), 2026-10-11, docs only. Sections 1–9 above are
ED's and FP's records, untouched.

### 10.1 THE MILESTONE: items SPAWN on the round-4 build — project blocker closed

Launch-1 of the round-4 PRX **59b0528b** (build id `l26-20261011T030618Z`,
snapshot a79b3655 — both confirmed in-log; contents per the accepted
declaration: r3 base + FR parity revert + FW literal markers, shim/first-
launch/KB TU set aside), session 08:47:49–08:50:27 console-local, evidence
`evidence/round4-spawn-session-20261011{.log,.decoded.log}` (195,738 B raw,
decoded via the GZ tool). The mod's own log receipts the chain: "Menu
created." → "Spawned Brick x1 ID=5e" → "Spawned Shotgun x1 ID=54" (plus 3x
Shotgun Ammo ID=55) — the ItemId read resolves, configure-pickup passes,
items spawn and the user eye-confirmed them in use. The FR parity revert is
proven by the FJ pickup probe ladder on the same session: 357 PRB-family
lines, `[PRB-1] inv type=userdata` → `udt t=UScriptStruct` → `[PRB-2] id
type=userdata` → `[PRB-3] tos val=3f2` AND `[PRB-4] TS val=3f2` both ok →
`[PRB-6] actor ok=1` — objects pass back through :ToString/__tostring, the
exact PC-mod shape. Driver rates: cb=23/24 sustained (25 T-cycles T1–T25,
db=v pc=v throughout — the 250ms LoopAsync cadence, GY's return-false law
held). Open tails from the same log: the GB self-test was armed but NOT
FIRED (`[ST2] boot v1 M=167 armed=true pf=false`, 0 idx lines — L1+R1 never
triggered); `[MAP]` absent (World instance lookup nil/"?" in the states
visited although the FFO2 class scan finds `World` exact=2 — class-shape-
specific); `[ST2] progress-io unavailable (resume disabled)` (expected
without the shim, SKIP-class by design). Full verdict:
`CLAUDE-ROUND4-SPAWN-PROVEN.md` (citations independently re-verified against
the decoded log by agent IJ — drift report in `agent-IJ-round8-receipt.md`;
the only material drift is "[PRB] done x2" vs 16 actual completed runs, an
understatement). Deploy notes: one MFA all-or-nothing refusal
(mfa_activate_failed -6, undeclared-file) fixed with a 0 B marker + full
22-row readback; zzdiag row re-pinned 4065→4094.

### 10.2 Claude round-8 map (directive → status; full table in CLAUDE-ROUND8-RECEIPT.md)

1. Freeze round-4 as known-good — DONE (root): FROZEN-KNOWN-GOOD-20261010.md
   §"ROUND-4 FREEZE (2026-10-11)" supersedes efbfc656 as fallback.
2. fname-eq check — authored (GB ueselftest call 29/37), execution pending
   the L1+R1 press.
3. Self-test lines+trigger — line-contract DONE (ueselftest/EXPECTED-
   MARKERS.txt; L1+R1→F12, 1736 B pad), already resident; armed-not-fired.
4. World bug + map logger — logger DONE (zzdiag-v11 `[MAP]`), World instance
   lookup bug OPEN (milestone question #4 rides launch-2).
5. Writable area — staged-at-AC (round-5 shim set aside in round4-setaside/
   with exact restore procedure, or pre-staged loadlisted progress file).
6. Deploy gate — DONE in-session (the MFA refusal was caught and fixed;
   size-exact + readback laws standing).
7. Labelled-build queue — staged-at-AC (WAKE-RUNBOOK §11/§12: serial-
   equivalence proof → P1 fault-ledger → P2 variant-B → round-5; round-4
   step complete).

### 10.3 Console state (2026-10-11)

- **Round-4 59b0528b RESIDENT and known-good** (decrypt-off verified;
  31,423,015 B; the fallback is now: 59b0528b > efbfc656 menu-up > nothing —
  never an unbuilt sha). zzdiag-v11 + ueselftest deployed, pad 1736 B, all
  22 loadlist rows verified. READBACK LAW unchanged: decrypt-off after every
  power event.
- **Ladder v2 queued**: GC's four type-mods + the self-test re-arm ride the
  next launch window on the FINAL gated round-5 build (one launch + up to 2
  restarts; bisect with the archived labelled builds, no separate launches).
- No round-8-specific agent artifacts had landed when this section was
  written; items 2/3 reuse GB's deployed ueselftest, 4's probe is FJ's v11.

— agent IJ (ZCode), 2026-10-11. Evidence counts re-verified by grep -a over
both the raw and decoded round-4 logs; raw logs untouched.
