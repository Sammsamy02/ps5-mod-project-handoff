# STAGE-PLAN-ANCHOR — modbridge_ingame_facts.prx (igf4-4891869337b4)

Worker: worker-anchor-validation, 2026-10-04. Console: ps5-host-not-configured.invalid (pldmgr
HTTP 8084, FTP 2121). All console steps are user-coordinated in procedure but
may be executed by the controller via the standing console authorization; the
game LAUNCH itself is always a user action.

## 0. What gets replaced (SAME-FILE replacement, no ini change)

- Staged file: `/data/plugins/modbridge_ingame_facts.prx`
- Replaces: v3 fself `igf3-de0c0cb644d3` (loaded and logged through its
  VMMAP records; cause of the later stall unknown). Same path, same ini
  line — the loader acceptance evidence is the v2/v3 lineage (identical
  7-import identity set, same module name, same log path).
- Module name unchanged: `modbridge_ingame_facts`. Log path unchanged:
  `/data/modbridge-ingame-facts.jsonl` (this file REPLACES the v3 log on
  the same path — the runner treats stale mtimes as absent evidence).
- Payload to upload: `outputs/2026-10-04-zcode-team/worker-anchor-validation/
  modbridge_ingame_facts.prx` — sha256
  `4aa624cc4cc045c6c137b07b2283f458ad2e41a576273444d0fc75e1e9762756`,
  26077 B, build id `igf4-4891869337b4` — FROZEN delivery;
  determinism run1 == run2 byte-identical.
- No .ini / config edit is required or permitted.

## 1. Pre-stage checks (host side, before upload)

1. `shasum -a 256 modbridge_ingame_facts.prx` must equal the hash above;
   refuse to stage on any mismatch.
2. Cross-check determinism artifacts exist (`determinism/run1`, `run2`,
   byte-identical).

## 2. Upload + ftpsrv decryption-oracle (expected values)

Upload via FTP (2121) to `/data/plugins/modbridge_ingame_facts.prx` (binary,
overwrite in place; back up the v3 file first as
`/data/plugins/modbridge_ingame_facts.igf3.bak` — reversible-op rule).

ftpsrv auto-DECRYPTS FSELF content on RETR. Verification therefore is:

- `RETR /data/plugins/modbridge_ingame_facts.prx` -> the returned stream is
  the DECRYPTED inner ELF, NOT the wrapped fself. Expected values:
  - first 4 bytes: `7f 45 4c 46` (`\x7fELF`), EI_CLASS=2 (ELF64) — the
    wrapped file on the host starts `4f 15 3d 1d`, so a returned
    `4f 15 3d 1d` proves NO decryption (oracle FAIL).
  - byte size: exactly `85536`.
  - sha256: exactly `bd591ed3159ff1c571b85ebba0118e5f65912b5780297ccafd41ebe640b51914`
    (the bare-sdk inner ELF; full-hash equality with the WRAPPED host file
    `4aa624cc…` is impossible by construction and must NOT be expected).
- Sanity probe after staging (no game running): `GET /` on the pldmgr surface
  to confirm the console is up; do NOT load the payload outside the launch
  ritual.

## 3. Launch ritual (empirically nailed; do not improvise)

1. Game fully CLOSED (no Townfall process, no suspended app).
2. ploader armed: user clicks `ploader.elf` in Payload Manager, or
   `GET /loadpayload:/data/pldmgr/payloads/ploader/ploader.elf`.
3. User launches Townfall (never automated).
4. Records appear ~1 MINUTE AFTER LAUNCH at injection completion; ploader
   shows "injecting 2 plugins", then OK/FAIL ~1 min in.
5. Freshness oracle: mtime of `/data/modbridge-ingame-facts.jsonl` (and the
   sibling runtime-aligned-entry log, if that payload is still armed).
6. One launch per staged build. If FAIL: collect ploader surface text and the
   jsonl tail via FTP; do NOT relaunch repeatedly without a changed
   implementation or a specific open question (standing rule).

## 4. First-look analysis (controller, after the run)

Entry-marker triage (unchanged lineage contract):

- file ABSENT -> load or pre-open startup failure;
- only seq=1 `entry` -> load + channel OK, later failure localized;
- entry + startup + seq 3..10 -> full run.

Per-record expectations (`fields`, all under one build_id `igf4-…`):

1. `mapscan` (seq 3): `rc=0`, `trusted=1`, `mod_ok=1` (buffer divides by the
   144-byte record exactly), `ksz_144` dominant, `stop_reason=0`,
   `retained` ~10k with `truncated=0` (v2/v3 scale). `trusted=0` or `rc!=0`
   means the safety gate had to refuse every dereference — that outcome is
   ITSELF the finding (current-invocation map unavailable); no crash occurs.
2. `tick_anchor` (seq 4) / `sco_anchor` (seq 5): `match=1` expected
   (live bytes == retained-image bytes, diff_mask=0, shape=
   CODE_I_PROLOGUE_LIKE). A mismatch (`match=0`) is a REAL finding — inspect
   `diff_mask` and `live_hex` (relocation/thunking hypothesis); a `refusal`
   field means the safety gate refused (map untrusted / not covered /
   not readable) — never a crash.
3. `gua_read` (seq 6): 96 live bytes at 0x97ba110 as `hex` + words
   w0..w3 (+0x00/+0x08/+0x10/+0x18). Layout-A reading: w0/w1 are the GC
   indices, w2=Objects, w3=PreAllocated. Layout-B reading: w0=Objects.
   These words alone discriminate the layout by eye.
4. `gua_writeback` (seq 7): `attempted=1, restored=1` proves the span is
   truly writable (same-value 96-byte writeback; user-authorized write
   test). `refusal:"covered_not_writable"` is a real finding; anything else
   refused is a gate outcome, not a crash.
5. `gua_pred_header` (seq 8, cfgH = 96-byte-span-only feed): for a
   heap-backed GUA the honest expectation is Refuted
   GOA_R_PTR_OUTSIDE_READABLE under both layouts (pointer targets are
   outside the single span by construction). This row is the task-literal
   feed; it is recorded but NEVER promoted on its own.
6. `gua_pred_live` (seq 9, cfgL = current readable spans): the structural
   verdict. Expected healthy state: exactly one of `p_la_verdict=Validated`
   (FUObjectArray outer) / `p_lb_verdict=Validated` (TUObjectArray direct)
   and `p_verdict=Validated`, with observed scalars (num_elements ≈ tens of
   thousands after real gameplay, max_chunks in [1,512], chunk pointers).
   `Inconclusive GOA_E_CHUNK_EXTENT_UNVERIFIABLE` = needed chunk bytes fall
   in unreadable holes — evidence unavailable, re-arbitrate. Any `Refuted`
   rcode is a hard contradiction to be read together with the w0..w3 words.
7. `summary` (seq 10): `anchor_confirmed` (both code anchors matched),
   `gua_validated_any_cfg`, `promotion_gate_indicator` = conjunction — an
   INDICATOR ONLY. Promotion of the layout facts remains the controller's
   call per SEGMENT-CORRESPONDENCE + worker-span-arbitration rules; every
   non-truncated line must `json.loads` (v3 writer, field shapes
   host-verified).

## 5. Rollback

Restore `/data/plugins/modbridge_ingame_facts.igf3.bak` over the staged file
(same filename), or simply leave igf4 in place: it quiesces inert after
logging (no threads, no unload, module stays loaded) — v2/v3 lineage
behavior.
