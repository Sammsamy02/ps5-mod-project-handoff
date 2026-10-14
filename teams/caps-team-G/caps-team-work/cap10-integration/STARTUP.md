# STARTUP behaviour — what #10 does at game start (Team G, task 2)

## GPT's delivery: Mono on EVERY launch

Proven from source (`cap10/native/MonoOwner.cpp`, `UE4SSProgram.cpp` engine-ready
hunk):

- The engine-ready callback ran `(void)Cap10Host::run_embedded_probe_once();`
  unconditionally whenever the unit was compiled with
  `MODBRIDGE_CAP10_ADMITTED_MONO_PORT=1` (which `cap10_integration.compile_argv`
  always added). No check for a C# mod, no switch.
- `run_embedded_probe_once` does, on every launch: `mono_set_dirs` +
  `mono_config_parse` + `mono_set_signal_chaining(true)` +
  `mono_jit_init_version`. Mono's init installs **process-wide** handlers for
  SIGFPE/SIGQUIT/SIGILL/SIGBUS/SIGSYS/SIGABRT/SIGSEGV, sets SIGPIPE to ignore,
  unblocks those on the init thread, and SIGQUIT arms the attach agent. It maps
  ~34.5 MiB (host trace: 33 mmap / 7 mprotect, three 8 MiB SGen regions) and
  starts a finalizer thread.

**Verdict on the delivery: fails the requirement.** With no C# mod present it
still initialises Mono, changes signals and maps memory.

## Team G fix: lazy, data-driven by the activation snapshot (like #9)

Requirement: with no C# mod enabled, **nothing from Mono runs** — no init, no
signal change, no thread, no mapping.

Change (shipping C++, in the patch):

1. New engine-free planner `cap10/native/Cap10CSharpMods.hpp`. In
   `setup_mods`' activation-snapshot branch it decides, from the snapshot only
   (the **same enable rule as #9** C++ mods: `mods.txt` `"Name : 1"` rows or a
   mod's `enabled.txt`), which mods are C# mods (they have a `csharp/`
   directory), and whether any **enabled** one carries an assembly
   byte-identical to the built-in probe image (`ProbeImage::bytes`). It sets a
   process flag `cap10_probe_requested` (default **false**; any error → false,
   fail closed). It uses only non-throwing `std::filesystem` (error_code)
   overloads and bounded loops.
2. The engine-ready callback now branches on that flag:
   - flag **true** (an enabled C# mod carries the probe): the original path —
     `Cap10BridgeAdmissionScope` + `on_engine_initialized()` +
     `Cap10Host::run_embedded_probe_once()` — runs. Mono starts.
   - flag **false** (default, no C# mod): the **unchanged pre-#10 call**,
     `m_native_context.on_engine_initialized();` only. No admission scope, no
     `run_embedded_probe_once`, so **no `mono_*` call is reached at all** —
     no init, no `mono_set_signal_chaining`, no SGen/finalizer thread, no Mono
     mapping.

The rule is generic and data-driven (no game/mod names, no offsets); it mirrors
#9's `discover_cpp_mods`. Proven by the host test
`tests/host/test_csharp_planner.cpp` (16 checks): probe requested **only** for an
enabled mod whose `csharp/` holds the exact probe; not for a disabled mod, a
non-probe assembly, a relative mods dir, or a missing probe image.

## The static corlib stays in the PRX (answer to the follow-up)

Lazy init stops Mono from **running**; it does **not** keep the 13.4 MiB static
corlib (or the Mono archive) **out of the PRX**. The owner references `mono_*`
and `mono_aot_module_mscorlib_info`, so the linker pulls the archive + corlib
into the runtime object unconditionally whenever `MODBRIDGE_CAP10_ADMITTED_MONO_PORT=1`
is compiled in. They are resident all session, C# mod or not.

Why it can't be lazily excluded: section/symbol selection happens at **link
time**; there is no runtime toggle that removes a linked archive. To keep the
corlib out of a build you must not compile #10 in for that build.

How it stays out of every other build: the #10 change is **additive and
self-contained** — the define, the NEW_UNITS and the artifact inputs are added
only by `cap10_integration` when the #10 patch is applied. A build without the
patch (the clean rollback, and the **Stellar Blade blind test**) has zero Mono
and the import count returns to 207. Recommendation in `APPLY.md` §5: build the
#10 launch with the patch; build Stellar Blade and the rollback without it.
