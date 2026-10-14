# Independent review (one reviewer, 2026-10-13) and what was changed

Scope: tools/*.py, tests, run_all.sh, DESIGN.md, evidence. Reviewer ran the tests (21/21 then) and the
classifier in its own scratch folder; read-only, no console. **Verdict: APPROVE WITH FIXES, no blockers.**

| # | Sev. | Finding | Fix (this folder) |
|---|---|---|---|
| 1 | MAJOR | G7 failed open without `--registry` or when the game's class is not provable | G7 now gives UNKNOWN whenever a game asset is replaced and its class cannot be proven; test added |
| 2 | MAJOR | only the primary class was judged; other export classes ignored | every other export class must be in the primary kind's allowlist (`asset_rules.sub_allowed`), else UNKNOWN; mutation test added |
| 3 | MINOR | a 'sub' class as primary fell through to LIKELY | now UNKNOWN |
| 4 | MAJOR | G8 listed engine functions only | generic rule added: any function of a class named `*Save*`, or `Save/Delete/Clear/Remove/Reset/Write...(Game/Data/Slot/Save)`; `defines-save-class` is a finding too; tests added |
| 5 | MAJOR | pair_compare compared unions; errors coerced to "clean"; files outside the TOC accepted; >1 MB files not hashed; decoder optional | per-sample signatures must match a console sample; unreadable structure = differs; files outside the given TOCs refused; BLAKE3 on every file (sha256-keyed receipt cache); skeletal meshes need the decoder to read every PC sample; decoder sha256 recorded; the classifier also checks the package's own bulk signature against the console signatures |
| 6 | MAJOR | "console-proven" overclaimed (`[pk] T1 found=8/8` counts MICs; no report says 25 meshes drew) | SkeletalMesh and DNAAsset downgraded to LIKELY_AS_IS ("earlier launch H, user-visual"); converter statuses reworded "console-tested, user-visual"; DESIGN fixed |
| 7 | MAJOR | save plan: S1 not marked as a console action; "undo from S2" contradicts the no-upload rule; S4 restore meaningless for a new slot | S1 marked (user OK); backup is now logical (decoded values of every slot) plus the user's own system copy; plainly "no file-level undo"; a failed new-slot import leaves an extra slot for the user to decide |
| 8 | MINOR | wrong facts (flags of all 151 packages, decoder misses, localization unbacked, REVIEW.md missing, "same format" stated as fact) | corrected; `evidence/localization-check.txt` added; this file |
| 9 | MINOR | hand-written `Paces_DT` default and `--target` default | `--must` default empty (run_all takes it from the PC samples' overrides); `--target` required, run_all reads it from the game's `.stinfo` pak entry |
| 10 | MINOR | bank-plan trusted console file names | regular files only, plain names only (no `/`, `..`, ` -> `); test added |
| 11 | MINOR | `--mod-pak` paired by position; provenance gaps; G3 checks packages, not export hashes | `--mod UTOC UCAS [PAK or -]`; sha256 of registry / pair evidence / global recorded; export-hash check listed as a known limit |
| 12 | MINOR | gvas: SkippedSerialize accepted; bool size unchecked; docstring; output label | refused / checked / fixed; output says "prototype, untested on a real save" |

Known limits kept (documented in DESIGN): G3 does not check imported public export hashes; LIKELY verdicts
rest on structural shape plus earlier user-visual launches, never on decoded property data.
