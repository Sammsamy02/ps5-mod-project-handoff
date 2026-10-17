#!/usr/bin/env python3
"""Route decisions for mod-only BASE Materials drawn directly by mesh slots (capability #12, round 6).

A mod Material (not an instance) without target-platform shaders cannot be re-parented: it has no parent.
When a mesh slot uses it DIRECTLY, the console draws the engine's checker on every section of that slot.
This module decides, from the game's and the mod's own data only, what such a material is. It is a pure
rule set (no file access); invisible_route.py gathers the facts and writes the result into the plan's
report section 'base_material_route'. It never changes a plan row (report only, fail closed).

Decision order (first match wins; every verdict carries its evidence):
  0  not a mod Material without target shaders                     -> 'not-this-route'
  1  overrides a game package                                      -> 'refused'
  2  no mesh slot uses it directly: instances use it as a parent   -> 'parent-only' (the instance routes
                                                                      handle it) / 'no-user'
  3  a mesh that uses it could not be decoded (mesh_sections)      -> 'refused' (fail closed)
  4  other run-time importers besides meshes and its own instances  -> 'refused' (users not provable)
  5  no section of any LOD uses its slots                           -> 'refused' (not provably unused)
     OPTION B, placeholder: every section of every LOD of every mesh that uses it is a placeholder
     (mesh_sections.section_placeholder: S1 section table, S2 geometry incl. enclosure, S3 engine UV
     density), and at least one such section exists               -> 'placeholder'
  6  OPTION B, draws nothing: the invisible route's own mod-parent proof (name + own blend + constant
     graph) holds                                                   -> 'draws-nothing'
  7  OPTION A, a real surface: some section is NOT a placeholder.  Needs, all from two sources:
       a) the textures it references (its imported Texture2D packages); none -> 'refused' (nothing to
          carry, roles cannot be proven);
       b) a role per texture: name suffix role (texture_name_role) AND pixel format role
          (format_role) must agree; any texture without a two-source role -> 'refused';
          two textures with the same role -> 'refused' (ambiguous);
       c) its blend group from its own property block (reader C) AND the compiled PC shader readers
          (A/B) must agree; disagreement / unreadable -> 'refused'. The group fixes the parent family:
          translucent only ever goes to translucent, masked to masked, opaque to opaque (never crossed);
                                                                    -> 'base-instance-candidate'
  'placeholder', 'draws-nothing' and 'base-instance-candidate' carry ship=False in this round: building
  a derived instance for a base Material (or re-pointing mesh slots, see mesh_slot_repoint.py) is not
  part of the shipping overlay yet; the parts stay as they are (fail closed) and the report says why.
"""
import re

TOOL = 'cap12-base-material-route-1'
SUFFIX_ROLE = {'d': 'base_color', 'bc': 'base_color', 'basecolor': 'base_color', 'albedo': 'base_color',
               'diffuse': 'base_color', 'col': 'base_color', 'color': 'base_color', 'colour': 'base_color',
               'n': 'normal', 'nrm': 'normal', 'normal': 'normal', 'normals': 'normal'}
PACKED = set('ormash')
FORMAT_COLOR = {'PF_DXT1', 'PF_DXT5', 'PF_BC7', 'PF_B8G8R8A8', 'PF_R8G8B8A8'}
FORMAT_NORMAL = {'PF_BC5'}
FORMAT_MASK = {'PF_DXT1', 'PF_DXT5', 'PF_BC7', 'PF_B8G8R8A8', 'PF_R8G8B8A8', 'PF_BC4', 'PF_G8'}
GROUPS = {'BLEND_Opaque': 'opaque', 'BLEND_Masked': 'masked', 'BLEND_Translucent': 'translucent'}


def texture_name_role(package):
    """Source 1: the texture's own name suffix -> 'base_color' | 'normal' | 'packed:XYZ' | None."""
    last = (package or '').rsplit('/', 1)[-1]
    tok = re.split(r'[_\-.]', last)[-1].lower() if last else ''
    if tok in SUFFIX_ROLE:
        return SUFFIX_ROLE[tok]
    if 3 <= len(tok) <= 4 and set(tok) <= PACKED and len(set(tok)) == len(tok):
        return 'packed:' + tok.upper()
    return None


def format_role(fmt, name_role):
    """Source 2: the texture's own pixel format must fit the role (BC5 = 2-channel normal; colour
    formats for colour; a packed mask must not be BC5). -> bool."""
    if name_role == 'normal':
        return fmt in FORMAT_NORMAL
    if name_role == 'base_color':
        return fmt in FORMAT_COLOR
    if name_role and name_role.startswith('packed:'):
        return fmt in FORMAT_MASK and fmt not in FORMAT_NORMAL
    return False


def placeholder_census(usage):
    """usage: [(mesh, lod, section row, is_placeholder, why)] for ONE material.
    -> {'sections', 'placeholders', 'real': [...], 'meshes'}"""
    real = [{'mesh': m, 'lod': l, 'section': r['section'], 'triangles': r['triangles'], 'why': w}
            for m, l, r, ph, w in usage if not ph]
    return {'sections': len(usage), 'placeholders': sum(1 for u in usage if u[3]), 'real': real,
            'meshes': sorted({u[0] for u in usage}),
            'evidence': sorted({u[4] for u in usage if u[3]})[:3]}


def decide(f):
    """f: facts of one mod Material:
      package, audit_class, audit_verdict, overrides_base,
      instance_children [mod MICs whose parent it is], other_importers {class: [packages]},
      meshes {mesh package: 'ok' | error string}, usage [(mesh, lod, row, placeholder, why)],
      parent_proof (invisible_route.decide_mod_parent result),
      textures [(package, pixel format or None)], reader_c {'readable','blend'}, readers_ab {'readable','blend_group'}
    -> verdict dict."""
    out = {'package': f['package'], 'ship': False}
    if f.get('audit_class') != 'Material' or f.get('audit_verdict') != 'NO_TARGET_SHADERS':
        out.update(result='not-this-route', why='not a mod Material without target shaders')
        return out
    if f.get('overrides_base'):
        out.update(result='refused', why='overrides a game package')
        return out
    meshes = f.get('meshes') or {}
    if not meshes:
        if f.get('instance_children'):
            out.update(result='parent-only', why='no mesh slot uses it directly; %d instance(s) use it as their '
                       'parent (handled by the instance routes)' % len(f['instance_children']))
        else:
            out.update(result='no-user', why='no mesh slot and no instance uses it')
        return out
    bad = {m: e for m, e in meshes.items() if e != 'ok'}
    if bad:
        out.update(result='refused', why='mesh not decodable (fail closed): ' +
                   '; '.join('%s: %s' % (m.rsplit('/', 1)[-1], e) for m, e in sorted(bad.items())[:3]))
        return out
    other = {k: v for k, v in (f.get('other_importers') or {}).items() if v}
    if other:
        out.update(result='refused', why='other run-time importers besides meshes: ' +
                   ', '.join('%s x%d' % (k, len(v)) for k, v in sorted(other.items())))
        return out
    cen = placeholder_census(f.get('usage') or [])
    out['census'] = cen
    if cen['sections'] == 0:
        out.update(result='refused', why='no section of any LOD uses its slots: not provably unused (it could be '
                                         'an overlay or a per-LOD override); left as is')
        return out
    if not cen['real']:
        out.update(result='placeholder', option='B',
                   why='every one of its %d section(s) in %d mesh(es) is a placeholder: %s' % (
                       cen['sections'], len(cen['meshes']), cen['evidence'][0]),
                   ship_why='not shipped: a placeholder cannot be seen; hiding it needs the mesh-slot / '
                            'base-package builder, which is not part of the shipping overlay')
        return out
    pp = f.get('parent_proof') or {}
    if pp.get('proven'):
        out.update(result='draws-nothing', option='B', why='invisible-route mod-parent proof: ' +
                   '; '.join((pp.get('sources') or {}).values()),
                   ship_why='not shipped: needs the base-package builder (not in the overlay)')
        return out
    # OPTION A
    out['option'] = 'A'
    tex = f.get('textures') or []
    if not tex:
        out.update(result='refused', why='Option A: drawn on %d real section(s) but references no texture; '
                   'roles cannot be proven (nothing to carry)' % len(cen['real']))
        return out
    roles, seen = {}, {}
    for pkg, fmt in tex:
        nr = texture_name_role(pkg)
        if nr is None:
            out.update(result='refused', why='Option A: texture %s has no name role (source 1)' % pkg)
            return out
        if not format_role(fmt, nr):
            out.update(result='refused', why='Option A: texture %s: format %s does not fit role %s (source 2)'
                       % (pkg, fmt, nr))
            return out
        if nr in seen:
            out.update(result='refused', why='Option A: two textures with role %s (%s, %s): ambiguous'
                       % (nr, seen[nr], pkg))
            return out
        seen[nr] = pkg
        roles[pkg] = nr
    rc, ab = f.get('reader_c') or {}, f.get('readers_ab') or {}
    g1 = GROUPS.get(rc.get('blend')) if rc.get('readable') else None
    g2 = ab.get('blend_group') if ab.get('readable') else None
    if g1 is None or g2 is None or g1 != g2:
        out.update(result='refused', why='Option A: blend not proven by two readers (own property block %s, '
                   'compiled PC shaders %s)' % (g1, g2))
        return out
    out.update(result='base-instance-candidate', roles=roles, blend_group=g1,
               why='Option A: %d texture role(s) from name + format; blend %s from two readers; parent must be '
                   'a %s game material (never crossed)' % (len(roles), g1, g1),
               ship_why='not shipped: the derived-instance builder for base Materials is not part of the '
                        'shipping overlay yet')
    return out
