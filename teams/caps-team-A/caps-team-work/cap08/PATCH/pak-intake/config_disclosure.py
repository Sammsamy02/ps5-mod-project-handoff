#!/usr/bin/env python3
"""Plain-language disclosure for config (.ini) mods: what each console variable
changes on screen, compared with the game's own config files.  Mac-only.

Engine-level cvar families only (r.*, sg.*); no game or mod names.  Exit 0 when
at least one setting was checked, 2 when zero settings were found (a gate that
checks nothing fails)."""
import argparse
import json
import re
import sys
from typing import Dict, List, Optional, Tuple

sys.dont_write_bytecode = True

# (pattern, category, plain effect).  First match wins.
FAMILIES: List[Tuple[str, str, str]] = [
    (r"(?i).*(DX12|D3D12|D3D11|DX11|Vulkan|SwapChain).*", "pc-api",
     "PC graphics-API setting; the console has no such API, so expect no effect"),
    (r"(?i)^r\.(VolumetricFog|Fog|ExponentialHeightFog).*", "fog",
     "fog / haze changed or removed: scenes look clearer and less foggy"),
    (r"(?i)^r\.(EyeAdaptation|DefaultFeature\.AutoExposure|ExposureOffset|AutoExposure).*", "exposure",
     "automatic exposure changed or off: dark places can look much brighter"),
    (r"(?i)^r\.(Tonemapper|Color\.|Gamma|TonemapperGamma|SceneColorFormat).*", "brightness-colour",
     "brightness / colour curve changed"),
    (r"(?i)^r\.(Shadow|ShadowQuality|DistanceFieldShadowing|ContactShadows|AllowPointLightCubemapShadows|CapsuleShadows).*",
     "shadows", "fewer, smaller or no shadows: scenes look flatter and brighter"),
    (r"(?i)^r\.(Lumen|LumenScene|Reflections|SSR|RayTracing|DistanceFieldAO|AmbientOcclusion|SSAO).*", "indirect-light",
     "bounced light / reflections / ambient occlusion changed: dark corners can look lighter"),
    (r"(?i)^r\.(DefaultFeature\.MotionBlur|MotionBlur|DepthOfField|SceneColorFringe|BloomQuality|LensFlare|Tonemapper\.Grain).*",
     "post-effects", "post effects (motion blur, depth of field, colour fringe) changed or off"),
    (r"(?i)^r\.Streaming\..*", "texture-memory",
     "texture streaming / memory budget; console memory differs from PC"),
    (r"(?i)^r\.FidelityFX\..*", "upscaler", "PC upscaler / frame-generation setting"),
    (r"(?i)^sg\..*", "scalability", "overall quality preset group"),
]
LOOK_CATEGORIES = {"fog", "exposure", "brightness-colour", "shadows", "indirect-light"}


def parse_ini(text: str) -> List[Tuple[str, str, str]]:
    out = []
    section = ""
    for raw in text.splitlines():
        line = raw.strip().lstrip("﻿")
        if not line or line.startswith((";", "#")):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
            continue
        if "=" in line:
            k, v = line.split("=", 1)
            k = k.strip().lstrip("+-!.")
            v = v.split(";", 1)[0].strip()
            out.append((section, k, v))
    return out


def classify(key: str) -> Tuple[str, str]:
    for pat, cat, text in FAMILIES:
        if re.match(pat, key):
            return cat, text
    return "other", "not classified; effect unknown"


def disclose(mod_text: str, base_texts: Dict[str, str]) -> Dict[str, object]:
    """Matches by (section, key): in an ini the same key in another section is a
    different setting, and a later line overrides an earlier one in the same
    section.  Values of the same key in OTHER sections are listed separately."""
    base: Dict[Tuple[str, str], Tuple[str, str]] = {}
    base_any: Dict[str, List[Tuple[str, str, str]]] = {}
    for name, t in base_texts.items():
        for sec, k, v in parse_ini(t):
            base[(sec.lower(), k.lower())] = (v, name)  # later line wins (ini semantics)
            base_any.setdefault(k.lower(), []).append((sec, v, name))
    parsed = parse_ini(mod_text)
    last_at = {}
    for i, (sec, k, v) in enumerate(parsed):
        last_at[(sec.lower(), k.lower())] = i
    rows = []
    for i, (sec, k, v) in enumerate(parsed):
        cat, text = classify(k)
        bv = base.get((sec.lower(), k.lower()))
        other = [{"section": bs, "value": bvv} for bs, bvv, _ in base_any.get(k.lower(), [])
                 if bs.lower() != sec.lower()]
        rows.append({"section": sec, "key": k, "mod_value": v, "game_value": bv[0] if bv else None,
                     "game_value_source": bv[1] if bv else None, "game_values_other_sections": other,
                     "overridden_later_in_mod": last_at[(sec.lower(), k.lower())] != i,
                     "category": cat, "effect": text})
    look = sorted({r["category"] for r in rows if r["category"] in LOOK_CATEGORIES})
    headline = None
    if look:
        headline = ("This config mod changes how the game LOOKS by design (%s). Brighter, clearer, "
                    "less scary scenes are the likely result on any platform, not a port bug." % ", ".join(look))
    return {"label": "Mac-only", "settings_checked": len(rows), "look_categories": look,
            "headline": headline, "settings": rows}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--mod-ini", required=True)
    ap.add_argument("--game-ini", action="append", default=[], help="the game's own ini (from its pak)")
    ap.add_argument("--out")
    a = ap.parse_args(argv)
    with open(a.mod_ini, encoding="utf-8-sig", errors="replace") as f:
        mod_text = f.read()
    base = {}
    for p in a.game_ini:
        with open(p, encoding="utf-8-sig", errors="replace") as f:
            base[p] = f.read()
    r = disclose(mod_text, base)
    if a.out:
        with open(a.out, "w") as f:
            json.dump(r, f, indent=1)
    print("settings checked: %d" % r["settings_checked"])
    if r["headline"]:
        print(r["headline"])
    for s in r["settings"]:
        gv = "" if s["game_value"] is None else " (game: %s)" % s["game_value"]
        if s["game_values_other_sections"]:
            gv += " (game, other sections: %s)" % ", ".join("[%s] %s" % (o["section"], o["value"])
                                                         for o in s["game_values_other_sections"])
        if s["overridden_later_in_mod"]:
            gv += " (overridden later in this file)"
        print("  %-48s %-10s%s  -> %s" % (s["key"], s["mod_value"], gv, s["effect"]))
    return 0 if r["settings_checked"] > 0 else 2


if __name__ == "__main__":
    sys.exit(main())
