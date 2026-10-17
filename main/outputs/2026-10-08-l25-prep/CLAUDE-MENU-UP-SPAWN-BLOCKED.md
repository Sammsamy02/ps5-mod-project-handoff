# TOWNFALL MOD MENU IS UP — full session report + the one remaining blocker

2026-10-10, session PRX efbfc656 (build l26-20261010T043709Z, snapshot
24025929). This is the Claude round-2 build: bridge + parameter marshaling +
[FFO2] oracle + [BUILD] stamp + toolchain probes, all provably composed
(13/13 manifest markers verified in the PRX before upload).

## 1. THE HEADLINE: THE MOD MENU POPS UP

User launched Townfall, loaded a save, got in-world, and the
TownfallItemSpawner menu OPENED. Log proof, in order:

```text
[Console log excerpt omitted from the private source handoff.]
```

Every oracle from your round-2 directive landed:
- [LUA-FN] id=0x31AAD for PlayerController — EXACTLY the [ST] self-test
  index you specified. The string seam is FIXED at the FName layer.
- [FFO2] oracle live: `q='GameEngine' id=0x47036 scanned=35479 exact=2
  first=0x19000390A0 arm=walk` — iteration works, real addresses.
- The FULL menu path (your dry-run ladder) passes 6/6, twice (F7 rerun).

## 2. TOOLCHAIN VERDICT (your two self-test lines)

```
[ST] wcslen-16units expect=16 got=16 stride=2     <- PASS
[ST] wstring-rt size=19 content-match=yes         <- PASS
```

wcslen walks 2-byte wchar_t CORRECTLY at runtime; std::wstring
assign/push_back/append round-trips byte-identical. The "prebuilt libc++
4-byte stride" theory is DEAD for wcslen/wstring themselves. The remaining
wide-libc suspicion narrows to whatever ONE path still refuses (below) —
it is NOT a global toolchain defect.

## 3. THE ONE REMAINING BLOCKER: spawn configure fails at ItemId read

User browses the menu, selects an item, spawns — actor spawns,
PickupInteractable component is FOUND (the check before passes), then:

```
[2026-10-10 00:55:47] [Lua] [TownfallItemSpawner] Failed at configure pickup.
  See UE4SS.log. ...main.lua:317: Spawned pickup has no valid inventory ID.
```

The mod's code (main.lua:311-317):
```lua
local actualID = ""
pcall(function()
    actualID = str(pickup.PickupInventoryItem.ItemId)   -- <- fails HERE
end)
if actualID == "" or actualID == "None" then
    error("Spawned pickup has no valid inventory ID.")   -- line 317
end
```
str(v) = plain string? v : pcall v:ToString() or "".
The pcall SWALLOWS the real error — we don't yet know if the read throws
or returns "None".

## 4. THE SMOKING GUN: one seam refusal still alive

```
[2026-10-10 00:54:35] [FN] seam refused: reason='embedded' len=4 at=1
  (NAME_None produced; embedded NUL in a wide buffer is the
  stride-corruption signature)
```

Fired ONCE (once-per-reason notify) at exactly the item-scan moment
(00:54:35 = "24 spawnable pickups found"). Something on the DATABASE-READ
path still produces an interleaved-NUL wide buffer of length 4 — the old
corruption signature, now correctly REFUSED and turned into NAME_None
instead of crashing. len=4 with embedded NUL at 1 = e.g. 'X\0Y\0' — a
2-byte-chars-as-4-byte-units read.

## 5. Marshaling coverage evidence ([LFB-M] first-10 families)

FIRED (each capped at 10):
- fname-narrow ×10: in='Minimum'/len=7 ok=1, 'X', 'Y', 'Maximum' —
  struct-field-name reads during the scan, all OK.
- fstring-fwd ×10 (ok=1)
- ftext-fwd ×10: in='test'/len=4 ok=1 (our DRY-6 SetText rides this)

NEVER FIRED (zero lines): fstring-rev, ftext-rev — the REVERSE families
(engine FString/FText -> Lua string via the bridge) have zero hits. The
ItemId read is exactly a reverse-direction read (FName/FString property ->
Lua string). Reads appear to return through a DIFFERENT converter that
still carries the seam. This is where we look next.

## 6. What we believe + next diagnostic (tell us if you'd change it)

Hypotheses for `pickup.PickupInventoryItem.ItemId` failing:
H1: PickupInventoryItem struct property read returns an unusable
    value -> indexing throws inside pcall -> actualID stays "".
H2: .ItemId returns an FName userdata whose ToString path hits the same
    'embedded'-NUL seam that refused during the scan -> "None".
H3: reverse property reads (FName/FString/FText -> Lua) route through a
    pre-bridge converter (unrouted site) — supported by zero reverse
    [LFB-M] lines.

NEXT STEP (no rebuild needed): instrument the MOD's main.lua (Lua-only,
30-second deploy) to replace the swallowing pcall with verbose logging:
type(pickup.PickupInventoryItem), type(.ItemId), raw tostring, and the
pcall error text. One more launch names the exact failing hop.
Alternative/companion: route the reverse property-read converter sites
(FName/FString/FText property reads in NativePropertyReadBackend /
CustomProperty / LuaUObject __index return path) through the bridge's
push_fstring_to_lua/from_fname — same treatment as round 2's forward
sites. Your call which first.

## 7. Progress ledger since your last review (all console-verified)

1. Phantom-build class SOLVED: the lfb PRX never contained the bridge
   (0/199 .d refs, 0 markers in PRX, stale source pin c2d09abf vs disk
   9fea2963). Fixes: CHANGES files[]+changes[] synced; NEW_UNITS driver
   registration (new TUs can't ride CHANGES rows — no BASE unit exists);
   link-argv injection for new objects; census 391->394 dynamic.
2. Build discipline (your Q4) all landed: [BUILD] first-line stamp
   (compile-time #error if generated header misnamed — this caught a real
   FF/FG contract mismatch mid-build), per-fix declared-marker manifest
   with verify_build.sh --manifest (13/13 PASS before upload; the old
   phantom PRX fails it 7/13), frozen snapshot 24025929 (tar+manifest,
   not a git repo), pins re-baselined in pipeline-pins.json (FH) instead
   of waivers.
3. Q1 marshaling landed (agent FD): to_fstring/to_ftext param pushes +
   push_fstring_to_lua/push_ftext_to_lua/fname_to_narrow/wide_to_narrow;
   routed UFunction string-arg path (strproperty/nameproperty/
   textproperty Set), Out-param table keys, 4 struct field-key sites,
   new LuaFText.cpp (FText ctor, ToString member, __eq rewritten off the
   corrupting FText::ToString seam).
4. Q2 [FFO2] landed (agent FE) + your oracle narrowed by code reading:
   the match predicate is FName::Equals = ComparisonIndex ONLY (Number
   never compared, NameTypes.hpp:471-474); derived classes match via
   SuperStruct walk. Q3 fault-ledger: parked per your "AFTER the menu".
5. Toolchain probes (your addendum): both PASS — see section 2.
6. THE MENU PATH END-TO-END: engine init -> slot-2 ctor -> decoder ->
   bridge -> FindFirstOf/StaticFindObject -> UMG Create -> SetText ->
   mod scan (24 pickups) -> F7 menu visible IN GAME.

## 8. Multi-mod generality (your ask — queued after spawn works)

12 candidate UE4SS Lua mods are staged for generality testing once the
ItemSpawner spawns. Test ladder per mod: loads without uninstall ->
keybinds fire -> FindFirstOf-class lookups resolve -> UI (if any) via the
proven Create/StaticConstructObject path -> mod-specific marshaling.
Each gets the same harvest.py verdict treatment. The bridge + marshaling
layer should make most name-based mods work as-is; mods calling exotic
UFunctions orSpawnActor paths may surface the next marshaling family.

## 9. Console/build state

- PRX efbfc656 live (fake-signed 4f153d1d, decrypt-off readback verified),
  settings original cache-off 1947 B, zzdiag v10 resident (auto-probe +
  M1-M6 + F7 rerun), ploader armed fresh, klogsrv restarted (died in a
  sleep cycle; wakeup restores), klog taps running.
- Session log: evidence/claude2-menu-session-20261010.log (mixed narrow/
  wide encoding — grep -a). Mod's console main.lua archived.
- Artifacts: 2026-10-08-l26-package-A (FINAL-RECEIPT, 48/48 gates),
  snapshots/snapshot-20261010T042420Z.tar.gz (24025929), agents
  FD/FE/FF/FG/FH/FI reports + PREFLIGHT-WAIVER + agent-EZ-incremental.

## 10. QUESTIONS

Q1. The reverse-read plan (section 6): instrument-mod-first (one launch,
    names the hop) or route-reverse-sites-first (rebuild ~25 min, maybe
    fixes it blind)? We lean instrument-first since the pcall hides the
    failure mode.
Q2. The 'embedded' len=4 refusal: any guess which call site produces a
    4-unit interleaved-NUL wide buffer during DB row reads? Candidates:
    GetItemIDFromDeprecatedEnum UFunction return marshaling, or the DB
    row struct FName field read.
Q3. wcslen/wstring both PASS — do you agree the remaining work is purely
    call-path routing (no toolchain-level fix needed), i.e. the u16string
    StringType rebuild is now optional hardening, not a blocker?
