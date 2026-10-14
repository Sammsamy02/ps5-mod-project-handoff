#!/usr/bin/env python3
"""Host tests for the Team H sound/video prototype (Mac-only, no console).

Run:  python3 -I tests/test_av.py      (prints the number of tests; fails on 0)
Synthetic fixtures only (tests/fixtures.py), plus one optional test that runs
the profile builder on the game's own local files when they are present.
"""
import hashlib
import json
import os
import shutil
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.dirname(HERE)
sys.path.insert(0, TOOL)
sys.path.insert(0, HERE)
import av_fetch  # noqa: E402
import av_formats as F  # noqa: E402
import av_install as I  # noqa: E402
import av_profile as P  # noqa: E402
import fixtures as X  # noqa: E402


def sha(b):
    return hashlib.sha256(b).hexdigest()


class Parsers(unittest.TestCase):
    def test_fsb5_fields(self):
        d = X.fsb5("VORBIS", ((48000, 2, 4800), (44100, 1, 1000)))
        r = F.parse_fsb5(d)
        self.assertEqual((r["codec"], r["samples"], r["rates"], r["channels"]), ("VORBIS", 2, [44100, 48000], [1, 2]))
        self.assertEqual(r["vorbis_setup_crc32"], [0x12345678])
        self.assertEqual(F.parse_fsb5(X.fsb5("PCM16", version=0))["header_bytes"], 0x40)

    def test_fsb5_refusals(self):
        d = bytearray(X.fsb5("VORBIS"))
        with self.assertRaises(F.FormatError) as e:
            F.parse_fsb5(bytes(d[:-10]))
        self.assertEqual(e.exception.code, "fsb5_truncated")
        d[24:28] = struct.pack("<I", 99)
        with self.assertRaises(F.FormatError) as e:
            F.parse_fsb5(bytes(d))
        self.assertEqual(e.exception.code, "fsb5_codec_unknown")
        with self.assertRaises(F.FormatError) as e:
            F.parse_fsb5(bytes(x ^ 0x5A for x in X.fsb5()))  # an encrypted FSB has no magic
        self.assertEqual(e.exception.code, "fsb5_magic")

    def test_bank_complete(self):
        for pad in (True, False):
            b = X.bank(fmt_version=141, odd_chunk=True, pad=pad)
            r = F.parse_fmod_bank(b)
            self.assertEqual(r["fmt_version"], 141)
            self.assertEqual(r["padded_chunks"], pad)
            self.assertEqual(r["snd"][0]["fsb"]["codec"], "VORBIS")
            self.assertEqual(r["snd"][0]["fsb_offset"] % 32, 0)
        self.assertIsNone(F.parse_fmod_bank(X.bank())["padded_chunks"])  # no odd chunk: either walk

    def test_bank_refusals(self):
        b = X.bank()
        for bad, code in ((b[:-4], "bank_size_mismatch"), (b"RIFX" + b[4:], "bank_riff_magic"),
                          (b[:8] + b"WAVE" + b[12:], "bank_form")):
            with self.assertRaises(F.FormatError) as e:
                F.parse_fmod_bank(bad)
            self.assertEqual(e.exception.code, code)
        enc = bytearray(b)
        k = b.index(b"FSB5")
        enc[k:k + 4] = b"\x13\x37\x13\x37"
        with self.assertRaises(F.FormatError) as e:
            F.parse_fmod_bank(bytes(enc))
        self.assertEqual(e.exception.code, "bank_snd_no_fsb5")

    def test_bank_head_and_hop(self):
        b = X.bank(metadata_bytes=70000)
        head = b[:65536]
        r = F.parse_fmod_bank(head, file_size=len(b), complete=False)
        self.assertFalse(any(s["fsb"] for s in r["snd"]))
        stops = r["walk_stop_candidates"]
        self.assertTrue(stops)
        k = b.index(b"SND ")
        self.assertIn(k, stops)
        view = head + bytes(k - len(head)) + b[k:k + 65536]
        have = lambda o, n: (o + n <= len(head)) or (k <= o and o + n <= k + 65536)  # noqa: E731
        r2 = F.parse_fmod_bank(view, file_size=len(b), complete=False, have=have)
        self.assertEqual(r2["snd"][0]["fsb"]["codec"], "VORBIS")

    def test_guid_encodings(self):
        g = "{A98BC465-070F-4740-AD12-02754A36B8EC}"
        self.assertEqual(F.guid_encodings(g)["fields"], X.guid_fields(g))
        self.assertIn(F.guid_encodings(g)["fields"], X.bank(guid=g))

    def test_bink(self):
        r = F.parse_bink(X.bink(b"KB2j", 1280, 720, 600, (30000, 1001), tracks=1, total=5000), file_size=5000)
        self.assertEqual((r["kind"], r["revision"], r["width"], r["height"], r["audio_tracks"], r["audio_rates"]),
                         ("bink2", "j", 1280, 720, 1, [48000]))
        self.assertEqual(F.parse_bink(X.bink(b"BIKi"))["kind"], "bink1")
        with self.assertRaises(F.FormatError) as e:
            F.parse_bink(X.bink(total=5000), file_size=5001)
        self.assertEqual(e.exception.code, "bink_size_mismatch")
        with self.assertRaises(F.FormatError):
            F.parse_bink(b"KB2j" + bytes(60))

    def test_mp4(self):
        r = F.parse_mp4(X.mp4())
        self.assertTrue(r["faststart"])
        v = [t for t in r["tracks"] if t["handler"] == "vide"][0]["entries"][0]
        a = [t for t in r["tracks"] if t["handler"] == "soun"][0]["entries"][0]
        self.assertEqual((v["fourcc"], v["width"], v["profile_idc"], v["level_idc"]), ("avc1", 1280, 100, 41))
        self.assertEqual((a["fourcc"], a["channels"], a["sample_rate"], a.get("aac_aot")), ("mp4a", 2, 48000, 2))
        late = X.mp4(faststart=False)
        self.assertFalse(F.parse_mp4(late)["faststart"])
        h = F.parse_mp4(late[:200], file_size=len(late), complete=False)
        self.assertIsNone(h["moov_offset"])
        self.assertEqual(h["moov_offset_hint"], late.index(b"moov") - 4)

    def test_wwise_and_sniff(self):
        self.assertEqual(F.parse_wwise(b"BKHD" + struct.pack("<III", 12, 140, 7))["bank_version"], 140)
        wem = b"RIFF" + struct.pack("<I", 30) + b"WAVE" + b"fmt " + struct.pack("<IH", 18, 0xFFFF) + bytes(16)
        self.assertEqual(F.parse_wwise(wem)["codec"], "VORBIS")
        self.assertEqual(F.sniff(X.bank()), "fmod_bank")
        self.assertEqual(F.sniff(X.bink()), "bink2")
        self.assertEqual(F.sniff(X.mp4()), "mp4")


class ProfileLogic(unittest.TestCase):
    def test_ini_semantics(self):
        c = P.Config()
        c.apply("a", "[S]\nK=1\n+L=x\n+L=y\n")
        c.apply("b", "﻿[S]\n-L=x\n+L=z\n+L=y\nK=\n")
        self.assertEqual(c.get("S", "L"), ["y", "z"])
        self.assertEqual(c.one("S", "K"), "")
        c.apply("c", "[S]\n!L=ClearArray\n")
        self.assertEqual(c.get("S", "L"), [])

    def test_hierarchy_order(self):
        h = P.hierarchy("Engine", "Proj", ["Parent", "Plat"])
        self.assertLess(h.index("Proj/Config/DefaultEngine.ini"), h.index("Engine/Platforms/Plat/Config/PlatEngine.ini"))
        self.assertEqual(h[-1], "Proj/Platforms/Plat/Config/PlatEngine.ini")

    def test_bink_code_scan(self):
        code = b"\x90\x81\xffKB2f\x74\x00\x81\xffKB2g\x90" + b"\x81\xc7" + \
            ((-int.from_bytes(b"KB2i", "little")) & 0xFFFFFFFF).to_bytes(4, "little") + \
            b"\x31\xc0\xc4\xe3\x7b\xf0\xcf\x18\x83\xf9\x05\x77\x0c" + b"\x81\xffBIKk"
        r = P.bink_header_checks(code)
        self.assertEqual(r["bink2_accepted"], ["KB2f", "KB2g", "KB2i", "KB2j", "KB2k", "KB2l", "KB2m", "KB2n"])
        self.assertEqual(r["bink1_recognised"], ["BIKk"])

    def test_vorbis_setup_table(self):
        import zlib
        h1 = b"\x05vorbis" + bytes(range(200))
        h2 = b"\x05vorbis" + bytes(range(100, 250)) * 2
        pad = lambda b: b + bytes((-len(b)) % 16)  # noqa: E731
        lib = bytearray(b"\xcc" * 64 + pad(h1) + pad(h2) + b"\xcc" * 12)
        lib += bytes((-len(lib)) % 4)
        recs = b""
        for h in (h1, h2):
            recs += struct.pack("<II", len(h), zlib.crc32(h)) + bytes(24)
        recs += struct.pack("<II", len(h1), 0xCAFEBABE) + struct.pack("<II", 150, 1) + bytes(16)  # derived record
        lib += recs + bytes(32)
        t = P.vorbis_setup_table(bytes(lib))
        self.assertTrue(t["validated"])
        self.assertEqual((t["headers"], t["records"], t["direct_records"]), (2, 3, 2))
        self.assertIn(0xCAFEBABE, t["crcs"])
        self.assertFalse(P.vorbis_setup_table(bytes(lib[:-len(recs) - 32]))["validated"])

    def test_fmod_lib_facts(self):
        blob = b"..FMOD WATERMARK VERSION:0x00020313 BUILD:161495\x00FMOD Vorbis Codec\x00FMOD FSB Codec\x00" \
               b"fmod_codec_fsbvorbis.cpp\x00\x05vorbis\x0fBCV"
        r = P.fmod_lib_facts(blob)
        self.assertEqual((r["version"], r["vorbis_setup_headers"]), ("2.03.13", 1))
        self.assertEqual(r["codec_names"], ["FSB", "Vorbis"])


class FetchPipeline(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="avtest-")
        self.console = os.path.join(self.tmp, "console-content")
        X.console_tree(self.console)
        self.remote = "/data/game dir/content"
        self.listing = av_fetch.walk(X.fake_list_dir(self.console, self.remote), self.remote, ["FMOD", "movies"])

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def fetch_all(self):
        out = os.path.join(self.tmp, "fetched")
        heads = av_fetch.heads_plan(self.listing)
        self.assertEqual(X.simulate_range_fetch(heads, self.console, out), len(heads["chunks"]))
        plans = [heads]
        for _ in range(3):
            hops = av_fetch.hops_plan(plans, out)
            if not hops["chunks"]:
                break
            X.simulate_range_fetch(hops, self.console, out)
            plans.append(hops)
        return out, plans

    def test_listing(self):
        e = self.listing["entries"]
        self.assertIn("fmod/banks/plat/music.bank", e)
        self.assertIn("movies/intro.bk2", e)
        self.assertNotIn("paks/readme.txt", e)
        self.assertEqual(self.listing["subdirs_found"], ["fmod", "movies"])
        with self.assertRaises(av_fetch.FetchError):
            av_fetch.parse_listing("garbage line\r\n")

    def test_heads_hops_check(self):
        out, plans = self.fetch_all()
        self.assertEqual(len(plans), 2)  # heads + one hop round
        self.assertEqual(sorted(c["ucas"] for c in plans[1]["chunks"]),
                         ["fmod/banks/plat/dialogue_en.bank", "movies/tv.mp4"])  # SND beyond head, moov after mdat
        media = av_fetch.check(self.listing, out, plans, X.profile())
        self.assertEqual(media["failures"], [])
        sc = media["selfcheck"]
        self.assertEqual(sc["bank_codecs"], ["AT9", "VORBIS"])
        self.assertTrue(sc["bank_codecs_decodable_by_runtime"])
        self.assertEqual(sc["bank_guid_encoding"], "fields")
        self.assertEqual(sc["banks_matched_to_assets"], 3)
        self.assertEqual(sc["bink_magics"], ["KB2j"])
        self.assertTrue(sc["bink_magics_accepted_by_exe"])
        self.assertEqual(sc["mp4_codecs"], ["avc1", "mp4a"])
        self.assertEqual(sc["base_vorbis_setups"], [0x12345678])
        self.assertTrue(sc["base_vorbis_setups_in_runtime_table"])

    def test_package_rows_verified_by_iohash(self):
        sys.path.insert(0, P.MF)
        try:
            import blake3
        except ImportError:
            self.skipTest("integrated BLAKE3 reader not present")
        out = os.path.join(self.tmp, "pk")
        data = b"\x00" * 10 + b"./Movies/Intro.bk2\x00" + b"\x01" * 30
        X.write(os.path.join(out, "packages/a"), data)
        plan = {"chunks": [{"save_as": "packages/a", "length": len(data), "io_hash20": blake3.iohash20(data),
                            "package_path": "Game/Content/Movies/Intro.uasset", "physical_ranges": []}]}
        res = av_fetch.check(self.listing, out, [plan], X.profile())
        self.assertEqual(res["packages"]["Game/Content/Movies/Intro.uasset"]["media_paths"], ["./Movies/Intro.bk2"])
        plan["chunks"][0]["io_hash20"] = "0" * 40
        self.assertTrue(av_fetch.check(self.listing, out, [plan], X.profile())["failures"])


class _InstallerBase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="avinst-")
        self.console = os.path.join(self.tmp, "console-content")
        X.console_tree(self.console)
        self.remote = "/data/game dir/content"
        self.listing = av_fetch.walk(X.fake_list_dir(self.console, self.remote), self.remote, ["fmod", "movies"])
        out = os.path.join(self.tmp, "fetched")
        heads = av_fetch.heads_plan(self.listing)
        X.simulate_range_fetch(heads, self.console, out)
        plans = [heads]
        for _ in range(3):
            hops = av_fetch.hops_plan(plans, out)
            if not hops["chunks"]:
                break
            X.simulate_range_fetch(hops, self.console, out)
            plans.append(hops)
        self.media = av_fetch.check(self.listing, out, plans, X.profile())
        self.assertEqual(self.media["failures"], [])
        self.mod = os.path.join(self.tmp, "mod")

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def plan(self, decisions=(), media="default", listing=None):
        return I.make_plan(X.profile(), listing or self.listing, self.media if media == "default" else media,
                           self.mod, list(decisions), "toolsha")

    def put(self, rel, data):
        return X.write(os.path.join(self.mod, *rel.split("/")), data)

    def codes(self, plan):
        return sorted(r["code"] for r in plan["refused"])



class Installer(_InstallerBase):
    def test_ready_plan_apply_restore_byte_exact(self):
        mb = X.bank(guid=X.GUIDS["Music"], fsb=X.fsb5("VORBIS", ((48000, 2, 9600),)))
        self.put("Game/Content/FMOD/Banks/Desktop/Music.bank", mb)
        self.put("Game/Content/FMOD/Banks/PC Stereo/Music.bank", mb[:-1] + b"\x01")  # other PC variant: ignored
        mv = X.bink(b"KB2j", 1920, 1080, 300, (30, 1), total=6000)
        self.put("Game/Content/Movies/Intro.bk2", mv)
        self.put("readme.txt", b"hello")
        p = self.plan()
        self.assertEqual(p["verdict"], "READY", json.dumps(p["refused"]))
        self.assertEqual(sorted(f["target"] for f in p["files"]), ["fmod/banks/plat/music.bank", "movies/intro.bk2"])
        self.assertEqual(len(p["operations"]), 10)
        before = {r: sha(open(os.path.join(self.console, r), "rb").read()) for r in
                  ("fmod/banks/plat/music.bank", "movies/intro.bk2")}
        mod_before = {f: sha(open(os.path.join(self.mod, f), "rb").read()) for f in
                      ("Game/Content/FMOD/Banks/Desktop/Music.bank", "Game/Content/Movies/Intro.bk2")}
        be = I.LocalBackend(self.console)
        j = os.path.join(self.tmp, "journal.json")
        with self.assertRaises(I.Refused):
            I.apply_plan(p, "wrong", be, j)
        I.apply_plan(p, p["plan_sha256"], be, j)
        self.assertEqual(open(os.path.join(self.console, "fmod/banks/plat/music.bank"), "rb").read(), mb)
        self.assertEqual(open(os.path.join(self.console, "movies/intro.bk2"), "rb").read(), mv)
        r = I.restore(p, be, j)
        self.assertTrue(all(x["sha256_verified"] for x in r["restored"]))
        for rel, h in before.items():
            self.assertEqual(sha(open(os.path.join(self.console, rel), "rb").read()), h)
        self.assertFalse(any(n.endswith(".bak") for _, _, fs in os.walk(self.console) for n in fs))
        r2 = I.restore(p, be, j)  # idempotent
        self.assertTrue(all(x["state"] == "original in place" for x in r2["restored"]))
        for f, h in mod_before.items():  # mod files never touched
            self.assertEqual(sha(open(os.path.join(self.mod, f), "rb").read()), h)

    def test_interrupted_apply_then_restore(self):
        self.put("Game/Content/FMOD/Banks/Desktop/Music.bank", X.bank(guid=X.GUIDS["Music"], fsb=X.fsb5("VORBIS")))
        p = self.plan()
        self.assertEqual(p["verdict"], "READY")
        orig = open(os.path.join(self.console, "fmod/banks/plat/music.bank"), "rb").read()
        be = I.LocalBackend(self.console, fail_after=1)  # rename done, upload fails
        j = os.path.join(self.tmp, "j.json")
        with self.assertRaises(I.Stopped):
            I.apply_plan(p, p["plan_sha256"], be, j)
        self.assertFalse(os.path.exists(os.path.join(self.console, "fmod/banks/plat/music.bank")))
        I.restore(p, I.LocalBackend(self.console), j)
        self.assertEqual(open(os.path.join(self.console, "fmod/banks/plat/music.bank"), "rb").read(), orig)

    def test_refusals(self):
        g = X.GUIDS["Music"]
        cases = [
            ("FMOD/Banks/Desktop/Music.bank", X.bank(fmt_version=999, guid=g), [], "bank_format_newer_than_game"),
            ("FMOD/Banks/Desktop/Music.bank", X.bank(fmt_version=100, guid=g), [], "bank_format_older_than_game"),
            ("FMOD/Banks/Desktop/Music.bank", X.bank(guid=g, fsb=X.fsb5("XMA", ((48000, 2, 10),))), [], "codec_not_in_runtime"),
            ("FMOD/Banks/Desktop/Music.bank", X.bank(guid=g, fsb=X.fsb5("FADPCM", ((48000, 2, 10),))), [], "codec_unproven_on_platform"),
            ("FMOD/Banks/Desktop/Music.bank", X.bank(guid=X.GUIDS["Master"]), [], "bank_guid_mismatch"),
            ("FMOD/Banks/Desktop/Music.bank", X.bank(guid=g, fsb=X.fsb5("VORBIS", vorbis_crc=0x01020304)), [],
             "vorbis_setup_unknown_to_runtime"),
            ("FMOD/Banks/Desktop/Master.bank", X.bank(guid=X.GUIDS["Master"]), [], "mixer_bank_refused"),
            ("FMOD/Banks/Desktop/NewBank.bank", X.bank(guid=g), [], "target_not_in_game"),
            ("Music.bank", X.bank(guid=g), [], "bank_outside_pc_bank_folder"),
            ("FMOD/Banks/PC Stereo/Music.bank", X.bank(guid=g), [], "non_default_pc_variant"),
            ("Movies/Intro.bik", X.bink(b"BIKi"), [], "target_not_in_game"),
            ("Movies/Intro.bk2", X.bink(b"BIKi"), [], "bink1_not_supported"),
            ("Movies/Intro.bk2", X.bink(b"KB2n"), [], "bink_revision_not_accepted"),
            ("Movies/Intro.bk2", X.bink(b"KB2h"), [], "bink_revision_unproven"),
            ("Movies/Tv.mp4", X.mp4(video="hvc1", profile=1, level=150), [], "mp4_codec_unproven"),
            ("Movies/Tv.mp4", X.mp4(level=52), [], "mp4_profile_level_above_game"),
            ("Sound/Music.wem", b"RIFF" + struct.pack("<I", 30) + b"WAVE" + bytes(26), [], "middleware_mismatch"),
            ("Sound/Music.ogg", b"OggS" + bytes(40), [], "raw_audio_needs_authoring"),
            ("weird.xyz", b"???", [], "unknown_file_kind"),
        ]
        for rel, data, dec, code in cases:
            shutil.rmtree(self.mod, ignore_errors=True)
            self.put(rel, data)
            p = self.plan(dec)
            self.assertEqual(p["verdict"], "REFUSED", rel)
            self.assertEqual(self.codes(p), [code], rel)
            self.assertFalse(p["executable"])

    def test_decisions_unlock_but_stay_flagged(self):
        self.put("FMOD/Banks/Desktop/Music.bank", X.bank(guid=X.GUIDS["Music"], fsb=X.fsb5("FADPCM", ((48000, 2, 10),))))
        p = self.plan(["accept-unproven-codec"])
        self.assertEqual(p["verdict"], "READY")
        self.assertTrue(any("needs the console proof" in n for n in p["files"][0]["facts"]["notes"]))
        with self.assertRaises(I.Refused):
            self.plan(["not-a-decision"])

    def test_pending_and_provisional_are_dry_run_only(self):
        self.put("FMOD/Banks/Desktop/Music.bank", X.bank(guid=X.GUIDS["Music"]))
        p = self.plan(media=None)
        self.assertEqual(p["verdict"], "DRY-RUN ONLY")
        self.assertTrue(p["pending_base_facts"])
        prov = I.provisional_listing(X.profile())
        self.assertIn("fmod/banks/plat/music.bank", prov["entries"])
        p2 = self.plan(media=None, listing=prov)
        self.assertEqual(p2["verdict"], "DRY-RUN ONLY")
        with self.assertRaises(I.Refused):
            I.apply_plan(p2, p2["plan_sha256"], I.LocalBackend(self.console), os.path.join(self.tmp, "j"))

    def test_tampered_plan_and_changed_base_refused(self):
        self.put("FMOD/Banks/Desktop/Music.bank", X.bank(guid=X.GUIDS["Music"]))
        p = self.plan()
        bad = json.loads(json.dumps(p))
        bad["operations"][3]["dst"] = "fmod/banks/plat/master.bank"
        with self.assertRaises(I.Refused) as e:
            I.apply_plan(bad, p["plan_sha256"], I.LocalBackend(self.console), os.path.join(self.tmp, "j1"))
        self.assertEqual(e.exception.code, "plan_tampered")
        with open(os.path.join(self.console, "fmod/banks/plat/music.bank"), "r+b") as f:
            f.seek(100)
            f.write(b"\xff")
        with self.assertRaises(I.Refused) as e:
            I.apply_plan(p, p["plan_sha256"], I.LocalBackend(self.console), os.path.join(self.tmp, "j2"))
        self.assertEqual(e.exception.code, "base_head_changed")


class ReviewRegressions(_InstallerBase):
    """One test per finding of the independent review (2026-10-13)."""

    def test_r1_second_apply_then_restore(self):
        self.put("FMOD/Banks/Desktop/Music.bank", X.bank(guid=X.GUIDS["Music"]))
        p = self.plan()
        orig = sha(open(os.path.join(self.console, "fmod/banks/plat/music.bank"), "rb").read())
        for n in range(3):
            j = os.path.join(self.tmp, "j%d.json" % n)
            I.apply_plan(p, p["plan_sha256"], I.LocalBackend(self.console), j)
            I.restore(p, I.LocalBackend(self.console), j)
            self.assertEqual(sha(open(os.path.join(self.console, "fmod/banks/plat/music.bank"), "rb").read()), orig)

    def test_r2_unknown_master_bank_refuses_all_banks(self):
        self.put("FMOD/Banks/Desktop/Master.bank", X.bank(guid=X.GUIDS["Master"]))
        prof = X.profile()
        prof["audio"]["fmod"]["master_bank"] = None
        p = I.make_plan(prof, self.listing, self.media, self.mod, [], "toolsha")
        self.assertEqual(self.codes(p), ["master_bank_unknown"])

    def test_r3_mp4_gate(self):
        cases = [(X.mp4(audio=False), "mp4_tracks_differ"),  # video-only for a video+audio file
                 (X.mp4(video="hvc1", profile=1, level=90), "mp4_codec_unproven")]
        bad_avcc = X.mp4().replace(b"avcC", b"zzzz")  # level/profile unreadable
        cases.append((bad_avcc, "mp4_video_facts_missing"))
        no_tracks = X.box(b"ftyp", b"isom" + bytes(4)) + X.box(b"moov", X.box(b"mvhd", bytes(100)))
        cases.append((no_tracks, "mod_file_mp4_no_tracks"))
        for data, code in cases:
            shutil.rmtree(self.mod, ignore_errors=True)
            self.put("Movies/Tv.mp4", data)
            self.assertEqual(self.codes(self.plan()), [code])

    def test_r4_crashes_become_refusals(self):
        stsd_at_eof = X.box(b"ftyp", b"isom" + bytes(4)) + X.box(b"moov", X.box(b"trak", X.box(b"mdia", X.box(
            b"hdlr", bytes(8) + b"vide" + bytes(12)) + X.box(b"minf", X.box(b"stbl", X.box(
                b"stsd", bytes(4) + struct.pack(">I", 1)))))))
        cases = [("Movies/Intro.bk2", b"KB2\xe9" + X.bink()[4:], "mod_file_bink_revision"),
                 ("Movies/Tv.mp4", stsd_at_eof, "mod_file_mp4_truncated_field"),
                 ("Movies/Tv.mp4", X.mp4()[:-30], "mod_file_mp4_box_overflow")]
        shutil.rmtree(self.mod, ignore_errors=True)
        self.put("Movies/Intro.bk2", X.bink(b"KB2j", fps=(1, 3000), frames=2, total=8192))
        p = self.plan()  # fps 1/3000 must not divide by zero
        self.assertTrue(p["files"] or p["refused"])
        for rel, data, code in cases:
            shutil.rmtree(self.mod, ignore_errors=True)
            self.put(rel, data)
            p = self.plan()
            self.assertEqual(p["verdict"], "REFUSED", rel)
            if code:
                self.assertEqual(self.codes(p), [code])

    def test_r5_restore_refuses_tampered_plan(self):
        self.put("FMOD/Banks/Desktop/Music.bank", X.bank(guid=X.GUIDS["Music"]))
        p = self.plan()
        j = os.path.join(self.tmp, "j.json")
        I.apply_plan(p, p["plan_sha256"], I.LocalBackend(self.console), j)
        bad = json.loads(json.dumps(p))
        bad["undo"].append({"op": "restore", "path": "fmod/banks/plat/master.bank",
                            "backup_stem": "fmod/banks/plat/dialogue_en", "aside_stem": "x", "ours_sha256": "0"})
        with self.assertRaises(I.Refused) as e:
            I.restore(bad, I.LocalBackend(self.console), j)
        self.assertEqual(e.exception.code, "plan_tampered")
        r = I.restore(p, I.LocalBackend(self.console), j)
        self.assertEqual([x["path"] for x in r["restored"]], ["fmod/banks/plat/music.bank"])

    def test_r6_check_fails_on_missing_facts(self):
        out = os.path.join(self.tmp, "fetched2")
        heads = av_fetch.heads_plan(self.listing)
        X.simulate_range_fetch(heads, self.console, out)
        hops = av_fetch.hops_plan([heads], out)
        self.assertTrue(hops["chunks"])
        # hops planned but never fetched -> failures, not silent passes
        res = av_fetch.check(self.listing, out, [heads, hops], X.profile())
        self.assertTrue(any("not fetched" in f for f in res["failures"]))
        res2 = av_fetch.check(self.listing, out, [heads], X.profile())  # hop plan not even passed
        self.assertTrue(any("sample-data header not reached" in f for f in res2["failures"]))
        self.assertTrue(any("moov" in f for f in res2["failures"]))

    def test_r6b_snd_header_in_head_payload_outside(self):
        # bank whose SND chunk header sits just inside the head, payload outside
        root = os.path.join(self.tmp, "c2")
        b = X.bank(guid=X.GUIDS["Music"], metadata_bytes=65536 - 120)
        k = b.index(b"SND ")
        self.assertTrue(k < 65536 < k + 8 + 64)
        X.write(os.path.join(root, "fmod/banks/plat/music.bank"), b)
        lst = av_fetch.walk(X.fake_list_dir(root, "/r"), "/r", ["fmod"])
        out = os.path.join(self.tmp, "f2")
        heads = av_fetch.heads_plan(lst)
        X.simulate_range_fetch(heads, root, out)
        hops = av_fetch.hops_plan([heads], out)
        self.assertEqual([c["hop_offset"] for c in hops["chunks"]], [k + 8])
        X.simulate_range_fetch(hops, root, out)
        res = av_fetch.check(lst, out, [heads, hops], X.profile())
        self.assertEqual(res["failures"], [])
        self.assertEqual(res["files"]["fmod/banks/plat/music.bank"]["fsb"][0]["codec"], "VORBIS")

    def test_r13_bank_without_samples_and_movie_only_mod(self):
        f = self.media["files"]["fmod/banks/plat/master.strings.bank"]
        self.assertTrue(f["parsed"])
        self.assertEqual(f["fsb"], [])
        self.put("Movies/Intro.bk2", X.bink(b"KB2j", total=6000))
        self.assertEqual(self.plan()["verdict"], "READY")

    def test_r14_oversized_moov_is_a_per_file_failure(self):
        root = os.path.join(self.tmp, "c3")
        big = X.mp4(faststart=False, mdat=70000)
        k = big.index(b"moov") - 4
        big = big[:k] + struct.pack(">I", 9 << 20) + big[k + 4:] + bytes((9 << 20) - (len(big) - k))
        X.write(os.path.join(root, "movies/tv.mp4"), big)
        X.write(os.path.join(root, "movies/intro.bk2"), X.bink(b"KB2j", total=8192))
        lst = av_fetch.walk(X.fake_list_dir(root, "/r"), "/r", ["movies"])
        out = os.path.join(self.tmp, "f3")
        heads = av_fetch.heads_plan(lst)
        X.simulate_range_fetch(heads, root, out)
        hops1 = av_fetch.hops_plan([heads], out)  # round 1: reads the moov box header
        self.assertEqual([c["hop_offset"] for c in hops1["chunks"]], [k])
        X.simulate_range_fetch(hops1, root, out)
        hops2 = av_fetch.hops_plan([heads, hops1], out)  # round 2: moov over the cap -> no hop, no abort
        self.assertEqual(hops2["chunks"], [])
        res = av_fetch.check(lst, out, [heads, hops1], X.profile())
        self.assertTrue(res["files"]["movies/intro.bk2"]["parsed"])
        self.assertTrue(any("tv.mp4" in x for x in res["failures"]))

    def test_r9_partial_upload_moved_aside(self):
        self.put("FMOD/Banks/Desktop/Music.bank", X.bank(guid=X.GUIDS["Music"]))
        p = self.plan()
        orig = sha(open(os.path.join(self.console, "fmod/banks/plat/music.bank"), "rb").read())
        j = os.path.join(self.tmp, "j.json")
        with self.assertRaises(I.Stopped):
            I.apply_plan(p, p["plan_sha256"], I.LocalBackend(self.console, fail_after=1, partial_on_fail=True), j)
        r = I.restore(p, I.LocalBackend(self.console), j)
        self.assertIn("unexpected/partial", r["restored"][0]["state"])
        self.assertEqual(sha(open(os.path.join(self.console, "fmod/banks/plat/music.bank"), "rb").read()), orig)

    def test_r11_lister_root_must_be_absolute(self):
        for bad in ("relative/dir", "/", ""):
            with self.assertRaises(av_fetch.FetchError):
                av_fetch.walk(X.fake_list_dir(self.console, "/x"), bad, ["fmod"])

    def test_r12_tool_sha_bound(self):
        self.put("FMOD/Banks/Desktop/Music.bank", X.bank(guid=X.GUIDS["Music"]))
        p = self.plan()
        with self.assertRaises(I.Refused) as e:
            I.apply_plan(p, p["plan_sha256"], I.LocalBackend(self.console), os.path.join(self.tmp, "j"),
                         tool_sha="other")
        self.assertEqual(e.exception.code, "tool_changed_since_plan")


class RealGameFiles(unittest.TestCase):
    """Optional: the profile builder on the game's own files (skipped if absent)."""

    def test_profile_on_game_files(self):
        root = os.path.dirname(os.path.dirname(os.path.dirname(TOOL)))
        mirror = os.path.join(root, "what/outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013")
        exe = os.path.join(root, "what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin")
        lib = os.path.join(root, "what/outputs/2026-10-06-recovery-fakesign/tf-libfmod.prx.self")
        if not all(os.path.exists(p) for p in (mirror, exe, lib)):
            self.skipTest("game files not on this Mac")
        prof = P.build(mirror, "PS5", exe, [lib])
        fm = prof["audio"]["fmod"]
        self.assertTrue(fm["version_agree"])
        self.assertEqual(fm["platform_bank_dirs"], ["FMOD/Banks/PS5"])
        self.assertIn("VORBIS", fm["fsb_codecs_decodable"])
        self.assertNotIn("XMA", fm["fsb_codecs_decodable"])
        self.assertFalse(prof["audio"]["engine"]["enabled"])
        self.assertTrue(prof["exe"]["bink1_refusal_string"])
        self.assertIn("KB2j", prof["exe"]["bink_header_checks"]["bink2_accepted"])
        self.assertTrue(fm["vorbis_table_validated"])
        self.assertGreaterEqual(len(fm["vorbis_setup_crcs"]), fm["vorbis_setup_headers"])


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    n = suite.countTestCases()
    print("test_av: %d tests" % n)
    res = unittest.TextTestRunner(verbosity=1).run(suite)
    sys.exit(0 if n and res.wasSuccessful() else 1)
