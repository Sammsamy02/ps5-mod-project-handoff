# REPORT — worker-stray-root-hardening

2026-10-05 · LOCAL-ONLY · Writes confined to this directory; frozen packaging
file untouched (sha verified unchanged before and after everything).

## Delivered

1. **`find_root_v2.py`** (sha256 `5590f8bd…bc2`) — versioned drop-in for the
   frozen `package_prx.py::_find_root()` contract (frozen sha `e6fbe6c5…`,
   read at lines 71–86, never modified):
   - STRONG marker required: `.superpowers/native-observations/` **and** ≥1
     corroborating marker, chosen from what actually exists at the true root
     (enumerated 2026-10-05): the ledger file
     `native-observations/transport-experiments-progress.json`, root
     `README-RETURN.md`, `.superpowers/sdd/`. The 05:42 stray (only
     `native-dependency-spike`) matches none.
   - Walks start + up to **8 ancestors** (frozen checked 6 nodes).
   - Ambiguity FAILS LOUD: `RootResolutionError` (a `RuntimeError` subclass,
     so existing handlers keep working) listing every candidate and every
     weak-marker-only decoy (bare `.superpowers`, or `native-observations`
     without corroboration). `--root` override honored — and itself verified,
     with `--allow-weak-override` as the explicit escape hatch. CLI included
     (`--start/--root/--weak-markers/--max-ancestors/--json`).
   - Return semantics matched to the frozen call sites (line 85
     `ROOT = _find_root()`, consumed by `SPIKE` line 86 and `PIN` lines
     89–123): no-arg call → resolved `Path`; failure → `RuntimeError`.
   - Policy knob `weak_markers=fail|warn|ignore`; default `fail` (a bare
     `.superpowers` on the walked path is exactly the incident condition and
     must not resolve silently).
2. **`host-test/`** — 9/9 PASS (`PASS-receipt.json`, `console.log`): clean
   tree; bare-`.superpowers` decoy between team and root → loud failure; decoy
   with `native-observations` but no corroboration → loud failure; 7- and
   8-hop nesting resolves (beyond the frozen 6); 9 hops fails loud;
   `--root` rescue + bad-override rejection + CLI subprocess check; multiple
   strong candidates → loud failure; contract checks (RuntimeError subclass,
   resolved Path, warn policy, bad policy rejected); and a REAL-tree check:
   v2 from the frozen file's directory equals the frozen module's own
   `ROOT` (imported read-only), the quarantined dir name classifies as
   non-marker, and the frozen sha is unchanged before/after.
3. **`ADOPTION.md`** — all 12 frozen-file-loading drivers grepped and grouped
   into patterns with exact one-line import + guard snippets: 9 × P1
   (`PKG_FIXED`/`check_sha`: comp6-driver, comp6-drybuild, d1fix, l11pre,
   drycompose, l10pre, walkguard, r3-armed, comp5-rebuild), 3 × P2
   (`FIXED`/`EXPECTED_SHA`: composition-3, composition-4-fix,
   comp5-minimal). Walkguard WGP E4 and r3 L10R3 E3 stay ARMED, not replaced.
   Gate-check scripts only mention the file in comments — no change. Vendor
   `build.py` `parents[2]` copies identified as the CAUSE class with the v2
   call as the suggested replacement there too.
4. **`LEDGER-ADDENDUM.md`** — PROPOSED text only (two entries in the ledger's
   exact `zcode_worker_*` shape): stray creation (05:42:47, seh64 vendor run,
   unreported anywhere), the two fail-closed wrong-root receipts (walkguard
   run-a `2026-10-05T07:00:29.111910`, 12 gaps, artifact preserved; r3 run-a
   ~07:06, log-preserved), the controller's executed quarantine (exact
   rename, bytes preserved, resolution restored), and the v2 hardening path.
   NOT appended — controller appends verbatim if accepted.

## Bug caught during my own testing (honesty item)

T3 caught a real bug in my first v2 draft: `_marker_missing()` inverted the
dir-kind check, so `.superpowers/sdd` "corroboration" was awarded to any
`.superpowers` dir — which would have silently downgraded the no-corroboration
decoy to a strong root. Fixed; 9/9 re-run. That case is in the suite
permanently.

## Limits (what this does NOT claim)

- v2 is host-tested only. No packaging driver has been modified and no
  packaging run has executed with v2 adopted; adoption is a controller /
  next-bounded-revision decision (drivers are provenance of existing pinned
  receipts — ADOPTION.md says new versioned filenames, never in-place edits).
- The corroborating-marker set reflects today's true root. If the ledger file
  is renamed, `README-RETURN.md` moves, or `sdd/` is restructured, v2 must be
  re-pinned (and re-pinned in any adopting drivers).
- The proposed ledger text's `utc` fields are placeholders for the controller
  to set at append time; the r3 run-a time (~07:06 local) is audit-inferred,
  not receipt-stamped.
- No engine, console, provider, or network contact; no mutation anywhere
  outside this directory (the only filesystem writes are the deliverables and
  the test tempdir, which is removed on PASS).
- Frozen `package_prx.py` byte-identity re-verified at the end:
  `e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313`.
