# REPORT — worker-pad-hardware-probe (2026-10-04)

## Status: DELIVERED AND FROZEN (local build + host verification; NO console run)

The standalone pad input probe exists as a ploader-loadable PRX,
**`modbridge_pad_probe.prx`, build `padp-79947e016dfe`** (20427 B, sha256
`deccca38e58ac76ea38e1a80b780eb3235fd4dfab7f81a32f776621cac75803a`,
byte-identical across determinism run1/run2). The user test that decides
the question — launch Townfall with the module loaded, press OPTIONS,
check `/data/pad-probe-events.jsonl` — is the controller runbook in
`STAGE-PLAN.md`. No in-game result is claimed.

## Task coverage

1. **PRX built from the prepared module's logic (standalone, NOT the
   composed runtime).** The prepared `worker-real-input-pad/pad_probe/`
   sources are C++ over libc++ (`std::shared_ptr/vector/function/string`),
   which the ploader PRX lineage does not carry — the console-proven v2/v3
   modules are freestanding with 7 imports. So `pad_source.c` is a
   line-faithful C PORT of the retained provider (exact PadBackend vtable,
   validity gates, activate() sequence incl. the pinned 0x80960003
   already-initialized acceptance, duplicate-timestamp suppression,
   baseline/reset edge semantics, phase names; the provider's
   PS5_PadData layout static_asserts re-verify at build time), and
   `pad_probe_core.c` ports `PadProbe.cpp`'s run contract exactly:
   probe_start first, activate, register ONE keybind (OPTIONS->F7),
   bounded poll, probe_end/probe_failed last, same record vocabulary.
   The real backend table is wired in `pad_probe_module.c` — exactly
   PS5PadNative.cpp's scePad*/sceUserService* set: the first-ever hardware
   contact for this backend when it runs.
2. **Same packaging recipe as all working probes.** Frozen v3 lineage
   reused exactly: import_references-first `ld.lld -r` combine, LOUD
   forbidden-symbol gate, SharpProspero Packager (vendored harness,
   module name `modbridge_pad_probe`), frozen finalize_sdk.py
   (SDK 0x11000000), pinned make_fself; lineage module/log naming
   (`modbridge_pad_probe.prx`, `/data/pad-probe-events.jsonl`).
3. **Entry marker first, then one keybind, then polls** — module_start
   writes seq=1 `entry` BEFORE any pad/service contact, seq=2 `startup`
   (pid, CLOCK_MONOTONIC health, sysctl KERN_VERSION, ctors_run),
   then the probe. One deliberate extension, documented in code: the poll
   window is WALL-CLOCK bounded (30 s + 5 s post-first-fire grace; the
   iteration budget stays as backstop) because module_start runs
   synchronously and a human must have time to press OPTIONS. Teardown
   closes the pad handle and releases the user-service lease ONLY when
   owned (retained deactivate semantics) — the game keeps a clean pad.
4. **Host-test against synthetic pad events — PASSED** (ASan+UBSan):
   6 scenarios, structural checks + line-by-line JSONL checks green
   (duplicate suppression, no phantom replug edge, user-init failure,
   NoUser, invalid-binding gate, factory failure, teardown/lease
   release). Two test-side bugs found and fixed during the run
   (NULL-arg deref in the scripted user_initialize; scenarios D/E
   memsetting cfg so requested_user=0 faithfully matched user 0 in the
   zero-filled list slots — the port's retained selection semantics were
   correct, the scenarios now request -1 like the console module).
5. **Two-run determinism:** run1 == run2 byte-identical at fself, inner
   ELF (69920 B), combined object, and contract; frozen lineage hash
   verified before building; LOCAL_PACKAGE_CHECKS_PASSED (14 NID-encoded
   strong dynsym, 0 weak, forbidden gate, init/fini 1/1, explicit
   DT_INIT/DT_FINI, SDK stamp, fself segment equality).
6. **STAGE-PLAN.md:** upload + ftpsrv decryption-oracle expected values,
   ploader.ini entry (mode-2 line, with pre-image backup), REQUIRED
   isolation (composed runtime must not co-load — pad service lease race),
   launch ritual, the user's press procedure, entry-marker triage table,
   rollback.

## Import identity upgrade (found during build)

The frozen 191-import aligned-entry contract ALREADY contains all
7 pad/service identities with Townfall's own executable import
declarations as evidence (encoded NIDs, e.g. scePadReadState
`YndgXqQVV7c#A#B`). All 14 rows of this module are therefore byte-identical
frozen-contract rows, cross-checked field-by-field against
`ue4ss-public-import-declarations-1` in the build script (mismatch = LOUD
failure). The aligned-entry runtime built from this same contract loaded
on console — materially better than the planned declared-input fallback.
Remaining honest risk: THIS new plugin's own load (pad rows inside a
ploader plugin) has no direct precedent; a silent load-rejection is a
possible outcome and STAGE-PLAN treats it as a real result, not a rerun
loop.

## Not done / not claimed

- No console staging, launch, or controller evidence — the PRX's
  `native_executed` and `in_game_results` are false; the runbook decides.
- The retained C++ Handler/PS5PadSource objects are NOT linked (libc++
  unproven in this PRX lineage); the probe tests pad DELIVERY through this
  module's registered binding, not the composed runtime's Lua dispatch.
- The ini mapping path (PS5Pad.ini) is deliberately not exercised here —
  it belongs to the composed-runtime step after this probe passes
  (prepared-module contract).
- Nothing outside this output dir was modified; all vendored lineage files
  are labeled copies, originals untouched.
