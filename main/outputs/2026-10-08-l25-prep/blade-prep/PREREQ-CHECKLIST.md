# PREREQ-CHECKLIST — what must exist before the Blade blind test can run

Agent GJ, 2026-10-08. Mac-only prep artifacts are DONE (this dir). The items
below are the gaps only the user / a later session can fill. Battery-mode
law respected: nothing below was performed now.

## 1. The user's own Blade dump (decrypt-off acquisition law) — USER ACTION

The eboot is NOT on the Mac (FZ §1). Stellar Blade is installed on the
console at `/data/Stellar Blade 01.014.001 PPSA13197` (titleId PPSA13197,
contentVersion 01.014.001, confirmed by the 2026-09-30 content discovery;
`eboot.bin` is listed in that directory).

DOCUMENTED, NOT PERFORMED (console awake, FTP :2121, user-authorized
session only). The profiling view is the DEFAULT-connection (decrypt-on)
download; the decrypt-off second connection is the provenance cross-check:

```bash
CONSOLE=ps5-host-not-configured.invalid   # per the wake-runbook lineage
SRC="/data/Stellar Blade 01.014.001 PPSA13197/eboot.bin"

# (a) profiling copy - DEFAULT connection (ftpsrv decrypts on download;
#     expect magic 7f454c46, size = decrypted view)
curl -s --max-time 600 "ftp://$CONSOLE:2121$SRC" -o blade-eboot-decrypted.bin
shasum -a 256 blade-eboot-decrypted.bin; wc -c blade-eboot-decrypted.bin

# (b) provenance cross-check - SECOND connection, decrypt OFF
curl -s -Q "SITE SELF" --max-time 60 "ftp://$CONSOLE:2121$SRC" -o blade-eboot-orig-head.bin
xxd -l 4 blade-eboot-orig-head.bin   # expect 4f153d1d or 5414f5ee (real on-disk magic)
# record BOTH sizes + sha256s in the acquisition receipt

# (c) stage under a NEW observations dir, exactly like the Townfall one:
#   .superpowers/native-observations/stellarblade-static-executable-<date>/
```

NEVER upload any downloaded/modified executable back (POSTMORTEM rule 2);
no executable leaves the staging dir; modified executables need the
make_fself fake-sign pipeline + decrypt-off readback before any launch
(rules 3-4) — none of which this blind test needs or does.

## 2. Then: generator run (Mac, minutes)

```
python3 outputs/2026-10-08-l25-prep/blade-prep/run-blade-profile.py \
    <staged blade-eboot-decrypted.bin>
```
- ACCEPTED → `runs/<sha12>/profile.json` (see TEST-PLAN §0.2 for the build).
- REFUSED → the refusal IS the result: seed-version refusal means the
  per-version table needs a measurement campaign for that version first
  (PER-VERSION-TABLE §4); 0-marker refusal would mean not-UE (not expected:
  PAK v11/IoStore v3 confirm an Unreal-built game, but they do NOT identify
  the engine version — FZ §1 caveat stands: version UNCERTAIN until the
  classifier runs).

## 3. Disk space

- Mac: eboot size unknown until fetched; same class as Townfall 194 MB /
  Astro 259 MB — budget ~0.5 GB for the staged copy + analysis artifacts.
  Volume has ~106 GiB free (checked this session): sufficient.
- Console: mod root is text-only (< 300 KB across 5 mods + enabled.txt +
  UEHelpers); loadlist row growth trivial. No pak/asset staging (out of
  scope until capabilities #7/#8).

## 4. SDK / tooling gaps (honest list)

1. **Engine version UNCERTAIN** (the only classification-relevant unknown).
   If it resolves to 5.6 → the blind test is config+build only (measured
   row). Any other version → seed row → generator REFUSES BY DESIGN → the
   table upgrade campaign (anchor measurement on the Blade binary per the
   row's confirm_when) becomes the new priority-(a) work.
2. **PRX build needed** (mains power): the generic runtime rebuilt against
   the generated profile; GB's ueselftest must be in the build; manifest
   PASS + decrypt-off readback. Not part of battery prep.
3. **GI automation not landed**: FIRST-LAUNCH-AUTOMATION.md absent at time
   of writing — first launch runs TEST-PLAN.md manually; reconcile schemas
   when GI lands (noted in per-version-table.json alignment_with_gi).
4. **GG mod-manager**: per-title mod root for PPSA13197 must be created by
   the manager (the Townfall root pattern); UEHelpers stages fresh (the
   Blade root starts empty — it is NOT shared with PPSA33286's root).
5. **FV keymap**: zero new PS5Pad.ini rows required by the five picks
   (keybind-free set); if a later cycle adds LineTraceMod, its F3 row needs
   the FV generator + MFA re-pin (no-chord-contains-chord law).
6. No other SDK gap: the classifier, anchor arms, and profile emission are
   already exercised end-to-end on both local controls (see runs/).

## 5. Lawful-source audit gate (before any of this ships publicly)

Addendum A4: any open-source release MUST EXCLUDE Epic engine source and
Sony SDK material. The per-version table's measured row cites Epic release
receipts as PROVENANCE only (hashes + commit ids, no vendored engine source
in this dir); keep it that way in every downstream artifact.
