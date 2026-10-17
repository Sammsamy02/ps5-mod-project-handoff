#!/usr/bin/env python3
"""av_profile.py -- build the SOUND/VIDEO base profile of a game from its own files (Mac-only).

Generic per engine: no game, mod or platform names in the logic. Every fact is
read from the game's own files that already sit on the Mac:
  * the game's main classic .pak (config .ini hierarchy, .uproject,
    AssetRegistry.bin) -- read with the pinned pak reader of capability #7,
  * the IoStore .utoc directory indexes (package names),
  * the game's executable (decrypted ELF copy) -- UTF-16 string table and the
    Bink header check constants in its code,
  * the audio middleware runtime libraries the game ships (string tables).
Nothing is written except the --out JSON. Nothing touches the console.

Each verdict in the profile lists its sources; a fact with fewer than two
independent sources is flagged "single_source" and the installer treats it
as advisory only (fail closed).

usage: av_profile.py --mirror DIR --platform NAME --exe ELF [--audio-lib LIB ...]
                     --out profile.json
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import re
import sys
from typing import Dict, List, Optional, Tuple

HERE = os.path.dirname(os.path.abspath(__file__))
L25 = os.environ.get("AV_L25_PREP", os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(HERE))), "what", "outputs", "2026-10-08-l25-prep"))
PAK_READER = os.path.join(L25, "pak-overlay", "overlay_builder.py")
MF = os.path.join(L25, "mod-manager", "material-fallback")


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def load_module(name: str, path: str):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


class ProfileError(RuntimeError):
    pass


def need(ok, why):
    if not ok:
        raise ProfileError(why)


# ---------------------------------------------------------------------------
# UE ini hierarchy (public semantics: + add unique, . add, - remove, ! clear)
# ---------------------------------------------------------------------------

def parse_ini(text: str) -> List[Tuple[str, str, str, str]]:
    """-> [(section, op, key, value)] in file order."""
    out = []
    sec = None
    for raw in text.lstrip("﻿").splitlines():
        line = raw.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            sec = line[1:-1]
            continue
        if sec is None or "=" not in line and not line.startswith("!"):
            continue
        op = line[0] if line[0] in "+-.!" else "="
        body = line[1:] if op != "=" else line
        key, _, val = body.partition("=")
        out.append((sec, op, key.strip(), val.strip()))
    return out


class Config:
    def __init__(self):
        self.vals: Dict[Tuple[str, str], List[str]] = {}
        self.layers: List[str] = []
        self.trace: Dict[Tuple[str, str], List[str]] = {}

    def apply(self, name: str, text: str) -> None:
        self.layers.append(name)
        for sec, op, key, val in parse_ini(text):
            k = (sec, key)
            cur = self.vals.setdefault(k, [])
            if op == "=":
                self.vals[k] = [val]
            elif op == "+":
                if val not in cur:
                    cur.append(val)
            elif op == ".":
                cur.append(val)
            elif op == "-":
                self.vals[k] = [v for v in cur if v != val]
            elif op == "!":
                self.vals[k] = []
            self.trace.setdefault(k, []).append("%s %s%s=%s" % (name, op if op != "=" else "", key, val))

    def get(self, sec: str, key: str) -> Optional[List[str]]:
        return self.vals.get((sec, key))

    def one(self, sec: str, key: str) -> Optional[str]:
        v = self.get(sec, key)
        return v[-1] if v else (None if v is None else "")


def hierarchy(kind: str, project: str, chain: List[str]) -> List[str]:
    files = ["Engine/Config/Base%s.ini" % kind]
    files += ["Engine/Platforms/%s/Config/Base%s%s.ini" % (p, p, kind) for p in chain]
    files += ["%s/Config/Default%s.ini" % (project, kind)]
    files += ["Engine/Platforms/%s/Config/%s%s.ini" % (p, p, kind) for p in chain]
    files += ["%s/Platforms/%s/Config/%s%s.ini" % (project, p, p, kind) for p in chain]
    return files


def resolve(pak_files: Dict[str, bytes], kind: str, project: str, chain: List[str]) -> Config:
    c = Config()
    for f in hierarchy(kind, project, chain):
        if f in pak_files:
            c.apply(f, pak_files[f].decode("utf-8", "replace"))
    return c


def struct_path(v: str) -> str:
    m = re.search(r'Path\s*=\s*"([^"]*)"', v)
    return m.group(1) if m else v.strip('"')


# ---------------------------------------------------------------------------
# executable / library evidence
# ---------------------------------------------------------------------------

def utf16_strings(data: bytes, minlen: int = 5) -> Dict[str, int]:
    out: Dict[str, int] = {}
    for m in re.finditer(rb"(?:[\x20-\x7e]\x00){%d,}" % minlen, data):
        s = m.group().decode("utf-16-le")
        out.setdefault(s, m.start())
    return out


def bink_header_checks(data: bytes) -> dict:
    """Bink magic constants the executable compares a file header against.

    x86-64 'cmp r32, imm32' is 81 /7 (ModRM F8..FF); a revision RANGE check is
    'add r32, -base' (81 /0, ModRM C0..C7) then 'cmp r32, imm8' (83 /7) within
    16 bytes. Constants are 'KB2'/'BIK' + a lowercase revision letter."""
    exact, ranges, offs = set(), set(), []
    for m in re.finditer(rb"\x81[\xf8-\xff](KB2|BIK)([a-z])", data):
        exact.add((m.group(1) + m.group(2)).decode())
        offs.append(m.start())
    for m in re.finditer(rb"\x81[\xc0-\xc7](....)", data, re.S):
        neg = (-int.from_bytes(m.group(1), "little")) & 0xFFFFFFFF
        tag = neg.to_bytes(4, "little")
        if tag[:3] in (b"KB2", b"BIK") and 0x61 <= tag[3] <= 0x7a:
            tail = data[m.end():m.end() + 16]
            k = re.search(rb"\x83[\xf8-\xff](.)(\x77|\x73|\x0f\x87|\x0f\x83)", tail, re.S)
            if k:
                # unsigned range test: 'ja' (fail if above) accepts 0..n, 'jae' 0..n-1;
                # any other jump form is ignored (fewer revisions = fail closed)
                n = k.group(1)[0] + (1 if k.group(2) in (b"\x77", b"\x0f\x87") else 0)
                for i in range(n):
                    if tag[3] + i <= 0x7a:
                        ranges.add((tag[:3] + bytes([tag[3] + i])).decode())
                offs.append(m.start())
    allv = exact | ranges
    return {"exact": sorted(exact), "range": sorted(ranges), "sites": len(offs),
            "bink2_accepted": sorted(v for v in allv if v.startswith("KB2")),
            "bink1_recognised": sorted(v for v in allv if v.startswith("BIK"))}


def vorbis_setup_table(data: bytes, max_header: int = 16384, stride: int = 0x20) -> dict:
    """Find the runtime's table of known FSB5 Vorbis setup IDs (generic search).

    FSB5 Vorbis streams name their setup header by CRC32 (zlib) of the header
    bytes.  The runtime carries the headers (0x05 'vorbis' ...) and a record
    array {u32 size, u32 crc32, ...}.  Anchors: an aligned (u32 L, u32 crc32
    of header[:L]) pair for an embedded header.  From the anchors the array is
    walked with a fixed stride while records look valid.  Validated only if
    every embedded header has a direct record (crc of its own first `size`
    bytes) -- otherwise the table is reported unvalidated (installer refuses)."""
    import struct as _st
    import zlib as _z
    heads = [m.start() for m in re.finditer(rb"\x05vorbis", data)]
    if not heads:
        return {"headers": 0, "validated": False, "crcs": []}
    words = {}
    for off in range(0, len(data) - 8, 4):
        words.setdefault(data[off:off + 8], off)
    anchors = []
    for i, h in enumerate(heads):
        end = min(heads[i + 1] if i + 1 < len(heads) else h + max_header, h + max_header, len(data))
        c = 0
        for L in range(1, end - h + 1):
            c = _z.crc32(data[h + L - 1:h + L], c)
            if L >= 64:
                at = words.get(_st.pack("<II", L, c & 0xFFFFFFFF))
                if at is not None:
                    anchors.append((at, h, L))
                    break
    if not anchors:
        return {"headers": len(heads), "validated": False, "crcs": [], "why": "no anchor record"}
    a0 = min(a for a, _, _ in anchors)
    if any((a - a0) % stride for a, _, _ in anchors):
        return {"headers": len(heads), "validated": False, "crcs": [], "why": "anchors not on one stride"}

    def ok(o):
        if o < 0 or o + 8 > len(data):
            return False
        size, crc = _st.unpack_from("<II", data, o)
        return 64 <= size <= max_header and crc != 0
    start = a0
    while ok(start - stride):
        start -= stride
    recs = []
    o = start
    while ok(o):
        recs.append(_st.unpack_from("<II", data, o))
        o += stride
    inside = all(start <= a < o for a, _, _ in anchors)
    direct = len({h for _, h, _ in anchors})
    return {"headers": len(heads), "records": len(recs), "direct_records": direct, "stride": stride,
            "table_offset": start, "validated": inside and direct == len(heads),
            "crcs": sorted({c for _, c in recs})}


def fmod_lib_facts(data: bytes) -> dict:
    codecs = sorted({m.group(1).decode() for m in re.finditer(rb"FMOD ([A-Za-z0-9 ]+?) Codec\x00", data)})
    srcs = sorted({m.group(0).decode() for m in re.finditer(rb"fmod_codec_[a-z0-9_]+\.cpp", data)})
    wm = re.search(rb"FMOD WATERMARK VERSION:0x([0-9A-Fa-f]{8}) BUILD:(\d+)", data)
    vorbis = [m.start() for m in re.finditer(rb"\x05vorbis", data)]
    return {"codec_names": codecs, "codec_sources": srcs,
            "watermark": ("0x%s build %s" % (wm.group(1).decode(), wm.group(2).decode())) if wm else None,
            # FMOD prints its version as hex nibbles holding decimal digits: 0x00020313 = 2.03.13
            "version": ("%d.%02d.%02d" % (int(wm.group(1)[0:4]), int(wm.group(1)[4:6]),
                                           int(wm.group(1)[6:8]))) if wm and wm.group(1).isdigit() else None,
            "vorbis_setup_headers": len(vorbis), "vorbis_setup_offsets": vorbis,
            "vorbis_setup_table": vorbis_setup_table(data)}


def lib_kind(data: bytes) -> Optional[str]:
    if b"FMOD WATERMARK" in data or b"FMOD FSB Codec" in data:
        return "fmod_core"
    if b"fmod_bankmodel.cpp" in data or b"Bank::getLoadingState" in data:
        return "fmod_studio"
    if b"AK::SoundEngine" in data or b"AkSoundEngine" in data:
        return "wwise"
    return None


# FSB5 codec id -> name of the FMOD codec that decodes it (public mapping;
# PCM rows are decoded inside the FSB codec itself).
FSB_CODEC_NEEDS = {"PCM8": ["FSB"], "PCM16": ["FSB"], "PCM24": ["FSB"], "PCM32": ["FSB"],
                   "PCMFLOAT": ["FSB"], "VORBIS": ["FSB", "Vorbis"], "FADPCM": ["FSB", "FADPCM"],
                   "OPUS": ["FSB", "Opus"], "AT9": ["FSB", "AT9"], "MPEG": ["FSB", "MPEG"],
                   "XMA": ["FSB", "XMA"], "IMAADPCM": ["FSB", "IMA ADPCM"]}


# ---------------------------------------------------------------------------
# main build
# ---------------------------------------------------------------------------

def build(mirror: str, platform: str, exe: Optional[str], libs: List[str]) -> dict:
    B = load_module("av_pak_reader", PAK_READER)
    sys.path.insert(0, MF)
    import utoc  # noqa: E402
    import asset_registry  # noqa: E402
    inputs = {PAK_READER: sha256_file(PAK_READER), os.path.join(MF, "utoc.py"): sha256_file(os.path.join(MF, "utoc.py")),
              os.path.join(MF, "asset_registry.py"): sha256_file(os.path.join(MF, "asset_registry.py"))}
    # 1) every classic pak in the mirror: collect config/project/registry files
    pak_files: Dict[str, bytes] = {}
    paks = sorted(f for f in os.listdir(mirror) if f.endswith(".pak"))
    need(paks, "no .pak in mirror")
    for name in paks:
        path = os.path.join(mirror, name)
        data = open(path, "rb").read()
        try:
            info = B.parse_pak(data)
        except Exception as e:  # empty companion paks are refused by the reader
            inputs[path] = "skipped: %s" % str(e)[:80]
            continue
        inputs[path] = sha256_file(path)
        mount = info["mount"].replace("../../../", "")
        for p, e in info["entries"].items():
            full = (mount + p).lstrip("/")
            if full.endswith((".ini", ".uproject", "AssetRegistry.bin", ".uplugin")):
                need(full not in pak_files, "duplicate pak entry " + full)
                pak_files[full] = data[e.offset:e.offset + e.size]
        del data
    projects = [p for p in pak_files if p.endswith(".uproject") and p.count("/") == 1]
    need(len(projects) == 1, "expected exactly one .uproject, got %r" % projects)
    project = projects[0].split("/")[0]
    uproject = json.loads(pak_files[projects[0]].decode("utf-8-sig"))
    plugins = {p["Name"]: bool(p.get("Enabled")) for p in uproject.get("Plugins", [])}
    # ini parent chain from the platform's DataDrivenPlatformInfo
    chain = [platform]
    ddpi = pak_files.get("Engine/Platforms/%s/Config/DataDrivenPlatformInfo.ini" % platform)
    if ddpi:
        c = Config(); c.apply("ddpi", ddpi.decode("utf-8", "replace"))
        parent = c.one("DataDrivenPlatformInfo", "IniParent")
        if parent:
            chain = [parent, platform]
    eng = resolve(pak_files, "Engine", project, chain)
    game = resolve(pak_files, "Game", project, chain)
    game_default = Config()
    for f in ("Engine/Config/BaseGame.ini", "%s/Config/DefaultGame.ini" % project):
        if f in pak_files:
            game_default.apply(f, pak_files[f].decode("utf-8", "replace"))

    # 2) packaging: loose (NonUFS) folders on this platform vs the default (PC) build
    PPS = "/Script/UnrealEd.ProjectPackagingSettings"
    nonufs = [struct_path(v) for v in (game.get(PPS, "DirectoriesToAlwaysStageAsNonUFS") or [])]
    nonufs_default = [struct_path(v) for v in (game_default.get(PPS, "DirectoriesToAlwaysStageAsNonUFS") or [])]
    removed_by_platform = [struct_path(t.split("=", 1)[1]) for t in
                           game.trace.get((PPS, "DirectoriesToAlwaysStageAsNonUFS"), [])
                           if "/Platforms/" in t.split(" ")[0] and t.split(" ", 1)[1].startswith("-")]

    # 3) asset registry: audio/video classes
    reg_name = "%s/AssetRegistry.bin" % project
    need(reg_name in pak_files, "AssetRegistry.bin missing")
    reg = asset_registry.Registry(pak_files[reg_name])
    by_class: Dict[str, List[dict]] = {}
    for a in reg.assets:
        by_class.setdefault(a["class"], []).append(a)

    def cls(suffix):
        return [a for c, v in by_class.items() if c.endswith(suffix) for a in v]

    # 4) IoStore directory indexes: package names (second source for asset sets)
    toc_paths = {}
    for f in sorted(os.listdir(mirror)):
        if f.endswith(".utoc"):
            t = utoc.load(os.path.join(mirror, f))
            for _row, name in t.files.items():
                toc_paths[name.replace("../../../", "")] = f

    def in_toc(package: str) -> Optional[str]:
        rel = package.replace("/Game/", "%s/Content/" % project, 1) + ".uasset"
        return toc_paths.get(rel)

    # 5) executable evidence
    exe_facts = {}
    if exe:
        inputs[exe] = sha256_file(exe)
        data = open(exe, "rb").read()
        need(data[:4] == b"\x7fELF", "executable must be a readable ELF copy (decrypt-off rules apply on the console)")
        w = utf16_strings(data)
        a = set(m.group().decode() for m in re.finditer(rb"[\x20-\x7e]{6,}", data))
        modules = [v for v in (eng.get("Audio", "AudioInfoModules") or [])]
        exe_facts = {
            "audio_decoder_modules_in_exe": {mname: (mname in w) for mname in modules},
            "audio_mixer_key_in_exe": "AudioMixerModuleName" in w,
            "bink_header_checks": bink_header_checks(data),
            "bink1_refusal_string": "No Bink 1 support." in a,
            "bink_player_class_in_exe": "UBinkMediaPlayer" in w,
            "file_media_source_class_in_exe": "UFileMediaSource" in w,
            "fmod_blueprint_functions": sorted(f for f in ("IsBankLoaded", "LoadBank", "UnloadBank", "LoadBankSampleData")
                                               if f in a),
        }
        del data
    lib_facts = {}
    for lp in libs:
        inputs[lp] = sha256_file(lp)
        data = open(lp, "rb").read()
        k = lib_kind(data)
        if k == "fmod_core":
            lib_facts["fmod_core"] = dict(fmod_lib_facts(data), file=os.path.basename(lp))
        elif k:
            lib_facts[k] = {"file": os.path.basename(lp)}

    # 6) audio verdicts
    FS = "/Script/FMODStudio.FMODSettings"
    fmod_plugin_version = None
    for name, blob in pak_files.items():
        if name.endswith("/FMODStudio.uplugin") and "/Platforms/" not in name:
            fmod_plugin_version = json.loads(blob.decode("utf-8-sig")).get("VersionName")
    bank_dir = struct_path(eng.one(FS, "BankOutputDirectory") or "")
    banks = cls(".FMODBank")
    under = lambda p: bank_dir and p.replace("\\", "/").lower().startswith(bank_dir.lower() + "/")
    platform_bank_dirs = [p for p in nonufs if under(p)]
    pc_bank_dirs = sorted({p for p in nonufs_default + removed_by_platform if under(p)} - set(platform_bank_dirs))
    core = lib_facts.get("fmod_core") or {}
    fmod = None
    if banks or plugins.get("FMODStudio"):
        fmod = {
            "detected_by": [s for s, ok in (("plugin enabled in .uproject", plugins.get("FMODStudio")),
                                             ("FMODSettings in config", eng.get(FS, "BankOutputDirectory") is not None),
                                             ("FMODBank assets in AssetRegistry", bool(banks)),
                                             ("FMOD core runtime library", bool(core))) if ok],
            "version_plugin": fmod_plugin_version, "version_runtime": core.get("version"),
            "runtime_watermark": core.get("watermark"),
            "version_agree": bool(fmod_plugin_version and fmod_plugin_version == core.get("version")),
            "bank_output_dir": bank_dir, "platform_bank_dirs": platform_bank_dirs,
            "pc_bank_dirs": pc_bank_dirs,
            "default_pc_bank_dir": next((p for p in nonufs_default if under(p)), None),
            "master_bank": eng.one(FS, "MasterBankName"), "force_platform_name": eng.one(FS, "ForcePlatformName"),
            "bank_key_set": bool(eng.one(FS, "StudioBankKey")),
            "load_all_banks": eng.one(FS, "bLoadAllBanks"), "load_all_sample_data": eng.one(FS, "bLoadAllSampleData"),
            "locales": eng.get(FS, "Locales") or [],
            "banks": sorted(({"name": a["name"], "package": a["package"], "guid": a["tags"].get("Guid"),
                              "in_toc": in_toc(a["package"])} for a in banks), key=lambda r: r["name"]),
            "bank_lookup_assets": [a["package"] for a in cls(".FMODBankLookup")],
            "event_assets": len(cls(".FMODEvent")),
            "runtime_codecs": core.get("codec_names", []),
            "runtime_codec_sources": core.get("codec_sources", []),
            "vorbis_setup_headers": core.get("vorbis_setup_headers"),
            "vorbis_table_validated": bool((core.get("vorbis_setup_table") or {}).get("validated")),
            "vorbis_setup_crcs": (core.get("vorbis_setup_table") or {}).get("crcs", []),
            "fsb_codecs_decodable": sorted(k for k, req in FSB_CODEC_NEEDS.items()
                                           if all(r in core.get("codec_names", []) for r in req)),
        }
    mixer = eng.one("Audio", "AudioMixerModuleName")
    device = eng.one("Audio", "AudioDeviceModuleName")
    sw = cls(".SoundWave")
    engine_audio = {
        "mixer_module": mixer, "device_module": device,
        "enabled": bool(mixer) or bool(device),
        "mixer_trace": eng.trace.get(("Audio", "AudioMixerModuleName"), []),
        "platform_format": eng.one("Audio", "PlatformFormat"),
        "platform_streaming_format": eng.one("Audio", "PlatformStreamingFormat"),
        "all_wave_formats": eng.get("Audio", "AllWaveFormats") or [],
        "soundwave_assets": len(sw), "soundcue_assets": len(cls(".SoundCue")),
        "metasound_assets": len([a for c, v in by_class.items() if "MetaSound" in c for a in v]),
        "soundwave_compression_types": sorted({a["tags"].get("SoundAssetCompressionType", "?") for a in sw}),
    }
    wwise = {"present": bool(lib_facts.get("wwise")) or any(c.startswith("/Script/AkAudio.") for c in by_class),
             "akaudio_assets": len([a for c, v in by_class.items() if c.startswith("/Script/AkAudio.") for a in v])}

    # 7) video
    bink_players = cls("BinkMediaPlayer.BinkMediaPlayer")
    fms = cls("MediaAssets.FileMediaSource")
    video = {
        "bink": {"plugin_enabled": plugins.get("BinkMedia", False), "player_assets": len(bink_players),
                 "texture_assets": len(cls("BinkMediaPlayer.BinkMediaTexture")),
                 "players": sorted(a["package"] for a in bink_players)},
        "electra": {"plugin_enabled": plugins.get("ElectraPlayer", False),
                    "file_media_sources": sorted(({"package": a["package"], "file_path": a["tags"].get("FilePath")}
                                                  for a in fms), key=lambda r: r["package"])},
        "other_media_plugins": {k: v for k, v in plugins.items() if k.endswith("Media") and k != "BinkMedia"},
        "media_dirs": sorted({(r["file_path"] or "").lstrip("./").split("/")[0] for r in
                              ({"file_path": a["tags"].get("FilePath")} for a in fms) if r["file_path"]}),
    }
    return {"schema": "av-base-profile/1", "label": "Mac-only", "project": project, "platform": platform,
            "ini_chain": chain, "plugins": {k: v for k, v in plugins.items()
                                            if any(s in k for s in ("FMOD", "Bink", "Electra", "Media", "Wwise", "Ak", "Opus"))},
            "staging": {"nonufs_platform": nonufs, "nonufs_default": nonufs_default,
                        "removed_by_platform": removed_by_platform},
            "audio": {"fmod": fmod, "engine": engine_audio, "wwise": wwise},
            "video": video, "exe": exe_facts, "libs": lib_facts,
            "registry_counts": reg.counts, "inputs": inputs}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--mirror", required=True)
    ap.add_argument("--platform", required=True)
    ap.add_argument("--exe")
    ap.add_argument("--audio-lib", nargs="*", default=[])
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    try:
        prof = build(a.mirror, a.platform, a.exe, a.audio_lib)
    except ProfileError as e:
        print("av_profile: REFUSED:", e)
        return 2
    with open(a.out, "w") as f:
        json.dump(prof, f, indent=1, sort_keys=True)
    fm = prof["audio"]["fmod"] or {}
    n = len(fm.get("banks", [])) + prof["video"]["bink"]["player_assets"] + \
        len(prof["video"]["electra"]["file_media_sources"]) + prof["audio"]["engine"]["soundwave_assets"]
    print("av_profile: %s/%s: %d FMOD banks, %d Bink players, %d file media sources, %d SoundWaves; "
          "engine audio %s; inputs %d" % (prof["project"], prof["platform"], len(fm.get("banks", [])),
                                          prof["video"]["bink"]["player_assets"],
                                          len(prof["video"]["electra"]["file_media_sources"]),
                                          prof["audio"]["engine"]["soundwave_assets"],
                                          "ENABLED" if prof["audio"]["engine"]["enabled"] else "DISABLED",
                                          len(prof["inputs"])))
    return 0 if n else 1


if __name__ == "__main__":
    sys.exit(main())
