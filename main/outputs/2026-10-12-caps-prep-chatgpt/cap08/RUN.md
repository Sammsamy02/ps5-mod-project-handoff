# Receipt gate (Mac-only)

Implemented standalone directive_gate.py + classification_adapter.py; original analyzer/runtime remain unchanged. 14 local fixture tests pass. tdd-red*.txt preserves missing-feature failures; tdd-green.txt contains the final scoped suite. Synthetic positive fixtures validate gate behavior, not real asset compatibility.

From the repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap08 -p 'test_*.py' -v
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap08/classification_adapter.py outputs/2026-10-12-caps-prep-chatgpt/cap08/classification-rerun.json outputs/2026-10-12-caps-prep-chatgpt/cap07/evidence/leon-container-selection.json --out outputs/2026-10-12-caps-prep-chatgpt/cap08/leon-gated-directives.json
```

Second command intentionally exits 2 (REVIEW_REQUIRED), with zero actionable rows. The real capture has 68 index overlaps: 66 in chunk0, 2 in chunk10. Classifier's store scope is chunk0 only: its 61 unknown store memberships include those 2 plus 59 unmatched in captured indexes. This is cross-container evidence coverage, not arithmetic error. No unknown is relabeled added.

An optional --receipts manifest requires schema=1, classification={path,sha256}, catalogue_complete=true, containers=[{container_id,store:{path,sha256}}], packages keyed by 16-digit package id. Each candidate needs store_container plus identity, intent, compatibility receipt refs. Hiding also needs target receipt ref. See test_directive_gate.py for executable fixtures. The adapter never promotes a discovery with catalogue_completeness_verified=false.

Each receipt file has schema=1, kind, facts, nonempty sources=[{path,sha256}], extractor={path,sha256}. Facts identify package_id; identity requires identical mod/base export names and classes; intent explicitly permits look loss. Compatibility identifies source/target classes and target package with import_compatible=true. Hiding additionally requires verified hide intent, target container/package, public export hash and a nonempty native shader-store hash view. Actual store v5 bytes are parsed with bounds and duplicate guards.

Limit: facts are explicit source-bound extractor attestations. This gate verifies hashes, subject consistency and required fields; it does not independently establish that an arbitrary extractor decoded export semantics correctly. Complete catalogue truth must come from the acquisition/extraction workflow, not guessed here. VALIDATED_DIRECTIVES means eligible derived-overlay instructions only, never a built/installable artifact or console approval. Builder reparsing, closure/hash checks, immutable-input proofs and visual console evidence remain required.

Bounded source-derived Zen metadata reader and real original-mod replay added: see ZEN-IDENTITY.md. It replaces heuristic/attested export names/public hashes/class-reference bytes for three selected subjects with direct parsed evidence; unresolved script class names and missing base payload still block compatibility.
