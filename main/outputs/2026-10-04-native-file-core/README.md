# Captured native Unreal core resolver result

A throwaway local caller exercised the existing public serialized-file endpoints against the original Townfall executable. Both debug and optimized runs produced the same results. The original input remained unchanged; all 31 selected Rust source hashes matched after execution. No analyzed game code was executed, no runtime address was supplied, and no console contact occurred.

| Endpoint | Status | Linked image label |
| --- | --- | --- |
| Engine version | 0 | 5.6 |
| GUObjectArray original | 0 | `0x93ba110` |
| GUObjectArray CFG candidate | 0 | `0x93ba110` |
| FName constructor original | 3 | unavailable |
| FName constructor call-site candidate | 0 | `0x23d590` |
| FName ToString | 3 | unavailable |
| GMalloc original | 3 | unavailable |
| GMalloc DeleteFile candidate | 0 | `0x9350950` |
| StaticConstructObject | 0 | `0x490aa0` |
| GameEngine Tick | 0 | `0x2fac8c0` |

Status 0 is a static resolver result; 3 is no match. Candidate endpoints retain their narrower evidence and explicit selection policy. These labels are outputs of the shared algorithms, never recipes or inputs to shared code.

This confirms that every family in the selected required mask `0x5d` has a serialized-file result or candidate. It does not establish all actual mapped collector results, singleton contents, target layouts, callable ABI, live game-thread delivery or Item Spawner. The existing engine-owned Kismet `Conv_NameToString` fallback covers the optional ToString bootstrap once its real object and ProcessEvent prerequisites are established; this survey did not execute that fallback.

The optimized local run spent roughly 0.95 seconds in the original GUObjectArray endpoint and 0.31 seconds in Tick. These are host timings, not console performance guarantees. The debug run is preserved as development evidence; it is not the target optimization profile.

Original capture SHA-256: `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`.

Reproduction sources and receipts: `.superpowers/native-dependency-spike/ue4ss-native-file-core-survey-1/`. The caller takes the captured file path as an argument and contains no title IDs, per-game addresses, paths or patches.

The separate mapped-source connection work must account for Sony executable types, execute-only ELF flags, loader-managed TLS, RELRO and file-only loader metadata. The captured Townfall ELF has these shapes; a GNU-ELF-only fixture does not cover its admission rules. A valid retained read lease and checked file-to-mapping relationship remain required.
