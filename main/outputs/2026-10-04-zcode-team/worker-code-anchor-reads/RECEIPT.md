# RECEIPT — worker-code-anchor-reads — modbridge_ingame_facts.prx (igf5)

- Worker: worker-code-anchor-reads (ZCode team, PS5 mod-manager)
- Date: 2026-10-04
- Build ID: `igf5-4186048a6dd6` (prefix `igf5-`; FROZEN delivery)
- Variant: igf5-code-anchor-reads
- Mission: the ONLY remaining unresolved check from the igf4 anchor
  validation — read the 16 code-anchor bytes at Tick `0x33ac8c0` and SCO
  `0x890aa0` (VMMAP reports the exec span execute-only, prot=0x4) under the
  controller-ordered mprotect-FIRST sequence, plus the GUA predicate replay
  with the corrected GC-order check.
- Module name / log path: UNCHANGED — `modbridge_ingame_facts` /
  `/data/modbridge-ingame-facts.jsonl` (same-file replacement of
  igf4-4891869337b4, no ini change).
- Entry marker: seq=1 `exp="entry"` written+flushed FIRST (lineage
  contract); focused checks are seq 3..7.
- Native execution: NONE. In-game results: NONE. Local packaging + host
  checks only.

## Artifact

| Artifact | Bytes | SHA-256 |
|---|---|---|
| `modbridge_ingame_facts.prx` (fself, == `determinism/run1/` == `run2/`) | 25981 | `486845b7e121d5993febdf812ac55815cc974fc1e2a062ebb4ac60fc5b8f8dfa` |
| inner ELF (`bare-sdk/modbridge_ingame_facts.prx`, SDK 0x11000000) | 85472 | `427fbf2f90aec4982f7e73456d289a5ed5c150d1ae753cfd62377d510dc6f646` |
| `combined-ingame.o` | — | `1abadb418c271749a92256d55f935ca3c5e5b104e4b1e9c77c07c7d505cc110f` |
| `import-contract.json` | — | `e3755524d3364144bd7ab3356eb2fbee8ddfaf248a2a9ba2b21800f90e1492e9` |

Frozen lineage re-verified before the build: `native-full-runtime-aligned-
entry-1/build-1/modbridge_runtime_aligned_entry.prx` sha256
`3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654`
(build-receipt `hash_verified: true`).

Determinism: run1 == run2 byte-identical on final PRX, inner ELF, combined
object, and contract.

## Imports (UNCHANGED 7-verbatim packaging)

Exactly 7 NID-encoded strong dynsym imports (artifact check), zero weak,
forbidden-symbol gate passed INCLUDING `sceKernelMprotect` (which is
conspicuously NOT an import — see Mechanism):

`1G3lF1Gg1k8#A#B, 4wSze92BhLI#A#B, 9BcDykPmo1I#A#B, DFmMT80xcNI#A#B,
HoLVWNanBBc#B#B, UK2Tl2DWUns#A#B, lLMT9vJAck0#B#B`

## The mprotect mechanism (controller-ordered mprotect-FIRST design)

- sceKernelMprotect import verification requested by the controller is
  NEGATIVE: no verbatim row in the frozen 191-contract, no declaration in
  pinned SDK v0.43, no observed row in the retained libkernel export
  metadata; the cited windows-candidate mprotect is HOST-POSIX in the
  Windows loader emulation, not a console import.
- Implemented instead: inline x86-64 syscall shim (2 `syscall` instruction
  sites in the combined object, asserted by the artifact check). Constants
  parsed at BUILD TIME from the PINNED SDK headers (nothing hardcoded):
  `SYS_mprotect=74` (sys/syscall.h:82), `PROT_READ=0x01 / PROT_EXEC=0x04`
  (sys/mman.h:52-55), `PAGE_SHIFT 14` = 16 KiB pages (machine/param.h:99).
  Signature `int mprotect(const void *, size_t, int)` (sys/mman.h:263).
- Numbering-preservation evidence: the console-proven lineage successfully
  calls plain FreeBSD wrappers getpid(20) and sysctl(202).
- RESIDUAL RISK requiring controller sign-off BEFORE staging: the kernel
  syscall number cannot be verified on-host. Refusal path is safe by
  construction (no read without mprotect success).

## Vendored byte-identity (igf4 lineage, loud-verified at build time)

record_sink.c `2b6a8c24…`, record_sink.h `539ac098…`, ingame_libc.c
`7e98a992…`, ingame_libc.h `3eec6579…`, vmmap_walk.c `e1798546…`,
mapscan.c `ce32b176…`, anchor_store.h `4b587b5d…`, ingame_abi.h
`b9543d74…`, import_references.S `705cd7c8…`, boundaries.S
`5f365b14…`, sentinels.c `61bbee66…` (full hashes in build-receipt.json
`vendored_byte_identity`). New code: anchor_pages.{h,c} (pure, host-tested),
sys_mprotect_shim.h (inline asm), code_anchor_probe.cpp, module_entry.c
variant.

## Expected bytes (anchor ground truth, cross-worker confirmed)

- Evidence: `worker-anchor-validation/expected_bytes_tick_sco.txt`
  (retained image `decrypted-eboot.bin`, sha256
  `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`;
  file_offset = profile_vaddr + 0x4000).
- Cross-checked at build time against the INDEPENDENT
  `worker-expected-bytes/EXPECTED-BYTES.json` (anchors 0/1 bytes_hex[0:32]
  equal; loud failure otherwise).
- Tick 0x33ac8c0 (off 0x2fb08c0) and SCO 0x890aa0 (off 0x494aa0), both
  expected16 `554889e54157415641554154534881ec`
  (push rbp; mov rbp,rsp; 5 pushes; sub rsp — canonical prologues).

## GUA predicate replay — gc-fix landed and is WIRED

worker-predicate-gc-fix delivered DURING this build (bundle:
`vendored-fixed/src/townfall_layout_probes.cpp` sha256
`bae7cd9a669d20be37547136f309ea8e8ae91b3b569e6b028249dc3b15ede649`, hpp
`270671d2a6da0a9012ca2153b29eff4d37e6af56652fd48eabea86e421934c52`, diff +
13/13 host suite + live-replay evidence in their directory). The build
hook detected it and compiled the CORRECTED predicate into igf5
(`IG_PRED_GC_FIX_PRESENT=1`): seq 6 `gua_pred_replay` will run the
corrected GC-order invariant (`ObjFirstGCIndex == ObjLastNonGCIndex + 1`
settled-state) under cfgH (96-byte header span) and cfgL (current readable
spans), map-gated exactly like the igf4 lineage. If it had not delivered,
seq 6 would have been a status=tbd record with GUA memory untouched.

## Host checks

- `tests/host_checks.py` → HOST_CHECKS_PASSED: page-range math (tick page
  0x33ac000, sco page 0x890000, crossing case, rejection cases),
  diff-mask semantics, and the REAL byte-identical lineage record writer
  emitting anchor-shaped records — every JSONL line json.loads-validated,
  refusal/no-read coupling asserted.
- Artifact checks: LOCAL_PACKAGE_CHECKS_PASSED (make_fself segment
  equality, SDK 0x11000000 stamp, explicit DT_INIT/DT_FINI, 1/1 init/fini
  slots, 7 encoded dynsym imports, no friendly forbidden symbol anywhere,
  2 raw `syscall` instruction sites present).

Native execution: NONE. In-game results: NONE. No promotion claimed.
