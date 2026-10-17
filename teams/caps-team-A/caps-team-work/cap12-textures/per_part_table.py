#!/usr/bin/env python3
"""Per-part table (markdown) from a round-4 reparent plan JSON. Usage: per_part_table.py PLAN.json"""
import json
import sys

d = json.load(open(sys.argv[1]))
rows = [r for r in d['rows'] if r['mod_package'].rsplit('/', 1)[-1].startswith('MI_') and r.get('params')
        and r['class'] == 'MaterialInstanceConstant' and r['verdict'] == 'INHERITS_MISSING']
print('| Part | Chosen parent | Parent shading model | Leon model | Coverage | Kept | Renamed | Lost |')
print('|---|---|---|---|---|---|---|---|')
n = 0
for r in sorted(rows, key=lambda r: r['mod_package']):
    part = r['mod_package'].rsplit('/', 1)[-1]
    mm = r.get('mod_model') or {}
    lm = '+'.join(m[4:] for m in mm.get('models', [])) or 'unreadable'
    if r['action'].startswith('reparent'):
        pc = r['parent_class']
        kept = [k for k, v in r['param_map'].items() if v['how'] == 'exact']
        ren = [f'{k}->{v}' for k, v in r['param_renames'].items()]
        lost = sorted(r['lost_params'])
        print(f"| {part} | {r['new_parent'].rsplit('/', 1)[-1]} | {'+'.join(m[4:] for m in pc['models'])} | {lm} | "
              f"{r['coverage']} | {', '.join(kept) or '-'} | {', '.join(ren) or '-'} | {', '.join(lost) or '-'} |")
    else:
        print(f"| {part} | none (stays checker) | - | {lm} | {r['coverage']} | - | - | {', '.join(r['params'])} |")
    n += 1
print(f'\n{n} parts', file=sys.stderr)
sys.exit(0 if n else 1)
