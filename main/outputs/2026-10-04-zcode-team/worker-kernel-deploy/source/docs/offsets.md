---
title: Working with module offsets
parent: References
nav_order: 3
---

# Working with module offsets

An "offset" here is a fact a module carries: which functions it exports, at what address and under what
identifier, and the versions it records for itself and the libraries it binds to. The SDK reaches every
service by name, so these are the facts that let you read a module, adjust it for a system, and call it
at run time. This page shows how to get them, set them, and use them. For how the pieces fit together,
see [Firmware compatibility](firmware.md).

## What an offset is

- **An export.** Each function a module provides has an identifier (the export name mapped through the
  module's identifier scheme) and an address within the module.
- **A recorded version.** A module records the system version it was built against, and, per library it
  imports from, the version it expects that library to publish.

Because binding is by name, you rarely need a raw address. What matters is that an export is present on
the running system and that the recorded versions match it. The tools below read and adjust exactly
those facts.

## Get: read a module's offsets

`offsets` reads any of the four forms (`.prx`, `.sprx`, `.elf`, `.self`); a signed container is
unwrapped first. It prints JSON by default, or a readable form with `--text`.

```
sharpprospero-bindgen offsets --file libkernel.sprx --firmware 12.70 --text
```

```
File:       libkernel.sprx
Container:  signed (.self / .sprx)
Firmware:   12.70
Built for:  12.70
Module:     libkernel (library version 0x0001)
Exports:    1635
  OjWstbIRPUo  0x2E4  func  libkernel
  ...
```

A `Needs:` line follows `Module:` when the module names dependencies of its own.

The JSON form carries the same facts (`file`, `container`, `firmware`, `builtAgainst`, `exportsReadable`,
`moduleName`, `libraryVersion`, `neededModules`, and an `exports` array of `{ nid, library, kind, address }`;
a `note` replaces the exports when they cannot be read), so it can be saved and shared. Add `--coverage` to
match the module against the names the SDK needs and report which are present, and `--library <name>` to
measure against one named library instead of the best match:

```
sharpprospero-bindgen offsets --file libkernel.sprx --firmware 12.70 --coverage
```

The `coverage` block names the matched library, the counts, the missing names, and, for each name, its
identifier and address. An empty `missing` means the system provides everything the SDK needs from that
library; a non-empty one is the fact worth contributing.

A coverage run that finds a missing name exits 4. A script that redirects the JSON to a file should read 4
as the signal worth acting on rather than as a failure.

## Set: adjust a module's version and tags

`retarget` rewrites what a module records, so a module built for one system can load on another. With no
action it reports the current values:

```
sharpprospero-bindgen retarget --file mylib.sprx
```

```
File:       mylib.sprx
Container:  signed (.self / .sprx)
Targets:    12.70 (0x12700001)
  needed module   libkernel                    v1.1 (0x0101)
  import library  libkernel                    v0.1 (0x0001)
  ...
```

Rewrite the recorded version with `--to` (the system rejects at load a module built against a newer
system than the one it runs on; this lowers that recorded version so it loads). Only the major and minor
are read by the check, so the patch is kept:

```
sharpprospero-bindgen retarget --file mylib.sprx --to 09.00 --out mylib.09.sprx
```

Rewrite the version a module records for one it needs with `--set-lib-version`, for the case where that
module publishes a different version on the target system. It matches the `needed module` rows above and
rewrites their version (`0x0101` is v1.1); the `import library` rows are left alone, because the module
version is the one the loader matches:

```
sharpprospero-bindgen retarget --file mylib.sprx --set-lib-version libSceHttp=0x0101 --out mylib.fixed.sprx
```

A signed module is unwrapped, edited, and re-signed; an encrypted one is refused, since its contents
cannot be read.

## Use: resolve and check at run time

Inside a module, read the system version and resolve services by name. The loader binds each name
against whatever the running system exports, so one build reaches a service across systems.

```csharp
using SharpProspero.Platform;

// Read the running version and fail early if the SDK does not support it.
FirmwareSupport.EnsureSupported();
FirmwareVersion firmware = FirmwareVersion.Current;   // e.g. 10.01

// Resolve a service a title does not link against.
using var library = SystemLibrary.Open("/system/common/lib/libSceExample.sprx");

// Required: throws a clear, firmware-named error if the export is absent.
var start = (delegate* unmanaged<int>)library.GetFunction("sceExampleStart");

// Optional: use a name only if this system has it, and fall back otherwise.
if (library.TryGetFunction("sceExampleNewInThisVersion", out void* fn))
{
    // call through fn
}
```

Before a feature that depends on a resolved-by-name service, check that the system provides every export
it needs, and refuse the feature with a specific reason otherwise:

```csharp
FirmwareValidation? result = FirmwareSupport.Validate("Package installer");

if (result is null || !result.IsValid)
{
    ShowMessage(result?.ToString() ?? "No service is registered under that name.");   // names the missing exports and the firmware
    return;
}
```

## Contribute

To extend the SDK's knowledge to a system it has not been checked on, read a real module from that
system and share the coverage:

```
sharpprospero-bindgen offsets --file libkernel.sprx --firmware <NN.NN> --coverage > libkernel.json
```

The JSON records the module's exports and how they cover what the SDK needs, so a name a system moved or
dropped is visible and can be added to the SDK's registry.
