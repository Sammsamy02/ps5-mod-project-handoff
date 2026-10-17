# Resource consumer and pooled handle — Mac-only, 2026-10-12

The queued worker calls the resource's returned handle through **+0x18 then +0x28**, passing offset, destination and length. The pooled fallback is now resolved from **global0x9308818 → vptr0x7ebccf0 → +0x10 target0x58e0 → handle constructor0x5240 → vptr0x7ebc630 → +0x28 target0x5500**. This gives concrete later diagnostic seams for both direct and pooled handles. The imported transfer routine names remain unconfirmed.

Saved executable SHA256: `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`,194241424 bytes. All addresses below are image-relative. This is fresh static evidence, not console execution, SDK-derived identity, or a shipping ABI admission. `receipt.json` provides PT_LOAD mappings and per-window hashes; `verification.json` provides critical bytes and one-based lines in bounded windows. Prior image/source receipts are hashed as read-only dependencies.

## Pointer flow into the consumer

- The previously traced `.ucas` factory operation stores an allocated resource pointer in its aggregate result+0x108 (`factory-audit/window-1c470.txt`,0x1c57a). Resource[0] holds the returned file handle; resource+8 holds size.
- Backend fn0x1756600 takes a container partition-array pointer from container+0x1d0 at0x1756ab9 and indexes0x30-byte records. Record+0x18 is used at0x1756add/0x1756afa. At0x1756bdc it forms the address of record+0x18;0x1756bf4 loads that field, then0x1756c17 stores it into descriptor[0] atstack[rbp-0x1d0]. It is not a second dereference of the resource.
- It obtains the factory through0x1dc30 at0x1756f48.0x1756f69 supplies RSI=&descriptor;RDX=&callback-like aggregate;RDI=factory;0x1756f96 callsfactory+0x28. Actual RELA resolves that slot to0x20760, which adjustsRDI-8 and calls0x20010.
- Fn0x20010 saves incoming descriptorRSI at0x20054. For a newly prepared task returned by0x1f6c0,0x20404 reloads the descriptor;0x20418 loadsdescriptor[0] intoRDX;0x2041f storestask+0x68=RDX.0x2042a storestask+0x78=block offset;0x2043c calculates a bounded remaining length. Queue helpers after0x206bb were not separately decoded.
- Worker0x1cb60 takes the complete factory implementation asRDI (factory+0x38 adapter0x1d0e0 adjusts-8 and calls it). It obtains a queued task from0x1dec0/0x1d200. At0x1cdf3,resource=task+0x68;0x1ce06 loads handle=[resource];0x1ce0c callshandle+0x18 withRSI=task+0x78. If successful,0x1ce13 reloads the same handle,RSI=task+0x60 (destination),RDX=task+0x88 (length), and0x1ce24 callshandle+0x28;AL is tested. On failure it repeats this pair through bounded retry branches in the same worker.

**Limit:** the open helper's record+0x18 publication and this consumer's record+0x18 load have compatible explicit pointer flow. The assignment installing the open helper's completed record array into container+0x1d0, and every intervening queue helper's pointer preservation, were not traced in this pass. Therefore this receipt establishes the concrete resource/task/handle consumer structure and path-associated frontend, but does not claim a fully closed allocation-to-read proof or that a specific game read executed it.

## Pooled fallback identity and behavior

| Connection | Exact evidence |
|---|---|
| Concrete pool object | Continuously decoded startup function0x3dd70 first storesbase vptr0x7ebce28 atglobal0x9308818 at0x3e0f5; thenLEA0x3e12f suppliesfinal vptr0x7ebccf0, stored at0x3e163. This global is object storage, not an object-pointer holder. Prior0x8970 fallback usesRDI=&global0x9308818 and loads its vptr beforecall+0x10 at0x8a88. |
| Pool target | Actual DT_RELA:0x7ebcd00 (pool+0x10)→0x58e0. That target converts filenameRSI and calls0x74720a0 at0x59a7 withRDI=converted filename,ESI=0,EDX=0. Nonnegative returned integer leads toallocation0x68 at0x59c7;constructor0x5240 receivesintegerESI,filenameRDX,ECX=0,R8D=0 at0x59dd. |
| Handle identity | Constructor0x5240 loadsvptr0x7ebc630 at0x525e and stores it to[this] at0x5284;storesinteger atthis+0x20 andflag byte at+0x48. Actual RELA resolveshandle+0x18→0x5770 andhandle+0x28→0x5500. |
| Position operation |0x5770,forflag0,clamps requestedRSI against[this+0x40]-1,storesthis+0x38 at0x579d andreturnsAL=1. Another flag branch calls0x7472050 withinteger[this+0x20],requested offset,EDX=0 andreturns whether result is nonnegative. |
| Transfer operation |0x5500 keeps lengthRDX inR14 and destinationRSI. Underflag1,it calls0x7472030 at0x5559 withinteger[this+0x20],destination,length. Underflag0,it calls0x7472040 at0x5574 withthe same integer/destination/length plusoffset[this+0x38] inRCX;on nonnegative result,it advancesoffset byreturned byte count.0x5582 comparesreturned count torequested length andreturns boolean success. Other code coordinates pool ownership/release. |

The worker's handle+0x28 now has two concretely constructed target candidates: direct handle0x7ebcfd8→0x2a180 from the prior factory audit, or pooled handle0x7ebc630→0x5500 from this audit. Receiver identity still decides which applies; no same-displacement substitution is used. The direct target's low-level routine0x6ecca60 and pooled0x7472030/0x7472040 imports remain unnamed. The pool's exact0x74720a0 open-like routine also remains unnamed. Sync/async API naming is not admitted by shape alone.

## Replay and remaining proof

```sh
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/outputs/2026-10-12-caps-prep-chatgpt/cap07/consumer-audit/tmp" python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/consumer-audit/verify_evidence.py
```

Fresh replay verifies11 boundedwindow hashes/decodes,19 criticalinstructions,4 relative relocations and2 vptr LEAs. `pool-vtables.json` resolves bounded slots from actualPT_DYNAMIC DT_RELA/DT_RELASZ/DT_RELAENT. `scan_pool_global.py` is a raw reference-discovery aid, not exhaustive write/alias proof.

Remaining exact proof gaps: container+0x1d0 array installation, intervening queue handoff, live selected direct/pooled branch, and primary-source import/API identities. These do not require guessed signatures or runtime patches to record. No builds, compiler/install, console/network contact, SDK lookup, or original file writes occurred; all new files are confined tothis folder.
