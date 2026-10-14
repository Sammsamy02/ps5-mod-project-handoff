# PAK compatibility: independent archive checkpoint

The generic archive reader now parses two independently produced Unreal v11 PAK fixtures from pinned [repak source](https://github.com/trumank/repak/tree/355b5f62f51959c7cc6dd5a51708646ef483065d/repak/tests/packs). These are upstream test archives, not Stellar Blade files.

The first run exposed a real bug: both secondary-index presence fields are four-byte integers; our earlier reader consumed a single byte. The generic primary reader and metadata pipeline are corrected in a new experiment, with the earlier sources/evidence preserved.

Both archives now pass primary SHA1 checking, classic directory parsing and decoding of all eight directory-referenced file records. Thirty-two focused positive and rejection cases pass. The corrected sources also compile and link for PS5; that ELF has not run on-console.

The payload pipeline now reproduces all eight selected file payloads byte for byte against the four retained upstream originals. The compressed fixture contains two Zlib entries and two uncompressed entries. The focused wrapper suite passes 2,237 checks, including shared read/codec budgets and full-output rollback after later failures; many checks inspect bytes, not distinct scenarios.

The stable pieces are now tracked in the [native PAK library](ps5-mod-bridge/native-host/pak/README.md). Its own offline build/check command compiles the promoted source closure, repeats all eight independent byte comparisons and rejects a stale primary digest without publishing payload bytes. Both promoted diagnostic callers also compile/link for PS5, without execution. Secondary-index and physical-entry integrity are separate work; successful raw bytes still do not establish mod conversion or game acceptance.

A limited source check confirmed identical C modules and only intended local header includes. Two development-tool issues were reproduced and corrected: Python optimization previously removed verifier assertions, and a buffered stdout flush failure could leave the payload probe reporting success. Three optimized-driver rejection controls and the actual output-failure control now pass. The real fixture workflow also passes under `python3 -O`.

The directory parser and bounded Zlib block decoder have completed their focused host checks and PS5 compile/link experiments. They have no demonstrated game runtime integration. UE4SS, Mod Engine 2 and arbitrary Windows DLL mods remain Unsupported.

Two more isolated components have completed their checks: the raw secondary-index SHA1 verifier passes five host groups/300 assertions, including both fixtures' four secondary spans and rejection controls; the physical-header parser accepts all eight actual fixture headers and its focused malformed/Unsupported/rollback cases. Both compile/link for PS5 without execution. Their results are not yet composed into the tracked payload library, and the physical parser preserves rather than verifies the per-entry digest. The next step combines these gates with directory-reference validation and one aggregate read budget.

Detailed local evidence: [Boolean-width correction](</WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/native-pak-v11-boolean-fix-1.md>), [directory parser](</WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/native-pak-directory-experiment-1.md>) and [Zlib decoder](</WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/native-zlib-codec-experiment-1.md>).

No console, FTP, account or game-file operation was performed for this checkpoint. Development continues toward the PS5-only manager; no mod has run in Stellar Blade.
