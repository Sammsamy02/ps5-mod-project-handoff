"""Hand-built ELF fixtures exercise rejection, without executing payloads."""
import importlib.util
import pathlib
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

def fixture(symbol="_start", undefined=False, constructor=False, needed=False, syscall=False):
    data = bytearray(1024)
    ident = b"\x7fELF\x02\x01\x01" + b"\x00" * 9
    data[:64] = struct.pack("<16sHHIQQQIHHHHHH", ident, 3, 62, 1, 256, 64, 512, 0, 64, 56, 1, 64, 7, 4)
    data[64:120] = struct.pack("<IIQQQQQQ", 1, 5, 0, 0, 0, 1024, 1024, 16384)
    data[256:259] = b"\x0f\x05\xc3" if syscall else b"\x90\x90\xc3"
    strings = b"\x00" + symbol.encode() + b"\x00"
    data[272:272+len(strings)] = strings
    data[344:368] = struct.pack("<IBBHQQ", 1, 0x12, 0, 0 if undefined else 1, 256, 3)
    names = b"\x00.text\x00.strtab\x00.symtab\x00.shstrtab\x00.init_array\x00.dynamic\x00"
    data[400:400+len(names)] = names
    sections = [
        (0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (1, 1, 6, 256, 256, 3, 0, 0, 1, 0),
        (7, 3, 0, 0, 272, len(strings), 0, 0, 1, 0),
        (15, 2, 0, 0, 320, 48, 2, 1, 8, 24),
        (23, 3, 0, 0, 400, len(names), 0, 0, 1, 0),
        (33, 14, 3, 384, 384, 8 if constructor else 0, 0, 0, 8, 8),
        (45, 6, 3, 464, 464, 32 if needed else 0, 2, 0, 8, 16),
    ]
    if needed:
        data[464:496] = struct.pack("<qQqQ", 1, 1, 0, 0)
    for i, section in enumerate(sections):
        data[512+i*64:576+i*64] = struct.pack("<IIQQQQIIQQ", *section)
    return bytes(data)

def second_load(address, memory_size, file_offset=0, file_size=0):
    data = bytearray(fixture())
    struct.pack_into("<H", data, 56, 2)
    struct.pack_into("<IIQQQQQQ", data, 120, 1, 6, file_offset, address,
                     address, file_size, memory_size, 16384)
    return bytes(data)

class VerifierTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = ROOT / "verify_elf.py"
        if path.is_file():
            spec = importlib.util.spec_from_file_location("verify_elf", path)
            cls.verifier = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(cls.verifier)

    def test_verifier_exists(self):
        self.assertTrue((ROOT / "verify_elf.py").is_file(), "native ELF acceptance check is not implemented")

    def test_accepts_independent_minimal_native_elf(self):
        if not hasattr(self, "verifier"): self.skipTest("implementation absent")
        result = self.verifier.verify_bytes(fixture())
        self.assertEqual(result["machine"], "x86_64")
        self.assertEqual(result["entry"], 256)

    def test_accepts_disjoint_page_aligned_loads(self):
        result = self.verifier.verify_bytes(second_load(0x4000, 0x4000))
        self.assertTrue(result["load_pages_disjoint"])
        self.assertTrue(result["load_starts_aligned"])
        self.assertTrue(result["load_address_ranges_bounded"])

    def test_rejects_unaligned_nonempty_load_start(self):
        with self.assertRaisesRegex(ValueError, "load segment start"):
            self.verifier.verify_bytes(second_load(0x100, 16, file_offset=0x100))

    def test_rejects_shared_load_pages(self):
        with self.assertRaisesRegex(ValueError, "load segment pages overlap"):
            self.verifier.verify_bytes(second_load(0, 16))

    def test_rejects_uint64_load_address_and_page_end_overflow(self):
        for size in (0x8000, 0x3fff):
            with self.subTest(memory_size=size):
                with self.assertRaisesRegex(ValueError, "load address range"):
                    self.verifier.verify_bytes(second_load(0xffffffffffffc000, size))

    def test_rejects_desktop_and_truncated_inputs(self):
        if not hasattr(self, "verifier"): self.skipTest("implementation absent")
        for data in [b"\xcf\xfa\xed\xfe" + b"\x00"*100, b"\x7fELF", fixture()[:600]]:
            with self.assertRaises(ValueError): self.verifier.verify_bytes(data)

    def test_rejects_imports_constructors_and_privilege_helpers(self):
        if not hasattr(self, "verifier"): self.skipTest("implementation absent")
        for data in [fixture(symbol="write", undefined=True), fixture(constructor=True), fixture(needed=True), fixture(symbol="__patch_init"), fixture(symbol="kernel_set_ucred_caps"), fixture(syscall=True)]:
            with self.assertRaises(ValueError): self.verifier.verify_bytes(data)

    def test_rejects_wrong_machine_alignment_and_non_executable_entry(self):
        if not hasattr(self, "verifier"): self.skipTest("implementation absent")
        for offset, value, fmt in [(18, 183, "H"), (112, 4096, "Q"), (68, 4, "I"), (24, 2048, "Q")]:
            data = bytearray(fixture()); struct.pack_into("<"+fmt, data, offset, value)
            with self.assertRaises(ValueError): self.verifier.verify_bytes(bytes(data))

    def test_rejects_hidden_program_header_behavior_and_wrong_entry_symbol(self):
        if not hasattr(self, "verifier"): self.skipTest("implementation absent")
        mutations = []
        for encoding in (b"\x0f\x05", b"\x0f\x34", b"\xcd\x80"):
            data = bytearray(fixture()); data[300:302] = encoding; mutations.append(data)
        data = bytearray(fixture())
        struct.pack_into("<H", data, 56, 2)
        struct.pack_into("<IIQQQQQQ", data, 120, 2, 4, 464, 464, 464, 32, 32, 8)
        struct.pack_into("<qQqQ", data, 464, 1, 1, 0, 0)
        mutations.append(data)
        data = bytearray(fixture()); struct.pack_into("<Q", data, 352, 300); mutations.append(data)
        data = bytearray(fixture()); data[348] = 0x11; mutations.append(data) # OBJECT instead of FUNC
        for data in mutations:
            with self.assertRaises(ValueError): self.verifier.verify_bytes(bytes(data))

    def test_rejects_dynamic_callbacks_and_packed_relocations(self):
        if not hasattr(self, "verifier"): self.skipTest("implementation absent")
        for tag in (12, 13, 25, 26, 27, 28, 32, 33, 35, 36, 37):
            data = bytearray(fixture())
            struct.pack_into("<Q", data, 512+6*64+32, 32)
            struct.pack_into("<qQqQ", data, 464, tag, 8, 0, 0)
            with self.assertRaises(ValueError): self.verifier.verify_bytes(bytes(data))

if __name__ == "__main__": unittest.main()
