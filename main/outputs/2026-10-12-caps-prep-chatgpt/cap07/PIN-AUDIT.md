# Captured-build pin audit — Mac-only

Source executable SHA256: `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, 194241424 bytes. Read via mmap; RELA location/size obtained from PT_DYNAMIC, not section headers or a hardcoded file offset. Receipt: evidence/vtable-audit.json.

## Confirmed discrepancy

PAK-PIN-PASS.md §6 says copy 0x258 bytes from 0x7fee8c0 and preserve a 16-byte header. pak-pin-pass-pins.json also says “74 slots (0x248 bytes)”. These statements disagree arithmetically and with the captured data:

| Item | Fresh saved-image result |
|---|---|
| Constructor LEA at 0x178aa1c | targets 0x7fee8c0 |
| Destructor LEA at 0x1765308 | independently targets 0x7fee8c0 (destination register differs) |
| Current table's preceding header | 0x7fee8b0 / 0x7fee8b8; both zero, no relocation |
| Function entries | 72 R_X86_64_RELATIVE entries from 0x7fee8c0 through 0x7feeaf8 |
| Next two zero words | 0x7feeb00 / 0x7feeb08, preceding next address point 0x7feeb10 |
| Header-inclusive candidate copy | [0x7fee8b0, 0x7feeb00), length 0x250; publish copied storage + 0x10 |

Nine named slot targets from the old pin inventory match the fresh relocation census. Copying from the address point misses the preceding header; the old length reaches the next table's function region. Treat the corrected extent as a candidate for this exact captured image, with fresh live readable-range and ownership validation still required. No runtime copy was attempted.

## Evidence still required before shipping

| Value or behavior | Existing evidence | Remaining gate |
|---|---|---|
| Table address point / extent | Constructor + destructor + contiguous relocation census in one image | Independent source record, authenticated-image binding and live vptr/range check |
| FileExists, FileSize, OpenRead, IterateDirectory offsets | Old call-site/implementation observations; nine targets rechecked here | Per-value source identities and exact ABI including all arguments/returns |
| Previously labeled async slots +0x108/+0x110 | Fresh bounded binary audit: +0x108 is write-open-shaped, +0x110 directory-predicate-shaped; unrelated IoStore receiver caused displacement confusion | See abi-audit/FINDING.md and PUBLIC-API-CROSSCHECK.md; identify actual read interface, exact version ABI and receiver before any thunk |
| Mount helper | Later PAK-PIN-PASS §3 corrects old design's arguments | Independently confirm args struct, padding/flag, outputs, lifetime and wrapper ABI; never use old priority-in-RDX claim |
| Platform-file chain | Static accessor/lower-link recipe | Validated object census, bounded cycle-safe walk, lifetime and game-thread ownership |
| IoStore late-add | Registration tail exists in the old static pass | Package-store update/priority/timing semantics remain unproved |

Four differently wrapped snapshots of the same code are not four independent semantic sources. Constructor/destructor corroboration is useful but is not by itself a second independently maintained source artifact. No existing confidence number admits a value. Missing evidence means one refusal marker and the feature disabled.

These corrections are proposals in our folder. Existing JSON, design documents, candidate source and harvest tools were not edited.
