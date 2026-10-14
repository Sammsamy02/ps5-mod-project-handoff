# Saved-image read-route findings — Mac-only, 2026-10-12

The `.utoc` setup reaches the current platform-file receiver through **+0xf0 → +0xf8** on the observed factory-backed mount branch. The associated `.ucas` setup dispatches a **different global factory interface at +0x18**. Neither route supports retaining the old +0x108/+0x110 async-read interpretation. Separately, partition discovery uses **+0x130**, so observing only the proposed +0x1c0 iteration slot would miss this call path.

All addresses are image-relative virtual addresses in the preserved decrypted executable, SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (194241424 bytes). `receipt.json` records ELF PT_LOAD mapping, bounded window byte hashes, and Capstone version. `verification.json` freshly verifies eleven critical instructions, four relative relocations from actual PT_DYNAMIC DT_RELA metadata, and three UTF16 strings. These are static facts about one saved image, not console observations or independently confirmed function names. Public API shape matching belongs in a separate source cross-check; no slot is admitted for shipping here.

## Concrete receiver and path flow

| Stage | Saved-image evidence | Consequence |
|---|---|---|
| Partition visitor provenance | In fn `0x16fc450`, calls `0xb8ed0`/`0xb8a70` obtain global holder/current receiver. At `0x16fc5df` the visitor vptr is `0x7fe7288`; `0x16fc5f4` writes that receiver to visitor+0x10. Call `0x16fc6e4` passes visitor in RDX and invokes current receiver+0x130. | This visitor is connected to the actual enumeration receiver, rather than attributed by equal displacement. |
| Partition callback | Relative relocation at `0x7fe72a0` resolves visitor+0x18 to `0x171b9d0`. It matches the suffix `ucas` at `0x78d11c4`. At `0x171ba68` RDI comes from visitor+0x10, RSI is the path, and `0x171ba72` invokes that receiver+0x88. Negative size is rejected. | File-size/catalog discovery; this alone does not open or read partition data. The +0x130 call is a concrete coverage gap for a +0x1c0-only observation plan. |
| Mount backend provenance | In Initialize fn `0x1769ee0`, factory-present branch constructs a holder, stores vptr `0x7fee680` at holder+0x10 (`0x176a29d`), then stores holder+0x10 in original this+0xc0 (`0x176a335`). Mount fn `0x176ec30` restores original this, loads [this+0xc0] into RSI at `0x176f3a5`, loads its vptr into R10, and calls +0x70 at `0x176f3e9`. | +0xc0 is an interface pointer on this branch, not merely an enable flag. An alternate construction branch through `0x1764610` remains unresolved. |
| Backend target | Actual relative relocation `0x7fee6f0 → 0x1756540`; target adjusts RSI by0x18 then calls `0x1755ab0` at `0x175654d`, which calls `0x1754560` at `0x1755b44`. | Bounded connection from backend dispatch to filename handling. |
| `.utoc` construction | Helper `0x1754560` appends UTF16 `.utoc` (`0x7974c24`, referenced at `0x1754673`), then passes the resulting path to `0x139d80` at `0x17547b4`. That calls `0x139890` at `0x139dc3`. | Genuine path-associated route, not an arbitrary same-slot xref. |
| `.utoc` interface entry | `0x139890` preserves input filename in R14, calls `0xb8ed0` then `0xb8a70`; the latter returns global holder[0] or initializes it. At `0x1398f1`: RDI=&aggregate result, RSI=current receiver, RDX=filename, ECX=0, call [receiver-vptr+0xf0]. | An aggregate-result interface operation associated with opening/preparing the utoc resource. Exact API name and sync/async classification remain source-unconfirmed. |
| `.utoc` Pak adaptation | FPak address point `0x7fee8c0` has relative relocation +0xf0→`0x84cb0`. On flags0, it calls same receiver+0xf8 at `0x84ce2` with filename in RSI and bool0 in EDX, wrapping returned RAX pointer/error. Table+0xf8→`0x1770bb0`; its conditional fallback loads [this+8] at `0x1770c95` and calls inner+0xf8 at `0x1770ca3`, returning pointer/zero. | Plausible mapped-resource API shape requiring primary-source confirmation. Current receiver actually being Pak is conditional; no live chain identity is claimed. Other branches can service a Pak-contained resource. |
| `.ucas` separate interface | Same helper appends `.ucas` (`0x78d1148`, referenced at `0x1754d51`), calls `0x1dc30` at `0x1754dbd`. That accessor returns global[0x930c530]. At `0x1754df0`: RSI=factory, RDX=ucas path, RDI=&aggregate result, RCX=&options, R8=&output, call [factory-vptr+0x18]. | This call bypasses the demonstrated current-platform-file entry seam. The factory implementation may internally re-enter platform-file APIs; that is not established. Factory identity and eventual byte-read implementation remain undecided. |

## Constructor correction and practical route decision

`0x177fc90` through `0x177fe44` contains no calls: it transfers fields from RSI to RDI, zeroes transferred source fields, and copies trailing byte+0xa8. It is move-transfer-shaped. Treating its historical constructor label as evidence of opening a utoc is unsound; the opening/preparation path occurs earlier as traced above.

For a later reviewed diagnostic change, cover the actual partition +0x130 entry and the path-associated utoc +0xf0/+0xf8 chain. Treat `.ucas` factory+0x18 as a separate unresolved route rather than assuming existing platform-file hooks see it. This is a route-selection finding only; no patch values, exact signatures, synchronous Read call, async request implementation, or shipping ABI are proven. Next exact blocker is the concrete vptr/target installed at global0x930c530, plus the alternate backend construction from0x1764610 and the post-open resource operation tables. No broad scan or console test was performed.

## Replay

From repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/outputs/2026-10-12-caps-prep-chatgpt/cap07/read-route-audit/tmp" python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/read-route-audit/verify_evidence.py
```

Inspect `verification.json` for exact instruction/window/one-based line associations. `capture_windows.py` recreates bounded disassembly windows; its optional xref output is candidate search only. Original executable and source were read-only; all writes were confined to this folder. Historical pin labels are context only and were not accepted as fresh semantic proof.
