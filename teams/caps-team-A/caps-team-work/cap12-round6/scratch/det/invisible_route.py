#!/usr/bin/env python3
"""Invisible route for capability #12 (Mac-only, read-only, generic UE5).

Problem: a PC mod hides parts of the game's characters with its OWN "invisible"
materials. Those exist only as PC shader code, so on the console the engine draws
its default checker where PC draws nothing.

Rule (fail closed; nothing names a game, mod or asset):
  A mod MaterialInstanceConstant whose parent is a MOD-ONLY Material proven
  "draws nothing" may be re-pointed to a GAME Material proven "draws nothing"
  that has target-platform shader maps and is proven compiled for skeletal meshes.

  Mod parent proven "draws nothing" (all must hold):
    M1 a mod Material without target shaders, not overriding a game package;
    M2 source 1, its name: an asset-name token in INVISIBLE_TOKENS (a name that also has a
       NEGATIONS token, e.g. 'NotHidden', never counts);
    M3 source 2, its own settings: blend mode masked or translucent read from its
       OWN property block (reader C), and the compiled-shader readers A/B do not
       contradict it (no opaque-only class). Blend labels are whitelisted (Opaque / Masked /
       Translucent); any other label is refused;
    M4 constant graph: no name besides its own (no parameters) and no package
       imports (no textures, no functions), so nothing can switch parts visible.
  Game material proven "draws nothing" + usable (all must hold):
    G1 a Material (not an instance), in the game's container-header store WITH
       target shader maps, not overridden by the mod, header fetched whole and
       IoHash-verified (the overlay needs it);
    G2 source 1, its name: a token in INVISIBLE_TOKENS;
    G3 source 2, its blend: masked or translucent from the asset registry AND
       from the material's own compiled shader types (two-source class) AND from
       its own property block (reader C, decoder learned without it), all equal;
    G4 constant graph (as M4);
    G5 compiled for skeletal meshes, two sources: (a) game SkeletalMesh packages
       import it (store import graph), (b) its own property block carries the
       usage flag LEARNED from the game's materials (the one property index that
       is present in every master the game draws on skeletal meshes and absent in
       several masters used only by static meshes). It is learned once on all of
       them (the tool fails if it cannot be learned) and again without the candidate;
       both must give the same index.
  Row (one mod instance):
    R1 a MaterialInstanceConstant row the plan left as 'known-checker', verdict
       INHERITS_MISSING, its DIRECT parent is a proven mod parent, and a proven game
       material with the SAME blend group exists (masked -> masked, translucent ->
       translucent; best ranked: most skeletal users, then name);
    R2 no own parameters and no inline shader code (no static permutation), so
       the instance draws exactly what its parent draws;
    R3 effective users (who loads it at run time: game importers the mod does not
       replace + mod packages that import it, through instance chains) include at
       least one SkeletalMesh. All users skeletal -> action 'reparent';
       other users too -> 'reparent-partial' (the existing user decision
       'allow-partial', which then also covers "non-skeletal users unproven": the
       user must be told): the hide material is proven only for skeletal meshes; on
       other users the engine may still draw its default checker (same as today).
       'coverage' stays 1.0 (no parameter is lost); 'user_coverage' is the share of
       users proven. No skeletal user, no user at all, or an instance chain deeper
       than MAX_DEPTH -> refused. Mod-only users get their class from the game's
       global ScriptObjects (--script-utoc/--script-ucas); without it they count
       as 'unknown' (fail closed).
  Anything else stays 'known-checker' with the reason written out.

ROUND 6 (Team A, static meshes): a game target may ALSO be proven for static meshes:
    G6 two sources: (a) its own PS5 shader-map content names the vertex-factory parameter
       layout that the engine's static-mesh vertex factory uses (VF_PARAMS['StaticMesh']),
       (b) that token is LEARNED from the game's own materials: the ONE vertex-factory token
       present in every game material that a game StaticMesh draws (>= MIN_VF_TRAIN of them),
       learned again without the candidate (same token). The skeletal token is learned the
       same way and must agree with G5 for the candidate (method check, not a new proof).
  The users a target is proven for ('proven_user_classes') then count as proven in R3:
  all users proven -> 'reparent'; some -> 'reparent-partial'; none -> refused.
    R3b (round 6) full coverage through the GAME's own original: when the row overrides a game
       instance, its game-side parent chain (store import graph, game data only) ends in a game
       Material whose whole header is fetched and IoHash-verified, EVERY vertex-factory token in that
       master's own PS5 shader map belongs to a class the target is proven for, and every MOD user
       (mod packages that load it) is of a proven class, then every game user can only draw it through
       vertex factories the target supports (anything else falls back to the default material in the
       base game too) -> 'reparent' (users_proven_by 'original-master-vertex-factories').
  Vertex-factory tokens come only from the material's own top-level export (CookedSerialOffset).

Output: the reparent plan with qualifying rows replaced (only with --apply) and an
'invisible_route' report section. The overlay (derive_substitutes / paktx) reads
the rows exactly as it reads the normal reparent rows. Every gate prints its count
and the tool exits 1 (and writes nothing) when it checked zero rows, zero game
materials, zero mod Materials, zero training rows, or could not learn the usage flag.
"""
import argparse
import collections
import json
import os
import re
import sys

sys.dont_write_bytecode = True

INVISIBLE_TOKENS = ('invisible', 'invis', 'hide', 'hidden')
NEGATIONS = ('not', 'non', 'no', 'un')            # 'M_NotHidden', 'M_Un_Hide' do not say invisible
CAN_DRAW_NOTHING = ('masked', 'translucent')      # blend groups whose pixels can all be clipped / zero
MIN_SK_TRAIN = 10       # skeletal-used game masters needed to learn the usage flag
MIN_ST_ABSENT = 5       # static-only masters that must LACK the flag (else it is not a usage flag)
SKELETAL = 'SkeletalMesh'
INSTANCE = 'MaterialInstanceConstant'
MAX_DEPTH = 8           # instance chains deeper than this are not followed (row refused)
TOOL = 'cap12-invisible-route-3'
STATIC = 'StaticMesh'
# engine vertex-factory parameter layouts per user class (engine-level type names, not game data)
VF_PARAMS = {'SkeletalMesh': 'FGPUSkinVertexFactoryShaderParameters', 'StaticMesh': 'FLocalVertexFactoryShaderParameters'}
VF_TOKEN_RE = re.compile(rb'F[A-Za-z0-9]+VertexFactoryShaderParameters(?:VS|PS)?')
MIN_VF_TRAIN = 10       # game materials of a user class needed to learn its vertex-factory token


# ---------------------------------------------------------------- pure rules
def name_tokens(asset_name):
    """'M_InvisibleMesh' -> ['m', 'invisible', 'mesh']; splits on _ - . and camel case."""
    out = []
    for part in re.split(r'[_\-.\s]+', asset_name or ''):
        out += [t.lower() for t in re.findall(r'[A-Z]+(?![a-z])|[A-Z]?[a-z]+|\d+', part)]
    return out


def name_says_invisible(asset_name):
    toks = name_tokens(asset_name)
    if any(t in NEGATIONS for t in toks):
        return None                                # a negated name is never a source (fail closed)
    hits = [t for t in toks if t in INVISIBLE_TOKENS]
    return hits[0] if hits else None


def blend_group(label):
    """Explicit whitelist; any other label (Additive, Modulate, unknown) -> None (refused)."""
    return {'BLEND_Opaque': 'opaque', 'BLEND_Masked': 'masked', 'BLEND_Translucent': 'translucent'}.get(label)


def constant_graph(names, own_names, imports):
    """-> None if the material has no parameter/texture/function inputs, else why not."""
    extra = sorted(set(names) - set(own_names))
    if extra:
        return 'has names besides its own (parameters or values): ' + ', '.join(extra[:6])
    if imports:
        return 'imports packages (textures / functions): ' + ', '.join(sorted(imports)[:6])
    return None


def decide_mod_parent(f):
    """f: facts of one mod Material. -> {'proven', 'why' | 'sources'}."""
    out = {'package': f['package'], 'proven': False}
    if f.get('audit_class') != 'Material' or f.get('audit_verdict') != 'NO_TARGET_SHADERS':
        out['why'] = 'not a mod Material without target shaders'
        return out
    if f.get('overrides_base'):
        out['why'] = 'overrides a game package (only mod-only parents are re-pointed)'
        return out
    tok = name_says_invisible(f.get('asset_name'))
    if not tok:
        out['why'] = 'source 1 missing: the name does not say invisible/hide'
        return out
    rc = f.get('reader_c') or {}
    if not rc.get('readable'):
        out['why'] = 'source 2 missing: own blend mode unreadable (reader C): ' + str(rc.get('why'))
        return out
    g = blend_group(rc.get('blend'))
    if g not in CAN_DRAW_NOTHING:
        out['why'] = f"source 2 contradicts: own blend mode is {rc.get('blend')} (an opaque material draws its surface)"
        return out
    ab = f.get('readers_ab') or {}
    if ab.get('readable') and ab.get('blend_group') != g:
        out['why'] = f"compiled-shader readers say {ab.get('blend_group')}, own property block says {g}"
        return out
    classes = ab.get('reader_a_classes')
    if classes is not None and classes and all(c.endswith('/opaque') for c in classes):
        out['why'] = 'compiled shader types fit only opaque game materials'
        return out
    cg = constant_graph(f.get('names', []), f.get('own_names', []), f.get('imports', []))
    if cg:
        out['why'] = 'not a constant graph: ' + cg
        return out
    out.update(proven=True, blend=rc['blend'], models=rc.get('models'),
               sources={'name': f"token '{tok}' in {f.get('asset_name')}",
                        'own_property_block': f"blend {rc['blend']} (reader C)",
                        'compiled_pc_shader_types': (', '.join(classes) if classes else 'not readable') +
                        ('' if ab.get('readable') else f" ({ab.get('why') or 'readers A/B unreadable'})")})
    return out


def decide_game_target(f):
    """f: facts of one game Material. -> {'proven', 'why' | 'sources'}."""
    out = {'package': f['package'], 'proven': False}
    if f.get('registry_class') != 'Material':
        out['why'] = 'not a Material in the asset registry (instances carry their own inputs)'
        return out
    if not f.get('shader_maps'):
        out['why'] = 'no target shader maps in the game store'
        return out
    if f.get('overridden_by_mod'):
        out['why'] = 'overridden by the mod'
        return out
    if not f.get('header_whole_verified'):
        out['why'] = 'header not fetched whole + IoHash-verified'
        return out
    tok = name_says_invisible(f.get('asset_name'))
    if not tok:
        out['why'] = 'source 1 missing: the name does not say invisible/hide'
        return out
    tsc = f.get('two_source_class') or {}
    rc = f.get('reader_c') or {}
    if not tsc.get('readable'):
        out['why'] = 'class not readable from registry + compiled shader types: ' + str(tsc.get('why'))
        return out
    if tsc.get('domain') != 'MD_Surface':
        out['why'] = f"domain {tsc.get('domain')} is not a surface material"
        return out
    if not rc.get('readable'):
        out['why'] = 'own property block unreadable (reader C): ' + str(rc.get('why'))
        return out
    if rc.get('blend') != tsc.get('blend'):
        out['why'] = f"sources disagree on blend: registry {tsc.get('blend')}, own property block {rc.get('blend')}"
        return out
    g = blend_group(tsc.get('blend'))
    if g not in CAN_DRAW_NOTHING:
        out['why'] = f"blend {tsc.get('blend')}: an opaque material draws its surface"
        return out
    cg = constant_graph(f.get('names', []), f.get('own_names', []), f.get('imports', []))
    if cg:
        out['why'] = 'not a constant graph: ' + cg
        return out
    sk = sorted(f.get('skeletal_users') or [])
    if not sk:
        out['why'] = 'no game SkeletalMesh imports it (skeletal-mesh use unproven)'
        return out
    flag = f.get('usage_flag') or {}
    if flag.get('index') is None:
        out['why'] = 'skeletal usage flag not learned: ' + str(flag.get('why'))
        return out
    if flag.get('global_index') is not None and flag['index'] != flag['global_index']:
        out['why'] = (f"usage flag learned without it is index {flag['index']}, learned on all game materials "
                      f"index {flag['global_index']}: not stable")
        return out
    if not flag.get('present'):
        out['why'] = (f"skeletal usage flag (property index {flag['index']}) absent from its own property block"
                      + (f": {flag['present_why']}" if flag.get('present_why') else ''))
        return out
    st_ok, st_src = static_proof(f)
    out.update(proven=True, blend=tsc['blend'], models=tsc.get('models'), skeletal_users=sk,
               proven_user_classes=[SKELETAL] + ([STATIC] if st_ok else []),
               static_mesh=(st_src if st_ok else {'proven': False, 'why': st_src}),
               sources={'name': f"token '{tok}' in {f.get('asset_name')}",
                        'asset_registry': tsc.get('sources', {}).get('asset_registry'),
                        'compiled_shader_types': tsc.get('sources', {}).get('shader_type_peers'),
                        'own_property_block': f"blend {rc['blend']} (reader C, learned without it)",
                        'skeletal_use': f'{len(sk)} game SkeletalMesh packages import it',
                        'skeletal_usage_flag': f"property index {flag['index']} present "
                                               f"(learned from {flag.get('trained')} game masters, without it)"})
    return out


def rank_targets(targets):
    """Deterministic: most skeletal users, then package name."""
    return sorted((t for t in targets if t['proven']), key=lambda t: (-len(t['skeletal_users']), t['package']))


def decide_row(row, f, parents, targets):
    """row: a plan row; f: row facts {'own_params', 'own_shader_tokens', 'users', 'users_truncated'};
    parents: {package: decide_mod_parent result}; targets: ranked proven game targets (a list; a single
    dict or None is accepted too). The target must have the SAME blend group as the mod parent.
    -> (new row or None, why)."""
    if isinstance(targets, dict):
        targets = [targets]
    if row.get('action') != 'known-checker':
        return None, f"plan action is {row.get('action')} (only known-checker rows are considered)"
    if row.get('class') != INSTANCE or row.get('verdict') != 'INHERITS_MISSING':
        return None, 'not an instance that inherits missing shaders (a Material has no parent to re-point)'
    par = parents.get(row.get('old_parent'))
    if par is None or not par['proven']:
        return None, 'direct parent not proven invisible: ' + (par['why'] if par else 'parent is not a mod Material')
    same = [t for t in (targets or []) if t.get('proven') and blend_group(t.get('blend')) == blend_group(par['blend'])]
    if not same:
        return None, (f"no game material proven invisible + skeletal with the same blend group as the mod parent "
                      f"({blend_group(par['blend'])})")
    target = same[0]
    ids = row.get('_ids') or {}
    if not all(ids.get(k) for k in ('pkg', 'from', 'to')):
        return None, 'package ids for the store repoint missing'
    if f.get('own_params') or row.get('params'):
        return None, 'instance has own parameters'
    if f.get('own_shader_tokens'):
        return None, 'instance carries its own shader code (static permutation)'
    if f.get('users_truncated'):
        return None, f'instance chain deeper than {MAX_DEPTH}: users not fully known'
    users = collections.Counter(f.get('users') or {})
    proven_cls = list(target.get('proven_user_classes') or [SKELETAL])
    n_all = sum(users.values())
    n_sk = sum(users.get(c, 0) for c in proven_cls)
    via_master, master_why = None, None
    om = f.get('original_master') or {}
    if n_all and n_sk < n_all:
        if om.get('tokens') is None:
            master_why = 'original master: ' + str(om.get('why') or 'not known')
        else:
            have = {VF_PARAMS[c] for c in proven_cls if c in VF_PARAMS}
            missing = sorted(set(om['tokens']) - have)
            mod_bad = sorted(set(f.get('mod_user_classes') or ()) - set(proven_cls))
            if missing:
                master_why = ('the game draws its original (%s) with vertex factories the hide material lacks: %s'
                              % (om['master'].rsplit('/', 1)[-1], ', '.join(missing)))
            elif mod_bad:
                master_why = 'mod users of unproven classes: ' + ', '.join(mod_bad)
            else:
                via_master = om
    if via_master is not None:
        n_sk = n_all
    if n_all == 0:
        return None, ('no run-time user: nothing the game or the mod loads imports it (directly or through '
                      'instances), so nothing draws it; left as is (soft references are not checked)')
    if n_sk == 0:
        return None, ('no ' + '/'.join(proven_cls) + ' user (users: ' +
                      (', '.join(f'{k} x{v}' for k, v in sorted(users.items())) or 'none found') +
                      '); the hide material is proven only for ' + ' and '.join(proven_cls) +
                      ('; ' + master_why if master_why else ''))
    action = 'reparent' if n_sk == n_all else 'reparent-partial'
    other = {} if via_master is not None else {k: v for k, v in users.items() if k not in proven_cls}
    notes = [f"invisible route: parent {row['old_parent']} draws nothing on PC "
             f"({'; '.join(par['sources'].values())}); new parent {target['package']} draws nothing on the "
             f"console ({target['sources']['name']}; blend {target['blend']} from registry, shader types and "
             'own property block)']
    if via_master is not None:
        notes.append('every game user is covered: the game draws its original (%s) only with %s, which the hide '
                     'material is proven for' % (via_master['master'].rsplit('/', 1)[-1], ', '.join(via_master['tokens'])))
    if other:
        notes.append('proven only for the ' + '/'.join(proven_cls) + ' users; on ' +
                     ', '.join(f'{k} x{v}' for k, v in sorted(other.items()))
                     + ' the hide material may lack that usage and the engine then still draws its default '
                     'checker there (same as without this route)')
    from_id, to_id, pid = ids['from'], ids['to'], ids['pkg']
    new = {k: v for k, v in row.items() if k not in ('why', 'parameterless', 'action', '_ids')}
    new.update({
        'action': action, 'route': 'invisible',
        'known_checker_before': row.get('why'),
        'new_parent': target['package'], 'new_export': target['export'], 'new_parent_class': 'Material',
        'param_map': {}, 'lost_params': {}, 'param_renames': {},
        'coverage': 1.0,                 # parameters: the instance has none, nothing is lost
        'user_coverage': round(n_sk / n_all, 3),   # share of run-time users proven (proven_user_classes)
        'proven_user_classes': proven_cls,
        'users_proven_by': ('original-master-vertex-factories' if via_master is not None else 'user-classes'),
        'original_master': ({'master': via_master['master'], 'vertex_factories': via_master['tokens']}
                            if via_master is not None else None),
        'users': dict(sorted(users.items())),
        'mod_class': {'readable': True, 'blend_group': blend_group(par['blend']),
                      'note': 'invisible route: blend from the own property block + name (two sources)'},
        'mod_model': {'readable': True, 'blend': par['blend'], 'models': par.get('models')},
        'invisible_evidence': {'mod_parent': par['sources'], 'game_target': target['sources']},
        'candidate_verified': 'iohash', 'skeletal_mesh_evidence': target['skeletal_users'],
        'look_notes': notes,
        'store_entry_repoint': {'package_id': pid, 'from_package_id': from_id, 'to_package_id': to_id},
    })
    if action == 'reparent-partial':
        new['partial_reason'] = ('users: proven for ' + '/'.join(proven_cls) + ' users only (the allow-partial '
                                 'decision covers this too: tell the user)' + ('; ' + master_why if master_why else ''))
    return new, None


def learn_usage_flag(train, exclude=()):
    """train: [(package, present property indices (set), user classes (set))] of game Materials.
    -> {'index', 'trained', 'why'}: the ONE property index present in every skeletal-used master
    and absent in >= MIN_ST_ABSENT static-only masters, learned without `exclude`."""
    sk = [p for pkg, p, u in train if pkg not in exclude and SKELETAL in u]
    st = [p for pkg, p, u in train if pkg not in exclude and u == {'StaticMesh'}]
    if len(sk) < MIN_SK_TRAIN:
        return {'index': None, 'trained': len(sk), 'why': f'only {len(sk)} skeletal-used game masters (need {MIN_SK_TRAIN})'}
    common = set.intersection(*sk)
    found = sorted(k for k in common if sum(1 for p in st if k not in p) >= MIN_ST_ABSENT)
    if len(found) != 1:
        return {'index': None, 'trained': len(sk),
                'why': f'{len(found)} property indices fit (need exactly 1): {found[:8]}'}
    return {'index': found[0], 'trained': len(sk), 'static_only_trained': len(st),
            'static_only_absent': sum(1 for p in st if found[0] not in p)}


def vf_tokens(blob):
    """Vertex-factory shader-parameter layout names in a material's own export bytes (its frozen
    PS5 shader-map content lists the parameter layout of every vertex factory it was compiled for)."""
    return {m.group().decode('ascii') for m in VF_TOKEN_RE.finditer(blob or b'')}


def learn_vf_token(train, user_class, exclude=(), need_absent_in_static_only=False):
    """train: [(package, vf token set, user classes)] of game Materials. -> {'token', 'trained', 'why'}:
    the ONE token present in every material with a `user_class` user (and, for the skeletal check, absent
    in >= MIN_ST_ABSENT static-only materials), learned without `exclude`."""
    pos = [t for pkg, t, u in train if pkg not in exclude and user_class in u]
    st = [t for pkg, t, u in train if pkg not in exclude and u == {STATIC}]
    if len(pos) < MIN_VF_TRAIN:
        return {'token': None, 'trained': len(pos), 'why': f'only {len(pos)} game materials with a {user_class} user '
                                                             f'(need {MIN_VF_TRAIN})'}
    common = set.intersection(*pos)
    if need_absent_in_static_only:
        common = {k for k in common if sum(1 for t in st if k not in t) >= MIN_ST_ABSENT}
    if len(common) != 1:
        return {'token': None, 'trained': len(pos), 'why': f'{len(common)} tokens fit (need exactly 1): {sorted(common)[:6]}'}
    tok = next(iter(common))
    return {'token': tok, 'trained': len(pos), 'static_only_absent': sum(1 for t in st if tok not in t)}


def static_proof(f):
    """G6 for one game target. f['vf']: {'tokens', 'static', 'static_global', 'skeletal', 'skeletal_global'}.
    -> (proven: bool, why or sources)."""
    vf = f.get('vf') or {}
    toks = vf.get('tokens')
    if toks is None:
        return False, 'own export unreadable for vertex-factory tokens'
    lt, lg = vf.get('static') or {}, vf.get('static_global') or {}
    if not lt.get('token'):
        return False, 'static-mesh token not learned: ' + str(lt.get('why'))
    if lt['token'] != lg.get('token'):
        return False, f"static-mesh token learned without it ({lt['token']}) differs from all ({lg.get('token')})"
    if lt['token'] != VF_PARAMS[STATIC]:
        return False, f"learned static-mesh token {lt['token']} is not the engine's {VF_PARAMS[STATIC]}"
    if lt['token'] not in toks:
        return False, f"its own shader map lacks {lt['token']}"
    sk = vf.get('skeletal') or {}
    if sk.get('token') != VF_PARAMS[SKELETAL] or VF_PARAMS[SKELETAL] not in toks:
        return False, ('method check failed: the skeletal vertex-factory token is not learned as '
                       f"{VF_PARAMS[SKELETAL]} or is absent although G5 proved skeletal use")
    return True, {'static_vertex_factory': f"own PS5 shader map lists {lt['token']} (engine static-mesh "
                                           'vertex factory parameters)',
                  'static_learned': f"the one token in all {lt['trained']} game materials drawn by game "
                                    'StaticMeshes (learned without it)',
                  'method_check': f"skeletal token learned the same way = {sk.get('token')} (agrees with G5)"}


def effective_users(pkg, game_importers, mod_imports, cls_of, overridden, depth=MAX_DEPTH, names=None,
                    state=None):
    """Who loads `pkg` at run time by hard import: game importers the mod does not replace,
    plus mod packages that import it; instances are walked through. -> Counter of user
    classes ('unknown' if the class is not known). `names`, if a dict, receives
    class -> sorted user packages; `state`, if a dict, gets 'truncated' = True when an instance
    chain is deeper than `depth` (the caller refuses then)."""
    users = collections.Counter()
    seen, front = {pkg}, [pkg]
    for _ in range(depth):
        nxt = []
        for p in front:
            imp = {q for q in game_importers.get(p, ()) if q not in overridden}
            imp |= {q for q, ims in mod_imports.items() if p in ims}
            for q in sorted(imp):
                if q in seen:
                    continue
                seen.add(q)
                c = cls_of(q) or 'unknown'
                if c == INSTANCE:
                    nxt.append(q)
                else:
                    users[c] += 1
                    if names is not None:
                        names.setdefault(c, []).append(q)
        front = nxt
    if state is not None:
        state['truncated'] = bool(front)
    return users


# ---------------------------------------------------------------- fact gathering (real files)
def _short(cls):
    return cls.rsplit('.', 1)[-1] if cls else None


def gather_and_plan(a):
    tools = a.tools_dir or os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, tools)
    import asset_registry  # noqa: E402
    import cityhash  # noqa: E402
    import container_header  # noqa: E402
    import material_class as mc  # noqa: E402
    import material_props as mprops  # noqa: E402
    import mod_material_audit as mma  # noqa: E402
    import reparent_plan as rp  # noqa: E402
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import mesh_sections  # noqa: E402  (round 6: CookedSerialOffset export slices)
    import base_material_route as bmr  # noqa: E402  (round 6: report-only decisions for base Materials)
    import param_roles  # noqa: E402

    def fail(why):
        print('invisible_route: FAIL (%s)' % why)
        return None, 1

    plan = json.load(open(a.plan))
    audit = json.load(open(a.audit))
    pk, _libs = mma.load_mod(a.mod_utoc, a.mod_ucas)
    script = mma.load_script_objects(a.script_utoc, a.script_ucas) if a.script_utoc else None
    store = {}
    for hp in a.base_header:
        store.update(container_header.parse(open(hp, 'rb').read())[1])
    checked = [r for p in a.checked_headers for r in json.load(open(p))['checked'] if 'verified' in r]
    headers = {r['package']: r for r in checked}
    registry = asset_registry.load(a.asset_registry)
    typeidx = rp.load_type_index(a.library, a.stinfo, store)
    peers = mc.Peers(registry, typeidx)
    whole = rp.fetched_packages(a.fetched_dir, checked)
    rc = mprops.ReaderC(mprops.training_set(whole, registry, mc.MATERIAL_AR_CLASS), peers.bits)
    if not rc.full.ok:
        return fail('reader C not learned: %s' % rc.full.why)

    by_id = {cityhash.ue_name_hash(x['package']): x for x in registry.assets}
    mod_names = set(pk)
    mod_ids = {cityhash.ue_name_hash(n) for n in mod_names}
    audit_by = {m['package']: m for m in audit['materials']}

    def mod_script_class(name):
        """Class of a mod-only package from its top-level export's script import (global ScriptObjects)."""
        p = pk.get(name)
        if p is None or script is None:
            return None
        tops = [e for e in p.exports if e['outer'][0] == 'Null']
        if len(tops) != 1:
            return None
        d = p.describe_ref(tops[0]['class'], script)
        return (d.get('path') or '').rsplit('/', 1)[-1] or None if d else None

    def cls_of(name):
        a_ = registry.asset(name)
        if a_:
            return _short(a_['class'])
        m = audit_by.get(name)
        if m:
            return m['class']
        return mod_script_class(name)

    # game import graph by package name (store keys are ids)
    game_importers = collections.defaultdict(set)
    for q, v in store.items():
        qa = by_id.get(q)
        if qa is None:
            continue
        for i in v['imports']:
            ia = by_id.get(i)
            if ia is not None:
                game_importers[ia['package']].add(qa['package'])
    mod_imports = {n: set(p.imported_packages) for n, p in pk.items()}
    overridden = {n for n in mod_names if registry.asset(n)}
    game_imports = collections.defaultdict(set)
    for q, ims in game_importers.items():
        for i in ims:
            game_imports[i].add(q)

    def present_indices(blob):
        """-> (set of present non-zero property indices, None) or (None, why)."""
        if not blob:
            return None, 'no single top-level export'
        try:
            ents, _ = mprops.parse_header(blob)
        except mprops.PropsError as e:
            return None, 'property header unreadable: %s' % e
        return {i for i, zero in ents if not zero}, None

    def own_export(z):
        """Top-level export bytes by CookedSerialOffset (None if not exactly one / not tiling)."""
        try:
            tops = [b for e, b in mesh_sections.export_slices(z) if e['outer'][0] == 'Null']
        except mesh_sections.MeshError:
            return None
        return tops[0] if len(tops) == 1 else None

    # usage-flag training: whole fetched game Materials, users from the GAME graph only
    train, vf_train = [], []
    for pkg, z in sorted(whole.items()):
        a_ = registry.asset(pkg)
        if not a_ or a_['class'] != mc.MATERIAL_AR_CLASS:
            continue
        pres, _why = present_indices(mprops.top_export_bytes(z))
        if pres is None:
            continue
        ucls = set(effective_users(pkg, game_importers, {}, cls_of, set()))
        train.append((pkg, pres, ucls))
        ob = own_export(z)
        if ob is not None:
            vf_train.append((pkg, vf_tokens(ob), ucls))
    vf_static = learn_vf_token(vf_train, STATIC)
    vf_skel = learn_vf_token(vf_train, SKELETAL, need_absent_in_static_only=True)
    print(f'invisible_route: vertex-factory training rows: {len(vf_train)} whole game Materials '
          f'({sum(1 for t in vf_train if STATIC in t[2])} drawn by StaticMeshes, '
          f'{sum(1 for t in vf_train if SKELETAL in t[2])} by SkeletalMeshes); static-mesh token '
          f"{vf_static.get('token') or 'NOT learned: ' + str(vf_static.get('why'))}; skeletal token "
          f"{vf_skel.get('token') or 'NOT learned: ' + str(vf_skel.get('why'))}")
    if not vf_train:
        return fail('zero vertex-factory training rows')
    glob = learn_usage_flag(train)
    print(f'invisible_route: usage-flag training rows: {len(train)} whole game Materials '
          f'({sum(1 for t in train if SKELETAL in t[2])} skeletal-used, '
          f"{sum(1 for t in train if t[2] == {'StaticMesh'})} static-only); learned index {glob.get('index')}"
          + (f" (absent in {glob['static_only_absent']} static-only)" if glob.get('index') is not None else
             f": {glob.get('why')}"))
    if not train:
        return fail('zero usage-flag training rows')
    if glob.get('index') is None:
        return fail('skeletal usage flag not learned: %s' % glob.get('why'))

    # game targets: every registry Material whose name says invisible
    scanned = 0
    targets = []
    for x in registry.assets:
        if x['class'] != mc.MATERIAL_AR_CLASS:
            continue
        scanned += 1
        pkg = x['package']
        asset_name = pkg.rsplit('/', 1)[-1]
        if not name_says_invisible(asset_name):
            continue
        pid = cityhash.ue_name_hash(pkg)
        st = store.get(pid)
        z = whole.get(pkg)
        f = {'package': pkg, 'asset_name': asset_name, 'registry_class': 'Material',
             'shader_maps': len(st['shader_maps']) if st else 0, 'overridden_by_mod': pid in mod_ids,
             'header_whole_verified': z is not None and (headers.get(pkg) or {}).get('verified') == 'iohash'}
        if z is not None:
            blob = mprops.top_export_bytes(z)
            f['two_source_class'] = mc.base_props(pkg, registry, typeidx, peers, headers)
            f['reader_c'] = rc.base(pkg, blob, {pkg}) if blob else {'readable': False, 'why': 'no single top export'}
            f['names'], f['own_names'], f['imports'] = list(z.names), [asset_name, pkg], list(z.imported_packages)
            fl = dict(learn_usage_flag(train, exclude={pkg}), global_index=glob['index'])
            pres, why = present_indices(blob)
            fl['present'] = pres is not None and fl.get('index') is not None and fl['index'] in pres
            if pres is None:
                fl['present_why'] = why
            f['usage_flag'] = fl
            f['skeletal_users'] = sorted(q for q in game_importers.get(pkg, ()) if cls_of(q) == SKELETAL)
            ob = own_export(z)
            f['vf'] = {'tokens': sorted(vf_tokens(ob)) if ob is not None else None,
                       'static': learn_vf_token(vf_train, STATIC, exclude={pkg}), 'static_global': vf_static,
                       'skeletal': learn_vf_token(vf_train, SKELETAL, exclude={pkg}, need_absent_in_static_only=True)}
        d = decide_game_target(f)
        d['export'] = asset_name
        targets.append(d)
    ranked = rank_targets(targets)
    groups = sorted({blend_group(t['blend']) for t in ranked})
    print(f'invisible_route: scanned {scanned} game Materials; {len(targets)} named invisible/hide; '
          f'{len(ranked)} proven' + (f" (blend groups {groups})" if ranked else '') +
          ''.join(f"; {t['package'].rsplit('/', 1)[-1]} proven for {'+'.join(t['proven_user_classes'])}"
                  for t in ranked))
    if scanned == 0:
        return fail('zero game materials scanned')

    # mod parents: every mod Material
    parents = {}
    for m in audit['materials']:
        if m['class'] != 'Material':
            continue
        p = pk.get(m['package'])
        blob = mprops.top_export_bytes(p) if p else None
        asset_name = m['package'].rsplit('/', 1)[-1]
        f = {'package': m['package'], 'asset_name': asset_name, 'audit_class': m['class'],
             'audit_verdict': m['verdict'], 'overrides_base': m.get('overrides_base'),
             'reader_c': rc.mod(blob) if blob else {'readable': False, 'why': 'no export'},
             'readers_ab': mc.mod_props(blob, peers) if blob else {'readable': False},
             'names': list(p.names) if p else [], 'own_names': [asset_name, m['package']],
             'imports': list(p.imported_packages) if p else []}
        parents[m['package']] = decide_mod_parent(f)
    print(f'invisible_route: checked {len(parents)} mod Materials; '
          f'{sum(v["proven"] for v in parents.values())} proven to draw nothing')
    if not parents:
        return fail('zero mod Materials checked')

    # round 6: base Materials used directly by mesh slots (report only; rows are never changed here)
    decoded = {}

    def mesh_of(name):
        if name not in decoded:
            p = pk.get(name)
            if p is None:
                decoded[name] = None, 'mesh bytes not on the Mac (game package)'
            else:
                try:
                    decoded[name] = mesh_sections.decode(p.data, label=name), 'ok'
                except mesh_sections.MeshError as e:
                    decoded[name] = None, str(e)
        return decoded[name]

    base_report = []
    for m in audit['materials']:
        if m['class'] != 'Material':
            continue
        pkg = m['package']
        p = pk.get(pkg)
        blob = mprops.top_export_bytes(p) if p else None
        importers = {q for q in game_importers.get(pkg, ()) if q not in overridden}
        importers |= {q for q, ims in mod_imports.items() if pkg in ims}
        children, meshes, other = [], {}, collections.defaultdict(list)
        for q in sorted(importers):
            c = cls_of(q) or 'unknown'
            if c == INSTANCE:
                children.append(q)
            elif c == SKELETAL:
                meshes[q] = mesh_of(q)[1]
            else:
                other[c].append(q)
        dec = {q: mesh_of(q)[0] for q, st in meshes.items() if st == 'ok'}
        usage = mesh_sections.material_usage(dec).get(pkg, [])
        textures = []
        for q in sorted(p.imported_packages if p else []):
            if (cls_of(q) or '') == 'Texture2D':
                textures.append((q, param_roles.pixel_format(pk[q]) if q in pk else None))
        f = {'package': pkg, 'audit_class': m['class'], 'audit_verdict': m['verdict'],
             'overrides_base': m.get('overrides_base'), 'instance_children': children,
             'other_importers': dict(other), 'meshes': meshes, 'usage': usage,
             'parent_proof': parents.get(pkg), 'textures': textures,
             'reader_c': rc.mod(blob) if blob else {'readable': False},
             'readers_ab': mc.mod_props(blob, peers) if blob else {'readable': False}}
        base_report.append(bmr.decide(f))
    bc = collections.Counter(r['result'] for r in base_report)
    print(f'invisible_route: base-material route checked {len(base_report)} mod Materials, '
          f'{sum(1 for r in decoded.values() if r[1] == "ok")} meshes decoded '
          f'({sum(len(l["sections"]) for r in decoded.values() if r[0] for l in r[0]["lods"])} sections): '
          + ', '.join(f'{k} {v}' for k, v in sorted(bc.items())) + ' (report only, nothing shipped)')
    if not base_report:
        return fail('zero mod Materials checked by the base-material route')

    def original_master(pkg):
        """The game's own parent chain of a package the mod overrides (game store graph only)."""
        if pkg not in overridden:
            return {'why': 'not an override of a game package'}
        cur, seen = pkg, {pkg}
        for _ in range(MAX_DEPTH):
            mats = sorted(q for q in game_imports.get(cur, ()) if cls_of(q) in (INSTANCE, 'Material'))
            if len(mats) != 1:
                return {'why': '%d material imports in the game chain at %s' % (len(mats), cur)}
            cur = mats[0]
            if cur in seen:
                return {'why': 'cyclic game chain'}
            seen.add(cur)
            if cls_of(cur) == 'Material':
                z = whole.get(cur)
                if z is None or (headers.get(cur) or {}).get('verified') != 'iohash':
                    return {'master': cur, 'why': 'master %s not fetched whole + IoHash-verified' % cur}
                ob = own_export(z)
                if ob is None:
                    return {'master': cur, 'why': 'master export unreadable'}
                toks = sorted(vf_tokens(ob))
                if not toks:
                    return {'master': cur, 'why': 'no vertex-factory tokens in the master'}
                return {'master': cur, 'tokens': toks}
        return {'why': 'game chain deeper than %d' % MAX_DEPTH}

    rows, report = [], []
    considered = 0
    for row in plan['rows']:
        if row.get('action') != 'known-checker':
            rows.append(row)
            continue
        considered += 1
        p = pk.get(row['mod_package'])
        data = p.data if p else b''
        unames, st = {}, {}
        f = {'own_params': row.get('params'),
             'own_shader_tokens': data.count(b'DXBC') + data.count(b'DXIL') + data.count(b'PCD3D'),
             'users': effective_users(row['mod_package'], game_importers, mod_imports, cls_of, overridden,
                                      names=unames, state=st)}
        f['users_truncated'] = st.get('truncated')
        f['mod_user_classes'] = sorted(effective_users(row['mod_package'], {}, mod_imports, cls_of, overridden))
        f['original_master'] = original_master(row['mod_package'])
        r2 = dict(row)
        par = parents.get(row.get('old_parent')) or {}
        tgt = next((t for t in ranked if par.get('proven') and blend_group(t['blend']) == blend_group(par['blend'])),
                   None)
        if tgt is not None:
            r2['_ids'] = {'pkg': f"{cityhash.ue_name_hash(row['mod_package']):016x}",
                          'from': f"{cityhash.ue_name_hash(row['old_parent']):016x}",
                          'to': f"{cityhash.ue_name_hash(tgt['package']):016x}"}
        new, why = decide_row(r2, f, parents, ranked)
        report.append({'mod_package': row['mod_package'], 'old_parent': row.get('old_parent'),
                       'result': new['action'] if new else 'refused', 'why': why,
                       'users': dict(sorted(f['users'].items())),
                       'user_packages': {k: v[:8] for k, v in sorted(unames.items())}})
        if new is not None:
            new['user_packages'] = {k: v[:8] for k, v in sorted(unames.items())}
        rows.append(new if (new and a.apply) else row)
    print(f'invisible_route: considered {considered} known-checker rows of {len(plan["rows"])}; '
          f'{sum(1 for r in report if r["result"] == "reparent")} reparent, '
          f'{sum(1 for r in report if r["result"] == "reparent-partial")} reparent-partial, '
          f'{sum(1 for r in report if r["result"] == "refused")} refused '
          f'({sum(1 for r in report if (r["why"] or "").startswith("no run-time user"))} of them have no run-time user)'
          + ('' if a.apply else ' (dry run: rows unchanged, pass --apply)'))
    if considered == 0:
        return fail('zero known-checker rows considered')
    out = dict(plan)
    out['rows'] = rows
    out['invisible_route'] = {
        'tool': TOOL, 'label': 'Mac-only', 'applied': bool(a.apply),
        'tokens': list(INVISIBLE_TOKENS), 'negations': list(NEGATIONS), 'game_materials_scanned': scanned,
        'usage_flag': {'index': glob['index'], 'training_rows': len(train), 'skeletal_used': glob.get('trained'),
                       'static_only': glob.get('static_only_trained'),
                       'static_only_absent': glob.get('static_only_absent')},
        'usage_flag_training_rows': len(train),
        'vertex_factory_tokens': {'training_rows': len(vf_train), 'static': vf_static, 'skeletal': vf_skel,
                                  'engine_names': VF_PARAMS},
        'mod_classes_from_script_objects': script is not None,
        'targets': targets, 'chosen_targets': [t['package'] for t in ranked],
        'chosen_target': ranked[0]['package'] if ranked else None,
        'mod_parents': list(parents.values()), 'rows': report,
        'base_material_route': {'tool': bmr.TOOL, 'shipped': 0, 'materials': base_report}}
    return out, 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--plan', required=True, help='reparent_plan.py output (rows)')
    ap.add_argument('--audit', required=True)
    ap.add_argument('--mod-utoc', required=True)
    ap.add_argument('--mod-ucas', required=True)
    ap.add_argument('--checked-headers', nargs='+', required=True)
    ap.add_argument('--fetched-dir', nargs='+', required=True)
    ap.add_argument('--base-header', nargs='+', required=True)
    ap.add_argument('--asset-registry', required=True)
    ap.add_argument('--library', nargs='+', required=True)
    ap.add_argument('--stinfo', nargs='+', required=True)
    ap.add_argument('--script-utoc', help="the game's global.utoc (classes of mod-only packages); optional: "
                                          "without it they count as 'unknown' users (fail closed)")
    ap.add_argument('--script-ucas')
    ap.add_argument('--tools-dir', help='folder of the cap12 material tools (default: this file\'s folder)')
    ap.add_argument('--apply', action='store_true', help='replace qualifying rows (else report only)')
    ap.add_argument('--out', required=True)
    a = ap.parse_args(argv)
    if bool(a.script_utoc) != bool(a.script_ucas):
        ap.error('--script-utoc and --script-ucas go together')
    out, code = gather_and_plan(a)
    if out is None or code != 0:
        return code or 1
    with open(a.out, 'w') as fh:
        json.dump(out, fh, indent=1)
    print('invisible_route: DONE')
    return 0


if __name__ == '__main__':
    sys.exit(main())
