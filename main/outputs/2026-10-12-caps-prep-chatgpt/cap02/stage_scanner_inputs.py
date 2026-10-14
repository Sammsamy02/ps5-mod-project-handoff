"""Combined exact-source scanner staging; original input is never written."""
import argparse
import hashlib
import json
from pathlib import Path

import stage_version_era

HERE = Path(__file__).resolve().parent
START = 'def parse_image(path):\n'
END = '\n\ndef scan_markers(image, markers, only_idx=None):\n'
REPLACEMENT = '''def parse_image(path):
    """Read supported plain ELF64/LSB geometry before marker classification.

    This validates the structures consumed by this scanner; it is not a
    complete ELF loader, signature check or engine/profile admission.
    """
    p = Path(path)
    h = hashlib.sha256()
    with p.open("rb") as f:
        head = f.read(64)
        if len(head) < 64:
            raise ValueError("file too small to be an ELF64")
        if head[:4] in (bytes.fromhex("4f153d1d"), bytes.fromhex("5414f5ee")):
            raise ValueError("SELF container: a supported plain ELF input is required")
        if head[:4] != b"\\x7fELF":
            raise ValueError("not an ELF")
        if head[4:7] != bytes([2, 1, 1]):
            raise ValueError("unsupported ELF class, byte order or identification version")
        if struct.unpack_from("<H", head, 18)[0] != 0x3e:
            raise ValueError("not x86-64")
        if struct.unpack_from("<I", head, 20)[0] != 1:
            raise ValueError("unsupported ELF object version")
        if struct.unpack_from("<H", head, 52)[0] != 64:
            raise ValueError("unsupported ELF64 header size")
        f.seek(0, 2)
        size = f.tell()
        phoff = struct.unpack_from("<Q", head, 32)[0]
        phentsize = struct.unpack_from("<H", head, 54)[0]
        phnum = struct.unpack_from("<H", head, 56)[0]
        if phnum == 0xffff:
            raise ValueError("extended program-header count is unsupported")
        if phnum and (phentsize < 56 or phoff < 64 or phoff > size
                      or phentsize * phnum > size - phoff):
            raise ValueError("invalid or truncated ELF64 program-header table")
        if not phnum and phoff:
            raise ValueError("program-header offset without program headers")
        loads = []
        extent = 0
        for i in range(phnum):
            # Read only the known 56-byte prefix, even for padded entries.
            f.seek(phoff + i * phentsize)
            raw = f.read(56)
            if len(raw) != 56:
                raise ValueError("truncated ELF64 program header")
            ptype, _flags, off, va, _pa, filesz, memsz, align = struct.unpack("<IIQQQQQQ", raw)
            if ptype != 1:
                continue
            if off > size or filesz > size - off:
                raise ValueError("PT_LOAD file range extends past end of file")
            if filesz > memsz:
                raise ValueError("PT_LOAD file size exceeds memory size")
            if memsz > (1 << 64) - 1 - va:
                raise ValueError("PT_LOAD virtual extent overflows uint64")
            if align not in (0, 1) and (align & (align - 1) or va % align != off % align):
                raise ValueError("invalid PT_LOAD declared alignment")
            loads.append({"off": off, "vaddr": va, "filesz": filesz, "memsz": memsz})
            extent = max(extent, va + memsz)
        f.seek(0)
        hashed_size = 0
        while True:
            chunk = f.read(1 << 20)
            if not chunk:
                break
            hashed_size += len(chunk)
            h.update(chunk)
        if hashed_size != size:
            raise ValueError("ELF size changed during read")
    return {"path": str(p), "sha256": h.hexdigest(), "size": size,
            "extent": extent, "loads": loads}
'''


def transform(source):
    text = stage_version_era.transform(source).decode('utf-8')
    if text.count(START) != 1 or text.count(END) != 1:
        raise ValueError('expected exactly one image parser boundary')
    first, last = text.index(START), text.index(END)
    if first >= last:
        raise ValueError('invalid image parser boundary order')
    original = text[first:last]
    if text.count(original) != 1:
        raise ValueError('image parser anchor must occur exactly once')
    return text.replace(original, REPLACEMENT, 1).encode('utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=stage_version_era.SOURCE)
    parser.add_argument('--write-dir',type=Path)
    args = parser.parse_args()
    try:
        original = args.source.read_bytes()
        derived = transform(original)
        receipt = {'label':'Mac-only combined scanner stage; not applied',
                   'items_checked':1,'replacements':2,
                   'source_path':str(args.source.resolve()),
                   'source_sha256':hashlib.sha256(original).hexdigest(),
                   'derived_sha256':hashlib.sha256(derived).hexdigest(),
                   'derived_bytes':len(derived),
                   'original_written_by_tool':False,'staged_copy_written':False,
                   'runtime_profile_admitted':False}
        if args.write_dir is not None:
            dest = args.write_dir.resolve()
            if not dest.is_relative_to(HERE) or dest == HERE or dest.exists():
                raise ValueError('destination must be a new directory inside cap02')
            dest.mkdir()
            (dest/'engine_match.py').write_bytes(derived)
            receipt['staged_copy_written'] = True
            (dest/'staging-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
        print(json.dumps(receipt,indent=2))
        return 0
    except (ValueError,OSError) as exc:
        print(json.dumps({'verdict':'REFUSED','reason':str(exc)}))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
