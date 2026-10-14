---
title: Samples
parent: Payloads
nav_order: 8
---

# Samples

The SDK ships ready-to-build payload projects under `samples/`. Each demonstrates a different
capability, so starting from the closest one is faster than starting from scratch.

## The full sample list

| Sample | What it does |
|---|---|
| `prospero-payload-echo` | The smallest possible payload: a single `__prospero_klog` call, then returns. A bring-up probe. |
| `prospero-payload-browser` | Opens a URL through the system browser service. |
| `prospero-payload-http2-get` | Makes an HTTPS request through the system HTTP/2 library and prints the response body. |
| `prospero-payload-httpd` | A web service that answers HTTP requests with a status page. |
| `prospero-payload-hwinfo` | Reads and reports the console model, serial number, and CPU temperatures. |
| `prospero-payload-list-files` | Lists files and directories under a given path. |
| `prospero-payload-notify` | Sends a user-visible system notification. |
| `prospero-payload-ps` | Walks the process list and prints each process's identifier, name, and title id. |
| `prospero-payload-read-param-json` | Reads and parses `sce_sys/param.json` from the caller's package root. |
| `prospero-payload-test-privileges` | Reports credential fields before and after an escalation, so a debug session can see what the write changed. |
| `prospero-payload-unjail` | A daemon that listens on `127.0.0.1:9069` and promotes a named process on request (credentials, capabilities, filesystem view). |

## Building any sample

```
pwsh build/build-app.ps1 -ProjectPath samples/prospero-payload-<name>/SampleApp.csproj -Payload -Output Folder
```

The output is a single `SampleApp.elf` under the project's `out/` folder. Send it with the payload
command:

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- payload --send --host <ip> --file samples/prospero-payload-<name>/out/SampleApp.elf
```

See [Sending a payload](payload-deploy.md) for the command and the wire format.

## Starting from a sample

The samples are structured so a new payload can start from a copy:

1. Copy the sample folder to a new name outside `samples/`.
2. Change the `AssemblyName` in `SampleApp.csproj` to the payload's name.
3. Adjust `<ProsperoSprx>` and `<ProsperoKernelSprx>` as the new payload needs — see
   [SPRX declarations](payload-sprx.md).
4. Rewrite `Program.cs` for the new behaviour.
5. Build and send.

The unjail sample is a good starting point for a daemon (`__managed__Main` runs a listen-accept
loop); the httpd sample shows a stateful request/response pattern; the browser, notify, and
hwinfo samples are one-shot actions.
