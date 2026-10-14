# PC-shader fallback — Leon glasses cause (file-level forensics) + generic design

Agent GM, 2026-10-08. Mac-only, battery-safe, offline. Everything below is
derived from retained local files cited by path; nothing was built, run on
console, or fetched. `.superpowers/` records were read, never written.
Companion prototype: `classify_materials.py` (+ its output
`leon-classification.json`) in this directory.

---

## Part 1 — Cause of the Leon lens checkerboard, from files

### 1a. The shader-platform dimension, enumerated

What constitutes a "platform cook" for a material, per the pinned CUE4Parse
schema retained locally
(`.superpowers/native-dependency-spike/unreal-dependency-compatibility-1/source/CUE4Parse/CUE4Parse/UE4/IO/Objects/FFilePackageStoreEntry.cs`):

1. **Container-header store entry `ShaderMapHashes`** — each package's
   `FFilePackageStoreEntry` in the `FIoContainerHeader` (chunk type 6) carries
   a CArrayView of 20-byte FSHAHash shader-map hashes next to its imported
   package ids (`FFilePackageStoreEntry.cs:23-25`: `ImportedPackages =
   ReadCArrayView<FPackageId>(Ar); Ar.Position += 8; //ShaderMapHashes`).
   With shared-material-shader-code cooks these hashes are the keys the
   runtime uses to fetch the package's shader maps from the platform shader
   library instead of inline export data.
2. **Inline per-platform shader resources in the export bundle** (chunk type
   1) — `FMaterialResource` payloads per EShaderPlatform. PC cooks carry
   DXBC/DXIL containers and `PCD3D_SM5`/`PCD3D_SM6` platform-name records.

Retained byte facts (all reproducible; the prototype re-derives each one and
fails on mismatch):

| Source | Fact |
|---|---|
| Mod container header chunk `156a95060856399000000006` (6048 B, sig `0x496f436e`, v5) | 127 packages, 367 imported-package records, **0 shader-map hashes** — every mod store entry is shader-hash-empty. |
| Base pakchunk0 ContainerHeader `townfall-original-container-header-1/container-header.bin` (2,272,324 B, sha256 `d7b6f7ca…73a2a7`, parsed end-to-end) | 38,439 store entries; 5,387 shader-map hashes; 155,248 imports. 5,114 entries have exactly 1 hash; 33,222 have none. |
| Old PS5 glasses instance store entry (pid `96cd0f10e42af022`) | imports `MM_Prop_Glass_Cutscenes` + a grunge-texture bulk; **1 shader hash `233d367d112a9b677e3e29f960b6d633a97cddf4`** — reproduced exactly, matching the observation record. |
| Published replacement (`MI_Simon_Accessories_Glass`, chunk `22f02ae4100fcd9600000001`, 492 B) | store entry hash-less; imports the new mod-local `M_Invisible` (public-export hash `a48b2ffd18348a31`) — source-replacement semantics, confirmed. |
| Mod `M_Invisible` (`9a19479f0133d75d00000001`, 1,662,774 B) | 296 `DXBC` + 205 `DXIL` + one each `PCD3D_SM6`/`PCD3D_SM5`; **216 `SFI0`, 170 `ISG1`, `OSG1`, `SV_Position`, `DISPATCH` chunks at the first DXBC offset (86671) — these are genuine D3D shader-container chunks, not accidental byte matches.** Zero `SF_PS5`/`SP_PS5`. |
| Mod `M_Leon_Body` (`fd95005df9e3321500000001`, 111,874 B) | 15 `DXBC` + 10 `DXIL`, first DXBC at 9283 with `SFI0`/`ISG1`; `PCD3D_SM6`/`PCD3D_SM5` as name-table records at 2139/2157. |
| Whole mod corpus (127 type-1 chunks scanned) | 5 packages carry inline PC shader payloads: `M_Invisible` ×2 (1,662,774 B and 1,539,341 B), `M_Leon_Body`, `M_Leon_Skin`, `M_Leon_Hair`. 0 packages contain any PS5-named platform token. |
| Base game fallback materials (paths from the five captured base UTOCs; store entries from the captured header) | `/Engine/EngineMaterials/WorldGridMaterial`, `DefaultMaterial`, `DefaultDeferredDecalMaterial`, `RemoveSurfaceMaterial`, `NaniteHiddenSectionMaterial`, and game-side `/Game/FluidFlux/Surface/Materials/M_FluxSurfaceInvisible` all exist in the base TOCs and each carries 1 store shader hash — known-good PS5-cooked fallback/hide material candidates. |
| Sidecar merge (`leon-iostore-adapter-trial-1/overlay/container-header-merge.json`) | merged store has 5,353 hashes vs base 5,387 — the merge dropped **34** base hashes attached to replaced entries, faithfully carrying the mod's empty arrays (`shader_hash_arrays_reencoded: true`). |

Note the heterogeneity that matters for 1b: hash-less base store entries are
normal (33,222 of 38,439 — all 24 mesh packages among the 68 Leon overlaps
are hash-less, and even a base material like `HairDefaultMaterial` has none).
So **a missing store hash is not by itself diagnostic**; the diagnostic is
the combination: mod store hash-less **and** the package's only shader bytes
are D3D containers **and** (for replaced pids) a base entry with a real PS5
hash was overwritten by a hash-less one.

### 1b. Is the missing hash the whole story?

No — it is half of a two-sided absence. The engine can obtain a material's
shaders from exactly two places: the shared shader library (keyed by the
store-entry hashes) or inline `FMaterialResource` data in the package. For
the mod's glasses chain BOTH are absent on PS5:

- the replacement instance's store entry has no hashes (verified in the mod's
  own container header and in the merged sidecar), and
- its parent `M_Invisible`'s inline payload is 100% D3D (`DXBC`+`DXIL`, with
  `SFI0`/`ISG1` container chunks; zero PS5-named tokens).

A PS5 runtime therefore has no SP_PS5 shader map to load for that material —
the old instance's hash `233d367d…` points at a library entry the merged
store no longer references. What the engine does next (default-material
fallback on the mesh section) is runtime behavior, not file content — but
the base game ships `WorldGridMaterial` (grid/checker appearance) and
`DefaultMaterial` natively, consistent with a pale checkerboard fallback
rendering on exactly the geometry whose material lost its shader map. The
observation's README fact (glasses intended hidden) plus the byte chain
(instance → PC-only invisible parent → no hashes anywhere) is coherent:
Simon's retained accessories geometry stays mounted and falls back, instead
of being hidden by a working invisible material.

### 1c. Test prediction + grep list for the next ladder launch (no new trip)

**File-level prediction about the log surface (already checkable, and it is
negative):** the shipping eboot
(`townfall-static-executable-2026-10-02/decrypted-eboot.bin`, 194,241,424 B)
contains **no UE log-format strings at all** — searched: `LogMaterial` 0,
`LogShaders`/`LogInit`/`Log file open` 0, `cached shader`/`shader map`/
`shader library`/`failed to load shader` 0 (case-insensitive, whole file;
only CVar/FName tokens like `ShaderMapDDCMisses`, `CompileGlobalShaderMap`,
`InitDefaultMaterials` exist). Shipping logging is compiled out, and the
game's Saved/Logs paths were already observed `error_perm` /
`logs_captured: 0` (`townfall-existing-log-observation-2026-10-01`). So
**no engine shader-miss line will appear in klog, whatever the cause** —
grep for it anyway once, to convert this prediction into a recorded negative.

Grep list for the next session's klog (captures per `agent-T-klog-capture.md`
conventions — TCP 3232 tap, bracketed by `[klog-capture <UTC>]` markers):

```sh
# 1) expected-NEGATIVE (engine shader-miss surface; eboot strings prove it
#    is stripped — record the zero, never treat it as "no defect"):
rg -in 'LogMaterial|LogShaders|LogShaderLibrary|Missing cached shader|\
shader map|shader library|WorldGridMaterial|M_Invisible' klog-*.log

# 2) launch/save-load window anchors (correlate the moment the lens is on screen):
rg -n 'launchApp\(PPSA33286\)|Big App started|libSceSaveData|atime:|klog-capture' klog-*.log

# 3) regression guards around any fallback-overlay A/B launch (the substitution
#    overlay itself must not add faults):
rg -n 'A user thread receives a fatal signal|proc name:|rip: |copyin|nonsleeping|fault address|eboot\.bin' klog-*.log

# 4) our own runtime's markers if the observer build is in the session
#    (material-name tags from the modbridge/zzdiag line prefixes):
rg -in 'zzdiag|modbridge|ue4ss' klog-*.log
```

The **decisive** console-side confirmation is not a log line but a behavioral
A/B: launch once with the Option-A/C substitution overlay (below), same save,
byte-identical inputs — if the lens checkerboard disappears while an
unchanged control keeps it, the shader-platform cause is confirmed
operationally. That launch belongs to the ladder, gated as always; nothing
here requests a new trip.

### 1d. Verdict

**STRONGEST-HYPOTHESIS (file level), not CONFIRMED-AT-FILE-LEVEL.** Files
prove, exactly: the mod ships zero PS5-consumable shader data for the glasses
chain (no store hashes; inline payloads are D3D-only), the base's working
PS5-store entry was replaced by a hash-less one, and known-good base
fallback/invisible materials exist. Files cannot prove: which engine path
produced the observed pale checkerboard (default material vs missing
texture), that the lens geometry is the Simon accessories component, and why
Leon's own PC-cooked materials appeared acceptable in the same screenshot
(the same DXBC evidence applies to `M_Leon_Body`/`Skin`/`Hair`, so the
engine's per-case tolerance is an open residual). Residual unknowns are
exactly: (1) runtime fallback branch, (2) component identity, (3) why
PC-inline added materials differed visibly from the replaced-instance chain.
The 1c A/B test resolves (1) and (2) in one launch; (3) needs an
engine-observer hook, not more files.

---

## Part 2 — Generic fallback design for mods that ship PC-only material shaders

Law: original mod files are never modified. All substitutions are DERIVED
overlay entries produced at install time (GK's overlay builder carries them);
the analyzer only emits directives.

### Option A — SUBSTITUTE the base game's equivalent export ("the game's own
shader for the same slot")

* **Trigger:** a mod package that *replaces* a base package (same package id
  present in the base container store / TOCs) AND whose replacement is
  material-family AND shader-broken (PC-inline payload, or parent-chain to
  one, or hash-less store entry where the base entry had hashes).
* **Matching rule:** package id first (exact 64-bit id, the same key the
  engine's package store uses); corroborate by export-name equality of the
  primary export and class type (`MaterialInstanceConstant` vs `Material` —
  class resolution needs the install-side export map, available to the
  builder; the Mac prototype approximates by name role). A pid match with a
  different export name is a manual-review flag, not a substitution.
* **Mechanics:** *suppression, not bytes*: drop the mod's store-entry
  replacement for that pid (keep the base entry, restoring its shader hashes)
  and drop the mod chunk rows for it — the engine resolves the package from
  the base container. No derived package needed. (The merge already proved
  store-entry surgery is byte-exact and reversible.)
* **Safe when:** invisible-intent (net effect identical to C), decals
  (`DefaultDeferredDecalMaterial` slots), simple-lit/UI materials, or when
  the mod's change to that slot is parameter-only. **Unsafe → warn-only when
  the mod deliberately re-skinned the slot** (custom surface look): restoring
  the base export silently reverts the mod's visual intent. The directive
  carries `look_loss: true` and defaults to *no change* unless the user opts
  in per mod (installer policy).

### Option B — CONVERT: cook a PS5 shader map at install — **REFUSED**

Cooking a valid SP_PS5 `FMaterialResource` requires Epic's console shader
compiler backend, which is distributed only under Sony's licensed PS5 SDK
(the same NDA-gated toolchain whose redistribution is prohibited; this
project has no PS5 license, and Mac cannot host it regardless). Any locally
synthesized "shader map" would be a fabricated payload, not a cook — that is
excluded by the never-fabricate rule. **Option B is refused on licensing and
integrity grounds, not deferred.** (The honest, legal partial path —
texture-only swaps via BC block permutation — is already GF's Phase A #8 and
does not touch shaders.)

### Option C — HIDE: substitute a known-good base invisible material

* **Trigger:** invisible-intent, detected from bytes: the package's own
  object name contains `Invisible`, or its mod-local parent chain does (the
  graph shape the observation documented: `MI_Simon_Accessories_Glass →
  M_Invisible`, parent has zero imports + large D3D payload).
* **Candidate pool (verified present with PS5 store hashes):**
  `/Game/FluidFlux/Surface/Materials/M_FluxSurfaceInvisible` (game-side,
  preferred), `/Engine/EngineMaterials/RemoveSurfaceMaterial`,
  `DefaultDeferredDecalMaterial`, `WorldGridMaterial`, `DefaultMaterial`.
* **Mechanics, in preference order:**
  1. If the *base game itself* has an invisible material at the same or
     sibling path (base did NOT have `M_Invisible` at the mod's Simon
     Materials path — checked), use the base's own.
  2. Else repoint: emit a DERIVED copy of the mod instance package whose
     import record targets the candidate's package id + public-export hash
     (builder-side rewrite; originals untouched). Keep the mod's texture
     parameters — an invisible parent ignores them.
  3. Else fall back to Option A suppression (geometry shows vanilla
     material; visible but native) — never leave the PC parent in place.

### Decision ladder (automated from package bytes)

```
for each mod material-family package P:
  broken  = PC-inline tokens > 0            # direct DXBC/DXIL/PCD3D evidence
          | ancestor-chain (mod-local) hits a PC-inline package
  replaced = P.pid in base container store (or base TOCs)
  invisible = 'invisible' in P.name or any dependency name
  if not broken: PASS
  elif invisible and candidate exists: HIDE_VIA_BASE_INVISIBLE   (C, then A)
  elif replaced:                     SUBSTITUTE_BASE_EXPORT       (A; look_loss flag)
  else (added, no base equivalent):  NEEDS_REPOINT_OR_DROP        (warn; Leon-own
                                     materials land here — never silently changed)
meshes: FLAG-MESH-PATH-BLOCKED (GF #8 recook research, separate track)
textures/others: PASS (BC permutation path already covers texture swaps)
```

### Leon-set result (prototype run, `leon-classification.json`)

| Count | Class |
|---|---|
| 127 | mod .uasset packages scanned (15 pinned checks reproduced exactly) |
| 59 | material-family; **all 59 are shader-broken** (5 PC-inline: `M_Invisible`×2, `M_Leon_Body`, `M_Leon_Skin`, `M_Leon_Hair`; 54 chain-to-PC-parent) |
| 66 | replaced pids in the captured pakchunk0 store (+2 overlap rows in pakchunk10 whose header is not captured: one mesh, one broken invisible-intent material) |
| 68 | overlap rows total = **43 broken material-family** (42 HIDE — every one invisible-intent, matching the mod's hide-Simon design) **+ 25 meshes/others** |
| 24/68 | overlap rows whose base store entry still shows a PS5 shader hash (24 of the 43 materials) |
| 17 | added broken materials (Leon's own look: `MI_Leon_*` + parents) → NEEDS_REPOINT_OR_DROP, warn-only |
| 44 | PASS (34 textures + 10 other) |

Notable: for the Leon set, A and C coincide into one dominant action — the
mod's whole Simon-material overlay is a hide operation, so the correct
default install is: repoint/suppress the 42 (+1 pakchunk10) invisible-intent
replacements toward `M_FluxSurfaceInvisible` (or suppress to base), and leave
Leon's own 17 materials untouched-with-warning (their visible quality hinges
on residual unknown (3) above — the engine's tolerance of PC-inline added
materials, which the same screenshot suggests is partially forgiving).

---

## Part 3 — Implementation sketch

`classify_materials.py` (this directory) is the analyzer prototype: pure
stdlib, read-only inputs, hard-fails unless every pinned fact (store totals,
glasses entry, four token records from the observation, per-TOC file counts,
68/68 overlap rows) reproduces. Install-time pipeline:

1. **Intake** — mod UTOC/UCAS unpack + base ContainerHeader/TOC capture (all
   stages already exist and are tested: toc reader, none-packer,
   container-header merge).
2. **Analyze** (this tool) → `directives.json` rows:
   `{package_id, action: hide_via_base_invisible|substitute_base_export|
     needs_repoint_or_drop|pass|flag, substitute_game_path?, look_loss,
     evidence: {tokens, chain, store_hashes}}`.
3. **Build** (GK's overlay builder, on-console at install): executes
   suppressions as store-entry/chunk-row drops; repoints as derived instance
   packages written into the derived overlay only; verifies by re-parsing the
   derived container and re-running the analyzer against it (expect zero
   `broken` verdicts among replaced rows).
4. **Accept** — the 1c A/B launch ladder; visual gate x3 across restarts per
   the standing acceptance law.

Honest bounds kept in-band: the prototype's `PS5-OK` is never asserted from
mod bytes (only absence of PC evidence); pakchunk10's uncaptured header is
flagged, not guessed; base bodies (128 GB UCAS) were never read — base
PS5-ness rests on the game shipping and running them, plus their store
hashes, not on decoded base payloads.

## Provenance

Inputs (read-only): `extracted/zzz_Leon_Townfall_2K_P.{utoc,ucas}`,
`leon-iostore-adapter-trial-1/raw/chunks/*` (162 chunks),
`townfall-original-container-header-1/container-header.bin`,
`townfall-main-utoc-2026-10-01/pakchunk{0,1,10,11,12}-ps5.utoc`,
`unreal-mod-local-dependency-closure-1/proof-3/graph.json`,
`townfall-static-executable-2026-10-02/decrypted-eboot.bin`,
`.superpowers/native-dependency-spike/leon-material-observation-1/*` (the
record this work strengthens). Created files: only
`shader-fallback/{classify_materials.py,leon-classification.json,fallback.md}`
and `agent-GM-shaderfallback.md`.
