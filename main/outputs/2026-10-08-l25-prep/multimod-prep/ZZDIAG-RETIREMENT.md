# ZZDIAG-RETIREMENT — exact removal state for the diagnostic mods

Agent-EF, 2026-10-08. Task 5 of the multimod-prep packet. Governs the moment
the proving ladder is ACCEPTED (CK rungs 0–4 green for TownfallItemSpawner):
the zzdiag diagnostic mods leave the console permanently so the multimod
milestone (TEST-PLAN.md) starts from a clean, diag-free composition.

## What zzdiag is, and which version may be resident

`zzdiag` is the studio-authored diagnostic mod (NEVER a third-party mod) in
two files under the mod root `/data/modbridge/roots/PPSA33286/`:

- `ue4ss/Mods/zzdiag/Scripts/main.lua`
- `ue4ss/Mods/zzdiag/enabled.txt` (0 B)

Known versions and their exact sizes/hashes (local archives, all under
`outputs/2026-10-08-l25-prep/`):

| Ver | Bytes | sha256 | Local archive |
|---|---|---|---|
| v2 | 1,107 | (console-resident baseline of the l22/l23 era) | console only |
| v3 | 2,314 | 2670e493d1c89d1f1499d82aad5f9dbd7d1f0fc85aa8798d5e9983f639fa8228 | `zzdiag-v3/main.lua` |
| v7 | 2,487 | 6c7624c5114f2cf83ddaec18fbcafa4285bd828e4fb55ef5643a973d044ae4ac | `zzdiag-v7/main.lua` |

(Intermediate v4–v6 existed in session history; the staged sidecars in this
worktree are v3 and v7. It does not matter which version is resident — the
removal state below is identical for all.)

## Removal procedure (execute AFTER acceptance, BEFORE multimod launch M1)

1. **Pull the live loadlist first** (the sidecar law — never blind-edit):
   `curl -sS ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/loadlist.txt -o /tmp/loadlist-pre-retire.txt`
2. Reconcile against the expected 14-line shape (4 header + 1 mod + 9 file
   rows, 545 B for the v7/v3 staging). If the live file already lacks zzdiag
   rows (a phase-5 removal already happened), skip to step 5 verification.
   If any OTHER row differs from the reference, keep LIVE values.
3. **Delete exactly these two lines** (the only zzdiag rows ever admitted):
   `file	lua_mod	ue4ss/Mods/zzdiag/Scripts/main.lua	<live-size>`
   `file	lua_mod	ue4ss/Mods/zzdiag/enabled.txt	0`
4. Upload the edited 12-line file; verify:
   `cmp <(curl -s ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/loadlist.txt) /tmp/loadlist-post-retire.txt`
   and `grep -c $'\t'` = **7 file rows**.
5. **Delete the remote files**:
   `curl -sS -Q "-DELE /data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua" ftp://ps5-host-not-configured.invalid:2121/`
   `curl -sS -Q "-DELE /data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/enabled.txt" ftp://ps5-host-not-configured.invalid:2121/`
   If the server rejects the leading-dash form, retry without the dash; if
   DELE is refused entirely, LEAVE the files — unlisted lua files are never
   admitted (MFA law) — and record it in the receipt.
6. Receipt entries: pre-retire loadlist copy + hash, the two deleted rows
   verbatim, post-retire readback cmp result, DELE outcomes, timestamp, and
   the acceptance label it follows (e.g. `l26-menu-1`).

## Exact post-retirement state (the multimod base composition)

```
modforge-loadlist	1
title	PPSA33286
engine	unreal
framework	ue4ss-ps5
mod	TownfallItemSpawner	1.0.0
file	config	ue4ss/UE4SS-settings.ini	1947
file	config	ue4ss/MemberVariableLayout.ini	588
file	config	ue4ss/VTableLayout.ini	216
file	config	ue4ss/PS5Pad.ini	3718
file	lua_mod	ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua	21415
file	lua_mod	ue4ss/Mods/TownfallItemSpawner/enabled.txt	0
file	lua_mod	ue4ss/Mods/shared/UEHelpers/UEHelpers.lua	9962
```
= 12 lines, 7 file rows (4 config + 3 lua_mod), no zzdiag anywhere.

Verification signature in every FUTURE session (the retirement is proven by
absence): no `[zz]` tokens of any kind (`Z0`, `K1`, `S1r`, `A1r`, E/F
chunks…), and `harvest.py`'s `diag-zz` verdict reads ABSENT (CK verified
this exact backward-compatible behavior — an ABSENT verdict on a diag-free
log is expected, never a harvest failure).

## Permanent rules after retirement

- **Never re-upload any zzdiag version.** The local v3/v7 archives stay as
  terminal evidence of the diagnostic era; they are not mods and have no
  role post-acceptance. If a future regression needs stepwise probing, it
  gets a NEW diagnostic with a NEW distinct ID under a dated output dir and
  its own loadlist lifecycle (the same law that governed v3→v7).
- The last zzdiag-bearing UE4SS.log/klog pair is the terminal diag record —
  preserve it raw alongside the acceptance harvest.
- Nothing else is touched by retirement: PRX/settings/pad/spawner rows all
  stay exactly as acceptance left them; multimod deltas start from the
  12-line base above (TEST-PLAN.md).
