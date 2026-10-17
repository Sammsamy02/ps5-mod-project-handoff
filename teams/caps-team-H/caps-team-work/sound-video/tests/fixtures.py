"""SYNTHETIC test fixtures built from the public format descriptions only.

These bytes are NOT real FMOD banks / Bink movies / game files: they carry
just the header fields the parsers read (sample data is filler).  They exist
only in temporary test folders and must never be uploaded anywhere.
"""
import os
import struct

RATE_CODE = {44100: 8, 48000: 9, 24000: 6, 32000: 7}
CH_CODE = {1: 0, 2: 1, 6: 2, 8: 3}
CODEC_ID = {"PCM16": 2, "XMA": 10, "AT9": 13, "VORBIS": 15, "FADPCM": 16, "OPUS": 17}


def fsb5(codec="VORBIS", samples=((48000, 2, 4800), (44100, 1, 1000)), version=1, vorbis_crc=0x12345678,
         data_per_sample=64):
    sh = b""
    off = 0
    for rate, ch, frames in samples:
        has_extra = codec == "VORBIS"
        mode = (1 if has_extra else 0) | (RATE_CODE[rate] << 1) | (CH_CODE[ch] << 5) | \
               ((off >> 5) << 7) | (frames << 34)
        sh += struct.pack("<Q", mode)
        if has_extra:
            payload = struct.pack("<I", vorbis_crc) + bytes(8)
            sh += struct.pack("<I", 0 | (len(payload) << 1) | (11 << 25)) + payload
        off += data_per_sample
    data = bytes(range(256)) * (off // 256 + 1)
    data = data[:off]
    hdr = b"FSB5" + struct.pack("<6I", version, len(samples), len(sh), 0, len(data), CODEC_ID[codec])
    if version == 1:
        hdr += struct.pack("<II", 0, 0) + bytes(16) + bytes(8)
    else:
        hdr += bytes(0x40 - len(hdr))
    return hdr + sh + data


def guid_fields(text):
    h = text.strip("{}").replace("-", "")
    return struct.pack("<IHH", int(h[0:8], 16), int(h[8:12], 16), int(h[12:16], 16)) + bytes.fromhex(h[16:])


def chunk(cid, payload, pad=True):
    out = cid + struct.pack("<I", len(payload)) + payload
    if pad and len(payload) & 1:
        out += b"\0"
    return out


def bank(fmt_version=140, guid="{A98BC465-070F-4740-AD12-02754A36B8EC}", fsb=None, metadata_bytes=40,
         odd_chunk=False, pad=True, snd=True):
    fmt = chunk(b"FMT ", struct.pack("<II", fmt_version, 0), pad)
    bnki = chunk(b"BNKI", guid_fields(guid) + bytes(metadata_bytes), pad)
    extra = chunk(b"STRD", b"xyz", pad) if odd_chunk else b""  # odd-sized top-level chunk
    lst = chunk(b"LIST", b"PROJ" + bnki, pad)
    body = b"FEV " + fmt + lst + extra
    if snd:
        fsb = fsb if fsb is not None else fsb5()
        # FSB5 starts on a 32-byte file boundary after the SND chunk header
        at = 8 + len(body) + 8
        padn = (-at) % 32
        body += chunk(b"SND ", bytes(padn) + fsb, pad)
    return b"RIFF" + struct.pack("<I", len(body)) + body


def bink(magic=b"KB2j", width=1920, height=1080, frames=300, fps=(30, 1), tracks=0, total=4096):
    hdr = magic + struct.pack("<10I", total - 8, frames, 1000, 0, width, height, fps[0], fps[1], 0, tracks)
    if magic[:3] == b"KB2" and magic[3:4] in b"ijk":
        hdr += bytes(4)
    for _ in range(tracks):
        hdr += struct.pack("<I", 4096)
    for _ in range(tracks):
        hdr += struct.pack("<HH", 48000, 0x2000)
    for i in range(tracks):
        hdr += struct.pack("<I", i)
    return hdr + bytes(total - len(hdr))


def box(typ, payload):
    return struct.pack(">I", 8 + len(payload)) + typ + payload


def mp4(video="avc1", profile=100, level=41, width=1280, height=720, audio=True, faststart=True, mdat=4000):
    vse = bytes(6) + struct.pack(">H", 1) + bytes(16) + struct.pack(">HH", width, height) + bytes(50)
    if video == "avc1":
        vse += box(b"avcC", bytes([1, profile, 0, level, 0xFF, 0xE0]))
    else:
        vse += box(b"hvcC", bytes([1, profile & 0x1F]) + bytes(10) + bytes([level]) + bytes(10))
    vtrak = box(b"trak", box(b"mdia", box(b"hdlr", bytes(8) + b"vide" + bytes(12)) +
                              box(b"minf", box(b"stbl", box(b"stsd", bytes(4) + struct.pack(">I", 1) +
                                                            box(video.encode(), vse))))))
    traks = vtrak
    if audio:
        ase = bytes(6) + struct.pack(">H", 1) + bytes(8) + struct.pack(">HH", 2, 16) + bytes(4) + \
            struct.pack(">I", 48000 << 16)
        esds = bytes(4) + bytes([3, 25, 0, 1, 0]) + bytes([4, 17, 0x40, 0x15]) + bytes(11) + bytes([5, 2, 0x11, 0x90])
        ase += box(b"esds", esds)
        atrak = box(b"trak", box(b"mdia", box(b"hdlr", bytes(8) + b"soun" + bytes(12)) +
                                  box(b"minf", box(b"stbl", box(b"stsd", bytes(4) + struct.pack(">I", 1) +
                                                                box(b"mp4a", ase))))))
        traks += atrak
    moov = box(b"moov", box(b"mvhd", bytes(100)) + traks)
    ftyp = box(b"ftyp", b"isom" + bytes(4) + b"isomavc1")
    md = box(b"mdat", bytes(mdat))
    return ftyp + (moov + md if faststart else md + moov)


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    return path


def ls_line(kind, size, name):
    perm = ("d" if kind == "d" else "-") + "rwxrwxrwx"
    return "%s 1 0 0 %d Oct 13 12:00 %s" % (perm, size, name)


def fake_list_dir(root_local, root_remote):
    """A list_dir() that serves a local folder as if it were the console's."""
    def list_dir(path):
        rel = path[len(root_remote):].lstrip("/")
        local = os.path.join(root_local, *rel.split("/")) if rel else root_local
        lines = [ls_line("d", 65536, "."), ls_line("d", 65536, "..")]
        for n in sorted(os.listdir(local)):
            p = os.path.join(local, n)
            lines.append(ls_line("d" if os.path.isdir(p) else "-", 65536 if os.path.isdir(p) else os.path.getsize(p), n))
        return "\r\n".join(lines) + "\r\n"
    return list_dir


def simulate_range_fetch(plan, root_local, out_dir):
    """Same contract as harvest/ftp_range_fetch.py, against a local folder."""
    n = 0
    for c in plan["chunks"]:
        src = os.path.join(root_local, *c["ucas"].split("/"))
        buf = b""
        with open(src, "rb") as f:
            for r in c["physical_ranges"]:
                f.seek(r["offset"])
                buf += f.read(r["length"])
        assert len(buf) == sum(r["length"] for r in c["physical_ranges"])
        write(os.path.join(out_dir, c["save_as"]), buf)
        n += 1
    return n


GUIDS = {"Music": "{1F9E3674-213E-4E97-A368-7456306A6C1A}", "Master": "{441EE9D6-EFB4-4788-8922-3CC460330AE2}",
         "Dialogue_EN": "{D69D6A83-2096-4DD9-A094-FAA10751D0A6}"}


def profile():
    """Minimal synthetic base profile (same keys av_profile.py writes)."""
    return {
        "schema": "av-base-profile/1", "project": "Game", "platform": "PLAT",
        "audio": {
            "fmod": {"platform_bank_dirs": ["FMOD/Banks/PLAT"],
                     "pc_bank_dirs": ["FMOD/Banks/Desktop", "FMOD/Banks/PC Stereo"],
                     "default_pc_bank_dir": "FMOD/Banks/Desktop", "master_bank": "Master",
                     "banks": [{"name": n, "guid": g, "package": "/Game/FMOD/Banks/" + n} for n, g in GUIDS.items()],
                     "bank_lookup_assets": [],
                     "fsb_codecs_decodable": ["AT9", "FADPCM", "OPUS", "PCM16", "VORBIS"],
                     "vorbis_table_validated": True, "vorbis_setup_crcs": [0x12345678, 0x0BADF00D]},
            "engine": {"enabled": False}, "wwise": {"present": False}},
        "video": {"bink": {"players": ["/Game/Movies/Intro"], "player_assets": 1},
                  "electra": {"file_media_sources": [{"package": "/Game/Movies/Tv", "file_path": "./Movies/Tv.mp4"}]},
                  "media_dirs": ["Movies"]},
        "exe": {"bink_header_checks": {"bink2_accepted": ["KB2f", "KB2g", "KB2h", "KB2i", "KB2j", "KB2k"]}},
    }


def console_tree(root):
    """A synthetic stand-in for the console's content folder (lower-case)."""
    write(os.path.join(root, "fmod/banks/plat/music.bank"), bank(guid=GUIDS["Music"], fsb=fsb5("AT9", ((48000, 2, 4800),))))
    write(os.path.join(root, "fmod/banks/plat/master.bank"), bank(guid=GUIDS["Master"], fsb=fsb5("VORBIS")))
    write(os.path.join(root, "fmod/banks/plat/dialogue_en.bank"),
          bank(guid=GUIDS["Dialogue_EN"], fsb=fsb5("VORBIS"), metadata_bytes=70000))  # SND beyond the 64 KiB head
    write(os.path.join(root, "fmod/banks/plat/master.strings.bank"), bank(guid=GUIDS["Master"], snd=False))  # no samples
    write(os.path.join(root, "movies/intro.bk2"), bink(b"KB2j", total=8192))
    write(os.path.join(root, "movies/tv.mp4"), mp4(faststart=False, mdat=70000))  # moov beyond the head
    write(os.path.join(root, "paks/readme.txt"), b"not listed")
