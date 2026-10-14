# STAGE-PLAN-V3 — modbridge_ingame_facts.prx (igf3-5e5b6fcf83cd)

Worker: worker-ingame-probe-v3-resume, 2026-10-04. Console: ps5-host-not-configured.invalid
(pldmgr HTTP 8084, FTP 2121). All console steps are user-coordinated in
procedure but may be executed by the controller via the standing console
authorization; the game LAUNCH itself is always a user action.

## 0. What gets replaced (SAME-FILE replacement, no ini change)

- Staged file: `/data/plugins/modbridge_ingame_facts.prx`
- Replaces: v2 fself `igf2-764f957dc0a6` (loaded and logged 56 records on
  console with the same path and the same ini line).
- Module name unchanged: `modbridge_ingame_facts`. Log path unchanged:
  `/data/modbridge-ingame-facts.jsonl` (this file REPLACES the v2 console
  log on the same path — the runner treats stale mtimes as absent evidence).
- Payload to upload: `outputs/2026-10-04-zcode-team/worker-ingame-probe-v3/
  modbridge_ingame_facts.prx` (sha256
  `2555b946e73b38f1744a036770103916bffb3b7397e0f7e9ef9dd101e448cd75`, 53165 B,
  build id `igf3-de0c0cb644d3` — FROZEN delivery; determinism run1==run2).
- No .ini / config edit is required or permitted (7-import identity set is the
  console-proven v2 set; loader acceptance evidence = v2 lineage).

## 1. Pre-stage checks (host side, before upload)

1. `shasum -a 256 modbridge_ingame_facts.prx` must equal the hash above;
   refuse to stage on any mismatch.
2. Cross-check determinism artifacts exist (`determinism/run1`, `run2`).

## 2. Upload + ftpsrv decryption-oracle (expected values)

Upload via FTP (2121) to `/data/plugins/modbridge_ingame_facts.prx` (binary,
overwrite in place; back up the v2 file first as
`/data/plugins/modbridge_ingame_facts.igf2.bak` — reversible-op rule).

ftpsrv auto-DECRYPTS FSELF content on RETR. Verification therefore is:

- `RETR /data/plugins/modbridge_ingame_facts.prx` -> the returned stream is
  the DECRYPTED inner ELF, NOT the wrapped fself. Expected values:
  - first 4 bytes: `7f 45 4c 46` (`\x7fELF`), EI_CLASS=2 (ELF64) — the
    wrapped file on the host starts `4f 15 3d 1d`, so a returned
    `4f 15 3d 1d` proves NO decryption (oracle FAIL).
  - byte size: exactly `109776`.
  - sha256: exactly `c52e582d95dff8665911355636049f848177953101009a7da6b48923597e5637`
    (the bare-sdk inner ELF; full-hash equality with the WRAPPED host file
    `2555b946…` is impossible by construction and must NOT be expected).
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
5. Freshness oracle: mtime of `/data/townfall_runtime_aligned_entry.log`
   (sibling payload's marker) and of `/data/modbridge-ingame-facts.jsonl`.
6. One launch per staged build. If FAIL: collect ploader surface text and the
   jsonl tail via FTP; do NOT relaunch repeatedly without a changed
   implementation or a specific open question (standing rule).

## 4. First-look analysis (controller, after the run)

1. Entry-marker triage (module_entry.c contract):
   file ABSENT -> load or pre-open startup failure; only seq=1 `entry` ->
   load + channel OK, later failure localized; entry+startup+V-records ->
   full run. Every non-truncated line must `json.loads` (v3 writer).
2. `v1_vmmap_full`: `first_record_structsize` (expect 144), `buffer_mod_exact`
   (expect 1; 0 refuses trust -> needs-map run), `ksz_144_records` vs
   `total_records` (P1), `total/retained/truncated` explicit, r/w/x counts vs
   the void 57/52/5 (P5), high family up to 0x306ae60000 present verbatim
   (P2; no 36-bit filter exists in v3).
3. `v2_base_hunt`: `bias_l7_start_pred` 0x953c000, `bias_gua_probe_pred`
   0x97ba110. A W span covering ~0x97ba110 is CANDIDATE EVIDENCE ONLY
   (consistent with BOTH open hypotheses — controller correction #2;
   worker-span-arbitration P6).
4. Per-match `segc_free_pass` / `segc_bias_pass` (never collapsed): arbitration
   between single-load-bias (L7 at 0x400000+0x913c000) and free-mapping
   requires L7's own correspondence evidence (extent/prot/anchor at the
   expected base, shared-page 0x9633000 behavior), per SEGMENT-CORRESPONDENCE.md.
   Promotion additionally requires Tick/SCO byte anchors at 0x33ac8c0/0x890aa0.
   `bias_anchor_checks` records the bias-predicted base anchor result for ALL
   five LOAD specs (ph 0/1/3/7/8; `strong` 0 = padding anchor, mismatch is
   not disproof); Tick/SCO anchor + code-shape fields are emitted in
   v2_base_hunt even when ZERO GUA candidates exist.
5. Cross-validate the emitted span list against
   `../worker-mapping-recovery/vmmap-v3-records.jsonl` (55 recovered spans,
   2 trusted) and apply the VERDICT.md decision rule: P1+P2 -> family REAL,
   VA >36-bit established; D1 -> displacement; neither -> re-arbitrate with
   the raw v3 buffer snapshots (`v1_vmmap_raw`, 16x136-byte headers).
6. Only a promoted candidate (multi-anchor + L7 correspondence) feeds the
   staged-pair assembly (worker-layout-minimum) in the controller queue.

## 5. Rollback

Restore `/data/plugins/modbridge_ingame_facts.igf2.bak` over the staged file
(same filename), or simply leave v3 in place: it quiesces inert after logging
(no threads, no unload, module stays loaded) — v2 lineage behavior.
