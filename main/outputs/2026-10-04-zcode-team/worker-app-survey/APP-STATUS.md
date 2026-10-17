# APP-STATUS.md — the app / mod-manager ecosystem, surveyed 2026-10-06

Origin: worker-app-survey. Read-only survey of the app/backend worker lineage
under `outputs/2026-10-04-zcode-team/`, the live app tree
`outputs/ps5-mod-bridge/native-host/`, the runtime port, and the console-state
workers. Every claim below cites its owning worker's receipt or a file:line
checked this session. Nothing here ran on a console; no game, runtime, or
"all mods work" claims are made or implied.

Question answered: the user wants the broader mod ecosystem to work ("make all
mods work"), not just the Item Spawner. What exists, what works end-to-end,
and what is between here and a PS5 user browsing/installing/enabling mods?

---

## 0. Executive summary

The app track is **host-complete and host-proven through the whole management
loop** — web UI -> HTTP backend -> mod-manager core (list / install zip / enable
/ disable / uninstall) -> proven installer binary -> records + loadlist root ->
gated console command files. It has **never touched the console**: zero staged
commands consumed, zero app builds run on the PS5, `console_executed:false`
everywhere. The only mod that has ever been staged for the runtime is the
hand-built TownfallItemSpawner loadlist. The PS5 user experience today is:
a desktop web UI on a Mac, plus a controller-run FTP/HTTP batch tool. The
on-console app (PS5 payload serving the same UI on :8085, browser opened by
`native-app-launcher`) compiles/links but has never executed.

Separately, the **runtime composition module itself is in a loading regression**
(the ~30.8 MB module rejected by `sceKernelLoadStartModule` in ~85 ms on warm
boots; cold-boot discriminator unrun — worker-boot-protocol). Until that
closes, no mod experience can exist on console regardless of the app track.

---

## 1. The product shape (as evidenced)

```
HOST (today: a Mac)                              CONSOLE (PS5, fw 13.60)
  web UI (/app: Games/Activity/Installed)          /data/modbridge/{intake,state,roots}
    |  session cookie + CSRF + Origin                payload ELF installer (MODE B)
    v                                                consumes e2e-cmd.json command files
  HTTP backend (C, http.c, port 8085)                -> journaled root swap ->
    |  /api/installed, /api/install,                    /data/modbridge/roots/PPSA33286/
    |  /api/mods/<id>/enable|disable, DELETE            loadlist.txt written LAST
    |  /api/mod-service (mfw8), /api/jobs,           runtime composition module
    |  /api/downloads, Nexus auth/catalog              mfa_activate() -> NativeModSource
    v                                                  -> UE4SS Lua VMs -> mods-started ack
  modmgr_api.py driver (fork/exec; flock)
    v
  modinst_composed (sha-pinned C99 installer)
    records.txt + state/bundles/<id>.mfb + roots/<TITLE>/
    ^_____________________ console drive _____________________|
        staged console-queue command files + manifest.jsonl,
        transferred/consumed today ONLY by the controller-run
        batch tool (console_batch.py over FTP 2121 / pldmgr 8084)
```

Console services actually present (read-only probe, worker-boot-protocol §1.1):
pldmgr payload manager on :8084, ftpsrv on :2121, websrv (autoload),
etaHEN 13.60 data present (OnionHEN is the autoloaded HEN). The ModBridge app
is NOT among the autoloaded payloads — it has never run there.

---

## 2. What is built, by worker

| Worker | Delivered | Proof | Status |
|---|---|---|---|
| worker-8-app-backend | Audit of the pre-existing app (13 route groups, read-only mod service, `GET /api/installed` missing while `web/app.js:935` calls it); drop-in C backend `mfw8_records/mfw8_actions/mfw8_caps/mfw8_service` mapping the app's records surface onto the proven `records.txt` store; PATCHES.md diffs to wire it in | 61/61 host tests vs the real `modinst_composed` + a real corpus zip | Superseded/integrated (patches applied by worker-app-backend-integration) |
| worker-app-modmanager | The mod-manager core loop as app logic: `app/modmgr.py` (list with honest enabled projection, install zip/mfb preserving the full desired set, guarded enable/disable as loadlist row edits with bundle-index attribution + backups + records-sha resync, uninstall via F-R2 removal), `app/console_link.py` (command-file emitter + transcript validator), CONSOLE-CONTRACT.md (9 safety gates) | 18/18 host tests; emitter reproduces all 13 pinned staged command files byte-identically (13/13) | Proven (host); vendored verbatim downstream |
| worker-app-backend-integration | Applied worker-8's patches into a vendored app tree; new HTTP endpoints `GET /api/installed`, `POST /api/install`, `POST /api/mods/<id>/enable|disable`, `DELETE /api/mods/<id>` driving modmgr via fork/exec driver `modmgr_api.py` (flock-serialized); every successful mutation also stages gated console command files | 42/42 offline API checks; full app build clean; mutations verified through the installer binary's own reader | Proven (host) |
| worker-app-download-runtime | Recovered the "lost" download-runtime build recipe; rebuilt the curl/MbedTLS download composition host-side; recorded the profile permanently (`receipts/download-runtime-profile.json`) | 42/42 on the download-gated compile; live A/B probe (`/api/downloads` 503 vs 404); `/report` shows the curl owner genuinely starts | Proven (host); live-cookie HTTP still unproven (fixture-only) |
| worker-app-ui-flows | Fixed the delivered binary hard-503ing modmgr routes (no auth runtime -> fixture-OAuth rebuild); fixed a real `-Os` scope bug (`resolved[]` escaping its block) that emptied `--file` on every release install; added the missing UI controls (install panel, per-row enable/disable, two-step uninstall) | Level 1 (real server + real session) 34/34; Level 2 (real served app.js in Node DOM stub) 18/18; binary `a677e7b2…` | Proven (host, no browser/pixels — DOM-structure only) |
| worker-app-payload-repin | P0 from track-closure: re-pinned `PAYLOAD_IDENTITY` from the stale MODE-A payload (`e3489297…`) to the MODE=B config-entry payload (`a21771f8…`/681888) in all three owned `modmgr_api.py` copies, so staged manifests agree with the batch tool's `mode_gate` | 42/42 API + 52/52 UI re-green; binary rebuilt byte-identical (pin lives in the runtime driver) | Applied and proven (host) |
| worker-app-track-closure | Read-only synthesis: APP-SESSION-INTEGRATION.md — the exact console-session slot, per-command drive protocol, end-user flow spec (§4), loadlist-merge follow-on (§5), honest gaps table G1–G11 | Paper + receipts; composition never executed anywhere | Spec only |

Supporting proven stack the app delegates to (not re-derived): worker-7-installer
(transactional installer, records.txt, exit contract 0–5, 108/108),
worker-zip-to-mfb (zip->mfb auto-bridge, 75/75), worker-f-r2-fix (composed
binary `f2215f2f…` + transcript pairing, 87/87), worker-e2e-staging-final
(13 pinned command files), worker-loadlist-final (final Item-Spawner loadlist,
45/45 through `mfa_activate`).

---

## 3. What a "mod" is on this system (the infrastructure a manager manages)

**Install unit.** A downloaded archive (Nexus zip) is quarantined, classified
member-by-member, and composed into a deterministic bundle
`state/bundles/<id>.mfb` (worker-zip-to-mfb BRIDGE-DESIGN.md §3; first rule
wins): `ue4ss/Mods/...` -> role `lua_mod`/`config`; bare `Mods/<X>/...` or a
single wrapper dir `<X>/Scripts/**` -> same, repointed under `ue4ss/Mods/`;
`.../Content/Paks/<tail>` -> role `pak`/`asset_override` at `mods/<tail>`;
`.ini` -> `config`. Everything else (readmes, DLLs, screenshots) is skipped as
non-installable metadata. Refusals are specific (no recognized content,
duplicate final path, >256 members, >64 MiB member bytes).

**State stores** (worker-app-modmanager APP-STATE.md §3, pinned by running the
real binary):
- `records.txt` (`modforge-records\t1`): `mod <id> <title> <state> <bundle>`,
  `game <title> <engine> <sdk> <generation> <loadlist-sha>`. States
  `downloaded -> prepared -> installed`; backward on uninstall; `running` is
  never stored — derived only from `runtime-status.txt` ack lines.
- `roots/<TITLE>/` installed root with `.modforge-root` ownership marker;
  `loadlist.txt` written LAST; the game row's sha is the installer's commit
  truth.
- `loadlist.txt` (`modforge-loadlist\t1`): `title/engine/sdk/framework/mod/file/
  order` TAB rows; `mod` rows are the enabled set + load order; `file` rows are
  `role relpath size` preloaded byte-exact (sizes verified at activation,
  `MFA_E_SIZE` on drift).

**Runtime consumption** (port source `src/Platform/NativeModSource.cpp:98-180`,
checked this session):
- Lua mods are enumerated from the preloaded snapshot: any
  `ue4ss/Mods/<Name>/Scripts*/.../main.lua` (case-insensitive Scripts/scripts;
  `shared` reserved for libraries). Max 4096 files, 8 MiB/file, 64 MiB total
  snapshot.
- The startup list is UE4SS-standard: `ue4ss/Mods/mods.txt` `Name:1` rows
  first, then per-mod `enabled.txt` presence markers. `enabled.txt` is EMPTY
  and presence-only — the runtime-disable mechanism is the mod's absence from
  the loadlist, which is exactly why app disable = guarded loadlist row strip
  (worker-loadlist-final T4; worker-app-modmanager §2).
- The Lua VM + `main.lua` execution happen on the loader thread during
  `module_start`, gated on nothing (worker-lua-bootstrap-audit §1-2); the
  `mods-started` ack is written on the init thread after engine-ready.
- Preload roles default to `lua_mod`, `config`, `script`
  (`modforge_activation.c:281`) — **`pak`/`asset_override` files install into
  the root but are not preloaded and have no proven runtime mount path** (see §4).
- C++ mods are a separate contract: `ue4ss/Mods/<Name>/dlls/main.prx` specs
  with exact path/size validation (NativeModSource ctor), started after Lua
  mods via `start_cpp_mods`.

**Console drive protocol** (CONSOLE-CONTRACT.md; worker-app-track-closure §3.2):
every app mutation stages the console-reproducing command file(s)
(`register-mod` / `prepare-archive` / `install --mods <full desired set>` /
`install --remove`) with a sha-bound manifest row carrying the payload identity
pin. A controller transfers each file to `intake/e2e-cmd.json`, dispatches the
payload once per command, and verifies the `.consumed` twin + transcript
pairing + records state. This protocol has never run against a live payload.

---

## 4. Coverage vs "make all mods work" (honest per-family state)

| Mod family | Install pipeline | Runtime path | State |
|---|---|---|---|
| UE4SS Lua mods | Proven (zip->mfb rules L/M/W; Item Spawner end-to-end staged) | Proven code path (enumeration + startup list + Lua 5.4 VM, host smoke 90/90; static mods-started audit) | The ONLY family with a complete chain; exactly one mod (TownfallItemSpawner) ever staged |
| PAK / asset_override (`~mods` paks, .ucas/.utoc) | Install-proven (rule P) | **No runtime path**: not a preload role, no mount/redirect implementation; NativeModSource's 8 MiB/file + 64 MiB snapshot caps exclude most paks anyway | Install-only; gameplay effect unproven. (Historical user reports of asset/camera mods working predate this pipeline and were manually staged — ZCODE-HANDOFF VERIFIED_VS_MISSING #4) |
| INI/config mods | Proven (rule I) | Preloaded as `config` | Complete for the roles the runtime reads (settings, layouts, pad); the v3 lockstep restage law governs them |
| UE4SS C++ mods (`dlls/main.prx`) | Not in the zip bridge (DLL members skipped as metadata) | Native cppmod contract exists (start_mod/uninstall_mod ABI, eh-frame, child lifetime) | Contract + packaging recipe only; no downloaded C++ mod has ever run |
| Windows DLL mods (proxy hooks: P3RFix/FF7RFix/ReShade shape) | Acquisition designed, blocked on user-only real OAuth (worker-dll-mod-acquire) | 8-family Windows-services surface: 6 families console-proven inside ALLFAM runs; Static TLS + SEH64 host-validated, first proof rides the dll8 candidate `79281191…` (worker-dll-track-closure) | Injection-surface primitives only. "No arbitrary DLL->PRX binary converter or complete Windows environment exists" (handoff, DLL CRITICAL GAP). Constrained DLL translation is its own research deliverable (console-only roadmap) |
| FOMOD / variant / choice mods | `install.plan`, `game.register`, FOMO choices: `unknown_method` (worker-8 REPORT §5) | — | Unimplemented; multi-mod archive prepare refused (one archive per download id) |
| Other engines (me3, Unity IL2CPP, ReShade presets as such) | Not started | — | Roadmap families only (console-only roadmap); no implementation claimed |

The app's honest capability vocabulary (console-only roadmap) already models
this: Supported / Needs conversion / Unsupported / Failed, with blockers
stated. The classifer it needs to refuse unknown-family mods accurately exists
as design + caps.tsv passthrough, not as a populated evidence base.

---

## 5. The user experience today (surfaces)

1. **Desktop web UI (works today on a Mac).** The rebuilt fixture-OAuth binary
   (`worker-app-ui-flows/build/final/…`, `a677e7b2…`) serves `/app` with
   Games/Activity/Installed tabs; install-from-archive panel, per-row
   enable/disable, two-step uninstall; real cookie/CSRF/Origin admission. It
   manages a local fixture tree through the real installer. Real Nexus
   sign-in needs the user's own OAuth client_id (one `oauth_config.h`
   regeneration + rebuild, G3); in-UI browse->download needs the jobs/archive
   composition build that is documented but not in the runnable binary (G4) —
   today a zip reaches intake by being placed on disk.
2. **On-console app (planned, never executed).** `build.py` has native
   profiles (`loopback`, `development_lan` port 8085) using ps5-payload-dev
   SDK; PS5 app builds compile/link with no host headers, but the configured
   Nexus/download composition "has not run on the PS5" (native-host README).
   `native-app-launcher` (compiled, never dispatched) is a small payload that
   asks the PS5 browser to open `http://127.0.0.1:8085/app`; home-screen
   entry, lifetime handling, and compose-with-server are explicitly
   unstarted. **Hard port blocker:** the modmgr loop is a Python driver
   fork/exec'd by the C server — a PS5 payload process cannot fork/exec
   Python; worker-8 named binding `mfw8_actions` to a console process model
   as the integration step, and it has not been done (worker-8 REPORT §5,
   worker-app-backend-integration §5).
3. **Controller batch tool (what actually touches the console today).**
   `console_batch.py` (worker-console-batch-runner + worker-batch-final-ini-adoption)
   stages payloads, the 8-entry ploader.ini, and the FIXED v3
   settings+loadlist pair over FTP/HTTP, gated by readback laws. It is the
   de-facto mod installer for the one staged mod; the app's staged command
   queue is designed to ride the same session (APP-SESSION-INTEGRATION §3.1,
   between `e2e` and `stage-settings` in a game-CLOSED window).

---

## 6. What works end-to-end, honestly

**Host-proven end-to-end (all receipts green):** UI click -> session admission
-> HTTP mutation -> Python driver -> pinned installer binary -> records +
generation + loadlist sha committed and read back through the binary's own
reader -> UI projection refresh -> console command files staged with the
correct MODE-B payload pin. 42/42 + 52/52 + 61/61 + 18/18 across the chain.

**Console-proven (runtime side, unrelated to the app):** the staged
Item-Spawner package activates through `mfa_activate` on host (45/45), an
aligned-entry module has run inside the game with 232 constructors, seed lines
(`g_uobject_array`, `gmalloc`) appear in UE4SS.log, and `runtime-status.txt`
reaches `activated` on console (ZCODE-HANDOFF #1; worker-boot-protocol §1.5).
User-reported asset/camera mod successes exist from earlier manual staging.

**Never done anywhere:** a console consuming an app-staged command
(`console_executed:false` everywhere); an app-installed mod loading at launch
(the v3 loadlist pair is a fixed recording without app rows — G6); the app
process running on the PS5; a real Nexus round trip through the UI
(`provider_contact:false`; real client_id is user-only); UE4SS in-game
`mods-started` ack on console; and — the live blocker — the current ~30.8 MB
composition module loading reliably (warm-boot regression, cold-boot
discriminator unrun, worker-boot-protocol §2.4).

---

## 7. Gap analysis — top 3

1. **Console execution of the whole app vertical (G1+G6+G11).** The chain
   ends at staged command files. Nothing has ever consumed them; and even
   after a successful drive, the fixed v3 loadlist recording means an
   app-installed mod still would not load at the next launch until the
   settings+loadlist pair is re-authored to include app rows (track-closure
   §5 recipe exists, unexecuted).
2. **No on-console manager process (UX gap).** A PS5 user today has zero UI:
   the manager runs on a Mac and the console is driven by a controller-run
   batch tool. The PS5 app build exists but (a) has never executed, (b) lacks
   any home-screen entry/lifetime story beyond the browser-opening launcher
   payload, and (c) cannot run the Python modmgr driver in a payload process
   — the C `mfw8_actions` path must be bound in-process first.
3. **Family coverage beyond Lua (the "all mods" gap).** Lua mods are the only
   complete family. PAK/asset mods install but have no runtime load path and
   exceed the snapshot budget; C++/DLL mods have primitive surfaces but no
   downloaded-mod result; FOMOD/variants are refused; in-UI download needs the
   jobs/archive build + real OAuth. The manager can only honestly say
   "Supported" for UE4SS Lua + config mods on this one title.

(Honorable mention, global blocker: the composition loading regression. It is
the runtime track's top item, not the app track's, but no app milestone is
observable until it closes.)

---

## 8. Top 5 concrete next steps (ranked by impact)

1. **Close the composition loading regression** (controller/runtime track:
   worker-boot-protocol §3 — LAUNCH-A rc capture, then outcome-keyed cold boot;
   worker-runbook-v2 vocabulary for the next candidate). Impact: everything
   else is invisible until the runtime module loads again. Cost: user power
   cycle + one instrumented launch; zero disk changes.
2. **Run the app drive in the return window** (APP-SESSION-INTEGRATION §3.1
   slot, after the batch `e2e` stage, before `stage-settings`): consume the
   staged install/enable/disable commands against the MODE=B payload
   (`a21771f8…`), verifying `.consumed` twins, transcript pairing, and records
   state per the drive protocol. Impact: first-ever console effect for the
   app track; converts G1/G2/G11 from host-proven to console-proven. All prep
   (P0 re-pin) is already applied and green.
3. **Re-author the settings+loadlist pair to include app rows and lockstep
   restage** (track-closure §5 recipe): freeze-check current pair, merge app
   rows (Item Spawner + any app-installed mod), re-record the inventory,
   restage as one lockstep pair. Impact: an app-installed mod actually loads
   at the next launch — the first true end-to-end "install in UI, mod runs in
   game" result.
4. **Build the on-console manager process**: bind `mfw8_records/mfw8_actions`
   in-process into the PS5 app build (replacing the fork/exec Python driver),
   compose native-host startup with `native-app-launcher`, and run the PS5
   loopback build once (serves `/app` on 8085, browser opened by launcher).
   Impact: the console-only product requirement (app runs entirely on PS5)
   gets its first live execution and the PS5 user gets a real UI. Prereq: the
   existing modmgr semantics must be re-expressed in C or carried by the
   installer binary's own verbs (they already exist as CLI verbs).
5. **Turn on browse->download in the UI**: rebuild with the documented
   jobs/archive/oauth composition (`--archive-profile --job-directory
   --oauth-config`; inputs recorded in worker-app-download-runtime
   `download-runtime-profile.json`) plus the user's real client_id; then
   acquire the first real DLL-mod archive per worker-dll-mod-acquire's
   runbook to seed the DLL census. Impact: closes G3+G4 and starts the
   evidence base for families beyond Lua (the "all mods" objective).

---

## 9. Survey limits

- Read-only; no console contact, no network, no code changes. All maturity
  figures quoted from the cited workers' receipts, not re-verified except
  where noted (NativeModSource.cpp, modforge_activation.c role table, web
  index.html tabs read directly this session).
- Console state is a snapshot from the newest recorded evidence
  (worker-boot-protocol, 2026-10-05 ~21:05); the loading-regression front is
  actively moving (worker-r3-import-fix, worker-insession-refusal, Oct 5/6)
  and this doc does not pre-judge its outcome.
- No quality score is assigned to any UI (never rendered by a human or a
  browser driver — worker-app-ui-flows §3); no "all mods work" percentage is
  implied. The 60%/30% figures in the handoff are opinion estimates, not
  measurements (VERIFIED_VS_MISSING).
