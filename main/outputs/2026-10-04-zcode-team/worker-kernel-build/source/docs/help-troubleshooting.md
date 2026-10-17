---
title: Troubleshooting
parent: Help
nav_order: 1
---

# Troubleshooting

Every symptom, its cause, and the fix. Grouped by where the failure surfaces: the build, the pack,
the install, or the run. For payload-specific run-time troubleshooting see
[Payloads → Troubleshooting](payload-troubleshooting.md).

## Build

| Symptom | Cause and fix |
|---|---|
| "No compiled object was produced" | The compile step runs on Linux. On Windows the build uses WSL for it — make sure WSL and the .NET 10 SDK are installed inside it (`doctor.ps1` checks). See [Setup](setup.md). |
| The link step reports no runtime archives | Run the compile step once so the .NET SDK restores its runtime pack, which `build.ps1` then gathers; on Windows this happens inside WSL. |
| A binding call is unresolved at link time | The module's export is not in the linker's catalog. Add it, or supply a stub for your own module with `ProsperoUserStubLibrary`. See [Bindings](bindings.md). |
| The compile refuses to run and asks for `OutputType=Library` | The project sets `<ProsperoModuleKind>Prx</ProsperoModuleKind>` but leaves `<OutputType>Exe</OutputType>`. A library project has to set both. |
| The link stops with "compat object refuses payload-only symbol" | An application-module build is trying to link against a payload-only compat forwarder, which is a separation violation. Confirm the project's kind (`--kind eboot` for an application module, `--kind payload` for a payload). |

## Pack

| Symptom | Cause and fix |
|---|---|
| "The application is missing a module it has to carry" | The build found a module the application imports from that the system does not publish, and could not gather it. Point `ProsperoModuleFolder` in the project (or the `PROSPERO_MODULES` environment variable) at a folder holding it, or drop the module into the project's `sce_module` folder. |
| The pack step reports an unknown field in `param.json` | The metadata check rejects a field the system does not recognise. See [The sce_sys/param.json fields](param-json.md) for the list of accepted values. |
| Two builds installed one over the other under the same title id | An installer treats a title already on the machine as present and declines to replace it. Give the second build a title id of its own with `-TitleId PPSA99098`, so both sit beside each other. |

## Install

| Symptom | Cause and fix |
|---|---|
| The package installs but the module will not load | The system version the application requires is lower than a module it ships needs. `build-app.ps1` settles this with `-SystemVersionPolicy`; the default `Match` raises the requirement to what the modules need. See [Firmware compatibility](firmware.md). |
| The package refuses to install with an unknown error | The `content id` and `title id` in `param.json` do not agree. `contentId` has to carry the `titleId` verbatim in its `_00-` suffix. See [The sce_sys/param.json fields](param-json.md). |
| The installer shows the wrong title on the home screen | A stale build with the same title id is on the console. Delete the old title and reinstall, or bump the title id. |

## Run

### Application modules

| Symptom | Cause and fix |
|---|---|
| The home screen shows the icon but the launch does nothing | The `applicationCategoryType` in `param.json` is a value the system does not recognise (only the ten listed values are accepted). See [The sce_sys/param.json fields](param-json.md). |
| The module launches, draws a black screen, and exits | `OnFrame` is throwing before the first draw. Add a try/catch around the frame body and log the exception. See [Diagnostics](diagnostics.md) for the log surface. |
| Every `FileSystem.ReadAllBytes` outside `/app0` throws "permission denied" | The module is confined to its per-title sandbox. Ask the unjail daemon to promote the application — see [Promoting an application](app-promotion.md). |
| A save mount succeeds but `WriteAllText` throws "disk full" | The save-data allowance is smaller than the write. Raise the allowance in `param.json` or trim what the save writes. See [Save data](save-data.md). |

### Payloads

| Symptom | Cause and fix |
|---|---|
| The send succeeds but nothing is visible on the console | The CRT bring-up failed silently. Rebuild with `-DiagnosticBreadcrumbs` and read the device log — the last `sp:*` breadcrumb names the failed step. See [Payloads → Troubleshooting](payload-troubleshooting.md). |
| The payload prints its own output and then the host process dies | The payload's managed code faulted after `sp:main:enter`. Add `__prospero_klog` calls around every significant step of `__managed__Main` to localise the fault. |
| A daemon-shaped payload stops accepting after some time | Either the accept loop leaks descriptors (add a `PayloadNetwork.Close` on every path), or the host process died and took the daemon thread with it (send the payload again). |

## Starting from a sample

| Symptom | Cause and fix |
|---|---|
| No sample found under `$SHARPPROSPERO_ROOT/samples/prospero-app` | The environment variable is set to the wrong folder. Re-run `setx SHARPPROSPERO_ROOT "<sdk>"` (or `export` on Linux) so it points at the SharpProspero root. See [Samples](app-samples.md). |
| Copied sample directory builds against the SDK but fails to launch | The identity fields in `sce_sys/param.json` (`titleId`, `contentId`, `contentVersion`, `masterVersion`) are still the sample's placeholders. Change them for the module. See [The sce_sys/param.json fields](param-json.md). |

## Everyday questions

**Where is the build output?** Under the project's `out/` folder. An application module writes
`out/module/` (and, when packing, `out/*.pkg`). A payload writes `out/<AssemblyName>.elf`.

**Do I have to run `pwsh doctor.ps1` every time?** Only when the machine changes. `doctor.ps1`
reports the .NET SDK, the SDK root, and — on Windows — the WSL compile host. When a `doctor.ps1`
run reports `[ ok ]` for every entry, it can stay untouched until the next machine.

**Can I skip the pack step?** For an application module, no — the console rejects a plain ELF. For
a payload, yes — the payload build has no pack step. See [Building a payload](payload-build.md).

**How do I compare two builds?** The `diff` command reads the exports added, removed, and moved
between two modules. `dotnet run --project tools/SharpProspero.Bindings.Generator -- diff --before out/old.elf --after out/new.elf`.

**Where does the log go?** `Log.AddSink(FileLogSink.Open("/data/app.log"))` writes to
`/data/app.log`. When you cannot reach `/data`, `ConsoleLogSink()` writes to the development
console when one is attached. See [Diagnostics](diagnostics.md).

**How do I read the log without a development console?** Read `/data/app.log` from the host
process — a companion payload sent while the unjail daemon has promoted the module can `cat` the file over the
network. See [Payload API → Filesystem](payload-api.md).

## Getting more help

- Search the whole site: press `s` on any page.
- Every SDK class is under a namespace; every page names the namespace it documents.
- The API surfaces map one-to-one to the pages under [Application Modules](application-modules.md)
  and [Payloads](payloads.md); the reference material is under [References](references.md).
