# STAGE-PLAN-CODE-ANCHOR — modbridge_ingame_facts.prx (igf5-4186048a6dd6)

Worker: worker-code-anchor-reads, 2026-10-04. Console: ps5-host-not-configured.invalid (pldmgr
HTTP 8084, FTP 2121). Console steps are user-coordinated in procedure but
may be executed by the controller via the standing console authorization;
the game LAUNCH itself is always a user action.

## 0. GATE — controller sign-off BEFORE staging (do not skip)

The probe calls BSD `mprotect` (syscall 74, from the pinned SDK's own
`sys/syscall.h`) through an inline shim because sceKernelMprotect has NO
import row anywhere in the verbatim provenance family (verified NEGATIVE;
full audit in RECEIPT.md / REPORT.md). The refusal path is safe by
construction, but the syscall NUMBER itself is not verifiable on-host.
The controller must explicitly accept this residual risk (or supply an
evidence-backed mprotect import row) before this PRX is staged. The 7
verbatim import identities are byte-identical to the loaded igf4 lineage —
loader acceptance risk is unchanged from igf4.

## 1. What gets replaced (SAME-FILE replacement, no ini change)

- Staged file: `/data/plugins/modbridge_ingame_facts.prx`
- Replaces: igf4 `igf4-4891869337b4` (sha256
  `4aa624cc4cc045c6c137b07b2283f458ad2e41a576273444d0fc75e1e9762756`)
  — igf4 safely REFUSED the exec-only anchor reads; igf5 performs them
  mprotect-first. Same path, same ini line, same 7-import identity set,
  same module name, same log path.
- Payload to upload:
  `outputs/2026-10-04-zcode-team/worker-code-anchor-reads/modbridge_ingame_facts.prx`
  — sha256 `486845b7e121d5993febdf812ac55815cc974fc1e2a062ebb4ac60fc5b8f8dfa`,
  25981 B, build id `igf5-4186048a6dd6`; determinism run1 == run2
  byte-identical.
- Log path unchanged: `/data/modbridge-ingame-facts.jsonl` (igf5 REPLACES
  the igf4 log on the same path; stale mtimes count as absent evidence).
- No .ini / config edit is required or permitted.

## 2. Pre-stage checks (host side)

1. `shasum -a 256 modbridge_ingame_facts.prx` must equal
   `486845b7e121d5993febdf812ac55815cc974fc1e2a062ebb4ac60fc5b8f8dfa`;
   refuse to stage on any mismatch.
2. `determinism/run1` and `run2` PRX bytes equal.
3. Back up the staged igf4 file first as
   `/data/plugins/modbridge_ingame_facts.igf4.bak` (reversible-op rule).

## 3. Upload + ftpsrv decryption-oracle (expected values)

Upload via FTP (2121) to `/data/plugins/modbridge_ingame_facts.prx`
(binary, overwrite in place). ftpsrv auto-DECRYPTS FSELF content on RETR:

- first 4 bytes of the RETR stream: `7f 45 4c 46` (`\x7fELF`, ELF64).
  A returned `4f 15 3d 1d` proves NO decryption (oracle FAIL).
- byte size: exactly `85472`.
- sha256: exactly `427fbf2f90aec4982f7e73456d289a5ed5c150d1ae753cfd62377d510dc6f646`
  (the bare-sdk inner ELF; full-hash equality with the WRAPPED host file
  `486845b7…` is impossible by construction and must NOT be expected).
- Sanity probe after staging (no game running): `GET /` on the pldmgr
  surface; do NOT load the payload outside the launch ritual.

## 4. Launch ritual (empirically nailed; do not improvise)

1. Game fully CLOSED (no Townfall process, no suspended app).
2. ploader armed: user clicks `ploader.elf` in Payload Manager, or
   `GET /loadpayload:/data/pldmgr/payloads/ploader/ploader.elf`.
3. User launches Townfall (never automated).
4. Records appear ~1 MINUTE AFTER LAUNCH at injection completion; ploader
   shows "injecting 2 plugins", then OK/FAIL ~1 min in.
5. Freshness oracle: mtime of `/data/modbridge-ingame-facts.jsonl`.
6. One launch per staged build. If FAIL: collect ploader surface text and
   the jsonl tail via FTP; do NOT relaunch repeatedly without a changed
   implementation or a specific open question (standing rule).

## 5. First-look analysis (controller, after the run)

Entry-marker triage (unchanged lineage contract):

- file ABSENT -> load or pre-open startup failure (imports are
  byte-identical to loaded igf4, so a pure load rejection would be a NEW
  lineage fact — capture ploader surface text before any retry);
- only seq=1 `entry` -> load + channel OK, later failure localized;
- entry + startup + seq 3..7 -> full run.

Per-record expectations (all under one build_id `igf5-…`):

1. `mapscan` (seq 3): `rc=0`, `trusted=1`, `mod_ok=1`, `ksz_144`
   dominant, `stop_reason=0`, `retained` ~10k, `truncated=0`. Note field
   documents the new role: internal coverage + restore-prot source; the
   map does NOT gate the anchors anymore (kernel mprotect success does).
2. `tick_anchor` (seq 4) at `rt_addr=0x33ac8c0`, page `0x33ac000`:
   expected `mprotect_called=1, mprotect_rc=0, read_performed=1,
   byte_match=1, diff_mask=0, restore_called=1, restore_rc=0,
   mprotect_restored=1, orig_prot=4 (current_vmmap_cover)`.
   - `byte_match=0` is a REAL finding — inspect `diff_mask` +
     `bytes_read_hex` (relocation/thunking hypothesis).
   - `mprotect_rc!=0` → refusal `mprotect_failed_no_read_attempted`;
     `mprotect_errno_hex=0x4e` (78, ENOSYS) is the syscall-renumbering
     signature; other errnos are genuine kernel refusals (e.g. 0x1
     EPERM). Either way NO read occurred — safe outcome, check stays open.
   - `mprotect_restored=0` → `anomaly=1` (original protection could not
     be re-verified) — record and treat as a real anomaly.
3. `sco_anchor` (seq 5) at `rt_addr=0x890aa0`, page `0x890000`: same
   expectations, independent of seq 4 (separate page, separate
   mprotect/restore cycle).
4. `gua_pred_replay` (seq 6): `status=performed`,
   `corrected_gc_order_present=1` (the gc-fix predicate IS wired).
   Expected healthy state: exactly one of `ph_la_verdict`/`ph_lb_verdict`
   and `pl_la_verdict`/`pl_lb_verdict` = `Validated` under the corrected
   GC-order invariant (live capture facts: ObjFirstGCIndex=35477,
   ObjLastNonGCIndex=35476 = first-1 — the normal settled state the OLD
   predicate auto-refuted). `Refuted` rcodes are hard contradictions;
   `Inconclusive GOA_E_CHUNK_EXTENT_UNVERIFIABLE` = chunk bytes in
   unreadable holes (re-arbitrate).
5. `summary` (seq 7): `anchors_confirmed` (both reads matched),
   `promotion_gate_indicator_anchors_only` (the anchor sub-gate),
   `promotion_gate_indicator` = anchors AND predicate Validated — an
   INDICATOR ONLY. Promotion remains the controller's call per
   SEGMENT-CORRESPONDENCE + worker-span-arbitration rules; every
   non-truncated line must `json.loads` (lineage writer, host-verified).

## 6. Rollback

Restore `/data/plugins/modbridge_ingame_facts.igf4.bak` over the staged
file (same filename), or simply leave igf5 in place: it quiesces inert
after logging (no threads, no unload, module stays loaded) — v2/v3/igf4
lineage behavior. Any mprotect change is bounded to the two 16 KiB anchor
pages and reversed before the record is flushed.
