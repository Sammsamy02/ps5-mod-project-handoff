"""Artificial PE/CLI format fixtures, never compiled or executable proof.

The fixture uses literal ECMA table layouts and signatures independently of
the parser. Each negative case catches admission of the named bad contract.
"""
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

import assembly_preflight as preflight


def fixture(*, pe64=False, convention=0x201, library="__Internal",
            reply_type=0x10, run_public=True, body=True, bad_owner=False,
            duplicate_map=False, extra_table=False, wide_heaps=False, out_reply=True):
    strings = bytearray(b"\0")
    def string(value):
        index = len(strings)
        strings.extend(value.encode() + b"\0")
        return index
    names = {s: string(s) for s in (
        "Probe.dll", "<Module>", "Probe", "ModBridge.Cap10", "AbiVersion",
        "RuntimeProbe", "Run", "ThrowForHostBoundaryTest", library,
        "modbridge_cap10_abi_version", "modbridge_cap10_runtime_probe",
        "Probe", "mscorlib", "cookie", "reply")}
    blobs = bytearray(b"\0")
    def blob(value):
        index = len(blobs)
        blobs.extend(bytes([len(value)]) + value)
        return index
    signatures = [blob(b"\0\0\x08"),
                  blob(bytes([0, 2, 8, 8, reply_type, 8])),
                  blob(b"\0\x01\x08\x08"), blob(b"\0\0\x01")]
    heap = "I" if wide_heaps else "H"
    def row(fmt, *values):
        return struct.pack("<" + fmt.replace("s", heap).replace("b", heap), *values)
    tables = {
        0: [row("HsHHH", 0, names["Probe.dll"], 1, 0, 0)],
        2: [row("IssHHH", 0, names["<Module>"], 0, 0, 1, 2 if bad_owner else 1),
            row("IssHHH", 0x100181, names["Probe"], names["ModBridge.Cap10"],
                0, 1, 2 if bad_owner else 1)],
        6: [row("IHHsbH", 0, 0x80, 0x2091, names["AbiVersion"], signatures[0], 1),
            row("IHHsbH", 0, 0x80, 0x2091, names["RuntimeProbe"], signatures[1], 1),
            row("IHHsbH", 0x2200 if body else 0, 0, 0x96 if run_public else 0x91,
                names["Run"], signatures[2], 3),
            row("IHHsbH", 0x2210, 0, 0x96, names["ThrowForHostBoundaryTest"], signatures[3], 4)],
        8: [row("HHs", 0, 1, names["cookie"]),
            row("HHs", 2 if out_reply else 0, 2, names["reply"]),
            row("HHs", 0, 1, names["cookie"])],
        26: [row("s", names[library])],
        28: [row("HHsH", convention, 3, names["modbridge_cap10_abi_version"], 1),
             row("HHsH", convention, 3 if duplicate_map else 5,
                 names["modbridge_cap10_runtime_probe"], 1)],
        32: [row("IHHHHIbss", 0x8004, 1, 0, 0, 0, 0, 0, names["Probe"], 0)],
        35: [row("HHHHIbssb", 4, 0, 0, 0, 0, 0, names["mscorlib"], 0, 0)],
    }
    if extra_table:
        tables[42] = [b"\0" * 8]  # unsupported GenericParam; must refuse
    valid = sum(1 << t for t in tables)
    stream = struct.pack("<IBBBBQQ", 0, 2, 0, 5 if wide_heaps else 0, 1, valid, 0)
    stream += b"".join(struct.pack("<I", len(tables[t])) for t in sorted(tables))
    stream += b"".join(b"".join(tables[t]) for t in sorted(tables))
    streams = [("#~", stream), ("#Strings", bytes(strings)), ("#Blob", bytes(blobs)),
               ("#GUID", b"\x01" * 16), ("#US", b"\0")]
    metadata = bytearray(struct.pack("<IHHII", 0x424A5342, 1, 1, 0, 12) + b"v4.0.30319\0\0")
    metadata.extend(struct.pack("<HH", 0, len(streams)))
    header_size = len(metadata) + sum(8 + ((len(n) + 1 + 3) & ~3) for n, _ in streams)
    offset = header_size
    payload = bytearray()
    for name, data in streams:
        metadata.extend(struct.pack("<II", offset, len(data)))
        encoded = name.encode() + b"\0"
        metadata.extend(encoded + b"\0" * (-len(encoded) % 4))
        payload.extend(data)
        offset += len(data)
        padding = -offset % 4
        payload.extend(b"\0" * padding)
        offset += padding
    metadata.extend(payload)
    optional_size = 240 if pe64 else 224
    image = bytearray(0x1000)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 0x3C, 0x80)
    image[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", image, 0x84, 0x8664 if pe64 else 0x14C, 1, 0, 0, 0,
                     optional_size, 0x2002)
    optional = 0x98
    struct.pack_into("<H", image, optional, 0x20B if pe64 else 0x10B)
    struct.pack_into("<I", image, optional + (108 if pe64 else 92), 16)
    struct.pack_into("<II", image, optional + (112 if pe64 else 96) + 14 * 8, 0x2000, 72)
    section = optional + optional_size
    image[section:section + 8] = b".text\0\0\0"
    struct.pack_into("<IIII", image, section + 8, 0xE00, 0x2000, 0xE00, 0x200)
    struct.pack_into("<IHHIIII", image, 0x200, 72, 2, 5, 0x2400, len(metadata), 1, 0)
    image[0x400:0x403] = b"\x0a\x16\x2a"  # tiny IL body: ldc.i4.0; ret
    image[0x410:0x412] = b"\x06\x2a"  # tiny IL body: ret (semantics unverified)
    image[0x600:0x600 + len(metadata)] = metadata
    return bytes(image)


class AssemblyPreflightTests(unittest.TestCase):
    def test_reads_contract_from_pe32_metadata_without_execution_claim(self):
        data = fixture()
        result = preflight.inspect(data)
        self.assertEqual(result["verdict"], "METADATA_CONTRACT_MATCH")
        self.assertEqual(result["assembly_sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual(result["type"], "ModBridge.Cap10.Probe")
        self.assertEqual(result["imports"], ["modbridge_cap10_abi_version", "modbridge_cap10_runtime_probe"])
        self.assertEqual(result["assembly_references"], ["mscorlib"])
        self.assertFalse(result["managed_execution_proven"])
        self.assertFalse(result["il_semantics_verified"])

    def test_pe32_plus_and_wide_heap_indexes(self):
        self.assertEqual(preflight.inspect(fixture(pe64=True, wide_heaps=True))["verdict"],
                         "METADATA_CONTRACT_MATCH")

    def test_rejects_wrong_pinvoke_call_convention(self):
        with self.assertRaisesRegex(ValueError, "calling convention"):
            preflight.inspect(fixture(convention=0x300))

    def test_rejects_missing_exact_spelling_metadata_bit(self):
        with self.assertRaisesRegex(ValueError, "ExactSpelling"):
            preflight.inspect(fixture(convention=0x200))

    def test_rejects_external_native_library(self):
        with self.assertRaisesRegex(ValueError, "__Internal"):
            preflight.inspect(fixture(library="other"))

    def test_rejects_non_byref_reply(self):
        with self.assertRaisesRegex(ValueError, "signature"):
            preflight.inspect(fixture(reply_type=0x0F))

    def test_rejects_reply_without_out_metadata(self):
        with self.assertRaisesRegex(ValueError, "parameter"):
            preflight.inspect(fixture(out_reply=False))

    def test_rejects_private_entry_and_absent_il_body(self):
        for kwargs in ({"run_public": False}, {"body": False}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                preflight.inspect(fixture(**kwargs))

    def test_rejects_method_owned_by_other_type(self):
        with self.assertRaisesRegex(ValueError, "owner|method set"):
            preflight.inspect(fixture(bad_owner=True))

    def test_rejects_duplicate_import_map(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            preflight.inspect(fixture(duplicate_map=True))

    def test_rejects_unsupported_tables_instead_of_guessing_stride(self):
        with self.assertRaisesRegex(ValueError, "unsupported metadata table"):
            preflight.inspect(fixture(extra_table=True))

    def test_rejects_unbacked_metadata_rva_and_truncation(self):
        bad = bytearray(fixture())
        struct.pack_into("<I", bad, 0x208, 0x9000)
        for data in (bytes(bad), fixture()[:0x700], b"not a managed assembly"):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                preflight.inspect(data)

    def test_rejects_overlapping_streams(self):
        bad = bytearray(fixture())
        # Metadata+32 has #~ header; +44 begins #Strings header.
        struct.pack_into("<I", bad, 0x600 + 44, struct.unpack_from("<I", bad, 0x600 + 32)[0])
        with self.assertRaisesRegex(ValueError, "overlap"):
            preflight.inspect(bytes(bad))

    def test_rejects_unaligned_metadata_stream(self):
        bad = bytearray(fixture())
        start, size = struct.unpack_from("<II", bad, 0x600 + 80)
        struct.pack_into("<II", bad, 0x600 + 80, start + 1, size - 1)
        with self.assertRaisesRegex(ValueError, "alignment"):
            preflight.inspect(bytes(bad))

    def test_cli_hash_admission_and_refusal_leave_input_unchanged(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as tmp:
            path = Path(tmp) / "artificial-format-fixture.dll"
            data = fixture()
            path.write_bytes(data)
            for expected, code in ((hashlib.sha256(data).hexdigest(), 0), ("0" * 64, 1)):
                result = subprocess.run([sys.executable, "-B", str(Path(preflight.__file__)),
                                         str(path), "--sha256", expected],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, code, result.stderr)
                self.assertIn('"managed_execution_proven": false', result.stdout)
            self.assertEqual(path.read_bytes(), data)
            self.assertEqual(list(Path(tmp).iterdir()), [path])


if __name__ == "__main__":
    unittest.main()
