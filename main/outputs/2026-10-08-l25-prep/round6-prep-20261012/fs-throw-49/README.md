# fs-throw-49 (Mac-only track output, 2026-10-12)

Start with **CLASSIFICATION.md**. In one line: none of the 49 throwing filesystem
calls can crash the game on PS5. 33 never run there and 16 are already caught.
One optional parity patch (P1) is staged.

Nothing here touched the console. No PRX was built. One exception to "write
only in this folder": in the first pass, the first two gate self-test runs used
the Mac's system temp folder (Python `tempfile`, `fs_throw_gate.py:334`). The gate
deleted those temp files itself. All later runs pin `TMPDIR` to `gate/tmp/`.
`gate/tmp/` also holds the pre-review copies (`*.before-review-fix`) of the files
changed after the adversarial review.

| File | What it is |
|---|---|
| `CLASSIFICATION.md` | The decision: plain verdict first, then all 49 sites, P1, extra findings and unknowns |
| `classification.json` | Machine-readable version of the 49 rows (file:line, function, PS5 path, verdict, handler, what happens, PC behaviour) |
| `check_classification.py` | Proves the JSON covers exactly the gate's 49 sites and the baseline keys. Bounds-checks the 266 file:line citations (the line exists; code text not checked). Checks the code text of 97 anchor lines. Proves the two factories without a mod source have no caller (4,037 files). Exits non-zero on zero or mismatch |
| `check-classification-result.txt` | Its output (PASS) |
| `REVIEW.md` | The adversarial reviewer's notes (5 minor problems, all fixed in this pass) |
| `fs49_context.py` | Helper. For each site, lists the enclosing function and any try/catch in the same file, using the gate's own scanner |
| `context-current-tree.txt` | Its output for the 11 baseline files |
| `patches/P1-posixfile-parent-dirs.patch` | Optional P1 as a unified diff against `candidate/src/File/FileType/PosixFile.cpp` |
| `patched/outputs/.../PosixFile.cpp` | Staged copy with P1 applied. Never copy it into the candidate tree without a labelled build |
| `control/outputs/.../PosixFile.cpp` | Byte-identical unpatched copy (sha `7ae47172…`), used as the control |
| `rerun_gates.sh` | Reruns all 8 gate checks and rewrites `gate/*.txt`. Prints the run count and fails on an unexpected exit code |
| `gate/` | Gate outputs, the file lists they used, and the rebased baselines. `RERUN-GATES-SUMMARY.txt` is the one-page summary. `tmp/` is the gate self-test scratch area (empty) |
| `gate/compiled-set-current-l26A-pass1.list` | The current build's compiled files (3,561), taken from `l26-A/pass1/objects/*.d` |
| `gate/compiled-set-{CONTROL,P1}.list`, `gate/baseline-1e49188b-rebased-{CONTROL,P1}.txt` | The same list and baseline with only the PosixFile path pointed at `control/` or `patched/` |
| `gate/unscanned-prebuilt-deps.list` | 335 sources/headers of prebuilt linked objects that the original gate list missed (SDK paths excluded) |
| `gate/grep-scope-1e49188b-plus-candidate.list` | File scope used for the caller greps |
| `gate/baseline-files.txt` | The 11 files named in the baseline |
| `syntax_check.py`, `syntax-check-summary.txt`, `syntax-check/` | One-at-a-time `-fsyntax-only` checks: exact build argv (PS5 target) and host clang, for the control, P1 and a deliberately broken copy. `syntax-check/negative-control/PosixFile.cpp` is broken on purpose, never use it |
| `host-test/` | P1 host test: `test_p1_posixfile.cpp`, `run_p1_host_test.py`, the result `P1-HOST-TEST-RESULT.txt`, build outputs in `build/`, scratch dirs `run-*/` |

How to re-verify (Mac only, about 1 minute):

```
python3 -B check_classification.py
zsh rerun_gates.sh
python3 -B syntax_check.py
python3 -B host-test/run_p1_host_test.py
```

The last two check that `vm_stat` shows at least 2 GiB free before every compile.
