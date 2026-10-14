# Queue selector setup — fresh Mac-only evidence

The constructor's zero at complete factory `F+0x238` is **not a permanent default**. Setup routine `0x1c710` overwrites this byte with `signed_i32[0x92dc07c] > 0`. The saved configuration word is `1`; nearby image registration associates its address with `s.IoDispatcherSortRequestsByOffset` and help text describing sorting outstanding requests by offset rather than sequence. This identifies the selector's configuration and changes which queue branch deserves further static coverage. It does not establish a launch's value.

Object distinction matters: `F` is the heap factory published through its secondary interface `F+8` at global `0x930c530`. Static receiver `0x9308808` is the separate file receiver used by the factory's size/open helper. Nothing here locates `F+0x238` in that static file receiver, or makes this flag select the direct versus pooled file-handle backend.

| Edge | Exact saved-image evidence |
| --- | --- |
| Correct factory setup method | Constructor `0x1d9d0` installs primary vptr `0x7ebcab0` at `[F]`. Actual PT_DYNAMIC relative relocation at primary slot `+0x78` (`0x7ebcb28`, raw relocation file offset `0x9330a80`) targets `0x1c710`. This setup method keeps its supplied receiver RSI in R12 at `0x1c72f`; RDI is separately retained as the aggregate result. Invocation during a live run remains unobserved. |
| Later writer and signed predicate | `0x1c754` loads EBX from `0x92dc07c`. The allocation branch saves EBX at `0x1c7c8`, preserves the receiver through RBX/local stack, and restores receiver R12 at `0x1c871` and EBX at `0x1c879`. Both paths reach `test ebx,ebx` at `0x1c897`, then `setg byte [r12+0x238]` at `0x1c8a1`. Thus positive produces 1; zero or negative produces 0 after this setup. |
| Configuration association | `0x3e826` supplies UTF16 name address `0x78781e0` in RSI, `0x3e82d` supplies configuration address `0x92dc07c` in RDX, and `0x3e834` supplies help text address `0x79c0910` in RCX to the same virtual call at `0x3e83e`. This is an association in image code, not an inferred API signature. Exact strings and bytes are preserved in `verification.json`. |
| Consumer predicate | Worker `0x1cb60` retains supplied factory in RBX, forms queue `F+0x130`, and compares `[F+0x238]` with 1 at `0x1cc1b`. `jne 0x1cc3d` reaches the pointer-array branch previously closed in `handoff-audit`; equality reaches the alternative beginning `0x1cc24`. |

Concrete next action: audit the flag==1 queue path before treating the prior flag!=1 enqueue/dequeue closure as representative of the saved default after setup. Observe the actual factory flag/configuration and setup chronology in an independently authorized runtime test before asserting live branch selection. Do not label this queue-order setting as a direct/pooled I/O backend switch.

Bounds and remaining gaps: the discovery search covers only code VA `[0,0x40000)` and the first `0x40000` bytes of the actual DT_RELA table. Raw-reference candidates are discovery aids; claimed instructions are decoded in saved bounded windows. No exhaustive later-write/alias search was performed. Mutable configuration can differ from file bytes; setup invocation/order and any later reconfiguration remain unproved. One preserved image is one source and admits no shipping values/signatures.

Replay from repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/outputs/2026-10-12-caps-prep-chatgpt/cap07/branch-selector-audit/tmp" python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/branch-selector-audit/verify_evidence.py
```

Fresh replay PASS: 5 bounded window hashes/decodes, 16 critical instructions, 1 actual dynamic relocation, 2 UTF16 strings and 5 before/after unchanged instruction/context source hashes. Image SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`. `receipt.json` records PT_LOAD mappings and window hashes; `verification.json` records critical bytes and one-based window lines. All writes stay in this folder; no live image edits, console/network, builds/compiler/install or external platform research.
