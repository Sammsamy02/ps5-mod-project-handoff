#!/usr/bin/env python3
"""Classify every asset of a PC mod for the console build: loads as-is / needs converter X /
unsupported / unknown (Mac-only, read-only, generic for UE5 IoStore titles; fail closed).

Usage:
  classify_mod.py --mod UTOC UCAS [PAK|-] [--mod ...]
                  --script-utoc G.utoc --script-ucas G.ucas      (game's global container)
                  --base-toc T.utoc ...                          (game's own TOCs)
                  [--base-header CH.bin ...]                     (game's container headers: shader maps)
                  [--registry AssetRegistry.bin]                 (game's own registry: class of overridden assets)
                  [--pair-evidence pair-compare.json]            (pair_compare.py output)
                  --target FMT-PLAT                              (shader format-platform, from the game's files)
                  --out report.json [--md report.md]
Mod files are only read. Exit 1 when zero packages were checked.

Checks per package (every one prints into 'findings'):
  G1 package flags: cooked + editor-only-filtered, no editor/developer/uncooked bits; unversioned like the game
  G2 every script import (class, struct, function) exists in the console build's ScriptObjects
  G3 every imported package is in the mod or in the game's TOCs
  G4 export data tiles the package; bulk-data map readable; no compressed/optional/memory-mapped payloads
  G5 no PC shader code outside materials (shader-format tokens other than the console's own)
  G6 class rule (asset_rules.py) + per-class deep check:
       Texture2D  -> texture_package.layout: linear PC form => CONVERT texture-ps5-tiling
       Material/MIC -> mod_material_audit verdicts => CONVERT material-reparent or as-is
       SkeletalMesh / pure data / Blueprint -> LIKELY_AS_IS only when pair evidence shows the same shape for
         that class AND the package's bulk-flag signature was seen on a console sample of that class
       every other export class must be covered by the primary kind's allowlist (asset_rules.sub_allowed)
  G7 overriding a game asset: the game's class (registry) must be provable and equal, else UNKNOWN
  G8 safety: a package whose script calls save functions (engine save-slot functions, or any function of a
     class named *Save*, or a Save/Delete/Clear/Remove/Reset..Game/Data/Slot function) or defines a save-game
     class => UNKNOWN + a 'safety' flag (on the console that can change the user's real save data)
"""
import argparse
import collections
import json
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import asset_rules as AR  # noqa: E402
import jlib  # noqa: E402

TOOL = 'teamJ-classify-mod-1'


# The cooked registry lists a Blueprint package under its editor asset class; the cooked package itself
# carries the generated class.
GENERATED_CLASS = {
    '/Script/Engine/Blueprint': '/Script/Engine/BlueprintGeneratedClass',
    '/Script/Engine/AnimBlueprint': '/Script/Engine/AnimBlueprintGeneratedClass',
    '/Script/UMGEditor/WidgetBlueprint': '/Script/UMG/WidgetBlueprintGeneratedClass',
    '/Script/ControlRigDeveloper/ControlRigBlueprint': '/Script/ControlRig/ControlRigBlueprintGeneratedClass',
}


def registry_class(path):
    """'/Script/Engine.Texture2D' -> '/Script/Engine/Texture2D' (None for non-script classes);
    Blueprint editor classes map to the generated class the cooked package carries."""
    if not path or not path.startswith('/Script/') or '.' not in path:
        return None
    pkg, obj = path.rsplit('.', 1)
    c = pkg + '/' + obj
    return GENERATED_CLASS.get(c, c)


def primary_class(z, so):
    tops = []
    for e in z.exports:
        if e['outer'][0] != 'Null':
            continue
        c = jlib.class_path(z, e['class'], so)
        if c and not c.startswith('export:'):
            tops.append((e['name'], c))
    if not tops:
        return None, []
    short = z.name.rsplit('/', 1)[-1]
    named = [c for n, c in tops if n in (short, short + '_C')]
    known = [c for _n, c in tops if AR.rule_for(c) and AR.rule_for(c)['kind'] != 'sub']
    pick = named[0] if named else (known[0] if known else tops[0][1])
    return pick, sorted({c for _n, c in tops})


MAT_MAP = {
    'NO_TARGET_SHADERS': (AR.CONVERT, 'material-reparent', 'shader code only for other platforms'),
    'INHERITS_MISSING': (AR.CONVERT, 'material-reparent', 'parent chain ends at a material without console shaders'),
    'OK_BASE_SHADERS': (AR.LIKELY_AS_IS, None, 'parent is a game material with console shader maps '
                        '(parent link console-proven in launch B; look not separately proven)'),
    'UNKNOWN': (AR.UNKNOWN, None, 'material audit could not prove the parent chain'),
}


def judge_package(R, so, z, data, bulk_len, path, ctx, chunk_pid=None):
    """All checks for one parsed package -> report row (see the module docstring)."""
    F = []
    fp = jlib.fingerprint(R, z, data, so)
    cls, tops = primary_class(z, so)
    rule = AR.rule_for(cls) if cls else None
    pid = jlib.package_id(R, z.name)
    rec = {'path': path, 'package': z.name, 'package_id': f'{pid:016x}', 'bytes': len(data),
           'class': cls, 'top_level_classes': tops, 'rule_kind': rule['kind'] if rule else None,
           'converter': None, 'findings': F}
    # G1 identity + flags
    if chunk_pid is not None and chunk_pid != pid:
        F.append({'check': 'G1', 'verdict': AR.UNSUPPORTED,
                  'why': f'package name {z.name} does not hash to its chunk id {chunk_pid:016x}'})
    if z.flags & jlib.PKG_REFUSED:
        F.append({'check': 'G1', 'verdict': AR.UNSUPPORTED, 'why': f'editor/developer/uncooked package flags {z.flags:#x}'})
    if z.flags & jlib.PKG_REQUIRED != jlib.PKG_REQUIRED:
        F.append({'check': 'G1', 'verdict': AR.UNSUPPORTED, 'why': f'not a cooked, editor-filtered package ({z.flags:#x})'})
    if not z.flags & jlib.PKG_UNVERSIONED:
        F.append({'check': 'G1', 'verdict': AR.UNKNOWN, 'why': 'versioned properties (the game\'s packages are unversioned)'})
    # G2 script imports
    if fp['unresolved_script_imports']:
        F.append({'check': 'G2', 'verdict': AR.UNSUPPORTED,
                  'why': f"{len(fp['unresolved_script_imports'])} script import(s) not in the console build: "
                         f"{fp['unresolved_script_imports'][:4]}"})
    # G3 package imports
    missing = [n for n in z.imported_packages
               if jlib.package_id(R, n) not in ctx['mod_pids'] and jlib.package_id(R, n) not in ctx['base_ids']]
    if missing:
        F.append({'check': 'G3', 'verdict': AR.UNKNOWN,
                  'why': f'{len(missing)} imported package(s) neither in the mod nor in the game: {missing[:4]} '
                         '(the loader skips a missing import with a warning; whether that matters depends on '
                         'what uses it, so this is not proven harmless)'})
    # G4 structure and bulk data
    if fp['export_slices'] != 'tiled':
        F.append({'check': 'G4', 'verdict': AR.UNKNOWN, 'why': fp['export_slices']})
    if fp.get('bulk_entries') is None:
        F.append({'check': 'G4', 'verdict': AR.UNKNOWN, 'why': 'bulk-data map unreadable: ' + fp.get('bulk_error', '')})
    elif fp['bulk_refused_flags']:
        F.append({'check': 'G4', 'verdict': AR.UNKNOWN,
                  'why': f"bulk payload flags {fp['bulk_refused_flags']} (compressed/optional/mapped)"})
    # G5 PC shader code outside materials
    foreign = sorted(set(fp['shader_tokens'] or []) - ctx['targets'])
    kind = rule['kind'] if rule else None
    if foreign and kind != 'material':
        F.append({'check': 'G5', 'verdict': AR.UNSUPPORTED,
                  'why': f'carries shader code for {foreign} and no console shaders'})
    # G6 class rule + deep check
    if not rule:
        F.append({'check': 'G6', 'verdict': AR.UNKNOWN, 'why': f'no rule for class {cls}'})
    elif kind == 'texture':
        try:
            lay = R.texture_package.layout(data, bulk_len, allow_ps5=True)
            if lay['ps5']:
                F.append({'check': 'G6', 'verdict': AR.AS_IS, 'why': 'already in the console GPU layout'})
            else:
                F.append({'check': 'G6', 'verdict': AR.CONVERT, 'why': f"linear {lay['format']} "
                          f"{lay['size'][0]}x{lay['size'][1]}, {len(lay['mips'])} mips: needs tiling"})
                rec['converter'] = 'texture-ps5-tiling'
        except Exception as ex:
            F.append({'check': 'G6', 'verdict': AR.UNSUPPORTED, 'why': f'tiling converter refuses: {str(ex)[:120]}'})
    elif kind == 'material':
        m = ctx['mat_by_pkg'].get(z.name)
        if not m:
            F.append({'check': 'G6', 'verdict': AR.UNKNOWN, 'why': 'material audit has no row for this package'})
        else:
            v, conv, why = MAT_MAP.get(m['verdict'], (AR.UNKNOWN, None, 'unmapped material verdict'))
            F.append({'check': 'G6', 'verdict': v, 'why': f"{m['verdict']}: {why} ({m.get('why', '')})"})
            if conv:
                rec['converter'] = conv
            rec['material_audit'] = {k: m.get(k) for k in ('verdict', 'why', 'parent', 'inline_shader_formats',
                                                            'overrides_base')}
    elif kind == 'sub':
        F.append({'check': 'G6', 'verdict': AR.UNKNOWN, 'why': f'primary asset is a sub-object class ({cls})'})
    elif kind in ('skeletal_mesh', 'pure_data', 'blueprint'):
        ev = (ctx['pair_ev'] or {}).get(cls)
        sig = sorted(fp.get('bulk_flags') or [])
        if kind == 'pure_data' and fp.get('bulk_entries'):
            F.append({'check': 'G6', 'verdict': AR.UNKNOWN,
                      'why': f"{fp['bulk_entries']} bulk payload(s) in a data class (cooked geometry or blobs)"})
        elif ev and ev['shape'] == 'same-shape':
            seen = [sorted(x) for x in ev.get('console_bulk_signatures') or []]
            if sig not in seen:
                F.append({'check': 'G6', 'verdict': AR.UNKNOWN,
                          'why': f'bulk payload flags {sig} not seen on any console sample of this class ({seen[:3]})'})
            else:
                F.append({'check': 'G6', 'verdict': rule['verdict'],
                          'why': f"same shape as {ev['ps5_samples']} console sample(s) of this class; "
                                 f"{rule['evidence'][:160]}"})
        else:
            F.append({'check': 'G6', 'verdict': AR.UNKNOWN,
                      'why': 'no console sample of this class compared yet' if not ev or ev['shape'] == 'one-side-only'
                      else f"console sample differs: {ev['reasons'][:2]}"})
    else:
        F.append({'check': 'G6', 'verdict': rule['verdict'], 'why': (rule['notes'] or rule['evidence'])[:200]})
        if rule['verdict'] in (AR.CONVERT, AR.UNSUPPORTED) and rule['converter']:
            rec['converter'] = rule['converter']
    uncovered = set()
    for e in z.exports:
        c = jlib.class_path(z, e['class'], so)
        if c == cls:
            continue
        if c in AR.SUB_RULES:
            v, why = AR.SUB_RULES[c]
            if not any(f['check'] == 'G6-sub' and f['why'] == why for f in F):
                F.append({'check': 'G6-sub', 'verdict': v, 'why': why})
        if rule and not AR.sub_allowed(kind, c):
            uncovered.add(c)
    if uncovered:
        F.append({'check': 'G6-sub', 'verdict': AR.UNKNOWN,
                  'why': f'other export classes not covered by the evidence for {kind}: {sorted(map(str, uncovered))[:4]}'})
    # G7 overriding a game asset: the game's class for that name must be provable and equal
    if pid in ctx['base_ids']:
        rec['overrides_game'] = list(ctx['base_ids'][pid])
        a = ctx['registry'].asset(z.name) if ctx['registry'] is not None else None
        gc = registry_class(a['class']) if a else None
        if gc is None:
            F.append({'check': 'G7', 'verdict': AR.UNKNOWN,
                      'why': 'replaces a game asset whose class is not provable (no registry entry or no registry given)'})
        elif cls and gc != cls:
            F.append({'check': 'G7', 'verdict': AR.UNKNOWN,
                      'why': f'the game expects a {gc} under this name; the mod ships a {cls} (a PC trick, e.g. '
                             'to hide the original; engine behaviour should match PC but is untested)'})
    # G8 safety: script that writes or deletes the player's saves (console: the system save container)
    if fp['save_flags']:
        rec['safety'] = fp['save_flags']
        F.append({'check': 'G8', 'verdict': AR.UNKNOWN,
                  'why': f"save data ({', '.join(fp['save_flags'])[:200]}): on the console this can change the user's "
                         'real saves; needs the user\'s OK and a save backup first'})
    rec['verdict'] = AR.worst(f['verdict'] for f in F)
    rec['fingerprint'] = {k: fp[k] for k in ('flags', 'exports', 'export_classes', 'bulk_entries', 'bulk_flags',
                                             'shader_tokens', 'audio_format_names') if k in fp}
    return rec


def classify(R, so, mods, base_ids, store, target, registry=None, pair_ev=None):
    targets = set(target.split('-'))
    containers, entries, zen_by_name, mod_pids, all_libs = [], [], {}, set(), []
    for utoc, ucas, pak in mods:
        C = jlib.Container(R, utoc, ucas)
        cinfo = {'utoc': utoc, 'utoc_sha256': jlib.sha256_file(utoc), 'ucas_sha256': jlib.sha256_file(ucas),
                 'chunk_types': {R.utoc.TYPE_NAMES.get(k, str(k)): v for k, v in sorted(C.type_counts.items())},
                 'compression_methods': C.toc.methods, 'findings': []}
        for kind, n in C.other_chunks.items():
            v = AR.UNSUPPORTED if kind in ('ShaderCodeLibrary', 'ShaderCode') else AR.UNKNOWN
            cinfo['findings'].append({'check': 'G4', 'verdict': v,
                                      'why': f'{n} {kind} chunk(s): PC shader code cannot run on the console'
                                      if v == AR.UNSUPPORTED else f'{n} {kind} chunk(s) not classified'})
        if pak:
            try:
                ver, n, mount = jlib.read_pak_entry_count(pak)
                cinfo['pak'] = {'version': ver, 'entries': n, 'mount': mount}
                if n:
                    cinfo['findings'].append({'check': 'G4', 'verdict': AR.UNKNOWN,
                                              'why': f'classic pak has {n} entries: not classified here (#7 route)'})
            except jlib.JError as ex:
                cinfo['findings'].append({'check': 'G4', 'verdict': AR.UNKNOWN, 'why': f'classic pak: {ex}'})
        containers.append(cinfo)
        mod_libs = [C.toc.chunk_ids[r].hex() for r in C.toc.rows_of_type(8)]
        all_libs += mod_libs
        for row, vpath, pid, data in C.packages():
            mod_pids.add(pid)
            try:
                z = R.zen.ZenPackage(data)
            except Exception as ex:
                entries.append({'path': vpath, 'package_id': f'{pid:016x}', 'bytes': len(data),
                                'verdict': AR.UNSUPPORTED, 'findings': [
                                    {'check': 'G1', 'verdict': AR.UNSUPPORTED, 'why': f'package unreadable: {ex}'}]})
                continue
            zen_by_name[z.name] = z
            entries.append({'path': vpath, 'package_id': f'{pid:016x}', 'z': z, 'data': data,
                            'bulk_len': C.bulk_len.get(pid), 'mod_libs': mod_libs})
    base_tocnames = {pid: v[0] for pid, v in base_ids.items()}
    try:
        mat = R.mod_material_audit.audit_packages(zen_by_name, all_libs, so, base_tocnames, store, target)
        mat_by_pkg = {m['package']: m for m in mat['materials']}
    except Exception as ex:  # fail closed: every material becomes UNKNOWN
        mat, mat_by_pkg = {'error': str(ex)}, {}
    ctx = {'targets': targets, 'mod_pids': mod_pids, 'base_ids': base_ids, 'mat_by_pkg': mat_by_pkg,
           'registry': registry, 'pair_ev': pair_ev}
    out = []
    for ent in entries:
        if 'z' not in ent:
            out.append(ent)
            continue
        out.append(judge_package(R, so, ent['z'], ent['data'], ent['bulk_len'], ent['path'], ctx,
                                 int(ent['package_id'], 16)))
    cont_v = [f['verdict'] for c in containers for f in c['findings']]
    summary = collections.Counter(r['verdict'] for r in out)
    by_class = collections.defaultdict(collections.Counter)
    for r in out:
        by_class[r.get('class') or 'unreadable'][r['verdict']] += 1
    conv = collections.Counter(r['converter'] for r in out if r.get('converter') and r['verdict'] == AR.CONVERT)
    safety = {r['package']: r['safety'] for r in out if r.get('safety')}
    return {'tool': TOOL, 'label': 'Mac-only', 'readers_sha256': R.sha256, 'target': target,
            'containers': containers, 'packages_checked': len(out),
            'mod_verdict': AR.worst([r['verdict'] for r in out] + cont_v),
            'summary': dict(summary), 'by_class': {k: dict(v) for k, v in sorted(by_class.items())},
            'converters_needed': {k: {'packages': n, **AR.CONVERTERS.get(k, {})} for k, n in conv.items()},
            'material_audit_counts': mat.get('verdict_counts', mat.get('error')),
            'safety': safety,
            'packages': sorted(out, key=lambda r: (AR.SEVERITY[r['verdict']], r['path'] or ''))}


def to_markdown(rep, title):
    L = [f'# {title}', '', f"Mac-only. Tool `{rep['tool']}`. Packages checked: {rep['packages_checked']}. "
         f"Mod verdict (worst part): **{rep['mod_verdict']}**.", '',
         '| Class | ' + ' | '.join(AR.SEVERITY) + ' |', '|---|' + '---|' * len(AR.SEVERITY)]
    for cls, c in rep['by_class'].items():
        L.append(f'| `{cls}` | ' + ' | '.join(str(c.get(v, '')) for v in AR.SEVERITY) + ' |')
    if rep['converters_needed']:
        L += ['', 'Converters needed:', '']
        for k, v in rep['converters_needed'].items():
            L.append(f"- `{k}` for {v['packages']} package(s) (converter status: {v.get('status')})")
    if rep.get('safety'):
        L += ['', 'Safety (script that touches the player\'s save data):', '']
    for pkg, fl in rep.get('safety', {}).items():
        L.append(f"- SAFETY `{pkg}`: {', '.join(fl)}")
    for c in rep['containers']:
        for f in c['findings']:
            L.append(f"- container `{os.path.basename(c['utoc'])}`: {f['verdict']}: {f['why']}")
    L += ['', 'Non-as-is packages, worst first (first 40):', '']
    n = 0
    for r in sorted(rep['packages'], key=lambda r: -AR.SEVERITY[r['verdict']]):
        if r['verdict'] == AR.AS_IS:
            continue
        why = '; '.join(f"{f['check']} {f['verdict']}: {f['why']}" for f in r['findings']
                        if f['verdict'] == r['verdict'])
        name = (r.get('path') or '').split('/Content/')[-1]
        if r.get('package') and not (r.get('path') or '').endswith(r['package'].rsplit('/', 1)[-1] + '.uasset'):
            name += f" = package {r['package']}"
        L.append(f"- {r['verdict']} `{name}` ({r.get('class')}): {why[:240]}")
        n += 1
        if n >= 40:
            break
    return '\n'.join(L) + '\n'


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--mod', nargs='+', action='append', required=True, metavar='FILE',
                    help='UTOC UCAS [PAK or -] for one mod (repeat per mod)')
    ap.add_argument('--script-utoc', required=True)
    ap.add_argument('--script-ucas', required=True)
    ap.add_argument('--base-toc', nargs='+', required=True)
    ap.add_argument('--base-header', nargs='*', default=[])
    ap.add_argument('--registry', default=None)
    ap.add_argument('--pair-evidence', default=None)
    ap.add_argument('--target', required=True,
                    help="shader format-platform of the console build, read from the game's own files "
                         "(e.g. its ShaderTypeInfo-*-<format>-<platform>.stinfo pak entry)")
    ap.add_argument('--readers', default=jlib.READERS_DEFAULT)
    ap.add_argument('--title', default='PC mod classification')
    ap.add_argument('--out', required=True)
    ap.add_argument('--md', default=None)
    a = ap.parse_args(argv)
    R = jlib.Readers(a.readers)
    so = jlib.script_objects(R, a.script_utoc, a.script_ucas)
    base_ids = jlib.base_index(R, a.base_toc)
    store = jlib.base_store(R, a.base_header)
    registry = R.asset_registry.load(a.registry) if a.registry else None
    pair_ev = None
    if a.pair_evidence:
        with open(a.pair_evidence) as fh:
            pair_ev = json.load(fh)['classes']
    mods = []
    for m in a.mod:
        if len(m) not in (2, 3):
            ap.error('--mod takes UTOC UCAS [PAK or -]')
        mods.append((m[0], m[1], m[2] if len(m) == 3 and m[2] != '-' else None))
    rep = classify(R, so, mods, base_ids, store, a.target, registry, pair_ev)
    rep['inputs'] = {'script_utoc': a.script_utoc, 'script_ucas_sha256': jlib.sha256_file(a.script_ucas),
                     'base_tocs': a.base_toc, 'base_headers': a.base_header,
                     'registry': a.registry, 'registry_sha256': jlib.sha256_file(a.registry) if a.registry else None,
                     'pair_evidence': a.pair_evidence,
                     'pair_evidence_sha256': jlib.sha256_file(a.pair_evidence) if a.pair_evidence else None,
                     'mod_paks': [m[2] for m in mods]}
    with open(a.out, 'w') as fh:
        json.dump(rep, fh, indent=1)
    if a.md:
        with open(a.md, 'w') as fh:
            fh.write(to_markdown(rep, a.title))
    print(f"classify_mod: checked {rep['packages_checked']} packages: {rep['summary']}; "
          f"mod verdict {rep['mod_verdict']}")
    if rep['packages_checked'] == 0:
        print('classify_mod: FAIL (nothing checked)')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
