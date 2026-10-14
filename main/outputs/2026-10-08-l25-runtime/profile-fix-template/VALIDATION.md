# VALIDATION — actual measured behavior of the template mechanics (2026-10-08)

All runs Mac-local under `profile-fix-template/validate/`. Live tree untouched.
The pass case used `values.example.json` (current values + placeholder
`UObjectDeleteListenersCritical=136`), i.e. the "no-op corrections + one new
fact" path.

## R1 — full instantiation on a sandbox copy: PASS

```
cp -R ../../profile validate/sandbox-profile
python3 instantiate_profile_fix.py --values values.example.json \
  --root validate/sandbox-profile \
  --remap-from <ABS>/outputs/2026-10-08-l25-runtime/profile \
  --emit-patch validate/instantiated-example.patch
```

Actual behavior:
- `/usr/bin/patch` applied all 5 file sections cleanly (no rejects, no fuzz):
  `profiles/townfall-build-facts.json`, `units/CompiledNativeLayoutDeclarations.hpp`,
  `units/RuntimeBuildProfile.hpp`, `units/JoinedLayoutPreflight.hpp`,
  `tests/run_tests.py`.
- Digest recomputed from the patched JSON
  (`112e9863c6e24ef8557d17a63ce924f3a46cce608addd214fe01859f1448c78e`) and
  written into both headers; CHANGES.json rewritten: candidates remapped to the
  sandbox, all four `candidate_sha256` rows recomputed, counts
  75/88/69/56, `listener_new_facts: 1`, scope/regenerate updated.
- Post-state checks: JSON parses; INI `[FUObjectArray]` ends up sorted with
  `UObjectDeleteListenersCritical=0x88` between `UObjectDeleteListeners=0x78`
  and `bShouldRecycleObjectIndices=0xb4`; every member < `UEP_TotalSize`;
  69 joined rows; digest present in all three locations.
- **The sandbox's patched `tests/run_tests.py` was then run and PASSED**
  (exit 0, same PASS summary line as baseline). One-line shim required, because
  the sandbox sits two directory levels deeper than the live profile and the
  script derives its workspace root by position:

  `ws=root.parents[2]` → `ws=Path("<ABS>/what")`

  (first shim attempt used a trailing `# comment` on that one-line statement
  chain and commented out the `old=…;prior=…;generic=…;sibling=…` tail —
  `NameError: name 'old' is not defined`; the shipped shim has no comment).
  This shim is validation-only; the live run needs nothing like it.

## R2 — raw unsubstituted template: applies, then fails predictably

`/usr/bin/patch` with the raw `profile-fix.diff.tmpl` (slot markers as text)
APPLIES (exit 0) — slot markers are ordinary text to patch(1). The result is
deliberately unusable and fails loudly at the first consumer:

```
python3 -c "import json;json.load(open('…/profiles/townfall-build-facts.json'))"
json.decoder.JSONDecodeError: Expecting value: line 1099 column 17   # <<CREATE_LISTENERS_DEC>>
```

The helper itself refuses earlier and more clearly: a values file missing any
of the six fields dies at once (`values file missing fields: […]`), values with
slot markers die at load, and `build_patch` aborts if any non-digest slot
survives substitution. Net: an un-filled template can never reach the build.

## R3 — inconsistent values refused before any write

Values with `UEP_TotalSize` smaller than a member offset (e.g. 100) are refused
pre-patch: `members at/after UEP_TotalSize (INI guard would refuse): […]`.
Nothing is written (verified with `--dry-run` and a scratch root).

## Bugs found and fixed while validating (documented for the record)

1. Template initially inserted a literal `\` + `n` into the decoded INI string,
   which `json.dumps` double-escaped to `\\n`; the helper's INI guard caught it
   (`ValueError: invalid literal for int() … '0x78\\nUObjectDeleteListenersCritical=0x88'`).
   Fixed to a real newline in `make_template.py`; regenerated.
2. `rewrite_changes` used `Path.replace` (a rename) for the include-directory
   remap → TypeError; fixed to string replace.
3. `--remap-from` must be an ABSOLUTE path: a relative `../../profile` silently
   resolved against the CWD to a nonexistent dir and skipped remapping (caught
   because the printed candidate hashes equaled the live pre-fix hashes).

## patch(1) CWD subtlety

`patch -d <root> -i <patchfile>` resolves the patchfile relative to the `-d`
directory, not the caller's CWD. The helper passes an absolute tempfile path, so
it is unaffected; keep this in mind for any manual application.

## Postscript — live landing while validating (2026-10-08 ~20:45 UTC)

While these docs were being finalized, a downstream agent applied AA's
binary-verified values to the LIVE profile via `apply_engine_binary_fix.py`
(staged in this template directory; it explicitly follows this template's §8
drop path). Post-landing verification by AM (read-only, backup/compare):
`tests/run_tests.py` exit 0 (`… prior88 immutable + 5 listener-zone corrected,
l24 corroborated`), deterministic regeneration, all four CHANGES pin rows
consistent. Facts learned that change this template's guidance:

1. **The default Critical-INI path is hazardous**: `NativeLayoutBinding.cpp:74`
   throws "Unknown native member" for INI keys without a
   `NativeLayoutSchema.inc` row, and `FUObjectArray::UObjectDeleteListenersCritical`
   has none — publishing it (my template's default) would break the runtime
   binding. The sanctioned path is the drop (§8), as applied. R1's sandbox pass
   exercised the mechanics only; its Critical key would have failed the real
   runtime binding despite passing the host profile tests (the profile suite's
   configparser has no schema check — a gap between host tests and the runtime
   binding worth remembering).
2. **Stale-template behavior verified**: applying the raw `profile-fix.diff.tmpl`
   to a copy of the POST-fix live profile fails loudly (`patch` exit 1, `.rej`
   files for the drifted hunks). Caveat: patch is per-file atomic, not
   multi-file atomic — some file sections can apply before a later one rejects;
   the §0 baseline-hash precondition plus the G2/run_tests gates are the real
   guards against partial application.

## Artifacts

- `validate/sandbox-profile/` — instantiated + validated copy (tests/ artifacts
  are from the passing R1 run; tests/run_tests.py carries the one-line shim).
- `validate/instantiated-example.patch` — the substituted patch (digest slot
  unfilled, as designed pre-apply).
- `validate/r1-run_tests-stdout.txt` / `r1-run_tests-stderr.txt` — passing run.
- `validate/r2-patch.log` — raw-template apply log.
- `baseline/` — pre-fix hashes, test stdout/stderr, RESULTS.json + test
  binaries backups, and the `build_variants.py --plan` baseline gate log.
