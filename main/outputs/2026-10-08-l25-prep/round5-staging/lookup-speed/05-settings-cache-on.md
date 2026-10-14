# 05 — settings enablement reference (deploy-time, NOT a code patch)

Agent IT (Claude round-13 item 2), 2026-10-12. Battery-safe prep only.

## The artifact

The cache-on settings file ALREADY EXISTS and is the MFA size-law shape
(exactly 1947 B, same as the deployed cache-off original):

- `outputs/2026-10-08-l25-prep/evidence/l25-settings-cacheon-1947.ini`
- sha256 `5847362ea7110900b61184aa8e56593afdd32ed8ff8af85ffbafd5d6373f9031`
- the one delta vs the deployed cache-off file: `bUseUObjectArrayCache=true`
  (verify with `diff` against `evidence/l24-settings-backup.ini`, also 1947 B).

## When it may be flipped

ONLY on the labelled fix build that carries patches 01-04 of this directory,
and only after that build's own gating (verify_build.sh manifest, snapshot
binding, decrypt-off readback per the standing PS5 laws). Order matters:

1. Patches 01-03 alone already cut every lookup well below one frame with the
   cache still OFF — they are safe to ship and measure first (one labelled
   build per the mission's bisect discipline).
2. Patch 04 adds the name-keyed cache and SKIPS the engine create-listener
   registration (the GMalloc fault family stays out — see 04's UnrealInitializer
   hunk). Flipping the ini to cache-on activates:
   - the existing searcher pools (unchanged upstream code path),
   - the new name buckets ([LookupCache] log lines, `arm=namecache` in [FFO2]).
3. The ini flip itself is reversible byte-exactly (keep the cache-off backup).

## What cache-on does NOT do on this build

- It does NOT register the engine create listener (`AddUObjectCreateListener`
  stays un-called). Bucket maintenance = init walk + StaticConstructObject
  post-hook + the console-proven engine-side delete listener (variant C).
- It does not touch FUObjectHashTables (still GATE-blocked; `bForceGUObjectArray
  ForIteration=false` is unchanged and harmless while the tables are
  unavailable — `ShouldUseHashTables()` stays false either way).

## Expected console markers on the fix build (add to the build's manifest)

- `[LookupCache] name-keyed FindXOf buckets populated and activated`
- `[LookupCache] engine create-listener registration SKIPPED (engine op not admitted; maintenance = init walk + StaticConstructObject hook + delete listener)`
- `[FFO2] ... arm=namecache` (replaces `arm=walk` for Lua FindFirstOf/FindAllOf once the cache is active)
- self-test: FindFirstOf / FindAllOf / uobj.IsValid / StaticFindObject-short
  lines should lose their `SLOW-ms=` suffixes (16 ms budget).
