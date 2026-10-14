#!/usr/bin/env python3
"""run_phasea_tests.py — local, read-only validation of the two PHASE A tools.

All fixtures are READ-ONLY files that already exist on this Mac:
  negative control : FZ-GAME2/astro-inner-decrypted.elf   (Astro Bot, non-UE)
  positive control : townfall decrypted eboot             (UE 5.6.1 shipping)
  crash fixture    : /tmp/l25-variant-A/klog-20261009-021938.log (real crash
                     blocks; two SIGSEGV dumps with module lists)
  map fixture      : l26-A/pass1/runtime.map               (`-r` partial-link)
  prx fixture      : l26-package-A/pass-1/bare-original PRX (dynsym+sections)
  prx sidecar      : prx-symbols-l26-package-A.json        (STAND-IN for the
                     future Packager-emitted sidecar; embeds the true sha256
                     of the prx fixture — exercises the PAIRING machinery)
  wrong-build prx  : l26-package-B/pass-1/bare-original PRX (different build;
                     pairing must HARD-REFUSE)
  fake-SELF        : astro-eboot-faked.self               (container refusal)
  modified build   : fixtures/townfall-modified-build.elf  (test 19: tampered
                     Townfall copy — 12 markers corrupted in every occurrence
                     + 6 old-version foreign markers spliced; MUST downgrade
                     to VERSION_TENTATIVE, never CONFIDENT; deterministic
                     recipe in fixtures/make_gw19_fixture.py)

No console contact, no writes outside this directory (a JSON receipt next to
this script). Exit 0 only if every check passes.
"""
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parent.parent.parent          # .../translation-core/what
ENGINE = str(HERE / "engine_match.py")
SYM = str(HERE / "symbolize_crash.py")
ASTRO = str(HERE.parent / "FZ-GAME2/astro-inner-decrypted.elf")
TOWNFALL = (WHAT / ".superpowers/native-observations/"
            "townfall-static-executable-2026-10-02/decrypted-eboot.bin")
FAKESELF = str(WHAT / "outputs/2026-10-06-recovery-fakesign/astro-eboot-faked.self")
KLOG = "/tmp/l25-variant-A/klog-20261009-021938.log"
MAP = str(WHAT / "outputs/2026-10-08-l26-build-variants/l26-A/pass1/runtime.map")
PRX = str(WHAT / "outputs/2026-10-08-l26-package-A/pass-1/bare-original/"
           "modbridge_supplier_composition.prx")
PRX_WRONG_BUILD = str(
    WHAT / "outputs/2026-10-08-l26-package-B/pass-1/bare-original/"
    "modbridge_supplier_composition.prx")
SIDECAR = str(HERE / "game-symbols-townfall.json")
PRX_SIDECAR = str(HERE / "prx-symbols-l26-package-A.json")

results = []


def check(name, ok, detail=""):
    results.append({"check": name, "pass": bool(ok), "detail": detail})
    print(("PASS " if ok else "FAIL ") + name + ("  " + detail if detail else ""))


def run(args, timeout=600):
    return subprocess.run([sys.executable] + args, capture_output=True,
                          text=True, timeout=timeout)


# ---------------------------------------------------------------- #2 tool ---
print("== #2 engine_match.py ==")
r = run([ENGINE, ASTRO, "--json"], timeout=300)
d = json.loads(r.stdout)
check("astro-refused", r.returncode == 0 and d["tier"] == "REFUSED",
      "tier=%s triage=%d/%d present=%d" %
      (d["tier"], d["triage"]["present"], d["triage"]["set_size"], d["present_count"]))
check("astro-sha", d["image"]["sha256"].startswith("7723d150328e8dd6"),
      d["image"]["sha256"][:16])

r = run([ENGINE, str(TOWNFALL), "--json"], timeout=600)
d = json.loads(r.stdout)
check("townfall-tier", d["tier"] == "VERSION_CONFIDENT", d["tier"])
check("townfall-family-version", d["family"] == "UE5" and d["version"] == "5.6",
      "%s %s" % (d["family"], d["version"]))
check("townfall-known-axes",
      d["axes"]["build_configuration"] == "shipping"
      and d["axes"]["stats"] == "off"
      and d["axes"]["stats_margin"] == 12,
      "build=%s stats=%s margin=%d" % (d["axes"]["build_configuration"],
                                       d["axes"]["stats"], d["axes"]["stats_margin"]))
check("townfall-present-54", d["present_count"] == 54,
      "present=%d" % d["present_count"])
check("townfall-image-id", d["image"]["sha256"].startswith("3b6cfad47dd17fea")
      and d["image"]["extent"] == 0xbc602c8, d["image"]["sha256"][:16])

r = run([ENGINE, FAKESELF, "--json"])
check("fakeself-refused", r.returncode == 2 and "SELF container" in r.stderr,
      r.stderr.strip()[:60])

# UE4 synthetic self-consistency (cost model must resolve UE4 versions too)
sys.path.insert(0, str(HERE))
import engine_match as em  # noqa: E402
vers, marks = em.load_markers(em.DEFAULT_MARKERS)
ok_ue4 = True
details = []
for target, label in [(vers.index((4, 27)), "4.27"), (vers.index((4, 7)), "4.7"),
                      (vers.index((5, 8)), "5.8")]:
    present = [em.covers(m, target, 2, False) for m in marks]
    sol = em.cost_solve(vers, marks, present)
    ok = sol["version"] == "%s" % ("4.27" if label == "4.27" else
                                   "4.7" if label == "4.7" else "5.8")
    ok_ue4 = ok_ue4 and ok
    details.append("%s->%s(m%d)" % (label, sol["version"], sol["version_margin"]))
import random  # noqa: E402
random.seed(7)
present = [em.covers(m, vers.index((4, 27)), 2, False) for m in marks]
for i in random.sample(range(len(marks)), 3):
    present[i] = not present[i]
sol = em.cost_solve(vers, marks, present)
ok_noise = sol["version"] == "4.27"
check("ue4-synthetic-cost-model", ok_ue4 and ok_noise,
      " ".join(details) + " +3flips->%s" % sol["version"])

r = run([ENGINE, "--diff", str(TOWNFALL), ASTRO, "--json"], timeout=900)
d = json.loads(r.stdout)
check("diff-townfall-astro", d["a"]["tier"] == "VERSION_CONFIDENT"
      and d["b"]["tier"] == "REFUSED" and d["differing_count"] == 54,
      "differing=%d" % d["differing_count"])

# ------------------------------------------- test 19 (GW): MODIFIED build ---
# Claude round-6 #2a: the 18 checks had NO modified-Unreal-build case, and the
# tool did NOT fail closed on one (margin-only tiers stayed CONFIDENT while
# absolute misfit exploded: simulated cost 43 -> 103 with margin still 20).
# Fixed by the VERSION_CONFIDENT_MAX_MISFIT guard in engine_match.py
# (misfit_ratio = best_cost / present_count must be <= 1.0 for CONFIDENT).
# This test proves the fix on REAL tampered bytes derived from the Townfall
# positive control: 12 native markers corrupted in every occurrence + 6
# old-version-only foreign markers spliced into data-segment slack (recipe:
# fixtures/make_gw19_fixture.py + tamper-manifest.json, deterministic).
print("== #2 test 19: MODIFIED Unreal build fails closed (GW) ==")
GW19_ELF = HERE / "fixtures/townfall-modified-build.elf"
GW19_MANIFEST = HERE / "fixtures/tamper-manifest.json"
if not (GW19_ELF.exists() and GW19_MANIFEST.exists()):
    run([str(HERE / "fixtures/make_gw19_fixture.py")], timeout=1200)
import hashlib  # noqa: E402
_gw19_sha = hashlib.sha256(GW19_ELF.read_bytes()).hexdigest() if GW19_ELF.exists() else ""
_gw19_man = json.loads(GW19_MANIFEST.read_text()) if GW19_MANIFEST.exists() else {}
_gw19_ok = (_gw19_man.get("fixture_sha256") == _gw19_sha
            and _gw19_man.get("source_sha256", "").startswith("3b6cfad47dd17fea")
            and GW19_ELF.exists()
            and _gw19_man.get("size") == GW19_ELF.stat().st_size)
check("gw19-fixture-integrity", _gw19_ok,
      "sha=%s corrupt=%d splice=%d"
      % (_gw19_sha[:16],
         _gw19_man.get("tamper", {}).get("corrupt", {}).get("count", -1),
         _gw19_man.get("tamper", {}).get("splice", {}).get("count", -1)))

r = run([ENGINE, str(GW19_ELF), "--json"], timeout=600)
d = json.loads(r.stdout)
check("gw19-modified-build-not-confident",
      r.returncode == 0 and d["tier"] == "VERSION_TENTATIVE"
      and d["family"] == "UE5",
      "tier=%s family=%s present=%d misfit=%.3f"
      % (d["tier"], d.get("family"), d["present_count"],
         d.get("misfit_ratio", 0.0)))
check("gw19-demotion-cause-is-misfit-guard",
      d.get("misfit_ratio", 0.0) > em.VERSION_CONFIDENT_MAX_MISFIT
      and d["axes"]["version_margin"] >= em.VERSION_MARGIN_CONFIDENT,
      "margin=%d (CONFIDENT-grade) but misfit=%.3f > %.1f -> demoted"
      % (d["axes"]["version_margin"], d.get("misfit_ratio", 0.0),
         em.VERSION_CONFIDENT_MAX_MISFIT))

# ---------------------------------------------------------------- #5 tool ---
print("== #5 symbolize_crash.py ==")
r = run([SYM, "--klog", KLOG, "--game-symbols", SIDECAR, "--json"], timeout=120)
d = json.loads(r.stdout)
blocks = d["crash_blocks"]
check("klog-blocks-found", len(blocks) >= 2, "blocks=%d" % len(blocks))
eb = [m for b in blocks for m in b["modules"] if m["path"] == "/app0/eboot.bin"]
check("klog-eboot-base-law", eb and all(m["base"] == "0x400000" for m in eb),
      "bases=%s" % sorted({m["base"] for m in eb}))
fp = {m["fingerprint"] for m in eb}
check("klog-eboot-build-id",
      fp == {"04bb4330a7d71dcefe1cc2c4d19769d500000000"},
      str(fp))
ripmods = [s["module"] for b in blocks for s in b["symbolized"]
           if s.get("module_offset") and "libkernel" in (s.get("module") or "")]
check("klog-rip-in-libkernel", len(ripmods) >= 2, str(ripmods))

r = run([SYM, "--addr", "0x3c4d583", "--base", "0x400000",
         "--game-symbols", SIDECAR, "--json"])
s = json.loads(r.stdout)["single"]
hit = s.get("symbols", [{}])[0]
check("game-sidecar-loadmap", "LoadMap" in hit.get("symbol", "")
      and hit.get("offset_in_symbol") == 0x123,
      "%s+%s" % (hit.get("symbol"), hex(hit.get("offset_in_symbol", 0))))

r = run([SYM, "--prx", PRX, "--addr", "0x10015a0", "--base", "0x1000000",
         "--json"])
s = json.loads(r.stdout)["single"]
hit = s.get("symbols", [{}])[0]
check("prx-dynsym-module-start",
      "module_start/DT_INIT" in hit.get("symbol", "")
      and hit.get("offset_in_symbol") == 0 and hit.get("tier") == "DYNSYM-EXACT",
      hit.get("symbol", "?"))

# --- #5 sidecar PAIRING (Claude round-5 review ask ii) ---
r = run([SYM, "--symbols-sidecar", PRX_SIDECAR, "--prx", PRX,
         "--addr", "0x10015a0", "--base", "0x1000000", "--json"], timeout=300)
d = json.loads(r.stdout)
hit = next((x for x in d["single"].get("symbols", [])
            if x.get("tier") == "SIDECAR-EXACT"), {})
check("sidecar-pairing-verified",
      r.returncode == 0
      and d["prx_symbols_sidecar"]["pairing"] == "verified"
      and "module_start/DT_INIT" in hit.get("symbol", "")
      and hit.get("offset_in_symbol") == 0,
      "pairing=%s %s+%s" % (d["prx_symbols_sidecar"]["pairing"],
                            hit.get("symbol", "?")[:30],
                            hex(hit.get("offset_in_symbol", -1))))

r = run([SYM, "--symbols-sidecar", PRX_SIDECAR, "--prx", PRX_WRONG_BUILD,
         "--addr", "0x10015a0", "--base", "0x1000000", "--json"], timeout=300)
check("sidecar-pairing-mismatch-refused",
      r.returncode == 2 and "PAIRING MISMATCH" in r.stderr,
      r.stderr.strip()[:60])

r = run([SYM, "--symbols-sidecar", SIDECAR, "--addr", "0x3c4d583",
         "--base", "0x400000", "--json"])
check("sidecar-unpairable-refused",
      r.returncode == 2 and "prx_sha256" in r.stderr,
      r.stderr.strip()[:60])

r = run([SYM, "--symbols-sidecar", PRX_SIDECAR, "--addr", "0x10015a0",
         "--base", "0x1000000", "--json"])
d = json.loads(r.stdout)
check("sidecar-unverified-no-prx",
      r.returncode == 0
      and d["prx_symbols_sidecar"]["pairing"].startswith("unverified")
      and any(x["tier"] == "SIDECAR-EXACT"
              for x in d["single"].get("symbols", [])),
      d["prx_symbols_sidecar"]["pairing"])

r = run([SYM, "--prx", PRX, "--map", MAP, "--addr", "0x1000014",
         "--base", "0x1000000", "--json"], timeout=300)
s = json.loads(r.stdout)["single"]
tiers = {x["tier"] for x in s.get("symbols", [])}
hit = next((x for x in s.get("symbols", []) if x["tier"] == "MAP-RECONSTRUCTED"), None)
check("map-reconstruction-tier",
      hit is not None and hit.get("offset_in_symbol") == 0x4,
      (hit or {}).get("symbol", "no MAP-RECONSTRUCTED hit")[:60])

import symbolize_crash as sc  # noqa: E402
mp = sc.parse_map(MAP)
tot = sum(x["size"] for x in mp["sections"] if x["name"].startswith(".text"))
ratio = tot / 0x123edcf
check("map-text-coverage", 0.9 < ratio < 1.05 and not mp["has_real_vmas"],
      "map .text=0x%x ratio=%.4f nonzero_rows=%d" %
      (tot, ratio, mp["nonzero_vma_rows"]))

# ------------------------------------------------------------------ save ----
out = {"tool_tests": results,
       "passed": sum(1 for x in results if x["pass"]),
       "failed": sum(1 for x in results if not x["pass"])}
(HERE / "phasea-test-receipt.json").write_text(json.dumps(out, indent=1) + "\n")
print("--\n%d passed, %d failed (receipt: phasea-test-receipt.json)"
      % (out["passed"], out["failed"]))
sys.exit(0 if out["failed"] == 0 else 1)
