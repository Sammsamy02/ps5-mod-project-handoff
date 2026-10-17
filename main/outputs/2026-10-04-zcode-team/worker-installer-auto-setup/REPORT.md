# worker-installer-auto-setup — REPORT (2026-10-04)

Mission: turn the two recurring manual fixes into product behavior:
1. `/data/modbridge` must be created automatically (console E2E stage 41 failed
   only because it was absent and the controller repaired it via FTP MKD).
2. The fileio SDK build must default its report path to `/data/` (the `/tmp/`
   default was manually corrected twice: InitOnce c49108fe, fileio cd9b5aec;
   `/tmp/` is unreadable over FTP from the controller).

Nothing outside `outputs/2026-10-04-zcode-team/worker-installer-auto-setup/` was
written. All changes ship as unified diffs plus applied preview copies under
`preview/`.

## Deliverables

| file | fixes |
|---|---|
| `fix1-ps5_io.h.diff` | FIX 1a (shared constant) |
| `fix1-ps5_io.c.diff` | FIX 1a (adapter open auto-creates root) |
| `fix1-compose_installer.py.diff` | FIX 1b (bootstrap step before any fileio owner opens) |
| `fix2-build.py.diff` | FIX 2a + 2b (default + /tmp/ warning) |
| `fix2-build.py.staged-copies.txt` | note: same diff applies verbatim to the 3 SELFTEST copies |
| `preview/` | applied modified copies used to generate the diffs and validations |

## FIX 1 — namespace root auto-creation

**a. Adapter (`worker-installer-ps5io/src/ps5_io.{h,c}`)**

- `ps5_io.h` gains `PS5IO_NAMESPACE_ROOT` (default `"/data/modbridge"`, the same
  default as build.py's `--fileio-root`; compile-time overridable). It lives in
  the header so the composed installer (which already includes `ps5_io.h` via
  the recorded `inc-ps5io` patch) shares the exact same constant.
- `ps5_io.c` insertion point chosen by reading the code: the shared semantic
  layer's `ps5io_open()` (both OS layers funnel through it), with a one-shot
  helper `ps5io_ensure_namespace_root()` defined after `ps5io_stat_path`
  (its dependencies). Behavior: on the first open, stat the root; if it is not
  an existing directory, `ps5io_mkdir_p()` it (0755), then proceed with the
  real open. Deliberately best-effort — a bootstrap failure is never forced
  onto opens that do not need the root (read-only opens, host tests); the real
  open then reports its own accurate errno. No new external symbols: it uses
  only the already-bound `sceKernelStat`/`sceKernelMkdir` exports, so the
  composition's nm import-provider check is unaffected. The installer core is
  single-threaded, so a plain static once-flag suffices.
- Gated `#ifndef PS5IO_HOST`: host builds (unit tests, host smoke flavor) get a
  no-op so nothing ever creates `/data` on the development Mac.

**b. Composed installer (`worker-installer-composition/compose_installer.py`)**

- New recorded patch `"namespace-bootstrap"` appended to the existing
  exact-anchor `PATCHES` chain (so it is hashed/receipted like every other
  adaptation in `receipts/patches.json`). Anchor:
  `if (mkdirs((char *)g_state_dir)) die("cannot create state dir %s", ...)` in
  `main()` — verified to occur exactly once in `worker-7-installer/src/modinst.c`;
  it sits before the first filesystem operation of any command (state-dir
  mkdir, `records_load`, `recover_all`, dispatch), i.e. before any fileio owner
  opens. Injected code: stat `PS5IO_NAMESPACE_ROOT`; if it is not an existing
  directory, `ps5io_mkdir_p` it; on failure `die("cannot create namespace root
  %s: %s", ..., ps5io_status_string(rc))` — the old silent stage-41 failure
  becomes an early, named error. Wrapped in `#ifndef PS5IO_HOST` so the host
  smoke flavor is byte-for-byte unaffected.
- FIX 1a + 1b compose deliberately: the explicit bootstrap surfaces genuine
  failures loudly; the adapter self-heal covers any open path that slips past
  it (e.g. the payload's own later opens).

## FIX 2 — report-path default (`build.py`)

Diffed against the canonical product copy
`worker-fileio-apply/vendor/windows-candidate/build.py`:

- **2a**: argparse default line 32 `/tmp/modbridge-owned-dll-candidate.jsonl`
  -> `/data/modbridge-owned-dll-candidate.jsonl`; help text now states why.
- **2b**: immediately after `p.parse_args()`, if the report path starts with
  `/tmp/`, a warning is printed to **stderr** and the build continues (keeps
  host-only tests usable); added the previously missing `import sys`.
- `fix2-build.py.staged-copies.txt` documents that all three
  `worker-fileio-prep/SELFTEST/.../windows-candidate/build.py` copies carry the
  identical anchors (verified 1/1/1 each), so the same diff applies verbatim.

## Validation evidence (all local, inside this output dir)

1. Every replacement was exact-string with asserted occurrence count 1.
2. `preview/build.py`: `py_compile` OK.
3. `preview/ps5_io.c` compiled with `clang -std=c11 -Wall -Wextra -Werror`:
   host branch (`-DPS5IO_HOST=1`) OK **and** console branch OK — both survive
   -Werror.
4. Full patch-chain replay: imported `preview/compose_installer.py` and applied
   all **36** recorded patches against the actual `worker-7-installer/src/modinst.c`
   with per-patch occurrence assertions — all pass, `namespace-bootstrap`
   applies exactly once and the patched main contains `PS5IO_NAMESPACE_ROOT`.

## Honesty / bounds

- Diffs are review-ready but not applied to the read-only originals; nothing
  was rebuilt end-to-end (no `compose_installer.py` run, no SDK rebuild).
- The console-branch compile used host clang, not the pinned prospero-clang
  sysroot; the real console object still needs the pinned build.
- No console was contacted: the on-target effect of the namespace mkdir and of
  the `/data/` report path remains target-unverified until the next console E2E
  run. The adapter self-heal relies on `sceKernelStat`/`sceKernelMkdir`
  semantics of the pinned exports (already bound and nm-verified elsewhere),
  but their behavior on a fresh console filesystem is unproven.
- No network, no console execution, no nested subagents. Call budget: ~18 of 40.
