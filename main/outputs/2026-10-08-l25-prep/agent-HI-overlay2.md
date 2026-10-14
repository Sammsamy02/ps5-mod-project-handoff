# agent-HI — classic-PAK intake + L1/L2 substitution consumption for the overlay builder

2026-10-10, retry after a provider outage (no prior progress existed).
Mac-only, battery-safe: python3.9 + the pinned local repak/retoc binaries
(read-only invocations, no builds, no console, no network). Edits confined
to `pak-overlay/*` (+ this report); `.superpowers` trees read-only. Rule-10
re-verified: zero game names in executable logic (names only in the
provenance docstring, GK precedent). Originals-untouched law held: mod
archives and source paks are read-only inputs, sha-pinned; every derived
byte lands in the overlay output or the state-dir staging.

## Deliverables (all in outputs/2026-10-08-l25-prep/pak-overlay/)

| File | What changed |
|---|---|
| `overlay_builder.py` | +pure-python classic PAK v11 reader (`parse_pak`/`inspect_pak`/`classify_pak_entries`, pins P9-P11 in the docstring), +`convert_classic_pak` orchestration of the pinned retoc binary, +`parse_substitutions` + hide/closest-match threading through `build_overlay`/`plan_conflicts`, +journal/CLI wiring (`--classic-pak`, `--retoc-tool`, `--retoc-version`, `--substitutions`, `inspect-pak` command); 2,886 lines total |
| `test_overlay_builder.py` | +16 tests (32 total): synthetic v11 pak fixture per the pinned writer layout, real-corpus verdict matrix, byte-oracle vs the pinned repak, conversion orchestration/determinism/admission gates, substitution semantics/refusals/lifecycle, and the GM real-classification fixture; 1,505 lines |
| `UNSUPPORTED.md` | section 7 rewritten (classic-PAK verdict matrix + honest unproven-content limit), new section 7a (substitutions) |
| `mod-manager/INTEGRATION.md` | CLI contract updated: `--classic-pak`/`--retoc-tool`/`--substitutions`, `inspect-pak`, classic-provenance journal fields, suggested call points 1a/1b |
| `leon-substitutions-fixture.json` | receipt of the real GM classification fixture run (ledger shape + derived sha) |

## A. Classic-pak verdict matrix (compact; full version in UNSUPPORTED.md §7)

| Input profile | Verdict |
|---|---|
| v11, unencrypted, all-stored, only cooked split packages (+shader libs/script objects) | **CONVERTIBLE_VIA_PINNED_RETOC** — pinned retoc v0.1.5 binary does pak->zen offline (verified deterministic: identical invocations → identical shas, proven on the real retained archive), output re-admitted through the ordinary ModSpec admission (TOC v8 via the fixed `--override-toc-version ReplaceIoChunkHashWithIoHash --override-container-header-version SoftPackageReferencesOffset` pair), merged as a normal IoStore mod |
| v1..v10 paks (real v5/v7/v8a/v8b/v9 fixtures retained) | UNSUPPORTED `classic_pak_version_unsupported` |
| Encrypted index / key GUID / encrypted entries (real fixtures) | REFUSED (Phase B keys) |
| Compressed entries (real Zlib fixtures; footer codec name reported) | REFUSED `classic_pak_compression_refused` |
| Loose non-asset files / orphan bulk / uncooked packages | REFUSED `classic_pak_content_refused` (pointer to the mod-manager classic-pak staging path — never a silent drop) |
| No full-directory index / non-encoded records / empty-or-tombstones | REFUSED (`classic_pak_full_directory_required` / `..._nonencoded_entries_unsupported` / `classic_pak_empty`) |
| Per-entry payload corruption (real corruption fixture) | REFUSED `classic_pak_entry_sha1_mismatch` |
| `--classic-pak` without `--retoc-tool` and no reusable staged trio | REFUSED `classic_pak_conversion_tool_unavailable` + documented pointer (pure-python re-serialization would fabricate export bundles — never-fabricate law) |

retoc's pak->zen WAS judged usable offline: a built pinned binary exists at
`.superpowers/native-dependency-spike/retoc-v0.1.5-raw-roundtrip-1/tool/
retoc_cli-aarch64-apple-darwin/retoc` (GN's "unbuilt" note was stale); the
builder orchestrates it rather than reimplementing LinkerSave. HONEST
LIMIT: no cooked legacy BP archive exists in any local tree, so full
CONTENT conversion is unproven — proven locally: reader correctness on
real corpora (byte-oracle equality with the pinned repak binary on the
project writer's paks; UnrealPak fixture corpus; the real 347-byte v11
archive), the exact orchestrator invocation on that real archive
(deterministic, emits a v8 TOC), failure propagation, staging-reuse laws,
source-drift refusal, and the admission gates (empty archive →
`classic_pak_empty` before any tool run). GN's stage 1 still needs a real
BP pak. Engine acceptance of any derived container remains the standing
Phase 0 question (unchanged).

Reader layout pins (P9-P11, all cited in the module docstring): repak
footer/pak.rs/entry.rs schema (footer geometry incl. the v8a/v8b 189/221
split, updated-index region order, encoded-entry bitfields, 4-byte LE
secondary booleans), the boolean-width/directory-schema research notes
(fail-closed parsing, INT32_MIN tombstones), and this project's promoted
v11 writer (seeded FNV-1a path hash with the UTF-16 double-step — the v11
"Fnv64BugFix" — verified against every path-hash secondary in the corpus).
Fixed en route: physical-header SHA1 sits at +28 (found via the repak-get
byte oracle); footer candidates for v4-v6 are 45 B not 61.

## B. Substitution consumption (GM's classifier directives)

`--substitutions FILE` (schema 1, validated strictly):
`{"mod_id","package_id"(16-hex or 0x-form),"action":"hide"|"closest-match"
[,"from_package_id","to_package_id"]}`.

- `hide` = suppress the override so the base export shows: the mod's chunk
  rows, store entry and directory rows for that pid contribute nothing;
  base rows (and the base store entry with its PS5 shader-map hashes) win;
  suppressed contributions never count as conflicts.
- `closest-match` = repoint: the DERIVED merged store entry's import list
  is rewritten from→to (positional FPackageIds — the engine's import
  resolution surface; the mod's package bytes stay verbatim). `to` must
  exist in the base store; `from` must be among the entry's imports;
  closest-match on a later-wins-contested pid refuses.

Substitution ledger shape (receipt `substitution_ledger` + journal; per
directive row, order-preserving; real example from the Leon fixture):

```json
{"mod_id": "leon2k", "package_id": "0xfdf50b1f37af2d44",
 "action": "closest-match", "base_replaced": true,
 "store_entry": "derived-import-repoint",
 "suppressed_chunk_ids": [], "suppressed_virtual_paths": [],
 "from_package_id": "0xd7ead7ce2462b4f1",
 "to_package_id": "0xedcd9102d436c3ad",
 "import_indexes": [0],
 "store_entry_imports_before": ["0xd7ead7ce2462b4f1"],
 "store_entry_imports_after": ["0xedcd9102d436c3ad"]}
```

(hide rows instead carry `store_entry: "base-retained"` + the suppressed
chunk-id and virtual-path lists.) Journal persistence: directives are
stored under `journal.substitutions` (source sha + parsed rows), retained
across re-installs without the flag, filtered automatically when a
substituted mod is uninstalled; uninstall-to-pristine also removes
classic staging. Laws: derived-overlay entries only; originals untouched;
deterministic (input-order permutation → byte-identical derived shas,
tested).

## Tests — 32/32 PASS (~30 s)

New coverage on top of GK's 16 (all still green):
- Reader: synthetic v11 pak round-trip (footer digests, path-hash pairs,
  sha1s, payload slices); synthetic refusals (loose/uncooked/corrupt/
  magic); real repak fixture matrix (9 paks: versions, compress w/ Zlib
  name, encrypt, encryptindex); real project-writer paks with BYTE-ORACLE
  equality against the pinned repak binary (`repak get` per entry); the
  real empty v11 archive; `inspect-pak` CLI.
- Conversion: no-tool refusal with pointer; content refusal; failure
  propagation (tool stderr kept verbatim in the refusal detail, nothing
  published, no journal); real-archive orchestration determinism (two
  runs → identical shas; v8 TOC byte confirmed); staging reuse without a
  tool; source-drift refusal.
- Substitutions: hide restores the base export (chunk row maps to the
  BASE payload through the derived index; store count drops the mod
  entry); closest-match rewrites only the derived merged header (import
  verified by re-parsing the extracted merged ContainerHeader; mod chunk
  bytes still extract verbatim); the four refusal codes; hide de-conflicts
  two mods contesting one package; determinism under input permutation;
  CLI journal lifecycle (install→verify→list→reinstall-retain→uninstall-
  rederive-drop).
- GM real fixture: all 42 HIDE_VIA_BASE_INVISIBLE rows of
  `shader-fallback/leon-classification.json` become closest-match repoints
  to the base invisible material (pid `edcd9102d436c3ad` resolved from the
  captured base TOCs; 1 PS5 store hash confirmed); every row applied
  (`derived-import-repoint`, base_replaced), 42/42 imports verified absent
  before/present after in the merged header; overlay semantics elsewhere
  unchanged (59,573 chunks / 56,993 directory files, lookup validates,
  verbatim sidecar).

## Notable engineering facts / honest notes

- The pinned tool's `to-zen` defaults emit TOC v3 (UE4_27) / v5 (UE5.x);
  the bridge pins the override pair that aligns its output with this
  builder's v8-only admission — verified byte-level on the real archive.
- Conversion staging lives under `<state-dir>/classic-staging/<mod_id>/`
  with the mod id as basename (the converted container id is
  basename-derived, so determinism requires a deterministic name).
- `_classic_mod_spec` reuses a staged trio when its recorded shas match
  (re-derive without the tool) but always refuses on source-pak drift.
- Per-entry SHA1 gating is safe: every retained producer family verifies
  (UnrealPak fixtures, promoted writer, earlier in-repo writer), and the
  intentional corruption fixture fails exactly there. The path-hash
  secondary is verified per entry (seeded FNV-1a); producers differ in the
  declared range size, so the check is a floor with inert extra bytes.
- One real later-wins interplay fixed en route: closest-match refuses when
  another mod also provides the pid (a repoint on a losing entry would be
  silently overwritten); hide stays per-mod (well-defined suppression).
- The old verification loop iterated every mod chunk id; under later-wins
  it compared a loser's payload against the winner's row (masked in GK's
  tests by identical synthetic payloads). Now verifies winner-owned rows
  only — a latent correctness fix.
- No console contact, no builds, no network; repak/retoc binaries invoked
  read-only from their pinned trees.
