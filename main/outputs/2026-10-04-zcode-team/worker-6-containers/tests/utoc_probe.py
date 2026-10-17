#!/usr/bin/env python3
"""Read-only IoStore TOC header probe + pinned retoc verify over saved utocs.

Header parse is detection only (magic/version/header size/entry count from the
documented FIoStoreTocResource layout). `retoc verify` is the pinned official
Darwin release v0.1.5 already used by container_repacking; running it is
read-only validation. No unpack/pack rerun (existing receipt already proved the
byte roundtrip; we do not duplicate large scratch).
"""
import hashlib, json, os, struct, subprocess, datetime

WS = "/WORKSPACE/.codex/worktrees/translation-core/what"
OUT = os.path.join(WS, "outputs/2026-10-04-zcode-team/worker-6-containers")
RETOC_DIR = os.path.join(WS, ".superpowers/native-dependency-spike/retoc-v0.1.5-raw-roundtrip-1/tool/retoc_cli-aarch64-apple-darwin")
MAGIC = 0xCAECA5A9

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

def probe(path):
    r = {"target": path.replace(WS + "/", ""), "bytes": os.path.getsize(path)}
    with open(path, "rb") as f:
        head = f.read(64)
    magic, ver, hsize, ecount = struct.unpack_from("<IIII", head, 0)
    r["magic_hex"] = hex(magic)
    if magic != MAGIC:
        r["note"] = "unexpected magic; remaining fields unreliable"
        return r
    r["toc_version"] = ver
    r["header_size"] = hsize
    r["entry_count"] = ecount
    # documented remaining header u32s (names per retoc/UE source):
    # 4 compressedBlockEntryCount? fields vary by version; report raw words
    r["header_u32_words"] = [struct.unpack_from("<I", head, 4 + 4 * i)[0] for i in range(1, 8)]
    r["file_sha256"] = sha256(path)
    return r

def retoc_verify():
    exe = None
    for root, _, files in os.walk(RETOC_DIR):
        for fn in files:
            if fn in ("retoc_cli", "retoc") or (fn.startswith("retoc") and not fn.endswith((".json", ".txt"))):
                p = os.path.join(root, fn)
                if os.access(p, os.X_OK):
                    exe = exe or p
    r = {"exe": exe.replace(WS + "/", "") if exe else None}
    if not exe:
        r["error"] = "no executable retoc binary found"
        return r
    targets = [
        ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc",
        ".superpowers/native-observations/townfall-manifest-capture-2026-10-01/global.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk1-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk10-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk11-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk12-ps5.utoc",
    ]
    runs = []
    for t in targets:
        p = os.path.join(WS, t)
        if not os.path.exists(p):
            runs.append({"target": t, "present": False}); continue
        try:
            cp = subprocess.run([exe, "verify", p], capture_output=True, text=True, timeout=180)
            runs.append({"target": t, "exit": cp.returncode,
                         "stdout_tail": cp.stdout.strip()[-400:],
                         "stderr_tail": cp.stderr.strip()[-400:]})
        except Exception as e:
            runs.append({"target": t, "error": f"{type(e).__name__}: {e}"})
    r["verify_runs"] = runs
    return r

def main():
    targets = [
        ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc",
        ".superpowers/native-observations/townfall-manifest-capture-2026-10-01/global.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk1-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk10-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk11-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk12-ps5.utoc",
        ".superpowers/native-observations/townfall-global-scriptobjects-1/global.ucas",
    ]
    results = []
    for t in targets:
        p = os.path.join(WS, t)
        if t.endswith(".ucas"):
            results.append({"target": t, "bytes": os.path.getsize(p), "file_sha256": sha256(p), "note": "ucas container, header probe applies to utoc side"})
        else:
            results.append(probe(p) if os.path.exists(p) else {"target": t, "present": False})
    receipt = {
        "utc": datetime.datetime.utcnow().isoformat() + "Z",
        "scope": "read-only utoc header detection + pinned retoc v0.1.5 verify; no unpack/pack rerun; in-game mount NOT tested here",
        "results": results,
        "retoc_verify": retoc_verify(),
    }
    with open(os.path.join(OUT, "evidence/utoc-retoc-verify.json"), "w") as f:
        json.dump(receipt, f, indent=1)
    print(json.dumps(receipt, indent=1)[:4000])

if __name__ == "__main__":
    main()
