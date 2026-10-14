# Independent Tester + Adversarial Review — installer-compat modules

**Tester:** independent agent, Mac-only, host-tested. All work confined to
the tester module (preserved verbatim at `tests/src-tester/` in this
project). No FTP, no payloads, no launches, no console
contact. No files outside `tester/` were created, edited, or removed (module
directories were only read and executed read-only or against throwaway
sandboxes under `tester/runs/`).

**Review window:** 2026-10-07, ~13:38–14:06 local. The modules were being
actively written during the review; findings below state the exact time a
bug was found and whether it was fixed by the module author during the
window. All final results are against the last observed module state
(snapshot 14:05:01, digests in `module_state.json`).

---

## Test infrastructure built (all in tester/)

| file | purpose |
|---|---|
| `framework.py` | test harness: sandboxed tests, assertions, PASS/FAIL/ERROR/PASS-EXPECTED-FAIL, JSON results in `results/` |
| `lua_syntax.py` | pure-Python Lua 5.3 syntax validator (full tokenizer + recursive-descent parser), the `lua53 -p` equivalent — no Lua existed on this host at start (verified: no lua/luajit/luac in PATH, /opt/homebrew, /usr/local, Applications). Self-test: 46 valid + 38 invalid cases, 0 failures. Cross-validated against the real Lua 5.4.7 binary later shipped by conformance-static: **18/19 corpus files agree** (sole disagreement: `#!` first line — fixed in validator to match lauxlib; deep nesting >~900 is a validator recursion limit, documented) |
| `corpus.py` + `corpus/` | 65 adversarial fixtures: truncated/empty/not-a-zip/encrypted-flag zips, bounded bomb, 10k-entry zip, unicode/NFC-NFD paths, symlink+loop entries, path traversal, duplicate member names, backslash paths, 20-deep nested zips, hostile manifests/configs, obfuscated/dynamic-dispatch Lua, syntax-error Lua, tamper targets |
| `poll.py` | 30s module poller with content digests |
| `review_static.py` | danger-pattern scan (network/shell=True/eval/pickle/abs-rmtree/console-contact) + interface discovery per module |
| `suite_installer.py` / `suite_detector.py` / `suite_uninstall.py` / `suite_compat.py` | per-module adversarial suites |
| `integration_tests.py` | L1/L2/L3 end-to-end flows |
| `adapters.py` | cross-module adapter + fixtures |

**Totals (final run against final module state):**

| suite | pass | fail | notes |
|---|---|---|---|
| suite_installer | 27 | 0 | |
| suite_detector | 18 | 1 | the 1 fail is a real module gap (Nexus URL regex) |
| suite_uninstall (+conflicts) | 19 | 0 | |
| suite_compat | 20 | 0 | |
| integration_tests | 3 | 0 | via adapter (see finding I-1) |
| **total** | **87** | **1** | |

Module authors' own test files also run clean under my harness:
detector 33/33, uninstall+conflicts 20/20, checker 35/35, installer 49/49.

---

## Per-module findings

### installer-core (`installer.py`, 1325 LOC)

**Blocker bugs found at 13:49 — both fixed by the module author at 13:52,
fixes pinned by my tests:**

1. **BUG (fixed): `NameError: config_writes` (was line 1013).** Dead
   reference to a removed variable, hit on EVERY install that modifies
   loadlist.txt — i.e. every first install. Reproduced dynamically with a
   minimal clean mod on a fresh tree: traceback, exit 1. Current code has
   no `config_writes` anywhere; `inst.happy.fresh_install_must_succeed`
   pins the fix (install → verify → loadlist row → ploader entry).
2. **BUG (fixed): `need_dir()` never mkdir-plans leaf directories.** The
   leaf was added to `seen_dirs` before the ancestor-chain walk, so
   `MyMod/` and `MyMod/Scripts/` never became mkdir actions; the first
   file write died with `ENOENT .../Scripts/main.lua.tmp-modbridge`.
   Proven by replaying the module's own planning code: planned mkdirs for
   `MyMod/Scripts/main.lua` came out as `[roots, T, ue4ss, Mods]` — no
   `MyMod`, no `Scripts`. Fixed in the current version (rewritten
   `need_dir`); now covered.

**Still-open findings:**

3. **No advisory locking; concurrent installs are destructive-but-detected.**
   No flock/fcntl anywhere. Measured behavior (12/12 attempts each):
   - Same mod, two versions, staggered: both exit 0; last writer wins
     cleanly; verify consistent afterwards (acceptable, but the first
     caller's success message is a lie about final state).
   - Two different mods, tight overlap: **12/12 runs end with exits
     4 (rollback incomplete) + 3 (verification failure), BOTH mods absent,
     loadlist.txt removed entirely** — a mutual-destruction race via the
     unlocked read-modify-write of loadlist.txt/ploader.ini. No residue or
     silent corruption was ever observed, but every concurrent first-install
     pair fails. Recommend a lock file per title root.
4. Duplicate zip members normalizing to the same install path
   (`Scripts/main.lua` + `Scripts/./main.lua`) are not rejected at
   inspection; they land in verification (exit 3) + rollback. Acceptable,
   but exit-2-at-inspect would be cleaner.
5. `verify` (and the install-time verify) trusts absolute `install_path`s
   recorded in the manifest; moving the roots dir invalidates every
   install (verify reports everything missing/tampered). Relative-path
   records would survive relocation.

**Solid:** journal-first transactions with per-file backups; crash
injection (`--crash-after N`, `--crash-at point`) recovers correctly
(verified: crash mid-write → recover → full rollback incl. loadlist row →
clean reinstall); ownership guard refuses unmanaged dirs (verified:
hand-installed file untouched); traversal/symlink/encrypted/10k-entry/
BOM-CRLF/foreign-title/garbage-ploader all rejected or survived without
corruption; unicode paths install+verify OK; NFC/NFD collision handled
deterministically; weird metadata (11 hostile variants) all exit 2.

### mod-types (`detector.py`, 829 LOC)

1. **GAP (open, minor): Nexus URL regex misses `/games/<title>/` path
   segments.** `nexusmods.com/games/cyberpunk2077/mods/12345` parses to NO
   dependency, while `nexusmods.com/mods/12345` works. Only 0–1 path
   component between domain and `/mods` is handled. (My failing test
   `det.requirements_adversarial_text` pins this.)
2. **GAP (open, design): `.lua` content is never sniffed.** A Windows PE
   binary renamed `main.lua` inside `Mods/<n>/Scripts/` classifies as
   `ue4ss-lua` (observed: `label=ue4ss-lua, windows_dll_present=False`).
   The `.dll`-renamed-to-`.lua` case is caught by extension checks, but a
   hostile archive can smuggle a binary past classification as "lua".
   Recommend: first-bytes MZ/ELF check on `.lua` members + binary/text
   heuristic.
3. **Minor:** scan truncation at MAX_ENTRIES (5000) is noted in `notes`
   but not surfaced in `flags` — a `.dll` hiding at entry 6000 of 10000
   would be silently unscanned. (My 10k-file test verifies truncation
   happens and is at least noted.)
4. Static marker detection is bypassable by construction
   (`_G["Find".."FirstOf"]`): expected for a static analyzer; combined with
   the compat-checker it is a documented blind spot, not a bug.

**Solid:** magic-byte format sniffing (misleading extensions handled);
zip-in-zip to depth 4 with content sniffing; tar.gz/xz/bz2 support; device/
symlink tar entries ignored safely; 7z/RAR sniffed as unsupported; declared-
size bomb guards + per-read caps; empty archives/mods classified; their own
33 unit tests pass; semver/requirements parsing solid on positives and
negatives (cue-word discipline verified: mentions without "requires" do not
become requirements).

### uninstall (`uninstall.py` 577 + `conflicts.py` 486 LOC)

1. **BUG (open, minor): `strip_lua_comments` mishandles level-N bracket
   comments.** `--[==[ RegisterKeyBind(Key.F9, cb) ]==]` is treated as a
   LINE comment, so everything after the first line of the comment becomes
   "live" code: observed `parse_keybinds` → `['F9','F1']` — F9 is
   commented out but reported. Consequence: false-positive KEYBIND_CONFLICT
   warnings. (compat-checker's `strip_lua` handles `(=*)` levels correctly
   — the two modules should share one implementation.)
2. **Minor crash-window:** in `uninstall()`, each file is moved to trash
   BEFORE the journal records it. A crash in that window leaves a file in
   the trash but absent from the journal, so `rollback()` skips it
   (manually recoverable from the trash dir, but silent). Installer's
   journal-first discipline is stricter; recommend the same here.
3. **Concurrency:** install-state read-modify-write has no lock; two
   concurrent uninstalls can lose one mod's state entry (same class as the
   installer race, not dynamically forced because the window is tiny).
4. Dependency cycles (A↔B) deadlock uninstalls by design — both refuse
   with `ModStillRequiredError` (verified: safe refusal, no corruption).
   Acceptable-but-worth-documenting behavior; a `--force-break-cycle` or
   cycle detection in conflicts.py would close it.

**Solid:** removes exactly manifest files; path-escape rejection (abs
paths, `..`, symlink resolve) verified; shared files kept; multiset config
line removal preserves other mods' duplicate rows byte-for-byte; modified-
since-install confirmation flow verified (refusal → confirm → removal);
missing files OK; file-now-directory refused without deleting the tree;
5000-entry manifest fine; full rollback (files+config+state) verified;
double-rollback refused; their own 20 unit tests pass.

### compat-checker (`checker.py`, 1029 LOC)

1. **GAP (open, documented-by-them implicitly): string-indexed API calls
   are invisible.** Probe: `_G["LoadAsset"]("/x")` where LoadAsset is
   cataloged `missing` → grade `unknown`, `total_calls: 0`, `missing: []`;
   the same call written directly → `unsupported`, 1 call. The obfuscated
   form is silently downgraded to "cannot verify". Recommend: detect
   `_G[...]` indexing and `load`/`loadstring` usage and emit an explicit
   "dynamic-access-risk" flag rather than relying on the reader noticing
   `total_calls: 0`.
2. **GAP (open, minor): Lua syntax errors are not detected.** A file that
   does not parse grades `unknown` with `total_calls: 0` —
   indistinguishable from an empty mod. Degrades gracefully (never lies
   about compatibility), but "0 calls found" vs "file doesn't parse"
   should be separate outcomes. (A validator like `tester/lua_syntax.py`
   or the bundled Lua 5.4 `loadfile` could supply this.)
3. Their `strip_lua` correctly handles `--[==[...]==]`, strings, and
   escapes while preserving offsets — verified against comment/string traps
   and cross-checked with real Lua.

**Solid:** grading ladder verified end-to-end (compatible / partly-
compatible with peripheral helper missing-call / unsupported with central
missing-call / unknown); catalog edge cases (empty, wrapper, not-JSON, null
entry, unrecognized status → `unknown-status` bucket, wrong shape) all
controlled; hostile Lua (40KB single line, 900-deep parens, NUL bytes,
fullwidth-paren syntax errors, CRLF, shebang, goto spaghetti) never crashes
the checker; missing paths rejected; their own 35 unit tests pass.

### conformance-static

Timeline matters here:
- **13:47 version:** my static analysis found the header claim "Every call
  is wrapped in pcall" was FALSE at the core runner (`run_call` invoked
  `fn()` via `table.pack(fn())` outside pcall) with **36 of 109 call sites
  raw**, plus a broken `and/or` guard (`read_global(...) and f or g` —
  `read_global` always returns a truthy record table, so the fallback was
  unreachable and a missing global would have crashed the suite at that
  line).
- **13:50 version (current):** the `and/or` guard is gone; raw sites
  reduced to 25, and — decisively — I ran the suite under their bundled
  `bin/lua` 5.4.7 with a hostile mock that deletes individual globals:
  **17/17 probes (UnrealVersion, GetCurrentThreadId, FName, PropertyTypes,
  Key, UE4SS, EFindName, ModifierKey, EObjectFlags, EInternalObjectFlags,
  NAME_None, ModRef, FText, FString, UEHelpers, Loop, EngineTickAvailable)
  survived: all 353 calls always emitted, failures became `fail` rows, zero
  crashes.** The earlier "one missing global kills the suite" risk no
  longer reproduces.
- Independently reproduced their README claims: mock variant A → 353/353
  ok; variant B → 353 total, 352 ok, 1 fail; diff tool → 350 MATCH /
  3 MISMATCH, exit 1.
- **Fixed-format log claim VERIFIED on real output:** all 353 CALL rows in
  both generated logs split into exactly 6 tab-separated fields
  (malformed=0); `sanitize` escapes tabs/newlines; statuses ∈ {ok, fail}.
- Syntax: valid per my validator AND real `lua 5.4.7 loadfile`.

**Open minor:** `rtype` column is not sanitized (userdata `:type()` strings
could theoretically contain a tab — low risk); README provenance references
a UE4SS source tree that does not exist on this host (see catalogs below).

### conformance-live

- Syntax valid (my validator + real Lua 5.4.7). Not host-executable by
  design (needs live game state) — reviewed statically + structurally.
- The recorder `call()` wraps every fn in `table.pack(pcall(fn))` and the
  validator in its own pcall; scanning all 66 UE4SS touch points found raw
  invocations only inside `pcall(function() ... end)` wrappers, `call()`
  closures, or message strings. `sanitize` maps control chars to spaces so
  the 6-column TSV cannot break; truncate is byte-based (UTF-8 could split
  a rune — cosmetic only).
- Read-only contract is plausible from code: no spawn/destroy/property-
  write calls; registrations use no-op callbacks and are unregistered;
  prerequisites emit SKIP rows.
- Its README honestly states it has NOT been executed against a game from
  the Mac — consistent with the Mac-only rules.

### catalog-globals / catalog-types

1. **Provenance unverifiable on this host (important caveat).** Both
   catalogs cite file:line registration sites (e.g.
   `src/Mod/LuaMod.cpp:1521`, `include/LuaType/LuaUObject.hpp:532`) against
   the tree `.superpowers/native-dependency-spike/...`. **That tree does
   not exist anywhere on this machine** (searched the whole home/projects
   and /tmp; `LuaUObject.hpp` appears nowhere). All line-level claims are
   therefore asserted-not-verified as far as this host can tell. The
   catalogs' structure, self-consistency, and honesty of labeling are fine;
   the provenance chain is not checkable here.
2. **`ps5_runtime_status` is `""` in 98/98 globals entries** — documented
   by the author as "filled in later". Mechanical consequence: feeding
   `globals.json` to compat-checker works (schema-compatible) but every
   cataloged call lands in `unknown-status` and a clean mod grades
   `partly-compatible`. The pipeline composes; it is not yet meaningful.
3. **Sample-catalog drift:** compat-checker's own `sample_catalog.json`
   contains 10 entries absent from catalog-globals (GetPlayerController,
   FName ctor, HTTP.Get, ...) and 3 methods absent from catalog-types
   (AActor:GetActorLocation, AActor:K2_SetActorLocation, FName:IsNone). If
   the sample is meant to preview the real catalogs, they have diverged.

---

## Integration tests (L1/L2/L3) — all PASS, with one major caveat

- **I-1 (major, open): the installer and the uninstaller/conflict modules
  do NOT share a contract.** Installer writes
  `manifests/<Mod>.json` (absolute `install_path`s, loadlist row blocks,
  loadlist.txt/ploader.ini as its config model); the uninstaller consumes
  `install_state.json` + manifests with `files[].path` relative to a game
  root and `config_entries` as line lists; conflicts.py models a third
  path scheme. Nothing shipped converts between them — my L1/L3 tests
  include `adapt_install_to_uninstall()` (~40 lines) to bridge at all.
  Until an official adapter exists, "install → uninstall" is not a working
  pipeline for a user.
- **I-2 (major, open): nothing wires conflicts.py into the installer.**
  The installer will happily install a mod whose files collide with an
  installed mod's files (no conflict check anywhere in installer-core). L2
  works only because my test calls conflicts.check_install explicitly.
- L1 detect→compat→install→verify→uninstall→verify-clean: PASS
  (detector labels `ue4ss-lua`; checker grades against the status-filled
  sample catalog; installer install+verify exit 0; adapter + uninstaller
  remove files and config rows; installer-side verify then correctly
  fails).
- L2 conflicting pair: conflict detected while A installed → uninstall A →
  conflict gone → B installs: PASS.
- L3 tampered install: installer verify catches (exit 3, "tampered");
  uninstaller refuses without confirmation, removes with it: PASS.

---

## Overall verdict

**The modules are individually solid and honestly labeled; the system is
not yet a system.** Per-module quality is high — five module-authored test
suites (137 tests total) pass, and my 88 adversarial/integration tests pass
87/88 against the final state, with the one failure being a minor regex
gap. Two genuine blocker bugs in installer-core were found, proven
dynamically, and fixed by the author mid-review; conformance-static's
pcall-isolation weaknesses were hardened during the window and survived all
17 of my hostile missing-global probes, with its fixed-format log claim
verified byte-exactly.

What blocks calling this an end-to-end mod manager today:
1. no installer↔uninstaller record adapter (I-1),
2. no conflict gate in the install path (I-2),
3. no locking anywhere (concurrent installs mutually destruct),
4. catalog `ps5_runtime_status` unfilled (compat grading not yet
   meaningful against the real catalogs), and
5. catalog provenance pointing at a source tree absent from this host.

Recommended priority: (1)+(2) first — they are small, concrete, and turn
five good parts into one pipeline; then the lock file; then fill the
statuses; then re-verify provenance on the machine that owns the UE4SS
tree.

*All testing Mac-only / host-tested. Every artifact, sandbox, and result
lives under the tester module, preserved verbatim at `tests/src-tester/`
in this project (results/*.json contain the full per-test detail;
module_state.json the final digests).*
