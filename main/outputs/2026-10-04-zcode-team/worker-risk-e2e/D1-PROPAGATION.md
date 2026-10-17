# D1-PROPAGATION.md — worker-risk-e2e (team 2026-10-04)

Purpose: map exactly where the D1 fix (journal-before-records commit-order
bug found by worker-uninstall-fault-expansion) must be propagated so the
console E2E run uses D1-safe code. All shas below were RECOMPUTED this
session (sha256, first 16 hex chars shown; files match the delivered pins —
no drift). Anchor occurrence counts were verified by direct string counting
in each chain's `composition/vendored/modinst_core.c`.

## 1. The defect and the fix (baseline facts)

- **D1**: `cmd_install` (used by install AND uninstall via `--remove`) and
  both `recover_title` commit paths deleted the journal BEFORE committing
  records. A death in that window leaves journal durably gone + records
  stale — unrecoverable ("records lie": installed claimed, files gone).
  Demonstrated on the baseline by worker-uninstall-fault-expansion
  (`receipts/d1-window-demo.log`).
- **Fix**: 4 exact-anchor patch tuples on `modinst_core.c` (records commit
  BEFORE `journal_delete` at all three sites; new `pre-journal-del` crash
  probe point; recovery repaired idempotently). The tuples touch only
  flavor-independent core code — identical text for host and console
  flavors. Baseline binary provenance was proven: uf's
  `composition/build_baseline/modinst_composed` is byte-identical to
  worker-zip-to-mfb's delivered pin (`947affc87b422197…`).

The 4 tuples (verbatim anchors live in `worker-uninstall-fault-expansion/
compose_fault.py`; per-tuple old/new sha256 in its
`composition/receipts/patches.json`, each `occurrences==1`):

| tuple | site | effect |
|---|---|---|
| `uf-order-commit-a` | cmd_install swap-phase tail (`maybe_crash("prevdel"); journal_delete(title);` before the commit block) | removes the early `journal_delete`; commits records first |
| `uf-order-commit-b` | cmd_install commit tail (`records_save(); say("installed …"); }`) | adds `maybe_crash("pre-journal-del")`, moves `journal_delete(title)` after the closing brace |
| `uf-recover-forward` | recover_title "finished swap forward" path | `records_commit_from_root` before `journal_delete` |
| `uf-recover-commit` | recover_title "committed" path | same reorder |

Post-fix verified state: patched binary sha256 `46d873455fa7f6e0…`,
49-tuple chain, patched core `f9110fb4531df28b…`; suites green: 75/75
bridge regression + 186/186 fault tests on the patched binary.

## 2. Chain-state matrix (verified current, 2026-10-04)

Chain lineage: worker-installer-composition (`compose_installer.py`,
`e9074052…`) → worker-e2e-final (41 modinst.c tuples) → worker-zip-to-mfb
(+4 zb-*, 45) → { worker-f-r2-fix (+2 r2-*, 47) , worker-uninstall-fault-
expansion (+4 uf-*, 49) } and → worker-zip-bridge-console-link (45, console
link). The two 45-successors have DIVERGED (r2 fixes vs D1 fixes) — see §4.

| chain root (worker) | chain-root file + current sha | tuples | core sha | delivered binaries | D1 fixed? |
|---|---|---|---|---|---|
| worker-e2e-final (the composition) | `compose_e2e.py` = `3f310101d78a506e…` | 41 | `5900023950ab30ff…` | host `c1a594c33ce74005…`; console ELF `3860cd561645cb55…` (664400 B, 116 imports) | **NO** — all 4 uf anchors present, occurrences==1 each |
| worker-zip-to-mfb (the bridge chain) | `compose_bridge.py` = `0c9ebc2cc48eef96…` | 45 | `6c55f1e1e477605b…` | host `947affc87b422197…` (this IS the uf baseline) | **NO** — all 4 uf anchors present, occurrences==1 each |
| worker-f-r2-fix (F-R2 lineage) | `compose_bridge_r2.py` = `ad8fa1d6a739a524…` | 47 | `67a38c1b8959ed6b…` | host `f2215f2ffd92b63c…` | **NO** — 3 of 4 uf anchors present; `uf-order-commit-b` has **0 occurrences** (r2 insertion broke it — see §4) |
| worker-zip-bridge-console-link (console bridge link) | `compose_console_bridge.py` = `ab9de3dd53fe208a…` | 45 | `6c55f1e1e477605b…` (same as zip-to-mfb) | **console ELF `cfa96ab5aa408491…` (681632 B, 116 imports) = the CURRENTLY STAGED PAYLOAD**; host suite binary `947affc87b422197…` | **NO** — all 4 uf anchors present, occurrences==1 each |
| final console E2E staging | `worker-e2e-console-staging/STAGING-MANIFEST.md` | pins payload `cfa96ab5…` + corpus zips/mfbs | — | staged set per manifest | **NO** — pins the pre-D1 payload; must re-pin after the D1 relink |
| worker-uninstall-fault-expansion (fix origin) | `compose_fault.py` | 49 (=45+4 uf) | `f9110fb4531df28b…` | host `46d873455fa7f6e0…` | **YES** (reference implementation; no r2 tuples, no console link) |

Anchor verification detail (occurrence counts of the exact uf OLD anchor
text per chain core, counted this session):

| core | uf-a | uf-b | uf-fwd | uf-cmt |
|---|---|---|---|---|
| e2e-final (41) | 1 | 1 | 1 | 1 |
| zip-to-mfb (45) | 1 | 1 | 1 | 1 |
| f-r2-fix (47) | 1 | **0** | 1 | 1 |
| zb-console-link (45) | 1 | 1 | 1 | 1 |
| uf-patched (49) | 0 | 0 | 0 | 0 (post-state, expected) |

## 3. Per-root propagation steps

### 3.1 worker-zip-to-mfb (the bridge chain) — D1 NOT fixed
- **Patch application**: vendor `worker-zip-to-mfb/compose_bridge.py`
  (verify `0c9ebc2c…`), append the 4 uf tuples to `EXTRA_PATCHES` exactly
  as in `compose_fault.py` (order: a, b, recover-forward, recover-commit;
  each asserted occurrences==1), rebuild `--flavor host`. Expected patched
  core: `f9110fb4…` (byte-identical to the uf chain — same 49-tuple
  sequence); expected host binary: `46d87345…`.
- **Regression**: `test/run_bridge_tests.sh` 75/75 + the uf fault suite
  (186/186; `d1_window_probe.sh` must now show the repaired window).
- **Status note**: this root's delivered binary is already superseded by
  the uf chain; propagate only if a consumer rebuilds from this driver
  directly.

### 3.2 worker-e2e-final (the composition) — D1 NOT fixed
- **Patch application**: vendor `worker-e2e-final/compose_e2e.py` (verify
  `3f310101…` against `final-provenance.json`), append the same 4 uf
  tuples → 45-tuple chain, rebuild `--flavor all`.
- **Verification**: all 4 anchors match verbatim (verified); after uf-b the
  core text has records-commit-before-journal_delete at all three sites;
  suites: smoke_host 26/26 + smoke_e2e 31/31 + uf fault suite.
- **Note**: prefer NOT to relink the E2E console payload from this root:
  the staged payload lineage (zb-console-link) is a strict superset of
  this chain (bridge TU included). e2e-final's chain is the fallback base
  only if the bridge is being dropped.

### 3.3 worker-f-r2-fix — D1 NOT fixed; anchor conflict
- **Conflict (verified)**: `r2-install-status-ok` inserts
  `if (mods_out[0]) e2e_status_event("stage-3-install","ok",mods_out);`
  immediately after the `say("installed …")` line, so `uf-order-commit-b`'s
  OLD anchor (`records_save(); say(…); }\n}`) no longer occurs (count 0).
  The other three uf anchors still match exactly once.
- **Patch application (verified compatible)**: apply uf BEFORE r2 —
  vendor `compose_bridge_r2.py`, reorder `EXTRA_PATCHES` to
  `[…zb-usage-bridge, uf-order-commit-a, uf-order-commit-b,
  uf-recover-forward, uf-recover-commit, r2-exit-shim,
  r2-install-status-ok]` (51 tuples). I simulated uf-b's NEW text applied
  to the 45-chain core: the r2 status anchor still occurs exactly once
  afterward, so NO text edits are required in either tuple set — only the
  append order changes.
- **Post-conditions**: per-tuple new_sha256 equality (from
  patches.json/compose_fault.py anchors); bridge suite 87/87 (includes the
  12 F-R2 checks) + uf fault suite 186/186. The merged core will NOT hash
  to `f9110fb4…` (that pin is the 49-tuple uf-only chain) — pin the merged
  core/binary shas fresh at build time.

### 3.4 worker-zip-bridge-console-link (the staged console lineage) — D1 NOT fixed; RECOMMENDED BASE
- **Why**: it is the only root with a linked console payload, and its
  payload (`cfa96ab5…`, 681632 B, 116 imports, missing=[]) is exactly what
  `worker-e2e-console-staging` verified and staged.
- **Patch application**:
  1. Vendor `compose_console_bridge.py` (verify `ab9de3dd…`).
  2. Insert the 4 uf tuples into the inner script's `EXTRA_PATCHES`
     immediately after `zb-usage-bridge` (i.e. BEFORE any r2 tuples if the
     F-R2 fix is also being folded in per §3.3; minimal path = uf only).
  3. `--flavor console`: the patched `modinst_core.c` must equal the
     uf-chain core `f9110fb4…` (minimal path; the C text is
     flavor-independent) — assert before linking.
  4. Link the console payload; provider check must stay
     116 imports / missing=[] (the uf tuples add no imports); record the
     new size (expect +~200–400 B of text; record exact) and NEW sha.
  5. `--flavor host` + suites: bridge 75/75 (minimal) or 87/87 (with r2),
     uf fault suite 186/186.
- **Rollback**: the tuples are exact-anchor splices with recorded shas;
  reverting = rebuild from the unmodified vendored driver (payload
  `cfa96ab5…` remains valid evidence of the pre-D1 lineage).

### 3.5 Final console E2E staging — must consume the D1-safe payload
- `worker-e2e-console-staging/STAGING-MANIFEST.md` pins
  `cfa96ab5aa408491…` (681632 B). After the §3.4 relink this pin is stale:
  update the manifest's payload row (new sha + size), keep the corpus pins
  (movement-speed-2x.zip `3850a2b5…`, mm-skin.mfb `7b9888b9…`,
  mm-lua.mfb `eefc774a…`, infinite-stamina-v1.zip `61cd9c81…`) unchanged,
  and bump the receipt reference to the new link receipt.
- Applying D1 changes NO user-visible command output and no exit codes (the
  fix only reorders durable writes and adds a crash probe point), so
  STEP-BY-STEP expected stdout/e2e-status lines stay valid EXCEPT:
  GO-NOGO/STAGING identity checks must accept the new sha — and per F-R3
  (RISK-REGISTER R-E2E-FR3) the console link is not bit-reproducible, so
  verify by size + 116-import set + bridge link-map symbols
  (`mi_bridge_*`), re-pinning sha deliberately with a receipt.
- Staging-channel reminder (R-FTP-01): verify the staged fself by the size
  law (readback = inner-ELF phdr-covered prefix) + ELF magic, never
  full-hash; freshness oracle = runtime aligned-log mtime.
- Honesty gates unchanged: running=1 stays SIMULATED (R-ACK-01); stage-1
  native download still deferred (R-DL-01; owner-placed-zip probe-A path);
  G1/G3 still open (R-ARGV-01/R-G1-02, R-G3-01).

## 4. Recommended canonical order (one relink covers D1 + F-R2)

Merged chain = **51 tuples** in this order:

```
[35 base tuples] + [6 e2e-*] + [4 zb-*] + [4 uf-*] + [2 r2-*]
  └─ worker-e2e-final        └ zip-to-mfb   └ D1 fix  └ F-R2 fix
```

- Driver: vendored `compose_console_bridge.py` + one new recorded splice
  that appends the uf block BEFORE the r2 block in `EXTRA_PATCHES`
  (reorder only; both tuple sets' anchors verified compatible in this
  order: uf-b OLD matches the 45-core once; r2 status OLD matches the
  uf-modified core once — simulated this session).
- Build both flavors; assert core equality expectations; run 87/87 + 186/186.
- If F-R2 is deferred: minimal D1-only path is 49 tuples (uf block last),
  which reproduces the uf chain exactly on host (`f9110fb4…` core,
  `46d87345…` binary) and needs only the console link.

## 5. Open items this mapping does NOT solve
- The r2/uf branch divergence in DELIVERED binaries (only the merged
  rebuild resolves it); merged-core/binary shas are pinned at build time.
- Console-side proof of the libc dirent/fsync route that D1's recovery
  depends on (RISK-REGISTER R-LIBC-01) — E2E stage-3 rollback probes.
- Any console execution itself: staging, launch, dispatch remain
  controller-owned; this worker touched no console and modified nothing
  outside `outputs/2026-10-04-zcode-team/worker-risk-e2e/`.
