# GAPS — fields the profile does NOT produce, and why

Mac-only. Measured on the Townfall executable (sha `3b6cfad4…`). "Fail closed" means the runtime
refuses that value / switches that feature off rather than using a wrong number. Nothing below is
guessed. References are to `evidence/sb-readiness-20261014/READINESS.md`.

## A. Optional features turned OFF (the seeds still arm; only that feature is unavailable)

Per READINESS B5, feature pins only switch their own feature off; they are not fatal. The emitter sets
the matching `[features]` flag to 0 and writes the section as comments.

| Field / section | Why not produced | Effect |
|---|---|---|
| `[pak]` (vtable, platform-manager getters, mount delegate, 6 code proofs) + `[features] pak_mount=0` | No generic pak-mount detector exists yet (READINESS C4/C5). The anchors are PS5/engine-specific and were never derived generically. | Outfit-pak mounting is OFF and fails closed. Lua mods and other features still work. **This is the one gap that touches the CORE blind test's "outfit paks"** — see APPLY.md. |
| `[listener] add/remove labels + prefixes`, `[code_pins] listener_*` + `[features] delete_listener=0` | No generic delete-listener detector (READINESS C2). Townfall used hand disassembly. | The lookup-cache delete listener does not arm; the runtime must rebuild-on-miss instead. |
| `[thread] accessor/game_name/render_name/empty_name`, `[code_pins] thread_name_prefix` + `[features] thread_name=0` | No generic thread-name-accessor detector (READINESS C3/S6). No string/behaviour anchor for it. | No thread name is bound; the runtime still runs. S6 proposes removing the need entirely. |

## B. Layout vtable slots the generator does not yet resolve (stay unresolved → fail closed)

These are listed in the profile's `[layout.vtables]` omission comments and in `runtime-profile.json`
`unresolved`. They are NOT written with a template value (READINESS D2).

| Slot | Why not derived |
|---|---|
| `UEngine.LoadMap` | No pure-virtual stub string anchors it (only `UEngine::Tick` has one in this engine). |
| `AGameModeBase.InitGameState` | No string or call-sequence anchor is analysed for it. |
| `FProperty.CopyValuesInternal` | Its callers are thin wrappers around a plain `memcpy`; no analysed code identifies the slot. |
| `FMalloc.*` full slot list (TryMalloc, Realloc, TryRealloc, QuantizeSize, Free, GetAllocationSize, Trim, … GetDescriptiveName) | Only `FMalloc.Malloc` and `FMalloc.Free` are derived (from the dispatch shape). The remaining slots depend on the concrete allocator vtable layout, which is not read. |

Note: `FProperty.GetMinAlignment` (0x170) **is now emitted** (Team S3 added it to the vtable emitter;
it was resolved but previously dropped). `FProperty.InitializeValueInternal` is emitted as `0x158`,
which READINESS open-point #5 flags as disputed vs an earlier `0x150` note — carried at the generator's
measured value, not reconciled here (no second source decides it on the Mac).

## C. Layout members the generator does not derive (kept in `unresolved`, not templated)

Reflection members no analysed function reads and no measured neighbours bound: most of `UClass`'s
cache/func-map/flags tail, several `FUObjectItem` packed variants, `UStruct` script-reference members,
`UFunction.RepOffset`, and the `FFieldClass` zero-initialised tail. Also member classes the layout
detectors do not cover at all: `FMapProperty`, `FSetProperty`, `FMulticastDelegateProperty`,
`FWorldContext`, `UEnum` and `UClass.Interfaces`. Full list: `[layout.members]` comment block in
`evidence/townfall-profile.ini` and `runtime-profile.json` `unresolved`. These are the same gaps the
branch generator already had; closing them is generator detector work beyond B4's emitter scope.

## D. Identity fields that differ from S1's text but are accepted (presence-only)

The runtime only checks these are present / valid hex (`BuildFactsChecks.hpp`: `text(p->name)`,
`hex_pin(p->digest)`), not that they equal a particular string. The generator writes its own correct
values instead of copying Townfall's:

| Field | Generator writes | S1 oracle | OK because |
|---|---|---|---|
| `name` | `PPSA33286-prospero-ue56-shipping-stats-off` (synthesized) | `townfall-ps5-ue56-shipping-stats-off` | presence-only label |
| `digest` | the generator's own evidence digest (`profile_digest`) | `kDigest` d648e85e… | presence-only; both are valid 64-hex |
| `build_source`, `stats_source` | the detector's measured method + evidence | hand strings | presence-only provenance |
| `body_sha256` | computed by the emitter over the canonical body | computed likewise | definitionally differs per file content |

## E. Not derivable from the image at all

| Field | Why | Owner |
|---|---|---|
| `exe_path` (console path of the readable executable copy) | A console/config fact, not in the executable. The generator leaves it out unless `--exe-path` is given. | Team S2 / item B8 (paused for the user's decision) |
| `image_base` confidence | The load base is a PS5 platform convention (medium) until a console observation; every runtime address is medium at best. Pass `--load-base` after a console read to raise it. | S2 |

## F. Engine calibration honesty (UE 4.25–4.27)

No UE4 PS5 executable exists on this Mac, so the detectors (tuned on Townfall's UE 5.6 shapes) could
not be run against a real UE4 image. The generator's design fits 4.25+ (FField/FProperty, FNamePool,
chunked object array) and it **fails closed** on anything it cannot resolve, which is the honest
outcome. Any UE4 failure on Stellar Blade must be fixed at engine-version level, never tuned to that
one image. Confidence for UE4 is therefore **unverified (reasoned only)**; the robustness suite's
synthetic UE4-shaped fixtures pass, but that is not the same as a real image.
