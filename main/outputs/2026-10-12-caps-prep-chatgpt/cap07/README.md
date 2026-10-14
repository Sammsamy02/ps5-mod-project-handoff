# Read-only container selector

Run from the repository root with Python 3 (standard library only):

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/discover_containers.py \
  --base-dir PATH_TO_CAPTURED_BASE_TOC_FOLDER \
  --mod-utoc PATH_TO_ORIGINAL_MOD.utoc
```

Repeat --mod-utoc for multiple inputs. --overlay-module can point to an explicitly selected existing overlay_builder.py. The parser source snapshot/hash is recorded; original parser code is never edited. The tool writes JSON to stdout only. Exit 0 means one candidate selected within the supplied capture, never runtime readiness. Exit 2 means refused. Empty scans refuse. Unsupported files are not silently skipped.

Tests (existing synthetic fixture writer is a read-only dependency; keep this folder under repository outputs):

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/test_discover_containers.py -v
```

Create cap07/tmp if absent when extracting the package. Temporary synthetic files stay there. Real replay is saved in evidence/leon-container-selection.json: the conservative multi-container refusal is expected and useful, not a successful mount or overlay composition.

## Compact offline package projection

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/project_packages.py \
  --utoc ORIGINAL_MOD.utoc --ucas ORIGINAL_MOD.ucas \
  --keep-json PACKAGE_IDS.json \
  --output-dir outputs/2026-10-12-caps-prep-chatgpt/cap07/NEW_PROJECTION
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/test_project_packages.py -v
```

PACKAGE_IDS.json is a nonempty array of unique 16-hex-digit package IDs. Output directory must not already exist. The command computes/verifies all bytes before creating output, then writes receipt last. Exit0 means offline projection verified; exit2 refuses. Keep output within this prep folder. Original inputs remain read-only. Existing parser/fixture sources are read-only dependencies, with hashes retained in receipts.

Nonheader exports/bulk remain byte-identical; header, directory and stored-block geometry are derived. This is a new intermediate input for composition, not an unchanged original archive or a standalone admitted mount. Source unsupported chunks refuse rather than disappear. No automatic import-owner assignment, material conversion or complete-catalogue claim is made.

Real replay is in derived-secondary/; its 2-package UCAS is 123,312 bytes. The required custom M_Invisible dependency is outside that exact subset; its 3-package mod-store closure is recorded in evidence/leon-secondary-mod-import-closure.json. Both closure/routing and material compatibility remain unresolved. The small output must not be deployed as a complete Leon overlay.

## Multi-container composition library

compose_by_owner.compose(api, project_packages, bases, mods, unmatched_placements, scratch_dir) returns all generated TOC/sidecar byte pairs and a source-bound receipt. api is the pinned existing overlay_builder module; mods are its source ModSpec objects. Each base dictionary supplies base_id, immutable utoc bytes, captured header bytes and declared integer ucas_size. unmatched_placements maps (mod_id, integer package_id) to a base_id. A missing placement refuses; a supplied placement is caller configuration, never evidence of a new package or complete catalogue. scratch_dir must exist inside cap07. Temporary derived files are removed on success/failure; the library publishes no files or handles.

```sh
python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap07/test_compose_by_owner.py -v
```

Ten tests cover the complete per-owner fixture and refusal/cleanup paths. The real refusal receipt identifies the missing secondary store. No caller should publish this library's outputs until physical base identities, complete ownership/material compatibility and the actual read-route/lifetime requirements are admitted separately.
