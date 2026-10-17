# Formats

These documents define the universal layer. All JSON documents carry
`schema_version` (currently `1`).

| Document | Where | Written by |
|----------|-------|------------|
| Mod package (`manifest.json` + `files/`) | catalog / downloads | `modforge build` |
| FOMOD selections (`{step: {group: [plugins]}}`) | user / app | `modforge fomod` shows the options |
| Engine definition | `engines/<engine>.json` | hand-written, one per engine |
| Game profile | resolved on demand (`profiles/` only for overrides) | `modforge profile` |
| Games registry | `games.json` | hand-written |
| Catalog | server | `catalog.build_catalog` |
| Deployment | `<mod root>/deployment.json` | `modforge deploy` |
| Load list | `<mod root>/loadlist.txt` | `modforge deploy` |
| **PS5 bundle** (`.mfb`) | downloaded by the console app | `modforge deploy --bundle` |
| **Console catalog** | one game's listing for the console app | `modforge catalog export` |
| **Mod bundle** (`.mfb`, kind 1) + `modinfo.txt` | one mod for one game, merged on the console | `modforge bundle mod` |

## Roles

A *role* is what a group of files is for. It determines the install location
(via the profile) and the platform compatibility.

| Role | Files | PS5 default |
|------|-------|-------------|
| `lua_mod` | `.lua` (and a mod folder's loose files) | native, once the engine's Lua runtime is ported |
| `script` | engine script languages: Papyrus `.pex`/`.psc`, redscript `.reds`, WitcherScript `.ws` | needs the engine's script support on console |
| `pak` | `.pak` `.utoc` `.ucas` `.sig` (Unreal/RE), `.bsa` `.ba2` (Bethesda) | per engine: Unreal paks install as-is; Bethesda archives hold PC assets and are converted |
| `native_plugin` | `.dll` `.asi` (Windows), `.prx` `.sprx` (PS5) | prebuilt `.prx` → native; `.dll` with source → rebuild; `.dll` without source → unsupported |
| `data_plugin` | `.esp` `.esm` `.esl` | native where the engine is Bethesda |
| `asset_override` | textures, models, audio, `.uasset`… | needs conversion |
| `config` | `.ini` `.json` `.txt` `.xml` `.cfg` `.toml` `.yaml`, `regulation.bin`, `plugins.txt` | native |

Classification is per top-level item: a top-level directory takes the
highest-priority role among the files it contains (priority order:
native > pak > data_plugin > lua > script > asset > config), so a UE4SS mod
folder with a stray `enabled.txt` stays one `lua_mod` component, and a Witcher 3
mod folder holding both `.bundle` archives and `.ws` scripts installs as one
unit under the archive role's base.

## Mod package

A `.zip` containing:

```
manifest.json
files/<...>          # the mod's files, laid out as the engine expects
```

`manifest.json`:

```json
{
  "schema_version": 1,
  "id": "unique-mod-id",
  "name": "Display Name",
  "version": "1.0.0",
  "author": "",
  "engine": "unreal",
  "games": ["PPSA33286"],              // title IDs, or ["*"] for any game of this engine
  "source": {"type": "nexus", "permission": "granted"},
  "requires": [{"id": "ue4ss-ps5", "min_version": "0.1.0"}],
  "load_order_priority": 100,
  "mod_type": "data",                  // derived: data|asset|pak|code|mixed
  "components": [
    {"role": "lua_mod", "root": "ItemSpawner",
     "files": ["ItemSpawner/scripts/main.lua", "ItemSpawner/enabled.txt"]}
  ]
}
```

`source.permission` ∈ `granted | open-license | user-download | self | unknown`
— a catalog should only redistribute `granted` / `open-license` / `self`.

`data_plugin` components also get a `plugin_info` map at build time, read from
each plugin's header. It records each file's masters and flags, or an `error`
if the file isn't a readable plugin:

```json
{"role": "data_plugin", "root": "Patch.esp", "files": ["Patch.esp"],
 "plugin_info": {"Patch.esp": {"masters": ["Skyrim.esm", "Big.esp"],
                               "master": false, "light": false, ...}}}
```

## FOMOD installers

Many Nexus mods, especially Bethesda and Fallout, ship as a FOMOD. That's a
`fomod/ModuleConfig.xml` (plus usually `fomod/info.xml`) describing an
installer with steps, groups of options and conditions. `modforge/fomod.py`
runs that installer logic on the host and builds a normal mod package from the
result, so the console never has to understand FOMOD.

Supported:

- `requiredInstallFiles` (always installed)
- `installSteps` → `group` → `plugin` with `<file>` / `<folder>` operations,
  `priority` (higher wins a destination clash), and backslash or
  wrong-case source paths. Sources are matched case-insensitively, as mod
  managers on Windows do.
- Group types `SelectExactlyOne`, `SelectAtMostOne`, `SelectAtLeastOne`,
  `SelectAny`, `SelectAll`. Explicit selections are validated against them.
- Plugin types `Required` (can't be dropped), `Recommended` (default on),
  `Optional`, `NotUsable` (can't be chosen), `CouldBeUsable`. A
  `dependencyType` uses its `defaultType`.
- Plugin `conditionFlags` and flag-based `conditionalFileInstalls` patterns
  (`And`/`Or`).

Not evaluated: file, game-version and mod-manager-version dependencies. A
conditional pattern that uses one is skipped and reported as a warning, never
guessed at. A DTD in the XML is refused, since that's where entity-expansion
attacks live.

Selections are JSON, `{step name: {group name: [plugin names]}}`. A group left
out gets its default. Defaults are:

- **Exactly one:** the Required option, else the Recommended one, else the
  first.
- **At most one:** the Required or Recommended option, else none.
- **At least one:** the Required and Recommended options, else the first.
- **All:** every usable option.
- **Any:** the Required and Recommended options.

FOMOD destination paths are relative to the game's data root, which is the
same base the engine roles install under. So the resolved files classify into
components like any other mod. A loose `.esp` becomes a `data_plugin`,
`textures/` an `asset_override`, `Scripts/` a `config`.

## Engine definition

One per engine: `engines/<engine>.json`. This is where coverage lives.

```json
{
  "schema_version": 1,
  "engine": "unreal",
  "name": "Unreal Engine 4/5",
  "framework": {"id": "ue4ss-ps5", "maturity": "integrating"},
  "components": {
    "ue4ss-ps5":  {"maturity": "integrating", "what": "UE4SS runtime ported to PS5"},
    "pak-loader": {"maturity": "available",   "what": "loads extra .pak/.utoc/.ucas mods"}
  },
  "asset_converter": "unreal-assets",
  "load_order": "manifest_priority",       // or "plugins_txt" (Bethesda)
  "detect": {"ext_any": [".utoc"], "path_contains_any": ["/content/paks/"], "name_any": []},
  "roles": {
    "lua_mod": {"dest": "ue4ss/Mods", "supported": true, "requires": ["ue4ss-ps5"]},
    "pak":     {"dest": "mods/~mods", "supported": true, "requires": ["pak-loader"]},
    ...
  }
}
```

- **`components`**: the pieces that must run on the console for this engine's
  mods to work, each with a `maturity`:
  - `available`: ported and working.
  - `integrating`: being ported. Mods may be installed but are not proven
    in-game.
  - `planned`: not ported. Roles that need it are reported unsupported.
- **`framework`**: the primary runtime the loader injects. Its maturity must
  match its `components` entry.
- **`roles.<role>.requires`**: component ids that role needs. Each must be
  declared in `components` (or be the `framework`).
- **`base_plugins`** (Bethesda): masters that ship with the game, as names or
  glob patterns, e.g. `Skyrim.esm` or `cc*.esl` for Creation Club content. A
  plugin may depend on these without anyone installing them.
- **`detect`**: hints for guessing an unknown game's engine from its file
  names. Signals are scored. Names and path fragments count more than
  extensions, and an extension several engines share (e.g. `.pak`) counts
  least. A tie returns "unknown" rather than a guess. The games registry is
  authoritative; detection is only a fallback.

## Game support profile

A profile is an engine definition *resolved* for one game: the same fields,
plus the game's facts. You normally don't write one. Generate it from the
game's `param.json`:

```bash
python3 -m modforge profile --param-json sce_sys/param.json -o profiles/PPSA33286.json
```

```json
{
  "schema_version": 1,
  "title_id": "PPSA33286",
  "name": "Townfall",
  "engine": "unreal",
  "sdk_version": "0x11000000",          // stamped into native modules
  "required_firmware": "13.60",
  "framework": {...}, "components": {...}, "roles": {...},   // from the engine
  "derived_from_engine": "unreal"
}
```

A role's `dest` is the base directory under which that component's files are
placed, preserving their path below `files/`, relative to the game's mod root.
An unsupported role may leave `dest` empty. `engine.resolve_profile` accepts
`role_overrides` for a game whose layout differs from its engine's default.

### Facts from `param.json`

`facts.facts_from_param_json` reads:

- `titleId` → `title_id` (required)
- `sdkVersion` → `sdk_version`, normalised to the 32-bit stamp: `0x11000000`
  from `0x1100000000000000`, `0x11000000` or `11.00`
- `requiredSystemSoftwareVersion` → `required_firmware`, e.g. `13.60`
- `localizedParameters.<defaultLanguage>.titleName` → `name`
- `contentVersion` → `game_version` in the profile

The version fields are BCD in their leading bytes. Townfall's values
(SDK 11.00 → `0x11000000`, firmware 13.60) match what the console test
needed. The exact field names should be confirmed against a real param.json
when this is wired into the app.

## Mod root layout

Each game's mod root on the console (`/data/modbridge/<TITLE_ID>`) is laid out
so every console component knows exactly where its files are:

| Under the mod root | Read by | Example |
|--------------------|---------|---------|
| `game/…` | the data-only overlay plug-in (`overlay-ps5`), which mounts this tree over the game's install root (`/app0`), so it mirrors the game's own layout | `game/Data/MyMod.esp`, `game/re_chunk_000.pak.patch_002.pak` |
| `<framework>/…` | that framework | `ue4ss/Mods/ItemSpawner/…`, `reframework/autorun/…` |
| `mods/…` | a loader component | `mods/~mods/Skin_P.pak` (Unreal pak loader) |
| `loadlist.txt`, `.modforge-root` | the runtime / installer | |

Engine definitions follow this rule, and a test enforces it: a role that
`requires` `overlay-ps5` installs under `game/`, and no other role writes
there. Overlay paths copy the PC layout relative to the game root. Whether a
game's PS5 install uses the same layout has to be confirmed per game; capture
any difference as `role_overrides`.

## Games registry

`games.json` maps games to engines so the app can list supported games. Only
stable, public facts go here: name, engine, and `offline_only` for games whose
mods must never go online. A `title_id` is listed only for console-verified
games (`"verified": true`, currently Townfall). Everything else is read from
`param.json` on the device.

## Compatibility tiers

`profile.compatibility(profile, manifest)` → `(tier, reasons)`:

- **native** — installs as-is.
- **converted** — at least one component needs host-side conversion or a native
  rebuild first.
- **unsupported** — at least one component cannot work on this platform (e.g. a
  prebuilt Windows DLL with no source, a role the engine doesn't support, or a
  role whose console component is still `planned`).

The whole-mod tier is the worst tier among its components.

## Catalog

```json
{
  "schema_version": 1,
  "entries": [
    {"id": "...", "name": "...", "version": "...", "mod_type": "data",
     "engine": "unreal", "games": ["PPSA33286"],
     "package": "https://.../mod.zip", "sha256": "...",
     "manifest": { ... full manifest ... }}
  ]
}
```

The embedded manifest lets the app compute compatibility and show only mods that
work for the chosen game, without downloading every package first.

- **Build:** `modforge catalog build <pkg>... --base-url URL -o catalog.json`.
  It validates every package, records its sha256, and **leaves out any mod
  whose `source.permission` doesn't allow redistribution**. Only `granted`,
  `open-license` and `self` are allowed; `user-download` and `unknown` mods
  must be fetched by the user from their original page.
- **Validation:** entry ids must be unique and match the embedded manifest's id.
- **List:** `modforge catalog list catalog.json <game selection> [--all]` shows
  the mods for one game with their tier. `--all` adds the unsupported ones and
  the reasons.
- **Verify before install:** `catalog.verify_package(entry, path)` (or
  `deploy --catalog`) checks a downloaded package's sha256, id and version
  against its entry. A tampered or mismatched download is refused.

## Deployment

The set of mods the user enabled for one game, resolved together
(`deploy.resolve_deployment`):

```json
{
  "schema_version": 1,
  "title_id": "PPSA33286", "engine": "unreal", "sdk_version": "0x11000000",
  "frameworks": ["pak-loader", "ue4ss-ps5"],   // components the loader must inject
  "mods": ["low-priority-mod", "high-priority-mod"],
  "files": [{"src": "files/...", "dest": "...", "role": "pak", "action": "copy", "mod": "..."}],
  "conflicts": [{"dest": "...", "previous": "a", "winner": "b"}],
  "load_order": ["Base.esm", "Patch.esp"],      // engines with load_order = plugins_txt
  "load_order_problems": [{"kind": "missing_master", "plugin": "...", "master": "...",
                           "description": "..."}],
  "warnings": ["mod: masters of X.esp not checked (...)"],
  "missing_requirements": [...],
  "rejected": [{"id": "...", "reasons": ["..."]}],
  "host_steps": [{"dest": "...", "mod": "...", "action": "convert:unreal-assets"}],
  "module_fixes": [{"mod": "...", "dest": "ue4ss/Mods/X/x.sprx",
                    "from": "0x02000009", "to": "0x11000000"}],
  "module_problems": [{"mod": "...", "dest": "...", "code": "not-library",
                       "description": "..."}],
  "ready": true
}
```

A file entry whose module needs the game's SDK stamp also carries
`"sdk_patch": "0x11000000"`. Whatever writes the file (staging, the game
bundle, a mod bundle) applies it, so the console receives an already-stamped
module. See [PS5 modules](#ps5-modules-prx--sprx).

- **Priority:** `load_order_priority` is "later wins". On a file conflict the
  higher-priority mod's file is used, and in a plugin load order the
  higher-priority plugin comes later.
- **Frameworks:** only the components the chosen mods' roles actually need are
  listed. A pak-only deployment doesn't ask the loader to inject UE4SS.
- **Not ready** means a host step (asset conversion, native rebuild) or a
  dependency is outstanding. That includes requiring a component that is still
  `planned`. The console only ever receives ready deployments.
- **Rejected mods** (unsupported for this game) are reported, not fatal.
- **Bethesda load order** (`plugins_txt`) follows the engine's rules, using the
  masters recorded at build time:
  - Plugins that load as masters (`.esm`, `.esl`, or the ESM flag) come first.
  - Every plugin comes after the masters it names.
  - Otherwise the user's priority is kept.

  These problems block the deployment:
  - `missing_master`: a master nobody provides that isn't a `base_plugins`
    entry. The game would crash on load.
  - `master_needs_plugin`: a master depends on a regular plugin.
  - `cycle`: plugins that depend on each other.

  A plugin whose header couldn't be read is ordered by priority, with a
  warning that its masters weren't checked.
- **PS5 modules:** every prebuilt `.prx`/`.sprx` is checked against the game
  (see below). `module_problems` block the deployment; `module_fixes` don't,
  because the fix is applied when the file is written.

## Load list

The file the engine runtime reads at `module_start`. That is the only moment
`/data` is readable, so the runtime can't browse a mods folder later. UTF-8
text, one record per line, fields separated by a single TAB, at most 5 fields.
Blank lines and `#` comments are ignored. Unknown record kinds must be skipped.

```
modforge-loadlist	1                         header, always first
title	PPSA33286
engine	unreal
sdk	0x11000000
framework	ue4ss-ps5                      components to start, zero or more
mod	example-item-spawner	1.0.0          enabled mods, load order (later wins)
file	lua_mod	ue4ss/Mods/ItemSpawner/scripts/main.lua	589
order	Base.esm                          data-plugin load order, first to last
```

`file` paths are relative to the game's mod root, by default
`/data/modbridge/<TITLE_ID>`. They never contain a TAB, a newline, a leading
`/`, a backslash or an empty, `.` or `..` segment. The size lets the runtime
allocate each buffer up front and detect truncated files.

Readers: `modforge/loadlist.py` (`parse`) and `runtime/modforge_loadlist.h`
(C99, allocation-free). The test suite checks that the two agree.

## Staged mod root

`modforge deploy --stage DIR` (or `stage.stage`) lays out the game's mod root
exactly as the console app would:

```
<root>/.modforge-root      marker: this directory is managed by modforge
<root>/deployment.json
<root>/loadlist.txt
<root>/<dest paths...>     the mods' files
```

Staging replaces the root's contents only if the root is empty or carries the
marker. A directory modforge didn't create is never wiped.

## PS5 bundle (`.mfb`)

What the console actually installs: one file per game, already resolved on the
host, so the console needs no Python, JSON or zip support. All integers little
endian.

```
header, 32 bytes
  0   8  magic "MFBUNDL1"
  8   4  u32 format version (1)
  12  4  u32 entry count
  16  8  u64 index size in bytes
  24  8  u64 reserved (0)
index, one record per entry
  u64 data offset (from the start of the file)
  u64 data size
  32  SHA-256 of the data
  u16 path length, then the UTF-8 path
data
  the entries' bytes, in index order
```

Writer (`modforge/bundle.py`) and both installers (`runtime/modforge_bundle.h`
on the console, `bundle.install_bundle` on a PC) enforce the same rules. The
fuzzer checks that they accept exactly the same inputs.

- **Paths:** relative and `/`-separated, strict UTF-8 (no overlong forms or
  surrogates), no control characters or backslashes, no empty, `.` or `..`
  segment, at most 1024 bytes, never `.modforge-root`.
- **Ranges:** every entry's data lies after the index and inside the file. The
  index size must equal the sum of its records.
- **Load list:** exactly one `loadlist.txt`. The installer writes it **last**,
  after every other file has passed its hash check. A truncated, tampered or
  interrupted install therefore never leaves a load list, and the runtime
  loads nothing from it.
- **Order of operations:** the whole index is validated before anything is
  written. Then the installer writes `.modforge-root`, then each file
  (hashing as it streams, and deleting a file that fails), then the load
  list.
- **Root:** the installer expects an empty or freshly cleared mod root. The
  app clears it only if it holds the `.modforge-root` marker.
- **Error messages:** printable, valid UTF-8. Hostile path bytes are shown as
  `?`.

## Console catalog

The full catalog is JSON with every manifest embedded. That's right for the
host, but the console app shouldn't need a JSON parser. So the host exports,
per game, a small listing for the app's browse screen
(`modforge catalog export catalog.json <game> -o <TITLE>.catalog.txt [--all]`),
read on the console by `runtime/modforge_catalog.h`.

Same conventions as the load list: UTF-8, one record per line, TAB-separated,
at most 8 fields, `#` comments and blank lines ignored, unknown kinds skipped.

```
modforge-catalog	1
game	PPSA33286	Townfall	unreal
mod	item-spawner	1.0.0	native	data	1234	Item Spawner
mod	hd-skin	2.1	converted	asset	99887	HD Skin
why	hd-skin	role 'asset_override' needs conversion 'unreal-assets'
```

- **`tier`:** `native` (installs as-is), `converted` (works once the host
  converts it while building the bundle), or `unsupported` (listed only with
  `--all`, so the app can grey it out; the `why` lines say why).
- **`size`:** the package's download size in bytes, in ASCII digits fitting in
  64 bits.
- **Fields:** ids, versions, title IDs and engine ids must be tokens (no
  whitespace or control characters). Free text (names, reasons) has control
  characters turned into spaces on export. Readers refuse control characters
  in any field.

## Mod bundles and the console composer

A **mod bundle** is the same container with header kind `1`. It holds one mod's
files at their final paths for one game, plus `modinfo.txt`, and never a
`loadlist.txt`. The host builds one per mod per game
(`modforge bundle mod <mod.zip> <game> -o <mod>.<TITLE>.mfb`). Only mods that
install as-is can be bundled: conversion and native rebuilds run on the host
first.

On the console, `mfx_compose()` (`runtime/modforge_compose.h`) merges the
bundles the user picked. `modforge bundle compose` is the reference in Python.
Both refuse before writing anything if:

- any bundle is malformed: bad header, index or paths, out-of-range data, a
  modinfo hash or syntax error, or a modinfo file list that isn't exactly the
  bundle's entries;
- a bundle was built for another game, or the bundles disagree on the engine,
  SDK, load-order mode or base plugins;
- the same mod is given twice;
- a dependency is missing or too old (versions compare part by part on their
  ASCII digits, as numbers of any length);
- a Bethesda load-order problem: a missing master, a master depending on a
  regular plugin, or a cycle.

Otherwise mods are ordered by `priority` (then id) and a later mod wins a file.
The load list's `framework` records are the union of every mod's. Files are
installed hash-checked, and `loadlist.txt` is written last. The result is
byte-identical to the host path (deploy, then game bundle, then install).
The tests check this on fixed and randomized mod sets, and the fuzzer checks
that the C and Python composers agree on mutated input.

`modinfo.txt`: UTF-8, TAB-separated, at most 4 fields, no empty fields or
control characters. Unknown kinds are skipped.

```
modforge-modinfo	1
mod	<id>	<version>	<priority>         priority: signed integer, at most 18 digits
title	<title id>
engine	<engine id>
sdk	<sdk version>                       only if the game has one
load_order	manifest_priority|plugins_txt
framework	<component id>                  zero or more
requires	<mod id>	<min version>         zero or more
base	<pattern>                           base-game masters, * and ? globs
file	<role>	<path>                      exactly the bundle's entries
plugin	<file name>	m|-|?                 data plugins in order (m = ESM flag, ? = header unreadable)
master	<plugin file name>	<master name>   zero or more per plugin
```

The arena the console passes to `mfx_compose()` holds the parsed tables. A few
MB covers large mod lists; if it's too small, the call fails with a message
saying so, before anything is written.

## PS5 modules (`.prx` / `.sprx`)

A native mod reaches the game as a module that the loader passes to
`sceKernelLoadStartModule`. `modforge/ps5module.py` (host) and
`runtime/modforge_prx.h` (console) read the same fields with the same rules.
`tests/test_prx.py` and the fuzzer hold them to identical output. All integers
are little-endian.

| Part | Where | What |
|------|-------|------|
| magic | 0x00 | `7F 45 4C 46` plain ELF · `4F 15 3D 1D` SELF, PS4 magic · `54 14 F5 EE` SELF, PS5 magic |
| SELF entry count | 0x18 (u16) | entries start at 0x20, 0x20 bytes each: u64 props, u64 file offset, u64 stored size, u64 memory size |
| entry props | | `0x2` encrypted, `0x8` compressed, `0x800` has blocks (holds segment bytes; digest entries don't), bits 20–35 = the program header it stores |
| inner ELF | 0x20 + 0x20 × entries | ELF header and program headers only. Their `p_offset`s describe the unwrapped file. |
| extended info | align16(ELF + e_phoff + 56 × e_phnum) | u64 auth id, **u64 program type** (1 = fake; the only kind kstuff accepts) |
| parameter block | first `PT_SCE_MODULE_PARAM` (0x61000002) or `PT_SCE_PROCPARAM` (0x61000001) | +0x00 u64 size · +0x08 magic `0x3C13F4BF` (module) / `ORBI` (process) · +0x0C block version (≥ 2 on PS5) · +0x10 PS4 SDK · **+0x14 PS5 SDK** |

In a SELF the parameter block is located through the first has-blocks entry
whose program header covers it: the file offset is the entry's offset plus the
distance into that segment. A plain ELF uses `p_offset` directly.

The SDK stamp is BCD-style: `0x11000000` is 11.00 and `0x02000009` is 2.00
patch 9. A module is retargeted when its major.minor (top 16 bits) differs from
the game's. Retargeting writes the game's 32-bit SDK to +0x14 and changes
nothing else. That 4-byte edit is exactly the one verified on the console.

Assessment codes (shared by host and console):

| Code | Kind | Meaning |
|------|------|---------|
| `not-library` | problem | `e_type` isn't 0xFE18; only libraries load as modules |
| `encrypted` | problem | encrypted or compressed segments: the stamp can't be read or fixed |
| `unreadable` | problem | not a module at all (host only; recorded at package time) |
| `plain-elf` | warning | not wrapped; only fake SELFs are known to load |
| `not-fake` | warning | SELF program type isn't 1 |
| `no-sdk` | warning | no readable parameter block |
| `ps4-toolchain` | warning | block version < 2 (OpenOrbis/GoldHEN layout, no PS5 SDK field); not retargeted, untested in PS5 games |

**Not known, so not claimed:** the kernel's exact rule (equal SDK? not older
than the game?), whether the PS4 SDK at +0x10 or the `PT_SCE_VERSION` records
are checked at all, and whether the SELF magic must pair with a particular
signature-area size. Only "match the game's major.minor" is verified.

A package records each module's summary in its component's `module_info`:

```json
"module_info": {"Spawner/spawner.sprx": {
  "container": "self-ps4", "fake": true, "encrypted": false, "kind": "library",
  "abi_version": 2, "param_format": "ps5", "sdk": "0x02000009"}}
```

A file that isn't a readable module is recorded as `{"error": "..."}`.
