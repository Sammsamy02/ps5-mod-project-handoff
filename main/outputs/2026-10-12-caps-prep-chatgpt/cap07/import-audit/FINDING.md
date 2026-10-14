# Saved-image import finding

Fresh Mac-only verification establishes a provider split. Four pooled-route stubs resolve to encoded imports from `libkernel`. Direct-route `0x6ecca60` is a local wrapper: call `0x6ecca7f` targets PLT stub `0x7479870`, importing from `libSceAmpr`.

| Requested routine | PLT stub | Exact encoded import | Provider module |
| --- | --- | --- | --- |
| `0x7472030` | `0x7472030` | `Cg4srZ6TKbU#5#J` | `libkernel` |
| `0x7472040` | `0x7472040` | `+r3rMFwItV4#5#J` | `libkernel` |
| `0x7472050` | `0x7472050` | `oib76F-12fk#5#J` | `libkernel` |
| `0x74720a0` | `0x74720a0` | `1G3lF1Gg1k8#5#J` | `libkernel` |
| `0x6ecca60` | `0x7479870` | `mQ16-QdKv7k#7#5` | `libSceAmpr` |

The provider library and module names agree for each entry. Each chain is RIP-relative PLT jump → GOT destination → matching DT_JMPREL relocation → indexed DT_SYMTAB entry → DT_STRTAB import and provider records. Relocation destinations, rather than stub push values, select symbols. Exact bytes, file offsets, PT_LOAD mappings and dynamic bounds are in `import-receipt.json`. Wrapper call bytes: `e8eccd5a00`.

No plaintext API name is established. The import names contain encoded function/library/module identifiers. The DT_HASH-bounded symbol search finds no defined dynamic symbol covering the local wrapper. Section-header geometry is not file-backed; program headers and dynamic metadata supply the bounds. Further naming requires an independently permitted identifier-to-name mapping. No SDK, external dictionary, platform-extension research or shape-based name guessing was used.

Concrete next action changed: preserve direct Ampr and pooled libkernel as distinct routes when deciding interception coverage. One provider's coverage cannot establish the other's. This proves no signature, synchronous/asynchronous semantics, live branch selection or runtime success, and admits no shipping pins.

Replay from repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/outputs/2026-10-12-caps-prep-chatgpt/cap07/import-audit/tmp" python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/import-audit/audit_imports.py --verify
```

The replay hash-gates the read-only mmap image, rebuilds all five metadata chains and wrapper call, searches bounded dynamic symbols, and compares the complete receipt. `source-receipt.json` hashes the local format parser and prior route context. Historical route evidence is distinct from this fresh metadata verification. Python on this Mac is 3.9.6 with Capstone 5.0.7.

Root replay passed the same five import chains and2,022-symbol check. ROOT-CONTEXT-CHECK.json preserves the audit-time source hashes and records the later deliberate cap07 STATUS.md integration; all other context/parser/implementation hashes still match. That status prose change is not image or runtime evidence.
