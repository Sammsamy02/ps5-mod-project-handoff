# RECEIPT — worker-anchor-validation — modbridge_ingame_facts.prx (igf4)

- Worker: worker-anchor-validation (ZCode team, PS5 mod-manager, THE CRITICAL
  PATH TASK)
- Date: 2026-10-04
- Build ID: `igf4-4891869337b4` (prefix `igf4-` per the anchor-validation
  amendment; FROZEN delivery)
- Variant: igf4-anchor-validation
- Module name / log path: UNCHANGED from the v2/v3 lineage —
  `modbridge_ingame_facts` / `/data/modbridge-ingame-facts.jsonl`
  (same-file replacement of v3 `igf3-de0c0cb644d3`, no ini change)
- Entry marker: seq=1 `exp="entry"` written and flushed FIRST, before any
  scan (module_entry.c lineage contract); startup record seq=2 separates
  init-array from later failure; focused checks are seq 3..10.
- Native execution: NONE. In-game results: NONE. Local packaging + host
  checks only.

## Artifact

| Artifact | Bytes | SHA-256 |
|---|---|---|
| `modbridge_ingame_facts.prx` (fself, deliverable == `determinism/run1/` == `run2/`) | 26077 | `4aa624cc4cc045c6c137b07b2283f458ad2e41a576273444d0fc75e1e9762756` |
| inner ELF (`bare-sdk/modbridge_ingame_facts.prx`, SDK 0x11000000) | 85536 | `bd591ed3159ff1c571b85ebba0118e5f65912b5780297ccafd41ebe640b51914` |
| `combined-ingame.o` | — | `30c645c20f9061d6db065561915716e338791af6225bd24937d90fb9f591cdcc` |
| `import-contract.json` | — | `42b2fdc3ea85bd62ffbc069629f5997b8c0ec922382f46560ca05bee1fbedf96` |

Frozen lineage provenance: `.superpowers/native-dependency-spike/
native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx`
sha256 `3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654`
re-verified this session before the build (build-receipt `hash_verified:
true`).

## Expected-bytes extraction evidence (the anchor ground truth)

- Source image: `.superpowers/native-observations/
  townfall-static-executable-2026-10-02/decrypted-eboot.bin`,
  sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`
  — re-hashed this session and EQUAL to the survey receipt's
  `capture_sha256` (run-release-1/receipt.json), so the retained bytes are
  the surveyed bytes.
- Correspondence re-derived from the image's own ELF header this session:
  LOAD0 `p_offset=0x4000, p_vaddr=0x0, filesz=0x7479b1c` (flags=R+X) →
  file_offset = profile_vaddr + 0x4000; L7 RW `vaddr 0x913c000, memsz
  0x4f7418` brackets the GUA profile vaddr 0x93ba110; base+vaddr checks:
  0x400000+0x2fac8c0=0x33ac8c0 (Tick), +0x490aa0=0x890aa0 (SCO),
  +0x93ba110=0x97ba110 (GUA) — exactly the checkpoint
  zcode_v3_MAPPING_BREAKTHROUGH facts.
- Extracted evidence file: `expected_bytes_tick_sco.txt` (written by this
  worker; build-time LOUD re-verification of image sha + offsets):
  - Tick: file offset 0x2fb08c0, expected16
    `554889e54157415641554154534881ec`
  - SCO: file offset 0x494aa0, expected16
    `554889e54157415641554154534881ec`
  - Both decode as the canonical x86-64 frame setup (push rbp; mov rbp,rsp;
    push r15/r14/r13/r12/rbx; sub rsp,imm32) — exactly the shape the
    vendored code-shape classifier labels CODE_I_PROLOGUE_LIKE (host-verified).
- Independent cross-validation: the concurrent worker-expected-bytes
  extraction (EXPECTED-BYTES.json, same source image, same sha) carries 32
  bytes per anchor whose FIRST 16 bytes are byte-identical to mine; bytes
  17..32 continue the bodies (Tick `sub rsp,0x88`; SCO `sub rsp,0x258`) —
  two workers, same image, same offsets, same bytes.
- The expected hex is INJECTED into the artifact at build time
  (generated/ingame_config.h `IG_TICK_EXPECTED_HEX` / `IG_SCO_EXPECTED_HEX`),
  so the PRX provably derives from the retained-image evidence; host check
  re-parses the evidence file and matches the generated header
  (`expected_bytes.match: true`).

## Import-identity proof table (7 rows, byte-identical to the frozen contract)

Source contract: `import-contract.json` (derived Packager); every row is
COPIED VERBATIM from `.superpowers/native-dependency-spike/
native-full-runtime-aligned-entry-1/import-contract.json` (the console-proven
frozen 191-import contract). Proof method: `json.dumps(row, sort_keys=True)`
string equality against the frozen row, asserted pre-write AND re-verified on
read-back by `build_anchor_probe.py` (LOUD failure on any divergence), and
re-verified independently by `check_anchor_artifact.py` post-build.

| # | Name | Library | Module (ver) | Soname | Row proof |
|---|------|---------|--------------|--------|-----------|
| 1 | `__error` | libkernel v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 2 | `sceKernelOpen` | libkernel v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 3 | `sceKernelWrite` | libkernel v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 4 | `sceKernelClose` | libkernel v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 5 | `sysctl` | libkernel v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 6 | `getpid` | libScePosix v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 7 | `clock_gettime` | libScePosix v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |

Packaged-dynsym evidence (`check_anchor_artifact.py`,
LOCAL_PACKAGE_CHECKS_PASSED): exactly 7 GLOBAL dynsym imports, 0 weak, every
name NID-encoded: `1G3lF1Gg1k8#A#B, 4wSze92BhLI#A#B, 9BcDykPmo1I#A#B,
DFmMT80xcNI#A#B, HoLVWNanBBc#B#B, UK2Tl2DWUns#A#B, lLMT9vJAck0#B#B`
(count + encoding gate; no friendly forbidden name anywhere). Dropped
identities (v2 isolation, unchanged): sceKernelGetModuleList/-Info,
GetExecutableModuleHandle, dlopen/dlclose/dlsym/__dlopen/__dlclose/__dlsym.
Forbidden-symbol gate over the ENTIRE combined object: PASSED.

## Cross-worker predicate proof (independent PS5 compile)

`src/townfall_layout_probes.{hpp,cpp}` are byte-identical copies of
worker-5-layout-validator/src (the batch1 GUA predicate). After the PS5
compile, `build_anchor_probe.py` hashed the built object against the
INDEPENDENT worker-predicate-compile bundle:

- ours: `2acd32177659ddce5d74190d74fac109cfc909fcdc669a9e6bb80ff5608b9ca9`
- worker-predicate-compile/ps5/townfall_layout_probes.o: same
- **match: true** — source identity + compile determinism proven across two
  independent workers with the same frozen CPPFLAGS/toolchain.

## Local checks performed (all green)

1. `tests/run_host_checks.py` — HOST-CHECKS-PASSED:
   - WRITER: the REAL vendored `src/record_sink.c` (v3 lineage,
     byte-unchanged) emits 3 records; 2 parse via `json.loads` with exact
     field shapes (negative i64, fixed-16-nibble 36-bit hex `0x0000000500000090`,
     escaping incl. \u0001, raw-value arrays, post-array separator); the
     deliberately truncated record carries the textual `"__trunc":1` marker
     and is exempt from the parse guarantee per the v3 contract.
   - PREDICATE (13 cases): synthetic layout-A image → Validated
     (FUObjectArray_Outer, num=200, chunk0 exact); layout-B image → Validated
     (TUObjectArray_Direct, A refuted); header-span-only feed with pointer
     target outside the 96 bytes → Refuted GOA_R_PTR_OUTSIDE_READABLE (the
     documented expected cfgH shape for a heap-backed GUA); code-shape
     classifier on the retained-image expected prologue → Inconclusive-class
     CODE_I_PROLOGUE_LIKE.
   - VMMAP WALK (13 cases): the REAL vendored v3 mode-1 walker parses the
     checkpoint's exact spans (exec [0x400000,0x787c000) prot 5, RW
     [0x970c000,0x9a34000) prot 6) + a 36-bit span, trust gate passes at 9
     sane, insane structsize=7 stops with reason 1, and the coverage
     inclusion formula used by the safety gate covers 0x33ac8c0/0x97ba110
     and rejects 0x9a34000.
   - EXPECTED BYTES: generated config hex == evidence file hex.
2. `build_anchor_probe.py` — packaging pipeline reused EXACTLY from the v3
   frozen recipe (import_references-first combine, SharpProspero Packager,
   SDK 0x11000000 stamp via frozen finalize_sdk.py, pinned make_fself).
   Build-time LOUD gates that all passed: frozen lineage hash, evidence-file
   sha + offset checks, forbidden-symbol gate, contract re-read equality,
   cross-worker predicate object hash.
3. `check_anchor_artifact.py` — LOCAL_PACKAGE_CHECKS_PASSED: fself header +
   segment bytes equal to inner ELF, sha-of-bare pinned in fself info,
   init/fini slots == 1 each, explicit DT_INIT/DT_FINI, arrays not
   loader-owned, SDK stamp 0x11000000, 7-encoded-import gate.
4. Determinism double-build: `determinism/run1/` vs `run2/` — final fself,
   inner ELF, combined object, and contract all BYTE-IDENTICAL (build id
   identical by construction: the seed pins the source set).

## Not claimed

No console staging, no launch, no in-game records, no layout promotion.
Everything above is local host evidence. Staging/launch =
STAGE-PLAN-ANCHOR.md, user-coordinated. Candidate != validated per the
standing honesty rules; the in-game records decide.
