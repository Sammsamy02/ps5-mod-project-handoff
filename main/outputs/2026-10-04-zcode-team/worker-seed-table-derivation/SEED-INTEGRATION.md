# SEED-INTEGRATION — staged-initializer seed path, gates, settings ini, argv, and per-slot validation design

Worker: worker-seed-table-derivation, 2026-10-04. **PROPOSALS ONLY** — no frozen file,
no OwnedMemory unit, no Rust crate, no staged source and no ini was modified; everything
below is for the controller. Static/offline work only; no console contact, no nested
subagents. Workspace root `/WORKSPACE/.codex/worktrees/translation-core/what` is
abbreviated `W:`. Evidence values come from `SEED-TABLE.json` / `derivation.json` in
this directory; the consumed staged unit is
`W:outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp`
(the composition replaces it via the existing `--replace-runtime-unit` mechanism,
`W:outputs/2026-10-04-zcode-team/worker-composition-4-fix/build_runtime_composition4_fixed.py:41,77-84`).

---

## 0. Summary of the change set

| # | What | Where | Blast radius |
|---|---|---|---|
| 1 | Seed constant table + `kSeedRequiredSlots` + per-slot seed assignment | staged `UnrealInitializer.cpp`, ScanGame() | one freshly compiled affected unit |
| 2 | Config gates (remove slots {0,2,3,4,6} from the required mask) | same file, config block | same unit |
| 3 | DoScan-arm gates (skip Lua-override AOB arms when seeds armed) | same file, two arm conditions | same unit |
| 4 | (optional, diagnostics-only) data-seed guard | same file, after seed assignment | same unit |
| 5 | `[EngineVersionOverride]` ini section (slot 6, zero scans) | `ue4ss/UE4SS-settings.ini` staging key | one ini file + manifest re-pin |
| 6 | `--replace-runtime-unit` argv + `--replace-receipt` | composition invocation | receipts only |

Tick (slot 10) needs **no diff**: `bHookEngineTick` + the existing scan→vtable fallback
(staged `:1352-1378`) already handles it once PostInitialize is reachable.

---

## 1. Diff 1 — seed constants and assignment (staged `UnrealInitializer.cpp`)

Insert immediately **before** `PsScanConfig config{};` (the config block at `:585-594`):

```cpp
#ifdef __PROSPERO__
        // REQUIRED-SLOT SEEDS (proposal, worker-seed-table-derivation 2026-10-04):
        // derived OFFLINE against decrypted-eboot.bin sha256 3b6cfad4...622fc
        // (SEED-TABLE.json / derivation.json in worker-seed-table-derivation);
        // runtime = linked + 0x400000 (base proven in-game by the B/D/E/C window
        // anchors and the GUA validation). Identity/ABI UNVERIFIED — same honesty
        // class as the mapped candidate provenance strings
        // (OwnedMappedScanSource.cpp:57-60) until the FC-1 / anchor validators run.
        // Each slot arms independently so the controller can gate a seed behind its
        // in-game validator result.
        constexpr bool kSeedRequiredSlots = true;   // master arm
        constexpr bool kSeedGUObjectArray = true;   // slot 0 — live-validated 0x97ba110
        constexpr bool kSeedFNameCtor     = true;   // slot 2 — FC-1 validator staged
        constexpr bool kSeedStaticConstructObject = true; // slot 4 — sco_anchor staged
        // slot 3 gmalloc: UNSEEDED (under-determined on this image; SEED-TABLE.json).
        // No constant exists to assign; Diff 2 removes it from the required mask and
        // skips its consumer so GMalloc stays null and upstream PostInitialize throws
        // loudly ("PostInitialize: GMalloc is uninitialized.") instead of burning
        // SecondsToScanBeforeGivingUp in a loop that can never succeed here.
#endif
```

Insert immediately **after** `PsScanResults results{};` (`:607`):

```cpp
#ifdef __PROSPERO__
        if constexpr (kSeedRequiredSlots)
        {
            if constexpr (kSeedGUObjectArray)
            {
                results.g_uobject_array = reinterpret_cast<void*>(0x97ba110ull);
            }
            if constexpr (kSeedFNameCtor)
            {
                results.fname_ctor_wchar = reinterpret_cast<void*>(0x63d590ull);
            }
            if constexpr (kSeedStaticConstructObject)
            {
                results.static_construct_object_internal = reinterpret_cast<void*>(0x890aa0ull);
            }
        }
#endif
```

**Why the seeded fields survive:** `unavailable_owned_mapped` clears only REQUESTED
fields (`OwnedMappedResolverCollector.cpp:24-29`) and `pointer()` writes only requested
slots (`:86-89`), so once Diff 2 sets the flags false, the seeded `results` fields are
retained across every ps_scan retry (the loop still runs; it just cannot unsatisfy
itself) and into the consumer assignments (`:812-815`, `:885-895`). With slot 6 removed
by settings (Diff 5), the required mask is empty regardless of scan outcome.

## 2. Diff 2 — config gates (config block, staged `:585-594`)

Append directly after `config.stats = true;`:

```cpp
#ifdef __PROSPERO__
        // Seeds own slots {0,2,4}: each is gated off exactly when its seed boolean
        // is armed (flipping a seed off restores the ScanOverrides semantics set
        // above, i.e. today's behavior for that slot). Slot 6 is NOT gated here —
        // it is owned by the [EngineVersionOverride] settings route (Diff 5): the
        // no-op version_finder override installed at UE4SSProgram.cpp:877 makes the
        // config.engine_version assignment at :585 false on its own, so a missing
        // ini degrades to today's loud behavior instead of a silent version=0.
        // Slot 3 gmalloc is UNSEEDED: removed from the required mask (the scan can
        // never resolve it on this image — 0/14 patterns, string anchors absent,
        // DeleteFile route dead) and its consumer below is skipped, leaving GMalloc
        // null -> loud PostInitialize throw. LOUD, never a wrong-but-silent address.
        if constexpr (kSeedRequiredSlots)
        {
            if constexpr (kSeedGUObjectArray)
            {
                config.g_uobject_array = false;
            }
            if constexpr (kSeedFNameCtor)
            {
                config.fname_ctor_wchar = false;
            }
            if constexpr (kSeedStaticConstructObject)
            {
                config.static_construct_object_internal = false;
            }
            config.gmalloc = false; // UNSEEDED: never resolvable on this image
        }
#endif
```

(A stricter alternative for slot 3 — leave `config.gmalloc` at its ScanOverrides
value so the loop times out exactly as today — is documented and rejected as slower
with the identical failure meaning. If the controller wants a hard dependency on the
ini, an optional startup assert `Version::Major != 0` before ScanGame makes a missing
`[EngineVersionOverride]` fail fast instead of publishing version 0 — proposal note,
not part of the minimal diff set.)

## 3. Diff 3 — DoScan-arm gates

The two Lua-override arms would run AOB containers that read code and fatal after
`SecondsToScanBeforeGivingUp` (throw at staged `:631`).

First pass (`:823-829`), condition becomes:

```cpp
            if ((!ctx.config.fname_tostring_fstring ||
                 !ctx.config.static_construct_object_internal ||
                 !ctx.config.gameengine_tick)
#ifdef __PROSPERO__
                && !kSeedRequiredSlots
#endif
            )
```

Second pass (`:906-915`), condition becomes:

```cpp
            if ((!ctx.config.fname_ctor_wchar ||
                 !ctx.config.gmalloc ||
                 !ctx.config.g_uobject_array ||
                 !ctx.config.ftext_fstring ||
                 !ctx.config.gnatives)
#ifdef __PROSPERO__
                && !kSeedRequiredSlots
#endif
            )
```

The seeds (Diff 1) plus settings (Diff 5) already assigned/owned everything these
arms would scan for on the required-mask path; `config.gameengine_tick` is untouched,
so the tick keeps its normal scan→vtable route at `:1352-1378`.

## 4. Diff 4 (optional, diagnostics only) — data-seed guard

Right after the Diff-1 assignment block, a guard **mirroring `permits_pointer_slot`**
(readable + 8-aligned; semantics of `OwnedMappedImageCaller.hpp:71-73`) for the two
data-class seeds, logged, **never fatal beyond today's behavior**:

```cpp
        // proposal shape; exact lease-access calls to be fixed at compile time:
        // for each seeded DATA address A in {0x97ba110}: A % 8 == 0 and a bounded
        // 8-byte read through the already-registered readable lease succeeds;
        // log "seed guard <slot> <addr> ok|<reason>". No code-address is probed
        // here (slots 2/4 are code: their validation lives in the FC-1 / anchor
        // probes, section 6).
```

## 5. Diff 5 — settings ini (slot 6), exact bytes and restage path

Canonical staged file: activation key **`ue4ss/UE4SS-settings.ini`** (consumed at
`SettingsManager.cpp:60-68`; missing key throws). Current canonical bytes: 779 B,
sha256 `9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f` (verified
against `W:outputs/2026-10-04-zcode-team/worker-loadlist-final/final-package/ue4ss/UE4SS-settings.ini`;
the file ends with `EnableDumping=0\n` — trailing newline present).

**Append exactly these 54 bytes** (no inline `;` comments — parser finding 3 of
`worker-ue4ss-settings/SETTINGS-CONTENT.md`; no comment-only sections — finding 2):

```
[EngineVersionOverride]
MajorVersion=5
MinorVersion=6
```

Resulting file: 833 B, sha256
`5c3af1a475ad9614688612657a0a7da41ee35b0951bf6ec2a18762f9258fb733`
(prebuilt as `UE4SS-settings.ini.seed-proposal` in this directory). Byte-exact
restage path: update the canonical ini in the staging package
(`worker-staging-pkg-c4` / `worker-console-staging-bundle` manifests pin the old
sha — re-pin to `5c3af1a4…`), keeping the activation key `ue4ss/UE4SS-settings.ini`.

Effect: `SettingsManager` parses `EngineVersionOverride.MajorVersion/MinorVersion`
(`SettingsManager.cpp:157-159`); `UE4SSProgram.cpp:858-878` sets
`Version::Major=5 / Minor=6` (both `!= -1` required — satisfied) and installs the
no-op `config.ScanOverrides.version_finder` (`:877`), which makes staged `:585`
`config.engine_version = !ScanOverrides.version_finder` false → required-slot bit 6
never requested. Version 5.6 is the profile-pinned value
(`worker-2-layout-facts/profiles/townfall-ps5-ue56.profile.json`, origin
`engine_version status=0 major=5 minor=6`). Honest note: `worker-ue4ss-settings`
deliberately omitted `EngineVersionOverride.*` as LAYOUT-GATE-sensitive; setting it
here is legitimate **only** as this operator-deliberate, profile-matching override,
with the S2 layout gate (staged `:863-876`) as the downstream correctness witness.

## 6. Composition argv (existing mechanism, no script changes)

Apply Diffs 1-4 to a copy of `W:outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp`
(kept under this worker's `staging/` by the controller), then recompose:

```
python3 outputs/2026-10-04-zcode-team/worker-composition-4-fix/build_runtime_composition4_fixed.py \
  --output <new composition out dir> \
  --rust-receipt <pinned>  --host-receipt <pinned> \
  --replace-runtime-unit unreal-unrealinitializer=outputs/2026-10-04-zcode-team/worker-seed-table-derivation/staging/UnrealInitializer.cpp \
  --replace-receipt outputs/2026-10-04-zcode-team/worker-seed-table-derivation/integration-patch/receipts/replace-receipt.json
```

Mechanics (verified in `build_runtime_composition4_fixed.py`): the target name must be
in the affected inventory — `unreal-unrealinitializer` is (`:61`); a
`--replace-runtime-unit` without `--replace-receipt` raises (`if rt_repl and not
a.replace_receipt`); the receipt is hash-pinned over the replacement source and every
spike project file the replacement touches (`:41`). No other unit, ABI, Rust crate, or
frozen file changes. If slot 3 is derived later, it lands as one more constexpr + one
more assignment in the same unit.

## 7. Per-slot in-game validation design (code-read-free where the class allows)

| slot | seed class | startup/in-game validator | status |
|---|---|---|---|
| 0 GUA 0x97ba110 | data | In-unit guard (Diff 4): alignment + readable via the registered lease (`permits_pointer_slot` semantics — this check alone is code-read-free). Richer optional probe: bounded 96-byte header-plausibility read under the trusted current-vmmap gate, igf4 `gua_read`/predicate discipline (layout A/B never collapsed). | **live-validated already** (71959 objects, read+write, `zcode_anchor_validation_RESULTS` + `zcode_igf6_RESULTS`), guard is belt-and-braces |
| 2 FName ctor 0x63d590 | code | FC-1 probe — **already built** (`worker-fname-runtime-probe`, `igf6-fc1prep-204a6d60a4ed`, 8/8 host scenarios): FC1-A mprotect-first 64-byte read vs captured bytes; FC1-B nine call-site windows re-decoded fail-closed (lea-rsi→anchor, one `BA 01 00 00 00`, `E8 rel32`→0x63d590); `fc1_validated_indicator` requires A(32+64) AND 9/9 sites. Until it runs in-game, the seed keeps the "identity and ABI unverified" provenance string. | built, not yet executed |
| 3 GMalloc | — | **No seed → no validator owed.** Rule enforced: a seed without a validator stays unseeded. Any future derivation must bring a code-read-free data validator (`[addr]` non-null, 8-aligned, heap-range pointer at scan time) before seeding. | unseeded |
| 4 SCO 0x890aa0 | code | igf4-style `sco_anchor`: mprotect-first gated 16-byte read vs expected bytes `554889e54157415641554154534881ec` (the FC-1 mprotect-first technique is the designed fix for the blocked plain read; igf5 `run_anchor` semantics). Until it runs, provenance string stays. | probe pattern built (igf5/igf4 lineage); byte-read blocked to date — never re-probed outside authorized builds |
| 6 version | settings | No address; the S2 layout gate (staged `:863-876`) is the downstream witness. | settings-provisioned |
| 10 tick | n/a | vtable fallback (staged `:1352-1378`) needs no seed; scan-vs-vtable agreement is logged. | existing route |

**Regression/acceptance:** the new "seeds armed, scan skipped" path needs its own
Editor/PlayMode checks (worker-assigned at composition time) plus the query-harness
layout gate as the downstream correctness witness; provenance strings remain
"identity and ABI unverified" until behavioral evidence accrues. Arming is per-slot
(Diff 1 booleans), so any validator failure flips exactly one seed off without
re-opening the others.

## 8. Explicit non-claims

- All diffs are proposals; nothing was applied. No console contact was made by this
  worker; every in-game statement cites existing recorded evidence.
- The seed VALUES are not identity-proven: slot 0 is live-validated; slots 2/4 are
  strong static derivations pending their in-game validators; slot 3 is unseeded.
- The tick convention correction (runtime 0x33ac8c0, not 0x37ac8c0) is a byte-evidenced
  record correction; it does not change any shipped behavior (tick is not seeded).
