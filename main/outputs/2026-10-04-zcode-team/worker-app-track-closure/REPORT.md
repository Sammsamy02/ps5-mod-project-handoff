# REPORT.md — worker-app-track-closure

Origin: worker-app-track-closure, 2026-10-04. Read-only synthesis + spec. No
console contact, no network, no nested subagents, no code changes anywhere
outside this worker dir. Every claim below is tied to a cited receipt; nothing
in the delivered spec has executed on a console.

## 1. Result

The app track's console-session closure is now SPECIFIED as a ride-along on
the already-planned return window, with no new console mechanics invented:

| Deliverable | Path | Status |
|---|---|---|
| Claim (first) | `CLAIM.json` | written before any work |
| App-session integration spec | `APP-SESSION-INTEGRATION.md` | exact slot, drive protocol, live-cookie needs, ordering laws |
| End-user flow spec | `APP-SESSION-INTEGRATION.md` §4 | install zip -> staged commands -> activation root -> next launch |
| Honest gaps table | this report §3 | what stays host-only until which console step |

Key synthesized decisions (each reconciled against on-disk evidence):

1. **The app drive rides between the batch tool's `e2e` stage and
   `stage-ridealongs`, in the same game-CLOSED payload-only window** — after
   `e2e` because that stage ends with the §7 full-tree reset of
   `/data/modbridge/**` (and leaves the `MODBRIDGE_INST_E2E` alias staged, so
   no new payload upload); before `stage-settings` because the v3 restage
   freeze-checks the loadlist against a recorded inventory and STORs a FIXED
   447 B pair — an app-authored loadlist must not be silently frozen over or
   silently clobbered.
2. **One required app-side change, specified but NOT applied**: the app's
   `PAYLOAD_IDENTITY` pin (`app_core/modmgr_api.py`) still names the D1 MODE-A
   payload (`e3489297…`/681528), while the batch tool's `mode_gate` runs cmd
   files ONLY against the MODE=B config-entry payload (`a21771f8…`/681888) and
   explicitly STOPs cmd files against D1. Re-pin + re-run 42/42 + 52 UI checks
   before anything is queued (APP-SESSION-INTEGRATION.md §2 P0).
3. **The UI's live-cookie need is already satisfied by the fixture build**:
   the runnable binary (`worker-app-ui-flows/build/final/...`, sha `a677e7b2…`
   re-verified on disk) has the fixture OAuth config compiled in
   (`oauth_config.h`: `MB_HOST_OAUTH_CONFIGURED 1`, client_id
   `owned_native_jobs_client`, loopback resolves, fixture CA, port 62283
   redirect) — real session admission (mb_session cookie + CSRF + Origin), zero
   provider contact. The REAL client_id is user-only and costs exactly one
   header regeneration + rebuild.
4. **The launch picks the app mod up only through the loadlist restage law** —
   and the v3 pair is a fixed recording, so this window's honest claim for the
   app drive is the console EFFECT (records + root + transcript), with the
   merged loadlist pair as an explicit follow-on recipe (§5 of the spec), never
   a silent expectation.

## 2. Inputs consumed (read-only)

worker-app-modmanager (CONSOLE-CONTRACT.md §2-§7, APP-STATE.md, REPORT.md) ·
worker-app-backend-integration (REPORT.md §2-§5; vendored
`app_core/modmgr_api.py`: PAYLOAD_IDENTITY, `stage_commands` slot format,
`queue`/`transcript` ops) · worker-app-download-runtime (DOWNLOAD-RUNTIME.md
§2-§4) · worker-app-ui-flows (UI-FLOW-RESULTS.md, FIXES.md; `build/final`
binary sha re-hashed; `vendored/bin/modinst_composed` sha re-hashed) ·
worker-console-batch-runner + worker-batch-final-ini-adoption
(USER-SESSION-GUIDE.md; `console_batch.py` `mode_gate`, `cmd_gone_ok`, §7
reset, 15 s settle) · worker-return-session-master-plan (phase order, dispatch
laws, §7/freeze/lockstep laws) · worker-e2e-suite-completion (SUITE-RESULTS.md:
348 host checks on the build-r1 chain; console dispatch [TARGET]) ·
worker-zip-bridge-console-link (bridge TU console-linkable; console download
stage still open) · `fixture-config-1/oauth-config.json` (identity fields
only).

## 3. Honest gaps table — what stays host-only until which console step

| # | Capability | Host truth today | Blocked on | Unblocks at |
|---|---|---|---|---|
| G1 | Mutating endpoints' real console effect (install/enable/disable/uninstall) | Host-proven: 42/42 API + 52 UI checks; every mutation also stages gated console commands (`console_executed:false`) | Payload dispatch session consuming the staged commands per drive protocol | The app-drive slot (spec §3.1) in the return window |
| G2 | Payload identity agreement for app commands | Manifests pin the stale MODE-A payload `e3489297…`; console window pins MODE=B `a21771f8…` | ONE re-pin in `modmgr_api.py` + regression re-run (host-side, P0) | Pre-window prep; no console dependency |
| G3 | Real OAuth (Nexus) | Fixture OAuth compiled in: real cookie/CSRF admission, loopback-only, no provider contact | User-only client_id/scope/CA + one `oauth_config.h` regeneration + rebuild | User supplies credentials; then the same binary serves real sessions |
| G4 | In-app Nexus download (browse->download through the UI) | Session runtime linked and starts (real entropy); catalog read-only; `/api/downloads` 503, `/api/jobs` 404 — jobs/archive composition NOT built | (a) jobs/archive composition build (`--archive-profile/--job-directory/--oauth-config`; retained inputs documented in `download-runtime-profile.json`); (b) G3 | Host rebuild + user client_id — NOT this window |
| G5 | Archive intake on console | Host intake only; server enforces containment (no raw upload by design) | Controller FTP transfer of the zip (`intake/<id>.zip`, sha-readback) | Spec §3.2 step 1 (this window) |
| G6 | App mod actually LOADING at the next launch | Console root update via drive protocol is spec'd; the v3 settings+loadlist pair is a FIXED recording without app rows | Merged pair re-authored + inventory re-recorded + lockstep restage (spec §5) | Follow-on window step after a green app drive |
| G7 | `running:true` for any mod | Ack-derived only (`simulate-runtime` host proofs; e2e stages 4-5 labeled SIMULATED) | Real in-game `mods-started` ack | The game session's CP0-CP4 evidence — never claimed from the app |
| G8 | MODE=B config-entry payload on console | Built (`a21771f8…`/681888) + 348 host checks 0 failed (bridge75/fr2-87/fault-186/config-entry) | Its own staged dispatch (the batch `e2e` stage, 15-dispatch cycle) | The same window, immediately before the app drive |
| G9 | Console download stage (RDWR) | Link-proven only (worker-ps5io-rdwr); console payload has no download command | ps5io RDWR console closure (integration queue item) | Separate console step; not this window |
| G10 | UI visual quality | Real app.js executed against a live server (DOM-structure assertions); never seen by a human | Real browser pass | Unrated — log lines/pixel claims never substitute |
| G11 | Console transcript pairing against a live payload | `E2eTranscript` proven on host transcripts (F-R2 lineage 87/87); `console_executed:false` everywhere | First real dispatch | The app drive's per-command step 5 (spec §3.2) |

## 4. Honest limits of this synthesis

- Everything is paper+receipts: the app drive protocol composes ONLY rituals
  already host-proven somewhere (modmgr byte-identical command emitter,
  batch-tool gates/laws, stage-settings lockstep law), but the composition
  itself has never run — on a console OR end-to-end on the host with a real
  FTP server.
- The §3.1 slot is my recommendation reconciling three recorded laws (§7
  reset, settings freeze, no-interleave). The controller owns the final call;
  the spec flags every collision honestly rather than picking silently.
- The P0 re-pin is the only identified app-code delta; I changed nothing.
- I did not re-verify any worker's numbers beyond re-hashing the two binaries
  named in §1; all other figures are quoted from the cited receipts.

## 5. Bounds

~22 tool calls (target 25-45). CLAIM.json first, REPORT.md last. Writes only
under `outputs/2026-10-04-zcode-team/worker-app-track-closure/`. No console
contact, no provider contact, no credentials touched (fixture client_id named
as a build identity only).
