# CAP13 step 4 — Managed DLL mod route (design)

Agent JI (ZCode), 2026-10-13. Owner of the managed route inside cap13 (JG owns PE
analysis, JH owns the native loader). **Mac-only design + staged source.** No
builds, no console contact, no main-tree edits, no integrator source changes.
Composes on top of #10; does not modify any cap10 file.

Evidence labels (same discipline as `../cap10/REVIEW-VERDICT.md` and HE's
`CAP10-CSHARP-DESIGN.md`):
- **[LOCAL-SOURCE]** — from files in this worktree (paths cited).
- **[TRAINING-KNOWLEDGE]** — ecosystem facts from training data; every such fact
  carries a pin action and is listed in the staleness register (§9). Nothing in
  this document depends on an unpinned fact for its v1 behavior: v1 admits only
  mods whose required surface is fully proven in-tree.
- **[INFERENCE]** — my reasoning on top of the above.

---

## 0. Scope and the one composition fact that defines this design

#13 step 4 (assignment, `[LOCAL-SOURCE: ../cap13-dll/ZCODE-ASSIGNMENT.md]` item 4):
"use #10 for compatible managed DLL mods, adding real mod discovery, dependency
resolution, framework/API integration and lifecycle beyond the embedded Probe,"
with the acceptance line "a real compatible C# DLL mod also performs its intended
action through the managed route. The embedded Probe alone is insufficient."

**What #10 actually is today** `[LOCAL-SOURCE: ../cap10/MAC-INTEGRATION-HANDOFF.md]`:

- PROVEN: Roslyn-compiled Probe.dll bound to the Mono-selected .NET Framework
  4.7.2 reference mscorlib; real matching-Mono host runs of the production
  InvocationBoundary (Run → exact managed exception → recovery, negative
  signature refusals, GC-unsafe region pairs); a genuine **source-built target
  Mono archive** (`../cap10-port-closure/target-work/mono/mini/.libs/
  libmonosgen-2.0.a`) with the strict owned-name loader patch compiled in; a
  **static target corlib AOT** + matching IL (`../cap10-cross-aot/`); a target
  config tree; and a successful `--no-undefined` standalone target link.
- NOT PROVEN: any in-game execution (the Townfall Probe console pass is Claude's
  pending integration test); the `libmono-native`/System.Native surface on
  target (JB §2 — even incidental corlib file I/O fails without it); thread
  attach of a second (engine tick) thread; SGen suspend/rooting under real game
  load; any managed assembly other than the single embedded Probe.
- The exact Probe contract: one-shot owner claim, `MONO_AOT_MODE_INTERP`,
  static corlib registration, owned-name dl-fallback provider answering exactly
  two `__Internal` symbols, per-entrypoint `mono_dllmap_insert`, static-signature
  admission, observation windows, `diagnostic_event` receipts.

**The composition fact** `[LOCAL-SOURCE: ../cap10/native/MonoOwner.cpp]`:
the current cap10 owner is **probe-shaped**. `run_embedded_probe_once()` creates
the root domain, runs the probe, and its `Lifetime` destructor calls
`mono_jit_cleanup` **before returning** — the runtime does not survive the
callback. A mod host needs a **resident** runtime: init once at ready admission,
stay alive across engine ticks, cleanup only at module shutdown on the init
thread. This design therefore stages a `ResidentMonoOwner` that mirrors
`MonoOwner.cpp` admission-for-admission (same `#error` guards, same owned-name
loader-patch requirement, same `mono_set_dirs` PREFIX semantics, same
`mono_aot_register_module` static corlib path) with the lifetime moved to module
exit, and runs the cap10 Run/throw/recovery boundary as **admission rung 1**
inside that resident runtime. cap10 files are not touched; this is a staged
composition delta for integrator review (§6d).

---

## 1. The real mod landscape — what real managed mods look like

Honest headline first, because it bounds everything below:

**There is no real population of managed DLL mods for Unreal Engine games.**
Townfall is an Unreal Engine title modded through UE4SS `[LOCAL-SOURCE: the
project's PS5-MODDING-MISSION state: UE4SS, FName pool, GUObjectArray listeners]`.
The .NET modding ecosystems are Unity-game ecosystems:

- **UE4SS has no managed (C#/.NET) plugin API at all.** Real UE4SS mods are (a)
  Lua script mods (`Mods/<name>/Scripts/main.lua`, registered in `mods.txt`) or
  (b) native C++ DLL mods built against the UE4SS headers, loaded by UE4SS's own
  mod loader (the `start_mod`-exporting Windows binaries the #9 host currently
  refuses) `[TRAINING-KNOWLEDGE, flag low — stable project design; also
  consistent with the #13 handoff's own wording]`. The task brief's "IMod for
  UE4SS" interface does not exist upstream; this design does not invent one and
  does not claim it.
- **BepInEx** and **MelonLoader** are Unity frameworks. Their real mod corpora
  bind `UnityEngine.*` assemblies and the game's managed script assembly — which
  exist in a Unity PC build, and **do not exist at all** in an Unreal game.
- Consequently, the only honest targets for the managed route in Townfall are:
  1. mods **purpose-built against this host's managed SDK** (the v1 minimal
     path, §5); and
  2. real downloaded BepInEx/MelonLoader mods **classified and precisely
     refused** with named gaps (or run only in the degenerate case where they
     reference and touch zero engine types — the mscorlib-only class, which the
     wild corpus essentially does not contain).
  This mirrors the assignment's own native-route discipline: "If no suitable
  real Townfall DLL mod exists, report that evidence gap and use a clearly
  labeled … fixture …; do not call that a real-mod pass."

For completeness — what a real managed mod looks like (all
`[TRAINING-KNOWLEDGE]`, pin actions in §9):

| | BepInEx 5 (Mono Unity) | BepInEx 6 bleeding-edge (IL2CPP) | MelonLoader | UE4SS |
|---|---|---|---|---|
| Target runtime | Unity Mono, legacy .NET Framework profiles (net35/net4x family) | .NET 6 CoreCLR + Il2CppInterop proxies | .NET Framework 4.x (Mono games); net6 line for IL2CPP | none (Lua / native C++) |
| Typical references | `BepInEx.dll`, `0Harmony.dll` (HarmonyX), `UnityEngine.CoreModule.dll`, `Assembly-CSharp.dll`, `Mono.Cecil` | `BepInEx.Core.dll`, `Il2Cppmscorlib.dll`, `Il2CppInterop.Runtime.dll`, `UnityEngine.CoreModule.dll` (proxies) | `MelonLoader.dll`, `UnityEngine.CoreModule.dll`, `Il2CppInterop.Runtime.dll` or plain Unity refs | UE4SS C++ API / Lua API |
| Entry point | `[BepInPlugin(guid, name, ver)] class X : BaseUnityPlugin` (BaseUnityPlugin is a MonoBehaviour) | same, interop-typed | `[assembly: MelonInfo(typeof(X), …)]` `[assembly: MelonGame(…)]` `class X : MelonMod` | `start_mod` export / `main.lua` |
| Lifecycle | ctor → Awake() (chainloader adds plugin as a GameObject component) → optional Start/Update/OnDestroy via Unity component lifecycle | same shape | `OnInitializeMelon()` → `OnUpdate()`/`OnLateUpdate()` per frame → `OnApplicationQuit()` | UE4SS mod init / per-tick Lua hooks |
| Native interop | usually none; some `DllImport("kernel32"/"user32")`; IL2CPP line routes everything through Il2CppInterop unmanaged delegates | Il2CppInterop `il2cpp_*` natives | same split as BepInEx | direct C++ |

Framework-version answers for the task questions, condensed:
(a) real managed mods target **.NET Framework 3.5–4.8** (Mono-line BepInEx 5 /
MelonLoader) or **.NET 6+** (IL2CPP-line, CoreCLR) `[TRAINING-KNOWLEDGE]`;
(b) they reference **framework DLLs + engine assemblies + the game's managed
assembly**, never mscorlib alone; (c) entry points are **framework loader
conventions** (attribute + base class), not `Main`; (d) native interop is rare in
Mono-line mods and Windows-bound when present; the IL2CPP lines replace
P/Invoke with unmanaged interop delegates.

**Our runtime sits on the Mono 4.x profile side** (`lib/mono/4.5/mscorlib.dll`
runtime corlib compiled against 4.7.2 reference assemblies — binding proven
exactly for Probe's surface `[LOCAL-SOURCE: cap10 REVIEW-VERDICT §4]`). .NET
6+/CoreCLR-targeting mods are **out of scope permanently for this runtime** —
they reference `System.Private.CoreLib`/netstandard profiles this Mono cannot
present; they are refused by name at discovery.

---

## 2. The discovery layer

Discovery is **native-side** (mod root, hashing, PE/CLR facts) with the
**managed loader** refining lifecycle classification by reflection after images
are admitted. Facts are computed **once per (path, size, sha256)** and cached;
no recurring scans `[LOCAL-SOURCE: #13 acceptance "discover/classify/cache
stable module facts once"]`.

### 2a. CLR metadata (interface contract with JG's pe_analyzer)

cap13 consumes a per-file facts record (JSON) with at least:

```
{ "path", "size", "sha256",
  "pe":   { "machine", "is_dll", "characteristics" },
  "clr":  { "present", "runtime_version",           // e.g. "v4.0.30319"
            "il_only", "anycpu_32bit_pref",          // COR20 flags
            "assembly_name",                         // self-declared Assembly name
            "native_entry_or_vtable_fixups" },       // mixed-mode detector
  "tables": { "assembly_ref": [ { "name", "version", "public_key_token" } ],
              "type_ref_namespaces": [ "BepInEx", "HarmonyLib", … ],
              "custom_attr_names":   [ "BepInPlugin", "MelonInfo", … ],
              "implmap_imports":     [ { "entry_point", "dll" } ] } }
```

`pe` + `clr` basics are standard PE analysis (JG's existing `pe_analyzer`
surface); the `tables` block is the cap13 ask: **AssemblyRef, TypeRef,
CustomAttribute and ImplMap enumeration**. If the analyzer's real output shape
differs, the adapter in the integrator's discovery step maps it; cap13 owns only
the classification semantics. Input files are read **read-only**; the facts
cache lives under our overlay (`facts/`), never inside the mod package — the
byte-identity law applies to mods `[LOCAL-SOURCE: HE design law 2]`.

### 2b. Framework classification (decision table)

Applied to every file whose CLR header is present and `il_only`:

| Order | Test (on facts record) | Classification |
|---|---|---|
| 1 | `native_entry_or_vtable_fixups` set, or non-empty unmanaged export table | `mixed-mode` → **refuse** ("mixed managed/native assemblies unsupported") |
| 2 | AssemblyRef contains `System.Private.CoreLib` / netstandard-only profile and no `mscorlib` | `dotnet-modern` (.NET 5/6/7/8+) → **refuse** ("runtime is Mono 4.x profile") |
| 3 | CustomAttr has `BepInPlugin` or TypeRef ns `BepInEx` | `bepinex-shape` |
| 4 | CustomAttr has `MelonInfo` or TypeRef ns `MelonLoader` | `melonloader-shape` |
| 5 | CustomAttr has cap13's `MbmModAttribute` (or the host admits the mod by manifest) | `mbm-sdk` — the supportable class |
| 6 | none of the above, AssemblyRefs ⊆ {mscorlib} | `standalone` (admitted as mscorlib-only) |
| 7 | anything else | `unknown` → **refuse** with the exact unmet reference list |

Classification is **conservative by construction**: a mod that references both
BepInEx and MelonLoader types is bepinex-shape AND melonloader-shape; the
loader then refuses it as "multi-framework" unless every referenced framework
assembly is present in the admitted shim set (§4c — today: none are, so
multi-framework = refuse). A BepInEx-shape mod whose AssemblyRefs happen to be
satisfiable still gets the full per-reference closure test below; shape alone
never admits.

### 2c. Dependency extraction and compatibility requirements

The **admission closure** (native, deterministic, from the facts records):

1. Node set = classified `mbm-sdk`/`standalone` mods. Edges = AssemblyRef
   matching another node's assembly name.
2. Reference resolution policy: `mscorlib` resolves to the statically
   registered runtime corlib (binding proven `[LOCAL-SOURCE: cap10 §4]`);
   `Mbm.ModSdk` resolves to the host-shipped SDK image (exact name+version
   match, no unify); any other AssemblyRef → **refuse the referrer** with the
   exact missing name+version+token.
3. Load order = topological sort of the edges (stable tie-break: sha256, so the
   order is reproducible); a cycle refuses every mod in the cycle, listing it.
4. ImplMap audit (per mod image): entry points with `dll == "__Internal"` must
   all be members of the frozen v1 ABI set (§6a); anything else → refuse
   "undeclared native interop `<entrypoint>`". Any `dll != "__Internal"`
   (kernel32, user32, game DLLs…) → refuse "external native dependency `<dll>`".
   Both refusals fire at admission with the precise name — never as a
   mid-game `DllNotFoundException` `[LOCAL-SOURCE: #13 acceptance "unavailable
   imports/features produce a precise diagnostic instead of a fake successful
   no-op"]`.
5. Mixed native dependencies (a managed mod wanting to P/Invoke into a #9
   native module): **not supported in v1**, refused by rule 4. Demonstrated
   mixed support is its own later rung per the assignment.

### 2d. Lifecycle determination

Per classification:

- `mbm-sdk`: the mod declares a public non-abstract subclass of
  `MbmModSdk.MbmMod` with a public parameterless constructor; lifecycle =
  `OnLoad(ModHost)` once at load, `OnTick()` per admitted engine tick,
  `OnUnload()` once at shutdown. Discovered **managed-side by reflection**
  (`Assembly.GetTypes()`, `IsSubclassOf`) after native admission — this is the
  managed ModLoader's core job, chosen over native subclass scanning because
  reflection under the interpreter is proven machinery `[LOCAL-SOURCE: cap10
  MONO-INTERPRETER-CHECK — reflection Invoke ran]` and keeps all type-system
  questions inside the runtime.
- `bepinex-shape` / `melonloader-shape`: lifecycle mapping (Awake→OnLoad,
  Update→OnTick; OnInitializeMelon→OnLoad, OnUpdate→OnTick) is **designed**
  (§6c, `ManagedLifecycle.hpp`) but gated behind the framework shims that do
  not exist (§4b/4c). v1: refused at admission with the framework name and the
  exact list of unsatisfiable AssemblyRefs. No partial "it loaded but Awake
  never fires" states.
- `standalone` (no recognized lifecycle): admitted as a library only if
  something else references it; never ticked; `OnLoad` semantics do not apply.
  A `standalone` mod with a public static entry may be invoked once for a
  diagnostic run under the Probe-style boundary if a future rung wants it —
  not v1.

---

## 3. Runtime integration — extending the #10 embedding to real mods

### 3a. The assembly loading chain

Target runtime shape (fixed by cap10, reused verbatim): interpreter mode
(`mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP)`), statically registered corlib
AOT + matching IL, `mono_set_dirs(PREFIX/lib, PREFIX/etc)` semantics,
`mono_dllmap_insert` + dl-fallback as the ONLY interop channel, **no ambient
dlopen** `[LOCAL-SOURCE: cap10 MonoOwner.cpp, MAC-INTEGRATION-HANDOFF]`.

Chain for mods (differs from Probe in that mod images are **not embedded**):

```
static corlib (AOT-registered at init, from cap10 packaging)
  └─ Mbm.ModSdk.dll        (host-shipped; bytes from our overlay; admitted by hash)
       └─ Mbm.ModLoader.dll (host-shipped; embedded like ProbeImage, hash-pinned)
            └─ admitted mod images, in topological order (bytes read by native
               from the mod root overlay, admitted by sha256 before load)
```

Mechanics per image: native reads bytes → sha256 check against the cached
admission record → `mono_image_open_from_data_with_name(bytes, len,
need_copy=true, &status, false, "modbridge-cap13-<sha8>-<name>.dll")` →
`mono_assembly_load_from_full` → per-entrypoint `mono_dllmap_insert(image,
"__Internal", entrypoint, owned_library_name, entrypoint)` for exactly the
admitted ABI symbols (the cap10 pattern; a catch-all NULL-func insert exists in
the Mono API but its exact semantics would need pinning — v1 uses the proven
per-symbol form only) → dl-fallback provider answers only the frozen ABI set
and returns NULL for anything else, which surfaces as a precise
`DllNotFoundException` naming the entry point. Unknown/unadmitted images never
get this far (rule 2c.2): there is no lazy surprise resolution mid-game.

**mscorlib-only discipline.** Only corlib is deployed on target today
`[LOCAL-SOURCE: cap10 handoff item 5: mscorlib IL + AOT + config; nothing else]`.
Therefore v1 admits only images whose closure is {mscorlib, Mbm.ModSdk, other
admitted mods}. In .NET 4.x that still buys real machinery — List<T>/Dictionary
(System.Collections.Generic), all of System.Reflection, StringBuilder,
System.Runtime.InteropServices, System.Threading primitives — but NOT LINQ
(System.Core) or System.dll. The staged C# sources obey this constraint
verbatim (no `using System.Linq`). Any additional BCL assembly is a new
packaging/cross-AOT decision, not a loader feature.

### 3b. Type resolution

Managed-side, inside the runtime, by the embedded ModLoader:

1. Native loads ModLoader first, then invokes its documented entry
   (`Mbm.ModLoader.LoaderMain(int)` — static, `int` return, exact signature
   admitted with the cap10 `signature_matches` discipline extended to the
   1-param form already proven).
2. ModLoader enumerates admitted mods via the bridge
   (`mbm_cs_mod_count()` / `mbm_cs_mod_assembly(i, StringBuilder, ref int)` —
   short names only, no paths, no managed file I/O) and binds each through
   `AppDomain.CurrentDomain.GetAssemblies()` name matching (the assemblies are
   already native-registered; this avoids Assembly.Load probing, which would
   need an ApplicationBase and file access — unavailable). Fallback if name
   registration differs on the matching-Mono host: pass bytes via
   `Assembly.Load(byte[])` — decided at the Mac validation rung M2 (§5), pinned
   in the rung receipt.
3. For each bound assembly: `GetTypes()` inside try/catch
   `ReflectionTypeLoadException` (loader exceptions are surfaced as **per-type
   precise diagnostics**, never swallowed); find non-abstract `MbmMod`
   subclasses; instantiate (`Activator.CreateInstance`); call `OnLoad(host)`.
4. BepInEx/Melon shapes, if a shim set ever ships, resolve the same way against
   the shim base classes; without shims they were already refused natively.

### 3c. The engine binding (managed → UE4SS host)

The only channel is the **owned-name dl-fallback provider**, exactly like
cap10's two-symbol provider, extended to the frozen v1 ABI (§6a): the provider
exact-matches symbol names, records per-symbol admission flags, and every
managed→native call is bracketed by the observation-window discipline
(entry/exit counted, cookie checked, poisoning on anomaly — mirroring
`Cap10InvocationObservation`, in a cap13-owned namespace). Engine-backed calls
(`mbm_cs_engine_*`) bind to **#9's admitted native mod services** through a
declared engine-surface interface; unbound surface = every call returns
`MBM_CS_ENGINE_UNBOUND` — fail-closed, no fake reads. This design does not
invent engine access; it consumes whatever #9's `NativeModAbi.h` lifecycle
admits, and the v1 read surface is deliberately tiny (find object by path,
read one numeric property) to mirror the Probe contract's minimalism.

GC discipline at the boundary (all `[LOCAL-SOURCE: cap10 InvocationBoundary.cpp]`
rules): every native entry that touches Mono state runs in a
`mono_threads_enter_gc_unsafe_region` pair; raw `MonoObject*` returns are
rooted via `mono_gchandle_new(..., pinned)` before first use; the engine-tick
thread attaches once (`mono_thread_attach`) and never detaches while the host
is resident; `mono_jit_cleanup` runs only on the init thread at module
shutdown. Reverse calls (native tick → managed `OnTick`) use
`mono_runtime_invoke` on the gchandle target with the exception slot checked
before any result use.

### 3d. The mod lifecycle

```
ready callback admission (engine initialized, cap10-style)
  → ResidentMonoOwner.start():  one-shot claim; init interpreter + static corlib;
      admission rung 1 = cap10 Run/throw/recovery must pass (else: no mods load,
      CAP13_STAGE records the refused state)
  → discovery + admission closure (native, from cached facts)
  → load ModSdk, ModLoader (hash-pinned), then mods in topo order
  → ModLoader discovers mod types, constructs, OnLoad(host) each
      (exception in OnLoad = mod disabled + precise diagnostic; other mods proceed)
steady state:
  → engine tick (UE4SS lifecycle, post-engine-update phase) → mbm_managed_host_tick()
      → for each live mod, invocation-boundary call OnTick(); a managed exception
        is caught per-mod (exception slot), counted, and after N=3 consecutive
        failures the mod's tick is disabled with a CAP13_EXC receipt — one bad
        mod never kills the tick loop or other mods
shutdown (module exit, init thread):
  → OnUnload() each (best-effort, exception-isolated) → gchandles freed →
      mods quiesced → observation/provider teardown ordering mirrors cap10
      (image closes may touch module cache; provider/user outlive it) →
      mono_jit_cleanup last
```

**Unload honesty:** Mono 6.12 embedded has no safe per-assembly unload
(`mono_domain_unload` is deprecated-unsafe; collectable domains are not a
proven path) `[TRAINING-KNOWLEDGE, flag medium]`. v1 "unload" = quiesce:
callbacks deregistered, handles released, mod marked inactive. The assembly
stays resident in the domain. This is stated in the support matrix, never
hidden: unload-clean means *quiesced with no live callbacks*, not *memory
reclaimed*.

---

## 4. The honest gap — Probe runs on host Mono vs a real mod runs in Townfall

(a) **PS5 Mono port.** Farther along than the task brief assumed — but not
closed. A genuine source-built target runtime archive, static target corlib AOT,
target config, and a standalone `--no-undefined` target link now exist
`[LOCAL-SOURCE: cap10 MAC-INTEGRATION-HANDOFF]`. Still missing before ANY mod
claim: the in-game Townfall Probe pass (real SGen stack rooting, pthread TLS,
signal/context behavior, AOT transitions in the game process); the target
`libmono-native`/System.Native build + dllmap (mods doing managed file I/O die
exactly like JB's §2 failure without it); second-thread attach under real game
load. The port is **artifact-proven, in-game-unproven**. Everything in this
design is staged against the admitted-port guards and compiles to nothing
without them.

(b) **UnityEngine shim.** Not buildable honestly. A stub `UnityEngine.dll`
would make mods load and then return garbage/throw at every property — a fake
success, which the project's never-fabricate law forbids. In an Unreal game
there is no real backing for Unity types. v1: refuse with the exact missing
reference list. A real UE↔Unity-object bridge would be a new capability, not
part of #13 step 4.

(c) **BepInEx framework shim.** The *metadata* surface is shim-able
(`BaseUnityPlugin`, `BepInPlugin`, `ConfigFile`, `ManualLogSource` are pure
managed types we could compile), and the lifecycle mapping is designed (§6c).
The *chainloader* is not: BepInEx boots through Unity's engine (GameObject
creation, script-component lifecycle, Unity dispatch), none of which exists
here. A "BepInEx-compatible" claim therefore cannot be made for Townfall.
`bepinex-shape` mods whose full closure is satisfiable *could* get a
SHIMMED-EXPERIMENTAL class in a later rung (Awake→OnLoad etc.); v1 refuses
with named gaps. Same reasoning for MelonLoader.

(d) **Harmony.** HarmonyX patches = memory detours/trampolines + (in interpreter
mode) managed method-table manipulation — both outside anything proven. On the
native side the legitimate subset maps onto #3's *proven* pointer-swap half for
engine methods (HE §3's map, adopted here by reference); runtime managed-method
patching under `MONO_AOT_MODE_INTERP` is an unproven research question, and any
executable-memory write is W^X-exposed on target. v1: a mod referencing
`0Harmony.dll` is refused at the closure (missing reference) with the precise
reason "Harmony patching unsupported on this host". Never silently run a mod
whose `Patch()` calls would no-op.

(e) **GC/threading.** SGen conservative stack/register scanning, suspend and
finalizer threads exist in the compiled target archive but are proven only as
far as the Mac static-corlib host run; in-game behavior is exactly what the
pending Townfall Probe must show `[LOCAL-SOURCE: cap10 handoff "Verify actual
SGen stack rooting, pthread TLS, signal/context and AOT transitions there"]`.
Mods that spawn managed threads are refused in v1 (no admitted thread-attach
gate for arbitrary mod threads); the single engine-tick thread attaches through
the proven owner discipline. Concurrent tick + ready-callback access is
serialized by the host lifecycle (tick phase never overlaps admission).

(f) **Additional facts this design refuses to paper over.** (i) There is today
no real downloaded mod that fits the v1 admitted class (§1) — the managed
acceptance will need either a genuinely fitting real mod found at integration
time or a labeled evidence gap, same discipline as native route item 1. (ii)
Facts caching must not introduce recurring scans (acceptance line) — cache keyed
by size+sha256, invalidated only on change. (iii) Startup/steady-state cost:
interpreter startup happens once at ready admission; per-tick cost is
N admitted mods × one boundary invoke — measured against the #9 baseline when
integrated, never claimed now.

---

## 5. The minimal path

1. **Fixture mod "HelloMod"** (staged source, `managed/HelloMod.cs`): C# on the
   4.x profile, references exactly mscorlib + `Mbm.ModSdk`; on load logs
   `MBM hello` via `mbm_cs_log`, finds one engine object by path, reads one
   numeric property once, logs the value, counts ticks for 64 ticks then
   self-quiesces via `host.Stop()`. Compiled with the exact cap10 recipe —
   pinned Roslyn `csc` from the dotnet 10.0.401 host tree, `/nostdlib+
   /noconfig`, against the Mono-selected v4.7.2 reference mscorlib
   `[LOCAL-SOURCE: cap10/evidence/COMPILE.json]` — plus `Mbm.ModSdk.dll`
   compiled the same way. HelloMod is OUR fixture and is labeled as such
   everywhere (original-mod-identity law); it can never be presented as a
   downloaded mod.
2. **Run it through the #10 embedding**: Mac validation rungs M1–M3 (§5.1
   ladder below) extend the existing MONO-HOST-CHECK harness shape — same
   matching-Mono runtime, ResidentMonoOwner in host-validation mode, ModLoader
   embedded, HelloMod loaded from bytes.
3. **Call into the engine**: on Mac, the engine surface binds to a mock table
   (deterministic object/property store); on target it binds to #9's admitted
   services. The contract (call shape, status codes, observation windows) is
   identical in both — only the binding differs, and target binding is
   integrator work.

### 5.1 Mac validation ladder (Mac-only; each rung leaves a receipt)

- **M1 — resident owner**: ResidentMonoOwner boots in host-validation mode;
  cap10 Run/throw/recovery passes as rung 1; runtime STAYS UP after the probe;
  explicit shutdown on the init thread passes. (New receipt class vs cap10's
  one-shot owner.)
- **M2 — loader + sdk**: ModSdk + ModLoader load; ModLoader's entry invokes;
  GetAssemblies name-matching confirmed (or byte-fallback chosen and pinned);
  discovery over a synthetic admission list finds HelloMod's type.
- **M3 — lifecycle + bridge**: OnLoad/OnTick/OnUnload fire through the
  boundary; per-mod exception isolation proven (a deliberate throwing fixture
  mod is disabled after 3 failures; sibling keeps ticking); all v1 ABI symbols
  resolve via the provider; refusals exercised (unknown import, missing ref,
  BepInEx-shape fixture) each producing the precise refusal receipt.
- M4 (target, integrator-owned, NOT authorized here): the composed build's
  console test card — markers `CAP13_STAGE / CAP13_MODS / CAP13_MOD_RUN /
  CAP13_TICKS / CAP13_EXC / CAP13_DONE`, 3-restart law per the standing rules.

---

## 6. The design — staged sources in `cap13/managed-loader/`

### 6a. The frozen v1 managed ABI (`abi/MbmManagedAbi.h`)

All symbols are `__Internal`, `Cdecl`, `ExactSpelling`; i32 status codes,
out-params written only on success, no exceptions across the boundary:

| Symbol | Shape | Notes |
|---|---|---|
| `mbm_cs_abi_version` | `i32 ()` | constant 1 |
| `mbm_cs_log` | `i32 (i32 level, const char* msg)` | host diagnostic_event bridge |
| `mbm_cs_host_query` | `i32 (i32 key, i64* out)` | tick count, mod count, boot id |
| `mbm_cs_mod_assembly` | `i32 (i32 index, char* buf, i32* len)` | admitted assembly short name |
| `mbm_cs_engine_find_object` | `i32 (const char* path, void** out_handle)` | #9 surface bound; UNBOUND → -5 |
| `mbm_cs_engine_read_number` | `i32 (void* handle, const char* prop, double* out)` | read-only; no writes in v1 |
| `mbm_cs_engine_release` | `i32 (void** handle)` | handle table hygiene |

Growth policy: additions are new symbols + version bump; existing symbols never
change shape. The provider answers only symbols in the frozen set — this set
*is* the discovery-time ImplMap whitelist (§2c.4).

### 6b. Managed side (`managed/`)

- `Mbm.ModSdk.cs` — the mod-facing contract: `MbmMod` abstract base
  (`OnLoad/OnTick/OnUnload`), `ModHost` facade (Log/FindObject/ReadNumber/
  TickCount/Stop) wrapping the `__Internal` P/Invokes. mscorlib-only; no LINQ,
  no System.Core, no System.dll (§3a constraint).
- `ModLoader.cs` — embedded loader: `LoaderMain(int)`; binds admitted
  assemblies by name; reflection discovery of `MbmMod` subclasses with
  `ReflectionTypeLoadException` surfaced per-type; construct + OnLoad; per-mod
  exception isolation on tick (3 strikes → disable + receipt); framework-shape
  detection as defense in depth (BepInEx/Melon subclass found → refuse that
  mod with the reason even if native facts missed it).
- `HelloMod.cs` — the labeled minimal fixture (§5.1).

### 6c. Native side (`native/`)

- `MbmManagedBridge.hpp/.cpp` — ResidentMonoOwner (adapted from cap10's
  MonoOwner with the lifetime delta, same admission guards + a new
  `MODBRIDGE_CAP13_RESIDENT_OWNER` compile admission so it cannot compile
  accidentally); dl-fallback provider over the frozen ABI with per-symbol
  admission flags and cap13-owned observation windows; discovery/closure
  application from cached facts; `mbm_managed_host_start/tick/shutdown`
  lifecycle entries for the integrator's #9 lifecycle to call; engine-surface
  binding hooks (fail-closed when unbound).
- `ManagedLifecycle.hpp` — the phase machine (Boot → Probe → Load → Tick →
  Shutdown) and the framework→lifecycle mapping table (mbm-sdk = active;
  bepinex/melon = mapped-but-gated with the named shim requirements; unknown =
  refuse), so the "framework/API integration" surface is explicit even where
  v1 refuses.

### 6d. Integration deltas (staged for review, nothing applied)

1. Resident owner replaces the one-shot probe shape in the composed build
   (cap10 files untouched; probe run becomes admission rung 1 inside the
   resident host). The cap10 observation/provider binding in the integrator's
   tree must keep working unchanged.
2. Two NEW_UNITS (bridge + its header set) added to the driver's pinned
   dependency metadata, mirroring the cap10 handoff's NEW_UNITS discipline.
3. Engine-surface binding to the current #9 `NativeModAbi.h` services —
   exact function selection is integrator work against Claude's read-only
   candidate tree; this design only fixes the fail-closed contract.
4. Mod bytes arrive from the mod-root overlay (#7 pattern) through the native
   discovery step; no managed file I/O anywhere.

---

## 7. Support matrix (v1, honest)

| Mod class | v1 behavior |
|---|---|
| mscorlib + Mbm.ModSdk (+ other admitted mods), no extra refs | ADMITTED — full lifecycle |
| mscorlib-only library (no lifecycle) | ADMITTED as a reference-only assembly |
| BepInEx-shape (references BepInEx/UnityEngine/0Harmony) | REFUSED — named missing refs; shim design documented, not shipped |
| MelonLoader-shape | REFUSED — same discipline |
| .NET 5/6+/netstandard-only | REFUSED — runtime is Mono 4.x profile |
| Mixed-mode managed/native (vtable fixups, native exports) | REFUSED — mixed support not demonstrated |
| Declares non-`__Internal` P/Invoke (kernel32, game DLL) | REFUSED — named import; precise, at admission |
| Declares undeclared `__Internal` import | REFUSED — named entry point |
| Spawns managed threads / uses System.Core / file I/O | REFUSED at admission (ImplMap/BCL closure) or fails precise at runtime — file I/O needs target libmono-native first |
| "Unload" | QUIESCE only — assembly stays resident (§3d) |

---

## 8. Open questions

1. `Assembly.GetAssemblies()` name-matching vs `Assembly.Load(byte[])` for
   natively registered images under matching Mono 6.12 — settled empirically at
   rung M2; both paths staged.
2. `mono_dllmap_insert` NULL-func catch-all semantics — v1 avoids it (per-symbol
   inserts only, the proven cap10 form); pin from `mono/metadata/assembly.c`
   before anyone "simplifies" it.
3. Whether any real mscorlib-only mod exists in the wild for ANY UE game —
   integration-time search task (expected answer: no; labeled-fixture path per
   §4(f)(i)).
4. Thread-attach gate for mods that legitimately need threads (config loaders,
   network) — post-v1, after the target port's thread story is proven.
5. Facade assemblies (netstandard) under this Mono — never probed here; treated
   as refuse until measured.

## 9. Staleness register ([TRAINING-KNOWLEDGE] items)

| Item | Claim used | Staleness | Pin action |
|---|---|---|---|
| BepInEx 5 architecture (BaseUnityPlugin, BepInPlugin, chainloader/Awake, HarmonyX as 0Harmony) | §1 table | LOW-MED (stable since ~2020, project alive) | vendor-pin BepInEx source + license receipt before any shim work |
| BepInEx 6 IL2CPP line shape (Il2CppInterop, net6) | §1 table | MED (bleeding-edge churn) | same pin |
| MelonLoader architecture (MelonInfo/MelonGame attrs, MelonMod, OnInitializeMelon/OnUpdate) | §1 table | MED | same pin |
| UE4SS has no managed plugin API (Lua + native C++ only) | §1 headline | LOW (stable design; corroborated by in-tree mission) | confirm against pinned UE4SS docs at vendor time |
| UnityEngine modularization (CoreModule since 2017.2) | §1, refusal text | LOW | pin from a real mod's reference list at classification-test time |
| Mono embedded per-assembly unload unsafety | §3d | MED (API churn post dotnet merge) | verify against pinned mono commit 0cbf0e290c3 sources |
| .NET 4.x corlib contents (Generic collections/reflection in mscorlib; LINQ in System.Core) | §3a constraint | LOW | compile check at M1 — a stray System.Core ref fails the mscorlib-only preflight loudly |
| Legacy .NET Framework profiles of BepInEx 5/MelonLoader (net35/net4x) | §1a | MED | pin exact TFM from vendored framework at shim time |

— agent JI (ZCode), 2026-10-13. Design + staged source only: no builds, no
console contact, no main-tree edits, no cap10 edits.
