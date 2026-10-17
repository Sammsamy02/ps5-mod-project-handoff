# Preserved native diagnostic artifacts

These files are an unexecuted, restricted x86-64 ELF candidate and its static reports. They are **not ready for hardware execution**. The corrected load pages pass static verification, but the installed loader identity, resolver convention, output capture and spawned-process return still need resolution. Nothing in this folder establishes firmware 13.60, UE4SS, Mod Engine 2, asset translation or gameplay support.

`manifest.json` records the exact file hashes and sizes. Candidate SHA-256: `33fba54b4e18d9467a3b295aca080a411729758e12ca91f6015c9dd74dd3be3f`; size 50,848 bytes. Source, GPL notices and repeatable preparation/build instructions are in `../ps5-mod-bridge/native-probe/`. The verified local build toolchain remains in the managed development worktree; it was not copied into this artifact folder.

Static acceptance can be repeated without executing the ELF:

```sh
python3 outputs/ps5-mod-bridge/native-probe/verify_elf.py \
  outputs/native-probe-artifacts/native-probe.elf
```

See `../2026-09-30-loader-abi-review.md` and `../2026-09-30-native-layout-report.md` for the source evidence and remaining boundaries.
