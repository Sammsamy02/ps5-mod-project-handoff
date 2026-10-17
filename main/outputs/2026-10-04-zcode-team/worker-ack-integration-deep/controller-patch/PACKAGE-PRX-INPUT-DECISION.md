# PACKAGE-PRX-INPUT-DECISION — no input-list change required

Question posed to this worker: does `worker-prx-packaging/package_prx.py`'s
input list (the 10 `BOOTSTRAP_UNITS` compiled at packaging time) need a diff
to carry the RUNTIME-ACK integration?

## Answer: NO. The wiring point is the composition, not the packaging unit list.

Measured facts:

1. `package_prx.py` consumes exactly 10 bootstrap units
   (`import_references.S`, `probe/probe.c`, `probe/startup_input.c`,
   `probe/memory_lua.c`, `lifecycle/lifecycle.c`, `lifecycle/module_entry.c`,
   `lifecycle/constructor_boundary.cpp`, `lifecycle/sentinels.c`,
   `import_check.c`, `diagnostic.c`). None of them reference
   `RuntimeOwner` / `PrepareActivatedUE4SSRuntime` / `mfa_status_*` — grep
   over `outputs/ps5-mod-bridge/native-runtime-startup/` finds zero references
   outside `bootstrap/`. The ack has no packaging-side entry point.

2. The ack-enabled code lives INSIDE the composed runtime object: the
   composition's `bootstrap_units` are
   `activated_mod_source`, `ue4ss_backend`, `runtime_owner` (composition-2
   receipt; notify-cancel base receipt pins their sources/objects). Adding a
   second, ack-enabled copy of those TUs at packaging time would create
   duplicate strong definitions in the `ld.lld -r` partial link
   (`_ZN9modbridge6native12RuntimeOwnerC[12]E...`, `RuntimeOwner5startEv`,
   `MakeActivatedUE4SSModSource...` are all exported `T` from the composed
   object — composition-2 `exports.stdout` lines 10343-10362). There is no
   additive way to inject them at packaging.

3. The supplier (`RuntimeAckConfig` values) is needed where
   `PrepareActivatedUE4SSRuntime` is CALLED. Today nothing in the composed
   object references it (zero undefined refs to its mangled name in
   `nm-composed-undefined.stdout`, 199 lines), i.e. the module_start caller is
   the next module's new work — it enters through the composition mechanism
   (extra/replaced units), exactly like every other runtime unit. A packaging
   list diff cannot reach it.

4. Import-safety of the ack at packaging time (why the packaging pre-check
   stays green): MEASURED — the ack-enabled units' new undefined symbols are
   (a) defined by the ack pair itself (RuntimeAckSink*, ack ctor), (b) defined
   inside the composition closure (`mfa_status_open/write/close` are `T` in
   the composed object, from the activation component; `libc++.a
   string.cpp.o` `to_string` + the `basic_string append/assign/insert`
   overloads are present in composition-2 `runtime.map`), or (c) already in
   the frozen import contract (`getpid`, `clock_gettime` — the supplier's
   needs). Zero new strong imports; the contract stays 196 (191+5).

So: `package_prx.py` runs UNCHANGED; the packaging re-run after composition-3
simply hash-verifies the NEW composed object checkpoint and links it. If a
future controller ever wants the packaging list to grow, that is a recipe
lineage change with its own pinned-values patch — out of scope here and not
needed for the ack.
