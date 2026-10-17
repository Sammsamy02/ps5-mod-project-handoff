#!/usr/bin/env python3
"""av_formats.py -- read-only header parsers for sound and video mod files (Mac-only).

Team H prototype (sound mods + video/cutscene replacement). Independent Python
implementation of PUBLIC format facts only:
  * FMOD Studio .bank  = RIFF form 'FEV ' with chunks; sample data sits in an
    'SND ' chunk that carries an FSB5 file (public facts as used by vgmstream
    fsb5.c / fsb5_fev.c and python-fsb5; no FMOD SDK material).
  * FSB5 header + per-sample 64-bit mode word + extra chunks (same sources).
  * Bink 2 (.bk2, 'KB2'+revision) and Bink 1 (.bik, 'BIK'+revision) fixed
    header (public facts as used by FFmpeg libavformat/bink.c).
  * ISO-BMFF .mp4 box tree (ISO/IEC 14496-12/-15 public structure).
  * Wwise .bnk ('BKHD') and .wem (RIFF/RIFX WAVE fmt tag) -- detection only.
No Sony SDK material, no Epic source, no encoder. Nothing here writes files.
Every parser fails closed (FormatError with a stable code) on anything it
does not understand; a prefix ("head") of a file can be parsed with
complete=False and then only what lies inside the prefix is reported.
"""
from __future__ import annotations

import struct
import zlib
from typing import Dict, List, Optional


class FormatError(ValueError):
    def __init__(self, code: str, detail: str = ""):
        super().__init__("%s: %s" % (code, detail) if detail else code)
        self.code = code
        self.detail = detail


def need(ok, code: str, detail: str = "") -> None:
    if not ok:
        raise FormatError(code, detail)


def u32(d: bytes, o: int) -> int:
    need(0 <= o and o + 4 <= len(d), "truncated", "u32 at %d" % o)
    return struct.unpack_from("<I", d, o)[0]


def u64(d: bytes, o: int) -> int:
    need(0 <= o and o + 8 <= len(d), "truncated", "u64 at %d" % o)
    return struct.unpack_from("<Q", d, o)[0]


# ---------------------------------------------------------------------------
# FSB5
# ---------------------------------------------------------------------------

# FMOD_SOUND_FORMAT numbering as carried in the FSB5 'mode' field (public).
FSB5_CODECS = {
    0: "NONE", 1: "PCM8", 2: "PCM16", 3: "PCM24", 4: "PCM32", 5: "PCMFLOAT",
    6: "GCADPCM", 7: "IMAADPCM", 8: "VAG", 9: "HEVAG", 10: "XMA", 11: "MPEG",
    12: "CELT", 13: "AT9", 14: "XWMA", 15: "VORBIS", 16: "FADPCM", 17: "OPUS",
}
FSB5_RATES = {0: 4000, 1: 8000, 2: 11000, 3: 11025, 4: 16000, 5: 22050, 6: 24000,
              7: 32000, 8: 44100, 9: 48000, 10: 96000}
FSB5_CHANNELS = {0: 1, 1: 2, 2: 6, 3: 8}
FSB5_EXTRA = {1: "channels", 2: "frequency", 3: "loop", 4: "comment", 6: "xma_seek",
              7: "dsp_coeff", 9: "atrac9_config", 10: "xwma_config", 11: "vorbis_setup",
              13: "peak_volume", 14: "vorbis_layers", 15: "opus_data_size"}
MAX_SAMPLES = 1_000_000


def parse_fsb5(d: bytes, at: int = 0, complete: bool = True) -> dict:
    """Parse an FSB5 file starting at offset `at` of `d`.

    complete=True: the whole FSB must lie inside d (mod files).
    complete=False: d may stop early (a fetched head); sample headers are
    parsed only as far as they are present."""
    need(d[at:at + 4] == b"FSB5", "fsb5_magic", "no 'FSB5' at %d (encrypted or not an FSB5)" % at)
    ver, nsamp, shsize, ntsize, dsize, mode = struct.unpack_from("<6I", d, at + 4) \
        if at + 28 <= len(d) else (None,) * 6
    need(ver is not None, "truncated", "FSB5 header")
    need(ver in (0, 1), "fsb5_version", str(ver))
    base = 0x40 if ver == 0 else 0x3C
    need(at + base <= len(d), "truncated", "FSB5 base header")
    need(0 < nsamp <= MAX_SAMPLES, "fsb5_sample_count", str(nsamp))
    need(mode in FSB5_CODECS, "fsb5_codec_unknown", str(mode))
    total = base + shsize + ntsize + dsize
    out = {"version": ver, "samples": nsamp, "sample_header_bytes": shsize,
           "name_table_bytes": ntsize, "data_bytes": dsize, "codec_id": mode,
           "codec": FSB5_CODECS[mode], "header_bytes": base, "total_bytes": total,
           "flags": u32(d, at + 0x20) if ver == 1 else None}
    if complete:
        need(at + total <= len(d), "fsb5_truncated", "declares %d bytes" % total)
    sh_end = at + base + shsize
    p = at + base
    rows: List[dict] = []
    vorbis_crcs = set()
    parsed_all = True
    for i in range(nsamp):
        if p + 8 > min(sh_end, len(d)):
            parsed_all = False
            break
        m = u64(d, p)
        p += 8
        extra = m & 1
        row = {"rate": FSB5_RATES.get((m >> 1) & 0xF), "channels": FSB5_CHANNELS[(m >> 5) & 3],
               "data_offset": ((m >> 7) & 0x07FFFFFF) << 5, "frames": (m >> 34) & 0x3FFFFFFF,
               "extras": []}
        need(row["rate"] is not None, "fsb5_rate_code", "sample %d" % i)
        need(row["data_offset"] <= dsize, "fsb5_data_offset", "sample %d" % i)
        while extra:
            if p + 4 > min(sh_end, len(d)):
                parsed_all = False
                break
            h = u32(d, p)
            p += 4
            extra, size, kind = h & 1, (h >> 1) & 0xFFFFFF, (h >> 25) & 0x7F
            need(p + size <= sh_end, "fsb5_extra_overflow", "sample %d" % i)
            if p + size > len(d):
                parsed_all = False
                break
            name = FSB5_EXTRA.get(kind, "type%d" % kind)
            row["extras"].append(name)
            if kind == 1 and size >= 1:
                row["channels"] = d[p]
            elif kind == 2 and size >= 4:
                row["rate"] = u32(d, p)
            elif kind == 11 and size >= 4:
                row["vorbis_setup_crc32"] = u32(d, p)
                vorbis_crcs.add(row["vorbis_setup_crc32"])
            p += size
        if not parsed_all:
            break
        rows.append(row)
    if complete:
        need(parsed_all and len(rows) == nsamp, "fsb5_sample_headers", "%d of %d" % (len(rows), nsamp))
        need(p <= sh_end, "fsb5_sample_header_overflow")
        offs = [r["data_offset"] for r in rows]
        need(offs == sorted(offs), "fsb5_data_offsets_not_ordered")
    out["sample_rows_parsed"] = len(rows)
    out["sample_rows_complete"] = len(rows) == nsamp
    out["rates"] = sorted({r["rate"] for r in rows})
    out["channels"] = sorted({r["channels"] for r in rows})
    out["vorbis_setup_crc32"] = sorted(vorbis_crcs)
    if mode == 15 and complete:
        need(all("vorbis_setup_crc32" in r for r in rows), "fsb5_vorbis_setup_missing")
    return out


# ---------------------------------------------------------------------------
# FMOD Studio bank (RIFF 'FEV ')
# ---------------------------------------------------------------------------

def _walk_riff(d: bytes, start: int, end: int, pad: bool, complete: bool, have):
    chunks = []
    p = start
    while p < end:
        if not have(p, 8):
            need(not complete, "riff_truncated", "chunk header at %d" % p)
            return chunks, p, False
        cid = d[p:p + 4]
        need(all(32 <= c < 127 for c in cid), "riff_chunk_id", repr(cid))
        size = u32(d, p + 4)
        nxt = p + 8 + size + (size & 1 if pad else 0)
        need(nxt <= end, "riff_chunk_overflow", "%s at %d" % (cid.decode(), p))
        chunks.append({"id": cid.decode("ascii"), "offset": p, "size": size})
        p = nxt
    return chunks, p, p == end


def parse_fmod_bank(d: bytes, file_size: Optional[int] = None, complete: bool = True, have=None) -> dict:
    """Parse an FMOD Studio bank (or pieces of it when complete=False).

    Returns RIFF facts: format version (the 'FMT ' chunk), top-level chunk
    list, every 'SND ' chunk and the FSB5 inside it (when present in d).
    have(offset, length) -> bool tells which bytes of d are real (a sparse
    view built from fetched pieces); default: everything inside d.
    FMOD's chunk padding is not documented, so both RIFF walking modes
    (word-padded and unpadded) are tried: a complete file must have exactly
    one walk that ends on the RIFF end; a partial view reports every
    candidate position where a walk stopped (the fetch plan reads them all)."""
    if have is None:
        have = lambda o, n: 0 <= o and o + n <= len(d)  # noqa: E731
    need(len(d) >= 12, "truncated", "bank header")
    need(d[:4] == b"RIFF", "bank_riff_magic", repr(d[:4]))
    need(d[8:12] == b"FEV ", "bank_form", repr(d[8:12]))
    riff = u32(d, 4)
    end = 8 + riff
    if file_size is not None:
        need(end == file_size, "bank_size_mismatch", "RIFF says %d, file is %d" % (end, file_size))
    if complete:
        need(end == len(d), "bank_size_mismatch", "RIFF says %d, have %d" % (end, len(d)))
    if complete:
        walks = []
        for pad in (False, True):
            try:
                chunks, stop, exact = _walk_riff(d, 12, end, pad, True, have)
            except FormatError:
                continue
            if exact:
                walks.append((pad, chunks, stop))
        need(walks, "bank_chunk_walk", "no chunk walk ends on the RIFF end")
        if len(walks) == 2 and walks[0][1] != walks[1][1]:
            raise FormatError("bank_chunk_walk_ambiguous")
        pad, chunks, stop = walks[0]
        out = _bank_body(d, pad, chunks, stop, end, True, have)  # errors propagate
        if len(walks) == 2:
            out["padded_chunks"] = None  # no odd-sized chunk: both walks agree
        return out
    results, errors = [], []
    for pad in (False, True):
        try:
            chunks, stop, exact = _walk_riff(d, 12, end, pad, False, have)
            results.append(_bank_body(d, pad, chunks, stop, end, False, have))
        except FormatError as e:
            errors.append(e)
    need(results, errors[0].code if errors else "bank_chunk_walk", errors[0].detail if errors else "")
    found = [r for r in results if any(x["fsb"] for x in r["snd"])]
    if len(found) == 2 and found[0]["snd"][0]["fsb_offset"] != found[1]["snd"][0]["fsb_offset"]:
        raise FormatError("bank_chunk_walk_ambiguous")
    out = (found or results)[0]
    # where to read next: chunk headers a walk could not see, and SND payloads
    # whose header was seen but whose FSB5 bytes were not fetched
    out["walk_stop_candidates"] = sorted({r["walk_stopped_at"] for r in results
                                          if r["walk_stopped_at"] is not None})
    out["snd_payloads_missing"] = sorted({x["payload"] for r in results for x in r["snd"] if x["fsb"] is None})
    return out


def _bank_body(d, pad, chunks, stop, end, complete, have) -> dict:
    need(chunks and chunks[0]["id"] == "FMT ", "bank_fmt_first", chunks[0]["id"] if chunks else "none")
    fmt = chunks[0]
    need(fmt["size"] >= 4 and have(fmt["offset"] + 8, min(fmt["size"], 8)), "bank_fmt_size")
    version = u32(d, fmt["offset"] + 8)
    sub = u32(d, fmt["offset"] + 12) if fmt["size"] >= 8 else None
    snds = []
    for c in chunks:
        if c["id"] != "SND ":
            continue
        payload = c["offset"] + 8
        rec = {"offset": c["offset"], "size": c["size"], "fsb_offset": None, "fsb": None,
               "payload": payload}
        if have(payload, min(c["size"], 64)):
            win = d[payload:payload + min(c["size"], 64)]
            k = win.find(b"FSB5")
            need(k >= 0, "bank_snd_no_fsb5", "SND at %d (encrypted bank?)" % c["offset"])
            need(win[:k] == bytes(k), "bank_snd_padding", "non-zero bytes before FSB5")
            rec["fsb_offset"] = payload + k
            rec["fsb"] = parse_fsb5(d, payload + k, complete=complete)
            if complete:
                need(rec["fsb"]["total_bytes"] <= c["size"] - k, "bank_fsb_exceeds_snd")
        snds.append(rec)
    if complete:
        need(len(snds) <= 16, "bank_too_many_snd")
    return {"kind": "fmod_bank", "riff_end": end, "fmt_version": version, "fmt_subversion": sub,
            "padded_chunks": pad, "chunks": [(c["id"], c["offset"], c["size"]) for c in chunks],
            "snd": snds, "complete": complete, "walk_stopped_at": stop if stop < end else None,
            "metadata_end": min([x["offset"] for x in snds] or [stop])}


def guid_encodings(text: str) -> Dict[str, bytes]:
    """Byte encodings of a '{8-4-4-4-12}' GUID string that a binary file may use.

    'fields' = Data1 u32 LE, Data2 u16 LE, Data3 u16 LE, Data4 8 bytes (the
    common struct layout); 'ue_words' = four u32 LE words A,B,C,D read from the
    hex groups as UE prints FGuid; 'raw' = the 16 bytes in printed order."""
    h = text.strip().strip("{}").replace("-", "")
    need(len(h) == 32 and all(c in "0123456789abcdefABCDEF" for c in h), "guid_text", text)
    raw = bytes.fromhex(h)
    a, b, c, dd = (int(h[0:8], 16), int(h[8:16], 16), int(h[16:24], 16), int(h[24:32], 16))
    return {
        "fields": struct.pack("<IHH", int(h[0:8], 16), int(h[8:12], 16), int(h[12:16], 16)) + raw[8:],
        "ue_words": struct.pack("<4I", a, b, c, dd),
        "raw": raw,
    }


# ---------------------------------------------------------------------------
# Bink
# ---------------------------------------------------------------------------

def parse_bink(d: bytes, file_size: Optional[int] = None) -> dict:
    """Bink 1/2 fixed header (first 0x2C bytes) -- FFmpeg bink.c field order."""
    need(len(d) >= 0x2C, "truncated", "Bink header")
    sig = d[:3]
    need(sig in (b"KB2", b"BIK"), "bink_magic", repr(d[:4]))
    need(0x61 <= d[3] <= 0x7A, "bink_revision", repr(d[3:4]))
    rev = chr(d[3])
    size_field, frames, largest, _unk, w, h, fn, fd, vflags, tracks = struct.unpack_from("<10I", d, 4)
    total = size_field + 8
    if file_size is not None:
        need(total == file_size, "bink_size_mismatch", "header %d, file %d" % (total, file_size))
    need(0 < frames <= 1_000_000, "bink_frames", str(frames))
    need(largest <= total, "bink_largest_frame")
    need(0 < w <= 8192 and 0 < h <= 8192, "bink_dimensions", "%dx%d" % (w, h))
    need(fn > 0 and fd > 0, "bink_fps")
    need(tracks <= 256, "bink_audio_tracks", str(tracks))
    out = {"kind": "bink2" if sig == b"KB2" else "bink1", "magic": d[:4].decode("ascii"),
           "revision": rev, "file_bytes": total, "frames": frames, "largest_frame": largest,
           "width": w, "height": h, "fps": round(fn / fd, 3), "fps_num": fn, "fps_den": fd,
           "seconds": frames * fd / fn,
           "video_flags": vflags, "audio_tracks": tracks, "audio_rates": None}
    # Audio track table: only for revisions whose layout FFmpeg documents.
    skip = 4 if (sig == b"KB2" and rev in "ijk") or (sig == b"BIK" and rev == "k") else \
        (0 if (sig == b"KB2" and rev in "afgh") or (sig == b"BIK" and rev in "bfghi") else None)
    if tracks and skip is not None:
        p = 0x2C + skip + 4 * tracks
        if p + 4 * tracks <= len(d):
            out["audio_rates"] = [struct.unpack_from("<H", d, p + 4 * i)[0] for i in range(tracks)]
    return out


# ---------------------------------------------------------------------------
# ISO-BMFF (.mp4)
# ---------------------------------------------------------------------------

CONTAINER_BOXES = {b"moov", b"trak", b"mdia", b"minf", b"stbl", b"edts", b"dinf", b"udta"}


def _boxes(d: bytes, start: int, end: int, depth: int, complete: bool):
    need(depth < 12, "mp4_too_deep")
    p = start
    out = []
    while p + 8 <= end:
        size = struct.unpack_from(">I", d, p)[0]
        typ = d[p + 4:p + 8]
        hdr = 8
        if size == 1:
            need(p + 16 <= len(d), "truncated", "largesize box")
            size = struct.unpack_from(">Q", d, p + 8)[0]
            hdr = 16
        elif size == 0:
            size = end - p
        need(size >= hdr, "mp4_box_size", repr(typ))
        need(all(32 <= c < 127 for c in typ), "mp4_box_type", repr(typ))
        if complete:
            need(p + size <= end, "mp4_box_overflow", repr(typ))
        out.append((typ, p, size, hdr))
        p += size
    return out


def _sample_entry(d: bytes, p: int, end: int, handler: str) -> dict:
    fourcc = d[p + 4:p + 8].decode("ascii", "replace")
    e = {"fourcc": fourcc}
    body = p + 8 + 8  # reserved(6) + data_reference_index(2)
    if handler == "vide" and body + 70 <= end:
        e["width"], e["height"] = struct.unpack_from(">HH", d, body + 16)
        sub = body + 70
    elif handler == "soun" and body + 20 <= end:
        e["channels"], e["sample_size"] = struct.unpack_from(">HH", d, body + 8)
        e["sample_rate"] = struct.unpack_from(">I", d, body + 16)[0] >> 16
        sub = body + 20
    else:
        return e
    for typ, bp, bs, hdr in _boxes(d, sub, end, 8, False):
        c = bp + hdr
        if typ == b"avcC" and c + 4 <= len(d):
            e["profile_idc"], e["level_idc"] = d[c + 1], d[c + 3]
        elif typ == b"hvcC" and c + 13 <= len(d):
            e["profile_idc"], e["level_idc"] = d[c + 1] & 0x1F, d[c + 12]
            e["tier"] = (d[c + 1] >> 5) & 1
        elif typ == b"esds" and c + 4 <= len(d):
            blob = d[c + 4:bp + bs]
            k = blob.find(b"\x04")
            if k >= 0 and k + 3 < len(blob):
                # DecoderConfigDescriptor: tag 4, size (1-4 bytes), objectTypeIndication
                q = k + 1
                while q < len(blob) and blob[q] & 0x80:
                    q += 1
                q += 1
                if q < len(blob):
                    e["object_type"] = blob[q]
                k5 = blob.find(b"\x05", q)
                if k5 >= 0:
                    q5 = k5 + 1
                    while q5 < len(blob) and blob[q5] & 0x80:
                        q5 += 1
                    q5 += 1
                    if q5 < len(blob):
                        e["aac_aot"] = blob[q5] >> 3
        elif typ == b"dOps" and c + 11 <= len(d):
            e["opus_channels"] = d[c + 1]
    return e


def parse_mp4(d: bytes, file_size: Optional[int] = None, complete: bool = True) -> dict:
    end = len(d) if complete else (file_size or len(d))
    top = _boxes(d, 0, min(end, len(d)) if not complete else end, 0, complete)
    need(top and top[0][0] == b"ftyp", "mp4_ftyp_first")
    ft = top[0]
    brand = d[ft[1] + 8:ft[1] + 12].decode("ascii", "replace")
    out = {"kind": "mp4", "brand": brand, "top": [(t.decode("ascii", "replace"), p, s) for t, p, s, _ in top],
           "tracks": [], "moov_offset": None, "faststart": None}
    order = [t for t, _, _, _ in top]
    moov = [b for b in top if b[0] == b"moov"]
    if moov:
        out["moov_offset"] = moov[0][1]
        out["faststart"] = b"mdat" not in order or order.index(b"moov") < order.index(b"mdat")
        mt, mp, ms, mh = moov[0]
        need(mp + ms <= len(d), "mp4_moov_truncated")
        for t, p, s, h in _boxes(d, mp + mh, mp + ms, 1, True):
            if t != b"trak":
                continue
            out["tracks"].append(_track(d, p + h, p + s))
        need(out["tracks"], "mp4_no_tracks")
    elif complete:
        raise FormatError("mp4_no_moov")
    else:
        mdat = [b for b in top if b[0] == b"mdat"]
        if mdat:
            out["moov_offset_hint"] = mdat[0][1] + mdat[0][2]
    return out


def _be32(d: bytes, o: int, end: int) -> int:
    need(0 <= o and o + 4 <= min(end, len(d)), "mp4_truncated_field", "at %d" % o)
    return struct.unpack_from(">I", d, o)[0]


def _track(d: bytes, start: int, end: int) -> dict:
    tr = {"handler": None, "entries": []}

    def walk(s, e, depth):
        for t, p, sz, h in _boxes(d, s, e, depth, True):
            if t == b"hdlr":
                need(p + h + 12 <= p + sz, "mp4_hdlr_short")
                tr["handler"] = d[p + h + 8:p + h + 12].decode("ascii", "replace")
            elif t == b"stsd":
                n = _be32(d, p + h + 4, p + sz)
                need(1 <= n <= 16, "mp4_stsd_count", str(n))
                q = p + h + 8
                for _ in range(n):
                    es = _be32(d, q, p + sz)
                    need(es >= 16 and q + es <= p + sz, "mp4_sample_entry_size")
                    tr["entries"].append(_sample_entry(d, q, q + es, tr["handler"] or ""))
                    q += es
            elif t in CONTAINER_BOXES:
                walk(p + h, p + sz, depth + 1)
    walk(start, end, 2)
    need(tr["handler"] is not None and tr["entries"], "mp4_track_incomplete")
    return tr


def parse_moov(box: bytes) -> List[dict]:
    """Tracks of one standalone 'moov' box (bytes start at its size field)."""
    need(len(box) >= 8 and box[4:8] == b"moov", "mp4_moov_magic")
    size = struct.unpack_from(">I", box, 0)[0]
    need(size == len(box), "mp4_moov_size", "%d vs %d" % (size, len(box)))
    tracks = [_track(box, p + h, p + s) for t, p, s, h in _boxes(box, 8, size, 1, True) if t == b"trak"]
    need(tracks, "mp4_no_tracks")
    return tracks


def mp4_top_level(d: bytes, file_size: int, have) -> dict:
    """Top-level boxes of a sparse view: stops at the first header not fetched."""
    p, boxes = 0, []
    while p < file_size:
        if not have(p, 8):
            return {"boxes": boxes, "stopped_at": p}
        size = struct.unpack_from(">I", d, p)[0]
        typ = d[p + 4:p + 8]
        hdr = 8
        if size == 1:
            if not have(p, 16):
                return {"boxes": boxes, "stopped_at": p}
            size, hdr = struct.unpack_from(">Q", d, p + 8)[0], 16
        elif size == 0:
            size = file_size - p
        need(size >= hdr and p + size <= file_size, "mp4_box_size", repr(typ))
        need(all(32 <= c < 127 for c in typ), "mp4_box_type", repr(typ))
        boxes.append((typ.decode("ascii"), p, size))
        p += size
    return {"boxes": boxes, "stopped_at": None}


# ---------------------------------------------------------------------------
# Wwise (detection only) and generic sniffing
# ---------------------------------------------------------------------------

WEM_CODECS = {0x0001: "PCM", 0xFFFE: "PCM(ext)", 0x0002: "ADPCM", 0x0069: "IMA-ADPCM",
              0xFFFF: "VORBIS", 0x3040: "OPUS", 0x3041: "OPUS_WEM", 0x0166: "XMA2",
              0x0165: "XMA", 0x0162: "WMAPRO", 0xFFFC: "PTADPCM", 0x3039: "OPUSNX"}


def parse_wwise(d: bytes) -> dict:
    if d[:4] == b"BKHD":
        need(len(d) >= 16, "truncated", "BKHD")
        return {"kind": "wwise_bank", "bank_version": u32(d, 8), "bank_id": u32(d, 12)}
    need(d[:4] in (b"RIFF", b"RIFX") and d[8:12] == b"WAVE", "wem_magic")
    p = 12
    big = d[:4] == b"RIFX"
    while p + 8 <= len(d):
        cid = d[p:p + 4]
        size = struct.unpack_from(">I" if big else "<I", d, p + 4)[0]
        if cid == b"fmt " and p + 10 <= len(d):
            tag = struct.unpack_from(">H" if big else "<H", d, p + 8)[0]
            return {"kind": "wwise_wem", "fmt_tag": tag, "codec": WEM_CODECS.get(tag, "0x%04x" % tag),
                    "big_endian": big}
        p += 8 + size + (size & 1)
    raise FormatError("wem_no_fmt")


def sniff(d: bytes) -> str:
    """Best-effort kind from the first bytes (used to classify mod files)."""
    if d[:4] == b"RIFF" and d[8:12] == b"FEV ":
        return "fmod_bank"
    if d[:4] == b"FSB5":
        return "fsb5"
    if d[:3] == b"KB2":
        return "bink2"
    if d[:3] == b"BIK":
        return "bink1"
    if d[4:8] == b"ftyp":
        return "mp4"
    if d[:4] == b"BKHD":
        return "wwise_bank"
    if d[:4] in (b"RIFF", b"RIFX") and d[8:12] == b"WAVE":
        return "wav_or_wem"
    if d[:4] == b"OggS":
        return "ogg"
    if d[:4] == b"-==-" or d[:16] == b"-==--==--==--==-":
        return "utoc"
    return "unknown"


def crc32(b: bytes) -> int:
    return zlib.crc32(b) & 0xFFFFFFFF
