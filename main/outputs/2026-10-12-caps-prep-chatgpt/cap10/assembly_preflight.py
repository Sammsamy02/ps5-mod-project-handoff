"""Bounded, read-only PE/CLI Probe contract reader; no loading or execution.

Only the explicitly listed metadata tables are supported. This is neither a
general assembly verifier nor an IL verifier. See ASSEMBLY-PREFLIGHT.md.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

MAX_BYTES = 1024 * 1024
MAX_ROWS = 4096

# Table schemas use heap, simple-table and coded-table index widths from II.24.
# An unsupported present table is refused before calculating any row offsets.
SCHEMAS = {
    0: (2, "s", "g", "g", "g"),
    1: ("ResolutionScope", "s", "s"),
    2: (4, "s", "s", "TypeDefOrRef", 4 + 100, 6 + 100),
    6: (4, 2, 2, "s", "b", 8 + 100),
    8: (2, 2, "s"),
    10: ("MemberRefParent", "s", "b"),
    12: ("HasCustomAttribute", "CustomAttributeType", "b"),
    17: ("b",),
    26: ("s",),
    27: ("b",),
    28: (2, "MemberForwarded", "s", 26 + 100),
    32: (4, 2, 2, 2, 2, 4, "b", "s", "s"),
    35: (2, 2, 2, 2, 4, "b", "s", "s", "b"),
}
CODED = {
    "ResolutionScope": (2, (0, 26, 35, 1)),
    "TypeDefOrRef": (2, (2, 1, 27)),
    "MemberRefParent": (3, (2, 1, 26, 6, 27)),
    "HasCustomAttribute": (5, (6, 4, 1, 2, 8, 9, 10, 0, 14, 23, 20, 17,
                                26, 27, 32, 35, 38, 39, 40, 42, 44, 43)),
    "CustomAttributeType": (3, (6, 10)),
    "MemberForwarded": (1, (4, 6)),
}


def checked(data, start, size):
    if start < 0 or size < 0 or start + size > len(data):
        raise ValueError("truncated or out-of-bounds structure")
    return data[start:start + size]


def number(data, start, size):
    return int.from_bytes(checked(data, start, size), "little")


def pe_metadata(data):
    if not 64 <= len(data) <= MAX_BYTES or data[:2] != b"MZ":
        raise ValueError("expected bounded PE assembly (64 B to 1 MiB)")
    pe = number(data, 0x3C, 4)
    if checked(data, pe, 4) != b"PE\0\0":
        raise ValueError("invalid PE signature")
    count, opt_size = number(data, pe + 6, 2), number(data, pe + 20, 2)
    if not 1 <= count <= 96 or not number(data, pe + 22, 2) & 0x2000:
        raise ValueError("expected DLL with bounded section count")
    optional = checked(data, pe + 24, opt_size)
    magic = number(optional, 0, 2)
    if magic not in (0x10B, 0x20B):
        raise ValueError("unsupported PE optional header")
    dirs = 96 if magic == 0x10B else 112
    if number(optional, dirs - 4, 4) < 15:
        raise ValueError("no CLR directory")
    clr_rva, clr_size = struct.unpack("<II", checked(optional, dirs + 14 * 8, 8))
    sections = []
    for i in range(count):
        section = checked(data, pe + 24 + opt_size + i * 40, 40)
        virtual_size, rva, raw_size, raw = struct.unpack_from("<IIII", section, 8)
        checked(data, raw, raw_size)
        sections.append((rva, max(virtual_size, raw_size), raw, raw_size))
    def rva_bytes(rva, size):
        matches = [raw + rva - va for va, mapped, raw, physical in sections
                   if va <= rva and rva - va + size <= min(mapped, physical)]
        if len(matches) != 1:
            raise ValueError("RVA is unbacked or ambiguously mapped")
        return checked(data, matches[0], size)
    if clr_size < 72:
        raise ValueError("truncated CLR header")
    clr = rva_bytes(clr_rva, 72)
    if number(clr, 0, 4) != 72:
        raise ValueError("unsupported CLR header size")
    flags, entry = number(clr, 16, 4), number(clr, 20, 4)
    if not flags & 1 or flags & (2 | 0x10 | 0x20000) or entry or any(clr[64:72]):
        raise ValueError("expected IL-only library without native entry/header or forced 32-bit")
    metadata = rva_bytes(number(clr, 8, 4), number(clr, 12, 4))
    return metadata, rva_bytes, "PE32" if magic == 0x10B else "PE32+"


def metadata_streams(data):
    if number(data, 0, 4) != 0x424A5342:
        raise ValueError("invalid CLI metadata signature")
    length = number(data, 12, 4)
    if not 1 <= length <= 256:
        raise ValueError("invalid runtime version length")
    version = checked(data, 16, length).split(b"\0")[0].decode("ascii")
    pos = (16 + length + 3) & ~3
    if number(data, pos, 2):
        raise ValueError("unsupported metadata flags")
    count = number(data, pos + 2, 2)
    if not 3 <= count <= 8:
        raise ValueError("invalid stream count")
    pos += 4
    ranges = {}
    for _ in range(count):
        start, size = number(data, pos, 4), number(data, pos + 4, 4)
        if start % 4:
            raise ValueError("metadata stream alignment mismatch")
        name_start = pos + 8
        end = data.find(b"\0", name_start, min(name_start + 32, len(data)))
        if end < 0:
            raise ValueError("unterminated stream name")
        name = data[name_start:end].decode("ascii")
        if name in ranges or name not in ("#~", "#Strings", "#Blob", "#GUID", "#US"):
            raise ValueError("duplicate/unsupported metadata stream")
        checked(data, start, size)
        ranges[name] = (start, size)
        pos = (end + 4) & ~3
    spans = sorted(ranges.values())
    if any(start < pos for start, size in spans):
        raise ValueError("stream overlaps metadata header")
    if any(a + n > b for (a, n), (b, _) in zip(spans, spans[1:])):
        raise ValueError("overlapping metadata streams")
    if not {"#~", "#Strings", "#Blob"} <= ranges.keys():
        raise ValueError("missing required metadata streams")
    return {k: checked(data, *v) for k, v in ranges.items()}, version


def table_rows(data):
    if number(data, 0, 4) or number(data, 4, 1) != 2 or number(data, 5, 1):
        raise ValueError("unsupported tables stream version")
    heap_flags = number(data, 6, 1)
    if heap_flags & ~7:
        raise ValueError("unsupported heap flags")
    valid = number(data, 8, 8)
    checked(data, 0, 24)
    counts, pos = {}, 24
    for t in range(64):
        if valid >> t & 1:
            if t not in SCHEMAS:
                raise ValueError("unsupported metadata table " + str(t))
            counts[t] = number(data, pos, 4)
            pos += 4
    if sum(counts.values()) > MAX_ROWS:
        raise ValueError("metadata row budget exceeded")
    def width(kind):
        if isinstance(kind, int):
            return kind if kind < 100 else (4 if counts.get(kind - 100, 0) >= 65536 else 2)
        if kind in ("s", "g", "b"):
            return 4 if heap_flags & {"s": 1, "g": 2, "b": 4}[kind] else 2
        bits, tables = CODED[kind]
        return 4 if max(counts.get(t, 0) for t in tables) >= 1 << (16 - bits) else 2
    rows = {}
    for t, count in counts.items():
        widths = [width(kind) for kind in SCHEMAS[t]]
        checked(data, pos, count * sum(widths))
        rows[t] = []
        for _ in range(count):
            row = []
            for size in widths:
                row.append(number(data, pos, size))
                pos += size
            rows[t].append(row)
    if any(data[pos:]):
        raise ValueError("unparsed nonzero metadata table bytes")
    return rows


def inspect(data):
    metadata, rva_bytes, pe_kind = pe_metadata(data)
    streams, version = metadata_streams(metadata)
    rows = table_rows(streams["#~"])
    strings, blobs = streams["#Strings"], streams["#Blob"]
    if not strings or strings[0] or not blobs or blobs[0]:
        raise ValueError("heap zero entries missing")
    def text(index):
        checked(strings, index, 1)
        end = strings.find(b"\0", index, min(index + 1024, len(strings)))
        if end < 0:
            raise ValueError("unterminated/overlong string")
        return strings[index:end].decode("utf-8")
    def blob(index):
        first = number(blobs, index, 1)
        if first < 0x80:
            size, prefix = first, 1
        elif first < 0xC0:
            size, prefix = ((first & 0x3F) << 8) | number(blobs, index + 1, 1), 2
        elif first < 0xE0:
            size = ((first & 0x1F) << 24) | int.from_bytes(checked(blobs, index + 1, 3), "big")
            prefix = 4
        else:
            raise ValueError("invalid blob length prefix")
        return checked(blobs, index + prefix, size)
    types, methods = rows.get(2, []), rows.get(6, [])
    wanted = [(i, t) for i, t in enumerate(types)
              if text(t[1]) == "Probe" and text(t[2]) == "ModBridge.Cap10"]
    if len(wanted) != 1:
        raise ValueError("missing/ambiguous probe type")
    i, probe_type = wanted[0]
    if probe_type[0] & 7 != 1 or probe_type[0] & 0x180 != 0x180:
        raise ValueError("probe must be public static class")
    starts = [t[5] for t in types] + [len(methods) + 1]
    if not starts or starts[0] != 1 or starts != sorted(starts) or any(not 1 <= n <= len(methods) + 1 for n in starts):
        raise ValueError("invalid method owner ranges")
    owned = {n: methods[n - 1] for n in range(starts[i], starts[i + 1])}
    expected = {
        "AbiVersion": (b"\0\0\x08", 1, True),
        "RuntimeProbe": (b"\0\x02\x08\x08\x10\x08", 1, True),
        "Run": (b"\0\x01\x08\x08", 6, False),
        "ThrowForHostBoundaryTest": (b"\0\0\x01", 6, False),
    }
    if len(owned) != 4 or {text(m[3]) for m in owned.values()} != set(expected) or len(methods) != 4:
        raise ValueError("probe method set/owner mismatch")
    parameters = rows.get(8, [])
    param_starts = [m[5] for m in methods] + [len(parameters) + 1]
    if param_starts[0] != 1 or param_starts != sorted(param_starts) or any(
            not 1 <= n <= len(parameters) + 1 for n in param_starts):
        raise ValueError("invalid parameter owner ranges")
    by_row = {}
    for n, method in owned.items():
        rva, impl, flags, name, signature, _ = method
        name = text(name)
        sig, access, pinvoke = expected[name]
        if blob(signature) != sig:
            raise ValueError("signature mismatch: " + name)
        actual_params = [(p[1], p[0]) for p in
                         parameters[param_starts[n - 1] - 1:param_starts[n] - 1]]
        expected_params = {"AbiVersion": [], "RuntimeProbe": [(1, 0), (2, 2)],
                           "Run": [(1, 0)], "ThrowForHostBoundaryTest": []}[name]
        if actual_params != expected_params:
            raise ValueError("parameter sequence/out/marshaling mismatch: " + name)
        if flags & 7 != access or not flags & 0x10 or bool(flags & 0x2000) != pinvoke or flags & (0x400 | 0x40):
            raise ValueError("method access/static/implementation mismatch: " + name)
        if impl != (0x80 if pinvoke else 0):
            raise ValueError("unsupported method implementation: " + name)
        if pinvoke and rva:
            raise ValueError("PInvoke method must not have IL body")
        if not pinvoke:
            if not rva:
                raise ValueError("managed method missing IL body")
            first = rva_bytes(rva, 1)[0]
            if first & 3 == 2:
                code_size, header_size = first >> 2, 1
            elif first & 3 == 3:
                header = rva_bytes(rva, 12)
                header_flags = number(header, 0, 2)
                header_size, code_size = (header_flags >> 12) * 4, number(header, 4, 4)
                if header_size != 12 or header_flags & 8:
                    raise ValueError("unsupported IL body header/extra sections")
            else:
                raise ValueError("invalid IL body header")
            if not 1 <= code_size <= 65536:
                raise ValueError("IL body size out of bounds")
            rva_bytes(rva, header_size + code_size)
        by_row[n] = name
    scopes = rows.get(26, [])
    if len(scopes) != 1 or text(scopes[0][0]) != "__Internal":
        raise ValueError("only __Internal native library is permitted")
    maps, seen, imports = rows.get(28, []), set(), []
    for flags, member, name, scope in maps:
        n = member >> 1
        if member & 1 != 1 or n not in by_row:
            raise ValueError("invalid PInvoke method reference")
        if n in seen:
            raise ValueError("duplicate PInvoke map")
        seen.add(n)
        if flags & 0x700 != 0x200 or flags & ~0x207:
            raise ValueError("PInvoke calling convention/mapping flags mismatch")
        if not flags & 0x1:
            raise ValueError("PInvoke ExactSpelling/NoMangle metadata bit required")
        entry = {"AbiVersion": "modbridge_cap10_abi_version",
                 "RuntimeProbe": "modbridge_cap10_runtime_probe"}.get(by_row[n])
        if scope != 1 or entry is None or text(name) != entry:
            raise ValueError("PInvoke entry or scope mismatch")
        imports.append(entry)
    if len(maps) != 2 or seen != {n for n, name in by_row.items() if name in ("AbiVersion", "RuntimeProbe")}:
        raise ValueError("exactly two PInvoke maps required")
    assemblies = rows.get(32, [])
    if len(assemblies) != 1 or not text(assemblies[0][7]):
        raise ValueError("one named assembly required")
    references = sorted(text(a[6]) for a in rows.get(35, []))
    return {"verdict": "METADATA_CONTRACT_MATCH", "label": "Mac-only static metadata",
            "assembly_sha256": hashlib.sha256(data).hexdigest(), "assembly_bytes": len(data),
            "assembly_name": text(assemblies[0][7]), "type": "ModBridge.Cap10.Probe",
            "pe_kind": pe_kind, "metadata_runtime_version": version,
            "imports": sorted(imports), "assembly_references": references,
            "managed_execution_proven": False, "il_semantics_verified": False,
            "runtime_abi_proven": False, "corlib_compatibility_proven": False,
            "full_assembly_validity_verified": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("assembly", type=Path)
    parser.add_argument("--sha256", required=True, help="expected compiled-assembly hash")
    args = parser.parse_args()
    try:
        with args.assembly.open("rb") as source:
            data = source.read(MAX_BYTES + 1)
        if len(data) > MAX_BYTES or hashlib.sha256(data).hexdigest() != args.sha256.lower():
            raise ValueError("assembly size/hash mismatch")
        result = inspect(data)
    except (ValueError, OSError, UnicodeError) as exc:
        print(json.dumps({"verdict": "REFUSED", "reason": str(exc), "managed_execution_proven": False}))
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
