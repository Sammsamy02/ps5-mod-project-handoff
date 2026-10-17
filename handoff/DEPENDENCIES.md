# Dependencies and rebuilding

This repository is a curated source handoff, not a hermetic release build. All
compiled objects, binaries, frozen releases, full vendor downloads and console
evidence were excluded. Some old scripts use `/WORKSPACE` placeholders, original
`.superpowers` paths, or composition receipts for omitted artifacts. Those cannot
be run unchanged from a clean clone. Do not disable their validation to force a build.

## App/UI

- `main/outputs/ps5-mod-bridge/tools/bootstrap_deps.py` and native subsystem scripts
  contain dependency URLs/version checks. Retained small vendored headers include
  their available licenses. The current UI tree is separate from main native work.
- Python 3, a C++20 compiler, Node, Chromium for browser tests, and libarchive are
  host prerequisites, depending on the test. Browser paths in older tests may need
  local configuration. No new dependency downloads were required for the export.
- Prior UI handoff recorded the ordinary make dependency verifier refusing local
  libarchive metadata where it expects pinned HTTPS sources. Resolve this through
  the intended dependency recipe rather than skipping verification.

## Runtime port

- The baseline text/source subset in `legacy-port-source` was selected from port
  directories referenced by the old runtime dependency graph. Epic-marked engine
  source is excluded, as are system/build dependencies. It is not a full copy of
  the research tree. Original source selection and pin graphs need regeneration.
- UE4SS, SDK, SharpProspero, PolyHook, AsmJit/AsmTK, fmt, Zydis/Zycore and other
  upstream components have separate licenses. Retained notices are in
  `third-party-licenses` and component folders. Obtain toolchains and any missing
  upstream sources from their pinned official upstreams via the retained recipes.
- Any required Unreal engine headers/source must be obtained independently by an
  appropriately entitled developer. They were not uploaded. Game-specific inputs
  must likewise be obtained locally by the new owner; no game dumps are included.

## Mono / C#

- Resume-team `cap10-mono-build/REPRODUCE.md` and tools preserve the bootstrap and
  build method. They reference Mono commit `0cbf0e290c31adb476f9de0fa44b1d8829affa40`
  and CoreFX commit `c4eeab9fc2faa0195a812e552cd73ee298d39386`.
- Keep the sequence of source patches and provider/owner changes, re-download the
  pinned upstream trees, and regenerate configuration and evidence. Full `sources`,
  `runtime-work`, `target-work`, `cross-work` and compiled libraries were omitted.
- Mono/CoreFX licenses and local portability notices remain where present under
  the resume team. Owned `Probe.cs`/harness source is retained; its assembly is rebuilt.

## Material/shader work

Own tool/patch sources are retained. Full dxil-spirv/vkd3d-proton downloads and
game shader/assets are excluded; use their source recipes and upstream licenses
when reconstructing an experiment. The export does not claim a complete translator.

## Excluded real-mod fixtures

The installer compatibility project included a downloaded Townfall Item Spawner
Lua copy. It was removed, along with known real third-party mod directories and
their duplicate code. Tests that inspect that real mod need an independently
obtained local fixture; pure planner/journal/controller tests remain available.
