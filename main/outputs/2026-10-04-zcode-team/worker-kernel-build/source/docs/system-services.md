---
title: System services
parent: Application Modules
nav_order: 10
has_children: true
---

# System services

The console's own services: reading system facts, showing overlay dialogs, saving player data,
trophies, the content library and screen capture, installing packages, and firmware compatibility.
Everything here lives in `SharpProspero.Platform`.

| Page | What it covers |
|---|---|
| [System information](system.md) | The software version and console facts, user parameters, signed-in users, keep-awake, and stored settings. |
| [Dialogs and overlays](dialogs.md) | Message and error dialogs, the on-screen keyboard, the web browser, the save picker, and toast notifications. |
| [Save data](save-data.md) | Enumerating, mounting, reading, writing, and deleting a player's saves. |
| [Trophies and events](trophies.md) | Reading trophy progress and posting telemetry events. |
| [Content and capture](content-capture.md) | The photo and video library, and capturing the finished screen. |
| [Packages and devices](packages-devices.md) | Installing titles, launching another title, USB and disc devices, and feature flags. |
| [Firmware compatibility](firmware.md) | Reading the running version and resolving services by name so one build runs across versions. |

## The permission model

A service page will often say a call "depends on what the running module is permitted to do." The
console runs a module inside a sandbox that grants a set of authorities. A service with an open step fails
there rather than on first use, so the shape to write is: try to open the service, handle the refusal, and
fall back. `Notification.Show`, `BluetoothHid.Initialize`, and `AppLauncher.Launch` return no handle to
guard, so they raise `ProsperoException` at the call itself — wrap the call. `FeatureFlag.IsOn` and
`IsWaitingReboot` return a `bool` and never signal a refusal, so a refused read looks the same as a
disabled feature.

{: .important }
> Prefer the `Try...` opener where a service offers one — it reports a refusal as `false` instead of
> raising. Where only a throwing `Open` exists, wrap it and catch `ProsperoException`. Never assume a
> privileged service is reachable; the same build may run in different sandboxes.

`DownloadService`, which controls the transfers the system is already running (see
[Networking](networking.md)), is one of the services that offers a `Try...` opener:

```csharp
if (DownloadService.TryOpen(out DownloadService? transfers))
    using (transfers) { /* use it */ }
else
    /* not permitted here — carry on without it */;
```
