---
title: The sce_sys/param.json fields
parent: Extras
nav_order: 3
---

# The `sce_sys/param.json` fields

Every package carries a `sce_sys/param.json` describing the title to the system: its identifiers, its
version, what kind of title it is, and the name shown on the home screen. The samples ship one
filled in with placeholder identifiers; change them for your own module before the first build. This
page describes the fields you are most likely to change and what the rest are for.

## Checking it

The build checks the metadata and fills in anything a finished title carries that yours does not:

```
== Metadata ==
  kind                           Game (0)

  Nothing missing.
```

Nothing here reports itself as an error on the console. A field the system cannot read leaves the home
screen drawing the title wrongly, or a service the title expected to reach is never offered to it, with
no message either way. So a field carrying a value the system does not recognise stops the build — and so
does an absent identity field: the title id, the content id, the content and master versions, the rights
model and the name block have no default worth guessing, so each is reported rather than invented.

Everything else is written in where it is absent: the kind of title, the badge, the age rating, the start
behaviour, the sharing table, the feature flags, the download size and the update-info link. None of those
stops a title running — the system supplies its own value for each — so the check fills them in to make the
title say what it means rather than inherit a value it never chose.

Check a folder yourself at any time:

```
sharpprospero-bindgen param --folder out/module
```

Add `--apply` to write the missing fields, `--category <kind>` to set the kind of title, and
`param --list` to see the kinds.

## Identity

| Field | What it is |
|---|---|
| `titleId` | The nine-character title id, four letters then five digits, e.g. `PPSA99099`. Identifies the title. |
| `conceptId` | The numeric concept id (usually the digits of the title id). Nothing checks it. |
| `contentId` | The full content id: `UP9000-<titleId>_00-<16 chars>`, 36 characters. The package is keyed by this, and it has to carry the title id. |
| `contentVersion` | The content version string, `NN.NNN.NNN`. Raise it for an update. |
| `masterVersion` | The master version, `NN.NN`. |

## The name on screen

`localizedParameters` holds the display name, per language. `defaultLanguage` names the fallback, and
each language code carries a `titleName`. The default language needs an entry of its own — without one
the home screen has no name to draw. Add a language by adding another entry:

```json
"localizedParameters": {
  "defaultLanguage": "en-US",
  "en-US": { "titleName": "My Game" },
  "ja-JP": { "titleName": "私のゲーム" }
}
```

## The kind of title

`applicationCategoryType` tells the system what it is starting: what the home screen draws for it, which
services it may reach, and which of them start alongside it. It is a plain number, and only these are
recognised:

| Value | Kind | What it is |
|---|---|---|
| `0` | `Game` | A title that ships and runs its own module. |
| `65536` | `MediaApp` | A media application that ships and runs its own module. |
| `65792` | `RnpsMediaApp` | A media application driven by the streaming service framework. |
| `66048` | `WebMediaApp` | A media application whose front end is a web document. |
| `131328` | `SystemBuiltInApp` | An application built into the system software. |
| `131584` | `BigDaemon` | A background service that runs with an application's resources. |
| `16777216` | `ShellUi` | The home screen and the menus drawn over a running title. |
| `33554432` | `Daemon` | A background service. |
| `50331648` | `CommonDialog` | A dialog the system draws on a title's behalf. |
| `67108864` | `ShellApp` | An application that runs as part of the home screen. |

**An application built here uses `0`.** That is the kind for any title that ships a module of its own,
whatever the module goes on to do — a media player written against this SDK is still a `0`.

The kind is not cosmetic. It selects real handling: whether HDCP is applied, whether the picture is
auto-scaled, how power saving is treated, and whether the title is given a media application's shared
storage allowance. But that handling comes with the service frameworks those kinds are built around.
Declaring a media kind does not gain a title the services — only the handling that assumes them.

Any value outside the table is a kind of title the system has no handling for, so the build stops rather
than produce one.

The memory a title is given is settled separately, not by the kind. Those fields are below.

## Badge and rights

| Field | What it is |
|---|---|
| `contentBadgeType` | The badge drawn on the icon: `0` none, `1` game, `2` application. It is yours to choose. The build fills it in only when it is absent or outside that range, starting at `1` for `Game` and `2` for the other kinds, and leaves a value you set alone. |
| `applicationDrmType` | The rights model: `standard`, `free`, `upgradable`, `demo` or `freemium`. `free` is the homebrew choice. |
| `ageLevel` | The age rating, per region, with a `default` for a region that has no entry of its own. The build adds only a missing `default` and leaves every regional entry as you wrote it. |
| `attribute`, `attribute2`, `attribute3` | Feature-flag bitmasks (background audio, cross-save, and so on). Leave `0` unless a feature needs one. |
| `downloadDataSize` | The extra download size to reserve, in bytes. `0` for a self-contained package. |

## How the title is started

`gameIntent.permittedIntents` names the ways a title may be started. A `Game` carries it; the media
kinds do not. Each entry is an object with an `intentType` of `launchActivity`, `joinSession`,
`launchMultiplayerActivity`, `launchByCustomParameters` or `launchTournamentMatch`:

```json
"gameIntent": {
  "permittedIntents": [ { "intentType": "launchActivity" } ]
}
```

`launchActivity` is the one the home screen uses, so a title that names nothing else still starts.

## Memory

A title runs on the default allowances unless it asks for more. Both blocks are optional, and absent
means default:

| Field | What it is |
|---|---|
| `kernel.flexibleMemorySize` | Flexible memory, in bytes. Has to be a whole multiple of the allocation granule. |
| `kernel.cpuPageTableSize`, `kernel.gpuPageTableSize` | Page-table memory, in bytes, on the same granule rule. |
| `amm.pagetableMemorySizeInMib` | Page-table memory, in MiB. |
| `amm.vaRangeInGib`, `amm.multimapVaRangeInGib` | Address-space range, in GiB. |

The check reads none of these fields, so a value off the granule passes the build unreported. Get them
right by hand.

Direct memory defaults to 768 MiB. Larger allowances exist in fixed steps (1024, 1280, 1536 and 1792
MiB), each of which a title has to be granted rather than simply ask for.

`ProsperoHeapHardLimitBytes` in the project file is a separate ceiling — it bounds what the managed
heap may grow to inside whatever the title is given, and does not raise the allowance.

## System and SDK versions

| Field | What it is |
|---|---|
| `requiredSystemSoftwareVersion` | The lowest firmware the title runs on, as sixteen hex digits after `0x`. |
| `sdkVersion` | The version the title was built against, in the same form. |

The `sysver` command settles both against the modules a package ships, so neither needs editing by hand.
A title that leaves `sdkVersion` at zero states it was built against nothing; running `sysver --apply`
(which every build does) writes the version the shipped modules settle on.

## Everything else

`addcont.serviceIdForSharing` carries the table of titles this one shares content with — seven entries
of nineteen characters, blank where nothing is shared. `pubtools` records the tool that produced the
metadata (`creationDate`, `toolVersion`). `versionFileUri` points at an update-info file for a title
that ships updates, and is empty otherwise. `originContentVersion` and `targetContentVersion` carry a
content version for a title that ships an update. Both are optional, and each is checked to the same
`NN.NNN.NNN` shape as `contentVersion` when present.

The packager reads `titleId`, `contentId` and the version fields; leave the rest as the sample
ships them unless a title needs the feature a field controls.
