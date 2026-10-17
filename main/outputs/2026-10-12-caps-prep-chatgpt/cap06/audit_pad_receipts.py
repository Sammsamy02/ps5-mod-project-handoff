#!/usr/bin/env python3
"""Read-only L3+R3/F7 supplier-diag adapter. No normalized/generated input events.
Reports provider emission chains; cannot prove menu action or deployed map identity.
"""
import sys
sys.dont_write_bytecode=True
import argparse
import hashlib
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
PROVIDER=ROOT/'outputs/2026-10-08-l22-runtime/candidate/PS5PadSource.cpp'
MAPPING=ROOT/'outputs/2026-10-08-l25-prep/keymap/GENERATED-BASELINE.ini'
CONTRACT=ROOT/'outputs/2026-10-08-l25-prep/keymap/MAPPING.md'
GENERATOR=ROOT/'outputs/2026-10-08-l25-prep/keymap/keymap-gen.py'
TAGS=('pad_l3r3_edge','pad_l3r3_edge_allow','pad_l3r3_edge_subscribed','pad_l3r3_edge_modifiers','pad_l3r3_f7_dispatched')
VALUES=(118,1,1,0,118)

def identity(path):
    raw=Path(path).read_bytes()
    return {'path':str(Path(path).resolve().relative_to(ROOT)) if ROOT in Path(path).resolve().parents else Path(path).name,
            'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)}

def audit(capture,provider=PROVIDER,mapping=MAPPING,consumer=None,consumer_lines=(),expected_action=None):
    provider_lines=Path(provider).read_text().splitlines()
    source_refs={tag:[i for i,line in enumerate(provider_lines,1) if 'diagnostic_event("'+tag+'"' in line] for tag in TAGS}
    map_lines=Path(mapping).read_text().splitlines()
    map_refs=[i for i,line in enumerate(map_lines,1) if line.strip().upper()=='L3+R3 = F7']
    rows=[];invalid=0;segments=0;prior=None
    # Keep raw physical line identity. Any invalid/non-event line breaks a chain.
    for line_number,line in enumerate(Path(capture).read_text(errors='replace').splitlines(),1):
        try:e=json.loads(line)
        except (ValueError,TypeError):invalid+=1;rows.append(None);prior=None;continue
        valid=isinstance(e,dict) and e.get('marker')=='modbridge.supplier-diag/v1' and e.get('schema')==1
        valid=valid and all(type(e.get(k)) is int and e[k]>=0 for k in ('seq','pid','t_ms','value'))
        if not valid:invalid+=1;rows.append(None);prior=None;continue
        if prior is None or e.get('tag')=='module_start_entered' or e['pid']!=prior['pid'] or e['seq']<=prior['seq'] or e['t_ms']<prior['t_ms']:segments+=1
        e=dict(e,line=line_number,segment=segments);rows.append(e);prior=e
    chains=[]
    if map_refs and all(source_refs.values()):
        for i in range(max(0,len(rows)-4)):
            group=rows[i:i+5]
            if any(e is None for e in group):continue
            first=group[0]
            if tuple(e.get('tag') for e in group)!=TAGS or tuple(e['value'] for e in group)!=VALUES:continue
            if any(e['pid']!=first['pid'] or e['segment']!=first['segment'] or e['seq']!=first['seq']+j for j,e in enumerate(group)):continue
            if [e['t_ms'] for e in group]!=sorted(e['t_ms'] for e in group):continue
            chains.append({'line_refs':[e['line'] for e in group],'pid':first['pid'],'capture_segment':first['segment'],
                           'seq_range':[first['seq'],group[-1]['seq']],'t_ms_range':[first['t_ms'],group[-1]['t_ms']],
                           'observed':'provider reports L3+R3 edge, allowed/subscribed/no modifiers, F7 enqueued',
                           'expected_action_observation':'UNPROVED'})
    sources={'capture':identity(capture),'provider':dict(identity(provider),emission_line_refs=source_refs),
             'generated_mapping':dict(identity(mapping),binding_line_refs=map_refs,deployed_identity='UNPROVED'),
             'mapping_contract':identity(CONTRACT),'generator':identity(GENERATOR)}
    if consumer:
        lines=Path(consumer).read_text().splitlines()
        if not consumer_lines or any(type(n) is not int or n<1 or n>len(lines) for n in consumer_lines):raise ValueError('Supply valid consumer source line references')
        sources['expected_action_source']=dict(identity(consumer),line_refs=list(consumer_lines),
            association='Caller-selected static source references only; not bound to capture build/run',
            expected_action_assertion=expected_action)
    return {'label':'Mac-only audit of historical raw console receipt','verdict':'INCONCLUSIVE',
            'reason':'Provider enqueue evidence does not identify consumer callback execution or menu action, and generated-map deployment/build pairing is absent.',
            'expected_action_observation':'UNPROVED','broader_mapping':'UNPROVED','provider_chain_count':len(chains),
            'provider_chains':chains,'invalid_or_non_event_lines':invalid,'sources':sources,
            'limits':['No button hold/release state trace: no no-repeat behavior verdict.',
                      'F7 may notify several consumers: queueing is not exclusive menu action.',
                      'Segment is inferred from capture resets/PID changes, not an independently established run ID.',
                      'Provider-source hash is local provenance, not proof this exact source built captured runtime.']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--capture',required=True,type=Path)
    p.add_argument('--provider-source',type=Path,default=PROVIDER);p.add_argument('--mapping',type=Path,default=MAPPING)
    p.add_argument('--expected-action');p.add_argument('--consumer-source',type=Path);p.add_argument('--consumer-lines',default='')
    p.add_argument('--output',type=Path)
    a=p.parse_args()
    try:
        refs=[int(x) for x in a.consumer_lines.split(',') if x]
        d=audit(a.capture,a.provider_source,a.mapping,a.consumer_source,refs,a.expected_action);text=json.dumps(d,indent=2)+'\n'
        if a.output:
            if HERE not in a.output.resolve().parents:raise ValueError('Output must stay inside cap06')
            if a.output.resolve() in [x.resolve() for x in (a.capture,a.provider_source,a.mapping,a.consumer_source,CONTRACT,GENERATOR) if x]:raise ValueError('Cannot overwrite an input')
            a.output.write_text(text)
        else:print(text,end='')
        return 1  # Contract intentionally cannot prove end-to-end expected action.
    except (OSError,ValueError,TypeError) as e:p.exit(2,'input error: '+str(e)+'\n')
if __name__=='__main__':raise SystemExit(main())
