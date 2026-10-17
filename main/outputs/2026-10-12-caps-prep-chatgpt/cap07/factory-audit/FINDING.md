# UCAS factory receiver resolved — Mac-only, 2026-10-12

The factory used by the saved `.ucas` setup has a concrete initialization path: **allocation+8 → vtable0x7ebcb68 → slot+0x18 target0x1c6a0**. Its helper opens the path through a **separate static receiver at0x9308808**, initialized with vtable0x7ebc6a0. This entry path does not retrieve the current platform-file chain. A swap confined to the manager's current top/Pak vtable therefore cannot be assumed to observe these UCAS size/open calls.

This narrows the earlier unresolved factory route; it does not admit shipping signatures or prove a console execution. The saved image SHA256 is `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (194241424 bytes). All VAs below are image-relative. See `receipt.json` for actual PT_LOAD mappings and per-window hashes, `factory-vtable.json` for actual PT_DYNAMIC RELA resolution, and `verification.json` for exact instruction/one-based window-line associations.

| Connection | Exact static evidence |
|---|---|
| Global publication | Initializer0x1d9d0 requests0x310 bytes; allocation saved inRBX at0x1d9eb. At0x1d9ee, R15=allocation+8. LEA0x1da2d supplies vtable0x7ebcb68 inRCX;0x1db5d stores it at[RBX+8].0x1db74 publishesR15 to global0x930c530. It also installs a distinct primary vptr0x7ebcab0 atallocation+0; the global points to the secondary interface, so the address adjustment matters. |
| Accessor and call-site connection | Accessor0x1dc30 loadsRAX fromglobal0x930c530 at0x1dc34. Previous `read-route-audit/window-1754560.txt` establishes `.ucas` filename and call0x1754dbd→0x1dc30, receiverRSI=RAX, vptrR9=[RAX], call[R9+0x18] at0x1754df0. Source hashes of those prior receipts are recorded here. |
| Concrete target | Relative relocation at0x7ebcb80 (address point0x7ebcb68+0x18) resolves to0x1c6a0.0x1c6a6 subtracts8 fromRSI, recovering the complete allocation;0x1c6ad calls0x1c470. Original filenameRDX and optionsRCX remain inputs; hidden resultRDI is retained. |
| Separate static receiver | Helper0x1c470 preserves filename inR12. When guard0x9308810 is unset,0x1c654 obtains address0x9308808 inR14 and0x1c65e calls0x5b80 withRDI=R14. Constructor0x5b80 retainsRDI inRBX;0x5bac loads vptr0x7ebc6a0, and0x5bb6 stores it to[RBX]. The full bounded constructor ends at0x5da9. |
| Size/open dispatch |0x1c4cf reads vptr fromstatic0x9308808;0x1c4d6 setsRDI=&static receiver andRSI=filename, then0x1c4e0 callsreceiver+0x88. Negative size is rejected.0x1c518 reloads its vptr;RDI=&static receiver, RSI=filename, EDX=0;0x1c52b callsreceiver+0x100. ReturnedRAX is stored as resource[0] at0x1c539; size is resource+8. Resource pointer ultimately goes into aggregate result+0x108 at0x1c57a. |
| Open target chain | Static vtable+0x88→0x7810. Static+0x100→0x25450, whose0x25457 calls the same receiver+0xf8. Static+0xf8→0x8970. This is not the FPak+0xf8 target0x1770bb0; equal slot displacements cannot substitute for receiver provenance. |
| Conditional handle construction |0x8970 normalizes the filename through0x71c0 and invokes0x85a0 withlocal output initialized-1. If output is nonnegative, it allocates0x30 and installs vptr0x7ebcfd8 at0x89fa, storesreturned integer at+8, size at+0x10, clears+0x18/+0x20/+0x28. A separate branch uses a pool interface atglobal0x9308818; that branch's implementation remains unresolved. |

A bounded follow-on candidate reaches byte-transfer-shaped behavior: the directly constructed handle vtable+0x28 relocation resolves to0x2a180. This target takes destinationRSI and lengthRDX, calls0x6ecca60 at0x2a282 with integerhandle[this+8], destination buffer, length, and currentoffset[this+0x18]; on success it advances the offset bylength at0x2a28f. It then calls0x6eccac0 and0x750 and conditionally copies a staging buffer to the original destination using0x7471e50 at0x2a2cf. **The UCAS consumer's actual call into this handle slot was not traced**, so this is handle-method behavior, not proof that a specific game read took this branch. No platform API/import name or sync/async class is assigned.

The practical diagnostic route choice is now concrete: UCAS factory entry0x1c6a0/0x1c470 and its separate receiver need their own reviewed coverage. Existing current-top/Pak slots alone are insufficient evidence of coverage. Do not patch these values on this receipt alone: one preserved image is only one source, no live vptr or execution has been observed, and exact API names remain unconfirmed.

Remaining bounded blockers are the actual UCAS resource consumer, the alternate pooled-handle construction, and the final byte-transfer/import identities. Raw whole-text RIP candidates in `global-references.json` are discovery aids only, not a claim of exhaustive writes; indirect aliases are possible. The actual initializer write was verified from continuous entry decoding.

Replay from repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/outputs/2026-10-12-caps-prep-chatgpt/cap07/factory-audit/tmp" python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/factory-audit/verify_evidence.py
```

Fresh replay: **10 window hashes/decodes,15 critical instructions,5 relative relocations,3 vptr LEAs PASS**. No original source/image writes, builds, SDK access, console actions, or runtime patches. All new files are confined to this folder.
