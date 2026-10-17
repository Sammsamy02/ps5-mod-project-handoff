#!/usr/bin/env python3
"""Write a SYNTHETIC "PC sound/video mod" for the dry run (never upload it).

The bytes follow the public header layouts only (tests/fixtures.py). Names
and bank ids come from the base profile (the game's own registry), the folder
layout from the profile's PC bank folder, so the planner sees exactly what a
real PC mod for this game would look like on disk.

usage: make_dryrun_mod.py --profile P --out DIR --bank NAME [--bank NAME] --movie PACKAGE
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixtures as X  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--profile", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--bank", action="append", default=[])
ap.add_argument("--movie", action="append", default=[])
ap.add_argument("--bad-vorbis-bank", action="append", default=[],
                help="bank written with a Vorbis setup id the runtime does not know (shows that refusal)")
a = ap.parse_args()
prof = json.load(open(a.profile))
fm = prof["audio"]["fmod"]
proj = prof["project"]
assert not os.path.exists(a.out), "output exists"
banks = {b["name"]: b for b in fm["banks"]}
known = fm["vorbis_setup_crcs"][0]  # a setup id from the game's own runtime table
for name in a.bank + a.bad_vorbis_bank:
    guid = banks[name]["guid"]
    crc = known if name in a.bank else 0x01020304
    X.write(os.path.join(a.out, proj, "Content", *fm["default_pc_bank_dir"].split("/"), name + ".bank"),
            X.bank(guid=guid, fsb=X.fsb5("VORBIS", ((48000, 2, 48000),), vorbis_crc=crc)))
for pkg in a.movie:
    rel = pkg.replace("/Game/", "", 1)
    X.write(os.path.join(a.out, proj, "Content", *rel.split("/")) + ".bk2", X.bink(b"KB2j", 1920, 1080, 90, (30, 1)))
X.write(os.path.join(a.out, "readme.txt"), b"synthetic dry-run mod: header-only test bytes, never upload\n")
X.write(os.path.join(a.out, proj, "Content", "WwiseAudio", "Windows", "1234.wem"),
        b"RIFF" + (30).to_bytes(4, "little") + b"WAVE" + b"fmt " + (18).to_bytes(4, "little") + b"\xff\xff" + bytes(16))
print("make_dryrun_mod: wrote", sum(len(f) for _, _, f in os.walk(a.out)), "files under", a.out)
