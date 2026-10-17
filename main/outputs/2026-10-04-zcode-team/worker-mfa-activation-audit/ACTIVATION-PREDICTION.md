# ACTIVATION-PREDICTION.md — composition-5 first launch, activation stage

Worker: worker-mfa-activation-audit · 2026-10-04 · static audit only (no console contact, no target execution)

Authority chain: `mfa_activate` implementation `.superpowers/native-dependency-spike/claude-takeover-43a96969/source/runtime/modforge_activation.c` (sha256 `f97d1d2094b090befa37e80f11bc62ca7fe010245e1bb3142cc8b8f264fbf81a`); header `.../source/runtime/modforge_activation.h`; parser `.../source/runtime/modforge_loadlist.h` (single-header, no .c). All four vendored copies in `worker-{d1-chain-relink,f-r2-fix,installer-composition,zip-to-mfb}/composition/vendored/modforge_activation.c` are byte-identical to the authoritative — there is exactly one validation set in every tree. Entry `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry.c` (sha prefix `1c041f8b` per controller) is byte-identical to the composition-5 `lineage-overlay-1/lifecycle/` copy (diffed before that staging dir was removed by concurrent composition-5-rebuild work at 18:44). All call signatures were already controller-verified; not redone.

Package under test: entry calls `mfa_activate` with `config{root="/data/modbridge/roots/PPSA33286", title_id="PPSA33286", preload_roles=NULL, limits zeroed, deadline=0, cancel=NULL, io=NULL}` (entry:239–250), then `modbridge_runtime_start_with_activation` (entry:281–283).

## Verdict (ordered): GO for the activation stage

`mfa_activate` is predicted to return `MFA_OK` and preload 35,897 bytes (35,890 payload + 7 NUL terminators; the entry will emit `mfa_activated_low=35897`, `mfa_activated_high=0`). No check predicts failure. The bridge stage (loader facts → context → prepare/start) is outside this audit's verdict.

Residual UNKNOWNs that could still flip the stage, ranked by probability:

1. `MFA_E_NO_LOADLIST (-2)` — the `/data` readability window: activation works only if `module_start` really can open `/data/modbridge/roots/PPSA33286/loadlist.txt` (header comment, modforge_activation.h:5–8). This is a runtime-timing contract, not checkable statically here.
2. `MFA_E_SIZE (-8)` / `MFA_E_OPEN (-6)` — staged bytes diverging from `final-package/` (e.g. a file edited after staging). Context reports matching staged sizes and two matching shas; the remaining five staged shas were not independently re-verified from here (console contact forbidden). Note: `mfa_activate` verifies SIZES ONLY (no sha anywhere in the C runtime — see check 9), so size equality alone suffices for activation to pass.

## 1. Every mfa_activate refusal branch (exact code + message)

Enum: `modforge_activation.h:46–60`. Every branch below sets `mfa_error{code, os_error, line, path, message}` and releases everything (all-or-nothing). Line numbers are `modforge_activation.c` unless noted.

| # | Code | Message (exact) | Where | Trigger |
|---|------|-----------------|-------|---------|
| 1 | `MFA_E_INVALID (-1)` | `no output pointer` | :330 | `out == NULL` |
| 2 | `MFA_E_INVALID (-1)` | `root and title_id are required` | :332–333 | `!cfg` or empty/NULL `root` or NULL `title_id` |
| 3 | `MFA_E_CANCELLED (-10)` | `activation cancelled` | :129–134 via :342, :178, :250 | `*cfg->cancel` raised |
| 4 | `MFA_E_DEADLINE (-11)` | `activation deadline passed` | :129–134 via :342, :178, :250 | `now_ms() >= deadline_ms` (deadline 0 = disabled) |
| 5 | `MFA_E_LIMIT (-5)` | `root path too long` | :217–218 | root+`loadlist.txt` > 1024 bytes |
| 6 | `MFA_E_NO_LOADLIST (-2)` | `no loadlist.txt (nothing installed)` | :220–223 | open ENOENT |
| 7 | `MFA_E_OPEN (-6)` | `cannot open loadlist.txt` | :224 | open failed, errno != ENOENT |
| 8 | `MFA_E_MEMORY (-9)` | `out of memory` | :226–229, :243–247, :408–421 | malloc/realloc/calloc failure |
| 9 | `MFA_E_LIMIT (-5)` | `loadlist.txt is too large` | :234–239 and :264–268 | > `max_loadlist_bytes` (default 1 MiB) |
| 10 | `MFA_E_READ (-7)` | `read failed` | :253–255 (loadlist), :181–183, :201 (payload) | read() < 0 |
| 11 | `MFA_E_LOADLIST (-3)` | `control character in a field` | :351–354 | byte <0x20 or 0x7f in any field of a known record (line recorded) |
| 12 | `MFA_E_LIMIT (-5)` | `too many files` | :366–370 | `file` rows > `max_files` (default 4096) |
| 13 | `MFA_E_LIMIT (-5)` | `path too long` | :371–375 (pass 1), :168–169 (pass 3) | path+2 > `MFA_PATH_MAX` 1024 |
| 14 | `MFA_E_LIMIT (-5)` | `file too large to preload` | :376–382 | preloaded role, size > `max_file_bytes` (default 8 MiB) |
| 15 | `MFA_E_LIMIT (-5)` | `preloaded files exceed the total limit` | :384–389 | running preload total > 64 MiB |
| 16 | `MFA_E_LOADLIST (-3)` | `malformed loadlist.txt` | :395–400, with parser line | `mf_next` < 0: bad UTF-8 (loadlist.h:154); >4 TAB fields (loadlist.h:162 split guard); header not exactly `modforge-loadlist\t1` (loadlist.h:170–174); kind/arity mismatch (loadlist.h:190–199); `file` path failing `mf_path_ok` — relative, '/', no backslash/control, no empty/`.`/`..` segment (loadlist.h:109–125); size not decimal u64 (loadlist.h:127–140, :198); empty input without header (loadlist.h:205) |
| 17 | `MFA_E_LOADLIST (-3)` | `loadlist.txt needs one title and one engine` | :401–406 | `n_title != 1 \|\| n_engine != 1 \|\| n_sdk > 1` |
| 18 | `MFA_E_TITLE (-4)` | `load list is for a different game` | :454–459 | `strcmp(list title, cfg->title_id) != 0`; `err->path` = the list's title |
| 19 | `MFA_E_DUPLICATE (-12)` | `path listed twice` | :461–472 | equal adjacent paths after bytewise qsort |
| 20 | `MFA_E_OPEN (-6)` | `cannot open listed file` | :170–174 | payload open failed |
| 21 | `MFA_E_SIZE (-8)` | `file is shorter than the load list says` | :185–188 | EOF before `size` bytes |
| 22 | `MFA_E_SIZE (-8)` | `file is longer than the load list says` | :200–204 | 1 extra byte readable past `size` |
| 23 | `MFA_E_READ (-7)` | `read returned more than asked` | :190–192 | defensive; I/O contract violation |

Ordering matters for "first failure": pass-1 row checks (11–16, in file order) run before the title-count check (17), which runs before allocation (8), title match (18), duplicates (19), and finally per-file reads in path-sorted order (20–22). `MF_REC_UNKNOWN` rows are skipped entirely, control chars included (loadlist.h skip at modforge_activation.c:349).

## 2. Staged package vs every check — PASS/PREDICTED-FAIL/UNKNOWN

Local exact bytes: `outputs/2026-10-04-zcode-team/worker-loadlist-final/final-package/`. `loadlist.txt` = 446 bytes, sha256 `49ab7a3a40ee15d3090a542a6e7150307b4d001a08aa3386de66c04b6981e3bf` (matches staged 49ab7a3a). Content (od-verified, LF only, trailing newline, no CR):

```
modforge-loadlist⇥1
title⇥PPSA33286
engine⇥unreal
framework⇥ue4ss-ps5
mod⇥TownfallItemSpawner⇥1.0.0
file⇥config⇥ue4ss/UE4SS-settings.ini⇥779
file⇥config⇥ue4ss/MemberVariableLayout.ini⇥588
file⇥config⇥ue4ss/VTableLayout.ini⇥186
file⇥config⇥ue4ss/PS5Pad.ini⇥2960
file⇥lua_mod⇥ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua⇥21415
file⇥lua_mod⇥ue4ss/Mods/TownfallItemSpawner/enabled.txt⇥0
file⇥lua_mod⇥ue4ss/Mods/shared/UEHelpers/UEHelpers.lua⇥9962
```

The console `file` grammar is exactly 4 TAB fields (`file`, role, path, size) — there is NO sha column in the runtime grammar (FORMATS.md "Load list"; parser demands `np == 4`, loadlist.h:196–199). Integrity is enforced upstream: the installer hashes each file while streaming and the bundle passes hash checks before the load list is written (FORMATS.md "Staged mod root"/bundle sections). The real shas of the local package, for provenance:

| path (relative to root) | listed size | actual size | sha256 |
|---|---|---|---|
| loadlist.txt | — | 446 | `49ab7a3a40ee15d3090a542a6e7150307b4d001a08aa3386de66c04b6981e3bf` |
| ue4ss/UE4SS-settings.ini | 779 | 779 | `9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f` |
| ue4ss/MemberVariableLayout.ini | 588 | 588 | `f291d552418dda916ec28675bd9e40d78bf1d3e9f75ce0c36df6b95dcfa63f32` |
| ue4ss/VTableLayout.ini | 186 | 186 | `9c20f7c9b1a1ea9a665db12df82ee7e62694fe4cd207cf53a44116e124fbad86` |
| ue4ss/PS5Pad.ini | 2960 | 2960 | `1d919fbfac4976f84e6c621970aaddc61493858fc5564c9481c90f94055eca1e` |
| ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua | 21415 | 21415 | `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658` |
| ue4ss/Mods/TownfallItemSpawner/enabled.txt | 0 | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| ue4ss/Mods/shared/UEHelpers/UEHelpers.lua | 9962 | 9962 | `5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7` |

Checks in execution order:

| Check | Verdict | Basis |
|---|---|---|
| Args: root non-empty, title_id non-NULL | PASS | entry:246–247 hardcodes both; zeroed struct otherwise (entry:239–245) |
| Cancel/deadline | PASS | both zero/NULL → checks disabled (:332–342) |
| Loadlist opens (staged root reachable) | PASS (predicted) / UNKNOWN (console timing) | staged per context; `/data` window is the header's documented module_start contract |
| Loadlist ≤ 1 MiB | PASS | 446 B |
| Parse: header, arities, UTF-8, paths, sizes | PASS | header exact; 7 file rows exactly 4 fields; all paths relative, '/'-separated, no dot segments; pure ASCII; sizes decimal; `mod` row 3 fields; counts title=1, engine=1, sdk=0≤1 |
| No control chars in fields | PASS | ASCII printable + TAB separators only (od) |
| ≤ 4096 files; paths ≤ 1024 | PASS | 7 files; longest path 45 chars |
| Per-file ≤ 8 MiB; total ≤ 64 MiB | PASS | max 21,415; total 35,890 (+7 NUL) |
| Title match `PPSA33286` | PASS | row 2 equals `MODBRIDGE_TITLE_ID` (entry:65) |
| No duplicate paths | PASS | 7 distinct paths |
| Per-file read_exact size equality | PASS (predicted) / UNKNOWN (staged byte-equality) | all 7 actual sizes equal listed sizes exactly, including the 0-byte `enabled.txt` (read_exact size-0 path: loop skipped, one EOF-probe read must return 0 — the empty file satisfies it) |
| Memory (~36 KB blob + arena) | PASS | trivial vs defaults |

FIRST FAILURE POINT: none predicted. If the stage fails despite this, the first recorded line would come from check order above — most plausibly `MFA_E_NO_LOADLIST` (root not readable at module_start) or `MFA_E_SIZE`/`MFA_E_OPEN` (staged divergence), never a grammar/role/limit/title/duplicate error.

## 3. Preload-role coverage

Default set when `config.preload_roles == NULL`: `{lua_mod, config, script}` (`mfa_default_roles`, modforge_activation.c:281; selected at :283–288). Roles present in the loadlist: `config` ×4, `lua_mod` ×3 — every row is in the default set, so all 7 files are preloaded and none is merely listed. No skip predicted.

There is NO role refusal anywhere in `mfa_activate`: a row with an out-of-set role (e.g. `pak`) would be parsed, counted, size/path/limit-checked, but not read (pass 3 skips it at :480) — a silent list-only entry, not `MFA_E_ROLE` (no such code exists). The task's "MFA_E_ROLE or a skip" alternative resolves to: it would be a skip, and here it does not occur.

## 4. runtime-status.txt — one-sink audit and dir existence

Writers of `<root>/runtime-status.txt` in the composed runtime, exhaustive:

1. `RuntimeAckSink` (the ack session): opened ONCE inside `PrepareActivatedUE4SSRuntime` via `RuntimeAckSink::Open` → `mfa_status_open(..., status_root, pid, now_ms)` (ue4ss_backend.cpp:101–102; runtime_owner.cpp:130–144), only AFTER `mfa_activate` succeeded and the activation was consumed (ue4ss_backend.cpp:100–104 comment). Emits `activated` / `engine-ready` / `mods-started` / `failed` / `stopped` through `mfa_status_write` (runtime_owner.cpp:154–178); `RuntimeAckConfig` is born solely in `MakeRuntimeAckConfig` with `status_root = mod_root` exactly (runtime_ack_supplier.cpp:44–54). Closed via `ack_->close()` in the owner-stop path (runtime_owner.cpp:74, :193–198). Sink has NO destructor — an exception path that abandons it without `close()` leaks the fd until process end (harmless, but nothing writes through it afterwards).
2. The entry's `write_status_failed` (entry:194–219): `mfa_status_open` + one `mfa_status_write("failed", code, note)` + `mfa_status_close`, reached only from `runtime_failure` (entry:221–226) on (a) `mfa_activate` failure — the bridge never ran, no sink can exist; (b) `runtime_start_failed` — if the sink had opened and the prepare threw, the sink's own `failed(...)` line was already written during unwinding and the object is destroyed; the entry's open uses `O_TRUNC` (modforge_activation.c:609), i.e. a fresh session REPLACES the file per the header contract (modforge_activation.h:146 "a new session replaces the file") — strictly sequential, never two concurrent writers; (c) `runtime_stop_failed` in module_stop — the owner (and sink) is already destroyed by `StopRuntimeWithAck` (runtime_ack_supplier.cpp:56–65).

No other runtime code writes that path (`e2e_status.h` / `modinst_*` references are host/installer-side records, not the game process). The one-sink rule (WIRING-SPEC S4) HOLDS: at most one active sink at any moment.

Dir existence: `mfa_status_open` uses `open(O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW, 0644)` (modforge_activation.c:609) — it creates the FILE, never parent directories. The staged root `/data/modbridge/roots/PPSA33286` exists (the package is staged there), so creation succeeds. If the root were absent, `mfa_activate` would already have returned `MFA_E_NO_LOADLIST` and `write_status_failed` deliberately no-ops (entry:205–206) — the diagnostic event and the UE4SS.log line still carry the failure. Same for the UE4SS.log parent `/data/plugins/` (the prx itself lives there).

## 5. First-recorded-failure signature to expect in UE4SS.log

On any activation-stage failure the entry appends exactly one line to `/data/plugins/UE4SS.log` (entry:166–192), formatted:

```
modbridge module_start failed: mfa_activate_failed code=<n> detail=<result_name>[ <message>[ <path>]]
```

- `<n>` is the negative `mfa_result` (e.g. `-2`, `-6`, `-8`), decimal via `log_append_i32` (entry:146–159).
- `detail` = `mfa_result_name(code)` (:102–119) + `error.message` + `error.path`, space-joined, printable-sanitized (entry:250–266).
- module_start still returns 0 — the game continues UNMODDED (entry:35, :268, :329–331).
- The same failure also writes one `state⇥1⇥failed⇥<n>⇥mfa_activate_failed <message> <path>` line into `<root>/runtime-status.txt` (fresh session header first), best-effort (entry:198–219).

Concrete signatures for the three plausible divergences:

- Root not readable at module_start: `modbridge module_start failed: mfa_activate_failed code=-2 detail=no-loadlist no loadlist.txt (nothing installed) loadlist.txt`
- A listed file missing on console: `modbridge module_start failed: mfa_activate_failed code=-6 detail=open cannot open listed file <relpath>` (first in path-sorted order: `ue4ss/MemberVariableLayout.ini`)
- Size drift: `modbridge module_start failed: mfa_activate_failed code=-8 detail=size file is shorter than the load list says <relpath>` (or `longer`)

Success signature instead: no failure line; diagnostic events `mfa_activated_low=35897` / `mfa_activated_high=0`, then `runtime_start_ok` if the bridge stage also succeeds (entry:270–273, :284–287).
