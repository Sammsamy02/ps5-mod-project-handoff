# REPORT.md — worker-fileio-rebuild (2026-10-04)

Worker: worker-fileio-rebuild. All writes under
`outputs/2026-10-04-zcode-team/worker-fileio-rebuild/` plus the build script's own
declared `--output` directory. No network, no nested subagents. 10 of 20 tool calls used.

## Outcome: SUCCESS

The fileio SDK candidate was rebuilt with the correct target-side report path
`/data/modbridge-owned-dll-candidate.jsonl` (previous candidate used the default
`/tmp/modbridge-owned-dll-candidate.jsonl`, unreadable via FTP — same lesson as
InitOnce c49108fe). Receipt verified: `report_path` is now the `/data/modbridge-owned-dll-candidate.jsonl`
FTP-readable path.

## Build status

`OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED` (build exit 0).

## Two deviations from the literal task command (both required, both verified)

1. **Build script path**: `outputs/ps5-mod-bridge/windows-candidate/build.py` does NOT
   support `--fileio`/`--fileio-root` (argparse error, exit 2; zero `fileio` hits in that
   file). The fileio-patched build script is the vendored copy from worker-fileio-apply:
   `outputs/2026-10-04-zcode-team/worker-fileio-apply/vendor/windows-candidate/build.py`
   (per its REPORT.md: patches 01–07 applied to a vendored copy, original untouched).
   I used the vendored script with the exact requested flags.
2. **Explicit tool paths**: the vendored script derives `ROOT=HERE.parents[2]`, so its
   default `--lld-link` (and sdk/llvm defaults) resolved inside the worker dir and failed
   (FileNotFoundError). Re-ran with explicit `--sdk`, `--llvm-bin`, `--lld-link` pointing
   at the real workspace toolchain paths (existence verified before the successful run).

Exact successful command (run from workspace root
`/WORKSPACE/.codex/worktrees/translation-core/what`):

```
python3 outputs/2026-10-04-zcode-team/worker-fileio-apply/vendor/windows-candidate/build.py \
  --entry --clock --workers --sync --init-once --mutex --fileio \
  --fileio-root /data/modbridge \
  --report-path /data/modbridge-owned-dll-candidate.jsonl \
  --output .superpowers/native-dependency-spike/windows-fileio-candidate-1 \
  --sdk "$PWD/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk" \
  --llvm-bin "$PWD/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin" \
  --lld-link "$PWD/.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/lld-link"
```

Note: `--report-path` is a target-side profile path compiled into the ELF ("Build never
opens it") — no host write occurred at that path; the controller stages the artifact below.

## Verification (all PASS)

Build ID: `664f83a5e8374059890f12c9b1e043f8`
Receipt: `.superpowers/native-dependency-spike/windows-fileio-candidate-1/664f83a5e8374059890f12c9b1e043f8/receipt.json`

- `receipt.result_profile.path` == `/data/modbridge-owned-dll-candidate.jsonl` — assert PASS
- `owned_profile.h` line: `MB_OWNED_REPORT_PATH "/data/modbridge-owned-dll-candidate.jsonl"` — PASS
- `result_profile.stdout == True` (console delivery still enabled), `build_id` matches dir
- `fileio_profile`: `{"enabled": true, "win64cc_defs": 20, "actual_pe_imports": 10, "namespace_root": "/data/modbridge", "handle_close": "native mb_win_fileio_close_handle delegation", "target_execution": false}`
- Receipt contains zero `"status": "error"` records
- Console test (host driver, run by the build): PASS —
  `Real DllMain attach gates 86/93 exports; both detach callbacks use live heap/TLS services`
  (`host-driver-normal.stdout`); `host-normal.jsonl` 4 records, all fileio-enabled, no
  unexpected-disabled assertions. `host-release.jsonl` and worker-failure controls also produced.

## Artifact for controller staging/dispatch

- ELF candidate: `.superpowers/native-dependency-spike/windows-fileio-candidate-1/664f83a5e8374059890f12c9b1e043f8/owned-dll-candidate.elf`
  - sha256: `42c0cae553057415bd4d055a71cd31eaa34a94dd149b7479f09f65a6b5ab1405`
  - bytes: `279504`
- Receipt: same dir `receipt.json`
  - sha256: `58c8c1a241241ef2ed5514b1ee547ad484ca39072ecdd3655496c52b39980bdc`
  - bytes: `173301`

(Absolute prefix for both: `/WORKSPACE/.codex/worktrees/translation-core/what/`)

## Honest scope

Compile-only candidate (`COMPILED_NOT_EXECUTED`): target-side report writing to the new
path has NOT been executed/observed on target; the previous /tmp report lesson is fixed
at the profile level (header + receipt), but actual on-target receipt retrieval via FTP
remains controller-side work. No claims beyond the checks listed above.
