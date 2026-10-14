"""Exact-once research-era correction; never changes the original scanner."""
import argparse
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[2] / 'outputs/2026-10-08-l25-prep/phasea-tools/engine_match.py'
EXPECTED_SHA256 = '10497b76d4402ca220d19ca64217c95665d584856ce75a1def6a30aee2e09920'
START = 'def anchor_pack_recommendation(family, version):\n'
END = '\n\ndef classify(image, versions, markers):\n'
REPLACEMENT = '''def anchor_pack_recommendation(family, version):
    """Research labels only; no anchor or object-array layout is admitted.

    Preserve the existing 4.23 FName-era research boundary using integer
    version components. An unknown minor version selects no UE4 era.
    """
    if family is None:
        return "none (refused)"
    if family == "UE4":
        match = re.fullmatch(r"4\\.(\\d+)", version) if isinstance(version, str) else None
        if match is None:
            return "ue4-era-unresolved (minor version unavailable/unsupported; no era pack selected)"
        if (4, int(match.group(1))) < (4, 23):
            return "ue4-legacy (pre-4.23 FName-era research candidate; object-array layout and every anchor need independent image evidence)"
        return "ue4-chunked (4.23+ FNamePool-era research candidate; object-array layout and every anchor need independent image evidence)"
    if family == "UE5":
        return "ue5-fnamepool (current Townfall pack basis: GUObjectArray 9-pattern + GMalloc 5-pattern + FName ctor slot; verify per-image)"
    return "unknown"
'''


def transform(source):
    if hashlib.sha256(source).hexdigest() != EXPECTED_SHA256:
        raise ValueError('original scanner source hash mismatch; drift/already staged')
    text = source.decode('utf-8')
    if text.count(START) != 1 or text.count(END) != 1:
        raise ValueError('expected exactly one research-era function boundary')
    first, last = text.index(START), text.index(END)
    if first >= last:
        raise ValueError('invalid replacement boundary order')
    original = text[first:last]
    if text.count(original) != 1:
        raise ValueError('replacement anchor must occur exactly once')
    return text.replace(original, REPLACEMENT, 1).encode('utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SOURCE)
    parser.add_argument('--write-dir', type=Path)
    args = parser.parse_args()
    try:
        original = args.source.read_bytes()
        derived = transform(original)
        receipt = {'label': 'Mac-only staged utility correction; not applied',
                   'items_checked': 1, 'replacements': 1,
                   'source_path': str(args.source.resolve()),
                   'source_sha256': hashlib.sha256(original).hexdigest(),
                   'derived_sha256': hashlib.sha256(derived).hexdigest(),
                   'derived_bytes': len(derived), 'runtime_profile_admitted': False,
                   'original_written_by_tool': False, 'staged_copy_written': False}
        if args.write_dir is not None:
            dest = args.write_dir.resolve()
            if not dest.is_relative_to(HERE) or dest == HERE or dest.exists():
                raise ValueError('destination must be a new directory inside cap02')
            dest.mkdir()
            (dest/'engine_match.py').write_bytes(derived)
            receipt['staged_copy_written'] = True
            (dest/'staging-receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
        print(json.dumps(receipt, indent=2))
        return 0
    except (ValueError, OSError) as exc:
        print(json.dumps({'verdict': 'REFUSED', 'reason': str(exc)}))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
