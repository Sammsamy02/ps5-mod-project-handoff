# APP-STATE.md — worker-app-modmanager: inventory of the mod-manager app track

Origin: worker-app-modmanager, 2026-10-04 (ZCode team, PS5 mod-manager).
Read-only inventory of the app/backend lineage under
`outputs/2026-10-04-zcode-team/` plus the live app tree
`outputs/ps5-mod-bridge/native-host/`. Every maturity claim below is tied to a
worker's own recorded receipt; nothing here is asserted from drafts.

## 1. The product shape (as evidenced)

End product = a PS5 Nexus-style mod manager. The evidenced architecture is:

```
host app (HTTP backend + web UI)          console (controller-only today)
  browse/auth/download (real)               injected payload ELF
  -> download store (real)                  /data/modbridge/{intake,state,roots}
  -> records store (real)                   installer verbs (payload)
  -> install pipeline (proven CLI)   <==>   command files + e2e-status.jsonl
  -> roots/<TITLE>/ loadlist root           runtime ack (simulate-runtime only)
```

Console ops remain **controller-only and PENDING**; everything below the app
layer that touches a console is a staged, gated but **never-executed-on-console**
sequence. All green evidence is host-side.

## 2. Component inventory + honest maturity table

Maturity scale: `proven` (host tests green against real bytes, receipt cited)
/ `partial` (works for a documented subset) / `wired-on-paper` (drop-in exists,
integration patch recorded but NOT applied to the live app) / `design` /
`pending` (console; nothing has run there).

| Component | Where | What is real | Maturity | Evidence |
|---|---|---|---|---|
| HTTP backend (auth, catalog, jobs, downloads, static UI) | `outputs/ps5-mod-bridge/native-host/http.c` | 13 route groups implemented; OAuth-gated catalog; serialized job queue (max 8, no install kind) | **proven (host)**, read-only mod-service | worker-8 `AUDIT.md` §1 (http.c:395-928 cites) |
| Web UI (`web/app.js`) | same tree | browse/download/jobs tabs real; Installed tab calls `GET /api/installed` which **does not exist** -> always degrades to empty | **partial** (known contract break) | worker-8 `AUDIT.md` §1 (`app.js:935` vs grep count 0) |
| Mod-service bridge (app <-> backend) | `mod_service/read_bridge.{h,cpp}`, `owner.cpp` | `modforge-service/1` contract; read-only passthrough of `service.describe`, `records.list`; generic call passthrough written as patch, not applied | **wired-on-paper** | worker-8 `AUDIT.md` §2, `PATCHES.md` 1-2 |
| Records store (`records.txt`) | worker-7 `modinst.c:340-362`; worker-8 `mfw8_records.*` | `modforge-records\t1` TSV; `mod`/`game` rows; `downloaded->prepared->installed`; `running` derived from `runtime-status.txt`, never stored | **proven** | worker-8 REPORT §2 (61/61), worker-7 REPORT (108/108) |
| Transactional installer (mfb compose, staged root, journaled swap, cancel, recovery) | worker-7 `src/modinst.c` (~1550 lines C99) | full root swap under `.next`/`.prev`, `.modforge-root` ownership guard, crash recovery between journal phases, exit contract 0-5 | **proven** | worker-7 REPORT §"Exact commands": 108/108 host + ASan/UBSan clean |
| Capability/requirement gate | worker-7 (caps.tsv) | `available`/`integrating`/`planned`; unknown/planned = install blocker; integrating = warning only | **proven** | worker-7 REPORT test matrix A8 etc. |
| Download stage (real curl-owner ABI + file sink) | worker-e2e-final `owned/modinst_download.c` | job TSV -> real sink (sha256, fsync, readback) -> `intake/<id>.zip` -> record `downloaded`; failure paths leave no record | **proven (host, stub transport)**; console link needs ps5io RDWR | worker-e2e-final REPORT §3 (31/31) |
| ps5_io RDWR extension (console download precondition) | worker-ps5io-rdwr | native download ELF **linked**: console providers/undef receipts, O_RDWR flag set extension | **proven (link-level)**; never run on console | worker-ps5io-rdwr `NATIVE-DOWNLOAD-LINK.md`, receipts |
| zip -> mfb bridge (F-R1 auto-bundle) | worker-zip-to-mfb `owned/modinst_zip_bridge.c` | quarantine manifest -> member classification (L/M/W/P/I rules) -> `bundles/<id>.mfb` (kind 1, byte-format identical); refusals with specific reasons | **proven (host)**, v1 budget <=256 members <=64 MiB | worker-zip-to-mfb `BRIDGE-DESIGN.md` + E2E logs |
| E2E command schema (config-file entry) | worker-e2e-console-dry `E2E-CMD-SCHEMA.md` | `{"schema":1,"verb":...,"state_root":...,"mods":...,"options":...}`; verb allow-list records/register-mod/prepare/prepare-archive/install/simulate-runtime/recover; key->flag map; deny-list | **design + host-tested grammar** (14/14 refusal cases) | worker-e2e-staging-final `HOST-TEST-RECEIPT.md` |
| Staged 13-command console sequence | worker-e2e-staging-final `cmd/` | 13 byte-pinned command files + SHASUMS; payload identity gate (sha+bytes); consumption protocol (`.consumed` rename + readback) | **staged, NOT executed** (payload gate: current payload has NO e2e-cmd.json reader -> MODE A argv only today) | `STAGED-E2E-COMMANDS.md` §0-§2 |
| e2e-status.jsonl protocol | worker-zip-to-mfb `owned/e2e_status.c` (+F-R2 fix) | begin/end/status events per command; 512-line/256 KiB caps then `{"event":"truncated"}`; F-R2: all controlled exits emit matched `end` (0 unmatched) | **proven (host transcripts)** | worker-f-r2-fix REPORT §3 (87/87 incl. pairing) |
| Console payload ELF | worker-d1-chain-relink `composition/build/console/modbridge-installer.payload.elf` | D1+F-R1+F-R2 merged chain, 681528 B, 116 imports, 0 missing providers, sha `e3489297...` | **linked, never dispatched** | STAGED-E2E-COMMANDS §0 |
| Runtime activation (status plumbing) | worker-7 vendored `modforge_activation.c` | real `mfa_activate()` consumes staged root: title/engine/framework gates, preload by role, `runtime-status.txt` writer driven via `simulate-runtime` | **proven (host)**; engine execution NOT claimed | worker-loadlist-final REPORT (45/45 incl. negative controls) |
| Records<->app backend adapter (`mfw8_*`) | worker-8 `src/` + `PATCHES.md` | drop-in `modforge-service/1` backend mapping app records surface onto proven store; actions delegate to composed CLI; caps honesty (`running` only from real ack) | **wired-on-paper** (61/61 vs real binary + real corpus zip; http.c patch recorded, not applied) | worker-8 REPORT §1-§4 |
| **Mod-manager core loop (list/install/enable/disable/uninstall) as app logic** | **this worker** | previously: only the raw CLI verbs existed; no enable/disable at all; no list projection over `roots/<TITLE>/loadlist.txt` | **new, host-proven here** (see REPORT) | this dir `test/receipts/run-tests.log` |

## 3. The real formats (pinned from receipts + live smoke this session)

Pinned by running the proven composed binary (sha `f2215f2f...`, vendored here)
in this worker's sandbox — see `receipts/smoke-pipeline.log`:

- **Installed root**: `<roots>/<TITLE>/` with `.modforge-root` marker,
  `loadlist.txt` written LAST, then the mods' files. `--roots BASE` +
  `--title T` -> `BASE/T` (matches console layout `/data/modbridge/roots/<TITLE>/`).
- **loadlist.txt**: `modforge-loadlist\t1` header; rows `title`, `engine`,
  `sdk`, `framework` (n>=0), `mod <id> <ver>` (enabled set, load order, later
  wins), `file <role> <relpath> <size>`, `order <plugin>`; TAB-separated,
  <=5 fields, `#` comments, unknown kinds skipped by readers (FORMATS.md
  "Load list"). `enabled.txt` inside a Lua mod is EMPTY and presence-only;
  the runtime derives the UE4SS startup list from that preloaded file
  (worker-loadlist-final T4), so a Lua mod is runtime-disabled only when its
  loadlist rows go away.
- **records.txt**: `modforge-records\t1`; `mod <id> <title> <state> <bundle>`;
  `game <title> <engine> <sdk> <generation> <loadlist-sha>`. The game row's
  sha is the installer's commit truth (recovery compares live loadlist sha
  against it during swap recovery).
- **Bundle path per mod** lives in the records `mod` row (zip before
  prepare-archive, `state/bundles/<id>.mfb` after the bridge) — this is the
  exact per-mod file attribution source for enable/disable (mfb index lists
  the mod's final paths).
- **e2e-status.jsonl**: `{"event":"begin","cmd":C,"title":T,"stage":S}`,
  `{"event":"end","exit":N}`, `{"event":"status","stage":S,"outcome":O,"detail":D}`,
  `{"event":"truncated"}`; append-only under `<state>/e2e-status.jsonl`.

## 4. Gaps this worker closes vs. gaps that remain

Closed here (host): app-level core loop (list with honest enabled state,
install zip/mfb preserving the installed set, enable/disable as guarded
loadlist row edit with exact bundle-derived attribution + records sha
resync, uninstall via F-R2 removal path), command-file emitter that
reproduces the pinned 13 staged files byte-identically, transcript
validator, host tests over a fake `/data/modbridge` tree.

Still open (honest): console execution of anything (payload dispatch,
MODE B config entry revision does not exist yet); live app wiring
(worker-8 PATCHES.md unapplied; `GET /api/installed` still missing);
UE4SS in-game execution unproven; `running` remains ack-derived only.
