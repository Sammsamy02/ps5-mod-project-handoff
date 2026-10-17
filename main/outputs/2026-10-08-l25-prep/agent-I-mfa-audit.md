# MFA (modforge file-admission) audit — agent I

Date: 2026-10-08. Read-only source analysis + local file checks only. No
network/console/FTP access was performed.

## Sources audited

| What | Path |
|---|---|
| Header (l24, current) | `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l24-package/lineage/modforge_activation.h` |
| Header (l23) | `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l23-package/lineage/modforge_activation.h` (byte-identical to l24 copy) |
| Implementation | `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/claude-takeover-43a96969/source/runtime/modforge_activation.c` (MD5 `9ce907127c6de918a500757b0289ba66`; verified byte-identical to every other vendored copy checked, incl. `native-coherent-runtime-1/source/activation/` and the `worker-f-r2-fix`/`worker-e2e-final` compositions) |
| Loadlist parser | `.../native-coherent-runtime-1/source/activation/modforge_loadlist.h` (identical copies in `claude-takeover-43a96969/source/runtime/`) |
| Runtime call site | `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l24-package/lineage/lifecycle/runtime_module_entry.c` |
| Activation→UE4SS adapter | `.../native-coherent-runtime-1/source-reconstruction-final-4/bootstrap/activated_mod_source.cpp` |
| Settings consumer | `.../native-coherent-runtime-1/source/port/ue4ss/src/SettingsManager.cpp` — this exact file is pinned for the l24 build by `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l24-build-r2/SOURCE-SELECTION.json` |
| Ini parser | `.../native-coherent-runtime-1/source/port/first/src/IniParser/{Ini,TokenParser,Value}.cpp` (pinned for l24 by the same SOURCE-SELECTION.json; headers under `ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/IniParser/` and `.../ParserBase/`) |
| Console loadlist | `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/harvest-20261008T174239Z-25929d/loadlist.txt` (before/after/harvest copies all MD5 `bb8c601f4169291953ba3b1b59a93759`) |
| Candidate files | `/tmp/l24-settings-nohooks-1947.ini`, `/tmp/l24-settings-backup.ini` |

Build provenance note: `outputs/2026-10-08-l23-runtime/checkpoint-before-l23.json:11056`
already pins the implementation as
`claude-takeover-43a96969/source/runtime/modforge_activation.c:311-495` ("all
vendored copies byte-identical"), consistent with this audit.

## 1. Full admission algorithm (`mfa_activate`)

Contract: `modforge_activation.h:10-23` (4 steps, all-or-nothing) and
`modforge_activation.h:90` (default preload roles lua_mod/config/script).

In `modforge_activation.c`:

1. **Argument validation** — `:330-333` (`root`/`title_id` required → `MFA_E_INVALID`).
2. **Limits defaults** — `:337-340`: 4096 files, 8 MiB per preloaded file, 64 MiB total preload, 1 MiB loadlist.
3. **Cancel/deadline check** — `:342`, helper `stop_requested :129-135`.
4. **Read `loadlist.txt`** — `read_loadlist :211-279`: ENOENT → `MFA_E_NO_LOADLIST` (`:221-223`); oversize → `MFA_E_LIMIT` (`:236`, `:264`); buffered whole, NUL-terminated (`:275`).
5. **Pass 1 — validate + size arena** (`:346-394`), record by record via `mf_next` (`modforge_loadlist.h:144-206`):
   - strict UTF-8 per line (`modforge_loadlist.h:154`, `mf_utf8_ok :82-103`);
   - CRLF tolerated (`:153`), blank/`#` lines skipped (`:155`);
   - header must be exactly `modforge-loadlist\t1` (`:170-176`); empty file is malformed (`:205`);
   - TAB-split, max 4 fields (`:162`);
   - per-kind field counts (`:190-199`); `file` records need `mf_path_ok` (`:109-125`: relative, `/`-separated, no backslash/control chars, no `.`/`..`/empty segments) and a decimal u64 size (`mf_parse_u64 :127-140`);
   - control chars in any field → `MFA_E_LOADLIST` (`mfa_activate :352`, `field_ok :137-144`);
   - file-count limit → `MFA_E_LIMIT` (`:366-370`); path length → `MFA_E_LIMIT` (`:371-375`);
   - per-file (8 MiB) and cumulative (64 MiB) preload limits for preload roles only → `MFA_E_LIMIT` (`:376-390`);
   - exactly one `title`, one `engine`, at most one `sdk` (`:401-406`).
6. **Pass 2 — copy strings** (`:423-451`), then **title equality** vs `cfg->title_id` → `MFA_E_TITLE` (`:454-459`).
7. **Sort by path + duplicate detection** → `MFA_E_DUPLICATE` (`:461-472`).
8. **Pass 3 — preload files in path order** (`:474-492`): every file whose role is in the preload set (default `lua_mod`, `config`, `script` — `mfa_default_roles :281`, `role_preloaded :283-288`) is read by `read_exact :163-208`:
   - open `O_RDONLY|O_NOFOLLOW|O_CLOEXEC` (`posix_open :41-48`) → `MFA_E_OPEN` (`:170-174`) — this is the **existence** check for preloaded roles;
   - read exactly `size` bytes: early EOF → `MFA_E_SIZE` "file is shorter than the load list says" (`:185-189`); read error → `MFA_E_READ` (`:181-183`); short/over-read guard (`:190-193`);
   - **one extra 1-byte read**: any byte remaining → `MFA_E_SIZE` "file is longer than the load list says" (`:196-204`) — this produced today's `code=-8`;
   - cancel/deadline re-checked between chunks (`:178`);
   - a NUL sentinel is appended *after* `size` bytes (`:487`; `mfa_file.data[size]==0` per header `:101`) — the sentinel is outside the exposed length (see §3).
9. **Non-preloaded roles** (e.g. `pak`, `plugin`): pass 3 skips them (`:480` — `if (!role_preloaded_c(...)) continue;`), so they are **never opened, never size-verified, never existence-checked**; they exist only as loadlist rows (header `:13-15` "lists every other file … without reading it"). Not applicable to this install — all 7 files are `config`/`lua_mod`.

**Content-based checks: NONE.** There is no hash, no checksum, no magic-number
check, and no parsing of file bytes anywhere in the admission path. **Byte-size
exactness is the ONLY content check for role=config.**

Failure reporting: the runtime call site
`runtime_module_entry.c:284-302` builds `detail = "<name> <message> <path>"`
(`:288-299`) and calls `runtime_failure("mfa_activate_failed", code, detail)`
(`:300`), then returns so the game continues unmodded and `module_start`
returns 0 (`:302` comment). This exactly reproduces the observed
`mfa_activate_failed code=-8 detail=size file is longer than the load list says ue4ss/UE4SS-settings.ini`.

## 2. MFA_E_* codes

From `modforge_activation.h:46-60` (names cross-checked against
`mfa_result_name`, `modforge_activation.c:102-119`):

| Code | Name | Meaning |
|---|---|---|
| 0 | `MFA_OK` | success |
| -1 | `MFA_E_INVALID` | bad arguments |
| -2 | `MFA_E_NO_LOADLIST` | loadlist.txt missing: nothing installed |
| -3 | `MFA_E_LOADLIST` | loadlist.txt malformed |
| -4 | `MFA_E_TITLE` | installed for a different game |
| -5 | `MFA_E_LIMIT` | too many files / bytes, path too long |
| -6 | `MFA_E_OPEN` | a listed file could not be opened |
| -7 | `MFA_E_READ` | read error |
| -8 | `MFA_E_SIZE` | a file is shorter or longer than listed |
| -9 | `MFA_E_MEMORY` | allocation failed |
| -10 | `MFA_E_CANCELLED` | the cancel flag was raised |
| -11 | `MFA_E_DEADLINE` | the deadline passed |
| -12 | `MFA_E_DUPLICATE` | the same path is listed twice |

## 3. Does a 1947-byte, different-content settings file pass admission? — CONFIRMED

- `read_exact` (`modforge_activation.c:163-208`) compares only the byte count
  against the loadlist size. Bytes are copied to the blob and never compared to
  anything.
- Local verification: both `/tmp/l24-settings-backup.ini` and
  `/tmp/l24-settings-nohooks-1947.ini` are exactly **1947 bytes** (MD5s differ:
  `c78c979571f28606fd8f2eabd2d181c8` vs `5667b5268ee11329b63f09ceda708e00`),
  matching loadlist row `file config ue4ss/UE4SS-settings.ini 1947`
  (`loadlist.txt:6`). `diff` shows exactly: line 60 comment loses `:` and `.`
  (-2 bytes), lines 63-64 `HookLoadMap`/`HookInitGameState` `true`→`false`
  (+2 bytes) — net 0.
- Downstream (post-admission) consumers also pass:
  - `activated_mod_source.cpp:40-43` binds `std::string_view(file->data, file->size)` — the MFA sentinel NUL at `data[size]` is **outside** the view;
  - `SettingsManager.cpp:62-65` (PROSPERO path) throws only if the file is missing or contains an embedded NUL — the file is pure ASCII (`file`: "ASCII text", 0 CR bytes), NUL-free.

## 4. UE4SS settings INI parser

Pinned consumer for this runtime: `SettingsManager.cpp` (`__PROSPERO__` path,
`:58-90`): reads `ue4ss/UE4SS-settings.ini` from the native mod source
(`:62`), throws on missing (`:63`) or embedded NUL (`:64-65`), then converts
**ASCII 1:1 to wchar_t** (`:66-75`; comment: PS5 SDK libc++ `codecvt` bug
garbles `ensure_str` — so the file must stay pure ASCII) and calls
`parser.parse`. `[Hooks]` bools are registered at `:287-312`
(`HookLoadMap` `:290`, `HookInitGameState` `:291`) via
`REGISTER_BOOL_SETTING` (`:27-34`) → `Ini::Parser::get_bool`.

**Comment line: parses identically.** `handle_semi_colon_token`
(`TokenParser.cpp:426-439`): any `;` token outside a value consumes everything
up to the next `NewLine` token. Comment *content* is never inspected or
retained. Original `"; A-PI/FLAG-3: both keys REQUIRED 0."` and shortened
`"; A-PI/FLAG-3 both keys REQUIRED 0"` therefore produce **identical parse
results** (no section, key, or value effects; both sit on their own line inside
`[Hooks]`, byte-verified: `; A-PI/FLAG-3 both keys REQUIRED 0\n` immediately
followed by `HookProcessInternal=0\n`).

**`false` parsing: accepted exactly as written.** `is_bool`
(`TokenParser.cpp:84-105`) lowercases and compares to `"true"`/`"1"`/
`"false"`/`"0"`; value text is built by `characters_to_string`
(`:163-231`) which stops at NewLine/CarriageReturn. Byte dump confirms the
lines are exactly `HookLoadMap=false\n` / `HookInitGameState=false\n` — no
trailing space, no CR — so the value string is exactly `"false"` →
`Bool{value=false, is_bool=true}` → `add_bool_value(false)` (`Value.cpp:80-90`)
→ `get_bool` returns false. Note the member defaults are `true`
(`SettingsManager.hpp:118-119`), so the flip is a real functional change; a
*failed* parse would silently revert to true (macro catch, `:27-34`), but this
parse succeeds.

Parser quirks worth knowing (none triggered by this file):
1. **Inline `;` is not a comment**: after `key=` (state `SetSectionValue`) a
   semicolon is appended to the value literally (`TokenParser.cpp:428-431`,
   `:202-204`). Full-line comments only.
2. **Trailing spaces become part of the value** (`:175-177`): `"false "` would
   fail `is_bool` and silently keep the default (throw swallowed by the macro).
   This file has zero trailing-whitespace lines (verified).
3. **Line endings**: CR is a token that parse ignores (`:445-446`), so CRLF and
   LF both parse; this file is pure LF (0 `\r` bytes) with a trailing `\n` on
   the last line (`FExecVTableOffsetInLocalPlayer=0\n`).
4. **Latent edge (not triggered)**: a comment as the final line *without* a
   trailing newline would spin in the skip loop (`peek() != NewLine` forever;
   EOF token is appended by `Tokenizer.cpp:216`, and `get_token` indexes the
   vector directly — `ParserBase/TokenParser.cpp:11-14`). Keep final `\n`.
5. **No BOM**: a UTF-8 BOM would put Characters before the first `;` in
   `StartOfFile` state and throw a syntax error (`handle_characters_token`
   else-branch, `TokenParser.cpp:328-332`). This file starts with `;` — clean.
6. A value that exactly matches another variable's name becomes a reference
   (`:303-307`); `"false"` matches no key — no issue.

## 5. Other loadlist files — expected sizes and verification mechanism

Console loadlist (harvest copy; before/after/harvest identical):

```
title PPSA33286 / engine unreal / framework ue4ss-ps5 / mod TownfallItemSpawner 1.0.0
config  ue4ss/UE4SS-settings.ini                            1947
config  ue4ss/MemberVariableLayout.ini                        588
config  ue4ss/VTableLayout.ini                                216
config  ue4ss/PS5Pad.ini                                     3718
lua_mod ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua     21415
lua_mod ue4ss/Mods/TownfallItemSpawner/enabled.txt              0
lua_mod ue4ss/Mods/shared/UEHelpers/UEHelpers.lua            9962
```

Mechanism to verify on next console contact:
- **Passive/authoritative**: MFA itself re-checks all 7 at `module_start`; any
  mismatch fails activation all-or-nothing with `mfa_activate_failed code=-8`
  plus the offending path (and a `state failed` line in the mod root's
  `runtime-status.txt`, `modforge_activation.h:139-148`).
- **Proactive (offline)**: download each file over FTP and `wc -c` against the
  rows above. All seven are plain-text files, so the ftpsrv SELF/SPRX
  decrypt-on-download hazard (AGENTS.md PS5 rules) does not apply; `SITE SELF`
  toggling is only needed for executables. Equivalently, run
  `outputs/2026-10-04-zcode-team/worker-data-preflight/tools/data_preflight.py`
  against a harvested root — it re-implements the runtime's own gates 1:1 with
  file:line pins (size gate `modforge_activation.c:185-204`; documented at
  `outputs/2026-10-08-l23-runtime/checkpoint-before-l23.json:12213`).
- History suggests re-verifying `ue4ss/PS5Pad.ini` (3718) in particular: the
  same checkpoint records a past incident where a stale staging tree carried a
  divergent pad config (em-dash + wrong row) that would have killed a launch.

## Verdict

**The 1947-byte replacement (`/tmp/l24-settings-nohooks-1947.ini`) will pass
MFA admission and activate** — admission is size-exact-only with zero
content/hash checks, the file is exactly 1947 bytes, and every downstream gate
(NUL check, ASCII conversion, INI parse, `false` bool parsing) passes with the
shortened comment parsing byte-for-byte identically to the original.

Residual risks:
1. **Transfer integrity**: the console copy must land at exactly 1947 bytes —
   binary-safe upload, LF endings, no BOM, trailing newline retained (quirks 3-5
   above). A CRLF-converted or BOM-prefixed upload would either re-trigger -8 or
   throw in `SettingsManager::deserialize`.
2. **Functional, intended**: `HookLoadMap=false` / `HookInitGameState=false`
   disable those UE4SS hooks (defaults are true); any Lua logic riding them
   stops firing.
3. **All-or-nothing**: admission still requires the other 6 files at their
  exact sizes (§5); verify sizes on next console contact, PS5Pad.ini first.
4. If admission fails again for any reason, the game silently continues
   unmodded (`runtime_module_entry.c:302`) — check `runtime-status.txt` /
   diagnostics for the `mfa_activate_failed` line rather than inferring from
   in-game behavior.
