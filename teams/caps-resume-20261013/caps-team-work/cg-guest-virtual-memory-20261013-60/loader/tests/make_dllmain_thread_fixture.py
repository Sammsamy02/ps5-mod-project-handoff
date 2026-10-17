"""Derive a synthetic PE with DllMain CreateThread.

The original fixture's bytes are never changed on disk. This exact PE shape
has spare executable and writable section space for one controlled probe.
"""

import struct


def derive(source: bytes) -> bytes:
    if len(source) != 0xA00 or source[:2] != b"MZ":
        raise ValueError("unexpected source fixture size or DOS header")
    pe = struct.unpack_from("<I", source, 0x3C)[0]
    if source[pe:pe + 4] != b"PE\0\0":
        raise ValueError("unexpected PE signature")
    optional = pe + 4 + 20
    if struct.unpack_from("<H", source, optional)[0] != 0x20B or \
       struct.unpack_from("<I", source, optional + 16)[0] != 0x1000:
        raise ValueError("unexpected PE32+ fixture entry")
    old_name = b"DisableThreadLibraryCalls\0"
    if source.count(old_name) != 1:
        raise ValueError("unexpected KERNEL32 import name")
    changed = bytearray(source)
    name_at = source.index(old_name)
    changed[name_at:name_at + len(old_name)] = \
        b"CreateThread\0".ljust(len(old_name), b"\0")
    struct.pack_into("<I", changed, optional + 16, 0x1080)

    entry_rva = 0x1080
    worker_rva = 0x10D0
    iat_rva = 0x3060
    handle_rva = 0x3080
    marker_rva = 0x3088
    body = bytearray()

    def emit(code: bytes) -> int:
        at = len(body)
        body.extend(code)
        return at

    def rel32(at: int, instruction_length: int, target_rva: int) -> None:
        disp = target_rva - (entry_rva + at + instruction_length)
        struct.pack_into("<i", body, at + instruction_length - 4, disp)

    emit(bytes.fromhex("4883ec38"))                 # sub rsp, 0x38
    emit(bytes.fromhex("83fa01"))                   # cmp edx, DLL_PROCESS_ATTACH
    skip_at = emit(bytes.fromhex("7500"))           # jne done
    emit(bytes.fromhex("31c9"))                     # null attributes
    emit(bytes.fromhex("31d2"))                     # default stack
    lea_at = emit(bytes.fromhex("4c8d0500000000"))  # r8 = worker entry
    emit(bytes.fromhex("4531c9"))                   # null worker argument
    emit(bytes.fromhex("48c744242000000000"))     # flags = 0
    emit(bytes.fromhex("48c744242800000000"))     # no thread-ID output
    call_at = emit(bytes.fromhex("ff1500000000"))   # IAT CreateThread
    save_at = emit(bytes.fromhex("48890500000000")) # save HANDLE in .idata
    done_at = len(body)
    emit(bytes.fromhex("b801000000"))               # return TRUE
    emit(bytes.fromhex("4883c438c3"))               # restore stack, ret
    if not -128 <= done_at - (skip_at + 2) <= 127 or len(body) > 0x50:
        raise ValueError("DllMain probe exceeds reserved code space")
    struct.pack_into("<b", body, skip_at + 1, done_at - (skip_at + 2))
    rel32(lea_at, 7, worker_rva)
    rel32(call_at, 6, iat_rva)
    rel32(save_at, 7, handle_rva)

    # Worker stores a marker in the PE's writable .idata then returns 42.
    worker = bytearray(bytes.fromhex("c7050000000001000000b82a000000c3"))
    struct.pack_into("<i", worker, 2,
                     marker_rva - (worker_rva + 10))
    text_raw = 0x400
    changed[text_raw + 0x80:text_raw + 0x80 + len(body)] = body
    changed[text_raw + 0xD0:text_raw + 0xD0 + len(worker)] = worker
    if any(changed[0x800 + 0x80:0x800 + 0x8C]):
        raise ValueError("probe output slots were not zero")
    return bytes(changed)
