# REPORT.md — worker-mfa-activation-audit

2026-10-04 · static audit · no console contact, no nested subagents, no target execution · writes confined to this dir.

## What was done

Restart context honored: the prior attempt left only CLAIM.json; that file was superseded (rewritten first with confirmed evidence), then the audit ran, then this report last.

1. **Implementation located and proven unique.** `mfa_activate` lives in `.superpowers/native-dependency-spike/claude-takeover-43a96969/source/runtime/modforge_activation.c:311–495` (sha256 `f97d1d2094b090be…fbf81a`), with the parser inline in `.../runtime/modforge_loadlist.h` (single header, no .c). All four vendored copies across `worker-{d1-chain-relink, f-r2-fix, installer-composition, zip-to-mfb}/composition/vendored/` are byte-identical — one validation set exists anywhere. The composition-5 `lineage-overlay-1` staging copies of the entry, bridge, and mfa header diffed byte-identical to `worker-module-start-wiring/` (captured before that staging dir was removed by concurrent composition-5-rebuild work at 18:44 — flagging the live mutation for the chief; my conclusions rest on the stable wiring outputs and authoritative source).
2. **All 23 refusal branches enumerated** with exact codes and messages, plus the 6 parser-internal malformed cases — full table with file:line in ACTIVATION-PREDICTION.md §1.
3. **Staged package cross-checked byte-by-byte.** `loadlist.txt` (446 B, sha256 `49ab7a3a40ee15d3…981e3bf`, matches staged) decodes as header + title/engine/framework/mod + 7 file rows. Every check PASSES; full sha256 of all 8 package files recorded in ACTIVATION-PREDICTION.md §2.
4. **Role coverage verified** (§3), **runtime-status.txt one-sink audit and dir-existence analysis** (§4), and the **ordered GO verdict with exact failure signatures** (§5) delivered.

## Key findings

- **VERDICT: GO for the activation stage.** No check predicts failure; predicted success path is `MFA_OK` with 35,897 preloaded bytes (entry will emit `mfa_activated_low=35897`).
- **The console loadlist grammar has NO sha column** — `file` rows are exactly `file⇥role⇥path⇥size` (parser demands `np == 4`, modforge_loadlist.h:196–199; FORMATS.md "Load list"). The staged rows conform. `mfa_activate` verifies sizes only (read_exact, modforge_activation.c:163–208); file integrity was enforced upstream by the streaming-hash installer. The two independently confirmed staged shas (loadlist `49ab7a3a`, settings `9f3fa127`) match local bytes exactly.
- **All 7 listed sizes equal actual local sizes exactly** (779/588/186/2960/21415/0/9962), including the 0-byte `enabled.txt`, whose size-0 read_exact path (loop skipped + single EOF probe) the empty file satisfies.
- **Roles:** default preload set `{lua_mod, config, script}` (modforge_activation.c:281) covers every row (`config`×4, `lua_mod`×3) — nothing skipped. There is no `MFA_E_ROLE`: an out-of-set role would be listed but not read, never refused.
- **One-sink rule HOLDS for `<root>/runtime-status.txt`:** exactly two in-process writers, lifetime-exclusive — the `RuntimeAckSink` (opened only after activation succeeds, ue4ss_backend.cpp:101–102) and the entry's `write_status_failed` (entry:194–219, only where no sink is active; its `O_TRUNC` open starts a fresh session, matching the header contract). Only overlap edge (prepare throws after sink open) is strictly sequential. Minor observation: `RuntimeAckSink` has no destructor, so an abandoned sink leaks one fd until process end — harmless, nothing writes through it.
- **Dir existence:** `mfa_status_open` creates the file, never parents (modforge_activation.c:609). The staged root exists; if it did not, `mfa_activate` would already have failed `MFA_E_NO_LOADLIST` and `write_status_failed` deliberately no-ops (entry:205–206) with diagnostic + UE4SS.log still carrying the failure.
- **Validation order matters for triage:** pass-1 row checks run before title match, duplicates, and reads — so the first failure, if any, is most plausibly `MFA_E_NO_LOADLIST (-2)` (the `/data` module_start window, the one non-statically-checkable condition) or `MFA_E_OPEN (-6)`/`MFA_E_SIZE (-8)` on staged divergence — never grammar, role, limit, title, or duplicate.
- **Failure surface contract:** any activation failure yields `module_start` returning 0 (game runs unmodded) plus one UE4SS.log line `modbridge module_start failed: mfa_activate_failed code=<n> detail=<name> <message> <path>` (entry:166–192, :250–268) and a best-effort `failed` state line in runtime-status.txt.

## Residual UNKNOWNs (honest limits)

- Byte-exact equality of the five staged files whose shas were not independently re-reported (console contact forbidden). Sizes match and sizes are all the runtime checks, so this does not change the verdict.
- The `/data` readability window at module_start is a runtime-timing contract (modforge_activation.h:5–8), not statically verifiable here.

## Deliverables

- `CLAIM.json` (first, superseding the dead attempt's file)
- `ACTIVATION-PREDICTION.md` (verdict, branch table, byte cross-check, roles, status-file audit, failure signatures)
- `REPORT.md` (this file, last)
