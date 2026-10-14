#!/usr/bin/env python3
"""Read-only UE PAK footer/index-header probe over saved original containers.

Detection only: magic, legacy magic, version, index offset/size, mount point,
declared entry count, footer compression-method table. No bytes are rewritten;
nothing here converts or repacks. If the index is encrypted or the layout is
unexpected, that is recorded as a detection result, not an error to paper over.
"""
import hashlib, json, os, struct, datetime

WS = "/WORKSPACE/.codex/worktrees/translation-core/what"
OUT = os.path.join(WS, "outputs/2026-10-04-zcode-team/worker-6-containers")
MAGIC = 0x5A6F12E1
MAGIC_LEGACY = 0x5A6F12E0

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

def probe(path):
    r = {"target": path.replace(WS + "/", ""), "bytes": os.path.getsize(path)}
    with open(path, "rb") as f:
        f.seek(-221, 2)  # worst-case footer window
        tail = f.read()
        r["file_sha256"] = sha256(path)
    # find footer magic scanning backwards
    pos = None; magic_found = None
    for m, name in ((MAGIC, "0x5A6F12E1"), (MAGIC_LEGACY, "0x5A6F12E0")):
        b = struct.pack("<I", m)
        i = tail.rfind(b)
        if i != -1:
            pos, magic_found = i, name
            break
    if pos is None:
        r["footer"] = "no pak magic in last 221 bytes"
        return r
    r["magic"] = magic_found
    v, off, size = struct.unpack_from("<IQQ", tail, pos + 4)
    r["version"] = v
    r["index_offset"], r["index_size"] = off, size
    r["index_hash_sha1"] = tail[pos + 24: pos + 44].hex()
    r["footer_size"] = len(tail) - pos
    if v >= 9 and r["footer_size"] >= 53:
        r["frozen_index"] = tail[pos + 44]
        names = []
        for i in range(5):
            s = tail[pos + 45 + 32 * i: pos + 45 + 32 * (i + 1)].split(b"\0")[0]
            names.append(s.decode("latin1"))
        r["footer_compression_methods"] = names
    # primary index header: mount point string + declared entry count
    try:
        with open(path, "rb") as f:
            f.seek(off)
            head = f.read(min(size, 4096))
        n = struct.unpack_from("<i", head, 0)[0]
        mount = head[4:4 + max(n - 1, 0)] if n > 0 else b""
        r["mount_point"] = mount.decode("latin1", "replace")
        if n >= 0 and 4 + n + 4 <= len(head):
            r["declared_entry_count"] = struct.unpack_from("<I", head, 4 + n)[0]
        r["primary_index_readable"] = n >= 0 and n < 4096
    except Exception as e:
        r["primary_index_readable"] = False
        r["primary_index_error"] = f"{type(e).__name__}: {e}"
    return r

def main():
    targets = [
        ".superpowers/native-observations/townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak",
        ".superpowers/native-observations/townfall-extra-pak-test-2026-10-01/pakchunk9999-ModBridgeOriginals_6ec975d1-61fc-43dc-9ca9-3475ed94d6d8_P.pak",
        ".superpowers/native-observations/townfall-extra-pak-test-2026-10-01/diagnostic-render-control/pakchunk9999-ModBridgeRenderDiag_a6d6f620-2878-494b-a0cb-45e4486fdb44_P.pak",
        ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.pak",
        ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/base-pak-metadata/townfall-pakchunk0-metadata-only.pak",
        ".superpowers/native-dependency-spike/unreal-config-pak-composition-1/trial-1/derived.pak",
    ]
    results = [probe(os.path.join(WS, t)) if os.path.exists(os.path.join(WS, t))
               else {"target": t, "present": False} for t in targets]
    receipt = {
        "utc": datetime.datetime.utcnow().isoformat() + "Z",
        "scope": "read-only PAK footer + primary-index header detection; no rewrites; console-mount and in-game acceptance NOT tested here",
        "results": results,
    }
    with open(os.path.join(OUT, "evidence/pak-probe.json"), "w") as f:
        json.dump(receipt, f, indent=1)
    for x in results:
        print(json.dumps(x))

if __name__ == "__main__":
    main()
