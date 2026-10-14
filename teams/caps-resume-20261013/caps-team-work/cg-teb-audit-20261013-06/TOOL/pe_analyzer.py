#!/usr/bin/env python3
"""pe_analyzer.py -- format-aware discovery for Windows PE images (.dll/.exe/.sys).

Capability #13 (run downloaded PC DLL mods unchanged on PS5), step 2.

Parses a PE file and reports:
  * architecture (x86/x64/ARM64/...), format (PE32/PE32+), subsystem, DLL characteristics
  * managed vs native detection (CLR data directory + ECMA-335 metadata walk:
    runtime version, metadata version string, module name, AssemblyRef table)
  * native: import directory (imported DLLs + functions, incl. delay-load) and
    export directory (names, ordinals, forwarders)
  * compatibility classification: native-x64 / managed-dotnet / unsupported-arch / unknown
  * UE4SS-specific dependencies (imports of UE4SS.dll and which functions;
    exports of the UE4SS native-mod entry point 'start_mod')

Stdlib only (struct/json/hashlib/argparse). Python 3.9+. Mac-safe: never executes
the analyzed image, only reads bytes.

Usage:
    pe_analyzer.py FILE [FILE ...] [--json] [--out FILE] [--quiet]
"""

import argparse
import hashlib
import json
import struct
import sys
import uuid
from typing import Any, Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Constants (Microsoft PE format + ECMA-335)
# ---------------------------------------------------------------------------

MACHINE_NAMES = {
    0x0000: "UNKNOWN",
    0x014C: "x86",            # IMAGE_FILE_MACHINE_I386
    0x0162: "riscv? (0x162)",
    0x01C0: "arm",            # ARM little endian
    0x01C4: "armnt",          # ARM Thumb-2
    0x0200: "ia64",
    0x8664: "x64",            # IMAGE_FILE_MACHINE_AMD64
    0x9064: "arm64? (0x9064)",
    0xAA64: "arm64",          # IMAGE_FILE_MACHINE_ARM64
    0x5032: "riscv32",
    0x5064: "riscv64",
    0x6264: "loongarch? (0x6264)",
    0xA641: "arm64x",         # ARM64X (chameleon) pe
}

SUBSYSTEM_NAMES = {
    0: "UNKNOWN", 1: "NATIVE", 2: "WINDOWS_GUI", 3: "WINDOWS_CUI",
    5: "OS2_CUI", 7: "POSIX_CUI", 9: "WINDOWS_CE_GUI",
    10: "EFI_APPLICATION", 11: "EFI_BOOT_SERVICE_DRIVER",
    12: "EFI_RUNTIME_DRIVER", 13: "EFI_ROM", 16: "XBOX",
    17: "WINDOWS_BOOT_APPLICATION",
}

DLL_CHARACTERISTIC_NAMES = [
    (0x0020, "HIGH_ENTROPY_VA"),
    (0x0040, "DYNAMIC_BASE"),          # ASLR
    (0x0080, "FORCE_INTEGRITY"),
    (0x0100, "NX_COMPAT"),             # DEP
    (0x0200, "NO_ISOLATION"),
    (0x0400, "NO_SEH"),
    (0x0800, "NO_BIND"),
    (0x1000, "APPCONTAINER"),
    (0x2000, "GUARD_CF"),
    (0x4000, "TERMINAL_SERVER_AWARE"),
]

FILE_CHARACTERISTIC_NAMES = [
    (0x0001, "RELOCS_STRIPPED"), (0x0002, "EXECUTABLE_IMAGE"),
    (0x0004, "LINE_NUMS_STRIPPED"), (0x0008, "LOCAL_SYMS_STRIPPED"),
    (0x0010, "AGGRESIVE_WS_TRIM"), (0x0020, "LARGE_ADDRESS_AWARE"),
    (0x0040, "16BIT? (0x40)"), (0x0080, "BYTES_REVERSED_LO"),
    (0x0100, "32BIT_MACHINE"), (0x0200, "DEBUG_STRIPPED"),
    (0x0400, "REMOVABLE_RUN_FROM_SWAP"), (0x0800, "NET_RUN_FROM_SWAP"),
    (0x1000, "SYSTEM"), (0x2000, "DLL"), (0x4000, "UP_SYSTEM_ONLY"),
    (0x8000, "BYTES_REVERSED_HI"),
]

DATA_DIRECTORY_NAMES = [
    "EXPORT", "IMPORT", "RESOURCE", "EXCEPTION", "SECURITY", "BASERELOC",
    "DEBUG", "ARCHITECTURE", "GLOBALPTR", "TLS", "LOAD_CONFIG",
    "BOUND_IMPORT", "IAT", "DELAY_IMPORT", "CLR_RUNTIME_HEADER", "RESERVED",
]

# ECMA-335 metadata table ids -> name (the ones with defined row schemas here)
METADATA_TABLE_NAMES = {
    0x00: "Module", 0x01: "TypeRef", 0x02: "TypeDef", 0x03: "FieldPtr",
    0x04: "Field", 0x05: "MethodPtr", 0x06: "MethodDef", 0x07: "ParamPtr",
    0x08: "Param", 0x09: "InterfaceImpl", 0x0A: "MemberRef",
    0x0B: "Constant", 0x0C: "CustomAttribute", 0x0D: "FieldMarshal",
    0x0E: "DeclSecurity", 0x0F: "ClassLayout", 0x10: "FieldLayout",
    0x11: "StandAloneSig", 0x12: "EventMap", 0x13: "EventPtr",
    0x14: "Event", 0x15: "PropertyMap", 0x16: "PropertyPtr",
    0x17: "Property", 0x18: "MethodSemantics", 0x19: "MethodImpl",
    0x1A: "ModuleRef", 0x1B: "TypeSpec", 0x1C: "ImplMap", 0x1D: "FieldRVA",
    0x1E: "EncLog", 0x1F: "EncMap", 0x20: "Assembly", 0x21: "AssemblyProcessor",
    0x22: "AssemblyOS", 0x23: "AssemblyRef", 0x24: "AssemblyRefProcessor",
    0x25: "AssemblyRefOS", 0x26: "File", 0x27: "ExportedType",
    0x28: "ManifestResource", 0x29: "NestedClass", 0x2A: "GenericParam",
    0x2B: "MethodSpec", 0x2C: "GenericParamConstraint", 0x30: "Document",
    0x31: "MethodDebugInformation", 0x32: "LocalScope",
    0x33: "LocalVariable", 0x34: "LocalConstant", 0x35: "ImportScope",
    0x36: "StateMachineMethod", 0x37: "CustomDebugInformation",
}

CLR_FLAG_ILONLY = 0x00000001
CLR_FLAG_32BITREQUIRED = 0x00000002
CLR_FLAG_ILLIBRARY = 0x00000008
CLR_FLAG_STRONGNAMESIGNED = 0x00000008  # different bit field group; informational only
CLR_FLAG_NATIVE_ENTRYPOINT = 0x00000010
CLR_FLAG_32BITPREFERRED = 0x00020000

# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------


class PEFormatError(Exception):
    pass


class PeImage(object):
    """Parsed view over one PE file. Collects diagnostics instead of raising."""

    def __init__(self, data: bytes, path: str):
        self.data = data
        self.path = path
        self.warnings: List[str] = []
        self.diagnostics: List[str] = []
        self.dos: Dict[str, Any] = {}
        self.coff: Dict[str, Any] = {}
        self.optional: Dict[str, Any] = {}
        self.sections: List[Dict[str, Any]] = []
        self.data_dirs: List[Dict[str, Any]] = []
        self.imports: List[Dict[str, Any]] = []
        self.delay_imports: List[Dict[str, Any]] = []
        self.exports: Dict[str, Any] = {}
        self.tls: Dict[str, Any] = {"status": "none"}
        self.gs_access: Dict[str, Any] = {"status": "not_scanned", "candidate_count": 0, "sites": []}
        self.debug: Dict[str, Any] = {"status": "none", "codeview": []}
        self.managed: Optional[Dict[str, Any]] = None

    def warn(self, msg: str) -> None:
        self.warnings.append(msg)

    def diag(self, msg: str) -> None:
        self.diagnostics.append(msg)

    def u8(self, off: int) -> int:
        return self.data[off]

    def u16(self, off: int) -> int:
        return struct.unpack_from("<H", self.data, off)[0]

    def u32(self, off: int) -> int:
        return struct.unpack_from("<I", self.data, off)[0]

    def u64(self, off: int) -> int:
        return struct.unpack_from("<Q", self.data, off)[0]

    def cstr(self, off: int, max_len: int = 4096) -> str:
        """NUL-terminated ASCII/UTF-8 string at file offset."""
        if off < 0 or off >= len(self.data):
            return "<out-of-bounds @0x%x>" % off
        end = self.data.find(b"\x00", off, off + max_len)
        if end < 0:
            end = min(off + max_len, len(self.data))
        raw = self.data[off:end]
        try:
            return raw.decode("utf-8", "replace")
        except Exception:
            return repr(raw)

    # -- layout mapping ----------------------------------------------------

    def rva_to_offset(self, rva: int) -> Optional[int]:
        """Map RVA to file offset via the section table."""
        if rva < 0:
            return None
        for s in self.sections:
            va = s["virtual_address"]
            vsize = max(s["virtual_size"], s["raw_size"])
            if va <= rva < va + vsize:
                delta = rva - va
                if delta < s["raw_size"]:
                    return s["raw_ptr"] + delta
                return None  # points into zero-filled virtual tail
        # Headers region (before first section)
        if self.sections and rva < self.sections[0]["virtual_address"]:
            return rva
        if not self.sections and rva < self.optional.get("size_of_headers", 0):
            return rva
        return None

    def read_at_rva(self, rva: int, size: int) -> Optional[bytes]:
        if rva < 0 or size < 0:
            return None
        off = self.rva_to_offset(rva)
        if off is None or off + size > len(self.data):
            return None
        for section in self.sections:
            start = section["virtual_address"]
            extent = max(section["virtual_size"], section["raw_size"])
            if start <= rva < start + extent:
                if size > section["raw_size"] - (rva - start):
                    return None
                break
        else:
            header_end = self.optional.get("size_of_headers", 0)
            if self.sections:
                header_end = min(header_end, self.sections[0]["virtual_address"])
            if size > header_end - rva:
                return None
        return self.data[off:off + size]

    # -- header parsing ----------------------------------------------------

    def parse(self) -> bool:
        if len(self.data) < 0x40 or self.data[:2] != b"MZ":
            self.diag("not an MZ image (missing DOS 'MZ' magic)")
            return False
        try:
            self._parse_headers()
        except (PEFormatError, struct.error, IndexError):
            self.diag("truncated or malformed PE headers")
            return False
        return True

    def _parse_headers(self) -> None:
        e_lfanew = self.u32(0x3C)
        self.dos = {"e_magic": "MZ", "e_lfanew": e_lfanew}
        if e_lfanew + 4 > len(self.data) or self.data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
            self.diag("missing 'PE\\0\\0' signature at e_lfanew=0x%x" % e_lfanew)
            raise PEFormatError("bad PE signature")

        coff_off = e_lfanew + 4
        (machine, num_sections, tstamp, _sym_ptr, _num_syms,
         opt_size, characteristics) = struct.unpack_from("<HHIIIHH", self.data, coff_off)
        self.coff = {
            "machine": machine,
            "machine_name": MACHINE_NAMES.get(machine, "0x%04X" % machine),
            "number_of_sections": num_sections,
            "time_date_stamp": tstamp,
            "size_of_optional_header": opt_size,
            "characteristics": characteristics,
            "characteristics_names": self._flag_names(characteristics, FILE_CHARACTERISTIC_NAMES),
            "is_dll": bool(characteristics & 0x2000),
        }

        opt_off = coff_off + 20
        if opt_size == 0:
            self.diag("no optional header (object file?)")
            raise PEFormatError("no optional header")
        magic = self.u16(opt_off)
        if magic == 0x20B:
            fmt = "PE32+"
        elif magic == 0x10B:
            fmt = "PE32"
        else:
            self.diag("unknown optional-header magic 0x%04X" % magic)
            raise PEFormatError("bad magic")

        o = self.optional
        o["magic"] = magic
        o["format"] = fmt
        is_pe32p = magic == 0x20B
        o["linker_version"] = "%d.%d" % (self.u8(opt_off + 2), self.u8(opt_off + 3))
        o["size_of_code"] = self.u32(opt_off + 4)
        o["address_of_entry_point"] = self.u32(opt_off + 16)
        o["base_of_code"] = self.u32(opt_off + 20)
        if is_pe32p:
            o["image_base"] = self.u64(opt_off + 24)
            base = 24 + 8
        else:
            o["image_base"] = self.u32(opt_off + 28)
            base = 24 + 4 + 4  # base_of_data + 4-byte image base
        (o["section_alignment"], o["file_alignment"]) = struct.unpack_from("<II", self.data, opt_off + base)
        o["size_of_image"] = self.u32(opt_off + base + 24)
        o["size_of_headers"] = self.u32(opt_off + base + 28)
        o["checksum"] = self.u32(opt_off + base + 32)
        o["subsystem"] = self.u16(opt_off + base + 36)
        o["subsystem_name"] = SUBSYSTEM_NAMES.get(o["subsystem"], "0x%X" % o["subsystem"])
        dllchar = self.u16(opt_off + base + 38)
        o["dll_characteristics"] = dllchar
        o["dll_characteristics_names"] = self._flag_names(dllchar, DLL_CHARACTERISTIC_NAMES)

        num_dirs_off = opt_off + (108 if is_pe32p else 92)
        num_dirs = self.u32(num_dirs_off)
        o["number_of_rva_and_sizes"] = num_dirs
        dirs_off = num_dirs_off + 4
        self.data_dirs = []
        for i in range(num_dirs):
            if dirs_off + i * 8 + 8 > len(self.data):
                self.warn("data directory table truncated at index %d" % i)
                break
            rva, size = struct.unpack_from("<II", self.data, dirs_off + i * 8)
            name = DATA_DIRECTORY_NAMES[i] if i < len(DATA_DIRECTORY_NAMES) else "DIR_%d" % i
            self.data_dirs.append({"index": i, "name": name, "rva": rva, "size": size})

        # Sections
        sec_off = opt_off + opt_size
        self.sections = []
        for i in range(num_sections):
            so = sec_off + i * 40
            if so + 40 > len(self.data):
                self.warn("section table truncated at index %d" % i)
                break
            name_raw = self.data[so:so + 8].rstrip(b"\x00")
            vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", self.data, so + 8)
            chars = self.u32(so + 36)
            self.sections.append({
                "name": name_raw.decode("utf-8", "replace"),
                "virtual_size": vsize,
                "virtual_address": va,
                "raw_size": rawsize,
                "raw_ptr": rawptr,
                "characteristics": chars,
                "readable": bool(chars & 0x40000000),
                "writable": bool(chars & 0x80000000),
                "executable": bool(chars & 0x20000000),
            })

    @staticmethod
    def _flag_names(value: int, table: List[Tuple[int, str]]) -> List[str]:
        return [name for bit, name in table if value & bit]

    # -- imports / exports ---------------------------------------------------

    def parse_imports(self) -> None:
        imp_dir = self._dir(1)
        if not imp_dir or imp_dir["size"] == 0:
            self.diag("no import directory")
            return
        self.imports = self._parse_import_descriptors(imp_dir["rva"], delay=False)

    def parse_delay_imports(self) -> None:
        d_dir = self._dir(13)
        if not d_dir or d_dir["size"] == 0:
            return
        self.delay_imports = self._parse_import_descriptors(d_dir["rva"], delay=True)

    def _dir(self, index: int) -> Optional[Dict[str, Any]]:
        if index < len(self.data_dirs):
            return self.data_dirs[index]
        return None

    def parse_tls(self) -> None:
        """Classify PE static TLS without executing callbacks or mapping the image."""
        directory = self._dir(9)
        if not directory or (directory["rva"] == 0 and directory["size"] == 0):
            return

        def malformed(reason: str) -> None:
            self.tls = {"status": "malformed", "reason": reason}
            self.warn("TLS directory: " + reason)

        is_64 = self.optional.get("format") == "PE32+"
        layout = "<QQQQII" if is_64 else "<IIIIII"
        size = struct.calcsize(layout)
        if directory["rva"] == 0 or directory["size"] < size:
            malformed("missing RVA or truncated structure")
            return
        if directory["rva"] + directory["size"] > self.optional["size_of_image"] or \
                self.read_at_rva(directory["rva"], directory["size"]) is None:
            malformed("declared directory outside file-backed image")
            return
        raw = self.read_at_rva(directory["rva"], size)
        if raw is None:
            malformed("structure outside file-backed image")
            return
        start, end, index_va, callbacks_va, zero_fill, _characteristics = struct.unpack(layout, raw)
        base = self.optional["image_base"]
        image_size = self.optional["size_of_image"]
        def in_image(va: int, size: int = 1) -> bool:
            return base <= va and va - base < image_size and size <= image_size - (va - base)

        if end < start or ((start == 0) != (end == 0)):
            malformed("invalid raw-data VA range")
            return
        if start and (not in_image(start) or end - base > image_size):
            malformed("raw-data VA range outside image")
            return
        initialized = end - start
        if initialized:
            if self.read_at_rva(start - base, initialized) is None:
                malformed("raw-data template outside file-backed image")
                return
        if index_va and (index_va % 4 or not in_image(index_va, 4)):
            malformed("TLS index VA outside image or unaligned")
            return

        callback_count = 0
        if callbacks_va:
            pointer_size = 8 if is_64 else 4
            pointer_format = "<Q" if is_64 else "<I"
            if callbacks_va % pointer_size or not in_image(callbacks_va, pointer_size):
                malformed("callback table VA outside image or unaligned")
                return
            callbacks_rva = callbacks_va - base
            for callback_count in range(257):
                entry = self.read_at_rva(callbacks_rva + callback_count * pointer_size,
                                         pointer_size)
                if entry is None:
                    malformed("callback table lacks a file-backed terminator")
                    return
                callback_va = struct.unpack(pointer_format, entry)[0]
                if callback_va == 0:
                    break
                if not in_image(callback_va):
                    malformed("callback target VA outside image")
                    return
            else:
                malformed("callback table exceeds 256 entries")
                return

        self.tls = {
            "status": "real" if initialized or zero_fill or callback_count else "empty",
            "initialized_bytes": initialized,
            "zero_fill_bytes": zero_fill,
            "callback_table_present": bool(callbacks_va),
            "callback_count": callback_count,
            "index_va_present": bool(index_va),
        }

    def parse_debug(self) -> None:
        """Read the image's own RSDS GUID/age; never claim a game-PDB match."""
        directory = self._dir(6)
        if not directory or (directory["rva"] == 0 and directory["size"] == 0):
            return

        def malformed(reason: str) -> None:
            self.debug = {"status": "malformed", "codeview": [], "reason": reason}
            self.warn("debug directory: " + reason)

        if directory["rva"] == 0 or directory["size"] == 0 or directory["size"] % 28:
            malformed("missing RVA or invalid entry-array size")
            return
        entries = self.read_at_rva(directory["rva"], directory["size"])
        if entries is None:
            malformed("entry array outside file-backed image")
            return
        codeview = []
        for offset in range(0, len(entries), 28):
            _flags, _timestamp, _major, _minor, kind, data_size, _data_rva, data_off = \
                struct.unpack_from("<IIHHIIII", entries, offset)
            if kind != 2:
                continue
            if data_size < 4 or data_off > len(self.data) or data_size > len(self.data) - data_off:
                malformed("CodeView record outside file or too short for signature")
                return
            record = self.data[data_off:data_off + data_size]
            if record[:4] != b"RSDS":
                continue
            if data_size < 25:
                malformed("RSDS record too short")
                return
            end = record.find(b"\0", 24)
            if end <= 24:
                malformed("RSDS PDB path missing terminator or name")
                return
            try:
                pdb_path = record[24:end].decode("utf-8")
            except UnicodeDecodeError:
                malformed("RSDS PDB path is not UTF-8")
                return
            pdb_name = pdb_path.replace("\\", "/").rsplit("/", 1)[-1]
            if not pdb_name:
                malformed("RSDS PDB path has empty basename")
                return
            codeview.append({
                "guid": str(uuid.UUID(bytes_le=record[4:20])),
                "age": struct.unpack_from("<I", record, 20)[0],
                "pdb_name": pdb_name,
            })
        self.debug = {"status": "rsds" if codeview else "present_no_rsds", "codeview": codeview}

    def _thunk_size(self) -> int:
        return 8 if self.optional.get("format") == "PE32+" else 4

    def _read_thunk(self, off: int) -> Tuple[int, bool]:
        """Return (value, by_ordinal)."""
        if self._thunk_size() == 8:
            v = self.u64(off)
            return v, bool(v & (1 << 63))
        v = self.u32(off)
        return v, bool(v & (1 << 31))

    def _parse_import_descriptors(self, dir_rva: int, delay: bool) -> List[Dict[str, Any]]:
        out: List[Dict[str, Any]] = []
        thunk_size = self._thunk_size()
        if delay:
            # IMAGE_DELAYLOAD_DESCRIPTOR: grAttrs, szName, phmod, pIAT,
            # pINT, pBoundIAT, pUnloadIAT, dwTimeStamp (all 4 bytes; RVA-based
            # when grAttrs has bit 0 set, otherwise old-style VA).
            off = self.rva_to_offset(dir_rva)
            if off is None:
                self.warn("delay import directory unmapped (rva=0x%x)" % dir_rva)
                return out
            while off + 32 <= len(self.data):
                gr_attrs, sz_name, _phmod, p_iat, p_int = struct.unpack_from("<IIIII", self.data, off)
                rest = self.data[off + 20:off + 32]
                off += 32
                if gr_attrs == 0 and sz_name == 0 and p_iat == 0 and p_int == 0 and rest == b"\x00" * 12:
                    break
                name_rva = sz_name if (gr_attrs & 1) else (sz_name - (self.optional.get("image_base") or 0))
                name_off = self.rva_to_offset(name_rva) if name_rva else None
                dll_name = self.cstr(name_off) if name_off is not None else "<unresolved>"
                funcs: List[str] = []
                if p_int:
                    toff = self.rva_to_offset(p_int)
                    if toff is not None:
                        funcs = self._read_thunk_names(toff)
                out.append({"dll": dll_name, "functions": funcs, "delay": True})
            return out

        off = self.rva_to_offset(dir_rva)
        if off is None:
            self.warn("import directory unmapped (rva=0x%x)" % dir_rva)
            return out
        while off + 20 <= len(self.data):
            (oft, _ts, _fwd, name_rva, first_thunk) = struct.unpack_from("<IIIII", self.data, off)
            off += 20
            if oft == 0 and name_rva == 0 and first_thunk == 0:
                break  # null terminator descriptor
            name_off = self.rva_to_offset(name_rva)
            dll_name = self.cstr(name_off) if name_off is not None else "<unresolved>"
            thunk_rva = oft if oft else first_thunk
            funcs: List[str] = []
            if thunk_rva:
                toff = self.rva_to_offset(thunk_rva)
                if toff is not None:
                    funcs = self._read_thunk_names(toff)
                else:
                    self.warn("import thunk table unmapped for %s (rva=0x%x)" % (dll_name, thunk_rva))
            out.append({"dll": dll_name, "functions": funcs, "delay": False})
        return out

    def _read_thunk_names(self, thunk_off: int) -> List[str]:
        """Walk an Import Name Table until the null thunk."""
        names: List[str] = []
        ts = self._thunk_size()
        pos = thunk_off
        while pos + ts <= len(self.data) and len(names) < 65535:
            val, by_ordinal = self._read_thunk(pos)
            if val == 0:
                break
            if by_ordinal:
                names.append("Ordinal#%d" % (val & 0xFFFF))
            else:
                hoff = self.rva_to_offset(val & 0x7FFFFFFF)
                if hoff is None:
                    names.append("<unmapped hint/name rva=0x%x>" % val)
                else:
                    names.append(self.cstr(hoff + 2))
            pos += ts
        return names

    def parse_exports(self) -> None:
        exp_dir = self._dir(0)
        if not exp_dir or exp_dir["size"] == 0:
            self.diag("no export directory")
            self.exports = {"present": False}
            return
        off = self.rva_to_offset(exp_dir["rva"])
        if off is None:
            self.warn("export directory unmapped (rva=0x%x)" % exp_dir["rva"])
            self.exports = {"present": False}
            return
        (_flags, _ts, _mj, _mn, name_rva, ordinal_base, addr_entries,
         name_ptrs, eat_rva, npt_rva, ot_rva) = struct.unpack_from("<IIHHIIIIIII", self.data, off)
        dir_start = exp_dir["rva"]
        dir_end = dir_start + exp_dir["size"]
        dll_name = self.cstr(self.rva_to_offset(name_rva) or -1)

        names: List[Dict[str, Any]] = []
        forwarders: List[str] = []
        names_by_index: Dict[int, str] = {}
        npt_off = self.rva_to_offset(npt_rva)
        eat_off = self.rva_to_offset(eat_rva)
        ot_off = self.rva_to_offset(ot_rva)
        for i in range(min(name_ptrs, 65535)):
            if npt_off is None or npt_off + i * 4 + 4 > len(self.data):
                self.warn("export name table truncated at %d" % i)
                break
            fn_rva = self.u32(npt_off + i * 4)
            fname = self.cstr(self.rva_to_offset(fn_rva) or -1)
            ordinal = None
            forwarder = None
            if ot_off is not None and ot_off + i * 2 + 2 <= len(self.data):
                ord_idx = self.u16(ot_off + i * 2)
                ordinal = ordinal_base + ord_idx
                names_by_index[ord_idx] = fname
                eat_entry = self.read_at_rva(eat_rva + ord_idx * 4, 4)
                if eat_entry is not None:
                    sym_rva = struct.unpack("<I", eat_entry)[0]
                    if dir_start <= sym_rva < dir_end:
                        forwarder = self.cstr(self.rva_to_offset(sym_rva) or -1)
            entry: Dict[str, Any] = {"name": fname, "ordinal": ordinal, "rva": None}
            if forwarder is not None:
                entry["forwarder"] = forwarder
            elif ordinal is not None:
                eat_entry = self.read_at_rva(eat_rva + (ordinal - ordinal_base) * 4, 4)
                if eat_entry is not None:
                    entry["rva"] = struct.unpack("<I", eat_entry)[0]
            names.append(entry)
        # Export Address Table entries can be forwarded without a name pointer.
        # Scan every ordinal so a proxy hint does not depend on named exports.
        for ord_idx in range(min(addr_entries, 65535)):
            eat_entry = self.read_at_rva(eat_rva + ord_idx * 4, 4)
            if eat_entry is None:
                self.warn("export address table truncated at %d" % ord_idx)
                break
            sym_rva = struct.unpack("<I", eat_entry)[0]
            if dir_start <= sym_rva < dir_end:
                target_off = self.rva_to_offset(sym_rva)
                if target_off is None:
                    self.warn("export forwarder unmapped at ordinal %d" % (ordinal_base + ord_idx))
                    continue
                label = names_by_index.get(ord_idx, "Ordinal#%d" % (ordinal_base + ord_idx))
                forwarders.append("%s -> %s" % (label, self.cstr(target_off)))
        self.exports = {
            "present": True,
            "dll_name": dll_name,
            "ordinal_base": ordinal_base,
            "address_table_entries": addr_entries,
            "number_of_name_pointers": name_ptrs,
            "names": names,
            "forwarders": forwarders,
        }

    # -- managed (CLR) detection ---------------------------------------------

    def parse_managed(self) -> None:
        clr_dir = self._dir(14)
        if not clr_dir or clr_dir["size"] == 0:
            self.managed = None
            self.diag("no CLR runtime header (native image)")
            return
        off = self.rva_to_offset(clr_dir["rva"])
        if off is None:
            self.warn("CLR header unmapped (rva=0x%x)" % clr_dir["rva"])
            self.managed = {"is_managed": True, "metadata_parsed": False}
            return
        cb, maj_rt, min_rt, md_rva, md_size, flags, ep_token = struct.unpack_from("<IHHIIII", self.data, off)
        info: Dict[str, Any] = {
            "is_managed": True,
            "metadata_parsed": False,
            "clr_header_size": cb,
            "runtime_version": "%d.%d" % (maj_rt, min_rt),
            "flags": flags,
            "ilonly": bool(flags & CLR_FLAG_ILONLY),
            "mixed_mode": not bool(flags & CLR_FLAG_ILONLY),
            "entrypoint_token": "0x%08X" % ep_token if ep_token else None,
        }
        md_root_rva = md_rva
        md_off = self.rva_to_offset(md_root_rva)
        if md_off is None or md_off + 16 > len(self.data) or self.data[md_off:md_off + 4] != b"BSJB":
            self.warn("CLR metadata root missing/invalid (BSJB not found)")
            self.managed = info
            return
        md_maj = self.u16(md_off + 4)
        md_min = self.u16(md_off + 6)
        ver_len = self.u32(md_off + 12)
        ver_raw = self.data[md_off + 16:md_off + 16 + ver_len]
        version_string = ver_raw.split(b"\x00", 1)[0].decode("ascii", "replace")
        info["metadata_version"] = version_string
        p = md_off + 16 + ver_len
        p = (p + 1) & ~1  # flags are 2-aligned after version
        _md_flags, n_streams = struct.unpack_from("<HH", self.data, p)
        p += 4
        streams: Dict[str, Tuple[int, int]] = {}
        for _ in range(n_streams):
            if p + 8 > len(self.data):
                self.warn("metadata stream table truncated")
                break
            s_off, s_size = struct.unpack_from("<II", self.data, p)
            p += 8
            end = self.data.find(b"\x00", p)
            s_name = self.data[p:end].decode("ascii", "replace")
            p = (end + 1 + 3) & ~3
            streams[s_name] = (md_off + s_off, s_size)  # stream offsets are from metadata root
        info["streams"] = sorted(streams.keys())

        if "#~" in streams:
            self._parse_tables(info, streams)
        elif "#-" in streams:
            info["uncompressed_metadata"] = True
            self.warn("#- (uncompressed) metadata: table walk skipped")
        else:
            self.warn("no #~ table stream found")
        self.managed = info

    def _idx_size(self, heap_sizes: int, table_rows: Dict[int, int],
                  tables: List[int], tag_bits: int = 0) -> int:
        """Index size (2 or 4 bytes) for a simple or coded index.

        ECMA-335 II.24.2.6: a coded index is 2 bytes only while the largest
        referenced-table row count still fits in (16 - tag_bits) bits,
        i.e. max_rows <= 0xFFFF >> tag_bits; otherwise 4 bytes.
        Simple table indexes use tag_bits=0 (threshold 65535 rows).
        """
        max_rows = 0
        for t in tables:
            max_rows = max(max_rows, table_rows.get(t, 0))
        if max_rows > (0xFFFF >> tag_bits):
            return 4
        return 2

    def _parse_tables(self, info: Dict[str, Any], streams: Dict[str, Tuple[int, int]]) -> None:
        t_off, t_size = streams["#~"]
        reserved, maj, minor, heap_sizes, _r, valid, _sorted = struct.unpack_from(
            "<IBBBBQQ", self.data, t_off)
        info["heap_sizes"] = heap_sizes
        str_wide = bool(heap_sizes & 0x01)
        guid_wide = bool(heap_sizes & 0x02)
        blob_wide = bool(heap_sizes & 0x04)
        s_w = 4 if str_wide else 2
        g_w = 4 if guid_wide else 2
        b_w = 4 if blob_wide else 2

        p = t_off + 24
        table_rows: Dict[int, int] = {}
        ids_in_order: List[int] = []
        for bit in range(64):
            if valid & (1 << bit):
                if p + 4 > len(self.data):
                    self.warn("row-count table truncated")
                    return
                n = self.u32(p)
                p += 4
                table_rows[bit] = n
                ids_in_order.append(bit)
        info["table_row_counts"] = {
            "%s (0x%02X)" % (METADATA_TABLE_NAMES.get(t, "Unknown"), t): n
            for t, n in sorted(table_rows.items())
        }

        # Coded index groups: name -> (table ids, tag bits)
        def coded(tables: List[int], bits: int) -> int:
            return self._idx_size(heap_sizes, table_rows, tables, tag_bits=bits)

        tdor = lambda: coded([0x02, 0x01, 0x1B], 2)          # TypeDefOrRef
        rsc = lambda: coded([0x00, 0x1A, 0x23, 0x01], 2)      # ResolutionScope
        mrp = lambda: coded([0x02, 0x01, 0x1A, 0x06, 0x1B], 3)  # MemberRefParent
        hca = lambda: coded([0x06, 0x04, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x00,
                             0x0E, 0x17, 0x14, 0x11, 0x1A, 0x1B, 0x20, 0x23,
                             0x26, 0x27, 0x28, 0x2A, 0x2B, 0x30], 5)  # HasCustomAttribute
        cat = lambda: coded([0x06, 0x0A], 3)                   # CustomAttributeType (unused tags too)
        mfor = lambda: coded([0x04, 0x06], 1)                  # MemberForwarded
        impl = lambda: coded([0x26, 0x23, 0x27], 2)            # Implementation
        hcs = lambda: coded([0x14, 0x17], 1)                   # HasSemantics
        tmd = lambda: coded([0x02, 0x06], 1)                   # TypeOrMethodDef
        hds = lambda: coded([0x02, 0x06, 0x20], 2)             # HasDeclSecurity

        def tidx(table: int) -> int:
            return self._idx_size(heap_sizes, table_rows, [table])

        # Row schemas: list of byte-widths in field order
        schemas: Dict[int, Any] = {
            0x00: lambda: [2, s_w, g_w, g_w, g_w],                      # Module
            0x01: lambda: [rsc(), s_w, s_w],                            # TypeRef
            0x02: lambda: [4, s_w, s_w, tdor(), tidx(0x04), tidx(0x06)],  # TypeDef
            0x03: lambda: [tidx(0x04)],                                 # FieldPtr
            0x04: lambda: [2, s_w, b_w],                                # Field
            0x05: lambda: [tidx(0x06)],                                 # MethodPtr
            0x06: lambda: [4, 2, 2, s_w, b_w, tidx(0x08)],              # MethodDef
            0x07: lambda: [tidx(0x08)],                                 # ParamPtr
            0x08: lambda: [2, 2, s_w],                                  # Param
            0x09: lambda: [tidx(0x02), tdor()],                         # InterfaceImpl
            0x0A: lambda: [mrp(), s_w, b_w],                            # MemberRef
            0x0B: lambda: [1, 1, coded([0x04, 0x08, 0x17], 2), b_w],    # Constant
            0x0C: lambda: [hca(), cat(), b_w],                          # CustomAttribute
            0x0D: lambda: [coded([0x04, 0x08], 1), b_w],                # FieldMarshal
            0x0E: lambda: [2, hds(), b_w],                              # DeclSecurity
            0x0F: lambda: [2, 4, tidx(0x02)],                           # ClassLayout
            0x10: lambda: [4, tidx(0x04)],                              # FieldLayout
            0x11: lambda: [b_w],                                        # StandAloneSig
            0x12: lambda: [tidx(0x02), tidx(0x14)],                     # EventMap
            0x13: lambda: [tidx(0x14)],                                 # EventPtr
            0x14: lambda: [2, s_w, tdor()],                             # Event
            0x15: lambda: [tidx(0x02), tidx(0x17)],                     # PropertyMap
            0x16: lambda: [tidx(0x17)],                                 # PropertyPtr
            0x17: lambda: [2, s_w, b_w],                                # Property
            0x18: lambda: [2, tidx(0x06), hcs()],                       # MethodSemantics
            0x19: lambda: [tidx(0x02), coded([0x06, 0x0A], 1), coded([0x06, 0x0A], 1)],  # MethodImpl
            0x1A: lambda: [s_w],                                        # ModuleRef
            0x1B: lambda: [b_w],                                        # TypeSpec
            0x1C: lambda: [2, mfor(), s_w, tidx(0x1A)],                 # ImplMap
            0x1D: lambda: [4, tidx(0x04)],                              # FieldRVA
            0x1E: lambda: [4, 4],                                       # EncLog
            0x1F: lambda: [4],                                          # EncMap
            0x20: lambda: [4, 2, 2, 2, 2, 4, b_w, s_w, s_w],            # Assembly
            0x21: lambda: [4],                                          # AssemblyProcessor
            0x22: lambda: [4, 4, 4],                                    # AssemblyOS
            0x23: lambda: [2, 2, 2, 2, 4, b_w, s_w, s_w, b_w],          # AssemblyRef
            0x24: lambda: [4, tidx(0x23)],                              # AssemblyRefProcessor
            0x25: lambda: [4, 4, 4, tidx(0x23)],                        # AssemblyRefOS
            0x26: lambda: [4, s_w, b_w],                                # File
            0x27: lambda: [4, 4, s_w, s_w, impl()],                     # ExportedType
            0x28: lambda: [4, 4, s_w, impl()],                          # ManifestResource
            0x29: lambda: [tidx(0x02), tidx(0x02)],                     # NestedClass
            0x2A: lambda: [2, 2, tmd(), s_w],                           # GenericParam
            0x2B: lambda: [mfor(), b_w],                                # MethodSpec
            0x2C: lambda: [tidx(0x2A), tdor()],                         # GenericParamConstraint
            0x30: lambda: [b_w, g_w, b_w, g_w],                         # Document
            0x31: lambda: [tidx(0x30), b_w],                            # MethodDebugInformation
            0x32: lambda: [tidx(0x06), tidx(0x35), tidx(0x33), tidx(0x34), 4, 4],  # LocalScope
            0x33: lambda: [2, 2, s_w],                                  # LocalVariable
            0x34: lambda: [s_w, b_w],                                   # LocalConstant
            0x35: lambda: [tidx(0x35), b_w],                            # ImportScope
            0x36: lambda: [tidx(0x06), tidx(0x06)],                     # StateMachineMethod
            0x37: lambda: [hca(), g_w, b_w],                            # CustomDebugInformation
        }

        # Walk rows in table-id order, tracking file offsets of each table.
        table_offsets: Dict[int, int] = {}
        unknown: List[str] = []
        pos = p
        for tid in ids_in_order:
            n = table_rows[tid]
            table_offsets[tid] = pos
            schema_fn = schemas.get(tid)
            if schema_fn is None:
                unknown.append(METADATA_TABLE_NAMES.get(tid, "0x%02X" % tid))
                info["metadata_parsed"] = False
                info["metadata_walk_stopped_at"] = METADATA_TABLE_NAMES.get(tid, "0x%02X" % tid)
                self.warn("unknown metadata table 0x%02X with %d rows; table walk stopped"
                          % (tid, n))
                break
            width = sum(schema_fn())
            pos += width * n
        else:
            info["metadata_parsed"] = True
        if unknown:
            info["unknown_tables"] = unknown

        def strings_get(idx: int) -> str:
            if "#Strings" not in streams:
                return "<no #Strings heap>"
            s_off, s_size = streams["#Strings"]
            if idx >= s_size:
                return "<string index OOB>"
            base = s_off + idx
            end = self.data.find(b"\x00", base, s_off + s_size)
            return self.data[base:end if end >= 0 else s_off + s_size].decode("utf-8", "replace")

        # Module name
        if 0x00 in table_offsets and table_rows.get(0x00):
            off_m = table_offsets[0x00]
            name_idx = self.u16(off_m + 2) if not str_wide else self.u32(off_m + 2)
            info["module_name"] = strings_get(name_idx)

        # Assembly name (self)
        if 0x20 in table_offsets and table_rows.get(0x20):
            off_a = table_offsets[0x20]
            # HashAlgId(4)@0 Major(2)@4 Minor(2)@6 Build(2)@8 Rev(2)@10 Flags(4)@12 PK(B)@16 Name(S) Culture(S)
            maj_v = self.u16(off_a + 4)
            min_v = self.u16(off_a + 6)
            bld_v = self.u16(off_a + 8)
            rev_v = self.u16(off_a + 10)
            info["assembly_version"] = "%d.%d.%d.%d" % (maj_v, min_v, bld_v, rev_v)
            base = off_a + 16 + b_w  # skip HashAlgId..Flags, then PublicKey blob index
            name_idx = self.u16(base) if not str_wide else self.u32(base)
            info["assembly_name"] = strings_get(name_idx)

        # AssemblyRef rows -> referenced assemblies
        if 0x23 in table_offsets and table_rows.get(0x23):
            off_ar = table_offsets[0x23]
            refs: List[Dict[str, Any]] = []
            for i in range(table_rows[0x23]):
                base = off_ar + i * (2 + 2 + 2 + 2 + 4 + b_w + s_w + s_w + b_w)
                maj_v = self.u16(base)
                min_v = self.u16(base + 2)
                bld_v = self.u16(base + 4)
                rev_v = self.u16(base + 6)
                name_pos = base + 2 + 2 + 2 + 2 + 4 + b_w
                name_idx = self.u16(name_pos) if not str_wide else self.u32(name_pos)
                refs.append({
                    "name": strings_get(name_idx),
                    "version": "%d.%d.%d.%d" % (maj_v, min_v, bld_v, rev_v),
                })
            info["assembly_references"] = refs

    # -- classification --------------------------------------------------------

    def classify(self) -> Tuple[str, str]:
        """Return (classification, reason)."""
        if not self.coff:
            return "unknown", self.diagnostics[0] if self.diagnostics else "not a PE image"
        machine = self.coff["machine"]
        managed = bool(self.managed and self.managed.get("is_managed"))
        if machine == 0x8664:
            if managed:
                return "managed-dotnet", "CLR header present; AMD64 image"
            return "native-x64", "native AMD64 image, no CLR header"
        if machine == 0x014C:
            if managed:
                # IL-only anycpu assemblies ship as I386; mixed/x86 native cannot load in x64
                if self.managed and self.managed.get("ilonly"):
                    return "managed-dotnet", "CLR IL-only image (I386 machine, runs under 64-bit runtime)"
                return "unsupported-arch", "mixed-mode/x86 I386 image cannot load into a 64-bit PS5 process"
            return "unsupported-arch", "native 32-bit x86 image cannot load into a 64-bit PS5 process"
        if machine == 0xAA64:
            return "unsupported-arch", "ARM64 image; PS5 host is AMD64"
        return "unsupported-arch", "machine %s (0x%04X) not supported by the PC-DLL route" % (
            self.coff["machine_name"], machine)

    # -- UE4SS ------------------------------------------------------------------

    def ue4ss_report(self) -> Dict[str, Any]:
        ue4ss_imports = [imp for imp in self.imports + self.delay_imports
                         if imp["dll"].lower() in ("ue4ss.dll", "ue4ss")]
        start_mod_exports = [e["name"] for e in self.exports.get("names", [])
                             if e["name"] in ("start_mod", "stop_mod")]
        proxy_dlls = ("dwmapi.dll", "winmm.dll", "version.dll",
                      "dbghelp.dll", "xinput1_4.dll", "dsound.dll")
        proxy_name = (any(imp["dll"].lower() in proxy_dlls for imp in self.imports)
                      or ((self.exports.get("dll_name") or "").lower() in proxy_dlls
                          and bool(self.exports.get("forwarders"))))
        return {
            "imports_ue4ss_dll": bool(ue4ss_imports),
            "ue4ss_imported_functions": sorted({f for imp in ue4ss_imports for f in imp["functions"]}),
            "exports_ue4ss_mod_entrypoints": start_mod_exports,
            "looks_like_proxy_dll": proxy_name,
        }

    # -- report -------------------------------------------------------------------

    def to_dict(self, full: bool = True) -> Dict[str, Any]:
        cls, reason = self.classify()
        h = hashlib.sha256(self.data).hexdigest()
        d: Dict[str, Any] = {
            "file": self.path,
            "size_bytes": len(self.data),
            "sha256": h,
            "classification": cls,
            "classification_reason": reason,
            "machine": self.coff.get("machine_name"),
            "format": self.optional.get("format"),
            "subsystem": self.optional.get("subsystem_name"),
            "is_dll": self.coff.get("is_dll"),
            "managed": bool(self.managed and self.managed.get("is_managed")),
            "ue4ss": self.ue4ss_report(),
            "tls": self.tls,
            "gs_access": self.gs_access,
            "debug": self.debug,
            "diagnostics": list(self.diagnostics),
            "warnings": list(self.warnings),
        }
        if not full:
            return d
        d["dos"] = self.dos
        d["coff"] = self.coff
        d["optional"] = self.optional
        d["sections"] = [
            {k: s[k] for k in ("name", "virtual_size", "virtual_address",
                               "raw_size", "raw_ptr", "executable", "readable", "writable")}
            for s in self.sections
        ]
        d["data_directories"] = self.data_dirs
        d["imports"] = self.imports
        d["delay_imports"] = self.delay_imports
        d["exports"] = self.exports
        d["managed_details"] = self.managed
        return d


def analyze_file(path: str) -> Tuple[PeImage, Dict[str, Any]]:
    with open(path, "rb") as f:
        data = f.read()
    img = PeImage(data, path)
    ok = img.parse()
    if ok:
        img.parse_managed()
        img.parse_imports()
        img.parse_delay_imports()
        img.parse_tls()
        img.parse_debug()
        img.parse_exports()
        from gs_audit import audit_gs_access
        img.gs_access = audit_gs_access(img)
    return img, img.to_dict(full=True)


def render_text(report: Dict[str, Any], max_items: int = 20) -> str:
    out: List[str] = []
    p = lambda s: out.append(s)
    p("file:               %s" % report["file"])
    p("size:               %d bytes" % report["size_bytes"])
    p("sha256:             %s" % report["sha256"])
    p("classification:     %s (%s)" % (report["classification"], report["classification_reason"]))
    p("machine/format:     %s / %s" % (report.get("machine"), report.get("format")))
    p("subsystem:          %s (is_dll=%s)" % (report.get("subsystem"), report.get("is_dll")))
    o = report.get("optional", {})
    p("entry point RVA:    0x%X" % o.get("address_of_entry_point", 0))
    p("image base:         0x%X" % o.get("image_base", 0))
    dc = o.get("dll_characteristics_names", [])
    if dc:
        p("dll characteristics: %s" % ", ".join(dc))
    secs = report.get("sections", [])
    if secs:
        p("sections (%d):" % len(secs))
        for s in secs:
            p("  %-8s VA=0x%06X VS=0x%06X raw=0x%06X+%06X %s" % (
                s["name"], s["virtual_address"], s["virtual_size"], s["raw_ptr"], s["raw_size"],
                "".join(c for c, f in (("R", s["readable"]), ("W", s["writable"]), ("X", s["executable"])) if f)))
    managed = report.get("managed_details")
    if managed:
        p("managed:            yes (runtime %s, metadata %s)" % (
            managed.get("runtime_version"), managed.get("metadata_version")))
        if managed.get("assembly_name"):
            p("  assembly:         %s %s" % (managed["assembly_name"], managed.get("assembly_version", "")))
        if managed.get("module_name"):
            p("  module:           %s" % managed["module_name"])
        p("  ilonly:           %s (mixed_mode=%s)" % (managed.get("ilonly"), managed.get("mixed_mode")))
        refs = managed.get("assembly_references") or []
        p("  assembly refs (%d):" % len(refs))
        for r in refs[:max_items]:
            p("    %s %s" % (r["name"], r["version"]))
        if len(refs) > max_items:
            p("    ... +%d more" % (len(refs) - max_items))
    else:
        p("managed:            no (native image)")
    imports = report.get("imports", [])
    if imports:
        p("imports (%d DLLs):" % len(imports))
        for imp in imports:
            p("  %s (%d funcs)%s" % (imp["dll"], len(imp["functions"]),
                                    " [delay]" if imp.get("delay") else ""))
            shown = imp["functions"][:max_items]
            for fn in shown:
                p("    %s" % fn)
            if len(imp["functions"]) > max_items:
                p("    ... +%d more" % (len(imp["functions"]) - max_items))
    exports = report.get("exports", {})
    if exports.get("present"):
        names = exports.get("names", [])
        p("exports:            %d named (dll self-name %r, ordinal base %d)" % (
            len(names), exports.get("dll_name"), exports.get("ordinal_base", 0)))
        for e in names[:max_items]:
            extra = " -> %s" % e["forwarder"] if e.get("forwarder") else (" @RVA 0x%X" % e["rva"] if e.get("rva") else "")
            p("  %s (ord %s)%s" % (e["name"], e.get("ordinal"), extra))
        if len(names) > max_items:
            p("  ... +%d more" % (len(names) - max_items))
    ue = report.get("ue4ss", {})
    p("ue4ss dependency:   imports_ue4ss=%s exports_start_mod=%s proxy=%s" % (
        ue.get("imports_ue4ss_dll"), bool(ue.get("exports_ue4ss_mod_entrypoints")),
        ue.get("looks_like_proxy_dll")))
    if ue.get("ue4ss_imported_functions"):
        p("  ue4ss functions:  %s" % ", ".join(ue["ue4ss_imported_functions"][:max_items]))
    tls = report.get("tls", {})
    tls_status = tls.get("status", "none")
    if tls_status in ("real", "empty"):
        p("tls:                %s (initialized=%s, zero-fill=%s, callbacks=%s)" % (
            tls_status, tls.get("initialized_bytes", 0), tls.get("zero_fill_bytes", 0),
            tls.get("callback_count", 0)))
    else:
        p("tls:                %s%s" % (
            tls_status, " (%s)" % tls["reason"] if tls.get("reason") else ""))
    debug = report.get("debug", {})
    codeview = debug.get("codeview", [])
    if codeview:
        for entry in codeview[:max_items]:
            p("debug CodeView:     %s (GUID=%s, age=%s)" % (
                entry["pdb_name"], entry["guid"], entry["age"]))
        if len(codeview) > max_items:
            p("  ... +%d more" % (len(codeview) - max_items))
    else:
        p("debug CodeView:     %s%s" % (
            debug.get("status", "none"),
            " (%s)" % debug["reason"] if debug.get("reason") else ""))
    for d in report.get("diagnostics", []):
        p("diagnostic:         %s" % d)
    for w in report.get("warnings", []):
        p("warning:            %s" % w)
    return "\n".join(out)


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="PE / managed-assembly discovery tool (capability #13)")
    ap.add_argument("files", nargs="+", help="PE files to analyze (.dll/.exe/.sys)")
    ap.add_argument("--json", action="store_true", help="emit JSON instead of text")
    ap.add_argument("--out", help="also write the full report to this file (JSON)")
    ap.add_argument("--summary-json", action="store_true",
                    help="JSON without headers/sections (for triage at scale)")
    args = ap.parse_args(argv)

    reports = []
    rc = 0
    for path in args.files:
        try:
            _img, rep = analyze_file(path)
        except OSError as e:
            print("error: cannot read %s: %s" % (path, e), file=sys.stderr)
            rc = 2
            continue
        if args.summary_json:
            rep = {k: v for k, v in rep.items()
                   if k not in ("dos", "coff", "optional", "sections", "data_directories")}
        reports.append(rep)
        if not args.json:
            print(render_text(rep))
            print("-" * 72)
    if args.json or args.out:
        payload = reports[0] if len(reports) == 1 else reports
        text = json.dumps(payload, indent=2)
        if args.json:
            print(text)
        if args.out:
            with open(args.out, "w") as f:
                f.write(text + "\n")
    return rc


if __name__ == "__main__":
    sys.exit(main())
