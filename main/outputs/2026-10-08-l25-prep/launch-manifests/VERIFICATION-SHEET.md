# VERIFICATION-SHEET — every launch-manifests artifact, pinned

Agent GQ (ZCode), 2026-10-10. All sizes = `wc -c` of the exact file in
this directory; all sha256 computed this session (shasum -a 256). GB's,
GC's, GG's, FV's and FJ's source files are read-only and hash-pinned
below exactly as they landed (nothing was edited).

## 1. Artifact inventory

| file | bytes | sha256 | role |
|---|---|---|---|
| `MANIFEST-LAUNCH-1.md` | 13342 | 96c727fc2d80a64a... | launch-1 manifest (round-4 spawn + zzdiag-v11 + ueselftest) |
| `launch1-loadlist.txt` | 1292 | 17b65436f5d1a12e4bc22b297c4c00159d268fcd155bef6057c157bb85a17c53 | launch-1 loadlist reference (27 lines / 22 file rows) |
| `launch1-loadlist-rollback.txt` | 545 | 8f86dfb7ca1554270fcf15975389ac7d37bab913980daffda25107f5d111d2f1 | launch-1 rollback (14 lines / 9 file rows; pad row back to 3718) |
| `launch1-PS5Pad.ini` | 1736 | 957d3e25e3c4802af94b1f561d247cc4e7a383193520bb94b4fab1b98f7ed453 | launch-1 pad upload (byte-copy of GB's `ueselftest/PS5Pad-round4-proposed.ini`) |
| `MANIFEST-LAUNCH-2.md` | 11916 | f37009f97d30ecb4... | launch-2 manifest (ladder v2 four type-mods) |
| `launch2-loadlist.txt` | 1766 | 314d1bccdf591f020d9b13193bf6c1a8bfaceaa27ff8babedfd1b5cf99b94977 | launch-2 loadlist (35 lines / 30 file rows; modmanager-synthesized) |
| `launch2-PS5Pad.ini` | 1736 | a6313eec781173df94a121d3925768a68eca149deb57cbb15940bde1da7ae981 | launch-2 pad upload (modmanager-regenerated; 7 bindings) |
| `launch2-loadlist-rollback.txt` | 1292 | 17b65436f5d1a12e4bc22b297c4c00159d268fcd155bef6057c157bb85a17c53 | launch-2 rollback = launch-1 state (verified: disable-path synthesis reproduces it byte-identical) |
| `launch2-deploy-plan.txt` | 3835 | 3d40391b4325d87cb84a6809306494787c48162079bd845f0320e210652f9ec1 | verbatim modmanager dry-run upload plan + audit receipt (0 violations / 0 warnings) |
| `launch2-modmanager-manifest.json` | 8196 | caa0fb0b6c7291bb05f91adc1629f7dbcf83f5ffe202f1b87abdb90d4018fcc8 | store manifest from the /tmp synthesis (per-file size+sha of all 6 store mods) |
| `GO-NO-GO.md` | 7288 | 2b09a902988d56ee... | per-launch GO/NO-GO checklists + hard stops |

Pinned source files (read-only inputs; sizes/hashes re-verified this
session): `ueselftest/main.lua` 3961 = 1eb63f7d...709b; parts 2-12 =
3735/3724/3731/3869/3732/3646/3662/3797/3681/2251/3180 B (full hashes in
MANIFEST-LAUNCH-1 §4); `zzdiag-v11/main.lua` 4065 =
779f4c16d599661ea2c8bc11b3d9024c563ed04aa1addab6efe507b11265ce8d
(REVISED mid-prep by a concurrent agent; FJ's report said 4084/84fb6a93...
— re-measure at wake, row = actual wc -c);
CheatManager 1407 = 0b704653...34c83; ConsoleEnabler 3173 =
b31feb8f...911a8; MiniMap 196743 = d8f8d64f...c920f; Stamina 2811 =
7667fed7...e335; spawner console-copy 21415 =
f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658;
l23 pinned pad 3718 (md5 5510caf11fb9307931fe26602245205d, per MAPPING.md).

## 2. Admission rule each artifact satisfies

- **Every mod row** (`file<TAB>lua_mod<TAB>path<TAB>N`): MFA SIZE-EXACT
  law — N == `wc -c` of the exact uploaded bytes; checked all-or-nothing
  at module_start (`modforge_activation.c` size gate, MFA_E_SIZE = -8);
  any mismatch = `mfa_activate_failed code=-8`, game continues unmodded.
- **The 13 ueselftest rows**: MFA PRELOAD law — activation preloads all
  listed lua_mod/config/script files into owned memory (after
  module_start, /data opens fail ENOENT; UE4SS serves require/dofile
  from the activation). Parts resolve at PRESS TIME via require ->
  native module searcher (LuaMod.cpp:1073-1112), which requires each
  part to be IN the activation = IN the loadlist. Upload-only is not
  admission (zzdiag-v7 sidecar precedent; LADDER-V2 §4.6 "unlisted lua
  files are never admitted").
- **The pad row** (`file config ue4ss/PS5Pad.ini 1736`): same size-exact
  law; re-pinned from the console-proven 3718 because the enabled set
  changed (round-4 set adds L1+R1 = F12). Parser legality independently
  verified: `keymap-gen.py --check` = 0 violations / 0 warnings for BOTH
  the launch-1 pad (against the round-4 mod set, --expect-size 1736) and
  the manager's launch-2 pad (audit receipt).
- **enabled.txt 0 B rows**: admission artifact per mod (UE4SS starts a
  mod only with enabled.txt present; MiniMap ships its own, the manager
  stages 0-byte artifacts outside the pristine tree for the rest).
- **Unchanged resident rows** (settings 1947 / MVL 588 / VTL 216 /
  spawner 21415+0 / UEHelpers 9962 / zzdiag 4065+0): carried verbatim
  from the live/base composition (sidecar law: pull live first, keep
  live values, never hand-shrink a row).

## 3. Deploy ORDER law (why upload order is pinned)

mod files FIRST -> PS5Pad.ini -> loadlist.txt LAST. MFA reads the
loadlist at module_start and preloads what it names: the console must
never hold a manifest referencing files that are not there yet (a
mid-deploy failure then leaves admission consistent with the PREVIOUS
state). The pad precedes the loadlist for the same reason (its row size
is pinned by the manifest). PRX/settings/ploader (prep_modded_launch)
are independent of the loadlist and run in their own gated sequence with
decrypt-off readback.

## 4. Who uploads what (split vs prep_modded_launch)

| surface | uploaded by | tool | when |
|---|---|---|---|
| round-4 PRX (`<round4-prx>`) / FX-final PRX (`<fx-final-prx>`) | ROOT (prep) | `prep_modded_launch.py --variant A --prx-path ...` + decrypt-off readback | before any Lua staging, game closed |
| UE4SS-settings.ini (1947 B), ploader.ini (440 B), loadpayload arm, klog tap, eboot monitor | ROOT (prep) | same tool | same step |
| zzdiag-v11 main.lua + its row (actual wc -c; 4065 current) | ROOT | `harvest/lua_fast_deploy.sh` (WAKE-RUNBOOK §2) | launch-1 staging, BEFORE ueselftest |
| ueselftest 13 files + round-4 pad + pad row re-pin + 13 rows + loadlist | ROOT (this manifest) | curl plan in MANIFEST-LAUNCH-1 §4-5 | launch-1 staging (game closed) |
| four type-mods + manager pad + synthesized loadlist | ROOT (GG manager, per this manifest) | `modmanager.py deploy --apply --allow-cofire --i-know-console-is-on` (or the plan's curl sequence verbatim) | launch-2 staging (game closed) |
| spawner main.lua / UEHelpers / MVL / VTL rows | NOBODY | resident, never re-upload (manager refuses non-scope paths by code) | — |

## 5. Verification notes (N-numbers cited by the manifests)

- **N1 launch-1 synthesis cross-validated**: GG's modmanager, given the
  same enabled set (zzdiag + ueselftest) over the same base, synthesizes
  the byte-count-identical 1292 B manifest. Row ORDER differs from GB's
  plan script (manager: main-first, lexicographic parts, enabled.txt
  last; GB: enabled-first, numeric parts) — same 13 rows, same sizes;
  MFA sorts by path internally, admission is order-independent. The
  launch-1 reference pins GB's plan order (ops run GB's script); the
  launch-2 reference pins the manager's order (ops run the manager).
  Both orders never mix: each launch pins exactly one file.
- **N2 rollback exactness**: manager disable-of-the-four synthesis ==
  `launch1-loadlist.txt` byte-identical (cmp-verified this session);
  hand-derived launch-1 rollback == base + zzdiag@4065 (545 B).
- **N3 pad determinism boundary**: FV's keymap-gen hashes
  name|path|content per mod into the `; inputs-sha256:` header line
  (fixed length), so the same enabled SET always yields the same SIZE
  (1736 B for the round-4 set) and the same bindings block, but the
  header BYTES differ across generators that used different input paths
  (GB's 957d3e25 vs the manager's a6313eec [re-pinned after zzdiag's mid-prep revision; was faf4742b pre-revision] vs a fresh local regen 5c132e30).
  Operationally pinned: upload launch-1 = GB's exact artifact,
  launch-2 = the manager's exact artifact; never hand-edit or
  cross-regenerate at deploy time; re-pin only what `wc -c` says.
- **N4 GB's header provenance**: GB's inputs-sha256 (cda50814...) is NOT
  reproducible from the current canonical paths (permutation-tested) —
  GB generated from its own working paths. Bindings and size are
  reproducible and check-clean; the artifact is pinned by hash, per the
  never-regenerate-at-deploy rule above.
- **N5 ueselftest multi-file rule**: see MANIFEST-LAUNCH-1 §3 (upload
  alone is insufficient; every part needs a size-exact row; GB's
  13-row plan is the correct reading of the MFA law, confirmed by
  modforge_activation.h + GG DESIGN + the LuaMod.cpp module-searcher
  citation).
