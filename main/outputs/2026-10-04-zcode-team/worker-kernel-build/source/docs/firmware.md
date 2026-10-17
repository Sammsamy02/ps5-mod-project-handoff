---
title: Firmware compatibility
parent: System services
grand_parent: Application Modules
nav_order: 7
---

# Firmware compatibility

One module has to run across a range of system software versions. Between versions the system can move
or rename an exported function, change how a loadable module behaves, or shift a detail of the calling
convention the SDK depends on. This page covers how the SDK stays compatible across those changes, and
how a module reads the firmware it is on and refuses a feature the running system cannot provide.

The SDK builds userland application modules. It reaches the system only through the system's own
exported functions and loadable modules — never through an absolute address — so the compatibility
problem is a symbol problem, not an offset problem. Everything below works at the level of exported
names.

## Read the firmware

`FirmwareVersion.Current` reads the system software version the module is running on. The version is
held the way the system stores it — one byte per part, the digits written as they read — so it compares
correctly (10.01 is newer than 09.60, and 11.20 is eleven-twenty, not seventeen thirty-two).

```csharp
using SharpProspero.Platform;

FirmwareVersion firmware = FirmwareVersion.Current;      // e.g. 10.01
if (firmware.IsAtLeast(FirmwareVersion.FromMajorMinor(7, 0)))
{
    // use a feature that needs 7.00 or later
}
```

`FirmwareSupport` puts that together with the range the SDK supports:

```csharp
FirmwareSupport.EnsureSupported();          // throw early if the system is out of range
bool ok = FirmwareSupport.IsSupported;      // or check without throwing
FirmwareRange range = FirmwareSupport.SupportedRange;   // "02.00 and later"

// The highest SDK version this system will load a module built against.
FirmwareVersion ceiling = FirmwareSupport.AllowedSdkVersion;
```

## Resolve services by name

A service a title does not link against is loaded at run time and resolved by name through
`SystemLibrary`. The loader binds each name against whatever the running system exports, so the same
module reaches the service across versions without an absolute address. This is the mechanism that
keeps the SDK compatible: it adapts to the firmware instead of assuming it.

Resolve a name and it succeeds when the export is present. **Try** a name and fall back when it is not,
so a version that renamed or dropped an export is handled instead of crashing:

```csharp
using var library = SystemLibrary.Open("/system/common/lib/libSceExample.sprx");

// Required: throw a clear, firmware-named error if it is absent.
var start = (delegate* unmanaged<int>)library.GetFunction("sceExampleStart");

// Optional: use it only if this firmware has it.
if (library.TryGetFunction("sceExampleNewInThisVersion", out void* fn))
{
    // call through fn
}
```

`HasExport` answers the same question as `TryGetFunction` without handing back a pointer.
`FindMissingExports` reports which of a list of names the running system does not export — the check
`FirmwareSupport.Validate` runs — and `RequireExports` is its throwing form, naming every missing symbol
in one message. None of the three needs a registry entry; they check whatever library is open.

The linked bindings are compatible for the same reason at build time: they are generated against the
lowest supported version's export surface, and later systems keep that surface (an export is added
across versions, not removed), so a module linked once loads across the whole range.

## The registry: one place that says what is expected

`FirmwareRegistry` is the single source of truth for what the SDK expects of the system:

- **`SupportedRange`** — the versions the SDK supports (open-ended above the minimum).
- **`LastValidatedOn`** — the most recent version the run-time surfaces were confirmed against.
- **`DynamicLibraries`** — the services whose export set is pinned here, each carrying the library path,
  the exports it needs, the version those were confirmed on, and a note on what it is for. Today that is
  the package installer and USB mass storage. The other services resolved by name — the transfer service,
  the keycode converter, the capture service and the stored-settings service — carry no entry, so
  `Validate` cannot check them ahead of time; each reports a missing export from its own `Open` instead.

Because the entry lists exactly which exports a service needs, the running system can be checked
against it before the service is used.

## Validate, then refuse gracefully

`FirmwareSupport.Validate` loads a service and reports which of its required exports resolve on the
running system. An empty result means the feature can run; a non-empty one names exactly what is
missing, so a feature can be refused with a specific reason rather than failing partway through a call
into it.

```csharp
FirmwareValidation? result = FirmwareSupport.Validate("Package installer");

if (result is null)
    return;   // no service is registered under that name

if (!result.IsValid)
{
    // result.ToString() names the missing exports and the firmware.
    ShowMessage(result.ToString());
    return;   // refuse the feature; do not call into a service the system does not fully provide
}
```

## Reading a module's facts to contribute

For a step-by-step walkthrough of the commands below and how to call the resolver from a module, see
[Working with module offsets](offsets.md).

The registry is validated against the systems it was checked on. To extend it to a firmware the
project has not seen, read a real module from that firmware and dump what it provides:

```
sharpprospero-bindgen offsets --file libkernel.sprx --firmware 12.70 --coverage
```

This reads any of the four forms (a signed container is unwrapped first), reports the version the
module was built against, and dumps every export's identifier and address as JSON. With `--coverage`
it matches the module to the SDK library it covers and reports which of the names the SDK needs are
present — so a name a firmware moved or dropped shows up as a miss. That JSON is the record to
contribute back. (An encrypted retail module is reported as such and skipped, since its exports cannot
be read.)

## Retargeting a module to a firmware

A module records the version it was built against, and the system rejects at load a module built
against a newer system than the one it runs on. To use a module built for a newer firmware on an older
one, rewrite that recorded version:

```
sharpprospero-bindgen retarget --file mylib.sprx --to 09.00 --out mylib.09.sprx
```

With no action it reports the version the module targets and its per-library version tags; `--to`
rewrites the recorded version (the major and minor the load check reads, keeping the patch), and
`--set-lib-version <name>=0xNNNN` rewrites the version the module records for a needed library, for the
rare case where that library publishes a different version on the target. A signed module is unwrapped,
edited, and re-signed; an encrypted one is refused.

## Why there are no offsets here

The `offsets` dump above prints an address for every export, but the registry records only names. A name
is what the loader binds and what `Validate` checks; an address read from one system's module holds for
that build alone.
