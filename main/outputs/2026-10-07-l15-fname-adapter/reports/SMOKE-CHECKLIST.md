# L15 SMOKE-CHECKLIST — admission and native FName binding

Current rules override historical recipes below: NO console changes without explicit approval for that step; the user launches once; harvest before another launch. No game executable uploads or modified kernel modules. Executable downloads require SITE SELF decrypt-off. Rows A–M retain historical diagnostic wording; their suggested uploads/config edits are not authorization.

L15-specific success: fresh live prologue matched; ByteProperty pool ToString equals UTF16 input; slot 2 bound; FName binding shared-budget receipt says verified-and-bound; initialization completes without crash or Stopping mod. Menu/spawn remains a separate result. Refused signature/roundtrip is a controlled refusal, not successful engine binding.

Historical evidence: /tmp/l12-modreview/REVIEW.md (risk table), /tmp/l12-staging/ (l12 diffs + configs),
/tmp/l12-pad/REBIND.md (L3+R3), mission: outputs/2026-10-06-recovery-fakesign/PS5-MODDING-MISSION.md.
Test = ONE launch on a FRESH boot; connect klog (TCP 3232) BEFORE launch; captures to NEW files.

## 0. PRE-FLIGHT (do first — can void the test)
- /tmp/l12-staging/loadlist.new.txt declares `ue4ss/VTableLayout.ini 186` but the staged
  file is 216 B (contains `[UObject] ProcessEvent=0x268`). Activation enforces exact
  byte-size (MFA_E_SIZE -8) → a 216/186 pair REFUSES. On console (FTP 2121): row must read
  216 with the 216 B file, or the 0x268 fix is NOT live and outcome B below is guaranteed.
- /data/plugins/UE4SS-settings.ini has `[Seeds] PoolAdmission` (oracle|auto|shadow; default
  auto). Confirm not `oracle`.
- Shadow fixture present: /data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin
  (sha 3b6cfad4…) — rescue 2 needs it.

## 1. OBSERVE (user)
- BOOT: game reaches its own menu / in-game | crash dialog (photograph code verbatim;
  0xa0020102 / auth-info-46 = unsigned-module class, see POSTMORTEM) | black screen hang.
- IN-GAME, load a save first. Chord = hold L3, tap R3 (edge-triggered, no simultaneity
  needed). OPTIONS is now game-pause ONLY (rebind shipped in same bundle).
- Menu: appears + D-pad UP/DOWN nav, LEFT/RIGHT page, CROSS select | nothing | game pause.
- Popups / on-screen text: copy VERBATIM (e.g. in-menu status "Load a save, then open with
  F7.", "Could not build pickup list. Load a save first.").
- Spawn: pickup visibly appears at aim point after CROSS.

## 2. HARVEST (zcode) — console FTP ftp://ps5-host-not-configured.invalid:2121
- /data/plugins/UE4SS.log → `grep -E "\[Seeds\]|\[L12\]|\[R3\]|TownfallItemSpawner|Stopping mod|0x80920004"`
- /data/modbridge/roots/PPSA33286/runtime-status.txt → `state 1 activated …` / `state 2 failed <0|1> <reason>`
- /data/modbridge-supplier-diag.jsonl (tail) → probe tags: `pool_copyin_probe_rc/_errno/_sink`,
  `pool_copyin_probe_fault_off` (real EFAULT, rel. offset), `pool_copyin_probe_stop` (== len
  on completion), `pool_probe_op/_rc/_pagealign` (short-write loop); pad tags:
  `pad_input_deferred`, `pad_input_deferred_error` (0x80920004 = host holds exclusive pad).
- klog TCP 3232 (connected pre-launch).
- Key line shapes:
  - `[Seeds][L12] pool admission retry armed: 30 rounds x 5s` then per-round
    `round {n}/30 exhausted: CurrentBlock … Blocks[0] 0x… class … oracle-note: …`
  - Admission sources, pick ONE: `source: span-membership` | `copyin-owned-read` |
    `… admitted via VirtualQuery coverage … source: sceKernelVirtualQuery` |
    `admitted via shadow corroboration (image sha256 verified, slide 0x…; cross-check vs
    pinned 0x9765EC0: MATCH; source: shadow-corroboration; oracle was: unavailable)`
  - Unified success: `[Seeds] FName pool validated at … source: <X>; 'None' anchor ok; …`
  - R3Marshal: `[R3] FuncPtr-marshal armed on carrier <fn>` / `event carrier armed: <fn>` /
    `marshal is the game-thread pump…`; FAILURE: `no ranked candidate passed the carrier
    precheck; closures will queue until a carrier arms`, `carrier candidate not found`,
    `ExecuteInGameThread: … carrier is not armed yet`.
  - Mod: `Loaded v1.0. Clean pickup-only release. F7 opens the menu.` | `Menu created.` |
    `UI creation failed: <err>` | `Failed at <player|ground trace|load pickup class|spawn
    actor|configure pickup>. <err>` | `Spawned <name> xN ID=<id>` | `Stopping mod '…'`.

## 3. INTERPRETATION TABLE (risk rank per REVIEW.md: 1 VTable 0x268, 2 R3 carrier, 3 pad handle, 4 first reflected-call marshal)
| # | Observed combo | Diagnosis | Next action (owner) |
|---|---|---|---|
| A | Admitted(any) + menu Y + spawn Y | GOAL MET | Announce per §5; save all captures |
| B | Admitted + `Menu created.` logged + NOTHING on screen | ProcessEvent stub slot 0x260 — VTableLayout fix not live (186/186 old file; see PRE-FLIGHT) | Verify on-console ini size/sha; re-upload 216 B pair with matching loadlist row (fix staged by worker-gap-l2a, sha 37a7d860…) |
| C | Admitted + chord yields NO new log lines | Input path dead. R3 armed + pad_input_deferred_error 0x80920004 forever → pad (rank 3). No `[R3] … armed` line → carrier never armed (rank 2) | Pad: worker-pad-coexist deferred-attach; consider single-button fallback (TOUCH_PAD) next build. Carrier: worker-l11-merge LuaMod DIFF 1 — check `carrier candidate not found` names |
| D | Admitted + `UI creation failed: <err>` | First reflected-call marshal failure in construct/UMG path (rank 4) | Capture err verbatim; LuaUObject unit + REVIEW §(c) owners |
| E | Admitted + menu Y + `Failed at ground trace` | LineTraceSingle/FHitResult OUT-marshal (rank 4; statics only, FHIT-VERDICT) | worker-gap-l2a readback path with live err text |
| F | Admitted + menu Y + `Failed at player` / "Load a save" | No pawn — not in-game yet | User loads save, retest (no code) |
| G | Admitted + menu Y + `Failed at load pickup class` | LoadAsset/AssetRegistry game-thread path | LuaMod.cpp:3648 path; needs IsInGameThread — retry in-level |
| H | Admitted via shadow-corroboration + crash right after admission | Blocks[0] actually unreadable — shadow admitted without coverage proof (never re-run) | Oracle/kernel read path (mission §5 kstuff-lite revival); quarantine shadow admit |
| I | Rounds exhausted + oracle-note "unavailable (sink-open/policy/partial/vacuous-sink)" + `shadow corroboration failed: cannot read decrypted eboot` | Oracle still down AND fixture missing | Restore eboot.bin (sha 3b6cfad4), relaunch once |
| J | `fallback declined: oracle resolution was REFUSED` and/or `pool_copyin_probe_fault_off` tag | NEW: real kernel evidence range unreadable (SCE-errno normalization reclassified EFAULT) — not oracle-down | Seed-pin audit (worker-code-scan); do not bypass |
| K | `VirtualQuery HARD NEGATIVE on Blocks[0]` | Real mapping evidence against 0x22C0000000 — pool moved | Re-pin pool; l12-pincheck/preflight.py |
| L | `shadow arm refused: <status>` or `cross-check … DIFFERS` | Image/slide drift (game update?) | Re-run pin preflight; verify sha/slide gates |
| M | `fallback considered (mode=oracle …)` | Settings switch not applied on console | Set [Seeds] PoolAdmission=auto in /data/plugins/UE4SS-settings.ini, relaunch |
| N | `Unreal initialization startup deadline exceeded` (runtime-status `state 2 failed 1`) before any fallback/rescue log; no `admitted via` and no `[Seeds] REFUTED` | Owner startup deadline expired before rescue paths ran; this is neither a completed pool refusal nor evidence that VirtualQuery/shadow rescue failed. Files remaining in Mods do not prove an active runtime. | Preserve the last round/timestamps and deadline reason; inspect the local admission budget and rescue ordering against the owner deadline. Keep cancellation/deadline propagation authoritative; do not extend/bypass the deadline or infer admission. Any later console test remains separately authorized. |
| O | Pool admitted/validated, then startup faults inside a direct engine call; klog identifies ABI argument/output mismatch | Engine-call ABI mismatch after admission; pool readability success does not establish callable ABI | Preserve registers/backtrace, decode exact pinned callee/caller ABI; fix locally with output/UTF16 tests, live prologue match and pool roundtrip; console upload requires separate approval. |

## 4. REVERT PLAN — approval required

The future approved upload must first save a fresh decrypt-off backup of the live l14 PRX (expected SHA 769284e3a713f741b51ca2c5da64e654db0fb93e6b725511488600bc35ac8444, SELF magic4f153d1d). Restore only that owned /data/plugins PRX if separately authorized. Restoring l14 reverses the code change; l14 itself crashed in the previous test, so this is not permission to relaunch it or a claim that it is safe. Any loader disable/config recovery step needs explicit approval. No game executable, kstuff, autoload, ini or loadlist changes are part of this build. After a crash, preserve logs and stop testing.

## 5. SUCCESS DEFINITION (announce exactly this)
- User-visible: L3+R3 opens overlay menu; D-pad navigates; CROSS spawns a visible pickup.
- Log chain, in order: `[Seeds][L12] pool admission retry armed` → admission line naming the
  SOURCE (`admitted via VirtualQuery coverage` / `admitted via shadow corroboration …
  cross-check … MATCH` / span/copyin) → `[Seeds] FName pool validated at 0x9765EC0
  (CurrentBlock 82, Blocks[0] 0x22C0000000; source: <X>; 'None' anchor ok; …)` →
  `[TownfallItemSpawner] Loaded v1.0 …` → `[R3] FuncPtr-marshal [event ]carrier armed: <fn>`
  → (L3+R3) `Menu created.` → (CROSS) `Spawned <name> xN ID=<id>`.
- runtime-status.txt: `state 2 failed 0`, no `Stopping mod` line in UE4SS.log.
- Announce the admission source (evidence strength: span-membership < copyin/virtual-query <
  shadow-corroboration is NOT the ordering — span/copyin are direct coverage, shadow is
  image-corroboration only; say which one fired).
