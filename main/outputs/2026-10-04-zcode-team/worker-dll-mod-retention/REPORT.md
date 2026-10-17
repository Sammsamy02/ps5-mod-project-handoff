# REPORT — worker-dll-mod-retention (2026-10-04)

Deliverables in this directory:
`INVENTORY.md`, `RETENTION-CRITERIA.md`, `ACQUISITION-PATH.md`,
`inspect_dll_census.py`, `baseline-dll-census.json`, `ue4ss-zip-census.json`,
`CLAIM.json`.

## 1. Inventory (INVENTORY.md)

**No third-party native DLL mod is retained anywhere in the workspace.** Whole-
tree `find` for `*.dll` returned only first-party artifacts (UE4SS
distribution `dwmapi.dll`+`UE4SS.dll`, project-built `worker.dll` /
`SharpProspero*` builds, bridge fixtures). `unzip -l` PE-scan of the complete
8-zip worker-6 corpus: only `ue4ss-v1.zip` contains PEs — our own toolchain;
the other 7 are asset/logic container mods. No download_store exists under
`work/` (only untouched credentials + pinned types). Exact terms and negative
results are tabulated in INVENTORY.md.

## 2. Retention criteria + candidates (RETENTION-CRITERIA.md)

Six criteria (UE4/UE5 host game with public Nexus DLL mods; installs a runtime
native DLL — not asset-only; popularity/maintenance; source or documented
requirements; license permitting interoperability retention; cover both load
shapes). Five concrete candidates proposed: FF7RFix (FF7R, UE4.27, dxgi/version
proxy), P3RFix (P3R, UE4.27, dinput8 proxy), SML bootstrap (Satisfactory,
UE4→UE5, native loader DLL), ReShade (per-game UE pages, dxgi/d3d11 injection),
and a UE4SS-hosted Palworld (UE5) native mod for the loader shape. Caveat: no
network here — names/licenses/download ranks are from public knowledge and must
be re-verified at acquisition; the doc says so explicitly.

## 3. Acquisition path (ACQUISITION-PATH.md)

Fully cited 8-step controller procedure built on the two receipt-backed owners:
`native_nexus_download_jobs` (plan/ledger/manifests, commit fcc16d5…, 21 actual
TLS transfers, explicit-config-only, process-lifetime credentials) and
`callable_archive_download_transport` (`mb_curl_runtime_start_v1` /
`mb_curl_download_v1` / `mb_curl_runtime_close_v1` from
`outputs/ps5-mod-bridge/native-host/download/`, exclusive curl/PSA ownership,
1 GiB sink, 64 KiB carry). Metadata schema mirrors the verified `app-host-5`
manifest (attributions + per-input bytes/sha256) plus mod provenance and
readback-verified receipt; retention dir convention
`.superpowers/native-observations/dll-mod-retention-<date>-1/<slug>/`.

## 4. Inspection tooling + baseline (ready for bytes)

`inspect_dll_census.py` — pure-stdlib, no network, in-memory zip parsing
(nothing extracted, nothing executed). Given archives/files/dirs it censuses
every PE DLL: full import table (name or ordinal, per library), delay imports,
bound-import flag, exports, static-TLS directory (with VA/RVA callback
resolution), CRT-startup imports with roles, CRT runtime DLLs imported,
static-CRT heuristic, and family classification into the project's 12 families
+ uncategorized. Output: census JSON.

**Baseline (retained dwmapi.dll + UE4SS.dll, also refreshes the backlog with
exact coverage numbers):**

| DLL | bytes | sha256 (16) | imports | family census |
|---|---|---|---|---|
| dwmapi.dll (our proxy) | 279,552 | 4e3e5324e68e033e | 135 | crt 104, uncategorized 12, fileio 6, loader 4, heap 4, workers 3, clock 2 |
| UE4SS.dll (our runtime) | 20,572,160 | 94c2ebd3ca9f4c42 | 634 | crt 390, uncategorized 152(+3 ordinal), fileio 21, heap 18, loader 10, workers 8, tls 7, clock 6, srw 6, address-wait 5, critical 4, mutex 2, init_once 2 |

- **UE4SS.dll exercises ALL 12 backlog families** (mutex=2, fileio=21 —
  consistent with mutex being worker-delivered and fileio in-flight); the
  retained corpus already proves full-family coverage, so a third-party mod's
  value is breadth/new-API delta, not family discovery alone.
- **Static TLS**: UE4SS.dll yes — 728 B raw TLS data, AddressOfIndex present, 2
  TLS callbacks (fields VA-encoded; tool resolves both VA/RVA). dwmapi.dll: no.
- **CRT startup**: both import ucrt (`api-ms-win-crt-runtime-l1-1-0`) with the
  full startup set (`_initterm`, `_initterm_e`, `_initialize_narrow_environment`,
  `_initialize/_execute_onexit_table`, `_register_onexit_function`, `_crt_atexit`,
  `_cexit`); neither statically links CRT.
- **Proxy confirmation**: dwmapi.dll re-exports 198 functions / 143 named
  (DWM forwarding shim); UE4SS.dll exports 4,256 (mod API).
- **Archive-mode proof**: census of `ue4ss-v1.zip` (21 entries scanned)
  reproduces the raw-file census byte-for-byte — ready for real Nexus zips.

Tool-call budget: 22 of 35 used. No network, no credentials touched, no files
modified outside this worker directory.
