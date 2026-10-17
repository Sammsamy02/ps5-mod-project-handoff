#!/usr/bin/env python3
"""Offline normalized-evidence verifier; does not invent/adapt runtime markers.
PASS means the supplied evidence contract satisfies ordering, not independent
attestation that coverage declarations are truthful. See README.md.
"""
import sys
sys.dont_write_bytecode=True
import importlib.util
import argparse
import hashlib
import json
import math
from pathlib import Path
import re


def verify(document):
    def result(verdict, reason):
        return {'label':'Mac-only evidence validation','verdict':verdict,'reason':reason,
                'attestation':'Input coverage/source declarations require human verification.'}
    inc=lambda reason:result('INCONCLUSIVE',reason)
    if not isinstance(document,dict) or document.get('schema')!=1:
        return inc('Expected schema 1 object')
    events=document.get('events')
    if not isinstance(events,list) or not events:return inc('No events')
    for e in events:
        if not isinstance(e,dict):return inc('Malformed event')
        if not isinstance(e.get('t'),(int,float)) or isinstance(e['t'],bool) or not math.isfinite(e['t']):return inc('Invalid event time')
        if not isinstance(e.get('source'),str) or not e['source'] or type(e.get('line')) is not int or e['line']<1:return inc('Missing source/line reference')
        for key in ('run_id','clock_id','build_sha256'):
            if not isinstance(e.get(key),str) or not e[key]:return inc('Missing '+key)
        if not re.fullmatch('[0-9a-f]{64}',e['build_sha256']):return inc('Invalid build identity')
    for key in ('run_id','clock_id','build_sha256'):
        if len({e[key] for e in events})!=1:return inc('Mixed '+key)
    selected={}
    for kind in ('coverage','module_start','observer_armed','loadmap_entry','begin_play'):
        found=[e for e in events if e.get('kind')==kind]
        if len(found)!=1:return inc('Expected exactly one '+kind)
        selected[kind]=found[0]
    c=selected['coverage']; first=selected['loadmap_entry']; end=selected['begin_play']['t']
    if c.get('from_process_start') is not True or c.get('lossless') is not True:return inc('No lossless coverage from process start')
    through=c.get('through')
    if not isinstance(through,(int,float)) or isinstance(through,bool) or not math.isfinite(through) or through<max(e['t'] for e in events):return inc('Coverage ends before observed events')
    if c['t']>min(e['t'] for e in events):return inc('Coverage starts after observed activity')
    if type(first.get('ordinal')) is not int or first['ordinal']!=1:return inc('First LoadMap entry not explicitly identified')
    times=[selected[k]['t'] for k in ('module_start','observer_armed','loadmap_entry','begin_play')]
    if len(set(times))!=len(times):return inc('Equal timestamps cannot prove strict ordering')
    if times!=sorted(times):return result('FAIL','Complete supplied coverage shows wrong milestone ordering')
    r=result('PASS','Supplied common-run/common-clock evidence places residency and arming before first LoadMap')
    r.update(run_id=first['run_id'],clock_id=first['clock_id'],build_sha256=first['build_sha256'])
    return r


def audit_raw(path):
    """Decode preserved UE4SS bytes, inventory search terms, refuse timing proof.
    Search-term hits include refusals/docs; never interpret them as delivered hooks.
    """
    root=Path(__file__).resolve().parents[3]
    decoder=root/'outputs/2026-10-08-l25-prep/harvest/decode_session_log.py'
    spec=importlib.util.spec_from_file_location('existing_log_decoder',decoder)
    dec=importlib.util.module_from_spec(spec);spec.loader.exec_module(dec)
    raw=Path(path).read_bytes();decoded,stats=dec.decode_bytes(raw)
    lines=decoded.decode('utf-8',errors='replace').splitlines()
    terms=('module_start_entered','[BUILD]','LoadMap','BeginPlay')
    return {'label':'Mac-only replay of preserved raw session log','verdict':'INCONCLUSIVE',
            'reason':'Raw-log search terms do not establish first-entry coverage, common clock, or delivered milestones; normalized cited evidence required.',
            'input_sha256':hashlib.sha256(raw).hexdigest(),
            'decoder_sha256':hashlib.sha256(decoder.read_bytes()).hexdigest(),
            'decoded_lines':len(lines),'decode_stats':stats,
            'search_term_line_numbers':{term:[i for i,line in enumerate(lines,1) if term in line] for term in terms}}

def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--input',type=Path);g.add_argument('--raw-log',type=Path);p.add_argument('--output',type=Path)
    a=p.parse_args()
    try:
        source=a.input or a.raw_log
        if a.raw_log:d=audit_raw(source)
        else:
            raw=source.read_bytes();d=verify(json.loads(raw));d['input_sha256']=hashlib.sha256(raw).hexdigest()
        output=json.dumps(d,indent=2)+'\n'
        if a.output:
            if Path(__file__).resolve().parent not in a.output.resolve().parents:raise ValueError('Output must stay inside cap04')
            if a.output.resolve()==source.resolve():raise ValueError('Output must not overwrite input')
            a.output.write_text(output)
        else:print(output,end='')
        return 0 if d['verdict']=='PASS' else 1
    except (OSError,ValueError,TypeError) as e:
        p.exit(2,'input error: '+str(e)+'\n')
if __name__=='__main__':raise SystemExit(main())
