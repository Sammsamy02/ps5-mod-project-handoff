# CRITICAL FINDING for Claude — runtime decode returns 1 char

## Evidence (session 17:42-17:55, PRX 961aa8ca, save loaded, user in-world walking)

**The SELF-TEST at init: ALL 19 PASS with FULL decodes:**
- [ST] fname PlayerController PASS id=0x31aad blk=3 decoded='PlayerController'
- [ST] fname ItemDatabaseSubsystem PASS id=0x39b47 blk=3 decoded='ItemDatabaseSubsystem'
- [ST] sfo /Script/UMG.Default__WidgetBlueprintLibrary PASS 0x3000bda3c0
- [ST] ffo Engine PASS / [ST] ffo GameInstance PASS

**At RUNTIME (same session, after save load, in-world):**
- [FFO] query-fallback [n:1DA05 num:0 ts:'I']  ← decoded "I" not "ItemDatabaseSubsystem"
- [FFO] query-fallback [n:1DA13 num:0 ts:'P']  ← decoded "P" not "PlayerController"  
- [FFO] query-fallback [n:1D9FD num:0 ts:'C']  ← decoded "C" not "..." 
- v8 auto-probe: db=i pc=i CA=-1 persistently
- Mod: "Item database not ready" on every press
- Game IS running (churn climbing 2→349→742→744, listener callbacks arriving)

## The specific bug
The pool decoder (NameTypes.cpp FNamePoolDecoder) decodes correctly at INIT
(full names, correct block numbers) but produces ONE CHARACTER per name at
RUNTIME when called from the game thread (inside ExecuteInGameThread
callbacks / the FindFirstOf path). The FFO query-fallback's round-trip
verify sees ts:'P' ≠ 'PlayerController' → falls to the string-compare
fallback → which ALSO uses the broken runtime decode → compares 'P' against
class names → never matches → FindFirstOf returns nil.

## Possible causes (for Claude to discriminate)
1. Thread-safety: the decoder's byte-wise volatile reads hit the pool from
   the game thread while the engine's own FName code reads/writes the same
   pool concurrently — a torn read of the entry header (len field) would
   produce a 1-unit decode.
2. The pool GREW between init and runtime (new blocks). The decoder's live
   CurrentBlock/CurrentByteOffset reads may return values from a stale
   cached address rather than the true live pool, making the OOB guard
   truncate multi-block entries.
3. The u16→u8 conversion path in the decoder's char loop may be counting
   units vs bytes differently when the entry header was read from a
   game-thread-torn state.

## What we need
The decoder needs to be thread-safe against the engine's concurrent pool
access, or the reads need to be retried/validated (read header twice,
compare; re-read if changed). The engine's own FName code uses a read lock
(shard RW lock) — our decoder reads without it.
