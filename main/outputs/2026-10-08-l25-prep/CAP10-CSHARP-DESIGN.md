# CAP10 — C# RUNTIME ON PS5 (design only)

Agent HE (ZCode), 2026-10-08 prep packet. User-added capability #10 (priority
after #9): run .NET mod assemblies (BepInEx / MelonLoader /
ScriptHookVDotNet classes) on PS5. **DESIGN ONLY** — Mac on battery: no
builds, no console contact, no network. Everything version-specific below is
training-knowledge and carries a staleness flag; nothing in this document
authorizes any console action.

Evidence labels:
- **[LOCAL-SOURCE]** — from files in this worktree (paths cited).
- **[TRAINING-KNOWLEDGE]** — from my training data; this domain (Mono/.NET/
  IL2CPP tooling) moves fast and several projects were relicensed or archived
  after my cutoff. Every such fact must be pinned from a primary source
  (vendored repo, same discipline as UE4SS) before code depends on it. A
  staleness register is at §9.
- **[INFERENCE]** — my reasoning on top of the above.

Laws inherited (binding on every route below) [LOCAL-SOURCE:
shader-fallback/fallback.md §"never-fabricate"/Option B refusal;
ORIGINAL-MOD-IDENTITY.md; PHASEB-PLAN.md §0]:
1. **No Sony SDK material.** The only proven Mono-on-PlayStation builds are
   Unity-internal, NDA/Sony-licensed console runtimes we may not use or
   reimplement from. Homebrew route = the already-pinned open payload SDK
   (`outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk`,
   prospero-clang; every l26 PRX is built with it) — reuse only that.
2. **Mod files byte-identical.** Original PC mod files are never modified
   (the Townfall acceptance-proof law, ORIGINAL-MOD-IDENTITY.md §3).
3. **Converted output generated at install.** Any per-mod transformation is
   a DERIVED artifact produced by the installer (analyzer emits directives;
   builder-side rewrite; originals untouched; revert = row removal) — same
   shape as the #8 material-fallback overlay entries
   [LOCAL-SOURCE: fallback.md Part 2 law].
4. Plus the standing console laws: 3-restart acceptance, one user-launched
   test per boot, no rapid-fire launches, klog to NEW files, decrypt-off
   executable fetches, no modified-executable upload
   [LOCAL-SOURCE: PHASEB-PLAN §0/§4, AGENTS PS5 rules].

---

## 0. THE ONE FACT THAT DEFINES THE PROBLEM (restate of GH's blocker)

PS5 console Unity titles build with the **IL2CPP scripting backend**: the
game's C# is AOT-transpiled to C++ and compiled to native code inside the
stripped eboot; `Assembly-CSharp.dll` and the whole managed surface do not
exist on console, there is **no Mono and no CoreCLR in a PS5 Unity game,
and PS5 userland has no JIT** (W^X) [LOCAL-SOURCE: OTHER-ENGINES-RESEARCH.md
§2b "Off-table blocker", restating 2026-09-30-roadmap-technical-audit.md
Phase 7; TRAINING-KNOWLEDGE on the Unity console requirement — flagged by GH
as long-standing, low staleness risk, re-verify against Unity docs].

Therefore "run PC C# mods" on PS5 decomposes into TWO independent problems:

- **P-EXEC**: execute IL (the mod's `.dll`) on a machine with no managed
  runtime — solved by (a) AOT-at-install, (b) an interpreter, or (c) JIT.
- **P-BIND**: the mod's compiled bindings against `Assembly-CSharp.dll`
  etc. reference types that only exist on console as IL2CPP-native code +
  `global-metadata.dat` — solved by the interop-proxy layer (§2), which is
  needed for ALL THREE execution models. P-BIND is the harder, more certain
  work; P-EXEC is a choice.

GH's honest scope verdict stands and is the anchor of this design: running
actual PC BepInEx plugins as-is is BLOCKED-class today; the honest PS5 scope
is "our own native IL2CPP interop" with a plugin-surface subset
[LOCAL-SOURCE: OTHER-ENGINES-RESEARCH §2c]. The local audit's revised
milestone is adopted verbatim: **"one exact plugin and all dependencies
execute on PS5 using the declared interpreter/interop subset, with GC,
callbacks, error paths and performance checked"** [LOCAL-SOURCE:
2026-09-30-roadmap-technical-audit.md Phase 7].

---

## 1. THE THREE EXECUTION MODELS (P-EXEC)

### 1a. AOT-AT-INSTALL — cross-compile the mod assembly to native at install

**What it is:** the installer (on the Mac/host, at mod-install time, per
law 3) transforms the mod's IL into a native PS5 artifact; the console only
ever loads pre-compiled code. Three candidate toolchains:

**(a1) Mono AOT.** Mono's AOT compiler emits native object code for a
target; full-AOT mode (the iOS mode: no JIT fallback at load) is the console
shape. License MIT ✓ [TRAINING-KNOWLEDGE — very stable fact]. BUT: Mono's
AOT is not a free-standing cross-compiler. It is the Mono runtime's own
code generator invoked in AOT mode, and the emitted objects still link
against a ported Mono runtime (runtime helpers, GC, libmono, eglib syscall
layer) at load time. To target Prospero you would need (i) a Mono runtime
host that embeds code generators for the Prospero x86-64/FreeBSD-flavored
ELF triple and PRX relocation model, and (ii) that same runtime ported to
Prospero to sit under the AOT images — i.e. **the "port Mono to PS5" wall
first, then more object-format work on top** [TRAINING-KNOWLEDGE, flag:
Mono AOT cross-compilation historically exists only for targets Mono was
ported to — iOS/Android/WASM; staleness: high, the Mono AOT docs and flags
moved with the dotnet/runtime merge]. Terminology correction from the task
brief: *monolite* is not an AOT cross-compiler — it is the minimal
bootstrap C# compiler subset used to build Mono itself [TRAINING-KNOWLEDGE,
flag medium]. **Can it target prospero-clang objects? Not as-is. Verdict:
not the v1; revisit only if a Mono-Prospero port (b) already exists, at
which point AOT becomes the perf tier of that same port.**

**(a2) .NET NativeAOT.** Upstream NativeAOT (CoreRT lineage, in
dotnet/runtime since .NET 7/8) compiles whole programs to self-contained
native code [TRAINING-KNOWLEDGE, flag: platform list moves per release;
freebsd-x64 RID support is partial/community — verify]. Two walls:
- **LICENSING WALL (hard, same class as the shader Option B refusal):**
  building a Prospero PAL/platform target means linking against Sony PS5
  system libraries and ABI definitions distributed only in the licensed PS5
  SDK [INFERENCE from LOCAL-SOURCE: the project has no PS5 SDK license and
  the never-fabricate law bars synthesizing or sourcing Sony-SDK-gated
  material — fallback.md Option B is the precedent refusal]. NativeAOT's
  linker/apphost model also assumes a normal executable, not a PRX module.
- **Runtime-shape mismatch:** NativeAOT hosts the app; we need the GAME's
  IL2CPP runtime to be the object model the mod binds. NativeAOT gives us
  a second, parallel .NET world that still cannot see the game's types
  without §2's proxy layer anyway.
**Verdict: refused for v1; likely refused permanently under law 1 unless a
fully-clean, SDK-free platform port emerges. Assess as a licensing wall,
not a work item.**

**(a3) C#-to-C++ transpile layer — "IL2CPP's own tooling, inverse."**
At install, transpile the mod's IL to C++ against our generated interop
layer (§2), compile with the pinned prospero-clang into a derived module,
load it like any plugin of ours. This is literally the IL2CPP idea applied
to the mod instead of the game (Unity's il2cpp: IL→C++→platform compile).
Two sub-routes:
- *Reuse Unity's own il2cpp tooling:* rejected — it is Unity-proprietary,
  emits game-runtime-coupled C++ against Unity's internal headers, and
  shipping it inside our installer raises Unity EULA questions we don't
  need [TRAINING-KNOWLEDGE + INFERENCE; flag: verify Unity tooling terms if
  ever revisited]. 
- *Own transpiler subset (the licensable path):* everything is MIT/our
  code; the toolchain is already proven in-tree (every l26 PRX is
  prospero-clang output). The cost is subset discipline: a transpiler must
  refuse what it cannot model — `Reflection.Emit` (IL emitted at runtime),
  expression trees, `dynamic`, Marshal-by-ref edge cases, LINQ-over-`Expression`
  — exactly the constructs the local audit flagged in Il2CppInterop's class
  injector (generated `DynamicMethod` IL!) [LOCAL-SOURCE:
  roadmap-technical-audit Phase 7]. **Verdict: the most licensable AOT
  route; NOT the v1 (subset breadth too thin for real-world mod corpora);
  the designated perf path for hot mods once the interop layer exists.**

**AOT-at-install and law 2/3 fit:** derived native artifact generated at
install, original `.dll` untouched in the package — clean fit with the
existing conversion-pipeline shape (#8's model).

### 1b. INTERPRETER — IL interpreter inside our PRX

**Mono's interpreter** (modern lineage in the Mono runtime,
`mini/interp/`; historically nicknamed "mint" — note "mint" today properly
refers to the modern interpreter's core, not the 2005-era standalone one
[TRAINING-KNOWLEDGE, flag high: interpreter location/flags moved across the
dotnet/runtime merge; pin the exact source tree at vendoring time]).
License MIT ✓. The decisive property: **an interpreter executes IL without
generating native code — it needs NO executable anonymous memory at all**,
which dissolves the W^X problem instead of fighting it. Existence proof in
a no-JIT environment: iOS ships the Mono interpreter precisely to avoid
JIT dependence [LOCAL-SOURCE: roadmap-technical-audit Phase 7, citing
Microsoft's iOS-interpreter docs; TRAINING-KNOWLEDGE for mechanics].

**Hosting requirements (the honest bill):**
- **GC**: Mono's two collectors are Boehm (conservative, non-moving) and
  sgen (generational, moving, concurrent) [TRAINING-KNOWLEDGE, stable].
  v1 pick: Boehm-class — conservative GC never moves objects, which
  collapses the pinning/lifetime problem across the native interop
  boundary (raw IL2CPP object pointers held by managed code stay valid).
  Unity's own embedded Mono used Boehm — the precedent shape.
- **Threads**: runtime needs at least a finalizer thread plus
  attach/detach of any native thread that calls into managed code. The
  payload environment has threads (PS5PadSource's ~5 ms poll loop and the
  UE4SS update thread are proven in-process [LOCAL-SOURCE:
  PHASEB-PLAN §3.3]) — pthread surface exists in the payload SDK.
- **P/Invoke surface**: mods `DllImport` OS/game functions. On console the
  supported surface is OUR interop layer + payload libc; Windows-targeting
  imports (kernel32 etc., common in SHVDN mods) do not exist. Route
  resolution through a Mono DllMap/host hook so unsupported imports
  fail-closed per import, not per process.
- **Signals**: Unix Mono uses signals for GC stop-the-world and (JIT mode)
  hardware null-checks; interpreter mode does its own null checking, and
  Boehm serialized reduces stop-the-world machinery — but `sigaction`
  availability/behavior inside a payload in a game process is an **honest
  unknown** (nothing in-tree proves it either way). Mitigation ladder:
  interpreter + Boehm serialized first; probe signal surface as a gated
  self-test rung before enabling anything fancier. [INFERENCE +
  TRAINING-KNOWLEDGE]
- **IO**: config/save reads from mods route through the mod root
  (NativeModSource logical-key pattern, #7) [LOCAL-SOURCE:
  OTHER-ENGINES-RESEARCH §1c(4)].
- **Perf cost**: interpreter overhead is typically a small integer factor
  vs JIT [TRAINING-KNOWLEDGE, flag: numbers vary wildly by workload; no
  PS5 number can exist yet]. Mod code is mostly event handlers and glue —
  the perf-sensitive minority is what route (a3) exists for.

**Verdict: the v1.** Broadest compatibility (reflection over metadata
works because the runtime is real), law 2 fit is perfect (the mod `.dll`
runs byte-identical, zero per-mod conversion), no W^X exposure, MIT-clean.
Cost: a research-scale Mono-Prospero port (no public PS5 Mono port exists —
console ports were always NDA-gated; we write our own from MIT sources
under law 1) [TRAINING-KNOWLEDGE, flag low: the "no public console Mono"
fact is stable].

### 1c. JIT WITH KERNEL HELP — generate + execute native code at runtime

**What the user's framing means concretely.** A JIT needs to (1) write
machine code into memory and (2) have that memory executable — on a W^X
system, not simultaneously: the canonical dance is map RW → emit →
**mprotect RX** → execute. Three PS5-specific questions, answered honestly:

- **Can our PRX just mmap RWX or mprotect anonymous→X in-process?**
  HONEST UNKNOWN — no receipt in-tree either way [LOCAL-SOURCE: VMMAP
  evidence covers eboot text being EXEC-ONLY to the in-process runtime,
  not anonymous-page policy]. Prior expectation is negative-to-mixed:
  console kernels generally forbid writable+executable mappings and often
  forbid newly-executable anonymous memory for game-certified processes —
  the entire reason console Unity mandates IL2CPP AOT
  [TRAINING-KNOWLEDGE + INFERENCE; verify empirically at probe time, on
  the battery of the #1 ladder, never by guessing].
- **Is userspace mprotect even kernel-mediated?** Calling `mprotect` (74)
  is a stock syscall — no kstuff residency needed to CALL it; note v1.11
  hooks mprotect 74 but passes it through with fixups, so the original
  executes [LOCAL-SOURCE: kstuff-lite-v1.11-VERIFIED.md §#1 item 3]. The
  policy enforcement lives INSIDE the kernel's vm layer: a plain syscall
  and a kekcall-mediated one hit the SAME policy.
- **What does Phase B actually add, then?** Exactly two privileges, both
  from the verified v1.11 surface [LOCAL-SOURCE: kstuff-lite-v1.11-VERIFIED
  §#3, GP CORRECTIONS]:
  1. **kekcall 5 REMOTE_SYSCALL mprotect in the game pid** — and since our
     UE4SS runtime is ploader-injected INTO the game process, the "target
     pid" is our own. Upstream kstuff's proven pattern is: remote
     `mmap RW` → write blob → verify readback → remote `mprotect R+X`
     (`main.c:1056-1083`), with the target-side mprotect handling TLB
     permission coherence (x86-64: i-cache coherent, no invalidation
     needed anywhere in kstuff's source).
     CAVEAT (must be stated): upstream proved this **against SceShellCore
     (a system process)**, not against a game pid — whether a game
     process's vm policy accepts X-flips on anonymous memory is unproven
     [LOCAL-SOURCE for the pattern + INFERENCE for the caveat].
  2. **`SYS_mdbg_call 573`** — the kernel debugger write channel (hooked-
     but-passthrough, callable). This is a debugger-class WriteProcessMemory:
     it can write into pages the writing process cannot write (e.g. RX
     scratch), which is the one true privilege a plain payload lacks.
  Anything beyond these two — page-table manipulation, PTE forging, policy
  patching — is **new kernel-resident code = L4 = PROHIBITED this phase**
  [LOCAL-SOURCE: PHASEB-PLAN §4.1, GP].

**Plausible kernel-helped JIT design (design-only, gated behind Phase B
L3):** at mod-load time, reserve one code-cache region via in-process
`mmap RW` (fallback: kekcall-remote mmap); emit interpreter-tier trampolines
or (a3)-compiled hot fragments; flip `mprotect RX` in-process; if the
in-process flip is refused, retry the flip via kekcall 5 REMOTE_SYSCALL; if
THAT is refused (policy, not permissions — remote mprotect runs the same
kernel code), the JIT tier is **reported BLOCKED, not forced** (no L4
bypass, same refusal discipline as every soft-refuse in the port). Fallback
that always works: stay on the interpreter — which is why (b) is v1.

**Verdict: not v1; not even v2. Design preserved; single honest probe rung
(§7 rung J0) could settle the W^X question early if ever worth it.**

### 1d. Comparison table

| | (a1) Mono AOT | (a2) .NET NativeAOT | (a3) IL→C++ transpile | (b) Interpreter | (c) JIT + kernel help |
|---|---|---|---|---|---|
| Runs real mod `.dll` unchanged | no (derived artifact) | no | no (derived artifact) | **YES (byte-identical)** | yes |
| Needs Mono-on-PS5 port | yes (runtime under images) | n/a | no | yes (runtime) | partial (codegen only) |
| Licensing | MIT ✓ | **WALL: Sony-SDK PAL + never-fabricate** | **MIT/own-code ✓ (cleanest)** | MIT ✓ | MIT ✓ (uses Phase B channels) |
| W^X exposure | none | none | none | **none** | **the whole problem** |
| Compatibility breadth | runtime-limited | runtime-limited | **subset-only (must refuse Reflection.Emit/dynamic/expr-trees)** | **broadest (real runtime)** | broadest in theory |
| Perf | native | native | native | interpreter tax (small-integer factor) | native |
| Per-mod install conversion | yes | yes | yes | **no** | no |
| Status in this design | deferred perf tier | **refused (licensing wall)** | designated perf path (post-v1) | **THE V1** | design-only, gated on Phase B L3 |

---

## 2. THE IL2CPP MISMATCH AND THE INTEROP-PROXY LAYER (P-BIND)

**The mismatch, precisely.** A PC Unity mod is a managed assembly compiled
against the PC build's managed assemblies — `Assembly-CSharp.dll`,
`UnityEngine.*.dll` — types that exist as IL. On the PS5 build those types
exist only as native C++ symbols plus **`global-metadata.dat`** (the type/
method/field/string-literal database IL2CPP needs at runtime) inside the
game's PFS container. Loading the mod under any execution model still
leaves every `ldtoken Assembly-CSharp.dll!SomeType` unresolved. This is
IL2CPP-inversion: on PC, BepInEx's IL2CPP line regenerates proxy assemblies
from the game's metadata so mod bindings resolve
[TRAINING-KNOWLEDGE + LOCAL-SOURCE: OTHER-ENGINES-RESEARCH §2a; roadmap-
technical-audit Phase 7].

**The real path: install-time proxies against the game's own dump.**

1. **Input (capability #2's scanner cousin):** for each owned title+patch,
   the installer acquires (user-owned copy, decrypt-off law for executables)
   and analyzes:
   - `global-metadata.dat` — magic/version sanity, type/method/field
     tables, string literals. This is name-table work — the direct cousin
     of our FName pool/seam history (construct with Find-not-Add, index
     verification) [INFERENCE from LOCAL-SOURCE seam lessons,
     OTHER-ENGINES-RESEARCH §2c].
   - the IL2CPP native image (inside the stripped eboot) — locate
     `il2cpp_init`/`il2cpp_domain_get`/`il2cpp_class_from_name`/method-
     resolution entry points by AOB anchors (PS5 IL2CPP games export
     nothing; everything is internal to the stripped eboot — GH's #2 MUST
     row), plus `CodeRegistration`/`MetadataRegistration` tables
     [TRAINING-KNOWLEDGE for the table names — stable since mid-2010s
     IL2CPP; flag medium; the anchor set itself is per-Unity-version and
     must be pinned per game].
   Existing tooling class to evaluate for the Mac-side analyzer (vendoring
   candidates, all license-verify-at-pin): Il2CppDumper, Cpp2IL,
   Il2CppAssemblyUnhozer-class regenerators [TRAINING-KNOWLEDGE, flag high:
   projects move/relicense; none vendored in-tree yet — GH surveyed and
   found no local copies].
2. **Output (generated AT INSTALL, law 3):**
   - for route (b) interpreter: **shim assemblies** — the proxy
     `Assembly-CSharp.dll`-equivalents regenerated from the dump, loaded by
     our Mono host so the mod's existing bindings resolve by name+signature
     (this is exactly what Il2CppInterop generates on PC, transplanted to
     install time [INFERENCE from LOCAL-SOURCE audit description of
     Il2CppInterop]). Runtime side: calls into shim types marshal through
     a native bridge (fixed C ABI → `MethodInfo` pointers) into the game's
     real functions. Delegate fn-ptrs, GC handles and native callbacks are
     the audit-flagged hard subset — declared, versioned, tested per
     feature, never assumed [LOCAL-SOURCE: roadmap-technical-audit Phase 7].
   - for route (a3) transpile: **C++ headers/stubs** against the same dump
     (one header set per game+patch, content-addressed like the offset
     profiles).
   - plus a **function-pin table** (AOB anchors → resolved addresses) fed
     through the existing profile/anchor machinery and its refusal
     discipline (the Astro-Bot-class clean refuse is the admission gate for
     a new Unity title) [LOCAL-SOURCE: agent-FZ precedent,
     OTHER-ENGINES-RESEARCH §1c(3)].
3. **The honest subset statement (audit law, restated):** "most IL2CPP PC
   mods work" is a corpus question, not a consequence of metadata
   availability. Supported = named, per-feature, receipt-tested; everything
   else refuses at install with the missing capability named
   [LOCAL-SOURCE: roadmap-technical-audit Phase 7 + Coverage section].

---

## 3. HARMONY-STYLE PATCHING, MAPPED TO #3's PROVEN AND DESIGN-ONLY HALVES

On PC-IL2CPP, HarmonyX-style patching of a game method is a **native
detour on the compiled method** (BepInEx's IL2CPP line patches method
pointers via a platform detour provider) [TRAINING-KNOWLEDGE, flag medium;
LOCAL-SOURCE: audit "native Harmony-style detours are not automatically
the managed-Harmony patching contract"]. Two further scoping facts from
the audit: **transpilers (IL rewriting) are impossible on console by
construction** — the method body is native code, there is no IL to rewrite;
prefix/postfix/finalizer wrappers around the original native body are the
only meaningful subset [INFERENCE from LOCAL-SOURCE audit + TRAINING-
KNOWLEDGE]. Now the map onto GV's #3 inventory [LOCAL-SOURCE:
CLAUDE-CAPABILITY-STATUS "#3 hook-kind inventory"; agent-GV-capability-
relabels.md §3]:

**Proven-class on console (pointer/table/data writes — the exact family
already receipted for UE):**

- **`MethodInfo` method-pointer swap** = the IL2CPP analog of our
  console-proven **UFunction `Func`-pointer swap** (RegisterPreHook/
  PostHook → save original `Func` → swap the field → engine dispatch calls
  the hook → restore on unhook; pure data-member write in a live object).
  In IL2CPP, calls that go through `MethodInfo`/invoker tables/virtual
  method-table slots load a function pointer from writable data and
  indirect-call it — swapping that pointer is the same W^X-free class.
  Requires per-Unity-version `MethodInfo` layout knowledge from the §2
  dump (offset law: layouts from the per-game profile, never hardcoded).
  [TRAINING-KNOWLEDGE for the IL2CPP side + LOCAL-SOURCE for the proven
  UE-side analog]
- **Listener/registration surfaces** (where a Unity subsystem exposes its
  own callback registration, analogous to our GUObjectArray listener
  admits) — per-game, profile-driven.
- **GOT/data-pointer redirection** of engine-internal call targets where
  the dump shows a data indirection.

**Design-only-class (instruction patching — the #3 half that soft-refuses
today):**

- **Inline detours** (write a jump into a compiled method prologue — the
  PLH x64Detour class) for hot-path methods called by *direct* call sites
  that never consult a swappable pointer. Today every live game-text touch
  is mediated by the owned-memory registry and refuses; eboot text is
  EXEC-ONLY in-process. Enabling = **Phase B L3**: the `SYS_mdbg_call 573`
  write channel with write→verify-readback→target-side-`mprotect` receipts
  (the upstream RW→write→verify→RX pattern, §1c), revert-first, behind the
  §4 ladder [LOCAL-SOURCE: CLAUDE-CAPABILITY-STATUS #3 row + inventory;
  PHASEB-PLAN §3.2; kstuff-lite-v1.11-VERIFIED §#3].
- **Managed-behavior patching on top** (Harmony prefix/postfix semantics)
  is implemented as a wrapper function invoked via the pointer-swap half —
  i.e. the mod-visible Harmony-subset API rides the PROVEN half wherever
  the method is pointer-dispatched, and is honestly "unsupported for this
  method" where only direct calls exist and L3 is not yet opened.

**Net:** the mod-facing patch API must expose exactly the proven half at
v1 (pointer-swap detours + registration surfaces), with per-method
capability flags from the dump (pointer-dispatched vs direct-call site
census — a static-analysis step in the §2 installer, cousin of the
SeedTable LEA censuses already in-tree [LOCAL-SOURCE: capability row #2
evidence]). Instruction-detour coverage is an explicit Phase-B-gated
upgrade, never silently assumed.

---

## 4. RECOMMENDATION + ROADMAP

**Recommendation: (b) interpreter for v1; (a3) install-time transpile as
the designated perf path post-v1; (a2) refused at the licensing wall;
(a1) folds into (b) as a later tier of the same Mono port; (c) kept as
design-only with one cheap probe rung.** Bridge product while the C#
runtime matures: the "UE4SS-for-Unity" Lua-over-IL2CPP surface (GH's
near-term BepInEx verdict — Lua hosting is console-proven machinery,
gives script mods immediately, and the §2 dump work is shared 100%)
[LOCAL-SOURCE: OTHER-ENGINES-RESEARCH §2c "New work" item 2].

Roadmap (each phase gated per the standing laws; NOTHING runs without
Claude review + explicit user OK):

- **C0. Pin + license table (Mac, network-free until fetch-authorized).**
  Vendor candidates: Mono source (choose between the dotnet/runtime mono
  component and Unity's mono fork — both MIT-class, but pin which is
  alive and buildable standalone; see staleness register), Il2CppInterop,
  one dumper (Il2CppDumper/Cpp2IL class). Write the license table with
  primary-source receipts before any code. License facts I assert here are
  TRAINING-KNOWLEDGE and MUST be re-verified: Mono=MIT (stable),
  Il2CppInterop=MIT?, BepInEx=LGPL-family?, HarmonyX=MIT?,
  MelonLoader=?, SHVDN=?, Il2CppDumper=? — every "?" is a pin-time task.
- **C1. IL2CPP dump pipeline (Mac-only, uses owned-game files).** Metadata
  parser + eboot AOB anchor pack + proxy/shim generator + pointer-vs-
  direct call-site census, all emitting install-time artifacts (law 3).
  Reuse the profile/anchor/refusal machinery. Deliverable: for one owned
  PS5 Unity title, a generated profile that the existing classifier either
  pins or cleanly refuses.
- **C2. Mac host harness.** Mono interpreter build for macOS; run the
  self-authored test plugin against shim assemblies from C1 fixtures;
  exercise the audit checklist (GC, callbacks, error paths, perf) in
  simulation.
- **C3. FIRST BUILD ACTION (specified now, run when on power — battery law
  bars builds today):** a minimal Mono-interpreter PRX for PS5 compiled by
  the pinned sdk-v0.43 prospero-clang — hello-world shape: interpreter
  boots inside the payload, loads ONE self-authored test assembly from the
  mod root, executes `Main` that emits a single `[CS]` marker line, exits.
  No game interop, no hooks, no proxy layer yet. Success on Mac-cross-
  build = PRX links, markers present, size sane. This is the gate that
  proves the research-scale port is tractable before any console minute is
  spent. (Sub-rungs if the full runtime fights back: static-lib-only
  eglib, Boehm serialized, no-signal config, single-threaded attach —
  the interpreter's console-mode configuration surface.)
- **C4. Console rung 1: IL2CPP precondition probes** (GH's BP1–BP6,
  unchanged — native-only, read-only, no managed runtime: domain_get,
  assemblies_count, class_from_name "System.String" round-trip, benign
  method resolution, metadata magic sanity; PASS = 4/5 core) [LOCAL-SOURCE:
  OTHER-ENGINES-RESEARCH §2d].
- **C5. Console rung 2: interpreter hello in-game** (§7 below — the first
  real CAP10 console test).
- **C6. Proxy round-trip:** test plugin resolves one dumped game type,
  calls one benign getter through the shim, unsubscribes, reverts.
- **C7. Harmony subset v1:** pointer-swap detour on one pointer-dispatched
  game method (prefix counter + restore; the BP7 shape, now on the proven
  #3 half).
- **C8. Perf tier:** (a3) transpiler pilot for ONE hot mod; compare
  against interpreter receipts.
- **C9 (optional, cheap):** W^X probe rung J0 (§1c questions) folded into
  any authorized Phase B L2 session — knowledge only.

---

## 5. DEPENDENCIES ON CAPABILITIES 1–8 (explicit)

| # | Dependency | Why |
|---|---|---|
| 1 kernel reads | OPTIONAL for v1 (everything is in-process); REQUIRED-class only for route (c) JIT (mdbg 573 / remote mprotect are Phase B L2/L3 channels) and for the J0 probe. | kstuff-lite REMOTE_SYSCALL + mdbg are the only "kernel help" that exists [LOCAL-SOURCE: VERIFIED §#1/§#3]. |
| 2 scanner | **MUST** — il2cpp_* function anchors, metadata locator, MethodInfo layouts per game+patch; anchor-pack + refusal machinery is the admission gate per Unity title. | GH's BepInEx table #2 row [LOCAL-SOURCE]. |
| 3 code patching | **MUST** for the Harmony subset — proven pointer-swap half carries v1; instruction-detour half (Phase B L3) extends coverage to direct-call methods. | §3 map above. |
| 4 early loading | TIMING-CLASS SOLVED by ploader (process-start, pre-engine-init proven for UE; per-game verify that module_start precedes `il2cpp_init`/first scene) — same argument as GH's #4 row. | [LOCAL-SOURCE: OTHER-ENGINES-RESEARCH header note]. |
| 5 debugger | OPTIONAL dev aid — mixed native/managed stack symbolization for interpreter frames; crash-symbolization tooling already Mac-tested. | Capability row #5. |
| 6 controller | HELPS — mod menus/hotkeys via the console-proven userspace PS5Pad L3+R3 route; no kernel input needed (declined-by-design row stands). | Capability row #6. |
| 7 file overlay | MUST-lite — plugins/, config/, shim-assembly and generated-profile staging all ride the mod-root/NativeModSource pattern. | GH's #7 row + law 3. |
| 8 asset conversion | HELPS — asset-side of Unity mods (Bundles/asset formats) is separate from the C# runtime; a mod is only "working" when every component class in its package works (mixed-package single-verdict law). | roadmap-audit Coverage law. |

Also depends on the Phase B ladder staying law-abiding for anything (c):
no L4, no new kernel-resident code, 3-restart acceptance per rung
[LOCAL-SOURCE: PHASEB-PLAN §4].

---

## 6. FIRST CONSOLE TEST + PASS/FAIL (3-restart law)

**Rung C5 — the first MANAGED console test of CAP10** (C4's BP probes are
native-only and precede it; both follow the standing session laws: one
user-launched test per boot, klog tap TCP 3232 to a NEW file, saves backed
up, decrypt-off for any executable fetch, no rapid-fire relaunches).

**Setup:** one owned PS5 Unity title with a C1-generated pinned profile;
frozen interpreter PRX (C3 lineage) uploaded via the standard
upload-discipline (decrypt-off readback magic + sha match, markers
N/N in verify_build); ONE pldmgr loadpayload dispatch after confirmed exit
of any prior resident; ploader.ini armed size-exact; the self-authored
test assembly staged under the mod root (labeled as OUR fixture in the
census, like the example-lua-mod stub law — never presented as a Nexus
mod) [LOCAL-SOURCE: PHASEB-PLAN §5.1 checklist; ORIGINAL-MOD-IDENTITY
false-candidate precedent].

**The test (one launch, read-mostly):**
1. `[CS] boot` marker: interpreter init result (config: which GC, signals
   on/off, threads attached).
2. `[CS] load` marker: test assembly loaded from mod root; sha256 of the
   loaded bytes == installer-recorded sha (byte-identity receipt even for
   our own fixture — law 2 hygiene).
3. `[CS] hello` marker: `Main` executed; the marker string is written by
   managed code (proves IL execution end-to-end).
4. `[CS] exit` marker: controlled shutdown, assembly unloaded, no hooks
   armed, nothing to revert (this rung touches zero game code).
5. klog clean: no crash-watch signature (the §4.4 watch list).

**PASS (single session) = all five markers + clean klog + game reaches
menu normally after payload bound.**
**DONE (capability rung accepted) = 3/3 identical passes across 3 separate
boots** — any variance ⇒ STOP, report, no relaunch that boot
[LOCAL-SOURCE: PHASEB-PLAN §4.6; mission 3-restart law].
**Explicit fail modes to pre-declare:** marker 1 fails ⇒ port-level block
(report which subsystem: GC/threads/signals); marker 2 fails ⇒ IO/mod-root
block; marker 3 fails with a runtime fault ⇒ capture klog + symbolize
before ANY retry (never blind-replay a failed launch); any game crash ⇒
full stop + regression-launch discipline before anything else.

---

## 7. UNLOCKS (what CAP10 buys, honestly scoped)

- **BepInEx-class IL2CPP mods** (the "Bleeding Edge"/Il2CppInterop-targeting
  corpus): the realistic v1 unlock — via interpreter + install-time shim
  assemblies + pointer-swap Harmony subset. Per-corpus honesty required
  (audit law); declare per-feature support, refuse with named gaps.
- **MelonLoader-class mods for IL2CPP games**: same P-EXEC+P-BIND
  dependency (its IL2CPP line is the same interop pattern); its loader
  conveniences (console, config) map onto our proven surfaces.
  [TRAINING-KNOWLEDGE, flag: MelonLoader internals/licensing verify at pin]
- **BepInEx 5 / Mono-targeted mods**: BLOCKED-class beyond the runtime —
  they bind the PC build's *real* managed assemblies, which exist only in
  a PC copy of the game. Possible future route: user-supplied managed
  assemblies from their own PC copy at install (law question to settle
  before promising; same "user-owned inputs" class as the #8 texture
  checkpoint's matched PC-original requirement [LOCAL-SOURCE:
  roadmap-audit "Exact user-owned inputs"]).
- **ScriptHookVDotNet**: double dependency — the C# runtime AND the
  SHV-class natives invoker (per GH §5: closed-source core ⇒ clean-room
  rebuild, per-build natives DB, highest legal heat; offline-only). Late,
  if ever. Windows `DllImport` surface in SHVDN mods additionally needs
  the §1b fail-closed import shims.
- **Unity-games generality**: the real structural unlock — the §2 dump
  pipeline + interpreter host is engine-version-generic machinery: any
  owned PS5 IL2CPP Unity title with a pinned profile becomes scriptable,
  the same way the UE profile generator generalizes across UE titles
  (pin-or-refuse admission per title). This also feeds GH's ranked second
  target work (REFramework shares the reflection-driven pattern).

---

## 8. HONEST GAPS + OPEN QUESTIONS (all must close before C3 builds)

1. No Mono/IL2CPP/BepInEx-ecosystem sources vendored in-tree yet (GH
   surveyed; only audit notes exist). C0 is therefore the blocking phase.
2. Signal surface and thread-attach behavior of a payload-hosted Mono in a
   game process: no receipt either way (§1b) — C2/C3 must probe boxed.
3. PS5 game-process W^X policy on anonymous-executable memory: unknown
   (§1c); the J0 probe is cheap but console-gated.
4. MethodInfo/invoker-table layouts are per-Unity-version; the dump tool
   must version-stamp and refuse unknowns (offset law).
5. Whether upstream kstuff's remote-mmap/mprotect pattern succeeds against
   a GAME pid (only SceShellCore-proven) — affects only route (c).
6. Perf: no PS5 interpreter numbers exist; C8 receipts must replace all
   [TRAINING-KNOWLEDGE] factor guesses.
7. The mixed-package single-verdict law: a Unity mod with scripts+assets is
   only "working" when #8's asset side also works for it.

## 9. STALNESS REGISTER ([TRAINING-KNOWLEDGE] items, most-stale first)

| Item | Claim used | Staleness | Pin action |
|---|---|---|---|
| Mono project status | MIT; repo landscape shifted (mono archived/maintenance-mode; dotnet/runtime mono component; Unity's fork) | HIGH — post-cutoff churn certain | C0 vendor-pin + license receipts |
| BepInEx/MelonLoader/SHVDN/HarmonyX licenses + current architecture | per-§0/§7 labels | HIGH | C0 license table |
| Il2CppDumper/Cpp2IL existence+license | §2 tooling candidates | HIGH | C0 |
| Mono AOT cross-target mechanics + monolite role | §1a | HIGH | C0 docs pin |
| NativeAOT platform/rid list | §1a2 wall | MEDIUM | only if ever revisited |
| IL2CPP internals (CodeRegistration/MetadataRegistration, MethodInfo shape) | §2/§3 | MEDIUM (stable families, volatile layouts) | per-game dump |
| Interpreter perf factor | §1b | MEDIUM | C8 receipts |
| Unity console=IL2CPP mandate | §0 | LOW (GH double-flagged) | Unity docs at C1 |
| Mono interpreter iOS existence proof | §1b | LOW (pinned via audit's MS-doc link) | already LOCAL-SOURCE-adjacent |
| PS5 W^X/no-JIT prior | §1c | MEDIUM (prior + inference; console-specific unverified) | J0 probe |

— agent HE (ZCode), 2026-10-08. Design only: no builds, no console contact,
no network, no tree edits beyond this file and `agent-HE-cap10.md`.
