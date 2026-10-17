# Agent IB — writable area outside admission (Claude round-8 item 5)

2026-10-11. Battery-safe: code + host tests only — NO builds (one Prospero
`-fsyntax-only` per the GA precedent), NO console contact, NO tree/driver/
CHANGES edits. Edited files: `round4-setaside/LuaPathShim.{cpp,hpp}` (the
set-aside round-5 content — editing them now is correct),
`host-lua-harness/path-shim.lua` (the rule twin; single source of truth on
host), `host-tests/lua-path-shim-test.lua` (+ its runner unchanged).

| File | sha256 | lines |
|---|---|---|
| `round4-setaside/LuaPathShim.cpp` | `821d74a261a177ca483a518607c1cb91fc56974a86c697ff9e4e069770f3aeb3` | 1656 (was 851) |
| `round4-setaside/LuaPathShim.hpp` | `0f5a043d741d828355220eb8eaa5bae3304c701f1a2b27fa36c6e35f15cea5be` | 121 |
| `host-lua-harness/path-shim.lua` | `9e76eb6ffe99ff43fa8fe31952ef1760d4f668e6ed74d2fdd2f5fdd76316cd39` | 518 |
| `host-tests/lua-path-shim-test.lua` | `ade2337230aab9d60584ae1db8df10c3629720a4c09104a062d3fdf79883dc32` | 342 |

## 1. MECHANISM VERDICT (diagnosed first, as ordered)

**The ENOENT is OS-level /data unreachability of the game process after the
module_start window — NOT a modforge virtual file layer inside `roots/`.**
The task's hypothesis ("modforge virtualizes only within roots/, so a
writable area outside roots/ works with plain fopen") is **refuted** by the
sources:

1. **modforge intercepts NOTHING.** `mfa_activate` reads via plain POSIX
   `open/read` during module_start (`modforge_activation.c` `posix_open`
   :41-48, `read_exact` :163-208; vendored copy
   `.superpowers/native-dependency-spike/claude-takeover-43a96969/source/runtime/`,
   byte-identical to every lineage). No `fopen`/`open` hook exists anywhere
   in the composition. The only "virtual layer" is *positive serving*:
   Lua `require`/`dofile`/`loadfile` resolve through the native module
   searcher against the in-memory NativeModSource built from the activation
   (LuaMod.cpp:1073-1112; `activated_mod_source.hpp`: "No filesystem access
   occurs in this adapter"). The patched liolib's `io_open` calls plain
   `fopen` (`ue4ss-stdio-consumer-adaptation-1/patched/liolib.c:270-277`) —
   modforge is not in that path at all.
2. **The contract is game-process-wide for /data, not roots-scoped.**
   `modforge_activation.h:6-8` (byte-identical in every lineage since l22):
   "the only moment /data is readable from the game process (a fresh open
   afterwards fails with ENOENT)". The runtime's own writers ALL obey the
   retained-fd law — diagnostic channel fd (`diagnostic.c:56-67`), UE4SS.log
   fd (`runtime_module_entry.c:183-221`: "later fresh opens fail by
   contract, so this happens strictly before any runtime thread starts"),
   runtime-status.txt fd (`mfa_status`, opened at module_start and kept
   open), RuntimeAckSink, and the copyin sink bank (opened during
   module_start's LMF capture, retired at admission completion —
   agent-Q-copyin-probe.md table). Zero post-module_start fresh-open
   successes exist anywhere in the console record.
3. **Console proof (round-4 session):** at 2026-10-11 08:47:52 — ~3 s after
   module_start (08:47:49), with the shim SET ASIDE, raw-liolib
   `io.open("/data/modbridge/roots/PPSA33286/ue4ss/Mods/ueselftest/progress.txt","w")`
   and its source-relative twin BOTH failed:
   `[ST2] progress-io unavailable (resume disabled)` / `pf=false` (log
   L820-826, verified verbatim by agent-IJ). A plain OS create failing
   ENOENT-class = the path prefix is not open-able from the game process.

**Writable-root evidence verdict:** NO /data subpath is proven openable from
the game process at runtime — `/data/modbridge/writable`,
`/data/homebrew/modbridge-writable` and `/data/tmp` are all subject to the
same documented contract; no console experiment distinguishes any of them
(ftpsrv's /data writability is a ROOT-LEVEL process and irrelevant to the
game process's opens). First choice **/data/modbridge/writable** stands
(sibling of `roots/`, one deploy surface, manager-visible); the second
choice buys nothing — same tree, same contract. Therefore the shim cannot
DEPEND on the sandbox being OS-writable: it attempts the physical open first
(if the console ever permits it, writes persist and are ftpsrv-visible) and
falls back to a per-mod **in-memory spill**, so settings saves, logs and new
files always work within the process. Console probe (one write+readback
marker at the next sanctioned launch) settles the physical question.

## 2. SANDBOX SHAPE (implemented, mirrored 1:1 in the twin)

- **Write resolution:** confinement to the calling mod's folder is
  UNCHANGED (key-space boundary + pinned refusal — the admitted set stays
  read-only, defense-in-depth), but the write now LANDS at
  `<writable-root>/<modname>/<path relative to the mod folder>`,
  path-shape preserving: `io.open("ue4ss/Mods/TownfallMiniMap/settings.txt","w")`
  → `/data/modbridge/writable/TownfallMiniMap/settings.txt`;
  `Scripts/diag.log` → `.../TownfallMiniMap/Scripts/diag.log`. The mod root
  is never a write target anymore (round-4 proved it cannot be).
- **Writable root derivation:** strip trailing `/roots/<title>` → append
  `/writable` (`/data/modbridge/roots/PPSA33286` → `/data/modbridge/writable`);
  any other root shape → `<root>-writable` (host fixtures stay out of
  audited trees). Test override: `set_writable_root_override` (C++) /
  `writable_root=` (twin).
- **Read order:** mod-root physical (case-resolved) → sandbox physical →
  in-memory spill. Pre-staged (loadlisted, physically present) and
  previously-written files both work; the mod keeps ONE namespace (its own
  logical paths) — write then read at the same logical path round-trips.
- **Spill backing:** per-context map keyed by sandbox path, flushed on every
  write (8 MiB/file + 16 MiB total caps, MFA's per-file cap as precedent);
  case-insensitive lookup fallback; liolib-shaped `MemFile` handle
  (`write/read("l"/"L"/"a"/"n"/<n>)/lines/seek/flush/close`, `__gc` never
  touches the context, so state-teardown order can never dangle). Fills in
  only when the backend `fopen` of the sandbox path fails (console reality)
  — on host tests the twin's virtual store models exactly this.
- **os.remove/os.rename:** sandbox-confined — they act on the sandbox mirror
  ONLY (the mod root, admitted set included, is untouchable); spill entries
  erase/move alongside the physical attempt.
- **Marker:** one line per install, first-10 budget:
  `[SHIM] write-area <mod> -> <writable-root>/<mod>`.

## 3. WHY NOTHING NEEDS ADMISSION (the point of the sandbox — DESIGN note)

MFA reads ONLY `<root>/loadlist.txt` and the files its rows list
(agent-I audit §1: pass-1 validates rows, pass-3 preloads listed files;
there is no directory walk of the mod root, and nothing ever reads outside
it). The writable area sits at `/data/modbridge/writable/<mod>/...` — a
sibling of the admitted `roots/` tree — so it can never be admission-checked,
never trips the size-exact all-or-nothing gate, needs NO loadlist row, NO
re-pin, NO prereq, and its existence cannot change next-launch admission
(the round-4 `-6` incident class — a row naming a missing file — is
impossible here because no row ever names it). Optional deploy note for ops:
pre-creating `/data/modbridge/writable/` via ftpsrv (root-level, like every
other deploy upload) is harmless and helps only IF the console turns out to
permit physical writes; the spill makes it functional either way.

## 4. Host verification (all run this session)

1. **Rule-contract tests — 71/71 PASS** (`host-tests/run-path-shim.sh`,
   was 38/38): all GA sections kept (updated §5 accept-matrix now asserts
   SANDBOX targets + 2 new traversal rejections; §8 asserts store-at-sandbox
   + the single-namespace round-trip), plus new §10 derivation/override/
   write-area fact, §11 read-order (mod-root wins / store-backs-new-file /
   io.lines), §12 remove/rename confinement (incl. pinned + cross-mod
   refusals), §13 sandbox escape matrix (traversal, absolute-path and
   writable-root-literal spellings all refused or confined inside the mod's
   own sandbox — never a sibling).
2. **host-tests/run-all.sh — GREEN** (prop-parity ALL PASS + path-shim 71/71).
3. **host-lua-harness 27/27 law holds**: full matrix rerun through the
   revised twin — all 27 baseline verdict lines byte-identical vs
   `runs-baseline/` (23 LOADS-CLEAN + MiniMap io-off :251 + BPModLoader
   :117 x2 + jsbProfiler io-off :115; spawner console copy LOADS-CLEAN both
   modes).
4. **Prospero `-fsyntax-only` on the revised TU — exit 0, zero
   diagnostics** (EY-TARGET-COMPILE.json flag base + round4-setaside
   include; one real fix found by the check: `handle` if-init scoping in
   io_lines_wrapper).

## 5. Round-5 restore addendum (for root — the shim returns WITH the area)

The restore procedure in ROUND4-CONTENTS-DECLARATION.md §OUT is UNCHANGED
and remains exactly correct: the public surface did not move
(`LuaPathShim_install(L, mod_name, modbridge_root)` signature identical;
`LuaMod-insertion-block.txt` still valid verbatim; NEW_UNITS rows and the
3 existing `[SHIM]` manifest entries unchanged). Notes for staging:

1. Optional 4th `[SHIM]` manifest entry: the new first-10 marker
   `[SHIM] write-area <mod> -> /data/modbridge/writable/<mod>` (root's
   call whether to pin it in the launch manifest's grep list).
2. The writable root constant is DERIVED (no new pin); if ops prefer an
   explicit deploy, pre-create `/data/modbridge/writable/` over ftpsrv —
   no loadlist/loadrow consequences (see §3).
3. Console probe for the open mechanism question (next sanctioned launch):
   one shim-mediated write+readback of e.g. `ue4ss/Mods/ueselftest/progress.txt`
   at boot — `[SHIM] write-area` + `[ST2] boot ... pf=<path>` proves the
   general fix; physical-vs-spill is distinguishable by the
   `io-open resolved=.../writable/...` line followed by success/failure of
   the backend (and a post-session ftpsrv `ls` of the writable root).

## 6. Known boundaries (honest)

- **Cross-launch persistence is unproven**: the spill is process-memory.
  ueselftest crash-RESUME across processes needs the physical sandbox to be
   console-writable (probe above) or a future runtime-side retained-fd
   flusher (would touch runtime_module_entry.c — out of this task's file
   scope). Within a process, saves/logs/new files always work.
- `io.open` READS of admitted files still go physical-first; per the same
  contract they may fail at runtime (require/dofile are unaffected — the
  searcher serves them from the activation). Serving io.open reads from the
  activation snapshot is the API-PARITY-TABLE §5 "dual read route" — a
  separate round-5 seam (needs NativeModSource access), not added here.
- Twin boundaries: store reader supports one format per `:read` call;
  `io.lines` format args pass through on physical hits, default `"l"` on
  store hits; `r+` on a spill-missing file refuses (liolib contract), the
  twin store always allows (documented divergence).
- Calling-mod detection's tail-call boundary (GA note) is unchanged: the
  fallback is the owning mod; the sandbox then uses that mod's name.
