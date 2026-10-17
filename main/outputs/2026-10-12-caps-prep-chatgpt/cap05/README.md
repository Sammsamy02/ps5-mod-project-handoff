# Offline crash reporter

Mac-only postmortem analysis; no live debugger, compiler or console interaction.

One-command replay from repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap05/crash_report.py --klog outputs/2026-10-08-l25-prep/evidence/tmp-preserved-20261010/tmp/l25-variant-A/klog-20261009-021938.log --build-id historical-20261009-fixture --output outputs/2026-10-12-caps-prep-chatgpt/cap05/preserved-crash-report.md
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap05 -p 'test_*.py'
```

Optional exact local artifact inputs: `--prx HASHED_PRX --symbols-sidecar SIDECAR_JSON --prx-module EXACT_MODULE_BASENAME`. With a sidecar, HASHED_PRX must match that fully parsed sidecar's prx_sha256; the current packager binds its packaged SELF and separately records the bare extraction SHA. Without a sidecar, --prx must be a bare ELF for direct dynsym extraction. The sidecar requires a PRX SHA match. An explicit module basename is required for either symbol-artifact input; symbols never attach to another module just because offsets overlap. A module basename is caller-supplied ownership, not proof that the captured module is that build. Missing/unknown owners stay unresolved. Nearest matches outside known size bounds remain module-only; bounded matches state that capture pairing is unverified.

Report records klog, symbolizer, PRX and sidecar hashes as available; module fingerprint comes from capture. `--build-id` is a caller assertion. Local PRX/sidecar pairing never proves the capture used that PRX. No final-link map/ELF or game-symbol sidecar options are provided, avoiding their currently unverified pairing. Input originals are read-only. Output must remain in cap05 and cannot overwrite an input. Human text redacts IPv4 literals; it does not dump the original log.

Exit 0: crash blocks found and report produced (not proof of resolved functions). Exit 1: NO CRASH BLOCKS, not crash proof. Exit 2: input/pairing/output error. Tests use small synthetic nonexecutable artifacts inside cap05/tmp and preserved logs read-only. Both tools disable bytecode on import/run.

Fresh tests: 8/8, including empty capture, stale pairing refusal, required ownership, same-offset wrong module exclusion, bounded/unbounded symbols, and unknown ownership. Preserved real crash replay: 2 blocks, both RIP libkernel.sprx +0x224c, MODULE_ONLY. No fresh console claim.

Historical blocker: phasea-tools/prx-symbols-l26-package-A.json declares PRX SHA 9ef26a8f089b958337813cdf2df8ac25f67706635ec8419d9cc90b0013705037, while current package-A/pass-1/bare-original PRX hashes 7fead02d7a67c9bd36024e2fd942f1e56e841cb48c10d47d7123d96f31d8b28d. The tool refuses this pair; the historical 25/25 receipt cannot establish current pairing. Needed: final-link sidecar for each exact build plus captured-module/build association evidence. Neither was fabricated here. A newer packaging sidecar now matches the current packaged PRX through the actual loader: symbol-input-audit/ROOT-LOADER-PAIRING-REPLAY.json records full JSON/hash validation and two exact dynamic anchors. Its bare extraction hash is separate, reconstructed -r rows stay unused, and capture/build binding/full internal-function coverage remain unproved.
