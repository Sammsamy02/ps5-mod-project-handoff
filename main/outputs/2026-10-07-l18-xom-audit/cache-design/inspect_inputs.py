#!/usr/bin/env python3
"""Read-only offline l18 audit; this is not runtime cache implementation."""
import hashlib
import json
import mmap
from pathlib import Path
import re
import struct
import time

ROOT = Path(__file__).resolve().parents[3]
UNITS = ROOT / "outputs/2026-10-07-l17-local-lifecycle/units"
IMAGE = ROOT / "outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin"
PIN = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
BASE = 0x400000
INPUTS = [
    "AGENTS.md",
    "outputs/2026-10-06-recovery-fakesign/POSTMORTEM.md",
    "outputs/2026-10-06-recovery-fakesign/PS5-MODDING-MISSION.md",
    "outputs/2026-10-07-l17-local-lifecycle/receipts/FREEZE.json",
    "outputs/2026-10-07-l17-local-lifecycle/units/runtime_start_bridge_r14_diag_spanpub.cpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/runtime_owner.cpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/UnrealInitializer.cpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/SeedTable.h",
    "outputs/2026-10-07-l17-local-lifecycle/units/SeedValidation.hpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/NativeFNameChecks.hpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/FNameAbiProfile.hpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/ImageLoadProfile.hpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/ImageCoverage.hpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/PoolAdmissionEngine.hpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/CopyinStartup.hpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/shadow_copy.hpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/shadow_copy.cpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/shadow_module_source.hpp",
    "outputs/2026-10-07-l17-local-lifecycle/units/shadow_arm.hpp",
    "outputs/2026-10-07-l16-local-handles/receipts/public-api/OpenOrbis-libkernel.h",
    "outputs/2026-10-04-zcode-team/worker-l3-profile/game-profile.townfall-ps5-ue56-v1.json",
    "outputs/2026-10-07-l16-full-build/console-test-20261007T183202Z-2318a17f/CONSOLE-REPORT.md",
    "outputs/2026-10-07-l16-full-build/runs/run-l16-2/commands.json",
    "outputs/2026-10-04-zcode-controller/launch10-build/final-units/live_module_facts.geometryfix.cpp",
    ".superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp",
]


def pattern(tokens):
    return re.compile(b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in tokens.split()), re.DOTALL)


def main():
    inputs = []
    for relative in INPUTS:
        data = (ROOT / relative).read_bytes()
        inputs.append({"path": relative, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    with IMAGE.open("rb") as file:
        before = IMAGE.stat()
        digest = hashlib.sha256()
        started = time.perf_counter()
        n = 0
        while chunk := file.read(65536):
            digest.update(chunk)
            n += len(chunk)
        elapsed = (time.perf_counter() - started) * 1000
        assert digest.hexdigest() == PIN
        with mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as data:
            assert data[:6] == b"\x7fELF\x02\x01"
            etype = struct.unpack_from("<H", data, 16)[0]
            phoff = struct.unpack_from("<Q", data, 0x20)[0]
            phsize, phnum = struct.unpack_from("<HH", data, 0x36)
            assert phsize == 56 and phnum == 14
            assert phoff <= n and phnum * phsize <= n - phoff
            headers = []
            loads = []
            for i in range(phnum):
                values = struct.unpack_from("<IIQQQQQQ", data, phoff + i * phsize)
                names = ["type", "flags", "file_offset", "vaddr", "paddr", "filesz", "memsz", "align"]
                ph = dict(zip(names, values), index=i)
                headers.append(ph)
                if ph["type"] == 1:
                    assert ph["filesz"] <= ph["memsz"]
                    assert ph["file_offset"] <= n and ph["filesz"] <= n - ph["file_offset"]
                    assert ph["memsz"] <= (1 << 64) - 1 - ph["vaddr"]
                    loads.append(ph)
            fname_source = (UNITS / "FNameAbiProfile.hpp").read_text()
            definition = re.search(r"kPrefix\[\] = \{([^}]+)\}", fname_source).group(1)
            fname = bytes(int(v, 16) for v in definition.split(","))
            slots = [
                ("fname_ctor_wchar", 0x23D590, fname, "active exact prologue check"),
                ("static_construct_object_internal", 0x490AA0, bytes.fromhex("55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec 58 02 00 00 48 8b 05 4d 14"), "staged shadow-arm exact prologue; active startup only binds by address class"),
            ]
            functions = []
            for name, linked, expected, status in slots:
                candidates = [p for p in loads if p["vaddr"] <= linked and linked - p["vaddr"] <= p["filesz"] and len(expected) <= p["filesz"] - (linked - p["vaddr"])]
                assert len(candidates) == 1
                p = candidates[0]
                offset = p["file_offset"] + linked - p["vaddr"]
                actual = data[offset:offset + len(expected)]
                assert actual == expected
                functions.append({"name": name, "status": status, "linked_vaddr": linked, "runtime_for_pinned_base": BASE + linked, "size": len(expected), "file_offset": offset, "pt_load_index": p["index"], "pt_load_flags": p["flags"], "actual_hex": actual.hex(), "matches_pinned_signature": True})
            signatures = [
                ("fname_ctor_16", "55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74", None),
                ("sco_25", "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec 58 02 00 00 48 8b 05 4d 14", None),
                ("gua_primary", "48 8d 1d ?? ?? ?? ?? 4c 89 e7 4c 89 fe ff 50 18", 0x93BA110),
                ("gua_corroborating", "48 8d 1d ?? ?? ?? ?? bf 00 00 00 00 39 43 24 7e", 0x93BA110),
                ("gmalloc_bootstrap_store", "48 89 1d ?? ?? ?? ?? 4c 8b 35 ?? ?? ?? ?? 0f b6", 0x930ADD8),
            ]
            scans = []
            for name, sig, target in signatures:
                hits = []
                for p in loads:
                    if not (p["flags"] & 1):
                        continue
                    for match in pattern(sig).finditer(data, p["file_offset"], p["file_offset"] + p["filesz"]):
                        linked = p["vaddr"] + match.start() - p["file_offset"]
                        hit = {"linked_vaddr": linked, "file_offset": match.start(), "actual_hex": match.group().hex(), "pt_load_index": p["index"]}
                        if target is not None:
                            decoded = linked + 7 + struct.unpack_from("<i", data, match.start() + 3)[0]
                            hit.update(decoded_rip_target=decoded, expected_target=target, agrees_with_seed=decoded == target)
                            assert decoded == target
                        hits.append(hit)
                scans.append({"name": name, "pattern": sig, "hit_count": len(hits), "hits": hits, "active_startup_scan": False})
        after = IMAGE.stat()
        assert before.st_size == after.st_size and before.st_mtime_ns == after.st_mtime_ns
    print(json.dumps({
        "audit_kind": "read-only local retained-image facts; not runtime implementation or console evidence",
        "command": "python3 outputs/2026-10-07-l18-xom-audit/cache-design/inspect_inputs.py",
        "image": {"path": str(IMAGE.relative_to(ROOT)), "bytes": n, "sha256": digest.hexdigest(), "pinned_sha_matches": True, "host_stream_hash_elapsed_ms": elapsed, "timing_scope": "host only; no console timing inference", "stable_stat_during_audit": True},
        "elf": {"type": etype, "phoff": phoff, "phentsize": phsize, "phnum": phnum, "pt_loads": loads, "all_program_headers": headers, "extent": max(p["vaddr"] + p["memsz"] for p in loads)},
        "functions": functions, "staged_signatures": scans, "inputs": inputs
    }, indent=2))


if __name__ == "__main__":
    main()
