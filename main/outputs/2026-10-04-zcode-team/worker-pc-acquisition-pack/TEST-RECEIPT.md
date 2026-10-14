# TEST-RECEIPT.md — verify_intake.py test receipt (worker-pc-acquisition-pack, 2026-10-05)

All runs are local, read-only on the staged inputs, against the final version of
`verify_intake.py` (lief 1.0.0, python 3.9.6, macOS arm64). Scratch fixtures lived under
`/tmp/` and were deleted after the runs; the dwmapi test also emitted the machine receipt
retained at `test-dwmapi-intake-receipt.json` in this directory.

## Scenario matrix (7 scenarios)

| # | staging input | expected behavior | observed | exit |
|---|---------------|-------------------|----------|------|
| T1 | `/tmp/pc-intake-empty` (empty dir) | honest NOT-READY, "nothing staged yet" | `STATUS: NOT-READY — nothing staged yet (this is the honest empty state).` | **1** |
| T2 | `/tmp/pc-intake-does-not-exist` (missing dir) | exit 2, dir-missing branch | `STATUS: NOT-READY — staging dir does not exist` | **2** |
| T3 | `inputs/pc-townfall/` (real staging dir, README.txt placeholder only) | README excluded from evidence; honest NOT-READY | `README.txt — placeholder (not evidence)` … `STATUS: NOT-READY — nothing staged yet` | **1** |
| T4 | copy of the REAL PE fixture `dwmapi.dll` (279,552 B, UE4SS proxy) | lief parses PE32+/AMD64; sha computed; honest FOREIGN-PE classification | `PE32+ (Win64), machine=AMD64, sections=6`; `sha256=4e3e5324e68e033e…`; `VERDICT: FOREIGN-PE — NOT TOWNFALL`; blockers: `no Townfall title marker in executable` | **1** |
| T5 | SYNTHETIC logic probe: MZ magic + "Townfall" + `++UE5+Release-5.6-CL-0` + UTF-16 `FMallocBinned3` (labeled synthetic; branch coverage only, never evidence) | SAME-UPDATE-PLAUSIBLE branch + allocator-literal scan | `Title marker "Townfall": FOUND`; `Allocator flavor literals: FMallocBinned3 x1`; `Engine release string(s): 5.6`; `VERDICT: SAME-UPDATE-PLAUSIBLE — TOWNFALL, PC UE 5.6 == PS5 UE 5.6`; lief honestly refused the fake image (`lief.PE.parse returned None`) and that is printed as a note | **1** |
| T6 | SYNTHETIC logic probe: same but `Release-5.3` | VERSION-DIVERGENT branch (classes never merged) | `Engine release string(s): 5.3`; `VERDICT: VERSION-DIVERGENT — TOWNFALL PC UE 5.3 vs PS5 UE 5.6`; receipt-field blockers listed (`store`, `remote_path` missing) | **1** |
| T7 | REAL PS5 pinned image via hard link (`decrypted-eboot.bin`, 194,241,424 B) — read-only | PS5-SIDE-FORMAT refusal | `magic=ELF`, `sha256=3b6cfad47dd17fea…`; `VERDICT: PS5-SIDE-FORMAT / NOT-PC-INTAKE`; `ELF input (e_type 65040, ELF64 X86_64)` | **1** |

## T4 fixture provenance and byte-identity proof

Source (retained, cited in INVENTORY.md §3):
`.superpowers/native-dependency-spike/windows-image-original-inputs-1/final-1/dwmapi.dll`

Independent `shasum -a 256` of BOTH source and the scratch copy returned the identical
digest, matching the recorded value:

```
4e3e5324e68e033e372193d2420367614eb13016cb98b22b8e85e9844f5c0c9a  /tmp/pc-intake-dwmapi/dwmapi.dll
4e3e5324e68e033e372193d2420367614eb13016cb98b22b8e85e9844f5c0c9a  .superpowers/.../final-1/dwmapi.dll
```

The verifier's own hash of the copy (from `test-dwmapi-intake-receipt.json`) is the same
digest — the sha256 path is proven end-to-end. lief section table for the fixture:
`.text .rdata .data .pdata .rsrc .reloc`, entrypoint 644254 (0x9D4FE), machine AMD64,
PE32+ — the PE-parse path is proven on a REAL binary, not a synthetic.

## T7 honesty check

The verifier was pointed at the PS5 pinned image (as a hard link, zero bytes written; the
image itself untouched and re-hashed to the pinned digest `3b6cfad47dd17fea…` in-run). It
refused it as PC intake on format grounds alone — no PS5-side material can masquerade as
PC Townfall input through this gate.

## Non-claims

- T5/T6 are SYNTHETIC byte strings built to exercise classification branches. They prove
  the branch logic, not any game fact; no real PC Townfall executable exists (INVENTORY.md).
- No scenario produced READY (exit 0) because READY requires a genuine Townfall PE + a
  provenance receipt — correctly unreachable until the user provides material.
- All PS5-side reference values restated from retained, cited derivations; not re-verified
  beyond the T7 sha confirmation.
