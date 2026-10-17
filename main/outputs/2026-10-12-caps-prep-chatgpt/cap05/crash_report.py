#!/usr/bin/env python3
"""One-command local crash report using the existing read-only symbolizer.
Optional symbols are paired with a local PRX and scoped to --prx-module.
Capture/build binding is deliberately UNVERIFIED: build labels are assertions.
"""
import sys
sys.dont_write_bytecode=True
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
SYMBOLIZER=ROOT/'outputs/2026-10-08-l25-prep/phasea-tools/symbolize_crash.py'

def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()

def report(klog,build_id,prx=None,sidecar=None,prx_module=None):
    if not build_id or not build_id.strip():raise ValueError('build ID is required (caller assertion)')
    if (prx or sidecar) and not prx_module:raise ValueError('Symbol artifacts require explicit --prx-module ownership')
    if sidecar and not prx:raise ValueError('Sidecar requires local PRX for hash pairing')
    spec=importlib.util.spec_from_file_location('existing_symbolizer',SYMBOLIZER)
    sym=importlib.util.module_from_spec(spec);spec.loader.exec_module(sym)
    identities={'klog_sha256':digest(klog),'symbolizer_sha256':digest(SYMBOLIZER)}
    sources=[];pairing='NO_SYMBOL_ARTIFACTS'
    if prx:
        identities['prx_sha256']=digest(prx)
        if sidecar:
            try:syms,info=sym.load_prx_symbols_sidecar(sidecar,prx)
            except sym.PairingRefused as e:raise ValueError(str(e)) from e
            sources.append(('SIDECAR-EXACT',syms));identities['sidecar_sha256']=digest(sidecar);pairing='VERIFIED_LOCAL_PRX'
        else:
            data=Path(prx).read_bytes();sources.append(('DYNSYM-EXACT',sym.elf_dynsym(data,sym.elf_sections(data))));pairing='LOCAL_PRX_DYNSYM'
    blocks=sym.parse_klog_crashes(klog);crashes=[]
    for b in blocks:
        def resolve(addr):
            if addr is None:return None
            # Resolve owner first; never apply runtime symbol offsets to other modules.
            r=sym.symbolize_addr(addr,b['modules'],[])
            if sources and r.get('module') and Path(r['module']).name==prx_module:
                r=sym.symbolize_addr(addr,b['modules'],sources)
            r['confidence']='MODULE_ONLY' if r.get('module') else 'UNRESOLVED'
            for hit in r.get('symbols',[]):
                hit['usable_function_match']=hit.get('inside_symbol_bounds') is True
                if hit['usable_function_match']:r['confidence']='LOCAL_ARTIFACT_FUNCTION_MATCH_CAPTURE_PAIRING_UNVERIFIED'
            return r
        crashes.append({'signal':b['signal'],'rip':resolve(b['rip']),'fault_address':resolve(b['fault_address'])})
    return {'label':'Mac-only postmortem analysis','status':'CRASHES_FOUND' if blocks else 'NO_CRASH_BLOCKS',
            'block_count':len(blocks),'build_id_assertion':build_id,'capture_build_binding':'UNVERIFIED',
            'symbol_pairing':pairing,'prx_module_assertion':prx_module,'identities':identities,'crashes':crashes}

def render(d):
    lines=['# Offline crash report','',d['status']+'; blocks: '+str(d['block_count']),
           'Label: Mac-only. Capture/build binding: UNVERIFIED.',
           'Build label (caller assertion): '+d['build_id_assertion'],
           'Symbol pairing: '+d['symbol_pairing'],'','Input identities:']
    lines.extend('- '+k+': '+v for k,v in d['identities'].items())
    for i,c in enumerate(d['crashes'],1):
        lines+=['','Crash '+str(i)+'; signal '+str(c['signal'])]
        for field in ('rip','fault_address'):
            r=c[field]
            if r is None:lines.append('- '+field+': absent');continue
            lines.append('- '+field+': '+r['address']+'; '+str(r.get('module','unknown'))+' + '+str(r.get('module_offset','unknown'))+'; '+r['confidence'])
            if r.get('module_fingerprint'):lines.append('  fingerprint: '+r['module_fingerprint'])
            for hit in r.get('symbols',[]):lines.append('  '+hit['tier']+': '+str(hit.get('symbol'))+'; within bounds='+str(hit.get('inside_symbol_bounds')))
    # Shareable text must never retain console IPv4 addresses from inputs/labels.
    return re.sub(r'\b(?:\d{1,3}\.){3}\d{1,3}\b','[address redacted]','\n'.join(lines)+'\n')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--klog',required=True,type=Path);p.add_argument('--build-id',required=True)
    p.add_argument('--prx',type=Path);p.add_argument('--symbols-sidecar',type=Path);p.add_argument('--prx-module')
    p.add_argument('--output',type=Path)
    a=p.parse_args()
    try:
        if a.output:
            if HERE not in a.output.resolve().parents:raise ValueError('Output must stay inside cap05')
            if a.output.resolve() in [x.resolve() for x in (a.klog,a.prx,a.symbols_sidecar) if x]:raise ValueError('Output must not overwrite an input')
        d=report(a.klog,a.build_id,a.prx,a.symbols_sidecar,a.prx_module);text=render(d)
        if a.output:a.output.write_text(text)
        else:print(text,end='')
        return 0 if d['block_count'] else 1
    except (OSError,ValueError,KeyError) as e:p.exit(2,'input error: '+str(e)+'\n')
if __name__=='__main__':raise SystemExit(main())
