#!/usr/bin/env python3
"""overlay_builder.py -- game-agnostic Unreal IoStore multi-mod overlay builder.

Agent GK (Claude round-6 item 3a), outputs/2026-10-08-l25-prep/pak-overlay/.
Mac-only, battery-safe: pure local python3 (3.9+), no network, no console,
no PRX builds. Rule-10 law: ZERO game names in logic -- the builder operates
on a generic "paks directory + mod archive manifest"; game titles appear
only in this docstring's provenance notes and in test fixtures.

CLASSIC PAK INTAKE (agent HI, Claude round-7 items A+B): a pure-python
reader for classic PAK v11 (Fnv64BugFix) archives -- unencrypted index and
entries, all-stored (compression None) only; everything else refuses with a
machine-readable verdict. Layout pins:
  P9  repak 355b5f62 retained source: footer.rs (per-version footer
      geometry, magic 0x5A6F12E1, 5x32B compression-name table),
      pak.rs Index::read (updated-index region order: mount FString, entry
      count, path-hash seed, has_path_hash u32 + offset/size/20B digest,
      has_directory u32 + offset/size/20B digest, encoded-blob byte size,
      encoded blob, non-encoded count), entry.rs read_encoded (bitfield
      layout, varint flags 31/30/29, 0x3f block-size escape) and Entry::read
      (physical 53-byte stored record).
  P10 pak-physical-header-schema-research-1 + pak-boolean-schema-research-1
      + pak-directory-schema-research-1 (native-observations): 4-byte LE
      secondary-presence booleans, full-directory record shape
      (dir FString, count, per-file name FString + i32 location; INT32_MIN
      tombstone), fail-closed bounded-reader policy.
  P11 The project's own promoted canonical v11 writer
      (ps5-mod-bridge/native-host/format/pak_stream_writer.c): exact emit
      geometry (221-byte footer, path-hash = seeded FNV-1a over lowercased
      mount-free path bytes with the UTF-16 double-step) proven against the
      retained repak fixtures and the retained tiny real v11 PAK of the
      Leon intake (347 B, 0 entries, unencrypted, None).
CONVERSION: asset re-serialization (classic Linker tables -> Zen export
bundles) is NOT done in python -- it would fabricate export bundles. The
pinned local retoc v0.1.5 binary (retoc-v0.1.5-raw-roundtrip-1, verified
deterministic) performs pak->zen offline; this builder only ORCHESTRATES it
behind --classic-pak/--retoc-tool, then admits the produced trio through
the exact same ModSpec admission as any IoStore mod. Without the tool the
verdict is an honest refusal plus the pointer (UNSUPPORTED.md section 7).

WHAT IT GENERALIZES (from the one-game adapter trial
.superpowers/native-dependency-spike/leon-iostore-adapter-trial-1 + its
primitive library iostore-sidecar-overlay-1, both READ-ONLY history):
  - N mods overlaid into ONE derived .utoc index + ONE sidecar partition
    (the proven Leon method: base UCAS untouched, mod payload bytes carried
    verbatim in a new partition-1 sidecar <stem>_s1.ucas, index derived).
  - deterministic mod ordering (explicit priority, then stable hash of mod
    identity), conflict detection with per-mod package-ID ledgers,
  - transaction journaling with sha verification, uninstall/rollback and
    stale game-update detection,
  - TOC version verdicts across the EIoStoreTocVersion 0..10 span with
    explicit UNSUPPORTED refusals where local pins are missing (never
    guesses), and encrypted-container refusal (decryption keys are out of
    scope Mac-side until Phase B).

LOCAL FORMAT PINS (every admitted layout fact; no external knowledge):
  P1  .superpowers/native-dependency-spike/unreal-library-container-survey-1/
      parser/source/CUE4Parse/CUE4Parse/UE4/IO/Objects/FIoStoreTocHeader.cs
      -- EIoStoreTocVersion enum 0..10, EIoContainerFlags, 144-byte header
      field order, TOC_MAGIC, UE6_0 reserved-field dual reading.
  P2  same tree FIoStoreTocResource.cs -- array-region order and version
      gates: perfect-hash seeds (>=4), overflow rows (>=5), directory index
      (>=2 AND Indexed AND size>0), metas ReplaceIoChunkHashWithIoHash
      (>=8), OnDemand extra (==6 AND flag), PartitionCount/PartitionSize
      forced for <3, EncryptionMethod derived from the Encrypted flag for
      <10 (EncryptionIVCount=0).
  P3  same tree FIoChunkId.cs / FIoOffsetAndLength.cs (10-byte 40+40-bit
      big-endian) / FIoStoreTocCompressedBlockEntry.cs (12-byte: 40-bit LE
      offset, 24-bit LE compressed, 24-bit LE uncompressed in u32, 1-byte
      method index) / FIoStoreTocEntryMeta.cs (v8: 20-byte hash + 1 flags +
      3 pad = 24; pre-v8 FIoChunkHash size NOT retained locally).
  P4  .superpowers/native-dependency-spike/unreal-dependency-compatibility-1/
      source/retoc/retoc/src/lib.rs (pinned retoc) -- FIoStoreTocHeader
      serialize order (asserts 0x90), read_hash_map/read_meta version
      gates, FNV-1a-style HashWithSeed (0xcbf29ce484222325 basis) used for
      the perfect-hash lookup.
  P5  .superpowers/native-dependency-spike/iostore-sidecar-overlay-1/
      {toc_overlay.py, container_header.py, directory_index.py} -- the
      proven one-game composition: derived row plan (base rows verbatim +
      sorted additions), negative-seed explicit-overflow lookup emit
      (CUE4Parse dispatches -seed-1 >= chunk_count to the overflow table),
      v5 ContainerHeader decode/serialize/merge-by-PackageId, v8
      directory-index canonical rebuild (dirs sorted by (depth, utf-8),
      files by (dir, name utf-8), FString string table).
  P6  .superpowers/native-dependency-spike/iostore-blake3-portable-1/
      upstream/ (official BLAKE3 portable reference incl.
      test_vectors.json) + verification.json ("v8 metadata uses the first
      20 bytes of a BLAKE3 digest", pure file-byte hashing, 27 real IoStore
      chunks matched retoc metadata).
  P7  outputs/2026-10-04-zcode-team/worker-container-headers/HEADER-SPEC.md
      -- from-retained-sources header spec incl. its explicit unknowns
      (FGuid internals, FIoChunkHash size, signature semantics).
  P8  Observed-file anchors: pakchunk0-ps5.utoc v8 (townfall records),
      the Leon PC mod trio v8 (leon-real-mod-intake utoc-analysis.json),
      Stellar Blade v3 headers (2026-09-30 container audit; no local .ucas
      pairing). All local captures: encrypted flag clear, signed flag
      clear, compression method count 0.

ADMISSION PROFILE (what build_overlay accepts; everything else refuses):
  TOC version 8, Indexed, not Encrypted/Signed/OnDemand/Compressed-flag,
  zero compression-method names (all blocks method-0 stored),
  single-partition unbounded inputs (PartitionCount==1,
  PartitionSize==UINT64_MAX), base UCAS 16-aligned and larger than the
  whole sidecar, base ContainerHeader chunk present, per-mod exactly one
  v5 ContainerHeader chunk with the mod's own container identity.

LAW: mod archive files (.utoc/.ucas) are READ-ONLY inputs -- opened with
mmap read-only, never written; every derived byte goes to the overlay
output/console-target directory; mod package bytes are carried VERBATIM in
the sidecar (this builder copies each mod's .ucas block stream directly,
unlike the historical trial which repacked through retoc -- see the report
DRIFT notes); only the INDEX and the merged ContainerHeader chunk are
derived. The base .ucas is never opened beyond bounded ranged reads.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import mmap
import os
import struct
import sys
import time
import uuid
from typing import Optional, Dict, List, Tuple, Sequence

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cityhash64 import cityhash64  # noqa: E402  (validated vs 17,138 stored FName hashes)

__all__ = [
    "Refused", "parse_toc", "inspect_toc", "parse_toc_header",
    "build_overlay", "ModSpec", "Blake3", "blake3_20", "main",
    "parse_pak", "inspect_pak", "classify_pak_entries", "PakEntry",
    "parse_substitutions", "convert_classic_pak",
]

TOC_MAGIC = b"-==--==--==--==-"
HEADER_SIZE = 144  # P1: FIoStoreTocHeader.SIZE
BLOCK_ENTRY_SIZE = 12  # P3
OFFSET_ENTRY_SIZE = 10  # P3
META_ENTRY_SIZE_V8 = 24  # P3: 20 hash + 1 flags + 3 pad
CHUNK_ID_SIZE = 12  # P3
U64_MAX = (1 << 64) - 1
U40_MAX = (1 << 40) - 1
UINT32_MAX = 0xFFFFFFFF

MAX_TOC_BYTES = 256 * 1024 * 1024
MAX_DIRECTORY_BYTES = 32 * 1024 * 1024
MAX_SIDECAR_BYTES = 512 * 1024 * 1024
MAX_CHUNKS = (1 << 31) - 2  # leave room for the negative lookup seed
MAX_PACKAGES = 262144
MAX_ARRAY_ITEMS = 4194304
MAX_HEADER_BYTES = 64 * 1024 * 1024

TOC_VERSION_NAMES = {
    0: "Invalid", 1: "Initial", 2: "DirectoryIndex", 3: "PartitionSize",
    4: "PerfectHash", 5: "PerfectHashWithOverflow", 6: "OnDemandMetaData",
    7: "RemovedOnDemandMetaData", 8: "ReplaceIoChunkHashWithIoHash",
    9: "AddedSourceHashes", 10: "ContainerEncryptionMethod",
}

CONTAINER_FLAGS = {0: "Compressed", 1: "Encrypted", 2: "Signed",
                   3: "Indexed", 4: "OnDemand"}

SUPPORTED_TOC_VERSION = 8
VERSION_REFUSAL_REASON = (
    "only v8 ReplaceIoChunkHashWithIoHash is locally pinned end-to-end "
    "(P3/P5/P8); pre-v8 FIoChunkHash size is not retained (P7 unknown 3) so "
    "meta rows and EOF closure cannot be verified, and v9/v10 add source-"
    "hash/encryption-IV regions with no locally observed file (P8)")


class Refused(ValueError):
    """Explicit unsupported/refused verdict; `code` is machine-readable."""

    def __init__(self, code: str, detail: str = ""):
        super().__init__(code + (": " + detail if detail else ""))
        self.code = code
        self.detail = detail


def require(condition: bool, code: str, detail: str = "") -> None:
    if not condition:
        raise Refused(code, detail)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def align16(value: int) -> int:
    return (value + 15) & ~15


# ---------------------------------------------------------------------------
# BLAKE3, pure python (P6: official portable reference algorithm).
# Used ONLY to hash derived bytes (the merged ContainerHeader chunk); mod
# and base meta rows are carried verbatim, never recomputed.
# ---------------------------------------------------------------------------

_B3_IV = (0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
          0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19)
_B3_SCHEDULE = (
    (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15),
    (2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8),
    (3, 4, 10, 12, 13, 2, 7, 14, 6, 5, 9, 0, 11, 15, 8, 1),
    (10, 7, 12, 9, 14, 3, 13, 15, 4, 0, 11, 2, 5, 8, 1, 6),
    (12, 13, 9, 11, 15, 10, 14, 8, 7, 2, 5, 3, 0, 1, 6, 4),
    (9, 14, 11, 5, 8, 12, 15, 1, 13, 3, 0, 10, 2, 6, 4, 7),
    (11, 15, 5, 0, 1, 9, 8, 6, 14, 10, 2, 12, 3, 4, 7, 13),
)
_M32 = 0xFFFFFFFF
_B3_BLOCK_LEN = 64
_B3_CHUNK_LEN = 1024
_B3_CHUNK_START = 1
_B3_CHUNK_END = 2
_B3_PARENT = 4
_B3_ROOT = 8


def _rotr(x: int, n: int) -> int:
    return ((x >> n) | (x << (32 - n))) & _M32


def _g(state: List[int], a: int, b: int, c: int, d: int, mx: int, my: int) -> None:
    state[a] = (state[a] + state[b] + mx) & _M32
    state[d] = _rotr(state[d] ^ state[a], 16)
    state[c] = (state[c] + state[d]) & _M32
    state[b] = _rotr(state[b] ^ state[c], 12)
    state[a] = (state[a] + state[b] + my) & _M32
    state[d] = _rotr(state[d] ^ state[a], 8)
    state[c] = (state[c] + state[d]) & _M32
    state[b] = _rotr(state[b] ^ state[c], 7)


def _round(state: List[int], m: Tuple[int, ...], schedule: Tuple[int, ...]) -> None:
    _g(state, 0, 4, 8, 12, m[schedule[0]], m[schedule[1]])
    _g(state, 1, 5, 9, 13, m[schedule[2]], m[schedule[3]])
    _g(state, 2, 6, 10, 14, m[schedule[4]], m[schedule[5]])
    _g(state, 3, 7, 11, 15, m[schedule[6]], m[schedule[7]])
    _g(state, 0, 5, 10, 15, m[schedule[8]], m[schedule[9]])
    _g(state, 1, 6, 11, 12, m[schedule[10]], m[schedule[11]])
    _g(state, 2, 7, 8, 13, m[schedule[12]], m[schedule[13]])
    _g(state, 3, 4, 9, 14, m[schedule[14]], m[schedule[15]])


def _compress_state(cv: Tuple[int, ...], block_words: Tuple[int, ...],
                    counter: int, block_len: int, flags: int) -> List[int]:
    state = [cv[0], cv[1], cv[2], cv[3], cv[4], cv[5], cv[6], cv[7],
             _B3_IV[0], _B3_IV[1], _B3_IV[2], _B3_IV[3],
             counter & _M32, (counter >> 32) & _M32, block_len, flags]
    for r in range(7):
        _round(state, block_words, _B3_SCHEDULE[r])
    return state


def _compress(cv: Tuple[int, ...], block_words: Tuple[int, ...],
              counter: int, block_len: int, flags: int) -> Tuple[int, ...]:
    state = _compress_state(cv, block_words, counter, block_len, flags)
    return tuple((state[i] ^ state[i + 8]) & _M32 for i in range(8))


def _compress_xof_16(cv: Tuple[int, ...], block_words: Tuple[int, ...],
                     counter: int, block_len: int, flags: int) -> Tuple[int, ...]:
    # blake3_compress_xof_portable: out[0..7] = state[i]^state[i+8];
    # out[8..15] = state[8+i]^cv[i]
    state = _compress_state(cv, block_words, counter, block_len, flags)
    return tuple(((state[i] ^ state[i + 8]) & _M32) if i < 8 else
                 ((state[i] ^ cv[i - 8]) & _M32) for i in range(16))


class _B3Output:
    __slots__ = ("cv", "block_words", "counter", "block_len", "flags")

    def __init__(self, cv, block_words, counter, block_len, flags):
        self.cv = cv
        self.block_words = block_words
        self.counter = counter
        self.block_len = block_len
        self.flags = flags

    def chaining_value(self):
        return _compress(self.cv, self.block_words, self.counter,
                         self.block_len, self.flags)


class _ChunkState:
    __slots__ = ("cv", "buf", "buf_len", "blocks_compressed", "counter", "flags")

    def __init__(self, key_words, counter, flags):
        self.cv = key_words
        self.buf = bytearray(_B3_BLOCK_LEN)
        self.buf_len = 0
        self.blocks_compressed = 0
        self.counter = counter
        self.flags = flags

    def start_flag(self):
        return _B3_CHUNK_START if self.blocks_compressed == 0 else 0

    def update(self, data: bytes) -> None:
        while data:
            if self.buf_len == _B3_BLOCK_LEN:
                words = struct.unpack("<16I", bytes(self.buf))
                self.cv = _compress(self.cv, words, self.counter,
                                    _B3_BLOCK_LEN,
                                    self.flags | self.start_flag())
                self.blocks_compressed += 1
                self.buf_len = 0
            take = min(_B3_BLOCK_LEN - self.buf_len, len(data))
            self.buf[self.buf_len:self.buf_len + take] = data[:take]
            self.buf_len += take
            data = data[take:]

    def length(self):
        return self.blocks_compressed * _B3_BLOCK_LEN + self.buf_len

    def output(self):
        words = struct.unpack("<16I", bytes(self.buf[:self.buf_len]).ljust(
            _B3_BLOCK_LEN, b"\0"))
        start = _B3_CHUNK_START if self.blocks_compressed == 0 else 0
        return _B3Output(self.cv, words, self.counter, self.buf_len,
                         self.flags | start | _B3_CHUNK_END)


class Blake3:
    """Minimal BLAKE3 (32-byte digest; keyed/XOF-by-length supported)."""

    def __init__(self, key: bytes = b""):
        if key:
            require(len(key) == 32, "blake3_key_must_be_32_bytes")
            self.key_words = struct.unpack("<8I", key)
        else:
            self.key_words = _B3_IV
        self.chunk = _ChunkState(self.key_words, 0, 0)
        self.cv_stack: List[Tuple[int, ...]] = []
        self.total_chunks = 0

    def _push_cv(self, new_cv: Tuple[int, ...], total_chunks: int) -> None:
        while total_chunks & 1 == 0:
            left = self.cv_stack.pop()
            block = struct.pack("<8I", *left) + struct.pack("<8I", *new_cv)
            new_cv = _compress(self.key_words, struct.unpack("<16I", block),
                               0, _B3_BLOCK_LEN, _B3_PARENT)
            total_chunks >>= 1
        self.cv_stack.append(new_cv)

    def update(self, data: bytes) -> "Blake3":
        if not data:
            return self
        # Finish any pending partial chunk first (P6 blake3.c
        # blake3_hasher_update_base: chunk_state_len > 0 branch).
        if self.chunk.length() > 0:
            take = min(_B3_CHUNK_LEN - self.chunk.length(), len(data))
            self.chunk.update(data[:take])
            data = data[take:]
            if not data:
                return self
            # chunk exactly full and more data follows: not the root; push.
            cv = self.chunk.output().chaining_value()
            self.total_chunks += 1
            self._push_cv(cv, self.total_chunks)
            self.chunk = _ChunkState(self.key_words, self.chunk.counter + 1, 0)
        # Whole 1024-byte chunks (never the root while more data follows).
        while len(data) > _B3_CHUNK_LEN:
            state = _ChunkState(self.key_words, self.chunk.counter, 0)
            state.update(data[:_B3_CHUNK_LEN])
            cv = state.output().chaining_value()
            self.total_chunks += 1
            self._push_cv(cv, self.total_chunks)
            self.chunk = _ChunkState(self.key_words, self.chunk.counter + 1, 0)
            data = data[_B3_CHUNK_LEN:]
        self.chunk.update(data)
        return self

    def digest(self, length: int = 32) -> bytes:
        output = self.chunk.output()
        parents = len(self.cv_stack)
        while parents > 0:
            parents -= 1
            left = self.cv_stack[parents]
            block = struct.pack("<8I", *left) + struct.pack("<8I", *output.chaining_value())
            output = _B3Output(self.key_words, struct.unpack("<16I", block),
                               0, _B3_BLOCK_LEN, _B3_PARENT)
        result = bytearray()
        counter = 0
        while len(result) < length:
            words = _compress_xof_16(output.cv, output.block_words, counter,
                                     output.block_len,
                                     output.flags | _B3_ROOT)
            result.extend(struct.pack("<16I", *words))
            counter += 1
        return bytes(result[:length])


def blake3_20(data: bytes) -> bytes:
    """First 20 bytes of the BLAKE3 digest = v8 IoStore meta hash (P6)."""
    return Blake3().update(data).digest(20)


# ---------------------------------------------------------------------------
# TOC parsing (v8 full admission; header-level verdicts for 0..10)
# ---------------------------------------------------------------------------

class Toc:
    """Parsed v8 IoStore TOC. Field names follow P1/P2."""

    __slots__ = ("header", "version", "ids", "offset_lengths", "seeds",
                 "overflow", "block_data", "directory_index", "metas",
                 "container_id", "block_size", "partition_count",
                 "partition_size", "container_flags", "key_guid")

    def __init__(self, **fields):
        for name, value in fields.items():
            setattr(self, name, value)

    @property
    def chunk_count(self) -> int:
        return len(self.ids)

    @property
    def block_count(self) -> int:
        return len(self.block_data) // BLOCK_ENTRY_SIZE

    def offset_length(self, row: int) -> Tuple[int, int]:
        start = row * OFFSET_ENTRY_SIZE
        raw = self.offset_lengths[start:start + OFFSET_ENTRY_SIZE]
        return (int.from_bytes(raw[:5], "big"), int.from_bytes(raw[5:], "big"))

    def meta(self, row: int) -> bytes:
        return self.metas[row * META_ENTRY_SIZE_V8:(row + 1) * META_ENTRY_SIZE_V8]

    def block(self, index: int) -> Tuple[int, int, int, int]:
        start = index * BLOCK_ENTRY_SIZE
        d = self.block_data
        return (int.from_bytes(d[start:start + 5], "little"),
                int.from_bytes(d[start + 5:start + 8], "little"),
                int.from_bytes(d[start + 8:start + 11], "little"),
                d[start + 11])


def parse_toc_header(data: bytes) -> Dict[str, object]:
    """Header-level parse for ANY version byte 0..10 -> verdict fields (P1)."""
    require(isinstance(data, bytes) and len(data) >= HEADER_SIZE,
            "toc_too_short_for_header")
    require(data[:16] == TOC_MAGIC, "invalid_toc_magic")
    version = data[16]
    require(version in TOC_VERSION_NAMES, "unknown_toc_version", str(version))
    return {
        "version": version,
        "version_name": TOC_VERSION_NAMES[version],
        "header_size": struct.unpack_from("<I", data, 20)[0],
        "toc_entry_count": struct.unpack_from("<I", data, 24)[0],
        "block_entry_count": struct.unpack_from("<I", data, 28)[0],
        "block_entry_size": struct.unpack_from("<I", data, 32)[0],
        "compression_method_name_count": struct.unpack_from("<I", data, 36)[0],
        "compression_method_name_length": struct.unpack_from("<I", data, 40)[0],
        "compression_block_size": struct.unpack_from("<I", data, 44)[0],
        "directory_index_size": struct.unpack_from("<I", data, 48)[0],
        "partition_count": struct.unpack_from("<I", data, 52)[0],
        "container_id": data[56:64],
        "key_guid": data[64:80],
        "container_flags": data[80],
        "encryption_method_raw": data[81],
        "perfect_hash_seeds_count": struct.unpack_from("<I", data, 84)[0],
        "partition_size": struct.unpack_from("<Q", data, 88)[0],
        "chunks_without_perfect_hash_count": struct.unpack_from("<I", data, 96)[0],
    }


def parse_toc(data: bytes, *, strict_admission: bool = True) -> Toc:
    """Parse the full v8 array region with EOF-closure verification.

    strict_admission=False only relaxes the reserved-zero-bytes checks (used
    when re-parsing derived output whose base reserved fields were nonzero);
    every admission rule that guards overlay composition stays on.
    """
    info = parse_toc_header(data)
    version = info["version"]
    require(version == SUPPORTED_TOC_VERSION,
            "unsupported_toc_version",
            "%d (%s): %s" % (version, info["version_name"],
                             VERSION_REFUSAL_REASON))
    require(len(data) <= MAX_TOC_BYTES, "toc_over_size_cap")
    require(info["header_size"] == HEADER_SIZE, "unsupported_header_size")
    flags = info["container_flags"]
    require(flags & ~0x1F == 0, "unknown_container_flag_bits", hex(flags))
    require(not flags & (1 << 1), "encrypted_container_refused",
            "Encrypted flag set: AES keys/decryption are out of scope on the "
            "Mac until Phase B (no local record decrypts any container)")
    require(info["key_guid"] == bytes(16), "encryption_key_guid_refused",
            "non-zero EncryptionKeyGuid; decryption out of scope (Phase B)")
    require(not flags & (1 << 2), "signed_container_refused",
            "Signed TOCs carry a signature array whose verification "
            "semantics are not locally pinned (P7 unknown 7)")
    require(flags & (1 << 3), "indexed_flag_required",
            "the directory index requires the Indexed flag (P2)")
    require(not flags & (1 << 4), "on_demand_container_refused")
    require(not flags & (1 << 0), "compressed_container_profile_refused",
            "Compressed flag set; no local record exercises it (P8)")
    require(info["compression_method_name_count"] == 0 and
            info["compression_method_name_length"] == 32,
            "unsupported_compression_methods",
            "only the zero-name all-stored profile is locally pinned (P8)")
    require(info["block_entry_size"] == BLOCK_ENTRY_SIZE,
            "unsupported_block_entry_size")
    if strict_admission:
        require(data[17:20] == bytes(3) and data[81:84] == bytes(3) and
                data[100:144] == bytes(44),
                "unsupported_reserved_header_fields")
    count = info["toc_entry_count"]
    blocks = info["block_entry_count"]
    directory_size = info["directory_index_size"]
    seed_count = info["perfect_hash_seeds_count"]
    overflow_count = info["chunks_without_perfect_hash_count"]
    require(0 < count < MAX_CHUNKS and blocks > 0 and
            seed_count <= count and overflow_count <= count,
            "invalid_toc_counts")
    require(info["partition_count"] in (1, 2), "unsupported_partition_count")
    expected = (HEADER_SIZE + count * (CHUNK_ID_SIZE + OFFSET_ENTRY_SIZE) +
                seed_count * 4 + overflow_count * 4 +
                blocks * BLOCK_ENTRY_SIZE + directory_size +
                count * META_ENTRY_SIZE_V8)
    require(expected == len(data), "toc_sections_do_not_consume_file",
            "expected %d bytes, file is %d" % (expected, len(data)))
    cursor = HEADER_SIZE
    ids = tuple(data[cursor + row * CHUNK_ID_SIZE:
                     cursor + (row + 1) * CHUNK_ID_SIZE] for row in range(count))
    require(len(set(ids)) == count, "duplicate_chunk_ids")
    cursor += count * CHUNK_ID_SIZE
    offsets = data[cursor:cursor + count * OFFSET_ENTRY_SIZE]
    cursor += count * OFFSET_ENTRY_SIZE
    seeds = struct.unpack_from("<%di" % seed_count, data, cursor) if seed_count else ()
    cursor += seed_count * 4
    overflow = struct.unpack_from("<%dI" % overflow_count, data, cursor) if overflow_count else ()
    cursor += overflow_count * 4
    require(len(set(overflow)) == len(overflow) and
            all(row < count for row in overflow), "invalid_overflow_rows")
    block_data = data[cursor:cursor + blocks * BLOCK_ENTRY_SIZE]
    cursor += blocks * BLOCK_ENTRY_SIZE
    directory = data[cursor:cursor + directory_size]
    cursor += directory_size
    metas = data[cursor:]
    for row in range(count):
        require(metas[row * META_ENTRY_SIZE_V8 + 20] in (0, 2) and
                metas[row * META_ENTRY_SIZE_V8 + 21:
                      row * META_ENTRY_SIZE_V8 + 24] == bytes(3),
                "unsupported_chunk_meta_flags_or_padding")
    return Toc(header=data[:HEADER_SIZE], version=version, ids=ids,
               offset_lengths=offsets, seeds=seeds, overflow=overflow,
               block_data=block_data, directory_index=directory, metas=metas,
               container_id=info["container_id"],
               block_size=info["compression_block_size"],
               partition_count=info["partition_count"],
               partition_size=info["partition_size"],
               container_flags=flags, key_guid=info["key_guid"])


def inspect_toc(data: bytes) -> Dict[str, object]:
    """Read-only verdict for any TOC version 0..10 (never guesses)."""
    info = parse_toc_header(data)
    flags = info["container_flags"]
    names = [name for bit, name in CONTAINER_FLAGS.items() if flags & (1 << bit)]
    verdict = "HEADER_PARSED"
    reasons: List[str] = []
    if info["version"] != SUPPORTED_TOC_VERSION:
        verdict = "UNSUPPORTED"
        reasons.append("version %d (%s)" % (info["version"],
                                            info["version_name"]))
        reasons.append(VERSION_REFUSAL_REASON)
    if flags & (1 << 1) or info["key_guid"] != bytes(16):
        verdict = "REFUSED"
        reasons.append("encrypted (Encrypted flag or non-zero key GUID); "
                       "keys out of scope Mac-side until Phase B")
    if flags & (1 << 2):
        if verdict == "HEADER_PARSED":
            verdict = "UNSUPPORTED"
        reasons.append("signed; signature array semantics not locally pinned")
    if info["compression_method_name_count"]:
        if verdict == "HEADER_PARSED":
            verdict = "UNSUPPORTED"
        reasons.append("compression methods declared (%d); only the "
                       "all-stored profile is pinned"
                       % info["compression_method_name_count"])
    if info["partition_count"] != 1:
        if verdict == "HEADER_PARSED":
            verdict = "UNSUPPORTED"
        reasons.append("partition_count %d; single-partition unbounded "
                       "inputs only" % info["partition_count"])
    out = dict(info)
    out["container_id_hex"] = info["container_id"].hex()
    out["key_guid_hex"] = info["key_guid"].hex()
    out["container_flag_names"] = names
    out["verdict"] = verdict
    out["reasons"] = reasons
    del out["container_id"]
    del out["key_guid"]
    return out


# ---------------------------------------------------------------------------
# Geometry validation + chunk extraction (single-partition stored profile)
# ---------------------------------------------------------------------------

def validate_single_partition(toc: Toc, file_size: int,
                              permit_tail_padding: bool = False) -> Dict[str, int]:
    require(toc.partition_count == 1 and toc.partition_size == U64_MAX,
            "input_must_be_unbounded_single_partition")
    require(isinstance(file_size, int) and 0 < file_size <= U40_MAX,
            "invalid_ucas_size")
    physical_end = 0
    aligned_end = 0
    for index in range(toc.block_count):
        offset, stored, decoded, method = toc.block(index)
        require(method == 0 and 0 < stored == decoded <= toc.block_size,
                "unsupported_block_encoding")
        require(offset + stored <= file_size, "block_exceeds_ucas_file")
        physical_end = max(physical_end, offset + stored)
        aligned_end = max(aligned_end, offset + align16(stored))
    require(permit_tail_padding or aligned_end <= file_size,
            "aligned_read_exceeds_ucas_file")
    for row in range(toc.chunk_count):
        offset, length = toc.offset_length(row)
        require(0 < length <= U40_MAX and
                offset + length <= toc.block_count * toc.block_size,
                "invalid_logical_chunk_range")
        first = offset // toc.block_size
        last = (offset + length - 1) // toc.block_size
        require(offset % toc.block_size < toc.block(first)[2],
                "chunk_starts_outside_decoded_block")
        require((offset + length - 1) % toc.block_size < toc.block(last)[2],
                "chunk_ends_outside_decoded_block")
        if last > first:
            require(all(toc.block(i)[2] == toc.block_size
                        for i in range(first, last)),
                    "chunk_spans_short_intermediate_block")
    return {"physical_end": physical_end, "aligned_end": aligned_end}


def extract_chunk_bytes(toc: Toc, row: int, store) -> bytes:
    """Independent logical-range reconstruction through block descriptors.

    `store` only needs slicing semantics (mmap or bytes-like view).
    """
    position, remaining = toc.offset_length(row)
    result = bytearray()
    while remaining:
        block_index, skip = divmod(position, toc.block_size)
        physical, stored, decoded, method = toc.block(block_index)
        if method != 0 or stored != decoded or skip >= decoded:
            raise Refused("chunk_not_wholly_in_decoded_blocks")
        take = min(remaining, decoded - skip)
        result.extend(store[physical + skip:physical + skip + take])
        position += take
        remaining -= take
    return bytes(result)


class _OffsetView:
    """mmap-like view of the sidecar with absolute partition-1 addressing."""

    __slots__ = ("data", "base")

    def __init__(self, data: bytes, base: int):
        self.data = data
        self.base = base

    def __getitem__(self, sl):
        if isinstance(sl, slice):
            start = sl.start - self.base if sl.start is not None else None
            stop = sl.stop - self.base if sl.stop is not None else None
            return self.data[start:stop]
        return self.data[sl - self.base]


# ---------------------------------------------------------------------------
# ContainerHeader (v5 SoftPackageReferencesOffset) -- P5 semantics
# ---------------------------------------------------------------------------

CH_MAGIC = 0x496F436E
CH_VERSION = 5


class StoreEntry:
    __slots__ = ("imported_packages", "shader_map_hashes")

    def __init__(self, imported_packages: Tuple[int, ...],
                 shader_map_hashes: Tuple[bytes, ...]):
        self.imported_packages = imported_packages
        self.shader_map_hashes = shader_map_hashes


class ContainerHeader:
    __slots__ = ("container_id", "packages", "soft_serial_region_absent",
                 "opaque_zero_suffix")

    def __init__(self, container_id, packages, soft_serial_region_absent,
                 opaque_zero_suffix):
        self.container_id = container_id
        self.packages = packages
        self.soft_serial_region_absent = soft_serial_region_absent
        self.opaque_zero_suffix = opaque_zero_suffix


def _need(data: bytes, offset: int, size: int) -> bytes:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise Refused("truncated_container_header_section")
    return data[offset:offset + size]


def parse_container_header(data: bytes) -> ContainerHeader:
    require(isinstance(data, bytes) and len(data) <= MAX_HEADER_BYTES,
            "container_header_bounds")
    magic, version, container_id = struct.unpack("<IIQ", _need(data, 0, 16))
    if magic != CH_MAGIC or version != CH_VERSION:
        raise Refused("unsupported_container_header_layout",
                      "magic/version %08x/%d" % (magic, version))
    count = struct.unpack("<I", _need(data, 16, 4))[0]
    require(count <= MAX_PACKAGES, "package_count_over_cap")
    ids = struct.unpack("<%dQ" % count, _need(data, 20, count * 8)) if count else ()
    require(len(set(ids)) == count, "duplicate_package_ids")
    cursor = 20 + count * 8
    store_size = struct.unpack("<I", _need(data, cursor, 4))[0]
    cursor += 4
    store = _need(data, cursor, store_size)
    cursor += store_size
    descriptor_bytes = count * 16
    require(store_size >= descriptor_bytes,
            "package_store_shorter_than_descriptors")
    packages: Dict[int, StoreEntry] = {}
    total_items = 0
    claimed = [(0, descriptor_bytes)]
    for index in range(count):
        entry_offset = index * 16
        (imported_count, imported_relative, shader_count,
         shader_relative) = struct.unpack_from("<4I", store, entry_offset)
        total_items += imported_count + shader_count
        require(total_items <= MAX_ARRAY_ITEMS, "array_items_over_cap")
        imported: Tuple[int, ...] = ()
        if imported_count:
            start = entry_offset + imported_relative
            require(start >= descriptor_bytes,
                    "imported_packages_inside_descriptor_table")
            imported = struct.unpack("<%dQ" % imported_count,
                                     _need(store, start, imported_count * 8))
            claimed.append((start, start + imported_count * 8))
        shaders: Tuple[bytes, ...] = ()
        if shader_count:
            start = entry_offset + 8 + shader_relative
            require(start >= descriptor_bytes,
                    "shader_hashes_inside_descriptors")
            raw = _need(store, start, shader_count * 20)
            shaders = tuple(raw[pos:pos + 20] for pos in range(0, len(raw), 20))
            claimed.append((start, start + len(raw)))
        packages[ids[index]] = StoreEntry(imported, shaders)
    covered_end = 0
    for start, end in sorted(claimed):
        if start > covered_end and any(store[covered_end:start]):
            raise Refused("unclaimed_nonzero_package_store_bytes")
        covered_end = max(covered_end, end)
    if any(store[covered_end:]):
        raise Refused("unclaimed_nonzero_package_store_suffix")
    for label in ("optional package IDs", "optional store entries",
                  "redirect names", "localized packages", "package redirects"):
        value = struct.unpack("<I", _need(data, cursor, 4))[0]
        cursor += 4
        if value != 0:
            raise Refused("nonempty_container_header_section", label)
    soft_offset, soft_size = struct.unpack("<qq", _need(data, cursor, 16))
    cursor += 16
    absent = soft_size == 0
    if absent:
        require(soft_offset in (0, cursor), "unknown_empty_soft_serial_offset")
    else:
        require(soft_size == 4 and soft_offset == cursor,
                "relocated_soft_serial_region")
        require(struct.unpack("<I", _need(data, cursor, 4))[0] == 0,
                "nonempty_soft_package_references")
        cursor += 4
    suffix = data[cursor:]
    require(len(suffix) <= 15 and not any(suffix),
            "nonzero_or_over_limit_opaque_suffix")
    return ContainerHeader(container_id, packages, absent, suffix)


def serialize_container_header(header: ContainerHeader) -> bytes:
    require(len(header.packages) <= MAX_PACKAGES, "package_count_over_cap")
    require(isinstance(header.opaque_zero_suffix, bytes) and
            len(header.opaque_zero_suffix) <= 15 and
            not any(header.opaque_zero_suffix), "unsupported_opaque_suffix")
    ids = sorted(header.packages)
    store = bytearray(len(ids) * 16)
    total_items = 0
    for index, package_id in enumerate(ids):
        entry = header.packages[package_id]
        total_items += len(entry.imported_packages) + len(entry.shader_map_hashes)
        require(total_items <= MAX_ARRAY_ITEMS, "array_items_over_cap")
        require(all(0 <= v <= U64_MAX for v in entry.imported_packages),
                "imported_id_range")
        require(all(len(v) == 20 for v in entry.shader_map_hashes),
                "shader_hash_size")
        position = index * 16
        imported_relative = 0
        shader_relative = 0
        if entry.imported_packages:
            imported_relative = len(store) - position
            store.extend(struct.pack("<%dQ" % len(entry.imported_packages),
                                     *entry.imported_packages))
        if entry.shader_map_hashes:
            shader_relative = len(store) - position - 8
            store.extend(b"".join(entry.shader_map_hashes))
        struct.pack_into("<4I", store, position, len(entry.imported_packages),
                         imported_relative, len(entry.shader_map_hashes),
                         shader_relative)
    output = bytearray(struct.pack("<IIQI", CH_MAGIC, CH_VERSION,
                                   header.container_id, len(ids)))
    if ids:
        output.extend(struct.pack("<%dQ" % len(ids), *ids))
    output.extend(struct.pack("<I", len(store)))
    output.extend(store)
    output.extend(b"\0" * 20)
    if header.soft_serial_region_absent:
        output.extend(struct.pack("<qq", 0, 0))
    else:
        output.extend(struct.pack("<qqI", len(output) + 16, 4, 0))
    output.extend(header.opaque_zero_suffix)
    require(len(output) <= MAX_HEADER_BYTES, "serialized_over_cap")
    return bytes(output)


# ---------------------------------------------------------------------------
# v8 directory index -- parse/emit + merge (canonical rebuild; P5)
# ---------------------------------------------------------------------------

DIR_INVALID = 0xFFFFFFFF


class DirFile:
    __slots__ = ("path", "directories", "name", "chunk_id")

    def __init__(self, path, directories, name, chunk_id):
        self.path = path
        self.directories = directories
        self.name = name
        self.chunk_id = chunk_id


def _read_fstring(data: bytes, offset: int, limit: int) -> Tuple[str, int]:
    require(offset <= len(data) - 4, "truncated_fstring_length")
    length = struct.unpack_from("<i", data, offset)[0]
    offset += 4
    if length == 0:
        return "", offset
    byte_count = abs(length) if length > 0 else -length * 2
    require(1 <= byte_count <= limit and byte_count <= len(data) - offset,
            "fstring_outside_limit")
    raw = data[offset:offset + byte_count]
    if length > 0:
        require(raw[-1] == 0, "fstring_missing_terminator")
        value = raw[:-1].decode("utf-8", "strict")
    else:
        require(raw[-2:] == b"\0\0", "fstring_missing_terminator")
        value = raw[:-2].decode("utf-16le", "strict")
    require("\0" not in value and len(value.encode("utf-8")) <= limit,
            "invalid_fstring_content")
    return value, offset + byte_count


def _pack_fstring(value: str) -> bytes:
    require("\0" not in value, "fstring_embedded_nul")
    raw = value.encode("utf-8", "strict")
    return struct.pack("<i", len(raw) + 1) + raw + b"\0"


def parse_directory_index(data: bytes, ids_by_row: Sequence[bytes],
                          source: str) -> Dict[str, object]:
    require(0 < len(data) <= MAX_DIRECTORY_BYTES, "directory_index_over_cap")
    require(len(ids_by_row) <= 2_000_000, "id_rows_over_cap")
    mount, pos = _read_fstring(data, 0, 4096)
    require(mount.endswith(("/", "\\")), "invalid_mount_point")
    directory_count = struct.unpack_from("<i", data, pos)[0]
    pos += 4
    require(1 <= directory_count <= 1_000_000, "directory_count_limit")
    directories = [struct.unpack_from("<IIII", data, pos + i * 16)
                   for i in range(directory_count)]
    pos += directory_count * 16
    file_count = struct.unpack_from("<i", data, pos)[0]
    pos += 4
    require(0 <= file_count <= 1_000_000, "file_count_limit")
    files = [struct.unpack_from("<III", data, pos + i * 12)
             for i in range(file_count)]
    pos += file_count * 12
    string_count = struct.unpack_from("<i", data, pos)[0]
    pos += 4
    strings = []
    for _ in range(string_count):
        value, pos = _read_fstring(data, pos, 4096)
        strings.append(value)
    require(pos == len(data), "directory_index_trailing_bytes")
    for index, (name, child, sibling, first_file) in enumerate(directories):
        require(name == DIR_INVALID or name < string_count,
                "directory_name_index_range")
        require(index == 0 or name != DIR_INVALID,
                "non_root_directory_without_name")
        require(child == DIR_INVALID or child < directory_count,
                "child_index_range")
        require(sibling == DIR_INVALID or sibling < directory_count,
                "sibling_index_range")
        require(first_file == DIR_INVALID or first_file < file_count,
                "file_index_range")
    require(directories[0][2] == DIR_INVALID, "root_directory_has_sibling")
    for name, next_file, user_data in files:
        require(name < string_count, "file_name_index_range")
        require(next_file == DIR_INVALID or next_file < file_count,
                "next_file_index_range")
        require(user_data < len(ids_by_row), "user_data_index_range")
    root_name = "" if directories[0][0] == DIR_INVALID else strings[directories[0][0]]
    directory_paths = {()}
    out_files: List[DirFile] = []
    seen_paths = set()
    seen_dirs = set()
    seen_files = set()
    prefix = mount if mount.endswith(("/", "\\")) else mount + "/"
    pending = [(0, ())]
    while pending:
        first, parent = pending.pop()
        index = first
        while index != DIR_INVALID:
            require(index not in seen_dirs, "directory_graph_cycle")
            seen_dirs.add(index)
            name_index, child, sibling, first_file = directories[index]
            name = "" if name_index == DIR_INVALID else strings[name_index]
            current = parent if index == 0 or not name else parent + (name,)
            directory_paths.add(current)
            file_index = first_file
            while file_index != DIR_INVALID:
                require(file_index not in seen_files, "file_graph_cycle")
                seen_files.add(file_index)
                file_name_index, next_file, user_data = files[file_index]
                file_name = strings[file_name_index]
                path = prefix + "/".join(
                    ((root_name,) if root_name else ()) + current + (file_name,))
                require(path not in seen_paths, "duplicate_virtual_path")
                seen_paths.add(path)
                out_files.append(DirFile(path, current, file_name,
                                         ids_by_row[user_data]))
                file_index = next_file
            if child != DIR_INVALID:
                pending.append((child, current))
            index = sibling
    require(len(seen_dirs) == directory_count, "unreachable_directory_entries")
    require(len(seen_files) == file_count, "unreachable_file_entries")
    return {"mount_point": mount, "root_name": root_name,
            "directories": frozenset(directory_paths),
            "files": tuple(out_files),
            "counts": {"directories": directory_count, "files": file_count,
                       "strings": string_count}}


def emit_directory_index(mount: str, root_name: str,
                         directories: frozenset,
                         files: Sequence[DirFile],
                         final_row_by_id: Dict[bytes, int]
                         ) -> Tuple[bytes, Dict[str, int]]:
    for row in files:
        require(row.chunk_id in final_row_by_id,
                "selected_file_references_unknown_chunk")
    all_dirs = set(directories) | {()}
    for row in files:
        for length in range(1, len(row.directories) + 1):
            all_dirs.add(row.directories[:length])
    ordered_dirs = sorted(all_dirs,
                          key=lambda parts: (len(parts),
                                             tuple(p.encode("utf-8")
                                                   for p in parts)))
    dir_index = {parts: i for i, parts in enumerate(ordered_dirs)}
    ordered_files = sorted(files, key=lambda row: (
        row.directories, row.name.encode("utf-8"), row.path))
    strings = {part for path in ordered_dirs for part in path}
    strings.update(row.name for row in ordered_files)
    if root_name:
        strings.add(root_name)
    ordered_strings = sorted(strings, key=lambda s: s.encode("utf-8"))
    string_index = {value: i for i, value in enumerate(ordered_strings)}
    child_paths: Dict[Tuple[str, ...], List[Tuple[str, ...]]] = {}
    for path in ordered_dirs:
        if path:
            child_paths.setdefault(path[:-1], []).append(path)
    files_by_dir: Dict[Tuple[str, ...], List[DirFile]] = {}
    for row in ordered_files:
        files_by_dir.setdefault(row.directories, []).append(row)
    file_indices = {row.path: i for i, row in enumerate(ordered_files)}
    next_sibling: Dict[Tuple[str, ...], int] = {}
    for children in child_paths.values():
        for i, path in enumerate(children):
            next_sibling[path] = (dir_index[children[i + 1]]
                                  if i + 1 < len(children) else DIR_INVALID)
    output = bytearray(_pack_fstring(mount))
    output.extend(struct.pack("<i", len(ordered_dirs)))
    for path in ordered_dirs:
        if path:
            name_index = string_index[path[-1]]
        elif root_name:
            name_index = string_index[root_name]
        else:
            name_index = DIR_INVALID
        children = child_paths.get(path, [])
        first_child = dir_index[children[0]] if children else DIR_INVALID
        rows = files_by_dir.get(path, [])
        first_file = file_indices[rows[0].path] if rows else DIR_INVALID
        output.extend(struct.pack("<IIII", name_index, first_child,
                                  next_sibling.get(path, DIR_INVALID),
                                  first_file))
    output.extend(struct.pack("<i", len(ordered_files)))
    for i, row in enumerate(ordered_files):
        next_file = (i + 1 if i + 1 < len(ordered_files) and
                     ordered_files[i + 1].directories == row.directories
                     else DIR_INVALID)
        output.extend(struct.pack("<III", string_index[row.name], next_file,
                                  final_row_by_id[row.chunk_id]))
    output.extend(struct.pack("<i", len(ordered_strings)))
    for value in ordered_strings:
        output.extend(_pack_fstring(value))
    require(len(output) <= MAX_DIRECTORY_BYTES, "merged_directory_over_cap")
    return bytes(output), {"directories": len(ordered_dirs),
                           "files": len(ordered_files),
                           "strings": len(ordered_strings),
                           "output_bytes": len(output)}


# ---------------------------------------------------------------------------
# Perfect-hash lookup verification (P4 HashWithSeed)
# ---------------------------------------------------------------------------

def hash_with_seed(chunk_id: bytes, seed: int) -> int:
    value = seed if seed else 0xcbf29ce484222325
    for byte in chunk_id:
        value = ((value * 0x100000001B3) & U64_MAX) ^ byte
    return value


def make_chunk_lookup(toc: Toc):
    overflow = {toc.ids[row]: row for row in toc.overflow}
    if not toc.seeds:
        plain = {cid: row for row, cid in enumerate(toc.ids)}
        return plain.get

    def lookup(cid):
        seed = toc.seeds[hash_with_seed(cid, 0) % len(toc.seeds)]
        if seed == 0:
            return None
        if seed < 0:
            slot = -seed - 1
            if slot >= toc.chunk_count:
                return overflow.get(cid)
        else:
            slot = hash_with_seed(cid, seed) % toc.chunk_count
        return slot if toc.ids[slot] == cid else None
    return lookup


def validate_lookup(toc: Toc) -> int:
    lookup = make_chunk_lookup(toc)
    require(all(lookup(cid) == row for row, cid in enumerate(toc.ids)),
            "chunk_lookup_does_not_resolve_all_rows")
    return toc.chunk_count


# ---------------------------------------------------------------------------
# Classic PAK v11 intake (agent HI; pure-python reader, P9-P11)
# ---------------------------------------------------------------------------

PAK_MAGIC = 0x5A6F12E1
PAK_FNV_BASIS = 0xCBF29CE484222325
PAK_FNV_PRIME = 0x100000001B3
PAK_TOMBSTONE = -0x80000000  # INT32_MIN location sentinel (P10)
SUPPORTED_PAK_VERSION = 11

MAX_PAK_BYTES = 2 * 1024 * 1024 * 1024
MAX_PAK_ENTRIES = 4_000_000
MAX_PAK_PATH_CHARS = 4096

# Footer geometry per version major (P9 footer.rs Version::size): base
# magic+version+offset+size+hash = 44; +1 encrypted (>=4); +16 key guid
# (>=7); +1 frozen (==9); +128 names (v8a); +32 more (v8b, so 5 slots).
_FOOTER_CANDIDATES = (
    (11, 221), (10, 221), (9, 222), (8, 221), (8, 189),
    (7, 61), (6, 45), (5, 45), (4, 45), (3, 44), (2, 44), (1, 44),
)

PAK_VERSION_NAMES = {
    1: "Initial", 2: "NoTimestamps", 3: "CompressionEncryption",
    4: "IndexEncryption", 5: "RelativeChunkOffsets", 6: "DeleteRecords",
    7: "EncryptionKeyGuid", 8: "FNameBasedCompression", 9: "FrozenIndex",
    10: "PathHashIndex", 11: "Fnv64BugFix",
}


class PakEntry:
    """One resolved classic-PAK record (stored profile)."""
    __slots__ = ("path", "offset", "size", "sha1", "sha1_verified")

    def __init__(self, path, offset, size, sha1, sha1_verified):
        self.path = path
        self.offset = offset
        self.size = size
        self.sha1 = sha1
        self.sha1_verified = sha1_verified


def _read_pak_fstring(data: bytes, offset: int) -> Tuple[str, int]:
    """PAK FString: i32 length; >0 utf-8 incl. NUL; <0 utf-16le (P10)."""
    require(offset <= len(data) - 4, "truncated_pak_fstring_length")
    length = struct.unpack_from("<i", data, offset)[0]
    offset += 4
    if length == 0:
        return "", offset
    if length > 0:
        require(length <= MAX_PAK_PATH_CHARS and offset + length <= len(data),
                "pak_fstring_outside_limit")
        raw = data[offset:offset + length]
        require(raw[-1] == 0, "pak_fstring_missing_terminator")
        value = raw[:-1].decode("utf-8", "strict")
        offset += length
    else:
        byte_count = -length * 2
        require(byte_count <= 2 * MAX_PAK_PATH_CHARS and
                offset + byte_count <= len(data), "pak_fstring_outside_limit")
        raw = data[offset:offset + byte_count]
        require(raw[-2:] == b"\0\0", "pak_fstring_missing_terminator")
        value = raw[:-2].decode("utf-16le", "strict")
        offset += byte_count
    require("\0" not in value, "pak_fstring_embedded_nul")
    return value, offset


def _pak_path_hash(seed: int, path: str) -> int:
    """Seeded FNV-1a over lowercased mount-free path bytes with the
    UTF-16 high-byte double-step (P11; the v11 'Fnv64BugFix' hash)."""
    value = (PAK_FNV_BASIS + seed) & U64_MAX
    for byte in path.lower().encode("utf-8"):
        value = ((value ^ byte) * PAK_FNV_PRIME) & U64_MAX
        value = (value * PAK_FNV_PRIME) & U64_MAX
    return value


def _locate_pak_footer(data: bytes) -> Tuple[int, int]:
    """Return (version_major, footer_offset) or refuse. Major 8 can be v8a
    (189 B) or v8b (221 B) -- both probed (P9)."""
    require(isinstance(data, bytes) and 0 < len(data) <= MAX_PAK_BYTES,
            "pak_size_outside_limits")
    for major, size in _FOOTER_CANDIDATES:
        if len(data) < size:
            continue
        footer = len(data) - size
        magic_at = footer + (16 if major >= 7 else 0) + (1 if major >= 4 else 0)
        if len(data) - magic_at < 8:
            continue
        magic, version = struct.unpack_from("<II", data, magic_at)
        if magic == PAK_MAGIC and version == major:
            return major, footer
    raise Refused("invalid_pak_magic",
                  "no classic-PAK footer (magic %08x) found at any known "
                  "version geometry" % PAK_MAGIC)


def parse_pak(data: bytes) -> Dict[str, object]:
    """Full parse of an unencrypted v11 classic PAK with all-stored entries.

    Returns pak facts: footer fields, mount, entries (path -> PakEntry with
    physical payload ranges + sha1 verification), secondary-index
    declarations, tombstone count. Refuses on: non-v11 versions, encrypted
    index/entries, compression, non-encoded records, missing full
    directory, path-hash mismatch, sha1 mismatch, out-of-range geometry
    (fail-closed per P10).
    """
    major, footer = _locate_pak_footer(data)
    require(major == SUPPORTED_PAK_VERSION, "classic_pak_version_unsupported",
            "version %d (%s): only v11 Fnv64BugFix is locally pinned "
            "end-to-end (P9-P11: the retained real v11 archive, the repak "
            "v5/v7/v8a/v8b/v9/v11 fixture corpus, and this project's "
            "promoted v11 writer)" % (major, PAK_VERSION_NAMES.get(major, "?")))
    encryption_guid = data[footer:footer + 16]
    encrypted_index = bool(data[footer + 16])
    index_offset, index_size = struct.unpack_from("<QQ", data, footer + 25)
    index_hash = data[footer + 41:footer + 61]
    compression_names = tuple(
        data[footer + 61 + i * 32:footer + 61 + (i + 1) * 32].split(b"\0")[0]
        .decode("ascii", "strict") for i in range(5))
    require(not encrypted_index, "classic_pak_index_encryption_refused",
            "footer EncryptedIndex set; AES keys/decryption are out of "
            "scope on the Mac until Phase B")
    require(encryption_guid == bytes(16),
            "classic_pak_encryption_refused",
            "non-zero EncryptionGuid; keys out of scope until Phase B")
    require(index_offset + index_size <= len(data),
            "classic_pak_index_outside_file")
    require(0 < index_size <= 256 * 1024 * 1024, "classic_pak_index_size")

    # primary index (P9 pak.rs updated-index order)
    mount, pos = _read_pak_fstring(data, index_offset)
    require(mount.endswith("/") or mount.endswith("\\"), "invalid_pak_mount")
    count = struct.unpack_from("<I", data, pos)[0]
    pos += 4
    require(0 <= count <= MAX_PAK_ENTRIES, "invalid_pak_entry_count")
    seed = struct.unpack_from("<Q", data, pos)[0]
    pos += 8
    has_path_hash = struct.unpack_from("<I", data, pos)[0]
    pos += 4
    require(has_path_hash in (0, 1), "invalid_pak_boolean_width")
    path_index = None
    if has_path_hash:
        ph_off, ph_size = struct.unpack_from("<QQ", data, pos)
        ph_hash = data[pos + 16:pos + 36]
        pos += 36
        require(ph_off + ph_size <= len(data),
                "classic_pak_path_index_outside_file")
        path_index = (ph_off, ph_size, ph_hash)
    has_directory = struct.unpack_from("<I", data, pos)[0]
    pos += 4
    require(has_directory in (0, 1), "invalid_pak_boolean_width")
    require(has_directory,
            "classic_pak_full_directory_required",
            "no full-directory index: only the path-hash secondary remains "
            "and it carries no paths -- names would have to be guessed")
    fd_off, fd_size = struct.unpack_from("<QQ", data, pos)
    fd_hash = data[pos + 16:pos + 36]
    pos += 36
    require(fd_off + fd_size <= len(data),
            "classic_pak_directory_outside_file")
    require(0 < fd_size <= 256 * 1024 * 1024, "classic_pak_directory_size")
    encoded_size = struct.unpack_from("<I", data, pos)[0]
    pos += 4
    encoded = data[pos:pos + encoded_size]
    pos += encoded_size
    nonencoded_count = struct.unpack_from("<I", data, pos)[0]
    pos += 4
    require(nonencoded_count == 0,
            "classic_pak_nonencoded_entries_unsupported",
            "%d non-encoded records; the pinned writer never emits them and "
            "their legacy layout is not locally pinned" % nonencoded_count)
    require(pos == index_offset + index_size,
            "classic_pak_primary_index_trailing_bytes")
    if path_index is not None:
        ph_off, ph_size, _ = path_index
        require(not (fd_off < ph_off + ph_size and ph_off < fd_off + fd_size),
                "classic_pak_secondary_ranges_overlap")

    # full directory index (P9/P10): dir FString, count, name FString, i32 loc
    directory = data[fd_off:fd_off + fd_size]
    dpos = 0
    dir_count = struct.unpack_from("<I", directory, dpos)[0]
    dpos += 4
    require(0 <= dir_count <= 1_000_000, "classic_pak_directory_count")
    rows: List[Tuple[str, int]] = []  # (path, location)
    tombstones = 0
    seen_paths = set()
    total_files = 0
    for _ in range(dir_count):
        dir_name, dpos = _read_pak_fstring(directory, dpos)
        file_count = struct.unpack_from("<I", directory, dpos)[0]
        dpos += 4
        require(0 <= file_count <= MAX_PAK_ENTRIES, "classic_pak_file_count")
        total_files += file_count
        for _ in range(file_count):
            file_name, dpos = _read_pak_fstring(directory, dpos)
            location = struct.unpack_from("<i", directory, dpos)[0]
            dpos += 4
            path = (dir_name[1:] if dir_name.startswith("/") else dir_name) \
                + file_name
            require(path not in seen_paths, "classic_pak_duplicate_path")
            seen_paths.add(path)
            if location == PAK_TOMBSTONE:
                tombstones += 1
                continue
            require(location >= 0,
                    "classic_pak_nonencoded_entries_unsupported",
                    "negative location %d resolves a non-encoded record"
                    % location)
            rows.append((path, location))
    require(dpos == len(directory), "classic_pak_directory_trailing_bytes")
    require(len(rows) + tombstones == total_files, "classic_pak_file_rows")

    # path-hash secondary (declared + verified when present; P10/P11).
    # Producers differ in the declared range: repak's own fixtures and the
    # promoted writer emit exactly 4+count*12 (+ an optional trailing u32(0));
    # the earlier in-repo writer declared the range through the directory
    # start. The pairs themselves are pinned; the check is therefore a
    # floor, extra declared bytes are inert.
    path_hash_pairs: Dict[int, int] = {}
    if path_index is not None:
        ph_off, ph_size, _ = path_index
        blob = data[ph_off:ph_off + ph_size]
        require(ph_size >= 4, "classic_pak_path_index_short")
        pcount = struct.unpack_from("<I", blob, 0)[0]
        require(ph_size >= 4 + pcount * 12, "classic_pak_path_index_size")
        require(pcount == len(rows), "classic_pak_path_index_count")
        for i in range(pcount):
            phash, loc = struct.unpack_from("<Qi", blob, 4 + i * 12)
            path_hash_pairs[phash] = loc

    entries: Dict[str, PakEntry] = {}
    for path, location in rows:
        require(0 <= location <= len(encoded) - 4,
                "classic_pak_encoded_location_range", path)
        epos = location
        bits = struct.unpack_from("<I", encoded, epos)[0]
        epos += 4
        compression_slot = (bits >> 23) & 0x3F
        entry_encrypted = bool(bits & (1 << 22))
        block_count = (bits >> 6) & 0xFFFF
        block_size_field = bits & 0x3F
        if block_size_field == 0x3F:
            block_size = struct.unpack_from("<I", encoded, epos)[0]
            epos += 4
        else:
            block_size = block_size_field << 11

        def _varint(flag_bit: int) -> int:
            nonlocal epos
            if bits & (1 << flag_bit):
                value = struct.unpack_from("<I", encoded, epos)[0]
                epos += 4
                return value
            value = struct.unpack_from("<Q", encoded, epos)[0]
            epos += 8
            return value

        offset = _varint(31)
        uncompressed = _varint(30)
        if compression_slot:
            compressed = _varint(29)
        else:
            compressed = uncompressed
        require(not entry_encrypted, "classic_pak_encryption_refused",
                "entry %s carries the encrypted bit; keys out of scope "
                "until Phase B" % path)
        require(compression_slot == 0,
                "classic_pak_compression_refused",
                "entry %s uses compression slot %d (%s); only the all-stored "
                "None profile is admitted -- decompression/recompression "
                "belongs to the conversion tool, and the IoStore sidecar "
                "profile carries stored blocks only" %
                (path, compression_slot - 1,
                 compression_names[compression_slot - 1]
                 if 0 < compression_slot <= 5 else "unnamed"))
        require(block_count in (0, 1),
                "classic_pak_compression_refused",
                "entry %s declares %d blocks; a stored record is monolithic"
                % (path, block_count))
        if block_count == 1 and block_size and uncompressed > block_size:
            require(False, "classic_pak_compression_refused",
                    "entry %s exceeds its declared block size (%d > %d): "
                    "block-split stored records do not exist in the pinned "
                    "profile" % (path, uncompressed, block_size))
        # physical record: verify against the 53-byte repeated header (P9/P11)
        require(offset + 53 + uncompressed <= len(data),
                "classic_pak_entry_outside_file", path)
        (phys_offset, phys_compressed, phys_uncompressed, phys_slot
         ) = struct.unpack_from("<QQQI", data, offset)
        stored_sha1 = data[offset + 28:offset + 48]
        require(phys_slot == 0 and phys_compressed == phys_uncompressed
                == uncompressed and phys_offset in (0, offset),
                "classic_pak_physical_header_mismatch", path)
        payload = data[offset + 53:offset + 53 + uncompressed]
        # Per-entry SHA1 = digest of the stored payload bytes. Verified
        # against every retained producer family (repak's UnrealPak
        # fixtures, the promoted v11 writer, the earlier in-repo writer);
        # the intentional corruption fixture fails exactly here.
        sha1_verified = hashlib.sha1(payload).digest() == stored_sha1
        require(sha1_verified, "classic_pak_entry_sha1_mismatch", path)
        if path_hash_pairs:
            phash = _pak_path_hash(seed, path)
            require(phash in path_hash_pairs and
                    path_hash_pairs[phash] == location,
                    "classic_pak_path_hash_mismatch", path)
        entries[path] = PakEntry(path, offset + 53, uncompressed,
                                 stored_sha1.hex(), sha1_verified)
    require(entries, "classic_pak_empty",
            "no live entries (all tombstones or zero files); nothing to "
            "convert into the IoStore overlay model")
    return {
        "version": major,
        "mount": mount,
        "entry_count": len(entries),
        "tombstones": tombstones,
        "path_hash_seed": seed,
        "index_offset": index_offset,
        "index_size": index_size,
        "index_sha1": index_hash.hex(),
        "index_sha1_verified": hashlib.sha1(
            data[index_offset:index_offset + index_size]).digest() == index_hash,
        "secondary": {
            "path_hash_index": path_index[2].hex() if path_index else None,
            "full_directory_sha1_declared": fd_hash.hex(),
            "full_directory_sha1_verified":
                hashlib.sha1(directory).hexdigest() == fd_hash.hex(),
        },
        "compression_names": compression_names,
        "entries": entries,
    }


def classify_pak_entries(pak: Dict[str, object]) -> Dict[str, object]:
    """Content-family verdicts for the pak->zen conversion stage.

    The pinned conversion tool converts cooked split packages
    (.uasset/.umap + .uexp, optional .ubulk/.uptnl/.m.ubulk siblings) and
    shader libraries; script objects feed VNI. Loose non-asset files have
    NO IoStore chunk model and belong to the classic-pak staging path (the
    mod manager's file-mod route), never to a silent drop.
    """
    entries: Dict[str, PakEntry] = pak["entries"]
    by_dir: Dict[str, List[str]] = {}
    for path in entries:
        head, _, name = path.rpartition("/")
        by_dir.setdefault(head, []).append(name)
    assets, uncooked, shader_libs, script_objects, loose, orphan_bulk = \
        [], [], [], [], [], []
    for path in sorted(entries):
        head, _, name = path.rpartition("/")
        stem, _, ext = name.rpartition(".")
        ext = ext.lower() if ext else ""
        siblings = set(by_dir.get(head, ()))
        if ext in ("uasset", "umap"):
            if (stem + ".uexp") in siblings:
                assets.append(path)
            else:
                uncooked.append(path)
        elif ext == "ushaderbytecode":
            shader_libs.append(path)
        elif name == "scriptobjects.bin":
            script_objects.append(path)
        elif ext in ("uexp", "ubulk", "uptnl", "m.ubulk"):
            owner = next((e for e in (stem + ".uasset", stem + ".umap")
                          if e in siblings), None)
            if owner is None and ext != "uexp":
                orphan_bulk.append(path)
            elif owner is None:
                uncooked.append(path)
        else:
            loose.append(path)
    convertible = (not uncooked and not loose and not orphan_bulk and
                   (assets or shader_libs or script_objects))
    return {
        "asset_packages": assets, "uncooked_assets": uncooked,
        "shader_libraries": shader_libs, "script_objects": script_objects,
        "orphan_bulk": orphan_bulk, "loose_files": loose,
        "convertible": convertible,
    }


PAK_VERSION_REFUSAL = (
    "only PAK v11 Fnv64BugFix is locally pinned end-to-end; other majors "
    "refuse (footer geometry known from the pinned repak source, but no "
    "v11-external production archive is retained locally)")


def inspect_pak(data: bytes) -> Dict[str, object]:
    """Read-only verdict for any classic PAK (never guesses)."""
    out: Dict[str, object] = {"format": "classic-pak"}
    try:
        major, footer = _locate_pak_footer(data)
    except Refused as error:
        return {"format": "classic-pak", "verdict": "REFUSED",
                "reasons": [error.code + ": " + error.detail]}
    out["version"] = major
    out["version_name"] = PAK_VERSION_NAMES.get(major, "?")
    encrypted_index = bool(data[footer + 16]) if major >= 4 else False
    guid = data[footer:footer + 16] if major >= 7 else bytes(16)
    out["encrypted_index"] = encrypted_index
    out["encryption_guid_hex"] = guid.hex()
    verdict = "UNSUPPORTED"
    reasons = [PAK_VERSION_REFUSAL] if major != SUPPORTED_PAK_VERSION else []
    if encrypted_index or guid != bytes(16):
        verdict = "REFUSED"
        reasons.append("encrypted (index flag or key GUID); keys out of "
                       "scope Mac-side until Phase B")
    elif major == SUPPORTED_PAK_VERSION:
        try:
            pak = parse_pak(data)
        except Refused as error:
            verdict = "REFUSED"
            reasons.append(error.code + ": " + error.detail)
        else:
            families = classify_pak_entries(pak)
            out["mount_point"] = pak["mount"]
            out["entry_count"] = pak["entry_count"]
            out["tombstones"] = pak["tombstones"]
            out["index_sha1_verified"] = pak["index_sha1_verified"]
            out["families"] = {k: len(v) for k, v in families.items()
                               if isinstance(v, list)}
            if families["convertible"]:
                verdict = "CONVERTIBLE_VIA_PINNED_RETOC"
                reasons.append("unencrypted all-stored v11 with only cooked "
                               "split packages; convert with the pinned "
                               "retoc binary (--classic-pak --retoc-tool), "
                               "then admit as an IoStore mod trio")
            else:
                verdict = "REFUSED"
                if families["uncooked_assets"]:
                    reasons.append(
                        "%d uncooked package(s) (no .uexp sibling): the "
                        "conversion tool skips them -- refuse rather than "
                        "silently lose content" %
                        len(families["uncooked_assets"]))
                if families["loose_files"] or families["orphan_bulk"]:
                    reasons.append(
                        "%d loose/orphan file(s) have no IoStore chunk "
                        "model; use the classic-pak staging path (mod "
                        "manager file mods), not the IoStore overlay" %
                        (len(families["loose_files"]) +
                         len(families["orphan_bulk"])))
    out["verdict"] = verdict
    out["reasons"] = reasons
    return out


# ---------------------------------------------------------------------------
# Classic-pak -> IoStore conversion orchestration (pinned local tool only)
# ---------------------------------------------------------------------------

RETOC_TOC_OVERRIDE = "ReplaceIoChunkHashWithIoHash"
RETOC_HEADER_OVERRIDE = "SoftPackageReferencesOffset"
RETOC_VERSIONS = ("UE4_25", "UE4_26", "UE4_27", "UE5_0", "UE5_1", "UE5_2",
                  "UE5_3", "UE5_4", "UE5_5", "UE5_6", "UE5_7")
DEFAULT_RETOC_VERSION = "UE5_3"


def convert_classic_pak(pak_path: str, mod_id: str, retoc_tool: str,
                        engine_version: str,
                        staging_dir: str) -> Tuple["ModSpec", Dict[str, object]]:
    """Convert one classic PAK into an admitted IoStore mod trio.

    The asset re-serialization is performed by the PINNED LOCAL retoc
    binary (verified deterministic: identical invocations produce identical
    output shas); this function never fabricates export bundles itself. The
    produced trio must pass the ordinary ModSpec admission or the
    conversion refuses. The source PAK is a read-only input; outputs land
    under staging_dir only (originals-untouched law).
    """
    import subprocess  # local, offline, pinned tool only
    require(engine_version in RETOC_VERSIONS, "invalid_retoc_version",
            engine_version)
    require(os.path.isfile(pak_path), "classic_pak_source_missing", pak_path)
    require(os.path.isfile(retoc_tool), "retoc_tool_missing", retoc_tool)
    with open(pak_path, "rb") as handle:
        pak_bytes = handle.read()
    pak = parse_pak(pak_bytes)
    families = classify_pak_entries(pak)
    require(families["convertible"], "classic_pak_content_refused",
            json.dumps({k: v for k, v in families.items()
                        if isinstance(v, list) and v})[:4000])
    stage = os.path.join(staging_dir, mod_id)
    utoc = os.path.join(stage, mod_id + ".utoc")
    ucas = os.path.join(stage, mod_id + ".ucas")
    if not (os.path.isfile(utoc) and os.path.isfile(ucas) and
            sha256_file(utoc) and sha256_file(ucas)):
        os.makedirs(stage, exist_ok=True)
        for stale in (utoc, ucas, os.path.join(stage, mod_id + ".pak")):
            if os.path.exists(stale):
                os.remove(stale)
        proc = subprocess.run(
            [retoc_tool, "--override-toc-version", RETOC_TOC_OVERRIDE,
             "--override-container-header-version", RETOC_HEADER_OVERRIDE,
             "to-zen", os.path.abspath(pak_path), utoc,
             "--version", engine_version],
            capture_output=True, text=True, timeout=1800)
        require(proc.returncode == 0, "retoc_conversion_failed",
                (proc.stderr or proc.stdout)[-2000:])
    spec = ModSpec(mod_id, utoc, ucas)
    return spec, {
        "kind": "classic-pak",
        "pak_path": os.path.abspath(pak_path),
        "pak_sha256": sha256_file(pak_path),
        "pak_entry_count": pak["entry_count"],
        "retoc_tool": os.path.abspath(retoc_tool),
        "retoc_tool_sha256": sha256_file(retoc_tool),
        "retoc_args": ["--override-toc-version", RETOC_TOC_OVERRIDE,
                       "--override-container-header-version",
                       RETOC_HEADER_OVERRIDE, "to-zen", "<pak>", "<out.utoc>",
                       "--version", engine_version],
        "retoc_version": engine_version,
        "staged_utoc_path": os.path.abspath(utoc),
        "staged_ucas_path": os.path.abspath(ucas),
        "utoc_sha256": spec.utoc_sha,
        "ucas_sha256": spec.ucas_sha,
    }


# ---------------------------------------------------------------------------
# L1/L2 shader-fallback substitutions (agent HI; consumes GM directives)
# ---------------------------------------------------------------------------

SUBSTITUTION_ACTIONS = ("hide", "closest-match")


def _parse_package_id(value: object, label: str) -> int:
    require(isinstance(value, str), "invalid_substitution_%s" % label,
            "expected hex string")
    text = value[2:] if value.lower().startswith("0x") else value
    require(len(text) == 16, "invalid_substitution_%s" % label,
            "expected 16 hex digits, got %r" % (value,))
    try:
        pid = int(text, 16)
    except ValueError:
        raise Refused("invalid_substitution_%s" % label, repr(value))
    require(0 <= pid <= U64_MAX, "invalid_substitution_%s" % label)
    return pid


def parse_substitutions(path_or_doc) -> List[Dict[str, object]]:
    """Load + schema-validate a substitution directive file (GM classifier
    output assembled by the mod manager). Rows:
      {"mod_id","package_id","action":"hide"} -- suppress the mod's override
        so the base export shows (base store entry + base chunk rows win);
      {"mod_id","package_id","action":"closest-match","from_package_id",
        "to_package_id"} -- repoint the DERIVED merged store entry's import
        from the broken parent to a base package id.
    Derived-overlay entries only; originals are never touched (law).
    """
    if isinstance(path_or_doc, str):
        with open(path_or_doc, "r", encoding="utf-8") as handle:
            doc = json.load(handle)
    else:
        doc = path_or_doc
    require(isinstance(doc, dict) and doc.get("schema") == 1,
            "invalid_substitution_schema")
    rows = doc.get("rows")
    require(isinstance(rows, list) and rows, "invalid_substitution_rows")
    seen = set()
    parsed = []
    for row in rows:
        require(isinstance(row, dict), "invalid_substitution_row")
        allowed = {"mod_id", "package_id", "action", "from_package_id",
                   "to_package_id"}
        require(set(row) <= allowed, "unknown_substitution_fields",
                ",".join(sorted(set(row) - allowed)))
        mod_id = row.get("mod_id")
        action = row.get("action")
        require(isinstance(mod_id, str) and mod_id,
                "invalid_substitution_mod_id")
        require(action in SUBSTITUTION_ACTIONS,
                "invalid_substitution_action", repr(action))
        pid = _parse_package_id(row.get("package_id"), "package_id")
        require((mod_id, pid) not in seen, "duplicate_substitution_row")
        seen.add((mod_id, pid))
        out = {"mod_id": mod_id, "package_id": pid, "action": action}
        if action == "closest-match":
            out["from_package_id"] = _parse_package_id(
                row.get("from_package_id"), "from_package_id")
            out["to_package_id"] = _parse_package_id(
                row.get("to_package_id"), "to_package_id")
        parsed.append(out)
    return parsed


# ---------------------------------------------------------------------------
# Multi-mod planning
# ---------------------------------------------------------------------------

def stable_mod_hash(mod_id: str) -> str:
    return hashlib.sha256(mod_id.encode("utf-8")).hexdigest()[:16]


class ModSpec:
    """One mod archive: a (utoc, ucas) pair. Files are read-only inputs."""

    def __init__(self, mod_id: str, utoc_path: str, ucas_path: str,
                 priority: int = 0):
        require(mod_id and "/" not in mod_id and "=" not in mod_id,
                "invalid_mod_id", mod_id)
        self.mod_id = mod_id
        self.utoc_path = os.path.abspath(utoc_path)
        self.ucas_path = os.path.abspath(ucas_path)
        self.priority = priority
        self.stable_hash = stable_mod_hash(mod_id)
        try:
            self.utoc_bytes = open(self.utoc_path, "rb").read()
            self.ucas_size = os.path.getsize(self.ucas_path)
        except OSError as error:
            raise Refused("mod_source_unreadable", str(error))
        self.utoc_sha = sha256_bytes(self.utoc_bytes)
        self.ucas_sha = sha256_file(self.ucas_path)
        self.toc = parse_toc(self.utoc_bytes)
        geometry = validate_single_partition(self.toc, self.ucas_size,
                                             permit_tail_padding=True)
        require(self.ucas_size - geometry["physical_end"] <= 15,
                "mod_ucas_unaccounted_trailing_bytes",
                "%s: %d trailing bytes" % (mod_id,
                                           self.ucas_size -
                                           geometry["physical_end"]))
        with open(self.ucas_path, "rb") as handle:
            store = mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ)
            try:
                require(not any(store[geometry["physical_end"]:]),
                        "nonzero_mod_ucas_trailing_padding")
            finally:
                store.close()
        self.ucas_aligned_end = align16(geometry["aligned_end"])
        header_rows = [cid for cid in self.toc.ids if cid[11] == 6]
        require(len(header_rows) == 1,
                "mod_requires_exactly_one_container_header",
                "%s has %d" % (mod_id, len(header_rows)))
        self.header_chunk_id = header_rows[0]
        require(self.header_chunk_id == self.toc.container_id + bytes.fromhex('00000006'),
                "mod_header_chunk_identity_mismatch")
        require(all(self.toc.meta(row)[20] == 0
                    for row in range(self.toc.chunk_count)),
                "memory_mapped_mod_chunks_unsupported")
        require(all(cid[11] != 4 for cid in self.toc.ids),
                "memory_mapped_bulk_mod_chunks_unsupported")
        require(len(self.toc.directory_index) > 0,
                "mod_directory_index_absent",
                "%s carries no directory index (an empty converted or empty "
                "archive); the overlay needs package paths" % mod_id)
        self.directory = parse_directory_index(self.toc.directory_index,
                                               self.toc.ids, mod_id)


def effective_mod_order(mods: Sequence[ModSpec]) -> List[ModSpec]:
    """Deterministic order: priority, then stable hash, then id.

    Later position = higher precedence (later-wins), mirroring PC ~mods
    alphabetical mount semantics where the lexically-later patch wins.
    """
    return sorted(mods, key=lambda m: (m.priority, m.stable_hash, m.mod_id))


_MOD_HEADER_CACHE: Dict[Tuple[str, str], bytes] = {}


def read_mod_header_bytes(mod: ModSpec) -> bytes:
    """Bounded ranged extraction of the mod's ContainerHeader chunk."""
    key = (mod.mod_id, mod.utoc_sha)
    cached = _MOD_HEADER_CACHE.get(key)
    if cached is not None:
        return cached
    row = mod.toc.ids.index(mod.header_chunk_id)
    with open(mod.ucas_path, "rb") as handle:
        store = mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ)
        try:
            data = extract_chunk_bytes(mod.toc, row, store)
        finally:
            store.close()
    _MOD_HEADER_CACHE[key] = data
    return data


def read_base_header_chunk(base: Toc, base_ucas_path: Optional[str],
                           preextracted_path: Optional[str]) -> bytes:
    """Base ContainerHeader chunk: bounded ranged reads from the base .ucas
    when present, or a pinned pre-extracted copy (the historical record)."""
    if preextracted_path is not None:
        return open(preextracted_path, "rb").read()
    require(base_ucas_path is not None, "base_container_header_unavailable",
            "provide the base .ucas path or a pre-extracted container-header "
            "chunk file")
    header_id = base.container_id + bytes.fromhex('00000006')
    row = base.ids.index(header_id)
    with open(base_ucas_path, "rb") as handle:
        store = mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ)
        try:
            return extract_chunk_bytes(base, row, store)
        finally:
            store.close()


def package_id_of_chunk(chunk_id: bytes) -> int:
    """First 8 bytes of an FIoChunkId, little-endian (P3/P5)."""
    return struct.unpack("<Q", chunk_id[:8])[0]


def zen_imported_package_ids(data: bytes) -> List[int]:
    """Package ids named INSIDE a Zen package header (ImportedPackageNames).

    UE5 unversioned Zen layout (container header v5): HasVersioningInfo u32,
    HeaderSize u32, ..., ImportedPackageNamesOffset i32 at byte 48; the names
    are an FName batch followed by one i32 number per name. PackageId =
    CityHash64(UTF-16LE(lower(name))) -- verified 127/127 against the real Leon
    store, which lists exactly these ids in the same order (texture-diagnosis
    tests/test_iostore_lib.py). Fails closed on anything else."""
    def bad(why):
        raise Refused("closest_match_package_unparsable", why)
    if len(data) < 60:
        bad("package shorter than a Zen summary")
    has_ver, header_size = struct.unpack_from("<II", data, 0)
    if has_ver != 0 or not 60 <= header_size <= len(data):
        bad("versioned or malformed Zen summary")
    off = struct.unpack_from("<i", data, 48)[0]
    if not 60 <= off < header_size:
        bad("imported package names offset outside header")
    num = struct.unpack_from("<I", data, off)[0]
    if num == 0:
        return []
    if num > 65536 or off + 16 + 10 * num > header_size:
        bad("imported package name count out of range")
    num_string_bytes = struct.unpack_from("<I", data, off + 4)[0]
    if struct.unpack_from("<Q", data, off + 8)[0] != 0xC1640000:
        bad("unknown FName hash algorithm")
    pos = off + 16 + 8 * num
    lens = struct.unpack_from(">%dh" % num, data, pos)
    pos += 2 * num
    strings_start = pos
    names = []
    for ln in lens:
        width, count = (2, ln + 32768) if ln < 0 else (1, ln)
        if pos + width * count > header_size:
            bad("imported package name overruns header")
        raw = data[pos:pos + width * count]
        try:
            names.append(raw.decode("utf-16-le" if width == 2 else "latin-1"))
        except UnicodeDecodeError:
            bad("imported package name is not valid UTF-16")
        pos += width * count
    # The batch states its own string byte total; any mismatch (including any
    # alignment padding this parser does not model) is a refusal, never a guess.
    if pos - strings_start != num_string_bytes:
        bad("imported package name bytes %d != NumStringBytes %d"
            % (pos - strings_start, num_string_bytes))
    if pos + 4 * num > header_size:
        bad("imported package name numbers overrun header")
    numbers = struct.unpack_from("<%di" % num, data, pos)
    full = [n if k == 0 else "%s_%d" % (n, k - 1) for n, k in zip(names, numbers)]
    return [cityhash64(n.lower().encode("utf-16-le")) for n in full]


def require_package_import_rewritten(mod: "ModSpec", package_id: int,
                                     from_package_id: int,
                                     to_package_id: int) -> None:
    """closest-match must not repoint ONLY the store entry.

    The package's own header names its imports (and the import map points at
    the parent's PUBLIC EXPORT HASH, a hash of the parent's export name). A
    store-only repoint leaves the package asking for the old parent's export
    inside the new parent package -> unresolvable import on the console. Until
    a DERIVED package copy with the import rewritten is supplied (see
    texture-diagnosis tools/zen_import_rewrite.py), refuse."""
    chunk_id = struct.pack("<Q", package_id) + bytes.fromhex("00000001")
    require(chunk_id in mod.toc.ids, "closest_match_package_chunk_missing",
            "0x%016x" % package_id)
    row = mod.toc.ids.index(chunk_id)
    with open(mod.ucas_path, "rb") as handle:
        store = mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ)
        try:
            data = extract_chunk_bytes(mod.toc, row, store)
        finally:
            store.close()
    ids = zen_imported_package_ids(data)
    require(to_package_id in ids and from_package_id not in ids,
            "closest_match_package_import_not_rewritten",
            "package 0x%016x still imports 0x%016x inside its own header "
            "(store-only repoint would leave an unresolvable parent import); "
            "supply a derived package copy whose import names 0x%016x"
            % (package_id, from_package_id, to_package_id))


def plan_conflicts(mods: Sequence[ModSpec],
                   hidden: Optional[Dict[str, set]] = None
                   ) -> Dict[str, object]:
    """Overlaps across mods: TOC chunk ids, store package ids, virtual paths.

    `hidden` maps mod_id -> set of package ids whose contributions are
    suppressed by substitution directives (hide action); suppressed
    contributions never conflict -- the base rows win instead.
    """
    hidden = hidden or {}
    ordered = effective_mod_order(mods)
    chunk_owner: Dict[bytes, str] = {}
    chunk_conflicts = []
    for mod in ordered:
        suppressed = hidden.get(mod.mod_id, set())
        for cid in mod.toc.ids:
            if package_id_of_chunk(cid) in suppressed:
                continue
            if cid in chunk_owner:
                chunk_conflicts.append({"chunk_id": cid.hex(),
                                        "earlier_mod": chunk_owner[cid],
                                        "later_mod": mod.mod_id})
            else:
                chunk_owner[cid] = mod.mod_id
    path_owner: Dict[str, str] = {}
    path_conflicts = []
    for mod in ordered:
        suppressed = hidden.get(mod.mod_id, set())
        for row in mod.directory["files"]:
            if package_id_of_chunk(row.chunk_id) in suppressed:
                continue
            if row.path in path_owner:
                path_conflicts.append({"path": row.path,
                                       "earlier_mod": path_owner[row.path],
                                       "later_mod": mod.mod_id})
            else:
                path_owner[row.path] = mod.mod_id
    pkg_owner: Dict[int, str] = {}
    pkg_conflicts = []
    for mod in ordered:
        suppressed = hidden.get(mod.mod_id, set())
        header = parse_container_header(read_mod_header_bytes(mod))
        for package_id in header.packages:
            if package_id in suppressed:
                continue
            if package_id in pkg_owner:
                pkg_conflicts.append({"package_id": "0x%016x" % package_id,
                                      "earlier_mod": pkg_owner[package_id],
                                      "later_mod": mod.mod_id})
            else:
                pkg_owner[package_id] = mod.mod_id
    return {"ordered_mod_ids": [m.mod_id for m in ordered],
            "chunk_id_conflicts": chunk_conflicts,
            "virtual_path_conflicts": path_conflicts,
            "package_id_conflicts": pkg_conflicts}


# ---------------------------------------------------------------------------
# Overlay assembly
# ---------------------------------------------------------------------------

def build_overlay(base_utoc_bytes: bytes,
                  base_ucas_size: int,
                  base_header_chunk: bytes,
                  mods: Sequence[ModSpec],
                  conflict_policy: str = "reject",
                  substitutions: Sequence[Dict[str, object]] = ()
                  ) -> Tuple[bytes, bytes, Dict[str, object]]:
    """Compose the derived index + sidecar partition for N mods.

    Returns (derived_utoc_bytes, sidecar_bytes, receipt). Writes nothing.
    Each mod's .ucas block stream is copied verbatim into a 16-aligned
    sidecar segment; the INDEX and the merged ContainerHeader chunk (under
    the base container's header-chunk id, in a derived tail segment) are
    the only derived artifacts (LAW).

    `substitutions` (GM classifier directives) edit DERIVED rows only:
      hide          -- drop that mod's override for the package id; the
                       base store entry, base chunk rows and base directory
                       rows survive (base export shows);
      closest-match -- rewrite the DERIVED merged store entry's import
                       from_package_id -> to_package_id (a base package).
    Mod archive bytes are never modified; every applied row is recorded in
    the receipt's substitution ledger.
    """
    require(conflict_policy in ("reject", "later-wins"),
            "invalid_conflict_policy")
    base = parse_toc(base_utoc_bytes)
    ordered = effective_mod_order(mods)
    require(ordered, "no_mods_supplied")
    by_id = {m.mod_id: m for m in ordered}

    # --- substitution validation (before any conflict/merge work)
    sub_rows = list(substitutions)
    hidden: Dict[str, set] = {}
    repoint: Dict[Tuple[str, int], Dict[str, object]] = {}
    mod_stores = {mod.mod_id: parse_container_header(read_mod_header_bytes(mod))
                  for mod in ordered}
    for row in sub_rows:
        require(row["mod_id"] in by_id, "substitution_mod_unknown",
                row["mod_id"])
        mod = by_id[row["mod_id"]]
        pid = row["package_id"]
        require(pid in mod_stores[row["mod_id"]].packages or
                any(package_id_of_chunk(cid) == pid for cid in mod.toc.ids),
                "substitution_target_not_in_mod",
                "mod %s contributes no package 0x%016x" % (row["mod_id"], pid))
        contributors = [mid for mid, store in mod_stores.items()
                        if pid in store.packages]
        if row["action"] == "closest-match":
            # a repoint applied to a losing entry would be silently
            # overwritten under later-wins -- refuse instead
            require(len(contributors) <= 1,
                    "substitution_target_conflicts_across_mods",
                    "package 0x%016x is provided by %s; under later-wins "
                    "another mod would overwrite the repointed entry"
                    % (pid, ",".join(sorted(contributors))))
        if row["action"] == "hide":
            hidden.setdefault(row["mod_id"], set()).add(pid)
        else:
            repoint[(row["mod_id"], pid)] = row
    conflicts = plan_conflicts(ordered, hidden)
    if conflict_policy == "reject" and (conflicts["chunk_id_conflicts"] or
                                        conflicts["virtual_path_conflicts"] or
                                        conflicts["package_id_conflicts"]):
        raise Refused("mod_conflict_rejected",
                      json.dumps(conflicts)[:4000])
    require(base.partition_count == 1 and base.partition_size == U64_MAX,
            "base_must_be_unbounded_single_partition")
    require(0 < base_ucas_size <= U40_MAX and base_ucas_size % 16 == 0,
            "partition_boundary_must_be_16_aligned")
    validate_single_partition(base, base_ucas_size, permit_tail_padding=True)
    header_id = base.container_id + bytes.fromhex('00000006')
    require(header_id in base.ids, "base_container_header_chunk_missing")
    base_map = {cid: row for row, cid in enumerate(base.ids)}
    require(all(base.meta(base_map[cid])[20] == 0
                for mod in ordered for cid in mod.toc.ids
                if cid in base_map),
            "memory_mapped_chunk_replacement_unsupported")

    # --- merged ContainerHeader: mod entries replace by exact PackageId in
    # effective order (later-wins); base identity/tail policy win (P5).
    # Substitutions edit derived entries only: hide skips the mod's entry
    # (base entry survives with its own shader-map hashes); closest-match
    # rewrites one import in the DERIVED entry (originals untouched).
    base_header = parse_container_header(base_header_chunk)
    merged_packages = dict(base_header.packages)
    header_receipts = []
    # per-substitution-row effects, keyed (mod_id, package_id); emitted in
    # directive order at receipt time (every validated row appears once)
    sub_effects: Dict[Tuple[str, int], Dict[str, object]] = {
        (r["mod_id"], r["package_id"]): {
            "mod_id": r["mod_id"], "package_id": "0x%016x" % r["package_id"],
            "action": r["action"],
            "base_replaced": None, "store_entry": None,
            "suppressed_chunk_ids": [], "suppressed_virtual_paths": [],
            "from_package_id": r.get("from_package_id"),
            "to_package_id": r.get("to_package_id"),
            "import_indexes": [], "store_entry_imports_before": None,
            "store_entry_imports_after": None,
        } for r in sub_rows}
    for mod in ordered:
        incoming = mod_stores[mod.mod_id]
        replaced = sorted(set(merged_packages) & set(incoming.packages))
        added = sorted(set(incoming.packages) - set(merged_packages))
        suppressed = hidden.get(mod.mod_id, set())
        for pid in replaced + added:
            effect = sub_effects.get((mod.mod_id, pid))
            if pid in suppressed:
                if effect is not None:
                    effect["base_replaced"] = pid in base_header.packages
                    effect["store_entry"] = "base-retained"
                continue
            entry = incoming.packages[pid]
            rep = repoint.get((mod.mod_id, pid))
            if rep is not None:
                require(rep["to_package_id"] in base_header.packages,
                        "substitute_not_in_base_store",
                        "0x%016x" % rep["to_package_id"])
                imports = list(entry.imported_packages)
                require(imports and rep["from_package_id"] in imports,
                        "substitution_import_not_found",
                        "mod %s store entry 0x%016x imports %s; "
                        "from_package_id 0x%016x is absent -- nothing to "
                        "repoint" % (mod.mod_id, pid,
                                     ["0x%016x" % i for i in imports],
                                     rep["from_package_id"]))
                require_package_import_rewritten(
                    mod, pid, rep["from_package_id"], rep["to_package_id"])
                indexes = [i for i, v in enumerate(imports)
                           if v == rep["from_package_id"]]
                for i in indexes:
                    imports[i] = rep["to_package_id"]
                if effect is not None:
                    effect["base_replaced"] = pid in base_header.packages
                    effect["store_entry"] = "derived-import-repoint"
                    effect["from_package_id"] = "0x%016x" % rep["from_package_id"]
                    effect["to_package_id"] = "0x%016x" % rep["to_package_id"]
                    effect["import_indexes"] = indexes
                    effect["store_entry_imports_before"] = [
                        "0x%016x" % v for v in entry.imported_packages]
                    effect["store_entry_imports_after"] = [
                        "0x%016x" % v for v in imports]
                entry = StoreEntry(tuple(imports), entry.shader_map_hashes)
            elif effect is not None:
                effect["base_replaced"] = pid in base_header.packages
                effect["store_entry"] = "mod-entry-unchanged"
            merged_packages[pid] = entry
        header_receipts.append({
            "mod_id": mod.mod_id,
            "mod_container_id": "0x%016x" % incoming.container_id,
            "mod_packages": len(incoming.packages),
            "replaced_package_ids": ["0x%016x" % p for p in replaced],
            "added_package_ids": ["0x%016x" % p for p in added],
        })
    merged_header_bytes = serialize_container_header(
        ContainerHeader(base_header.container_id, merged_packages,
                        base_header.soft_serial_region_absent,
                        base_header.opaque_zero_suffix))
    merged_header_hash = blake3_20(merged_header_bytes)

    # --- winner resolution per chunk id (later-wins across effective order);
    # each mod's OWN ContainerHeader chunk is dropped (P5 behavior: the
    # merged header lives under the BASE header id, the mod's own header row
    # never enters the derived index). Hidden package ids (substitutions)
    # contribute nothing -- the base rows win instead.
    winner: Dict[bytes, ModSpec] = {}
    for mod in ordered:
        suppressed = hidden.get(mod.mod_id, set())
        for cid in mod.toc.ids:
            if package_id_of_chunk(cid) in suppressed:
                effect = sub_effects.get((mod.mod_id,
                                          package_id_of_chunk(cid)))
                if effect is not None:
                    effect["suppressed_chunk_ids"].append(cid.hex())
                continue
            winner[cid] = mod
    mod_header_ids = {m.header_chunk_id for m in ordered}
    for cid in mod_header_ids:
        winner.pop(cid, None)
    added_ids = (set(winner) - set(base.ids))
    final_ids = list(base.ids) + sorted(added_ids)
    require(len(final_ids) < MAX_CHUNKS, "too_many_chunks_for_lookup")

    # --- sidecar layout: verbatim per-mod segments + derived header segment
    segment_offset = 0
    block_first = base.block_count
    mod_plan = []
    for mod in ordered:
        mod_plan.append({
            "mod_id": mod.mod_id, "priority": mod.priority,
            "stable_hash": mod.stable_hash,
            "segment_offset": segment_offset,
            "segment_bytes": mod.ucas_aligned_end,
            "block_first": block_first,
            "block_count": mod.toc.block_count,
            "utoc_sha256": mod.utoc_sha, "ucas_sha256": mod.ucas_sha,
            "spec": mod,
        })
        block_first += mod.toc.block_count
        segment_offset = align16(segment_offset + mod.ucas_aligned_end)
    header_segment_offset = segment_offset
    header_blocks = [merged_header_bytes[i:i + base.block_size]
                     for i in range(0, len(merged_header_bytes),
                                    base.block_size)]
    mod_blocks_total = block_first - base.block_count
    header_block_first = base.block_count + mod_blocks_total
    total_blocks = base.block_count + mod_blocks_total + len(header_blocks)
    sidecar_len = align16(header_segment_offset +
                          sum(len(b) for b in header_blocks))
    require(sidecar_len < base_ucas_size,
            "sidecar_would_require_more_than_one_new_partition")
    require(sidecar_len <= MAX_SIDECAR_BYTES, "sidecar_over_cap")

    # --- derived rows: offsets/metas (winner rows redirect to partition 1)
    offsets = bytearray()
    metas = bytearray()
    plan_by_mod = {p["mod_id"]: p for p in mod_plan}
    contributed = {m.mod_id: [] for m in ordered}
    for cid in final_ids:
        mod = winner.get(cid)
        if mod is not None:
            plan = plan_by_mod[mod.mod_id]
            row = mod.toc.ids.index(cid)
            offset, length = mod.toc.offset_length(row)
            new_offset = plan["block_first"] * base.block_size + offset
            require(new_offset <= U40_MAX and new_offset + length <= U40_MAX,
                    "new_logical_range_exceeds_40_bits")
            offsets.extend(new_offset.to_bytes(5, "big") +
                           length.to_bytes(5, "big"))
            metas.extend(mod.toc.meta(row))
            contributed[mod.mod_id].append(cid.hex())
        else:
            row = base_map[cid]
            offsets.extend(base.offset_lengths[row * OFFSET_ENTRY_SIZE:
                                               (row + 1) * OFFSET_ENTRY_SIZE])
            metas.extend(base.meta(row))
    # base header row -> merged header segment (derived tail)
    header_row = base_map[header_id]
    header_logical = header_block_first * base.block_size
    offsets[header_row * OFFSET_ENTRY_SIZE:
            (header_row + 1) * OFFSET_ENTRY_SIZE] = (
        header_logical.to_bytes(5, "big") +
        len(merged_header_bytes).to_bytes(5, "big"))
    metas[header_row * META_ENTRY_SIZE_V8:
          (header_row + 1) * META_ENTRY_SIZE_V8] = (
        merged_header_hash + b"\x00" + b"\x00" * 3)

    # --- derived block table: base verbatim + shifted mod blocks + header
    blocks = bytearray(base.block_data)
    for plan in mod_plan:
        mod = plan["spec"]
        seg = plan["segment_offset"]
        for index in range(mod.toc.block_count):
            raw = mod.toc.block_data[index * BLOCK_ENTRY_SIZE:
                                     (index + 1) * BLOCK_ENTRY_SIZE]
            local = int.from_bytes(raw[:5], "little")
            new_physical = base_ucas_size + seg + local
            require(new_physical <= U40_MAX and
                    new_physical + int.from_bytes(raw[5:8], "little") <= U40_MAX,
                    "new_physical_range_exceeds_40_bits")
            blocks.extend(new_physical.to_bytes(5, "little") + raw[5:])
    header_physical = base_ucas_size + header_segment_offset
    for block in header_blocks:
        blocks.extend(header_physical.to_bytes(5, "little") +
                      len(block).to_bytes(3, "little") +
                      len(block).to_bytes(3, "little") + b"\x00")
        header_physical += len(block)

    # --- merged directory index (later-wins on exact virtual path)
    base_dir = parse_directory_index(base.directory_index, base.ids, "base")
    for mod in ordered:
        require(mod.directory["mount_point"] == base_dir["mount_point"],
                "mount_point_mismatch", mod.mod_id)
        require(mod.directory["root_name"] == base_dir["root_name"],
                "root_name_mismatch", mod.mod_id)
    selected: Dict[str, Tuple[DirFile, str]] = {}
    for row in base_dir["files"]:
        selected[row.path] = (row, "base")
    override_ledger = []
    for mod in ordered:
        suppressed = hidden.get(mod.mod_id, set())
        for row in mod.directory["files"]:
            if package_id_of_chunk(row.chunk_id) in suppressed:
                effect = sub_effects.get(
                    (mod.mod_id, package_id_of_chunk(row.chunk_id)))
                if effect is not None:
                    effect["suppressed_virtual_paths"].append(row.path)
                continue
            if row.path in selected:
                override_ledger.append({"path": row.path,
                                        "loser": selected[row.path][1],
                                        "winner": mod.mod_id})
            selected[row.path] = (row, mod.mod_id)
    files_for_emit = [selected[path][0] for path in
                      sorted(selected, key=lambda p: p.encode("utf-8"))]
    final_row_by_id = {cid: i for i, cid in enumerate(final_ids)}
    merged_directory, dir_counts = emit_directory_index(
        base_dir["mount_point"], base_dir["root_name"],
        base_dir["directories"], files_for_emit, final_row_by_id)

    # --- assemble sidecar bytes: verbatim mod streams + merged header
    sidecar = bytearray(sidecar_len)
    for plan in mod_plan:
        mod = plan["spec"]
        with open(mod.ucas_path, "rb") as handle:
            store = mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ)
            try:
                n = plan["segment_bytes"]
                sidecar[plan["segment_offset"]:plan["segment_offset"] + n] = \
                    store[:n]
            finally:
                store.close()
    sidecar[header_segment_offset:
            header_segment_offset + len(merged_header_bytes)] = \
        merged_header_bytes

    # --- derived header (patched from the base header; P5 emit shape)
    header = bytearray(base.header)
    for offset, value in ((24, len(final_ids)), (28, total_blocks),
                          (48, len(merged_directory)), (52, 2),
                          (84, 1), (96, len(final_ids))):
        struct.pack_into("<I", header, offset, value)
    struct.pack_into("<Q", header, 88, base_ucas_size)
    seed = struct.pack("<i", -(len(final_ids) + 1))
    overflow = struct.pack("<%dI" % len(final_ids), *range(len(final_ids)))
    output = (bytes(header) + b"".join(final_ids) + bytes(offsets) +
              seed + overflow + bytes(blocks) + merged_directory +
              bytes(metas))

    # --- self-verification (parse back, lookup, byte-identity, extraction)
    derived = parse_toc(output, strict_admission=False)
    validate_lookup(derived)
    require(derived.block_data[:len(base.block_data)] == base.block_data,
            "original_block_table_changed")
    unchanged = [cid for cid in base.ids
                 if cid not in winner and cid != header_id]
    for cid in unchanged:
        row = base_map[cid]
        require(derived.offset_length(row) == base.offset_length(row) and
                derived.meta(row) == base.meta(row),
                "unaffected_chunk_mapping_changed")
    view = _OffsetView(bytes(sidecar), base_ucas_size)
    require(extract_chunk_bytes(derived, derived.ids.index(header_id), view)
            == merged_header_bytes, "merged_header_extraction_mismatch")
    for plan in mod_plan:
        mod = plan["spec"]
        # verify exactly the rows the derived index attributes to this mod
        # (winner-owned only: later-wins losers never enter the derived index)
        own_cids = [cid for cid, owner in winner.items() if owner is mod]
        with open(mod.ucas_path, "rb") as handle:
            store = mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ)
            try:
                for cid in own_cids:
                    observed = extract_chunk_bytes(
                        derived, final_ids.index(cid), view)
                    expected = extract_chunk_bytes(
                        mod.toc, mod.toc.ids.index(cid), store)
                    require(observed == expected,
                            "derived_partition_mapping_changed_payload",
                            "%s chunk %s" % (mod.mod_id, cid.hex()))
            finally:
                store.close()

    receipt = {
        "schema": 1,
        "format": "v8-indexed-unsigned-unencrypted-none-multi-mod",
        "sources": {
            "base_utoc_sha256": sha256_bytes(base_utoc_bytes),
            "base_ucas_size": base_ucas_size,
            "container_id": base.container_id.hex(),
            "mod_order": [m.mod_id for m in ordered],
            "mod_order_rule": "priority asc, stable sha256(id)[:16] asc, id "
                              "asc; later-wins",
            "conflict_policy": conflict_policy,
        },
        "counts": {
            "base_chunks": base.chunk_count,
            "final_chunks": len(final_ids),
            "base_blocks": base.block_count,
            "final_blocks": total_blocks,
            "added_chunk_ids": len(added_ids),
            "replaced_chunk_ids": len(set(winner) & set(base.ids)),
            "merged_packages": len(merged_packages),
            "merged_directory": dir_counts,
            "header_segment_blocks": len(header_blocks),
        },
        "conflicts": {k: conflicts[k] for k in
                      ("chunk_id_conflicts", "virtual_path_conflicts",
                       "package_id_conflicts")},
        "per_mod_ledger": {
            p["mod_id"]: {
                "priority": p["priority"], "stable_hash": p["stable_hash"],
                "segment_offset": p["segment_offset"],
                "segment_bytes": p["segment_bytes"],
                "blocks": p["block_count"],
                "contributed_chunk_ids": contributed[p["mod_id"]],
                "utoc_sha256": p["utoc_sha256"],
                "ucas_sha256": p["ucas_sha256"],
            } for p in mod_plan},
        "container_header_merge": {
            "base_container_id": "0x%016x" % base_header.container_id,
            "merged_packages": len(merged_packages),
            "per_mod": header_receipts,
            "merged_header_sha256": sha256_bytes(merged_header_bytes),
            "merged_header_blake3_20": merged_header_hash.hex(),
            "header_hash_algorithm": "BLAKE3 first 20 bytes (P6)",
        },
        "directory_override_ledger_count": len(override_ledger),
        "substitution_ledger": {
            "rows": len(sub_rows),
            "actions": {a: sum(1 for r in sub_rows if r["action"] == a)
                        for a in SUBSTITUTION_ACTIONS},
            "law": "derived-overlay entries only; mod archives and base "
                   "container never modified by substitutions",
            "applied": [sub_effects[(r["mod_id"], r["package_id"])]
                        for r in sub_rows],
        } if sub_rows else None,
        "byte_identity": {
            "mod_payloads_verbatim": True,
            "base_block_table_prefix_identical": True,
            "unaffected_rows_identical": True,
            "note": "sidecar = verbatim concatenation of mod .ucas block "
                    "streams (16-aligned segments) + derived merged "
                    "ContainerHeader tail segment; only the INDEX and the "
                    "merged header chunk are derived",
        },
        "output_utoc_sha256": sha256_bytes(output),
        "output_sidecar_sha256": sha256_bytes(bytes(sidecar)),
        "output_sidecar_bytes": len(sidecar),
    }
    return output, bytes(sidecar), receipt


# ---------------------------------------------------------------------------
# Transaction journal + install/uninstall/list/verify
# ---------------------------------------------------------------------------

JOURNAL_SCHEMA = 2


class Journal:
    def __init__(self, path: str, data: Optional[Dict[str, object]] = None):
        self.path = path
        self.data = data if data is not None else {
            "schema": JOURNAL_SCHEMA, "state": "empty"}

    @staticmethod
    def load(path: str) -> "Journal":
        with open(path, "r", encoding="utf-8") as handle:
            return Journal(path, json.load(handle))

    def save(self) -> None:
        tmp = self.path + ".tmp-%s" % uuid.uuid4().hex
        with open(tmp, "w", encoding="utf-8") as handle:
            json.dump(self.data, handle, indent=2, sort_keys=True)
            handle.write("\n")
        os.replace(tmp, self.path)


def journal_path_for(state_dir: str, index_name: str) -> str:
    stem = os.path.splitext(index_name)[0]
    return os.path.join(state_dir, "%s.overlay-journal.json" % stem)


def _staged(paks_dir: str, index_name: str, nonce: str, stem: str
            ) -> Dict[str, str]:
    return {
        "original": os.path.join(paks_dir, index_name),
        "backup": os.path.join(paks_dir,
                               "%s.original-%s.bak" % (index_name, nonce)),
        "index_temp": os.path.join(paks_dir,
                                   "%s.overlay-%s.tmp" % (index_name, nonce)),
        "sidecar": os.path.join(paks_dir, "%s_s1.ucas" % stem),
        "sidecar_temp": os.path.join(paks_dir,
                                     "%s_s1.ucas.%s.tmp" % (stem, nonce)),
    }


def _mod_entry(spec: str, priority: Optional[int]) -> Dict[str, object]:
    if "=" in spec:
        mod_id, path = spec.split("=", 1)
    else:
        path = spec
        mod_id = os.path.splitext(os.path.basename(path))[0]
    require(os.path.isfile(path), "mod_utoc_missing", path)
    ucas = os.path.splitext(path)[0] + ".ucas"
    require(os.path.isfile(ucas), "mod_ucas_missing", ucas)
    return {"mod_id": mod_id, "kind": "iostore", "utoc_path": os.path.abspath(path),
            "ucas_path": os.path.abspath(ucas),
            "priority": priority if priority is not None else 0,
            "utoc_sha256": sha256_file(path), "ucas_sha256": sha256_file(ucas)}


def _classic_entry(spec: str, priority: Optional[int]) -> Dict[str, object]:
    if "=" in spec:
        mod_id, path = spec.split("=", 1)
    else:
        path = spec
        mod_id = os.path.splitext(os.path.basename(path))[0]
        if mod_id.endswith(".pak"):
            mod_id = mod_id[:-4]
    require(os.path.isfile(path), "classic_pak_source_missing", path)
    return {"mod_id": mod_id, "kind": "classic-pak",
            "pak_path": os.path.abspath(path),
            "pak_sha256": sha256_file(path),
            "priority": priority if priority is not None else 0}


def _classic_mod_spec(entry: Dict[str, object], staging_dir: str,
                      retoc_tool: Optional[str],
                      retoc_version: str) -> Tuple[ModSpec, Dict[str, object]]:
    """Materialize a classic-pak journal/CLI entry into an admitted mod spec.

    Reuses the staged trio when its recorded shas still match (the pinned
    conversion is deterministic, so reuse == reconvert); otherwise converts
    via the pinned local tool, which must be supplied.
    """
    mod_id = entry["mod_id"]
    pak_path = entry["pak_path"]
    if os.path.isfile(pak_path):
        require(sha256_file(pak_path) == entry["pak_sha256"],
                "classic_pak_source_changed", pak_path)
    staged = {k: entry.get(k) for k in ("staged_utoc_path", "staged_ucas_path",
                                        "utoc_sha256", "ucas_sha256")}
    reusable = (staged["staged_utoc_path"] and staged["staged_ucas_path"] and
                os.path.isfile(staged["staged_utoc_path"]) and
                os.path.isfile(staged["staged_ucas_path"]) and
                sha256_file(staged["staged_utoc_path"]) == staged["utoc_sha256"] and
                sha256_file(staged["staged_ucas_path"]) == staged["ucas_sha256"])
    if reusable:
        spec = ModSpec(mod_id, staged["staged_utoc_path"],
                       staged["staged_ucas_path"], entry.get("priority", 0))
        provenance = {k: entry[k] for k in entry
                      if k not in ("utoc_path", "ucas_path")}
        return spec, provenance
    require(retoc_tool is not None,
            "classic_pak_conversion_tool_unavailable",
            "no staged trio for %s and no --retoc-tool given. The pinned "
            "local retoc binary (retoc-v0.1.5, verified deterministic) "
            "performs the offline pak->zen conversion; pass its path via "
            "--retoc-tool. Pure-python asset re-serialization is refused by "
            "the never-fabricate law (UNSUPPORTED.md section 7)." % mod_id)
    spec, provenance = convert_classic_pak(
        pak_path, mod_id, retoc_tool,
        entry.get("retoc_version", retoc_version), staging_dir)
    provenance["priority"] = entry.get("priority", 0)
    return spec, provenance


def _materialize_mods(mod_specs: Dict[str, Dict[str, object]],
                      staging_dir: str, retoc_tool: Optional[str],
                      retoc_version: str
                      ) -> Tuple[List[ModSpec], Dict[str, Dict[str, object]]]:
    specs: List[ModSpec] = []
    provenance: Dict[str, Dict[str, object]] = {}
    for entry in mod_specs.values():
        if entry.get("kind") == "classic-pak":
            spec, prov = _classic_mod_spec(entry, staging_dir, retoc_tool,
                                           retoc_version)
        else:
            spec = ModSpec(entry["mod_id"], entry["utoc_path"],
                           entry["ucas_path"], entry.get("priority", 0))
            prov = dict(entry)
        specs.append(spec)
        provenance[entry["mod_id"]] = prov
    return specs, provenance


def _filter_substitutions(rows: List[Dict[str, object]],
                          mod_ids: Sequence[str]
                          ) -> List[Dict[str, object]]:
    keep = set(mod_ids)
    return [r for r in rows if r["mod_id"] in keep]


def _resolve_base_ucas_size(args, stem: str) -> Tuple[Optional[str], int]:
    base_ucas = os.path.join(os.path.abspath(args.paks_dir), stem + ".ucas")
    size = None
    if os.path.isfile(base_ucas):
        size = align16(os.path.getsize(base_ucas))
        require(size == os.path.getsize(base_ucas),
                "base_ucas_not_16_aligned", base_ucas)
    if args.base_ucas_size is not None:
        require(size in (None, args.base_ucas_size), "base_ucas_size_conflict",
                "on-disk %s vs given %s" % (size, args.base_ucas_size))
        size = args.base_ucas_size
    require(size is not None, "base_ucas_size_unknown",
            "no local %s.ucas; pass --base-ucas-size (pinned from records)"
            % stem)
    return (base_ucas if os.path.isfile(base_ucas) else None), size


def do_install(args, journal_file: str) -> Dict[str, object]:
    paks_dir = os.path.abspath(args.paks_dir)
    index_path = os.path.join(paks_dir, args.index)
    stem = os.path.splitext(args.index)[0]
    require(os.path.isfile(index_path), "base_index_missing", index_path)
    base_ucas_path, base_ucas_size = _resolve_base_ucas_size(args, stem)
    with open(index_path, "rb") as handle:
        live_bytes = handle.read()
    live_sha = sha256_bytes(live_bytes)

    journal = None
    if os.path.exists(journal_file):
        journal = Journal.load(journal_file)
        # STALE detection: the live index must be the journal's derived index
        # or its recorded original (game-update hazard).
        if live_sha not in (journal.data.get("derived", {}).get("index_sha256"),
                            journal.data.get("base", {}).get("index_sha256")):
            raise Refused(
                "stale_journal_state",
                "live index sha %s matches neither the journal's derived "
                "index nor its recorded original -- a game update or foreign "
                "write replaced it. Restore the original from %s (verify its "
                "sha) or reconcile manually before continuing."
                % (live_sha, journal.data["paths"]["backup"]))
        # The base identity always comes from the pristine backup, never from
        # the derived live index (whose header row points into the sidecar).
        backup_path = journal.data["paths"]["backup"]
        require(os.path.isfile(backup_path), "journal_backup_missing",
                backup_path)
        with open(backup_path, "rb") as handle:
            base_bytes = handle.read()
        base_sha = sha256_bytes(base_bytes)
        require(base_sha == journal.data["base"]["index_sha256"],
                "journal_backup_drifted", backup_path)
    elif args.expect_existing:
        raise Refused("journal_missing", journal_file)
    else:
        base_bytes = live_bytes
        base_sha = live_sha
    base_toc = parse_toc(base_bytes)

    mod_specs: Dict[str, Dict[str, object]] = {}
    if journal is not None:
        for entry in journal.data.get("mods", []):
            mod_specs[entry["mod_id"]] = entry
    for spec in args.mod:
        entry = _mod_entry(spec, args.priority)
        prior = mod_specs.get(entry["mod_id"])
        if prior is not None:
            require(prior.get("kind", "iostore") == "iostore",
                    "mod_kind_changed", entry["mod_id"])
            require(prior["utoc_sha256"] == entry["utoc_sha256"] and
                    prior["ucas_sha256"] == entry["ucas_sha256"],
                    "mod_identity_changed", entry["mod_id"])
            if args.priority is not None:
                entry = dict(prior)
                entry["priority"] = args.priority
        mod_specs[entry["mod_id"]] = entry
    for spec in args.classic_pak:
        entry = _classic_entry(spec, args.priority)
        prior = mod_specs.get(entry["mod_id"])
        if prior is not None:
            require(prior.get("kind") == "classic-pak",
                    "mod_kind_changed", entry["mod_id"])
            require(prior["pak_sha256"] == entry["pak_sha256"],
                    "mod_identity_changed", entry["mod_id"])
            if args.priority is not None:
                entry = dict(prior)
                entry["priority"] = args.priority
        mod_specs[entry["mod_id"]] = entry
    for mod_id in args.remove_mod:
        require(mod_id in mod_specs, "unknown_mod_id", mod_id)
        del mod_specs[mod_id]
    require(mod_specs, "no_mods_to_install")
    staging_dir = os.path.join(os.path.dirname(journal_file), "classic-staging")
    specs, provenance = _materialize_mods(
        mod_specs, staging_dir, args.retoc_tool, args.retoc_version)
    ordered_provenance = sorted(provenance.values(),
                                key=lambda e: e["mod_id"])

    substitution_rows = []
    substitution_sha = None
    if args.substitutions:
        substitution_rows = parse_substitutions(args.substitutions)
        substitution_sha = sha256_file(args.substitutions)
    elif journal is not None and journal.data.get("substitutions"):
        # retain the journal's directives across re-installs (filtered to
        # the surviving mod set; rows for removed mods drop silently)
        prior = journal.data["substitutions"]
        substitution_rows = _filter_substitutions(prior.get("rows") or [],
                                                  sorted(mod_specs))
        substitution_sha = prior.get("sha256")
    for row in substitution_rows:
        require(row["mod_id"] in mod_specs, "substitution_mod_unknown",
                row["mod_id"])

    header_chunk = read_base_header_chunk(base_toc, base_ucas_path,
                                          args.base_container_header)
    derived_utoc, sidecar, receipt = build_overlay(
        base_bytes, base_ucas_size, header_chunk, specs, args.conflict,
        substitution_rows)

    nonce = uuid.uuid4().hex
    paths = _staged(paks_dir, args.index, nonce, stem)
    plan = {"kind": "plan", "nonce": nonce, "paths": dict(paths),
            "derived_index_sha256": sha256_bytes(derived_utoc),
            "derived_index_bytes": len(derived_utoc),
            "sidecar_sha256": sha256_bytes(sidecar),
            "sidecar_bytes": len(sidecar),
            "mods": sorted(mod_specs)}
    if args.dry_run:
        return {"result": "dry-run", "plan": plan,
                "counts": receipt["counts"]}

    # verify-by-sha around every mutation (console-transaction mirror)
    require(sha256_file(index_path) == live_sha,
            "base_index_changed_during_plan")
    if journal is not None:
        backup_path = journal.data["paths"]["backup"]
        require(os.path.isfile(backup_path) and
                sha256_file(backup_path) == base_sha,
                "journal_backup_missing_or_drifted", backup_path)
    mutations = []
    with open(paths["sidecar_temp"], "wb") as handle:
        handle.write(sidecar)
    require(sha256_file(paths["sidecar_temp"]) == plan["sidecar_sha256"],
            "sidecar_temp_sha_mismatch")
    with open(paths["index_temp"], "wb") as handle:
        handle.write(derived_utoc)
    require(sha256_file(paths["index_temp"]) ==
            plan["derived_index_sha256"], "index_temp_sha_mismatch")
    if journal is None:
        os.replace(index_path, paths["backup"])
        mutations.append({"kind": "rename", "name": "backup_original_index",
                          "source": paths["original"],
                          "destination": paths["backup"]})
        require(sha256_file(paths["backup"]) == base_sha, "backup_sha_mismatch")
    os.replace(paths["sidecar_temp"], paths["sidecar"])
    mutations.append({"kind": "rename", "name": "publish_sidecar",
                      "source": paths["sidecar_temp"],
                      "destination": paths["sidecar"]})
    os.replace(paths["index_temp"], index_path)
    mutations.append({"kind": "rename", "name": "publish_derived_index",
                      "source": paths["index_temp"],
                      "destination": paths["original"]})

    journal_data = {
        "schema": JOURNAL_SCHEMA,
        "state": "installed",
        "updated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "base": {"paks_dir": paks_dir, "index": args.index,
                 "index_sha256": base_sha, "index_bytes": len(base_bytes),
                 "ucas_size": base_ucas_size,
                 "container_id": base_toc.container_id.hex()},
        "mods": ordered_provenance,
        "substitutions": (
            {"sha256": substitution_sha, "rows": substitution_rows}
            if substitution_rows else None),
        "derived": {"index_sha256": plan["derived_index_sha256"],
                    "index_bytes": plan["derived_index_bytes"],
                    "sidecar_sha256": plan["sidecar_sha256"],
                    "sidecar_bytes": plan["sidecar_bytes"]},
        "paths": {
            # an existing journal keeps its real backup path: a re-install
            # does not rename the original again
            "backup": (journal.data["paths"]["backup"] if journal is not None
                       else paths["backup"]),
            "original": paths["original"],
            "sidecar": paths["sidecar"],
        },
        "acknowledged_mutations": mutations,
        "overlay_receipt": receipt,
    }
    Journal(journal_file, journal_data).save()
    return {"result": "installed", "plan": plan, "journal": journal_file}


def do_uninstall(args, journal_file: str) -> Dict[str, object]:
    require(os.path.exists(journal_file), "journal_missing", journal_file)
    journal = Journal.load(journal_file)
    data = journal.data
    paks_dir = os.path.abspath(args.paks_dir)
    index_path = os.path.join(paks_dir, args.index)
    stem = os.path.splitext(args.index)[0]
    live_sha = sha256_file(index_path) if os.path.exists(index_path) else None
    derived_sha = data.get("derived", {}).get("index_sha256")
    original_sha = data.get("base", {}).get("index_sha256")
    if live_sha not in (derived_sha, original_sha):
        return {"result": "stale", "code": "stale_journal_state",
                "detail": "live index sha %s matches neither the journal's "
                          "derived index nor its original -- a game update "
                          "or foreign write replaced it. Manual "
                          "reconciliation required (backup at %s)."
                          % (live_sha, data["paths"]["backup"])}
    mods = {e["mod_id"]: e for e in data.get("mods", [])}
    for mod_id in args.mod:
        require(mod_id in mods, "unknown_mod_id", mod_id)
    remaining = ({k: v for k, v in mods.items() if k not in set(args.mod)}
                 if args.mod else {})
    paths = data["paths"]
    if args.dry_run:
        if not remaining:
            return {"result": "dry-run",
                    "would": "uninstall-to-pristine",
                    "restored_index_sha256": original_sha}
        return {"result": "dry-run", "would": "uninstall-rederive",
                "remaining_mods": sorted(remaining)}

    if not remaining:
        if live_sha == derived_sha:
            require(os.path.isfile(paths["backup"]) and
                    sha256_file(paths["backup"]) == original_sha,
                    "backup_sha_mismatch_refusing_restore")
        os.replace(paths["backup"], index_path)
        require(sha256_file(index_path) == original_sha, "restore_sha_mismatch")
        if os.path.exists(paths["sidecar"]):
            os.remove(paths["sidecar"])
        staging = os.path.join(os.path.dirname(journal_file),
                               "classic-staging")
        if os.path.isdir(staging):
            import shutil
            shutil.rmtree(staging)
        os.remove(journal_file)
        return {"result": "uninstalled-to-pristine",
                "restored_index_sha256": original_sha,
                "removed_sidecar": paths["sidecar"],
                "journal_removed": journal_file}

    if live_sha == original_sha:
        require(os.path.isfile(paths["backup"]) and
                sha256_file(paths["backup"]) == original_sha,
                "backup_sha_mismatch")
        os.replace(index_path, paths["backup"])
    staging_dir = os.path.join(os.path.dirname(journal_file), "classic-staging")
    specs, _ = _materialize_mods(remaining, staging_dir, args.retoc_tool,
                                 args.retoc_version)
    journal_subs = (data.get("substitutions") or {}).get("rows") or []
    substitution_rows = _filter_substitutions(journal_subs,
                                              sorted(remaining))
    with open(paths["backup"], "rb") as handle:
        base_bytes = handle.read()
    base_toc = parse_toc(base_bytes)
    base_ucas_path = os.path.join(paks_dir, stem + ".ucas")
    header_chunk = read_base_header_chunk(
        base_toc, base_ucas_path if os.path.isfile(base_ucas_path) else None,
        args.base_container_header)
    derived_utoc, sidecar, receipt = build_overlay(
        base_bytes, data["base"]["ucas_size"], header_chunk, specs,
        args.conflict, substitution_rows)
    nonce = uuid.uuid4().hex
    temp_paths = _staged(paks_dir, args.index, nonce, stem)
    with open(temp_paths["sidecar_temp"], "wb") as handle:
        handle.write(sidecar)
    with open(temp_paths["index_temp"], "wb") as handle:
        handle.write(derived_utoc)
    os.replace(temp_paths["sidecar_temp"], paths["sidecar"])
    os.replace(temp_paths["index_temp"], index_path)
    data["mods"] = sorted(remaining.values(), key=lambda e: e["mod_id"])
    data["derived"] = {"index_sha256": sha256_bytes(derived_utoc),
                       "index_bytes": len(derived_utoc),
                       "sidecar_sha256": sha256_bytes(sidecar),
                       "sidecar_bytes": len(sidecar)}
    data["state"] = "installed"
    data["updated_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    if data.get("substitutions"):
        data["substitutions"]["rows"] = substitution_rows
        if not substitution_rows:
            data["substitutions"] = None
    data["overlay_receipt"] = receipt
    journal.save()
    return {"result": "uninstalled-rederived",
            "remaining_mods": sorted(remaining),
            "derived_index_sha256": data["derived"]["index_sha256"]}


def do_list(args, journal_file: str) -> Dict[str, object]:
    if not os.path.exists(journal_file):
        return {"result": "no-journal", "paks_dir": args.paks_dir,
                "index": args.index}
    data = Journal.load(journal_file).data
    mods = []
    for entry in data.get("mods", []):
        row = {"mod_id": entry["mod_id"],
               "kind": entry.get("kind", "iostore"),
               "priority": entry.get("priority", 0)}
        if row["kind"] == "classic-pak":
            row["pak_sha256"] = entry["pak_sha256"]
            row["utoc_sha256"] = entry.get("utoc_sha256")
        else:
            row["utoc_sha256"] = entry["utoc_sha256"]
        mods.append(row)
    subs = data.get("substitutions")
    return {"result": "listed", "state": data.get("state"),
            "base": data.get("base"),
            "mods": mods,
            "substitutions": ({"sha256": subs.get("sha256"),
                               "rows": len(subs.get("rows") or [])}
                              if subs else None),
            "derived": data.get("derived")}


def do_verify(args, journal_file: str) -> Dict[str, object]:
    if not os.path.exists(journal_file):
        return {"result": "no-journal", "verdict": "NOT_INSTALLED"}
    data = Journal.load(journal_file).data
    index_path = os.path.join(os.path.abspath(args.paks_dir), args.index)
    live_sha = sha256_file(index_path) if os.path.exists(index_path) else None
    derived_sha = data["derived"]["index_sha256"]
    original_sha = data["base"]["index_sha256"]
    backup_path = data["paths"]["backup"]
    backup_sha = (sha256_file(backup_path)
                  if os.path.exists(backup_path) else None)
    sidecar_path = data["paths"]["sidecar"]
    sidecar_sha = (sha256_file(sidecar_path)
                   if os.path.exists(sidecar_path) else None)
    checks = [
        {"check": "live_index_is_derived", "ok": live_sha == derived_sha},
        {"check": "backup_original_intact", "ok": backup_sha == original_sha},
        {"check": "sidecar_matches_journal",
         "ok": sidecar_sha == data["derived"]["sidecar_sha256"]},
    ]
    for entry in data.get("mods", []):
        if entry.get("kind") == "classic-pak":
            ok = (os.path.isfile(entry.get("staged_utoc_path", "")) and
                  os.path.isfile(entry.get("staged_ucas_path", "")) and
                  sha256_file(entry["staged_utoc_path"]) == entry["utoc_sha256"] and
                  sha256_file(entry["staged_ucas_path"]) == entry["ucas_sha256"])
            source_ok = (os.path.isfile(entry["pak_path"]) and
                         sha256_file(entry["pak_path"]) == entry["pak_sha256"])
            checks.append({"check": "classic_source_%s" % entry["mod_id"],
                           "ok": source_ok})
            checks.append({"check": "classic_staged_%s" % entry["mod_id"],
                           "ok": ok})
            continue
        ok = (os.path.isfile(entry["utoc_path"]) and
              sha256_file(entry["utoc_path"]) == entry["utoc_sha256"] and
              os.path.isfile(entry["ucas_path"]) and
              sha256_file(entry["ucas_path"]) == entry["ucas_sha256"])
        checks.append({"check": "mod_source_%s" % entry["mod_id"], "ok": ok})
    stale = live_sha is not None and live_sha not in (derived_sha, original_sha)
    verdict = ("STALE" if stale else
               "OK" if all(c["ok"] for c in checks) else "DRIFT")
    return {"result": "verified", "verdict": verdict, "checks": checks}


def do_inspect(args) -> Dict[str, object]:
    with open(args.utoc, "rb") as handle:
        data = handle.read()
    return {"result": "inspected", "file": args.utoc, "bytes": len(data),
            "sha256": sha256_bytes(data), "toc": inspect_toc(data)}


def do_inspect_pak(args) -> Dict[str, object]:
    with open(args.pak, "rb") as handle:
        data = handle.read()
    return {"result": "inspected", "file": args.pak, "bytes": len(data),
            "sha256": sha256_bytes(data), "pak": inspect_pak(data)}


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Game-agnostic IoStore multi-mod overlay builder "
                    "(agent GK, Claude round-6 item 3a).")
    parser.add_argument("--paks-dir", required=True,
                        help="game dump paks directory (base index/ucas "
                             "read here; derived outputs land here)")
    parser.add_argument("--index", required=True,
                        help="base container index file name (a .utoc)")
    parser.add_argument("--state-dir", default=None,
                        help="journal directory (default "
                             "<paks>/.overlay-state)")
    parser.add_argument("--base-ucas-size", type=int, default=None,
                        help="authoritative base .ucas size when the file "
                             "is not present locally (pinned from records)")
    parser.add_argument("--base-container-header", default=None,
                        help="pre-extracted base ContainerHeader chunk file "
                             "(bounded ranged read from the .ucas otherwise)")
    parser.add_argument("--conflict", choices=("reject", "later-wins"),
                        default="reject",
                        help="policy when two mods override the same "
                             "package id (default: reject with a report)")
    parser.add_argument("--priority", type=int, default=None,
                        help="explicit priority for mods named by --mod "
                             "(lower = earlier = weaker; later-wins)")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--expect-existing", action="store_true",
                        help="refuse if no journal exists yet")
    parser.add_argument("--mod", action="append", default=[],
                        metavar="[ID=]PATH.utoc",
                        help="mod archive TOC (sibling .ucas required); "
                             "repeatable")
    parser.add_argument("--classic-pak", action="append", default=[],
                        metavar="[ID=]PATH.pak",
                        help="classic PAK v11 mod (unencrypted, all-stored, "
                             "cooked split packages only); converted via "
                             "the pinned retoc tool into an IoStore trio "
                             "before the overlay merge; repeatable")
    parser.add_argument("--retoc-tool", default=None, metavar="PATH",
                        help="path to the pinned local retoc binary used "
                             "for --classic-pak conversions (required "
                             "whenever a fresh conversion is needed)")
    parser.add_argument("--retoc-version", default=DEFAULT_RETOC_VERSION,
                        choices=RETOC_VERSIONS,
                        help="engine version passed to the conversion tool "
                             "(default: %s)" % DEFAULT_RETOC_VERSION)
    parser.add_argument("--substitutions", default=None, metavar="FILE",
                        help="L1/L2 shader-fallback substitution directives "
                             "JSON (hide / closest-match rows; the GM "
                             "classifier output shape); applied to DERIVED "
                             "overlay entries only, recorded in the journal")
    parser.add_argument("--remove-mod", action="append", default=[],
                        metavar="ID", help="mod ids to remove this install")
    parser.add_argument("--utoc", default=None,
                        help="inspect target (inspect command only)")
    parser.add_argument("--pak", default=None,
                        help="classic PAK inspect target (inspect-pak "
                             "command only)")
    parser.add_argument("command", choices=("install", "uninstall", "list",
                                            "verify", "inspect",
                                            "inspect-pak"))
    args = parser.parse_args(argv)

    state_dir = os.path.abspath(
        args.state_dir or os.path.join(args.paks_dir, ".overlay-state"))
    journal_file = journal_path_for(state_dir, args.index)
    try:
        if args.command == "inspect":
            require(args.utoc is not None, "inspect_requires_utoc")
            result = do_inspect(args)
        elif args.command == "inspect-pak":
            require(args.pak is not None, "inspect_pak_requires_pak")
            result = do_inspect_pak(args)
        elif args.command == "install":
            os.makedirs(state_dir, exist_ok=True)
            result = do_install(args, journal_file)
        elif args.command == "uninstall":
            result = do_uninstall(args, journal_file)
        elif args.command == "list":
            result = do_list(args, journal_file)
        else:
            result = do_verify(args, journal_file)
    except Refused as error:
        print(json.dumps({"result": "refused", "code": error.code,
                          "detail": error.detail}, indent=2))
        return 2
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
