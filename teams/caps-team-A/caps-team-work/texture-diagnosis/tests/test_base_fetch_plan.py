"""Fetch plan: exact ranges and IoHash verification (positive + negative)."""
import json
import os
import unittest

from common import (MOD2K_UTOC, MOD2K_UCAS, BASE_UTOCS, GLOBAL_UTOC, GLOBAL_UCAS, OUT, fresh_tmp, record,
                    require_inputs)
import iostore_lib as io
import base_fetch_plan as bfp


def _write(path, data):
    with open(path, "wb") as f:
        f.write(data)


BASE_WANTED = [
    "Townfall/Content/Townfall/Characters/Simon/Textures/T_Shirt_D.uasset",
    "Townfall/Content/Townfall/Characters/Simon/Textures/T_Simon_Body_FirstStage_D.uasset",
    "Townfall/Content/Townfall/Characters/Materials/M_MRO_Cloth_NonVT.uasset",
    "Townfall/Content/Townfall/Characters/Simon/Materials/MI_Simon_Shirt.uasset",
    "Townfall/Content/MetaHumans/Common/Materials/M_Hide.uasset",
]


class TestFetchPlan(unittest.TestCase):
    def setUp(self):
        require_inputs(self, [MOD2K_UTOC, MOD2K_UCAS, GLOBAL_UTOC, GLOBAL_UCAS] + BASE_UTOCS)

    def test_base_plan_ranges_cover_lengths(self):
        res = bfp.plan(BASE_UTOCS[0], "pakchunk0-ps5.ucas", BASE_WANTED, False)
        self.assertEqual(res["missing"], [])
        self.assertEqual(len(res["rows"]), len(BASE_WANTED))
        for r in res["rows"]:
            self.assertEqual(sum(x["length"] for x in r["ucas_ranges"]), r["logical_length"])
            self.assertEqual(r["compression"], ["None"])
            self.assertEqual(len(r["expected_iohash20"]), 40)
        with open(os.path.join(OUT, "base-fetch-plan.json"), "w") as f:
            json.dump(res, f, indent=1)
        record("base_fetch_rows", len(res["rows"]))

    def test_verify_positive_and_negative_on_mod_chunks(self):
        toc = io.parse_toc(MOD2K_UTOC)
        script = io.load_script_objects(GLOBAL_UTOC, GLOBAL_UCAS)
        tmp = fresh_tmp("fetch-verify")
        n = 0
        tex = [i for i, p in toc.paths.items() if p.endswith(".uasset") and "/Textures/" in p][:3]
        for i in tex:
            # emulate the integrator's raw-range read: concatenate the planned physical ranges
            rngs = io.chunk_physical_ranges(toc, i)
            with open(MOD2K_UCAS, "rb") as f:
                data = b""
                for s, e in rngs:
                    f.seek(s)
                    data += f.read(e - s)
            good = os.path.join(tmp, "good-%d.bin" % i)
            _write(good, data)
            res = bfp.verify(MOD2K_UTOC, i, good, script)
            self.assertEqual(res["result"], "VERIFIED")
            self.assertEqual(res["exports"][0]["texture"]["status"], "PARSED")
            bad = os.path.join(tmp, "bad-%d.bin" % i)
            _write(bad, data[:-1] + bytes([data[-1] ^ 1]))
            self.assertEqual(bfp.verify(MOD2K_UTOC, i, bad)["result"], "IOHASH_MISMATCH")
            short = os.path.join(tmp, "short-%d.bin" % i)
            _write(short, data[:-5])
            self.assertEqual(bfp.verify(MOD2K_UTOC, i, short)["result"], "LENGTH_MISMATCH")
            n += 1
        self.assertEqual(n, 3)
        record("fetch_verify_cases", n * 3)


if __name__ == "__main__":
    unittest.main()
