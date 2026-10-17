#!/usr/bin/env python3
"""min_predicates.py — host mirror of the in-process layout predicates.

worker-layout-minimum / team 2026-10-04-zcode-team.

CONSUME/CONVERGE with the sibling worker-5-layout-validator contract
(src/townfall_layout_probes.hpp): same verdict vocabulary
(NotProbed/Validated/Inconclusive/Refuted), same rationale-code names
(GOA_*, CODE_*), same two GUObjectArray layout hypotheses (A: label ->
FUObjectArray outer; B: label -> TUObjectArray direct), same constants
(NumElementsPerChunk=64*1024 [UObjectArray.hpp:88], MaxChunks bound 512,
FUObjectItem size 0x18, scan bound 64 chunks).

CONVERGENCE POINT for the controller: worker-5's compiled `tlp` predicates are
the in-process authority inside the game module; this mirror runs host-side on
span dumps / synthetic images and must agree. If they ever disagree, both
results are preserved and the value stays a candidate (never accepted).

All values probed here are CANDIDATES from
../candidates/layout-candidates.profile.json (ported-default offsets,
Windows-derived; never validated until these predicates pass in-process on
target). Pure functions only: no network, no console, no allocation beyond
plain Python, reads only caller-supplied spans.
"""
from __future__ import annotations

import hashlib
import json
import struct
from typing import Dict, List, Optional, Sequence

VERDICT_NOT_PROBED = "NotProbed"
VERDICT_VALIDATED = "Validated"
VERDICT_INCONCLUSIVE = "Inconclusive"
VERDICT_REFUTED = "Refuted"

# Constants cited in candidates/layout-candidates.profile.json (C-uoa-hpp / C-set-506).
NUM_ELEMENTS_PER_CHUNK = 64 * 1024      # UObjectArray.hpp:88
MAX_CHUNKS_BOUND = 512                  # Chunks_Type = UObjectBase**[512]
FUOBJECTITEM_SIZE = 0x18                # FUObjectItem UEP_TotalSize (5_06 setter)
MAX_CHUNK_SCAN = 64                     # operational bound, not an offset
FUOBJECTARRAY_TOTAL = 0xB8              # hypothesis A header extent
TUOBJECTARRAY_TOTAL = 0x20              # hypothesis B header extent

# Candidate offsets (decimal) from candidates/layout-candidates.profile.json.
TUOFF = {  # TUObjectArray members
    "Objects": 0x0, "PreAllocatedObjects": 0x8, "MaxElements": 0x10,
    "NumElements": 0x14, "MaxChunks": 0x18, "NumChunks": 0x1C,
    "UEP_TotalSize": 0x20,
}
FAOFF = {  # FUObjectArray members (hypothesis A; TUObjectArray embedded at ObjObjects=0x10)
    "ObjFirstGCIndex": 0x0, "ObjLastNonGCIndex": 0x4,
    "MaxObjectsNotConsideredByGC": 0x8, "OpenForDisregardForGC": 0xC,
    "ObjObjects": 0x10, "UEP_TotalSize": 0xB8,
}
UIOFF = {"Object": 0x0, "Flags": 0x8, "ClusterRootIndex": 0xC,
         "SerialNumber": 0x10, "RefCount": 0x14, "UEP_TotalSize": 0x18}
UBOFF = {"ObjectFlags": 0x8, "InternalIndex_Private": 0xC, "ClassPrivate": 0x10,
         "NamePrivate": 0x18, "OuterPrivate": 0x20, "UEP_TotalSize": 0x28}


# ---------------------------------------------------------------- span access
def read_bytes(spans: Sequence[dict], addr: int, length: int) -> Optional[bytes]:
    """Return the bytes at [addr, addr+length) iff fully inside ONE span."""
    for span in spans:
        start, size = span["start"], span["size"]
        if start <= addr and addr + length <= start + size:
            data = span["bytes"]
            off = addr - start
            return bytes(data[off:off + length])
    return None


def range_readable(spans: Sequence[dict], addr: int, length: int) -> bool:
    return read_bytes(spans, addr, length) is not None


def pointer_canonical_or_null(p: int) -> bool:
    """Null OR aligned-8 canonical user-mode x86-64 pointer (bits 63..47 zero-extended)."""
    if p == 0:
        return True
    return p % 8 == 0 and p < (1 << 47)


def _u32(b: bytes) -> int:
    return struct.unpack("<I", b)[0]


def _u64(b: bytes) -> int:
    return struct.unpack("<Q", b)[0]


def span_digest(spans: Sequence[dict]) -> str:
    h = hashlib.sha256()
    for span in sorted(spans, key=lambda s: s["start"]):
        h.update(struct.pack("<QQ", span["start"], span["size"]))
        h.update(bytes(span["bytes"]))
    return h.hexdigest()


# ------------------------------------------------------------- code shape
def probe_code_shape(spans, image_base, vaddr):
    """Inconclusive-classifier ONLY (never Validated), mirrors tlp::probe_code_shape."""
    addr = image_base + vaddr
    result = {"probe": "code_shape", "verdict": VERDICT_INCONCLUSIVE,
              "codes": [], "vaddr": hex(vaddr), "address": hex(addr)}
    data = read_bytes(spans, addr, 16)
    if data is None:
        result["codes"].append("CODE_E_UNREADABLE")
        return result
    result["bytes_read"] = len(data)
    result["first_bytes"] = data.hex()
    first = data[0]
    if first in (0x55, 0x53, 0x56, 0x57, 0x41) or data[:2] in (b"\x48\x83", b"\x48\x81"):
        result["codes"].append("CODE_I_PROLOGUE_LIKE")
    else:
        result["codes"].append("CODE_I_NOT_PROLOGUE_LIKE")
    pre = read_bytes(spans, addr - 2, 2)
    if pre is not None and all(b in (0xCC, 0x90) for b in pre):
        result["codes"].append("CODE_I_PRE_PADDING")
    return result


# ------------------------------------------------------- GUObjectArray chain
def _probe_hypothesis(spans, header_addr, first_gc=None, last_non_gc=None):
    """Shared TUObjectArray checks at header_addr; returns (verdict, codes, observed)."""
    codes: List[str] = []
    obs: Dict[str, int] = {}
    hdr = read_bytes(spans, header_addr, TUOBJECTARRAY_TOTAL)
    if hdr is None:
        return VERDICT_INCONCLUSIVE, ["GOA_E_ADDR_UNREADABLE"], obs
    objects = _u64(hdr[TUOFF["Objects"]:TUOFF["Objects"] + 8])
    prealloc = _u64(hdr[TUOFF["PreAllocatedObjects"]:TUOFF["PreAllocatedObjects"] + 8])
    max_elements = _u32(hdr[TUOFF["MaxElements"]:TUOFF["MaxElements"] + 4])
    num_elements = _u32(hdr[TUOFF["NumElements"]:TUOFF["NumElements"] + 4])
    max_chunks = _u32(hdr[TUOFF["MaxChunks"]:TUOFF["MaxChunks"] + 4])
    num_chunks = _u32(hdr[TUOFF["NumChunks"]:TUOFF["NumChunks"] + 4])
    obs.update({"objects_ptr": objects, "preallocated_objects_ptr": prealloc,
                "max_elements": max_elements, "num_elements": num_elements,
                "max_chunks": max_chunks, "num_chunks": num_chunks})

    if not pointer_canonical_or_null(objects) or not pointer_canonical_or_null(prealloc):
        return VERDICT_REFUTED, ["GOA_R_PTR_NONCANONICAL"], obs
    if objects == 0 and prealloc == 0:
        return VERDICT_REFUTED, ["GOA_R_PTRS_BOTH_NULL"], obs
    if not (0 < num_elements <= max_elements):
        codes.append("GOA_R_COUNTS_INSANE")
    if not (1 <= max_chunks <= MAX_CHUNKS_BOUND):
        codes.append("GOA_R_COUNTS_INSANE")
    if num_chunks > max_chunks:
        codes.append("GOA_R_COUNTS_INSANE")
    if max_elements > max_chunks * NUM_ELEMENTS_PER_CHUNK:
        codes.append("GOA_R_COUNTS_INSANE")
    if codes:
        return VERDICT_REFUTED, sorted(set(codes)), obs

    needed_chunks = min((num_elements + NUM_ELEMENTS_PER_CHUNK - 1) // NUM_ELEMENTS_PER_CHUNK,
                        MAX_CHUNK_SCAN)

    # Preallocated mode: one contiguous item array.
    if objects == 0:
        if not range_readable(spans, prealloc, num_elements * FUOBJECTITEM_SIZE):
            return VERDICT_INCONCLUSIVE, ["GOA_E_CHUNK_EXTENT_UNVERIFIABLE"], obs
        obs.update({"layout_mode": "preallocated", "chunks_scanned": 1,
                    "chunk0_ptr": prealloc, "chunk1_ptr": 0})
        return VERDICT_VALIDATED, [], obs

    # Chunked mode: chunk-pointer array at Objects.
    if not range_readable(spans, objects, needed_chunks * 8):
        if not range_readable(spans, objects, 8):
            return VERDICT_REFUTED, ["GOA_R_CHUNK_UNREADABLE"], obs
        return VERDICT_INCONCLUSIVE, ["GOA_E_CHUNK_EXTENT_UNVERIFIABLE"], obs

    seen: List[int] = []
    windows: List[tuple] = []
    for i in range(needed_chunks):
        ptr = _u64(read_bytes(spans, objects + 8 * i, 8))
        if ptr == 0:
            return VERDICT_REFUTED, ["GOA_R_CHUNK_NULL_WHILE_NEEDED"], obs
        if not pointer_canonical_or_null(ptr) or ptr == 0:
            return VERDICT_REFUTED, ["GOA_R_PTR_NONCANONICAL"], obs
        if not any(s["start"] <= ptr for s in spans):
            return VERDICT_REFUTED, ["GOA_R_CHUNK_UNREADABLE"], obs
        if ptr in seen:
            return VERDICT_REFUTED, ["GOA_R_CHUNK_DUPLICATE"], obs
        for (optr, olen) in windows:
            if ptr < optr + olen and optr < ptr + FUOBJECTITEM_SIZE:
                return VERDICT_REFUTED, ["GOA_R_CHUNK_OVERLAP"], obs
        if not range_readable(spans, ptr, FUOBJECTITEM_SIZE):
            return VERDICT_INCONCLUSIVE, ["GOA_E_CHUNK_EXTENT_UNVERIFIABLE"], obs
        seen.append(ptr)
        windows.append((ptr, FUOBJECTITEM_SIZE))
        if i == 0:
            obs["chunk0_ptr"] = ptr
        if i == 1:
            obs["chunk1_ptr"] = ptr
    if num_elements > num_chunks * NUM_ELEMENTS_PER_CHUNK:
        return VERDICT_REFUTED, ["GOA_R_CAPACITY_MISMATCH"], obs
    obs.update({"layout_mode": "chunked", "chunks_scanned": needed_chunks})
    return VERDICT_VALIDATED, [], obs


def probe_guobject_array(spans, image_base, goa_vaddr):
    """Probe both retained hypotheses; mirrors tlp::probe_guobject_array verdicts."""
    addr = image_base + goa_vaddr
    result = {"probe": "guobject_array", "goa_vaddr": hex(goa_vaddr),
              "image_base": hex(image_base), "address": hex(addr),
              "span_digest": span_digest(spans),
              "verdict": VERDICT_NOT_PROBED, "codes": [],
              "layout_a": {}, "layout_b": {}, "observed": {}}

    # Hypothesis A: FUObjectArray outer; embedded TUObjectArray at +0x10.
    a_codes: List[str] = []
    a_obs: Dict[str, int] = {}
    hdr_a = read_bytes(spans, addr, FUOBJECTARRAY_TOTAL)
    a_verdict = VERDICT_INCONCLUSIVE
    if hdr_a is None:
        a_codes.append("GOA_E_ADDR_UNREADABLE")
    else:
        first_gc = _u32(hdr_a[FAOFF["ObjFirstGCIndex"]:FAOFF["ObjFirstGCIndex"] + 4])
        last_non_gc = _u32(hdr_a[FAOFF["ObjLastNonGCIndex"]:FAOFF["ObjLastNonGCIndex"] + 4])
        emb_max = _u32(hdr_a[FAOFF["ObjObjects"] + TUOFF["MaxElements"]:
                       FAOFF["ObjObjects"] + TUOFF["MaxElements"] + 4])
        a_verdict, a_codes, a_obs = _probe_hypothesis(
            spans, addr + FAOFF["ObjObjects"], first_gc, last_non_gc)
        a_obs["obj_first_gc_index"] = first_gc
        a_obs["obj_last_non_gc_index"] = last_non_gc
        if a_verdict == VERDICT_VALIDATED:
            if not (0 <= first_gc <= last_non_gc <= emb_max):
                a_verdict, a_codes = VERDICT_REFUTED, ["GOA_R_GC_ORDER"]

    # Hypothesis B: TUObjectArray direct at the label.
    b_verdict, b_codes, b_obs = _probe_hypothesis(spans, addr)

    result["layout_a"] = {"verdict": a_verdict, "codes": sorted(set(a_codes)),
                          "observed": {k: (hex(v) if k.endswith("_ptr") else v)
                                       for k, v in a_obs.items()}}
    result["layout_b"] = {"verdict": b_verdict, "codes": sorted(set(b_codes)),
                          "observed": {k: (hex(v) if k.endswith("_ptr") else v)
                                       for k, v in b_obs.items()}}

    a_pass = a_verdict == VERDICT_VALIDATED
    b_pass = b_verdict == VERDICT_VALIDATED
    if a_pass and b_pass:
        result["verdict"] = VERDICT_INCONCLUSIVE
        result["codes"] = ["GOA_E_LAYOUT_AMBIGUOUS"]
    elif a_pass:
        result["verdict"] = VERDICT_VALIDATED
        result["layout_matched"] = "A"
        result["observed"] = result["layout_a"]["observed"]
    elif b_pass:
        result["verdict"] = VERDICT_VALIDATED
        result["layout_matched"] = "B"
        result["observed"] = result["layout_b"]["observed"]
    else:
        # Neither validated: Refuted wins over Inconclusive only when the
        # bytes actively contradict BOTH hypotheses; otherwise inconclusive.
        any_refuted = (a_verdict == VERDICT_REFUTED and b_verdict == VERDICT_REFUTED)
        result["verdict"] = VERDICT_REFUTED if any_refuted else VERDICT_INCONCLUSIVE
        result["codes"] = sorted(set(a_codes) | set(b_codes))
        result["layout_matched"] = None
    return result


# --------------------------------------------- UObjectBase / FUObjectItem gate
def probe_uobject_item_chain(spans, gua_result, item_count=8):
    """Gate for FUObjectItem + UObjectBase candidate entries.

    Reads the first item_count items under the VALIDATED hypothesis and checks
    the UObjectBase cross-consistency: Object canonical non-null, ClassPrivate
    canonical non-null, InternalIndex_Private == position. Refutes the
    UObjectBase candidate offsets if the round trip fails.
    """
    result = {"probe": "uobject_item_chain", "verdict": VERDICT_NOT_PROBED,
              "codes": [], "items_checked": 0}
    if gua_result.get("verdict") != VERDICT_VALIDATED:
        result["codes"].append("UOB_E_NO_VALIDATED_ARRAY")
        result["verdict"] = VERDICT_INCONCLUSIVE
        return result
    obs = gua_result["observed"]
    num = int(obs.get("num_elements", 0))
    if obs.get("layout_mode") == "preallocated":
        base_items = int(obs["preallocated_objects_ptr"], 16)
        chunk_of = lambda i: base_items
    else:
        objects = int(obs["objects_ptr"], 16)
        chunk_ptrs = [int(obs.get("chunk0_ptr", "0"), 16),
                      int(obs.get("chunk1_ptr", "0"), 16)]
        def chunk_of(i):
            if i < NUM_ELEMENTS_PER_CHUNK:
                return chunk_ptrs[0] if chunk_ptrs[0] else None
            return chunk_ptrs[1] if chunk_ptrs[1] else None
    checked = 0
    for i in range(min(item_count, num)):
        chunk = chunk_of(i)
        if chunk is None:
            continue
        item_addr = chunk + (i % NUM_ELEMENTS_PER_CHUNK) * FUOBJECTITEM_SIZE
        item = read_bytes(spans, item_addr, FUOBJECTITEM_SIZE)
        if item is None:
            result["codes"].append("UOB_E_ITEM_UNREADABLE")
            continue
        obj = _u64(item[UIOFF["Object"]:UIOFF["Object"] + 8])
        # UObjectBase fields live IN THE OBJECT the item points to (candidate
        # offsets), never overlaid on the FUObjectItem itself.
        ub = read_bytes(spans, obj, UBOFF["UEP_TotalSize"]) if obj else None
        if ub is None:
            result["codes"].append("UOB_E_UOBJECT_UNREADABLE")
            continue
        cls = _u64(ub[UBOFF["ClassPrivate"]:UBOFF["ClassPrivate"] + 8])
        idx = _u32(ub[UBOFF["InternalIndex_Private"]:UBOFF["InternalIndex_Private"] + 4])
        if obj == 0:
            result["codes"].append("UOB_R_OBJECT_NULL")
        elif not pointer_canonical_or_null(obj):
            result["codes"].append("UOB_R_OBJECT_NONCANONICAL")
        if cls == 0:
            result["codes"].append("UOB_R_CLASS_NULL")
        elif not pointer_canonical_or_null(cls):
            result["codes"].append("UOB_R_CLASS_NONCANONICAL")
        if idx != i:
            result["codes"].append("UOB_R_INDEX_MISMATCH")
        checked += 1
    result["items_checked"] = checked
    codes = sorted(set(result["codes"]))
    result["codes"] = codes
    refute = [c for c in codes if c.startswith("UOB_R_")]
    if checked == 0:
        result["verdict"] = VERDICT_INCONCLUSIVE
    elif refute:
        result["verdict"] = VERDICT_REFUTED
    else:
        result["verdict"] = VERDICT_VALIDATED
    return result


# ------------------------------------------------------------------ synthetic
def make_synthetic_image(hypothesis="A", num_elements=16, corrupt=None):
    """Build a synthetic mapped image exercising the predicate chain (host tests).

    Returns (spans, image_base, goa_vaddr). `corrupt` applies mutations:
    'counts' (NumElements > Max), 'gc_order' (A only), 'ambiguous',
    'unreadable', 'null_objects'.
    """
    image_base = 0x100000
    goa_vaddr = 0x93ba110
    addr = image_base + goa_vaddr
    num = num_elements
    per_chunk = NUM_ELEMENTS_PER_CHUNK
    max_elements = per_chunk * 2
    num_chunks = 1 if num <= per_chunk else 2

    chunk0 = 0x2000000
    chunk1 = 0x4000000 if num_chunks > 1 else 0
    objects_arr = 0x3000000
    items = [0x5000000 + i * 0x40 for i in range(num)]  # UObject addresses

    def item_bytes(i):
        # FUObjectItem: the UObjectBase fields live in the OBJECT it points to.
        it = bytearray(FUOBJECTITEM_SIZE)
        struct.pack_into("<Q", it, UIOFF["Object"], items[i])
        struct.pack_into("<I", it, UIOFF["SerialNumber"], i + 1)
        return bytes(it)

    def uobject_bytes(i):
        ub = bytearray(UBOFF["UEP_TotalSize"])
        struct.pack_into("<Q", ub, UBOFF["ClassPrivate"], 0x6000000)
        struct.pack_into("<Q", ub, UBOFF["NamePrivate"], 0x100 + i)
        struct.pack_into("<Q", ub, UBOFF["OuterPrivate"], 0x7000000)
        struct.pack_into("<I", ub, UBOFF["InternalIndex_Private"], i)
        return bytes(ub)

    chunk0_bytes = b"".join(item_bytes(i) for i in range(min(num, per_chunk)))
    chunk1_bytes = b"".join(item_bytes(i) for i in range(per_chunk, num))
    objects_bytes = struct.pack("<QQ", chunk0, chunk1 if num_chunks > 1 else 0)

    def tu_bytes(objects_ptr):
        b = bytearray(TUOBJECTARRAY_TOTAL)
        struct.pack_into("<Q", b, TUOFF["Objects"], objects_ptr)
        struct.pack_into("<Q", b, TUOFF["PreAllocatedObjects"], 0)
        struct.pack_into("<I", b, TUOFF["MaxElements"], max_elements)
        struct.pack_into("<I", b, TUOFF["NumElements"], num)
        struct.pack_into("<I", b, TUOFF["MaxChunks"], 512)
        struct.pack_into("<I", b, TUOFF["NumChunks"], num_chunks)
        return bytes(b)

    spans = []
    if hypothesis == "ambiguous":
        # Bytes valid under BOTH hypotheses: u64@0x0 serves as B's Objects ptr
        # AND A's {FirstGC=low32=0, LastNonGC=high32=1}; u64@0x8 = 0 so A's
        # MaxObjects/OpenForDisregard are 0 and B's PreAllocated is null; the
        # embedded TUObjectArray (A, at +0x10) and B's direct read of the same
        # offsets both land on a consistent chunked header.
        amb = bytearray(FUOBJECTARRAY_TOTAL)
        struct.pack_into("<Q", amb, 0x0, (2 << 32) | 0)      # B Objects=0x2_00000000 / A GC pair (0,2)
        struct.pack_into("<Q", amb, 0x8, 0)                  # B PreAllocated=0
        # offsets 0x10.. double as B{MaxElements,NumElements,MaxChunks,NumChunks}
        # and A-embedded{Objects(low/high), PreAllocated(low/high)}.
        struct.pack_into("<Q", amb, 0x10, (1 << 32) | 0x1000000)  # A emb.Objects=0x1_01000000; B Max=0x1000000 Num=1
        struct.pack_into("<Q", amb, 0x18, (1 << 32) | 512)   # A emb.PreAlloc (non-null, ignored); B MaxChunks=512 NumChunks=1
        struct.pack_into("<I", amb, 0x20, max_elements)      # A emb.MaxElements
        struct.pack_into("<I", amb, 0x24, num)               # A emb.NumElements
        struct.pack_into("<I", amb, 0x28, 512)               # A emb.MaxChunks
        struct.pack_into("<I", amb, 0x2C, num_chunks)        # A emb.NumChunks
        spans.append({"start": addr, "size": FUOBJECTARRAY_TOTAL, "bytes": bytes(amb)})
        emb_objects = (1 << 32) | 0x1000000
        b_objects = 2 << 32
        for arr in (emb_objects, b_objects):
            spans.append({"start": arr, "size": 16,
                          "bytes": struct.pack("<QQ", chunk0, 0)})
    else:
        if hypothesis == "A":
            fa = bytearray(FUOBJECTARRAY_TOTAL)
            struct.pack_into("<I", fa, FAOFF["ObjFirstGCIndex"], 0)
            struct.pack_into("<I", fa, FAOFF["ObjLastNonGCIndex"], num - 1)
            struct.pack_into("<I", fa, FAOFF["MaxObjectsNotConsideredByGC"], max_elements)
            struct.pack_into("<I", fa, FAOFF["OpenForDisregardForGC"], 0)
            fa[FAOFF["ObjObjects"]:FAOFF["ObjObjects"] + TUOBJECTARRAY_TOTAL] = \
                tu_bytes(objects_arr if corrupt != "null_objects" else 0)
            if corrupt == "gc_order":
                struct.pack_into("<I", fa, FAOFF["ObjFirstGCIndex"], num + 5)
            spans.append({"start": addr, "size": FUOBJECTARRAY_TOTAL, "bytes": bytes(fa)})
        elif hypothesis == "B":
            spans.append({"start": addr, "size": TUOBJECTARRAY_TOTAL,
                          "bytes": tu_bytes(objects_arr if corrupt != "null_objects" else 0)})
        if corrupt == "counts" and spans:
            span = spans[0]
            data = bytearray(span["bytes"])
            struct.pack_into("<I", data, (FAOFF["ObjObjects"] if hypothesis == "A" else 0)
                             + TUOFF["NumElements"], max_elements * 8)
            span["bytes"] = bytes(data)

    if corrupt != "no_chunks":
        spans.append({"start": objects_arr, "size": 16, "bytes": objects_bytes})
        spans.append({"start": chunk0, "size": max(len(chunk0_bytes), 1), "bytes": chunk0_bytes})
        if chunk1:
            spans.append({"start": chunk1, "size": max(len(chunk1_bytes), 1), "bytes": chunk1_bytes})
        for i in range(num):  # UObject bytes behind each item's Object pointer
            spans.append({"start": items[i], "size": UBOFF["UEP_TotalSize"],
                          "bytes": uobject_bytes(i)})
    if corrupt == "no_header":
        spans = [s for s in spans if s["start"] != addr]
    return spans, image_base, goa_vaddr


if __name__ == "__main__":  # pragma: no cover - convenience dump for a span file
    import sys
    with open(sys.argv[1]) as fh:
        payload = json.load(fh)
    spans = payload["spans"]
    gua = probe_guobject_array(spans, payload["image_base"], payload["goa_vaddr"])
    uob = probe_uobject_item_chain(spans, gua)
    print(json.dumps({"guobject_array": gua, "uobject_item_chain": uob}, indent=2))
