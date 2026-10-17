# EXECUTION-GUIDE.md — controller cheat sheet: promotion signed → Item Spawner evidence checked

Worker: worker-post-promo-exec, 2026-10-04. This is the literal, numbered command
sequence the controller runs in the 30-minute window after the layout-pair promotion
signs. Every command is taken verbatim from the verified sources (cited per step):
`worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md` (§A–§H, the 9-file manifest),
`worker-final-staging-rehearsal/REHEARSAL-RESULT.md` (§6 — host-rehearsed, READY TO
EXECUTE, 0 mismatches), `worker-post-promotion-runtime/RUNTIME-STAGING-PLAN.md`
(§5–§7 evidence + launch ritual), `worker-promotion-decision/PROMOTION-DECISION.md`
(§2 criteria, §4 sign-off block), `worker-acceptance-evidence/EVIDENCE-SPEC.md`
(rung checks), `worker-integration-checklist/INTEGRATION-CHECKLIST.md` (STEP 0/14 bars).

**Honest status line:** the rehearsal verified commands and hashes host-side only.
Gate A0 (FTP STOR write route to the console) has never been evidenced — it is the
first live command and carries a hard STOP. The PRX on the manifest is the
composition-3 PLACEHOLDER (composition-4 has not built, rehearsal finding G3): this
launch can produce ONLY Tier A evidence. Item Spawner evidence (Tier B) is checked
at STEP 10 with the documented-absence expectation; no acceptance rung can advance
from this launch. "No crash" is never success; one launch per staged build.

## Conventions (paste once at the top of the controller shell; used by every step)

```sh
set -e   # abort the pass on ANY failed command (all steps below assume this)
HTTP=http://ps5-host-not-configured.invalid:8084        # payload manager (ploader arming)
FTP=ftp://ps5-host-not-configured.invalid:2121          # ftpsrv, anonymous; auto-DECRYPTS FSELF on RETR
TAG=$(date +%Y%m%d-%H%M%S)
W=/WORKSPACE/.codex/worktrees/translation-core/what
T=$W/outputs/2026-10-04-zcode-team
R=/data/modbridge/roots/PPSA33286    # activation root == exact mfa_config::root string
CONSOLE=$T/zcode-controller/console  # evidence/archive destination (create if absent)
mkdir -p "$CONSOLE" /tmp/ppsa33286-preimages
```

Time budget: 30 min total — Step 0: 3 · 1: 2 · 2: 2 · 3: 1 · 4: 3 · 5: 3 · 6: 3 ·
7: 1 · 8: 4 · 9: 4 · 10: 2 · Z: 3. If any single step consumes its budget twice,
stop and record; do not compress verification steps to catch up.

---

## STEP 0 — Sign the promotion decision (3 min)

**Command / action.** C2 is the signature blocker: sign ONLY if igf6 returned
`byte_match=1` for BOTH anchors (Tick `0x33ac8c0`, SCO `0x890aa0`, expected
`554889e54157415641554154534881ec`) under the refusal discipline — or you now name
one concrete alternative read with its own record evidence (PROMOTION-DECISION §2 C2).
Then complete every field of the §4 sign-off block into a NEW file (never edit
PROMOTION-DECISION.md itself — raw reports stay unchanged):

```sh
cat > "$CONSOLE/promotion-signoff-$TAG.md" <<'EOF'
DECISION (circle one):   PROMOTE (rung 1->2 subset)   /   HOLD   /   NO-GO
Timestamp (UTC): <fill, e.g. 2026-10-04THH:MM:SSZ>
C1 GUA validation stands .......... [x] accepted  (worker-predicate-gc-fix/REPORT.md §4-5;
                                     anchor-validation-results.jsonl seq 6-7,9; digest 97a16867...)
C2 Code anchors settled ........... [ ] (a) igf6 byte_match=1 both anchors  build_id: <fill>
                                       run: record seq(s): <fill>  bytes: tick <fill> sco <fill>
                                    [ ] (b) alternative (NAMED): <fill or strike>
C3 Identity + axes accepted ....... [x] ratified  revision "5.6.1-0+++UE5" [x]
                                     build_id "04bb4330a7d71dcefe1cc2c4d19769d5" [x]
                                     axes SHIPPING / STATS OFF [x]
C4 Scope limit respected .......... [x] FUObjectArray + TUObjectArray only (rung 1->2)
C5 Artifact re-emission w/ real identity, ParsePair + negative control ... [x] verified
   (worker-emit-final-pair/emission-final: parsepair-result.json ok:true;
    BuildId stripped -> exit 3; null identity -> exit 2)
igf5 failure acknowledged as documented, cause under diagnosis, no replay ... [x]
Evidence citations consulted: <list the records actually read>
Conditions or reservations: <fill if HOLD/NO-GO: the blocking criterion>
Controller signature: zcode-controller, session 2026-10-04-zcode-team
EOF
```

Also append the promotion addendum to the transport ledger checkpoint (same pattern as
`zcode_anchor_validation_RESULTS` in
`worker-composition-3/evidence/transport-ledger.composition3-mirror.json`) — a NEW
entry; never rewrite history.

**Expected:** the signed file exists with PROMOTE circled and every box resolved.
**If C2 is unsettled and no alternative is named:** record `HOLD` with the blocking
criterion and STOP — the rest of this window does not run; the pair stays evidence.
**C5 note:** the pair to stage is `worker-emit-final-pair/emission-final/` (real
identity, LIVE-EVIDENCE-EMITTED) — never any `GAP-NOT-A-SCAN-FACT` artifact.

## STEP 1 — Freeze check: local re-hash of all 9 manifest files (2 min)

Manifest §H mandates re-verification at execution time. Run and compare to the
manifest §A values (rehearsal C1–C9 already matched them; this catches drift since):

```sh
shasum -a 256 \
  "$T/worker-composition-3/prx-candidate-composition3-1/modbridge_supplier_composition.prx" \
  "$T/worker-console-staging-bundle/loadlist.txt" \
  "$T/worker-settings-bundle-merge/staged-package/ue4ss/UE4SS-settings.ini" \
  "$T/worker-emit-final-pair/emission-final/MemberVariableLayout.ini" \
  "$T/worker-emit-final-pair/emission-final/VTableLayout.ini" \
  "$T/worker-settings-bundle-merge/staged-package/ue4ss/PS5Pad.ini" \
  "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua" \
  "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/enabled.txt" \
  "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"
shasum -a 256 "$T/worker-composition-3/prx-candidate-composition3-1/build-receipt.json" \
  && python3 -c "import json,sys;g=json.load(open(sys.argv[1]))['gaps'];print('gaps:',g);assert g==[]" \
     "$T/worker-composition-3/prx-candidate-composition3-1/build-receipt.json"
```

**Expected (§A truth):** `b8bfae77c8d147b54864a6fc5ac1f24355ba3fb13c79b0cd92e11bc7e36aab35`
(PRX, 30756791 B) · `49ab7a3a40ee15d3090a542a6e7150307b4d001a08aa3386de66c04b6981e3bf`
(loadlist, 446 B) · `9f3fa127…30f7f` (settings, 779 B) ·
`f291d552…a63f32` (MemberVariableLayout, 588 B) · `9c20f7c9…fbad86` (VTableLayout,
186 B) · `1d919fbf…5eca1e` (PS5Pad, 2960 B) · `f586043a…9658` (main.lua, 21415 B) ·
`e3b0c442…7852b855` (enabled.txt, 0 B) · `5a8f6609…c12215a7` (UEHelpers.lua, 9962 B);
`gaps: []` printed.
**Fail:** any hash differs from §A ⇒ STOP; that file drifted since the rehearsal —
re-run the rehearsal checks / re-emit before anything stages. `gaps` non-empty ⇒ do
not stage this PRX (packaging rule: a candidate whose run produced GAPS never stages).

## STEP 2 — Gate A0 write-probe + pre-image backup (2 min)

**Command (MASTER-STAGING-PACKAGE §B Step 0, rehearsal §6 #0/0b):**

```sh
# Gate A0 — FIRST, exactly once:
curl -s -T /etc/hostname "$FTP/data/modbridge/intake/.writeprobe" && \
  curl -s "$FTP/data/modbridge/intake/.writeprobe" | head -c 16
# Pre-image RETR + hash-log of everything this pass will overwrite:
curl -s "$FTP/data/plugins/ploader.ini" -o /tmp/ppsa33286-preimages/ploader.ini.pre || echo "no prior ploader.ini"
shasum -a 256 /tmp/ppsa33286-preimages/ploader.ini.pre 2>/dev/null || true
curl -s "$FTP/data/plugins/modbridge_supplier_composition.prx" -o /tmp/ppsa33286-preimages/prx.pre || echo "no prior module"
shasum -a 256 /tmp/ppsa33286-preimages/prx.pre 2>/dev/null || true
curl -s "$FTP$R/loadlist.txt" -o /tmp/ppsa33286-preimages/loadlist.pre || echo "no prior loadlist"
shasum -a 256 /tmp/ppsa33286-preimages/loadlist.pre 2>/dev/null || true
# On-console PRX backup ONLY after its pre-image hash is recorded:
curl -s -T /tmp/ppsa33286-preimages/prx.pre \
  "$FTP/data/plugins/modbridge_supplier_composition.$TAG.bak"
```

**Expected:** A0 roundtrips the hostname text (≤16 chars). Pre-image lines print
either a hash or "no prior …"; record all three hashes in the controller log.
**Fail:** A0 does not roundtrip ⇒ **hard STOP** — no evidenced write route exists
(rehearsal G1, the one open risk); report; do not improvise another route. Do NOT
re-run the probe (exactly once). A real failure later at STOR still surfaces here.

## STEP 3 — mkdir activation tree (1 min)

```sh
for d in $R/ue4ss $R/ue4ss/Mods $R/ue4ss/Mods/TownfallItemSpawner \
         $R/ue4ss/Mods/TownfallItemSpawner/Scripts $R/ue4ss/Mods/shared \
         $R/ue4ss/Mods/shared/UEHelpers ; do
  curl -s -Q "MKD $d" "$FTP/" || true
done
```

**Expected:** silence or 5xx on already-existing dirs (fine by design); real failures
surface at STEP 4's STOR. Never `mkdir` inside `roots/` beyond these — installer
generations own that tree; if the installer service is reachable, its Route A
(`worker-ue4ss-settings/STAGING-COMMANDS.md`) is preferred with the SAME bytes/hashes.
**Fail:** STEP 4 STOR failing with 5xx on these paths ⇒ re-check this step's dirs
before anything else; do not add directories ad hoc.

## STEP 4 — STOR all 9 files, order fixed (3 min)

Activation tree first (files 3–9), PRX second-to-last, loadlist LAST (mfa reads the
loadlist as commit truth — it must never precede its files; §B Step 2 verbatim):

```sh
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/UE4SS-settings.ini"   "$FTP$R/ue4ss/UE4SS-settings.ini"
curl -s --ftp-create-dirs -T "$T/worker-emit-final-pair/emission-final/MemberVariableLayout.ini"         "$FTP$R/ue4ss/MemberVariableLayout.ini"
curl -s --ftp-create-dirs -T "$T/worker-emit-final-pair/emission-final/VTableLayout.ini"                 "$FTP$R/ue4ss/VTableLayout.ini"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/PS5Pad.ini"           "$FTP$R/ue4ss/PS5Pad.ini"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua" "$FTP$R/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/enabled.txt"      "$FTP$R/ue4ss/Mods/TownfallItemSpawner/enabled.txt"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"       "$FTP$R/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"
curl -s -T "$T/worker-composition-3/prx-candidate-composition3-1/modbridge_supplier_composition.prx" "$FTP/data/plugins/modbridge_supplier_composition.prx"
curl -s -T "$T/worker-console-staging-bundle/loadlist.txt" "$FTP$R/loadlist.txt"
```

**Expected:** all nine commands return clean (30 MB PRX dominates the time). Nothing
else is ever uploaded: no `mods.txt`, no `UE4SS_Signatures/`, no `dlls/`, nothing
`ue4ss/` under `/data/plugins`, no runtime-created files. **Fail:** any STOR errors ⇒
fix that one file's path/dir and STOR it again once; do not restart the whole pass.

## STEP 5 — Readback-verify all 9 + Gate A1 decrypt oracle (3 min)

```sh
verify () { curl -s "$FTP$1" | shasum -a 256; }
verify "$R/ue4ss/UE4SS-settings.ini"                        # 9f3fa127…
verify "$R/ue4ss/MemberVariableLayout.ini"                  # f291d552…
verify "$R/ue4ss/VTableLayout.ini"                          # 9c20f7c9…
verify "$R/ue4ss/PS5Pad.ini"                                # 1d919fbf…
verify "$R/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua" # f586043a…
verify "$R/ue4ss/Mods/TownfallItemSpawner/enabled.txt"      # e3b0c442…
verify "$R/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"       # 5a8f6609…
verify "$R/loadlist.txt"                                    # 49ab7a3a…
curl -s "$FTP/data/plugins/modbridge_supplier_composition.prx" | shasum -a 256  # b8bfae77…
curl -s "$FTP/data/plugins/modbridge_supplier_composition.prx" | xxd -l 4       # MUST: 7f45 4c46
curl -s -Q "MDTM $R/loadlist.txt" "$FTP/"; curl -s -Q "MDTM /data/plugins/modbridge_supplier_composition.prx" "$FTP/"
```

**Expected:** every hash equals the STEP 1/§A value (rehearsal C14 verified these
comment pairs match the manifest verbatim); PRX readback first 4 bytes `7f 45 4c 46`
(inner ELF — ftpsrv decrypted the FSELF; Gate A1 PASS); MDTM shows fresh timestamps.
**Fail:** hash mismatch ⇒ halt, do NOT proceed to STEP 6; re-STOR that file ONCE,
re-verify; persisting mismatch ⇒ run the rollback (Fallback D) and report. PRX magic
returns `4f 15 3d 1d` ⇒ decrypt oracle broken ⇒ STOP staging anything further until
understood (STAGING-CHECKLIST §2–§4).

## STEP 6 — ploader.ini update (3 min)

Edit LOCALLY, never on-console. The section must read EXACTLY (§C):

```ini
[PPSA33286]
/data/plugins/modbridge_supplier_composition.prx = 2
```

```sh
# pre-image already RETR'd in STEP 2; on-console backup first:
curl -s -T /tmp/ppsa33286-preimages/ploader.ini.pre "$FTP/data/plugins/ploader.ini.$TAG.bak"
# apply the edit LOCALLY to the pre-image -> /tmp/ppsa33286-preimages/ploader.ini.new
#   (add the ONE line under the existing [PPSA33286] header; if it already exists,
#    change NOTHING and skip the STOR — the no-op path is correct)
curl -s -T /tmp/ppsa33286-preimages/ploader.ini.new "$FTP/data/plugins/ploader.ini"
curl -s "$FTP/data/plugins/ploader.ini"   # eyeball: §C shape, nothing else changed
```

**Expected:** readback shows exactly the pre-image plus the one mode-2 line (or
unchanged if it was already present). Mode `2` = LATE load; the SDK stamp 0x11000000
this candidate carries is what previously turned ESDKVERSION 0x80020063 into a real OK.
**Prohibited:** touching any other section/line, the frozen
`modbridge_runtime_aligned_entry.prx` entry, whitespace/comments/reordering, deleting
the header to "recreate" it. **Fail:** readback differs beyond the one line ⇒ restore
the `.pre` (Fallback D item 2) and redo the edit carefully.

## STEP 7 — Freshness reset (1 min, immediately before launch)

```sh
curl -s -Q "DELE /data/townfall_supplier_composition.log" "$FTP/" || true
curl -s "$FTP/data/plugins/UE4SS.log" -o /dev/null && curl -s -Q "DELE /data/plugins/UE4SS.log" "$FTP/" || true
curl -s "$FTP$R/runtime-status.txt" -o /dev/null && curl -s -Q "DELE $R/runtime-status.txt" "$FTP/" || true
```

**Expected:** three runtime-created files absent (or deleted) — without this, the
STEP 9/10 freshness oracle (mtime) is meaningless. Stale logs are not evidence.

## STEP 8 — Launch request to the user (exact message; STEP M) (4 min)

**Command (send to the user verbatim; the game LAUNCH is always a user action):**

> Promotion is signed and the build is staged with every hash verified. Please do
> exactly this, one launch only: (1) close Townfall fully — no suspended app;
> (2) arm the ploader payload (Payload Manager → `ploader.elf`), and tell me when
> armed; (3) launch Townfall normally; (4) about a minute in, the ploader surface
> should report "injecting 2 plugins" with OK/FAIL — tell me which; (5) after
> injection, reach the title screen and stay there about two minutes — do not load a
> save yet. I will then pull the evidence files. If anything pops FAIL or the game
> misbehaves, stop there and tell me what you saw — do not relaunch.

While waiting for "armed", re-send the mode-2 entry for THIS launch (per-launch rule,
§D step 3):

```sh
curl -s -T /tmp/ppsa33286-preimages/ploader.ini.new "$FTP/data/plugins/ploader.ini"
```

**Expected:** user reports armed → launched → injection OK ~1 minute in.
**Fail:** FAIL popup or no injection by ~2 min after launch ⇒ go to Fallback E
(collect ploader surface text + log tails; do NOT blind-relaunch — one changed
attempt max, per the launch ritual). ONE launch per staged build, always.

## STEP 9 — Evidence collection (T+2 min after launch) (4 min)

Fetch the three logs + mtimes into the console evidence path (§D step 5 / §E; RECORD
the FTP reads themselves in the receipt):

```sh
curl -s -Q "MDTM /data/townfall_supplier_composition.log" "$FTP/"; \
  curl -s "$FTP/data/townfall_supplier_composition.log" -o "$CONSOLE/townfall_supplier_composition.$TAG.log"
curl -s -Q "MDTM /data/plugins/UE4SS.log" "$FTP/" || true; \
  curl -s "$FTP/data/plugins/UE4SS.log" -o "$CONSOLE/UE4SS.$TAG.log" || echo "UE4SS.log absent"
curl -s -Q "MDTM $R/runtime-status.txt" "$FTP/" || true; \
  curl -s "$FTP$R/runtime-status.txt" -o "$CONSOLE/runtime-status.$TAG.txt" || echo "runtime-status.txt absent"
shasum -a 256 "$CONSOLE"/*.$TAG.*   # hash every collected artifact
```

**What to look for.**
- **Tier A — `townfall_supplier_composition.log`, fresh mtime (EXPECTED this
  launch):** `module_start_entered`, stack-alignment events, `constructor_count`,
  constructor begin/return pairs, then supplier/facts events (`sceKernelGetModuleInfo`).
  This is the ONLY evidence the composition-3 placeholder as linked can produce.
  Absent/stale ⇒ the module did NOT load this launch → Fallback E.
- **Tier B — `runtime-status.txt` + `/data/plugins/UE4SS.log` + Lua lines (NOT
  expected this launch; absence next to healthy Tier A is the documented WIRING
  state, not a runtime failure — the placeholder's symbol table has no
  `MakeRuntimeAckConfig`/`StartActivatedRuntimeWithAck`, rehearsal G3):** if present
  anyway, expected order: `modforge-runtime-status 1` → `session <pid> <ms>` →
  `activated` → `engine-ready` → `mods-started` (`failed <code> <detail>` otherwise;
  `stopped` at most once); UE4SS.log lines: `No specific game configuration found…`,
  `Config: /data/plugins/UE4SS-settings.ini`, `root directory: /data/plugins`,
  `working directory: /data/plugins`, `game executable: … (N bytes)`,
  `mods directories: [0] ue4ss/Mods`, then `Using engine version: 5.6`.
- **Always true:** if any staged file had been missing/size-mismatched, mfa refuses
  activation all-or-nothing (MFA_E_SIZE ⇒ game runs UNMODDED) — unmodded gameplay +
  Tier A log is a valid "staged-wrong" outcome, never a crash to blind-retry.
  A `failed` ack line, a missing UE4SS.log beside a healthy module log, or a
  layout-gate refusal detail ("Native activation requires both member and vtable
  layout files") are all honest negative evidence — triage via
  `worker-post-promotion-runtime/FAILURE-TRIAGE.md`.

## STEP 10 — Item Spawner evidence check (2 min)

```sh
grep -n "\[TownfallItemSpawner\]" "$CONSOLE"/*.$TAG.* || echo "NO MOD-TAG LINES"
grep -c "module_start_entered" "$CONSOLE/townfall_supplier_composition.$TAG.log"
```

**Expected this launch (honest):** `module_start_entered` count ≥ 1 (Tier A healthy)
and **zero** `[TownfallItemSpawner]` lines — the placeholder cannot start the UE4SS
mod stack; that absence is the wiring state, recorded as such. If Tier B artifacts
DID appear (composition-4 contingency was taken before STEP 1 — recorded with BOTH
PRX hashes per §C): the rung-4 phase-1 bar is the byte-exact line, exactly ONCE, in
the active Lua log: `[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7
opens the menu.` and NOTHING else mod-tagged at load time. A healthy UE4SS.log with
no mod lines ⇒ check `enabled.txt` (0 B, loadlisted) and the `mods-started` ack; the
nil-`FindFirstOf` F7 tail would indicate the rung-3 API gap, not a mod defect.
Never edit main.lua to make a line appear — the rung's claim is the UNCHANGED mod.

## STEP Z — Acceptance evaluation (3 min)

Run the acceptance-evidence rung checks (EVIDENCE-SPEC), honestly scoped to what this
launch can prove — the placeholder PRX cannot advance ANY rung, and the harmless-query
milestone (INTEGRATION-CHECKLIST STEP 14 / query-harness-integration REPORT §4 six
conditions) stays **unevaluated** until the recomposed (composition-4) PRX runs the
full chain. Write the verdict record:

```sh
cat > "$CONSOLE/acceptance-verdict-$TAG.md" <<'EOF'
Launch <TAG>, staged PRX sha b8bfae77… (composition-3 PLACEHOLDER).
Rung checks run (EVIDENCE-SPEC):
 RUNG 1 (game-thread callback): NOT EVALUABLE — needs recomposed PRX + harness dispatch
   counters; placeholder produces no C2/C3 channels.
 RUNG 2 (GetNumElements): NOT EVALUABLE — same precondition.
 RUNG 3 (reflected access): NOT EVALUABLE.
 RUNG 4 phase-1 pre-flight: staged pair present/listed (588+186 B, loadlisted),
   enabled.txt present, settings ini corrected (779 B), log freshness reset done.
   Observed: <Tier A present? yes/no; mod-tag line present? expected NO for placeholder>.
   Phase-1 PASS only if the IS:746 line appeared exactly once (composition-4 path).
 RUNG 5: NOT EVALUABLE.
Verdict: <NO RUNG ADVANCED (expected for placeholder) / RUNG 4 phase-1 PASSED/FAILED>.
Negative evidence recorded: <list what appeared instead — failed ack line, missing
 UE4SS.log, layout-gate refusal, unmodded run, …>.
Prohibited claims: "no crash" != success; host suites never advance rungs; no
 re-rating of unchanged evidence; milestone needs the STEP-14 six conditions.
EOF
```

**Expected:** a verdict file that advances nothing but records exactly what the
evidence shows, plus the STEP 9 artifacts and hashes — that is the honest terminal
state of this window. Any single unmet condition of a rung ⇒ milestone NOT passed;
unchanged evidence is never re-rated hoping for a different verdict.

---

## Fallbacks

**F1 — readback hash mismatch (STEP 5):** halt; re-STOR that one file once; re-verify.
Persisting ⇒ Fallback D + report. Never launch on an unverified tree.
**F2 — PRX readback `4f 15 3d 1d` (STEP 5):** decryption oracle broken ⇒ STOP all
staging; investigate ftpsrv before anything else in this window.
**F3 — Gate A0 no roundtrip (STEP 2):** hard STOP; no evidenced write route; report.
**F4 — Rollback (game CLOSED, ploader NOT armed; manifest §F):**
```sh
curl -s -Q "DELE $R/loadlist.txt" "$FTP/" || true    # master switch: game runs unmodded
curl -s -T /tmp/ppsa33286-preimages/ploader.ini.pre "$FTP/data/plugins/ploader.ini"  # or DELE if none prior
curl -s -Q "DELE /data/plugins/modbridge_supplier_composition.prx" "$FTP/" || true
curl -s -Q "DELE /data/plugins/modbridge_supplier_composition.$TAG.bak" "$FTP/" || true
# files 3–9: DELE each at its $R path, then RMD empty dirs bottom-up (Scripts,
# TownfallItemSpawner, UEHelpers, shared, Mods, ue4ss) — RMD only succeeds when empty
curl -s -Q "DELE /data/plugins/UE4SS.log" "$FTP/" || true
curl -s -Q "DELE $R/runtime-status.txt" "$FTP/" || true
curl -s -Q "DELE /data/townfall_supplier_composition.log" "$FTP/" || true
# verify: RETR listings of /data/plugins/ and $R/ — only pre-existing entries remain
```
If the tree was installer-committed, rollback goes through the installer's
uninstall/recovery journal — never manual DELE inside a committed generation.
**F5 — injection FAIL / no Tier A log (STEP 8–9):** collect ploader surface text +
any log tail via FTP; check Gate A1 magic and the ploader.ini entry; at most ONE
changed-implementation retry — never repeated blind relaunches (no duplicate
dispatch, no failed-job replay).
