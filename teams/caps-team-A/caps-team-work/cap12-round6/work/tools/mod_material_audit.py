#!/usr/bin/env python3
"""Per-mod material audit: which of a mod's materials have NO shader code for the
target platform, so the engine will draw them with its default material
(UE's grey/white checker 'WorldGridMaterial') instead. Mac-only, read-only.

Generic for UE5 IoStore titles; nothing game- or mod-specific is hard-coded.
Facts used (each from the files themselves):
  * mod package classes: zen export class -> global ScriptObjects path
  * the target shader format/platform: from the game's own shader libraries
    (shader_inputs.py output, e.g. 'SF_PS5-SP_PS5')
  * shader code a mod could bring: its own type-8 libraries (by name suffix) or
    inline shader-map data naming a shader format (strings like PCD3D_SM6)
  * base packages that DO have target shaders: the game's own container header
    store entries (ShaderMapHashes), keyed by package id
Verdicts per material export:
  NO_TARGET_SHADERS   Material/static-permutation MIC whose shader data is only for
                      other formats (renders as engine default material)
  INHERITS_MISSING    MIC whose parent chain ends at a NO_TARGET_SHADERS material
  OK_BASE_SHADERS     MIC whose parent chain ends at a base package that has target
                      shader maps in the game's own store
  UNKNOWN             anything not provable (fail closed: never reported as OK)
Exit 1 when zero material exports were checked.
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cityhash  # noqa: E402
import container_header  # noqa: E402
import utoc  # noqa: E402
import zen  # noqa: E402

MATERIAL = '/Script/Engine/Material'
MIC = '/Script/Engine/MaterialInstanceConstant'
FORMAT_TOKEN = re.compile(rb'\b([A-Z][A-Z0-9]{1,15}_[A-Z0-9_]{2,24})\x00')


def reader(path):
    """Read-only random access; opens per call so no handle is left open."""
    def r(o, n):
        with open(path, 'rb') as fh:
            fh.seek(o)
            return fh.read(n)
    return r


def load_script_objects(utoc_path, ucas_path):
    t = utoc.load(utoc_path)
    rows = t.rows_of_type(5)
    zen.need(len(rows) == 1, 'expected exactly one ScriptObjects chunk')
    return zen.ScriptObjects(t.read_chunk(rows[0], reader(ucas_path)))


def inline_formats(blob, known_formats):
    found = set()
    for m in FORMAT_TOKEN.finditer(blob):
        tok = m.group(1).decode()
        if tok in known_formats or tok.startswith(('PCD3D_', 'SF_', 'VULKAN_', 'METAL_', 'GLSL_')):
            found.add(tok)
    return sorted(found)


def load_base(base_tocs, base_headers):
    base_ids = {}
    for bt_path in base_tocs:
        bt = utoc.load(bt_path)
        for r in bt.rows_of_type(1):
            base_ids[int.from_bytes(bt.chunk_ids[r][:8], 'little')] = os.path.basename(bt_path)
    base_store = {}
    for hp in base_headers:
        with open(hp, 'rb') as fh:
            base_store.update(container_header.parse(fh.read())[1])
    return base_ids, base_store


def load_mod(mod_utoc, mod_ucas):
    t = utoc.load(mod_utoc)
    rd = reader(mod_ucas)
    packages = {}
    for row in t.rows_of_type(1):
        p = zen.ZenPackage(t.read_chunk(row, rd))
        packages[p.name] = p
    return packages, [t.chunk_ids[r].hex() for r in t.rows_of_type(8)]


def audit(mod_utoc, mod_ucas, script, base_tocs, base_headers, target_suffix):
    packages, mod_libs = load_mod(mod_utoc, mod_ucas)
    base_ids, base_store = load_base(base_tocs, base_headers)
    rep = audit_packages(packages, mod_libs, script, base_ids, base_store, target_suffix)
    rep['mod_toc'] = os.path.basename(mod_utoc)
    return rep


def audit_packages(packages, mod_libs, script, base_ids, base_store, target_suffix):
    target_fmt = target_suffix.split('-')[0]
    known = {target_fmt, target_suffix.split('-')[-1]}
    mats = {}
    for name, p in packages.items():
        for e in p.exports:
            c = p.describe_ref(e['class'], script)
            cls = c.get('path') if c else None
            if cls not in (MATERIAL, MIC):
                continue
            blob = p.export_bytes(e) or b''
            pid = cityhash.ue_name_hash(name)
            rec = {'package': name, 'export': e['name'], 'class': cls.rsplit('/', 1)[-1],
                   'export_bytes': e['serial_size'], 'inline_shader_formats': inline_formats(blob, known),
                   'overrides_base': base_ids.get(pid),
                   # None = unknown: not a base package, or its container header is not loaded
                   'base_had_target_shader_maps':
                   bool(base_store[pid]['shader_maps']) if pid in base_ids and pid in base_store else None,
                   'imports': list(p.imported_packages)}
            if name in mats:  # one material per package assumed; fail closed otherwise
                rec['verdict'] = mats[name]['verdict'] = 'UNKNOWN'
                rec['why'] = mats[name]['why'] = 'several material exports in one package'
            mats[name] = rec
    # a mod-own shader library could carry target code; not decoded here -> such
    # materials end UNKNOWN (never OK) unless their inline data names the target
    mod_has_target_lib = bool(mod_libs)

    def own_shaders_missing(rec):
        fm = rec['inline_shader_formats']
        has_target = target_fmt in fm or (mod_has_target_lib and not fm)
        return not has_target

    def resolve(name, depth=0):
        rec = mats[name]
        if 'verdict' in rec:
            return rec['verdict']
        if depth > 16:
            rec['verdict'] = 'UNKNOWN'; rec['why'] = 'parent chain too deep'
            return rec['verdict']
        if rec['class'] == 'Material' or rec['inline_shader_formats']:
            if own_shaders_missing(rec):
                rec['verdict'] = 'NO_TARGET_SHADERS'
                rec['why'] = ('shader data only for ' + ','.join(rec['inline_shader_formats'])
                              if rec['inline_shader_formats'] else 'no shader data and no target library')
            else:
                rec['verdict'] = 'UNKNOWN'
                rec['why'] = 'mod carries target-format data; not decoded here'
            return rec['verdict']
        # MIC without own shader data: parent decides
        parents = [i for i in rec['imports'] if i in mats]
        base_parents = [i for i in rec['imports'] if i not in packages and
                        base_store.get(cityhash.ue_name_hash(i), {}).get('shader_maps')]
        if len(parents) == 1 and not base_parents:
            rec['parent'] = parents[0]
            v = resolve(parents[0], depth + 1)
            rec['verdict'] = 'INHERITS_MISSING' if v in ('NO_TARGET_SHADERS', 'INHERITS_MISSING') else (
                'OK_BASE_SHADERS' if v == 'OK_BASE_SHADERS' else 'UNKNOWN')
            rec['why'] = f'parent {parents[0]} is {v}'
        elif len(base_parents) == 1 and not parents:
            rec['parent'] = base_parents[0]
            rec['verdict'] = 'OK_BASE_SHADERS'
            rec['why'] = 'parent is a base package with target shader maps in the game store'
        else:
            rec['verdict'] = 'UNKNOWN'
            rec['why'] = f'parent not provable (mod candidates {parents}, base candidates {base_parents})'
        return rec['verdict']

    for n in mats:
        resolve(n)
    unknown_store = sorted(r['package'] for r in mats.values()
                           if r['overrides_base'] and r['base_had_target_shader_maps'] is None)
    for rec in mats.values():
        v = rec['verdict']
        if v in ('NO_TARGET_SHADERS', 'INHERITS_MISSING'):
            if rec['overrides_base']:
                rec['action_options'] = ['restore-base (drop this override: base look returns, mod intent lost)',
                                         'reparent-to-base-material (keep mod intent where parameters allow)']
            else:
                rec['action_options'] = ['reparent-to-base-material (needs base parameter names)',
                                         'leave (renders as engine default checker)']
        else:
            rec['action_options'] = []
    counts = {}
    for rec in mats.values():
        counts[rec['verdict']] = counts.get(rec['verdict'], 0) + 1
    return {'label': 'Mac-only', 'tool': 'mod_material_audit.py', 'target_format_platform': target_suffix,
            'mod_shader_libraries': mod_libs,
            'packages_checked': len(packages), 'material_exports_checked': len(mats),
            'verdict_counts': counts,
            'overrides_with_container_header_not_loaded': unknown_store,
            'materials': sorted(mats.values(), key=lambda r: r['package'])}


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--mod-utoc', required=True)
    ap.add_argument('--mod-ucas', required=True)
    ap.add_argument('--script-utoc', required=True)
    ap.add_argument('--script-ucas', required=True)
    ap.add_argument('--base-toc', nargs='+', required=True)
    ap.add_argument('--base-header', nargs='+', required=True)
    ap.add_argument('--target', required=True, help="format-platform suffix, e.g. from shader_inputs.py")
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    so = load_script_objects(a.script_utoc, a.script_ucas)
    rep = audit(a.mod_utoc, a.mod_ucas, so, a.base_toc, a.base_header, a.target)
    with open(a.out, 'w') as fh:
        json.dump(rep, fh, indent=1)
    print(f"mod_material_audit: checked {rep['packages_checked']} packages, "
          f"{rep['material_exports_checked']} material exports: {rep['verdict_counts']}")
    if rep['overrides_with_container_header_not_loaded']:
        print(f"mod_material_audit: {len(rep['overrides_with_container_header_not_loaded'])} overridden base "
              f"packages have UNKNOWN base shader maps (pass that container's header with --base-header)")
    if rep['material_exports_checked'] == 0:
        print('mod_material_audit: FAIL (nothing checked)')
        return 1
    print('mod_material_audit: DONE')
    return 0


if __name__ == '__main__':
    sys.exit(main())
