#!/usr/bin/env python3
"""Part role (skin / hair / eye / cloth / hard) of a mod material instance and
of a base parent, used ONLY to rank parents that are already allowed and map
the same number of parameters (Mac-only, generic; an English part vocabulary,
no game, mod or asset names). Vocabulary origin: common English words for
character parts, written while working on the first test mod (so it includes
that mod's part words: face, mouth, lash, pistol, knife, watch, props); it
never decides whether a parent is allowed, only the order among allowed ones.

Two sources per side:
  model  the shading model, read with two sources (mod: reader C enum + bit
         field; base: asset registry + reader C, leave-one-out):
           Hair -> hair, Eye -> eye, Cloth in the set -> cloth,
           only skin-type models (SubsurfaceProfile, Subsurface,
           PreintegratedSkin) -> skin, ClearCoat only -> hard, anything else none
  names  word tokens of the instance / material name, its master's name and
         (mod side) the names of the textures it binds; camelCase and _ split.
role(model, names):
  both give the same role          -> (role, 2)
  one gives a role, other gives none -> (role, 1)
  they disagree or neither gives one -> (None, 0)   [disagreement is reported]
score(part, parent):  0 same role, both strength 2;  1 same role otherwise;
                      2 either side unknown;          3 known and different.
"""
import re

VOCAB = {
    'skin': {'skin', 'face', 'head', 'hand', 'hands', 'mouth', 'lip', 'lips', 'teeth', 'tongue', 'neck'},
    'hair': {'hair', 'beard', 'brow', 'brows', 'eyebrow', 'eyebrows', 'lash', 'lashes', 'eyelash',
             'eyelashes', 'fur', 'groom', 'moustache', 'mustache'},
    'eye': {'eye', 'eyes', 'eyeball', 'iris', 'cornea', 'sclera'},
    'cloth': {'cloth', 'clothes', 'clothing', 'shirt', 'jacket', 'coat', 'trousers', 'pants', 'jeans', 'dress',
              'shoes', 'boots', 'tights', 'vest', 'glove', 'gloves', 'outfit', 'fabric', 'leather', 'hoodie',
              'sweater', 'skirt', 'socks', 'scarf'},
    'hard': {'metal', 'metallic', 'pistol', 'gun', 'rifle', 'knife', 'blade', 'weapon', 'watch', 'watches',
             'chrome', 'steel', 'prop', 'props', 'plastic', 'glass'},
}
SKIN_MODELS = {'MSM_SubsurfaceProfile', 'MSM_Subsurface', 'MSM_PreintegratedSkin'}
SPLIT = re.compile(r'[A-Z]+(?![a-z])|[A-Z]?[a-z]+|[0-9]+')


def tokens(*names):
    out = set()
    for n in names:
        if not n:
            continue
        for part in re.split(r'[/_\-\s.+]+', n.rsplit('/', 1)[-1]):
            out.update(t.lower() for t in SPLIT.findall(part))
    return out


def role_from_names(*names):
    t = tokens(*names)
    hits = {r for r, words in VOCAB.items() if t & words}
    return next(iter(hits)) if len(hits) == 1 else None


def role_from_models(models):
    if not models:
        return None
    m = set(models)
    if 'MSM_Hair' in m:
        return 'hair'
    if 'MSM_Eye' in m:
        return 'eye'
    if 'MSM_Cloth' in m:
        return 'cloth'
    if m <= SKIN_MODELS:
        return 'skin'
    if m == {'MSM_ClearCoat'}:
        return 'hard'
    return None


def role(models, *names):
    a, b = role_from_models(models), role_from_names(*names)
    if a and b and a != b:
        return {'role': None, 'strength': 0, 'model': a, 'names': b, 'note': f'model says {a}, names say {b}'}
    r = a or b
    return {'role': r, 'strength': (2 if a and b else 1) if r else 0, 'model': a, 'names': b}


def score(part, parent):
    if not part['role'] or not parent['role']:
        return 2
    if part['role'] != parent['role']:
        return 3
    return 0 if part['strength'] == 2 and parent['strength'] == 2 else 1
