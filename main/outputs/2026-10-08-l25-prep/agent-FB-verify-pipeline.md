# agent-FB — post-build pre-upload verification pipeline

- agent: FB (verify pipeline)
- date: 2026-10-10 (local 2026-10-09 late)
- task: catch "the fix didn't compose in" failures BEFORE wasting a console launch
- console contact: NONE (pure Mac-local gate; runs before upload, honors the
  permanent PS5 rules — never uploads, never launches)

## Deliverables

| file | role |
|---|---|
| `harvest/verify_build.sh` | the gate: marker grep + PRX identity (magic/sha256/size) vs FINAL-RECEIPT + timing stamp |
| `harvest/grep_markers.py` | helper it calls: marker scan in BOTH UTF-8 and UTF-16LE, exit 1 on missing CRITICAL markers |

One-line use (from any directory):

```
/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/verify_build.sh <prx-path> [receipt-path-or-dir]
```

Example against the current resident-build artifact:

```
harvest/verify_build.sh outputs/2026-10-08-l26-package-A.seamfix-192411/pass-1/modbridge_supplier_composition.prx
```

## Design

1. **Marker grep** (delegated to `grep_markers.py`, registry defined there):
   - CRITICAL (gate, exit 1 when missing): `[LUA-FN]`, `[ST] lua-ffo`,
     `[FN] pool-decoder verified`.
   - Informational (report only): `[DRY]`, `[FN] seam refused`,
     `[FFO] query-fallback`, `vmt_engine_tick_drain`, `[ST] fname`,
     `[Seeds][L27`.
   - Every marker is searched as UTF-8 AND as UTF-16LE (each char + 0x00;
   non-ASCII handled via the utf-16-le codec). Per-marker verdict:
   `FOUND (UTF-8) / FOUND (UTF-16LE) / FOUND (both) / MISSING` with counts.
   - `--json` and `--marker <text>` (ad-hoc probes, non-gating) supported.
2. **PRX identity**: magic must be `4f153d1d` (fake-signed SELF). `5414f5ee`
   is flagged (readback-acceptable, never our output); `7f454c46` (plain ELF
   / decrypted view) is a hard FAIL — never upload.
3. **FINAL-RECEIPT match**: sha256 + bytes must equal the receipt's `prx`
   block. Receipt discovery: explicit arg (file or dir) or auto-discovery
   walking up from the PRX (max 4 parents), preferring a receipt whose
   `prx.sha256` equals the artifact. `prx.path` in receipts is NOT trusted
   (it is stale across same-tree rebuilds — e.g. the seamfix receipt points
   at the l26-package-A path while carrying the seamfix sha).
4. **Timing stamp**: one `VERIFY-TIMESTAMP utc=... prx=... sha256=... bytes=...
   magic=... elapsed_ms=... result=...` line at the end.
5. Exit codes: `0` PASS; `1` CRITICAL marker missing; `2` usage/env;
   `3` bad magic; `4` receipt mismatch/not found.

## Test results against real PRXes (all in the package dirs)

| # | artifact | sha256 (head) | magic | receipt | markers | exit |
|---|---|---|---|---|---|---|
| 1 | `2026-10-08-l26-package-A.seamfix-192411/pass-1/...prx` (currently resident build) | `961aa8ca` | `4f153d1d` OK | auto-discovered, MATCH | `[LUA-FN]` MISSING, `[ST] lua-ffo` MISSING, `[FN] pool-decoder verified` FOUND (UTF-16LE) | **1** |
| 2 | `2026-10-08-l26-package-A/pass-1/...prx` (58a30cd1 upload in LAUNCH-READY-A-20261010T002544Z) | `58a30cd1` | `4f153d1d` OK | auto-discovered (regenerated), MATCH | same critical misses; `[FFO] query-fallback` + `[FN] pool-decoder verified` FOUND (UTF-16LE); `[ST] fname` FOUND (both) | **1** |
| 3 | `l25-prep/packet/attachments-l26/prx-variantA/...prx` with explicit receipt dir `build-receipts-root` | `5ea2c845` | `4f153d1d` OK | VARIANT-A preferred over B candidate, MATCH | same critical misses | **1** |
| 4 | `...seamfix-192411/pass-1/bare-original/...prx` (pre-sign ELF) | `abd6febb` | `7f454c46` FAIL | SHA-MISMATCH reported | — | **3** |
| 5 | synthetic all-markers artifact + matching receipt (built+deleted in /tmp, PASS-path proof only) | — | `4f153d1d` OK | MATCH | all 9 markers FOUND (both encodings) | **0** |
| 6 | same but tampered receipt sha | — | `4f153d1d` OK | SHA-MISMATCH | all markers present | **4** |

Runtime on a real 31.4 MB PRX: ~0.35 s wall (timing stamp `elapsed_ms≈350`).

The headline result: **both builds that went to console in the seam round
genuinely lack `[LUA-FN]` and `[ST] lua-ffo` even in the bare pre-sign ELF**
— composition-out happened at link time, invisible to source review, and this
gate flags it before upload. Exactly the failure class it was built for.

## Encoding evidence (why both encodings are searched)

Verified against the real artifacts and the composed sources:

- `STR()` wide literals (UE4SS StringType = char16_t on Prospero) land as
  UTF-16LE: `[FN] pool-decoder verified` (NameTypes.cpp:445),
  `[FN] seam refused` (NameTypes.cpp:73), `[FFO] query-fallback`,
  `[ST] fname` wide variant — all found ONLY as UTF-16LE in the PRXes.
- Narrow literals (char*/snprintf/fmt::format) land as UTF-8:
  `[LUA-FN] in='%.*s' len=%llu ...` (LuaFNameBridge.hpp:448 snprintf
  template), the Lua seam chunk `k_seam_chunk`, `[ST] fname` narrow template.
- The fake-sign SELF container does NOT obscure string data: identical
  marker counts in the bare `7f454c46` ELF and the signed `4f153d1d` PRX.

## Caveats the chief should know

1. **`[ST] lua-ffo` contiguous-literal caveat**: in the current composed
   source (typescan-r4 `LuaMod.cpp`), the Lua self-test line is
   runtime-composed from the UTF-16LE template `[ST] lua-{} {}{}\n` plus
   narrow probe strings (`"ffo PlayerController"`), so the contiguous bytes
   `[ST] lua-ffo` exist in the binary only if the next build emits them as a
   literal (the host test `actual_ev_string_seam.cpp:321` prints it
   literally; the source comment names "UTF-8 chunk bytes + UTF-16LE marker
   literals" as the composition-grep strings). The gate implements the exact
   directive (grep the given string). If a future FAIL shows `[ST] lua-ffo`
   missing while `[LUA-FN]` is present, check whether the line is
   template-composed before rebuilding:
   `python3 grep_markers.py <prx> --marker '[ST] lua-'` (ad-hoc, non-gating).
   The Lua chunk itself is a solid narrow UTF-8 witness:
   `--marker "pcall(FindFirstOf,'PlayerController')"`.
2. **`vmt_engine_tick_drain`** is a C-linkage symbol; its name survives only
   in dynsym/section strings, which the packager strips (absent even in the
   bare ELF, though `runtime.map` shows `.text.vmt_engine_tick_drain`).
   Informational only; if the pump must be provable, emit a log literal.
3. **`[Seeds][L27`** and **`[DRY]`** currently exist only in comments/host
   tests — expected MISSING until a build carries them; non-gating.
4. Receipt auto-discovery prefers a matching sha across
   `FINAL-RECEIPT*.json` candidates; with no match it compares against the
   newest and FAILs (exit 4) — a pre-upload artifact without a matching
   receipt must not upload.
5. macOS assumptions (darwin): `stat -f %z`, `shasum -a 256`, `xxd`; bash
   3.2-compatible (no mapfile/associative arrays).

## Verdict

Pipeline built, tested against three real package PRXes plus synthetic
pass/fail/tamper paths; it correctly FAILS every current artifact on the
critical markers (the live composition bug), passes identity checks where
they hold, and rejects wrong magic / wrong receipts. Ready for the next
build: run it on the new PRX before upload; do not bypass a FAIL.
