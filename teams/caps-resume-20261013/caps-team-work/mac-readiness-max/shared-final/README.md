# Final accepted #10 pair: ordinary shared link

The actual accepted signal owner object and fixed v2 Mono runtime link with genuine target static corlib, the unchanged invocation boundary and link-only observer/provider fixtures using the public SDK. This refresh supersedes the earlier shared-link receipt for the old signal pair.

The strict ordinary ELF shared link passes `--no-undefined -z defs`; no PIE, TEXTREL, 32-bit/PC32 dynamic relocations or dynamic Mono dependency remains. All Mono registration and GC-rooting functions remain local and defined. Only the link fixture's `main` is exported. No generated ELF was run, packaged as a game PRX or sent to a console.

There are **290 strong SDK-backed dynamic imports and six unprovided weak hooks**, for 296 undefined dynamic symbols. The single new strong import versus the old owner is `sigismember`. The weak names are `__dladdr`, `__dlclose`, `__dlerror`, `__dlopen`, `__dlsym`, and `kernel_mprotect`; link permission does not establish their runtime function. These are not claimed to be provided by the game. Needed SDK libraries are `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`.

The output is retained locally for diagnosis and excluded from the delivery archive. The exact inputs, commands, output hash, symbols and relocations are in `evidence/SHARED-OWNER-LINK.json` and `logs/`. This component check does not certify Claude's full combined game/PRX build, packaging imports, target Mono execution or signal delivery.
