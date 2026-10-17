#!/usr/bin/env python3
"""Real-data replay (Mac-only, read-only inputs) for paktx.py.

Inputs (never written): the 2026-10-01 console captures of Townfall's
paks-folder index files (.superpowers/native-observations/
townfall-main-utoc-2026-10-01/*.utoc, townfall-main-pak-full-2026-10-01/
pakchunk0-ps5.pak, townfall-original-container-header-1/container-header.bin),
the unchanged downloaded Leon 2K v1.1 trio, and the console-PROVEN trial
artifacts (leon-iostore-adapter-trial-1/overlay/derived/: derived index
3ff1946c..., sidecar 27c7b50a..., the bytes the user saw Leon with).

Checks:
 1. record from the captures; pakchunk0 header tied to its TOC by IoHash.
 2. ownership of the 127 Leon packages: owners counted per container.
 3. a full install plan: expected to REFUSE because the pakchunk10 header is
    not available locally (the input the integrator must fetch).
 4. equivalence with the console-proven trial: build the pakchunk0 target
    alone and compare it with the trial's derived index + sidecar.
 5. the crash fallback `install --only-into pakchunk0-ps5` plans exactly that
    pakchunk0 build and lists the 2 pakchunk10 packages as not installed.
Outputs only under tests/work/real/ and a JSON receipt on stdout.
"""
import json
import mmap
import os
import shutil
import struct
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import test_paktx as T  # noqa: E402  (shares the loaded tool + builder)

paktx, B = T.paktx, T.B
REPO = T.REPO
OBS = os.path.join(REPO, ".superpowers/native-observations")
SPIKE = os.path.join(REPO, ".superpowers/native-dependency-spike")
UTOCS = os.path.join(OBS, "townfall-main-utoc-2026-10-01")
PAK0 = os.path.join(OBS, "townfall-main-pak-full-2026-10-01", "pakchunk0-ps5.pak")
HDR0 = os.path.join(OBS, "townfall-original-container-header-1", "container-header.bin")
LEON = os.path.join(OBS, "leon-real-mod-intake-2026-10-01", "extracted")
TRIAL = os.path.join(SPIKE, "leon-iostore-adapter-trial-1", "overlay", "derived")
TRIAL_UTOC_SHA = "3ff1946c5bb665c55f8774c08550691a1a0ec491d4aec9c17d72d0232920c428"
TRIAL_SIDE_SHA = "27c7b50a7ac7f1664fc8105f39aee728c0396dbc01b7cc93f5f4739cac29ea9d"
# .ucas sizes from the 2026-10-01 capture receipts (ucas_bytes_from_listing)
SIZES = {"pakchunk0-ps5.ucas": 128240377200, "pakchunk1-ps5.ucas": 62955632,
         "pakchunk10-ps5.ucas": 113396720, "pakchunk11-ps5.ucas": 43697344,
         "pakchunk12-ps5.ucas": 183707072}
OUT = os.path.join(HERE, "work", "real")


def main():
    shutil.rmtree(OUT, ignore_errors=True)
    mirror = os.path.join(OUT, "mirror")
    headers = os.path.join(OUT, "headers")
    mod = os.path.join(OUT, "mod")
    for d in (mirror, headers, mod):
        os.makedirs(d)
    for n in sorted(os.listdir(UTOCS)):
        if n.endswith(".utoc"):
            os.symlink(os.path.join(UTOCS, n), os.path.join(mirror, n))
    os.symlink(PAK0, os.path.join(mirror, "pakchunk0-ps5.pak"))
    os.symlink(HDR0, os.path.join(headers, "pakchunk0-ps5.container-header.bin"))
    for n in os.listdir(LEON):
        os.symlink(os.path.join(LEON, n), os.path.join(mod, n))
    out = {"label": "Mac-only", "inputs_read_only": True}
    rec = paktx.make_record(B, mirror, "PPSA33286", "02.003.000",
                            "/data/homebrew/GAME/townfall/content/paks", SIZES, headers,
                            {"source": "2026-10-01 console captures (partial: small .pak "
                                       "companions and global.* not captured)"},
                            relative_to=OUT)
    rpath = os.path.join(OUT, "record.json")
    paktx.write_json(rpath, rec)
    out["record"] = {"files": len(rec["files"]), "record_sha256": rec["record_sha256"],
                     "headers": {s: (c.get("header") or {}).get("sha256")
                                 for s, c in rec["containers"].items()}}
    assert rec["containers"]["pakchunk0-ps5"]["header"]["iohash_matches_toc"]
    # 2. ownership
    rec["_tocs"] = {s: B.parse_toc(open(os.path.join(mirror, c["utoc"]), "rb").read())
                    for s, c in rec["containers"].items() if c.get("admitted")}
    inv = paktx.inventory_mod(B, mod)
    own = paktx.ownership(B, rec, inv["iostore"], None)
    counts = {d: {m: {k: len(v) for k, v in x.items()} for m, x in mm.items()}
              for d, mm in own["summary"].items()}
    out["ownership"] = {"primary": own["primary"], "per_container": counts,
                        "ignored_mod_files": inv["ignored"]}
    print("ownership:", json.dumps(counts), file=sys.stderr)
    # 3. full plan refuses on the missing secondary header
    code, res, err = T.run_cli("install", "--record", rpath, "--mirror", mirror, "--mod", mod,
                               "--work", os.path.join(OUT, "plan"), "--dry-run")
    out["full_plan"] = {"exit": code, "code": res.get("code"), "detail": res.get("detail")}
    # 4. equivalence: pakchunk0 target vs the console-proven trial bytes
    leon = inv["iostore"][0]
    pids0 = own["groups"]["pakchunk0-ps5"][leon.mod_id]
    proj = paktx.project_mod(B, leon, set(pids0), os.path.join(OUT, "proj-chunk0"))
    base = open(os.path.join(mirror, "pakchunk0-ps5.utoc"), "rb").read()
    derived, sidecar, receipt = B.build_overlay(base, SIZES["pakchunk0-ps5.ucas"],
                                                open(HDR0, "rb").read(), [proj])
    os.remove(os.path.join(OUT, "proj-chunk0.ucas"))
    mine = B.parse_toc(derived, strict_admission=False)
    B.validate_lookup(mine)
    tu = open(os.path.join(TRIAL, "pakchunk0-ps5.utoc"), "rb").read()
    assert paktx.sha256_bytes(tu) == TRIAL_UTOC_SHA
    trial = B.parse_toc(tu, strict_admission=False)
    fh = open(os.path.join(TRIAL, "pakchunk0-ps5_s1.ucas"), "rb")
    tside = mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ)
    tside_sha = paktx.sha256_file(os.path.join(TRIAL, "pakchunk0-ps5_s1.ucas"))
    assert tside_sha == TRIAL_SIDE_SHA
    btoc = rec["_tocs"]["pakchunk0-ps5"]
    hid = btoc.container_id + bytes.fromhex("00000006")
    base_size = SIZES["pakchunk0-ps5.ucas"]
    other_pids = {p for d, mm in own["groups"].items() if d != "pakchunk0-ps5"
                  for ps in mm.values() for p in ps}
    mine_ids, trial_ids = set(mine.ids), set(trial.ids)
    only_trial = sorted(c.hex() for c in trial_ids - mine_ids)
    only_mine = sorted(c.hex() for c in mine_ids - trial_ids)
    assert all(B.package_id_of_chunk(bytes.fromhex(c)) in other_pids for c in only_trial), only_trial
    assert not only_mine, only_mine
    base_map = {c: r for r, c in enumerate(btoc.ids)}
    mvw, tvw = B._OffsetView(sidecar, base_size), B._OffsetView(tside, base_size)

    def in_partition1(toc, row):
        off, ln = toc.offset_length(row)
        first, last = off // toc.block_size, (off + ln - 1) // toc.block_size
        return all(toc.block(b)[0] >= base_size for b in range(first, last + 1))
    same_rows = same_payload = 0
    mrow = {c: i for i, c in enumerate(mine.ids)}
    trow = {c: i for i, c in enumerate(trial.ids)}
    for c in mine.ids:
        mr, tr = mrow[c], trow[c]
        if c == hid:
            continue
        if in_partition1(mine, mr):
            assert in_partition1(trial, tr), c.hex()
            assert B.extract_chunk_bytes(mine, mr, mvw) == B.extract_chunk_bytes(trial, tr, tvw), c.hex()
            same_payload += 1
        else:
            br = base_map[c]
            assert mine.offset_length(mr) == trial.offset_length(tr) == btoc.offset_length(br), c.hex()
            assert mine.meta(mr) == trial.meta(tr) == btoc.meta(br), c.hex()
            assert not in_partition1(trial, tr), c.hex()
            same_rows += 1
    mh = B.parse_container_header(B.extract_chunk_bytes(mine, mrow[hid], mvw))
    th = B.parse_container_header(B.extract_chunk_bytes(trial, trow[hid], tvw))
    extra = set(th.packages) - set(mh.packages)
    assert extra == other_pids and not (set(mh.packages) - set(th.packages)), (extra, other_pids)
    differing = [p for p in mh.packages if
                 (mh.packages[p].imported_packages, mh.packages[p].shader_map_hashes) !=
                 (th.packages[p].imported_packages, th.packages[p].shader_map_hashes)]
    assert not differing, differing[:5]
    md = B.parse_directory_index(mine.directory_index, mine.ids, "mine")
    td = B.parse_directory_index(trial.directory_index, trial.ids, "trial")
    mpaths = {f.path: f.chunk_id for f in md["files"]}
    tpaths = {f.path: f.chunk_id for f in td["files"]}
    dpaths = sorted(set(tpaths) - set(mpaths))
    assert all(B.package_id_of_chunk(tpaths[p]) in other_pids for p in dpaths), dpaths
    assert all(tpaths[p] == mpaths[p] for p in mpaths), "path->chunk mapping differs"
    tside.close()
    fh.close()
    out["equivalence_with_console_proven_trial"] = {
        "trial_derived_index_sha256": TRIAL_UTOC_SHA, "trial_sidecar_sha256": tside_sha,
        "my_pakchunk0_derived_index_sha256": paktx.sha256_bytes(derived),
        "my_pakchunk0_sidecar_sha256": paktx.sha256_bytes(sidecar),
        "my_pakchunk0_sidecar_bytes": len(sidecar),
        "base_rows_identical": same_rows,
        "mod_chunk_payloads_identical": same_payload,
        "chunks_only_in_trial_moved_to_secondary_owner": only_trial,
        "trial_only_virtual_paths": dpaths,
        "merged_store_entries_identical": len(mh.packages),
        "packages_trial_put_in_pakchunk0_but_owned_by_pakchunk10": sorted("%016x" % p for p in extra),
        "directory_rows_identical": len(mpaths),
    }
    # 5. the named crash fallback (review N5): --only-into pakchunk0-ps5 on the real
    #    files must plan exactly the pakchunk0 target built above, without the
    #    (still missing) pakchunk10 header
    code2, res2, err2 = T.run_cli("install", "--record", rpath, "--mirror", mirror, "--mod", mod,
                                  "--work", os.path.join(OUT, "plan-only0"), "--dry-run",
                                  "--only-into", "pakchunk0-ps5")
    assert code2 == 0, (res2, err2[-2000:])
    plan2 = json.load(open(res2["plan"]))
    t0 = plan2["targets"][0]
    fb = {"exit": code2, "targets": res2["targets"],
          "derived_index_sha256": t0["derived"]["sha256"],
          "sidecar_sha256": t0["sidecar"]["sha256"],
          "equals_pakchunk0_build_above": (t0["derived"]["sha256"] == paktx.sha256_bytes(derived) and
                                           t0["sidecar"]["sha256"] == paktx.sha256_bytes(sidecar)),
          "not_installed": {d: {m: len(p["replaced"]) + len(p["added"]) for m, p in mm.items()}
                            for d, mm in (plan2["skipped_by_only_into"] or {}).items()},
          "packages": {m: {k: len(v) for k, v in p.items()} for m, p in t0["packages"].items()}}
    out["only_into_fallback"] = fb
    shutil.rmtree(os.path.join(OUT, "plan-only0", "targets"))  # ~190 MB of derived bytes
    print("only-into fallback:", json.dumps(fb), file=sys.stderr)
    # preview of the optional in-game probe (regenerate from the real plan)
    fake = {"targets": [{"packages": own["summary"]["pakchunk0-ps5"]}], "plan_sha256": "preview00000" + "0" * 52}
    lua = paktx.make_probe(B, fake, mod)
    with open(os.path.join(HERE, "output", "leon-probe-preview.lua"), "w") as f:
        f.write(lua)
    out["probe_preview"] = {"bytes": len(lua.encode()), "paths": lua.split("local P={", 1)[1].split("}", 1)[0].count('"/')}
    ok_full = code == 2 and res.get("code") == "base_container_header_unavailable"
    ok_fb = (fb["targets"] == ["pakchunk0-ps5"] and fb["equals_pakchunk0_build_above"] and
             fb["not_installed"] == {"pakchunk10-ps5": {leon.mod_id: 2}})
    # 6. complete 2-container plan (integrator fix 2026-10-13): with the original
    #    pakchunk10-ps5.ucas present (read-only console pull) the plan must build both
    #    targets; pakchunk10's deeper mount point needs the re-rooted projection.
    #    Every derived virtual path must be byte-identical to a base or mod path.
    U10 = os.path.join(REPO, "outputs/2026-10-08-l25-prep/evidence/console-pulls-20261013-paks2",
                       "pakchunk10-ps5.ucas")
    assert os.path.isfile(U10), "missing required input " + U10
    mirror2 = os.path.join(OUT, "mirror2")
    shutil.copytree(mirror, mirror2, symlinks=True)
    os.symlink(U10, os.path.join(mirror2, "pakchunk10-ps5.ucas"))
    rec2 = paktx.make_record(B, mirror2, "PPSA33286", "02.003.000",
                             "/data/homebrew/GAME/townfall/content/paks", SIZES, headers,
                             {"source": "replay full"}, relative_to=OUT)
    rpath2 = os.path.join(OUT, "record2.json")
    paktx.write_json(rpath2, rec2)
    code3, res3, err3 = T.run_cli("install", "--record", rpath2, "--mirror", mirror2, "--mod", mod,
                                  "--work", os.path.join(OUT, "plan-full"), "--dry-run")
    full = {"exit": code3, "targets": res3.get("targets"), "code": res3.get("code")}
    rerooted = 0
    if code3 == 0:
        mdir = B.parse_directory_index(leon.toc.directory_index, leon.toc.ids, "m")
        mpath = {f.path: f.chunk_id for f in mdir["files"]}
        for dest, want in (("pakchunk0-ps5", 125), ("pakchunk10-ps5", 2)):
            tdir = os.path.join(OUT, "plan-full", "targets", dest)
            dt = B.parse_toc(open(os.path.join(tdir, dest + ".utoc"), "rb").read(), strict_admission=False)
            B.validate_lookup(dt)
            dd = B.parse_directory_index(dt.directory_index, dt.ids, "d")
            bt = B.parse_toc(open(os.path.join(mirror2, dest + ".utoc"), "rb").read())
            bd = B.parse_directory_index(bt.directory_index, bt.ids, "b")
            bpaths = {f.path for f in bd["files"]}
            served = sum(1 for f in dd["files"] if mpath.get(f.path) == f.chunk_id)
            full[dest] = {"mount": dd["mount_point"] == bd["mount_point"],
                          "all_base_paths_kept": bpaths <= {f.path for f in dd["files"]},
                          "unknown_paths": sum(1 for f in dd["files"]
                                               if f.path not in bpaths and f.path not in mpath),
                          "mod_served_paths": served}
            if dest == "pakchunk10-ps5":
                rerooted = served
        shutil.rmtree(os.path.join(OUT, "plan-full", "targets"))
    out["full_two_container_plan"] = full
    print("full plan:", json.dumps(full), file=sys.stderr)
    ok_two = (code3 == 0 and full["targets"] == ["pakchunk0-ps5", "pakchunk10-ps5"] and
              all(full[d]["mount"] and full[d]["all_base_paths_kept"] and full[d]["unknown_paths"] == 0
                  for d in ("pakchunk0-ps5", "pakchunk10-ps5")) and
              full["pakchunk10-ps5"]["mod_served_paths"] == 2 and
              full["pakchunk0-ps5"]["mod_served_paths"] > 0)
    out["result"] = "PASS" if ok_full and ok_fb and ok_two else "FAIL"
    checked = same_rows + same_payload + len(mh.packages) + len(mpaths) + rerooted
    out["items_checked"] = checked
    if checked == 0:
        out["result"] = "FAIL"
    print(json.dumps(out, indent=1, sort_keys=True))
    return 0 if out["result"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
