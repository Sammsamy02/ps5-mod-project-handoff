# agent-HE — CAP10 C# RUNTIME design (report)

2026-10-08, design-only packet (Mac on battery: reads + docs only; no
builds/console/network). Deliverable written:
`outputs/2026-10-08-l25-prep/CAP10-CSHARP-DESIGN.md`. Context read first:
GH OTHER-ENGINES-RESEARCH (BepInEx §2), GD PHASEB-PLAN + GP CORRECTIONS,
kstuff-lite-v1.11-VERIFIED §#1/§#3, GV capability relabels + the #3
hook-kind inventory, CLAUDE-CAPABILITY-STATUS, roadmap-technical-audit
Phase 7, ORIGINAL-MOD-IDENTITY + shader-fallback laws.

## Model comparison verdict

- **(b) INTERPRETER = the v1.** Mono's MIT interpreter executes IL with NO
  generated native code — it dissolves the W^X problem entirely (iOS
  existence proof already pinned in the audit). Mod `.dll` runs
  byte-identical (best law fit), reflection works because the runtime is
  real, hosting bill stated honestly: Boehm-class GC (non-moving → no
  pinning problem across the interop boundary), pthreads (payload-proven),
  DllMap-routed P/Invoke with fail-closed imports, signal surface flagged
  as an honest unknown, IO via the mod-root pattern. Cost: research-scale
  Mono-Prospero port (no public console Mono exists — the only
  PlayStation Mono builds are Unity-internal NDA ones we may not touch).
- **(a3) IL→C++ INSTALL-TIME TRANSPILE = designated perf path post-v1.**
  The "IL2CPP inverse": transpile the mod against generated interop
  headers with the in-tree proven prospero-clang (sdk-v0.43). Most
  licensable AOT route (all MIT/own code), but subset-only — must refuse
  Reflection.Emit/dynamic/expression trees (the exact constructs the audit
  flagged in Il2CppInterop). Reusing Unity's own il2cpp tooling: rejected
  (proprietary, EULA risk, wrong output target).
- **(a1) Mono AOT: not independently viable** — Mono AOT is not a
  free-standing cross-compiler; it needs a Mono runtime ported to the
  target underneath, so it reduces to "(b) first, then AOT as its perf
  tier". monolite corrected: bootstrap compiler, not an AOT tool.
- **(a2) .NET NativeAOT: REFUSED — licensing wall.** A Prospero PAL needs
  Sony-SDK system libs/ABI material (project has no PS5 license;
  never-fabricate law; same class as the refused shader Option B). Also
  assumes an EXE apphost, not a PRX.
- **(c) JIT WITH KERNEL HELP: design-only, gated, possibly impossible.**
  Plain `mprotect` is a stock syscall (74, hooked-passthrough in v1.11) —
  calling it needs no kernel residency, and kekcall-5 REMOTE_SYSCALL
  mprotect hits the SAME kernel policy. What Phase B actually adds:
  kekcall-5 remote mmap RW→write→verify→mprotect RX (upstream pattern,
  proven against SceShellCore only — NOT game-pid-proven) and
  SYS_mdbg_call 573 debugger-writes. Whether a game process may X-flag
  anonymous memory at all is an honest unknown; if policy says no, only L4
  (new kernel-resident code — PROHIBITED) would help. Fallback that
  always works: the interpreter. One cheap J0 probe rung proposed.

## The IL2CPP mismatch (P-BIND) — the real path

Two independent problems: executing IL (P-EXEC, above) AND resolving the
mod's bindings to types that exist on console only as native code +
global-metadata.dat (P-BIND — needed by ALL three models). Real path =
install-time interop proxies: decrypt-law dump of the owned game's
metadata + AOB anchors for il2cpp_* internals (capability #2's cousin),
generating (i) shim assemblies for the interpreter (Il2CppInterop
transplanted to install time — law-3 compliant), or (ii) C++ headers for
the transpile path, plus a pointer-vs-direct call-site census per method.

## Harmony mapping to #3

Proven half (pointer/table swaps, console-receipted family):
MethodInfo method-pointer swap = the exact IL2CPP analog of our
console-proven UFunction Func-field swap (data-member write, W^X-free) —
covers pointer-dispatched/invoker/virtual calls; transpilers impossible
by construction (no IL on console); prefix/postfix wrappers ride the
pointer swap. Design-only half: inline prologue detours for direct-call
methods (PLH class, soft-refused today; eboot text EXEC-only in-process)
= Phase B L3 mdbg channel, upstream RW→write→verify→RX pattern,
revert-first. Mod API exposes the proven half at v1 with per-method
capability flags from the dump.

## Dependencies / test / unlocks

Deps: #2 MUST (anchors+metadata), #3 MUST (patching), #4 timing-solved by
ploader (per-game verify vs il2cpp_init), #7 MUST-lite (staging), #8 HELPS
(assets; mixed-package single-verdict law), #6/#5 HELPS, #1 only for
route (c)/J0. First console test = rung C5: five `[CS]` markers
(boot/load-with-sha/hello/exit + clean klog), read-mostly, self-authored
fixture, PASS = all markers + game reaches menu; DONE = 3/3 boots
(3-restart law); pre-declared fail modes, no blind relaunch. Unlocks:
BepInEx-IL2CPP + MelonLoader-IL2CPP mod classes (per-corpus honesty),
Unity-title generality via the dump pipeline; BepInEx-5/Mono mods stay
BLOCKED-class (bind PC-only real assemblies; user-PC-copy route is an
open law question); SHVDN late (needs clean-room natives invoker too).

## Licensing walls found (the asked-for summary)

1. **Sony-SDK wall**: .NET NativeAOT platform support (and any
   Sony-ABI-complete runtime targeting) — hard refuse under the
   no-Sony-SDK/never-fabricate laws.
2. **NDA-console-Mono wall**: the only proven PlayStation Mono runtimes
   are Unity-internal NDA builds — unusable; we must port MIT Mono
   ourselves via the open payload SDK only.
3. **Unity-tooling wall**: Unity's own il2cpp compiler cannot be shipped
   in the installer (proprietary/EULA) — own transpiler subset instead.
4. All ecosystem licenses (BepInEx/MelonLoader/SHVDN/HarmonyX/
   Il2CppInterop/dumpers) are [TRAINING-KNOWLEDGE] and unpinned — C0
   license-table phase is blocking before any build; staleness register
   (9 items, mono-project status highest) written into the design doc.

No builds, no console contact, no network, no source-tree edits
performed. Only writes: CAP10-CSHARP-DESIGN.md and this report.
