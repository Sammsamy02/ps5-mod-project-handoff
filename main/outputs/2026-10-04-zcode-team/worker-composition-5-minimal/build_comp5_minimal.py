#!/usr/bin/env python3
"""worker-composition-5-minimal: minimal composition-5 bisection PRX builder.

Takes the composition-3 PRX path (worker-prx-link-gaps/package_prx.py, sha
e6fbe6c5... -- the fixed script that produced the 20/20 static-check pass)
and builds a MINIMAL checkpoint variant of it:

  - NO composed runtime object: the 117 MB supplier composition is replaced
    by the worker's tiny module_entry_comp5.c checkpoint writer.
  - The full frozen 191-import surface comes from the lineage's frozen
    import_references.S typed rows (byte-identical to the console-proven
    aligned-entry lineage).
  - comp5:      +1 typed shim row for sceKernelGetModuleInfo (donor
                sceKernelOpen) plus an untyped reference from the checkpoint
                writer = EXACTLY 192 imports.
  - comp5-zero: control with EXACTLY the frozen 191 imports.

Pipeline steps mirror the proven recipe exactly (frozen prospero-clang flag
set, import_references-first ld.lld -r partial link, fail-closed combined
pre-check, derived contract, patched Packager harness via the fixed script's
own patch_packager, Packager, finalize_sdk 0x11000000, make_fself, static
verification). The fixed script itself is executed UNMODIFIED; this driver
only reuses its functions with variant-local pinned values.

Usage:  python3 build_comp5_minimal.py <comp5|comp5-zero> <fresh-out-dir>
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
MY = ROOT / 'outputs/2026-10-04-zcode-team/worker-composition-5-minimal'
MY_SRC = MY / 'module_entry_comp5.c'
FIXED = ROOT / 'outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py'
EXPECTED_FIXED_SHA = 'e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313'

LIN = (ROOT / '.superpowers/native-dependency-spike/'
       'native-full-runtime-aligned-entry-1')
FROZEN_CONTRACT = LIN / 'import-contract.json'
C2RUN = (ROOT / '.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/'
         'ue4ss-native-supplier-composition-2/run-1')
SDK_NM_EVIDENCE = {'sceKernelGetModuleInfo': 'sdknm-libkernel.so.stdout'}
CANDIDATE_BASENAME = 'modbridge_supplier_composition.prx'
COMP5_LOG_PATH = '/data/plugins/UE4SS.log'

VARIANTS = {
    'comp5': {
        'tag': 'COMP5', 'imports_text': '192 imports', 'ref_moduleinfo': True,
        'extensions': {'sceKernelGetModuleInfo': 'sceKernelOpen'},
    },
    'comp5-zero': {
        'tag': 'COMP5Z', 'imports_text': '191 imports', 'ref_moduleinfo': False,
        'extensions': {},
    },
}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def load_fixed_module():
    got = sha256_file(FIXED)
    assert got == EXPECTED_FIXED_SHA, f'fixed package_prx.py sha drift: {got}'
    spec = importlib.util.spec_from_file_location('package_prx_fixed', FIXED)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def derive_contract_comp5(out_contract: Path, extensions: dict, b) -> dict:
    """Extend the frozen 191-import contract by the variant's extension rows,
    mirroring package_prx.derive_contract exactly (same key structure, donor
    row cloning, diagnostics/requirements appends, count bumps) but with
    honest worker-composition-5-minimal provenance."""
    contract = json.loads(FROZEN_CONTRACT.read_text())
    parent_sha = sha256_file(FROZEN_CONTRACT)
    rows = contract['contract']['imports']
    by_name = {r['ImportSymbol']['Name']: r for r in rows}
    for name, donor_name in extensions.items():
        donor = by_name[donor_name]
        nm_ev = C2RUN / SDK_NM_EVIDENCE[name]
        nm_count = nm_ev.read_text(errors='replace').count(name)
        new_row = json.loads(json.dumps(donor))
        new_row['ImportSymbol']['Name'] = name
        new_row['ImportSymbol']['PublishedName'] = name
        new_row['symbol_type'] = 'FUNC'
        new_row['binding'] = 'GLOBAL'
        new_row['evidence'] = [{
            'source': str(nm_ev),
            'sha256': sha256_file(nm_ev),
            'locator': 'nm match count %d' % nm_count,
            'qualification': (
                'SDK stub nm declaration captured by the composition-2 run '
                '(provider of the proven composition-2/3 extension); identity '
                f'cloned from the frozen {donor_name} row '
                f'({donor["ImportSymbol"]["Soname"]}); provider availability '
                'on target not observed by this worker.'),
        }]
        rows.append(new_row)
        contract['diagnostics'].append({
            'name': name, 'binding': 'GLOBAL', 'symbol_type': 'FUNC',
            'status': 'supplier_extension_added',
            'reference_type_requires_reconciliation': False,
            'declared_symbol_types': ['FUNC'],
            'evidence': new_row['evidence']})
        contract['actual_resolved_requirements'].append(
            {'name': name, 'binding': 'GLOBAL', 'symbol_type': 'NOTYPE'})
        contract['compiled_requirements'].append(
            {'name': name, 'binding': 'GLOBAL', 'symbol_type': 'FUNC'})
    added = len(extensions)
    for key in ('required_strong', 'identity_covered_strong', 'covered_strong'):
        contract['counts'][key] = 191 + added
    contract['supplier_extension'] = {
        'purpose': ('composition-5 bisection: checkpoint-writer module with the '
                    'frozen 191-import surface plus exactly the added strong '
                    'imports; isolates import resolution as the composition-4 '
                    'zero-output suspect.'),
        'parent_contract_sha256': parent_sha,
        'added_strong': sorted(extensions),
        'identity_cloned_from': dict(extensions),
        'evidence_files': {n: SDK_NM_EVIDENCE[n] for n in extensions},
        'derived_by': 'worker-composition-5-minimal/build_comp5_minimal.py',
        'lineage_note': ('composition-3/4 packaging derived the same sceKernelGetModuleInfo '
                         'row from the same frozen contract (see '
                         'worker-prx-link-gaps/package_prx.py EXTENDED_IMPORTS)'),
    }
    out_contract.write_text(json.dumps(contract, indent=2) + '\n')
    b.note(f'derived contract ({191 + added} strong) -> {out_contract}')
    return contract


def verify_minimal(m, b, out: Path, variant: dict, precheck: dict) -> dict:
    """Static verification mirroring package_prx.verify_candidate, with the
    baseline expectations adapted to the minimal composition-5 module
    (expected import set = frozen 191 + variant extensions; no composed-run
    baseline exists for a checkpoint-only module and none is claimed)."""
    bare = (out / 'bare-sdk' / CANDIDATE_BASENAME).read_bytes()
    wrapped = (out / CANDIDATE_BASENAME).read_bytes()
    checks: dict = {}
    ok = True

    def check(name, passed, detail):
        nonlocal ok
        checks[name] = {'pass': bool(passed), 'detail': detail}
        if not passed:
            ok = False
            b.gap(f'static verification failed: {name}')
        b.note(f"check {name}: {'PASS' if passed else 'FAIL'}")

    check('elf_magic_bare', bare[:4] == b'\x7fELF', bare[:4].hex())
    check('fself_magic_wrapped', wrapped[:4] == bytes.fromhex('4f153d1d'),
          wrapped[:4].hex())
    count = struct_count = None
    import struct
    count = struct.unpack_from('<I2HQ2H4x', wrapped, 8)[4]
    elf_at = 32 + 32 * count
    phoff = struct.unpack_from('<Q', bare, 32)[0]
    phnum = struct.unpack_from('<H', bare, 56)[0]
    header_end = max(64, phoff + 56 * phnum)
    expected_header = bytearray(bare[:header_end])
    struct.pack_into('<H', expected_header, 60, 0)
    check('wrapped_embedded_elf_header_equal',
          wrapped[elf_at:elf_at + header_end] == bytes(expected_header),
          {'segments': count})
    info_at = (elf_at + header_end + 15) & ~15
    check('fself_payload_digest_matches_bare',
          struct.unpack_from('<4Q32s', wrapped, info_at)[4]
          == hashlib.sha256(bare).digest(), None)
    segs, seg_ok = [], True
    for j in range(1, count, 2):
        props, offset, stored, _ = struct.unpack_from('<4Q', wrapped, 32 + 32 * j)
        index = (props >> 20) & 65535
        ph = struct.unpack_from('<II6Q', bare, phoff + 56 * index)
        if stored != ph[5] or wrapped[offset:offset + stored] != bare[ph[2]:ph[2] + ph[5]]:
            seg_ok = False
        segs.append({'index': index, 'type': hex(ph[0]), 'bytes': stored})
    check('wrapped_segments_equal', seg_ok, segs)

    e_type, e_machine = struct.unpack_from('<HH', bare, 16)
    e_flags = struct.unpack_from('<I', bare, 48)[0]
    checks['elf_module_type_flags'] = {'pass': True, 'detail': {
        'e_type': hex(e_type), 'e_machine': hex(e_machine), 'e_flags': hex(e_flags)}}
    check('elf64_x86_64_le', e_machine == 0x3E and bare[4] == 2 and bare[5] == 1,
          {'e_machine': hex(e_machine), 'class': bare[4], 'data': bare[5]})

    phdrs = m.parse_phdrs(bare)
    module = [p for p in phdrs if p[0] == 0x61000002]
    check('pt_sce_module_unique', len(module) == 1, [hex(p[0]) for p in phdrs])
    sdk_stamp = struct.unpack_from('<I', bare, module[0][2] + 20)[0] if module else None
    check('sdk_stamp_0x11000000', sdk_stamp == 0x11000000,
          hex(sdk_stamp) if sdk_stamp is not None else None)

    dynamic = next((p for p in phdrs if p[0] == 2), None)
    tags: dict[int, int] = {}
    if dynamic:
        for i in range(0, dynamic[5], 16):
            tag, val = struct.unpack_from('<QQ', bare, dynamic[2] + i)
            tags[tag] = val
    check('explicit_dt_init_dt_fini', tags.get(12) != 0 and tags.get(13) != 0,
          {'DT_INIT': tags.get(12), 'DT_FINI': tags.get(13)})
    check('loader_owns_no_arrays',
          all(tags.get(t, 0) == 0 for t in (25, 26, 27, 28)),
          {t: tags.get(t, 0) for t in (25, 26, 27, 28)})

    sections, named = m.parse_sections(bare)
    init_slots = named['.init_array'][5] // 8 if '.init_array' in named else 0
    fini_slots = named['.fini_array'][5] // 8 if '.fini_array' in named else 0
    check('init_fini_slots_present', init_slots > 0 and fini_slots > 0,
          {'init_slots': init_slots, 'fini_slots': fini_slots,
           'note': 'one inert checkpoint slot each (loader owns no arrays; '
                   'module_start never walks them)'})
    for name in ('.eh_frame', '.eh_frame_hdr'):
        check(f'section_{name}_present', name in named and named[name][5] > 0, None)
    b.note('informational: .gcc_except_table '
           + ('present' if named.get('.gcc_except_table', (None, None, None, None, None, 0))[5] > 0
              else 'ABSENT (checkpoint module contains no C++ EH code; deliberate '
                   'minimal-build delta vs the full composition-3 runtime)'))

    dynsyms = m.symbols_in(bare, named, sections, '.dynsym')
    symtabs = m.symbols_in(bare, named, sections, '.symtab')
    und_imports = [s for s in dynsyms if s['shndx'] == 0 and s['bind'] in (1, 2)]
    contract_rows = json.loads((out / 'supplier-contract.json').read_text())['contract']['imports']
    contract_names = {r['ImportSymbol']['Name'] for r in contract_rows}
    nid_to_contract = {}
    for n in contract_names:
        nid_to_contract.setdefault(m.sce_nid(n), n)
    decoded: dict[str, str] = {}
    malformed, unknown_nids = [], []
    suffix_hist: dict[str, int] = {}
    for s in und_imports:
        match = m.NID_LONG_RE.match(s['name'])
        if not match:
            malformed.append(s['name'])
            continue
        head, lib, mod = match.groups()
        suffix_hist[f'{lib}#{mod}'] = suffix_hist.get(f'{lib}#{mod}', 0) + 1
        if head not in nid_to_contract:
            unknown_nids.append(s['name'])
        else:
            decoded.setdefault(nid_to_contract[head], s['name'])
    nid_decoded_names = set(decoded) | set(malformed)
    check('imports_equal_contract',
          nid_decoded_names == contract_names and not unknown_nids,
          {'count': len(nid_decoded_names), 'expected': len(contract_names),
           'decoded_extra': sorted(nid_decoded_names - contract_names),
           'missing': sorted(contract_names - nid_decoded_names),
           'malformed_names': sorted(malformed),
           'unknown_nid_imports': sorted(unknown_nids),
           'suffix_histogram': dict(sorted(suffix_hist.items()))})
    check('linker_provided_dynamic_not_imported', '_DYNAMIC' not in nid_decoded_names, None)

    frozen_names = {r['ImportSymbol']['Name'] for r in json.loads(
        FROZEN_CONTRACT.read_text())['contract']['imports']}
    extensions = sorted(nid_decoded_names - frozen_names)
    providers = {}
    for r in contract_rows:
        providers[r['ImportSymbol']['Name']] = r['ImportSymbol']['Soname']
    soname_hist: dict[str, int] = {}
    for n in nid_decoded_names:
        so = providers.get(n, 'UNKNOWN')
        soname_hist[so] = soname_hist.get(so, 0) + 1
    check('imports_resolve_to_contract_providers',
          nid_decoded_names <= frozen_names | set(variant['extensions']),
          {'outside_frozen_191': extensions,
           'expected_extensions': sorted(variant['extensions']),
           'provider_soname_histogram': dict(sorted(soname_hist.items()))})

    def entry_evidence(symbol: str) -> dict:
        want_nid = m.sce_nid(symbol)
        cands = [s for s in dynsyms if s['shndx'] != 0
                 and s['name'].split('#')[0] == want_nid]
        by = 'dynsym-nid'
        if not cands:
            cands = [s for s in symtabs if s['name'] == symbol and s['shndx'] != 0]
            by = 'symtab-name'
        if not cands:
            return {'symbol': symbol, 'nid': want_nid, 'found': False}
        sym = max(cands, key=lambda s: s['size'])
        load = next((p for p in phdrs if p[0] == 1
                     and p[3] <= sym['value'] < p[3] + p[5]), None)
        if load is None:
            return {'symbol': symbol, 'nid': want_nid, 'found': True, 'mapped': False}
        off = sym['value'] - load[3] + load[2]
        window = bare[off:off + 96]
        patterns = {'and_rsp_imm8_-16': bytes.fromhex('4883e4f0'),
                    'and_rsp_imm32_-16': bytes.fromhex('4881e4f0ffffffff')}
        hits = {k: window.find(v) for k, v in patterns.items()}
        return {'symbol': symbol, 'nid': want_nid, 'found': True, 'mapped': True,
                'resolved_by': by, 'value': hex(sym['value']),
                'file_offset': off, 'size': sym['size'],
                'first_bytes': window[:32].hex(),
                'realignment_pattern_offsets': hits,
                'prologue_source_attribute': 'force_align_arg_pointer (module_entry_comp5.c)'}

    start_ev = entry_evidence('module_start')
    stop_ev = entry_evidence('module_stop')
    checks['entry_alignment_evidence'] = {
        'pass': bool(start_ev.get('found') and start_ev.get('mapped')
                     and (start_ev['realignment_pattern_offsets']['and_rsp_imm8_-16'] >= 0
                          or start_ev['realignment_pattern_offsets']['and_rsp_imm32_-16'] >= 0)),
        'detail': {'module_start': start_ev, 'module_stop': stop_ev}}
    if not checks['entry_alignment_evidence']['pass']:
        b.gap('entry alignment byte evidence not found at module_start')

    checks['combined_precheck'] = {'pass': True, 'detail': precheck}
    return {'status': 'STATIC_CHECKS_PASSED' if ok else 'STATIC_CHECKS_GAPS',
            'checks': checks}


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    variant_name, out = sys.argv[1], Path(sys.argv[2])
    if variant_name not in VARIANTS:
        print(f'unknown variant {variant_name}; expected one of {sorted(VARIANTS)}')
        return 2
    if out.exists():
        print(f'refusing to overwrite: {out} already exists', file=sys.stderr)
        return 2
    variant = VARIANTS[variant_name]
    out.mkdir(parents=True)

    m = load_fixed_module()
    # variant-local pinned values consumed by the reused fixed-script functions
    m.EXTENDED_IMPORTS = dict(variant['extensions'])
    m.EXTENDED_IMPORT_EVIDENCE = {n: SDK_NM_EVIDENCE[n] for n in variant['extensions']}
    b = m.Build(out, link=True)
    b.note(f'output dir: {out}')
    b.note(f'variant: {variant_name} tag={variant["tag"]} '
           f'imports={variant["imports_text"]} extensions={sorted(variant["extensions"])}')

    try:
        env_extra = {'LLVM_CONFIG': str(m.PIN['llvm'] / 'llvm-config')}
        build = out / 'build'
        build.mkdir(parents=True)
        flags = m.CLANG_FLAGS
        includes = ['-I' + str(p) for p in (
            LIN / 'generated', LIN / 'probe', LIN / 'lifecycle', LIN,
            m.PIN['lua_include'])]

        # 1. frozen typed import surface, byte-identical source.
        # Order matters: the typed shim (if any) must precede the checkpoint
        # writer so the merged UND symbol keeps the FUNC type (ld.lld -r keeps
        # the first-seen type on UND merges; measured 2026-10-04). The Packager
        # requires the exact typed owned reference.
        objects = []
        unit_records = {}
        units = [(LIN / 'import_references.S', 'import_references', [], None)]
        if variant['extensions']:
            shim = build / 'supplier_import_extension.S'
            shim.write_text(m.supplier_import_shim())
            units.append((shim, 'supplier_import_extension', [], None))
        units.append((MY_SRC, 'module_entry',
                      ['-DCOMP5_TAG="%s"' % variant['tag'],
                       '-DCOMP5_IMPORTS="%s"' % variant['imports_text'],
                       # Deliberate driver-level flag addition (recorded): the
                       # prospero target emits no unwind tables for plain C, so
                       # the frozen flag set alone yields an eh_frame-less
                       # checkpoint module. The composition-3 runtime carried
                       # its .eh_frame from C++ units; this C-only module asks
                       # for asynchronous unwind tables explicitly instead.
                       '-fasynchronous-unwind-tables']
                      + (['-DCOMP5_REF_MODULEINFO'] if variant['ref_moduleinfo'] else []),
                      MY_SRC))
        for src, stem, extra_defs, _ in units:
            target = build / f'{stem}.o'
            compiler = m.PIN['sdk'] / 'bin' / (
                'prospero-clang++' if src.suffix == '.cpp' else 'prospero-clang')
            b.run([compiler, *flags, *extra_defs, *includes, '-c', src, '-o', target],
                  env_extra=env_extra)
            objects.append(target)
            unit_records[stem] = {'source': str(src), 'source_sha256': sha256_file(src),
                                  'object_sha256': sha256_file(target)}

        # 2. partial link (import_references first, proven order)
        combined = build / 'combined-comp5-minimal.o'
        b.run([m.PIN['llvm'] / 'ld.lld', '-r', '-m', 'elf_x86_64', '-o', combined,
               *objects], env_extra=env_extra)
        combined_sha = sha256_file(combined)

        # 3. fail-closed combined pre-check: exact expected import set
        frozen_names = {r['ImportSymbol']['Name'] for r in json.loads(
            FROZEN_CONTRACT.read_text())['contract']['imports']}
        expected_names = frozen_names | set(variant['extensions'])
        reader = m.PIN['llvm'] / 'llvm-readelf'
        strong, weak, defined = m.readelf_undefined(reader, combined)
        boundary = set(m.PIN['frozen_boundary']) | set(m.BOUNDARY_ADDITIONS)
        allowed_weak = set(m.PIN['frozen_allowed_weak'])
        actual = {n for n in strong - defined - boundary}
        unexpected = sorted(actual - expected_names)
        missing = sorted(expected_names - actual)
        weak_unexpected = sorted((weak - defined) - allowed_weak)
        precheck = {'strong_unresolved': len(actual), 'expected': len(expected_names),
                    'unexpected': unexpected, 'missing': missing,
                    'weak_unexpected': weak_unexpected,
                    'combined_sha256': combined_sha}
        b.note(f'combined pre-check: {precheck}')
        if unexpected or missing or weak_unexpected:
            b.gap(f'combined object unresolved set differs from expected: {precheck}')
            raise RuntimeError('combined pre-check failed; not packaging')

        # 4. contract (frozen bytes for the control; derived for comp5)
        contract_path = out / 'supplier-contract.json'
        if variant['extensions']:
            derive_contract_comp5(contract_path, variant['extensions'], b)
        else:
            contract_path.write_bytes(FROZEN_CONTRACT.read_bytes())
            b.note(f'contract: frozen 191 bytes copied verbatim -> {contract_path}')
        contract_sha = sha256_file(contract_path)

        # 5. patched Packager harness (reused fixed-script function, same patches)
        patches = m.patch_packager(b, out / 'harness', contract_sha,
                                   sha256_file(FROZEN_CONTRACT))

        # 6. Packager
        harness_dll = out / 'harness/bin/Release/net10.0/Packager.dll'
        if harness_dll.exists():
            raise RuntimeError('refuse to reuse an existing Packager build')
        b.run([m.PIN['dotnet'], 'build', str(out / 'harness/Packager.csproj'),
               '-c', 'Release', '--ignore-failed-sources'])
        bare_original = out / 'bare-original' / CANDIDATE_BASENAME
        bare_original.parent.mkdir(parents=True, exist_ok=True)
        pack = b.run([m.PIN['dotnet'], str(harness_dll), str(combined),
                      str(contract_path), str(bare_original)])
        packaging = {'packager': json.loads(pack.stdout)}

        # 7. finalize_sdk 0x11000000 + make_fself (console-proven finalize)
        profile = {
            'title_id': 'PPSA33286',
            'module_path': f'/data/plugins/{CANDIDATE_BASENAME}',
            'log_path': COMP5_LOG_PATH,
            'sdk_version': '0x11000000',
            'sdk_stamp_provenance': 'console-proven fix for ESDKVERSION 0x80020063',
            'variant': variant_name,
            'checkpoint_message': f'{variant["tag"]}: {variant["imports_text"]}, '
                                  'module_start reached',
            'combined_object_sha256': combined_sha,
            'parent_recipe': str(LIN),
            'marker': f'composition-5-minimal/{variant_name} build='
                      + combined_sha[:16],
            'promotion_allowed': False,
            'native_executed': False,
        }
        profile_path = out / 'package-profile.json'
        profile_path.write_text(json.dumps(profile, indent=2) + '\n')
        bare_sdk = out / 'bare-sdk' / CANDIDATE_BASENAME
        bare_sdk.parent.mkdir(parents=True, exist_ok=True)
        fin = b.run(['python3', str(m.PIN['finalize_sdk']), str(bare_original),
                     str(profile_path), str(bare_sdk)])
        packaging['finalize_sdk'] = json.loads(fin.stdout)
        wrapped = out / CANDIDATE_BASENAME
        b.run(['python3', str(m.PIN['make_fself']), str(bare_sdk), str(wrapped)])
        packaging['candidate'] = {'path': str(wrapped), 'bytes': wrapped.stat().st_size,
                                  'sha256': sha256_file(wrapped)}
        packaging['bare_original'] = {'path': str(bare_original),
                                      'sha256': sha256_file(bare_original),
                                      'bytes': bare_original.stat().st_size}
        packaging['bare_sdk'] = {'path': str(bare_sdk),
                                 'sha256': sha256_file(bare_sdk),
                                 'bytes': bare_sdk.stat().st_size}

        # 8. static verification
        verification = verify_minimal(m, b, out, variant, precheck)
        b.write_json('verification-evidence/static-checks.json', verification)
    except Exception as exc:  # noqa: BLE001
        b.gap(f'exception: {exc}')
        b.note(f'FAILED: {exc}')

    receipt = {
        'worker': 'worker-composition-5-minimal',
        'driver': str(Path(__file__).resolve()),
        'variant': variant_name,
        'utc_tag': b.log[0] if b.log else None,
        'mode': 'link',
        'fixed_script_reused_unmodified': str(FIXED),
        'frozen_contract': {'path': str(FROZEN_CONTRACT),
                            'sha256': sha256_file(FROZEN_CONTRACT)},
        'checkpoint_source': {'path': str(MY_SRC), 'sha256': sha256_file(MY_SRC)},
        'expected_strong_imports': 191 + len(variant['extensions']),
        'gaps': b.gaps,
        'log': b.log,
        'commands': b.records,
        'units': unit_records if 'unit_records' in dir() else None,
        'packaging': packaging if 'packaging' in dir() else None,
    }
    b.write_json('build-receipt.json', receipt)
    if b.gaps:
        b.write_json('GAPS.json', {'gaps': b.gaps,
                                   'action': 'do not stage/promote; reconcile first'})
        b.note(f'{len(b.gaps)} gap(s); candidate NOT approved')
        return 1
    b.note('done: minimal composition-5 candidate produced and statically verified '
           '(no in-game claim)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
