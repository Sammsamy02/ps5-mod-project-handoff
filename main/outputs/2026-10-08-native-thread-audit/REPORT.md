# Native engine-thread identity audit — local candidate

The authenticated Townfall image contains a real native GameThread ID32 producer and a subsequent readiness write. These are review-pending candidate profile facts; no selected profile/source changed, no game/console/network operation occurred, and no live thread or readiness value was observed. Image SHA256 is `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (194241424 bytes).

## Native proof chain

| Datum | Exact native evidence | Qualification |
|---|---|---|
| Named GameThread ID32, image `9358A98`, width4 | `A26301` LEA points to the global; `A26308` compares its dword with EDI. Equal returns static FString `9402AC8`; its construction at `A26365/A2636F` uses UTF16 `GameThread` at `76D6DEC`. The next branch independently selects `RenderThread` from global `9358A9C`. | High-confidence native naming/width correlation; stripped global symbol name is inferred. |
| Current ID producer | `3A14E8E e80dd5a503` calls `74723A0`; `3A14E93 488d0dfe3b9405` addresses `9358A98`; `3A14E9C 8901` stores EAX dword. A separate caller `BB1D3` calls the same import, retains EAX in R15D, and compares it with the named global at `BB213`. | Actual getter-result dataflow; no arbitrary callback publication participates. The large containing function starts `3A14720`; its complete startup lifecycle was not recovered. |
| Readiness byte, image `9358AA4`, width1, ready value1 | Immediately after ID store, `3A14E9E 488d0dff3b9405` addresses this byte and `3A14EA5 c60101` stores1. Three native consumers test the byte. | Native producer/consumer semantics support an initialization flag; exact global symbol name remains inferred. |
| Getter import | `74723A0 ff25f201cc01` jumps through GOT `9132598`. DT_JMPREL entry90 has relocation `0x5E00000007` and dynsym94 names `EI-5-jlq2dE#5#J`. Native module9 and library57 dynamic records both name `libkernel`; suffix `J`/`5` matches those IDs. | Native import/NID identity is directly byte-backed. SDK symbol export is corroboration, not its implementation. |
| C declaration | Saved OpenOrbis header line573: `int32_t scePthreadGetthreadid(void);`, SHA `d15c49347fabdc332e1ba180c2361e7cfe22733be2d06236262210d435ed98ea`. Local SDK export-name census finds one matching NID: `scePthreadGetthreadid`. | Third-party PS4 public prototype, independently correlated with saved PS5 no-explicit-argument callers and EAX32 result flow. No official PS5 prototype/body claim. The distinct `pthread_getthreadid_np` NID is never silently substituted. |

`THREAD-CONTRACT-WINDOWS.json` contains297 exact instruction windows plus2 UTF16 strings. `THREAD-CONTRACT-REFS.json`, `THREAD-SUPPORT.json`, and `LIBKERNEL-IMPORT-IDENTITY.json` carry instruction/import bytes, ELF storage geometry, source hashes and reproducible scope. Addresses are image-relative; file offsets are explicitly stored in evidence.

## Readiness and lifetime limits

The complete native predicate at `B9090` returns true when the readiness byte is not1. After readiness1, TLS dword `fs:-13C0` equal2 returns true; TLS value1 compares current native ID with a lazily initialized cache `932A548`; other values return false. This is an inferred `IsInGameThread`-like method, not a strict authentication accessor. The isolated policy does not call it or admit a pre-init/TLS shortcut.

The bounded scalar RIP instruction-family census found13 references across the GameThread ID, readiness byte and RenderThread ID. All3 literal GameThread references were inspected: two reads and the producer write. All4 readiness references were inspected: three consumers and the producer write. No reset write occurred in that family. All standard DT_RELA addends were checked for the two addresses:0 exact pointer aliases. Computed addresses, other opcode families, non-relative relocations, indirect stores and hidden lifecycle resets remain unresolved; this is not a complete all-writer proof.

Both globals reside in the zero-fill part of RW PT_LOAD `913C000` (`filesz1CC5F2`, `memsz4F7418`). They have no saved file-backed current value. No live readiness, permanence, ID reuse guarantee, or atomic multi-field snapshot guarantee is inferred from BSS or a single initializer.

## Separate identity representations

The actual SDK target census has10 facts: `pthread_t` is pointer8/align8, `std::thread::id` is8/align8, and `std::this_thread::get_id()` emits a `pthread_self` dependency. The selected `NativeGameThreadIdentity::Identity` caches that opaque pointer identity after its first callback. Its own source explicitly says publication does not prove engine identity. Selected LuaMod `funcptr_marshal_hook` currently publishes a first accepted carrier callback, and the R4 initializer getters use that affinity state. Those sources are pinned in `THREAD-SUPPORT.json`; no thread authenticity is attributed to that publication.

The native engine ID is scalar32. Neither integer truncation of opaque affinity nor numeric equality between the two representations is permitted. The compiler census and typed adapter are compile controls, not engine execution or live ABI observation.

## Isolated strict policy and tests

`policy/candidate/StrictNativeThreadPolicy.hpp` requires two successful stable snapshots, readiness exactly1, stored/current native IDs in `1..INT32_MAX`, native ID equality, no conflicting subsequent initialization, and cancellation checks. Only then does it bind the separate real `std::thread::id`; equal native IDs on another opaque thread refuse. Reader exceptions/failure refuse before binding. Cancellation is permanent for this one-lifetime policy instance. Snapshot readers own image/profile/address authentication and safe failure reporting; this prototype performs no game-address read.

The 24 initial host tests failed against an unimplemented refusal stub, then passed. Three exception tests subsequently failed against the initial candidate and passed after the failure boundary was added. Final27 host tests pass with fatal ASan/UBSan settings, including snapshot failure/zero/error/shared-error values, readiness transitions, changing ID, conflicting init, foreign native/opaque identities, cancellation, retry after refusal, and eight real concurrent threads. No game/native callback was invoked. Actual target compilation succeeds for both the selected SDK identity census and the candidate typed adapter;576 dependency files are pinned. The adapter object imports the exact `scePthreadGetthreadid` symbol and stores its EAX32 result; no `pthread_getthreadid_np` alias occurs.

`python3 outputs/2026-10-08-native-thread-audit/validate_thread_audit.py` validates native byte/data/import windows, source/compiler dependency pins, actual census object values, typed adapter import,27 real host policy tests and6 evidence/contract mutation refusals. The initial target adapter syntax-error capture is preserved; final target compilation exits0.

Before integration, independently review these candidate facts, bind a freshly authenticated image/profile/base and expected import provider, implement a safe read-only snapshot reader, and coordinate policy cancellation with callback/engine-call lifetime. Two equal snapshots cannot detect ABA or prove atomic multi-field linearizability; admission does not itself synchronize a later engine call with teardown. These are explicit unresolved integration constraints. No profile promotion, callback installation, package success or menu/game success is claimed.
