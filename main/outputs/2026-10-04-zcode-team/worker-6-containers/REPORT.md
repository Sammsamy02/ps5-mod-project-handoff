# REPORT — worker-6-containers (2026-10-04)

Scope delivered: (1) evidence-grounded capability matrix, (2) pack/extract roundtrip tests against
saved originals, (3) detection/handling notes incl. generation limits, (4) profile-driven per-build
container rules. All work read-only outside this output directory. No network, no PS5 contact,
no subagents. ~40 tool calls total.

## Files owned (all under outputs/2026-10-04-zcode-team/worker-6-containers/)

- CLAIM.json — ownership, inputs, pinned-stack acknowledgment
- CAPABILITY-MATRIX.md — format x operation x status (proved-on-target / host-proved / declared-only / gap)
- DETECTION-NOTES.md — compression variants, encryption/signing detection, generation limits
- PROFILE-RULES.md — per-build container-fact profile schema + Townfall baseline + generic rules
- tests/zip_roundtrip.py, tests/pak_probe.py, tests/utoc_probe.py
- evidence/zip-roundtrip.json, evidence/pak-probe.json, evidence/utoc-retoc-verify.json

## Exact commands and results (host, originals read-only)

1. `python3 tests/zip_roundtrip.py`
   - 8/8 saved original Nexus ZIPs PASS: CRC verify, extract, per-member repack with original
     methods, sha256+size member compare. Methods observed {store, deflate} only; no encryption;
     no unsafe path names; data-descriptor flag on ue4ss-v1.zip and leon-2k-v1.1.zip only.
   - Targets: infinite-stamina-v1, mod-menu-v6, minimap-v3.1, ue4ss-v1, leon-2k-v1.1,
     item-spawner original.zip, movement-speed-2x, performance-pak original.zip
     (paths + hashes inside evidence/zip-roundtrip.json).
   - Honest scope: host integrity evidence; does NOT re-run the native C zip intake (that already
     has its own console receipts) and does NOT prove mod execution.
2. `python3 tests/pak_probe.py`
   - 6/6 saved paks parse at footer+primary-index header. All v11 (0x5A6F12E1), frozen=0, footer
     compression-method table EMPTY (uncompressed loose files), mount `../../../`.
   - townfall pakchunk0-ps5.pak: 297,572,462 B, sha256 ee145cbc…, declared_entry_count 4699,
     index_offset 297250226, index_size 56502.
   - ModBridge extra-pak candidates: 3 and 1 entries — reader-compatible with the originals' shape.
   - Leon wrapper pak zzz_Leon_Townfall_2K_P.pak is 347 B with the real payload in utoc/ucas.
   - derived.pak (config composition trial) same entry count 4699, size +~30 KB.
3. `python3 tests/utoc_probe.py`
   - retoc v0.1.5 (pinned official Darwin release, in-workspace binary) `verify`:
     Leon trio utoc exit 0 "verified"; 1_999_P.utoc exit 0 "verified".
   - The five main-game utoc captures + global.utoc FAIL verify only because their .ucas siblings
     were never saved (retoc panics file-not-found) — recorded, not worked around.
4. `shasum -a 256` on townfall-62-rar-intake-1/bundle-input/1_999_P.{utoc,ucas}
   - 66a7b203… / 78dc8828… — identical to container_repacking roundtrip receipt source_before AND
     source_after ⇒ the byte-roundtrip corpus is unchanged since that receipt; no rerun needed.

## Key findings

1. **PS5 saves use Zen-style TOCs, not desktop IoStore magic.** Every saved console .utoc begins
   with ASCII `-==-` (0x2d3d3d2d) x4, never 0xCAECA5A9. Matches checkpoint receipt
   `townfall-original-zen-headers-1`. Intake must probe both magics; this is exactly the kind of
   per-platform fact that belongs in profiles.
2. **Capability reality vs comments:** console-executed container work is reader-side only
   (scan/extract/verify/prepared-candidate readback; game_files_modified false everywhere).
   native_pak_writer: ps5_executed false, game_acceptance_proved false. native_iostore_reader:
   ps5 link passed, executed/payload_verified/runtime/installation all false. "A console reader
   can consume the bundle" is proved for ZIP-on-PS5 and prepared-Leon readback; "contained mod
   executes" is proved nowhere in the container lineage.
3. **Only two complete saved utoc+ucas pairs exist** (Leon trio, 1_999_P retoc corpus). Main-game
   IoStore chunk-level work is not reproducible from saved bytes until the ucas halves are
   captured (controller-owned console action, not mine).
4. **Entry-count delta:** primary index declares 4699 entries for main pakchunk0; checkpoint
   survey counted 4544 classic names. Recorded as open question (likely duplicates/deleted
   entries); no number was "corrected".
5. **Pinned stack respected:** libarchive 3.8.7 (sources + host build), xz 5.8.4, zlib 1.3.2
   (closure-3 descriptor verified), MbedTLS 3.6.7 lineage commit 068ff080…. Separately noted:
   outputs/ps5-mod-bridge/third_party/manifest.json pins libarchive v3.7.4 HEADERS with system
   -larchive for the host app — a version-skew fact for the controller to reconcile, not changed.
6. Compression today: ZIP store/deflate and PAK uncompressed proved; Oodle absent and a hard gap;
   lzma host controls exist but decoder heap not bounded and no production intake proof.
7. Windows-cooked assets: `platform_compatible: false` in three separate receipts; cooked
   packages (195k .uasset refs observed) cannot be treated as installable content — classification
   must route them to unsupported-needs-conversion.

## Open questions (explicit gaps; no guessed byte rewrites)

- Zen TOC field semantics beyond magic; 4699-vs-4544 delta; in-game mount acceptance of prepared
  wrappers; whether Oodle appears in the two complete pairs (bounded TOC method census suggested);
  encrypted ZIP/PAK fixtures absent; RAR is a one-off corpus, not a capability; bsa/ba2 and 7z are
  pure gaps; Zen TOC signing semantics unknown.

## Integration contract (for the controller)

- Consume CAPABILITY-MATRIX.md statuses as the intake planner's admission set:
  ZIP(store/deflate) read/extract = admitted (console receipts exist); PAK v11 read = admitted;
  PAK write = host-plan only (no game-acceptance claim); IoStore Zen verify = admitted via pinned
  retoc on host; everything else = explicit unsupported reason per DETECTION-NOTES.md taxonomy.
- Profiles: PROFILE-RULES.md schema is the per-build container-fact contract; Townfall baseline
  filled; other builds start empty and are evidence-filled.
- The evidence/ JSONs are receipts; tests/ scripts are re-runnable read-only probes
  (`python3 tests/<name>.py` from this directory; scratch confined to tests/scratch-zip).
- Console actions NOT requested by me: capturing missing .ucas siblings, any in-game mount test.
  Those are controller-owned per the handoff.

## Deviations / notes

- utoc_probe.py initially expected 0xCAECA5A9; the observed Zen magic made those parsed fields
  unreliable, so the evidence addendum marks them non-authoritative and keeps only magic-bytes
  observation + retoc results. The script is left as-is (fixture of the wrong assumption) with the
  addendum correcting the record.
- All referenced handoff paths were present; nothing materially missing. The nine checkpoint keys
  were found as top-level JSON keys (not experiment-list entries).
