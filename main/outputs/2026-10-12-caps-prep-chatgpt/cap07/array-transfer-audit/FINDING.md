# Array transfer boundary — fresh Mac-only saved-image evidence

Both sorted-array call sites reach the same **undefined encoded import from libc**. The image establishes the provider and exact identifier, but supplies no plaintext routine name or resolved implementation. Nonzero transfer semantics remain unproved; this audit does not close insertion-before or shifting pop by guessing a familiar libc function.

| Identity edge | Exact saved-image evidence |
| --- | --- |
| Insert transfer | Call `0x1dcad`, bytes `e8de414507`, targets `0x7471e90`. |
| Pop transfer | Call `0x1e9d2`, bytes `e8b9344507`, targets the same `0x7471e90`. |
| Import resolution | PLT RIP-relative indirect jump → GOT destination → matching actual `DT_JMPREL` relocation (type7) → indexed dynamic symbol → name `+P6FRGH4LfA#6#4`. Dynamic library and module records both resolve to `libc`. Symbol section is0 (undefined). Exact GOT address, file offsets, symbol/relocation/provider bytes and dynamic bounds are in `receipt.json`. |

Register facts immediately before the calls, where P is the pointer-array base, n its old count and i the selected insertion/removal index:

| Call site | RDI | RSI | RDX | Related pointer preservation |
| --- | --- | --- | --- | --- |
| Insert `0x1dcad` | P+8*(i+1) | P+8*i | 8*(n-i) | T remains in callee-saved R12; after return `0x1dcb6` explicitly stores T into array[i]. Count is already n+1. |
| Pop `0x1e9d2` | P+8*i | P+8*(i+1) | 8*(n-i-1) | Selected task RAX is saved in callee-saved R15 at `0x1e9c7` and restored at `0x1e9db`; count is reloaded after the call. The branch at `0x1e9be` skips the call when the computed remainder is0. |

These are register/address arithmetic facts, not a declaration that RDI is a destination, RSI a source, RDX a byte count, or that the import obeys a particular C signature. The surrounding array-count and explicit task-pointer store make rightward insertion and leftward removal the intended transfer interpretation; that remains an inference until the import's semantics are independently established.

For the concrete next cases, inserting T before one existing task supplies pointer registers P+8/P and RDX=8; popping index0 from a two-task bucket supplies P/P+8 and RDX=8. To assert the other task remains at the shifted array entry requires proof that this import actually transfers those eight bytes in the intended direction and preserves the unaffected prefix. The caller alone does not prove that contract. For larger shifts, overlap behavior is an additional unresolved requirement.

The unchanged next blocker is precise: an independently permitted mapping of `+P6FRGH4LfA#6#4` to a documented transfer contract, or an actual resolved implementation captured through an independently authorized route. The saved PLT/undefined-symbol metadata provides neither. No identifier dictionary, SDK/platform-extension research, external source or live loader state was consulted. Provider `libc` by itself cannot establish copy/move behavior, overlap handling, return conventions or failure behavior. No runtime or shipping admission follows.

Replay from repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/outputs/2026-10-12-caps-prep-chatgpt/cap07/array-transfer-audit/tmp" python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/array-transfer-audit/audit.py
```

Fresh replay PASS:3 narrow windows,26 instructions,2 independently computed relative call targets,1 PLT/GOT/relocation/symbol/provider chain,5 before/after unchanged context hashes; one full-image SHA pass. SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`. Subsequent replay compares immutable exact-image receipt fields before writing a fresh context-hash record. The only table walk is the bounded DT_JMPREL table and relevant dynamic provider entries; no full-image instruction scan or broader implementation search. All writes stay here; originals/live files remain read-only. No console/network, build/compiler/install, source download or shipping pins.
