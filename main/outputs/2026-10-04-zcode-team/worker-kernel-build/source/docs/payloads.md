---
title: Payloads
nav_order: 5
has_children: true
---

# Payloads

A **payload** is a position-independent `.elf` a small loader maps into a process that is already
running, over a network connection, and jumps to. There is no package and no signature. The payload
runs inside the host process and ends when its entry returns. Payloads are the executable form the
toolchain builds for background services, one-shot actions, and any code that has to run inside an
already-open process — see [Application Modules](application-modules.md) for the other form.

## What a payload is

A payload's loader does none of the setup the console's own program loader does for an application
module. It maps the segments, applies only base-relative fix-ups, and calls the entry: no import
binding, no thread-local set-up, no constructors. Everything the loader would ordinarily do — running
the constructors, marking the C runtime threaded, allocating and installing the thread-local block,
resolving every function the payload calls — the payload's own start code does at run time. The pages
under this section cover that runtime, the resolver, the kernel-access surface, the SPRX
declarations, the API, the samples, and the send path.

The trade-off is capability: an application module reaches the full device-service surface because
the loader binds those services for it; a payload reaches only what its resolver finds against the
loader-supplied module handles, plus what the kernel-access surface exposes through the pipe
primitive. Payloads can do things an application module cannot — read and write kernel structures,
walk the process list, promote another running application — because they run inside a host
process the loader chose and because their credential set is the host's.

## The API surface

| Page | What it covers |
|---|---|
| [Overview](payload-overview.md) | When to build a payload and how it differs from an application module. |
| [Building a payload](payload-build.md) | `build-app.ps1 -Payload`, the output layout, and per-firmware options. |
| [Runtime bring-up](payload-runtime.md) | The start-code sequence, the syscall gateway, the resolver cascade, TCB setup, and thread exit. |
| [Resolver cascade](payload-resolver.md) | The three module handles the resolver probes and the name-encoding rules. |
| [Kernel access](payload-kernel.md) | `PayloadKernel`, the pipe primitive, and the per-firmware data table. |
| [SPRX declarations](payload-sprx.md) | Adding a system library a payload calls with `<ProsperoSprx>` and `<ProsperoKernelSprx>`. |
| [Payload API](payload-api.md) | The `SharpProspero.Payload` namespace: network, filesystem, process, notification, and kernel. |
| [Samples](payload-samples.md) | Every payload sample the SDK ships and what each demonstrates. |
| [Sending a payload](payload-deploy.md) | The `payload --send` command, the default port, and the loader wire format. |
| [Promoting a running application](payload-promotion.md) | Building the daemon side of the application-promotion request. |
| [Troubleshooting](payload-troubleshooting.md) | Recognising the CRT breadcrumbs and diagnosing the common failures. |

## The build path

The `-Payload` switch compiles and links a payload:

```
pwsh build/build-app.ps1 -ProjectPath samples/prospero-payload-unjail/SampleApp.csproj -Payload -Output Folder
```

The output is a single `.elf` under the project's `out/` folder. Send it to a listening loader:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- payload --send --host ps5-host-not-configured.invalid --file samples/prospero-payload-unjail/out/SampleApp.elf
```

The default send port is 9021. Add `-DiagnosticBreadcrumbs` to include the CRT's `sp:*` log
checkpoints in the built ELF, which are useful during bring-up and can be omitted for production.

## The lifecycle

A payload's lifecycle is: sent, mapped, run once, returns. The loader closes the connection when the
entry returns, and the host process continues without the payload's thread. A daemon-shaped payload
that has to persist installs itself as a long-running loop in the host process (accepting requests on
a TCP socket, for instance) and never returns; the loader tears down its own connection immediately
after the map, and the payload continues running inside the host.
