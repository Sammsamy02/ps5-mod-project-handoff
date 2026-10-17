"""Caller-local SDK v0.43 dynamic-table layout; never edits the SDK."""
import hashlib
from pathlib import Path
import struct

SDK_SCRIPT_SHA256 = '3bacf56a21602a1298752023a4c5b0c305954ddf6cdbce1464476fc9da9d9093'
CALLER_SCRIPT_SHA256 = '6704c97b902a95e00e1fc592fecbe307c25be8b6dc38ad690cdf3596473a2d55'


def write_linker_script(sdk_root, output_dir):
    source = Path(sdk_root) / 'ldscripts/elf_x86_64.x'
    original = source.read_bytes()
    if hashlib.sha256(original).hexdigest() != SDK_SCRIPT_SHA256:
        raise ValueError('SDK v0.43 linker script differs from pinned source')
    anchor = b'\t} : ph_rodata : ph_dyn\n'
    reset = b'\n\t.dynsym : { *(.dynsym) } : ph_rodata\n'
    if original.count(anchor) != 1:
        raise ValueError('SDK dynamic-table declaration differs')
    fixed = original.replace(anchor, anchor + reset)
    if (fixed.replace(reset, b'') != original or
            hashlib.sha256(fixed).hexdigest() != CALLER_SCRIPT_SHA256):
        raise ValueError('caller-local linker script differs from reviewed reset')
    target = Path(output_dir) / 'elf_x86_64.dynamic-only.x'
    with target.open('xb') as stream:
        stream.write(fixed)
    return {'sdk_script': str(source), 'sdk_script_sha256': SDK_SCRIPT_SHA256,
            'caller_script': str(target), 'caller_script_sha256': CALLER_SCRIPT_SHA256,
            'change': 'dynsym assigned only to ph_rodata resets ph_dyn orphan inheritance'}


def validate_dynamic_layout(elf_path):
    data = Path(elf_path).read_bytes()

    def unpack(fmt, offset):
        size = struct.calcsize(fmt)
        if offset < 0 or offset > len(data) or size > len(data) - offset:
            raise ValueError('truncated ELF layout')
        return struct.unpack_from(fmt, data, offset)

    if len(data) < 64 or data[:6] != b'\x7fELF\x02\x01':
        raise ValueError('ELF64 little-endian layout required')
    header = unpack('<HHIQQQIHHHHHH', 16)
    if header[1] != 62 or header[8] != 56 or header[10] != 64:
        raise ValueError('unexpected native ELF machine/header sizes')
    segments = [unpack('<IIQQQQQQ', header[4] + i * header[8])
                for i in range(header[9])]
    sections = [unpack('<IIQQQQIIQQ', header[5] + i * header[10])
                for i in range(header[11])]
    dynamic_segments = [row for row in segments if row[0] == 2]
    dynamic_sections = [row for row in sections if row[1] == 6]
    if len(dynamic_segments) != 1 or len(dynamic_sections) != 1:
        raise ValueError('exactly one PT_DYNAMIC and SHT_DYNAMIC required')
    segment, section = dynamic_segments[0], dynamic_sections[0]
    offset, address, size = section[4], section[3], section[5]
    if (segment[2] != offset or segment[3] != address or
            segment[5] != size or segment[6] != size):
        raise ValueError('PT_DYNAMIC offset/address/filesz/memsz differs from SHT_DYNAMIC')
    if (not size or size % 16 or offset % 16 or address % 16 or
            section[9] != 16):
        raise ValueError('dynamic table lacks aligned 16-byte entries')
    if offset > len(data) or size > len(data) - offset:
        raise ValueError('dynamic table extends past ELF bytes')
    tags = [unpack('<qQ', offset + i) for i in range(0, size, 16)]
    if tags[-1][0] != 0:
        raise ValueError('dynamic table lacks terminal DT_NULL')
    return {'validated': True, 'offset': offset, 'address': address,
            'filesz': segment[5], 'memsz': segment[6], 'section_size': size,
            'entry_size': 16, 'entries': len(tags), 'terminal_dt_null': True}
