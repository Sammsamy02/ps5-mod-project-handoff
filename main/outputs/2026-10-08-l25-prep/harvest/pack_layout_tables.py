#!/usr/bin/env python3
"""pack_layout_tables.py — Rule-10 provenance whitelist for layout tables.

[agent HH 2026-10-10] GI's audit found the blind-test game name inside the
authoring table's generated_by; the table's own laws (blade-prep/per-version-
table.json laws.rule10) demand "No game names" — the shippable form must
actually satisfy that.

What this does (Mac-only, read-only on the authoring original):
  1. reads an authoring layout table (default: blade-prep/per-version-table.json);
  2. neutralizes every known GAME-NAME occurrence in strings — keys and values,
     at any depth — replacing each with a role token (MEASURED-TITLE / BLIND-
     TEST-TITLE / NEGATIVE-CONTROL-TITLE) so sentence grammar survives;
  3. writes the shippable artifacts next to the table (never over the input):
       <out>/per-version-table.shippable.json        full neutralized table
       <out>/per-version-table.shippable.cells.json  per-version cells only
       <out>/per-version-table.provenance-diff.txt   unified diff proof
       <out>/PACK-LAYOUT-TABLES-RECEIPT.json         receipt (below)
  4. gates itself: the OUTPUT files are re-scanned (case-insensitive) for every
     known name; any survivor => receipt status FAILED + exit 1 (fail-closed).

Receipt records: input pin (path/sha256/bytes), every replacement (JSON path,
token, count), output pins, scan gate result, and retained-item review notes
(measurement sha256s stay: they are measurement provenance, not names — the
ship reviewer decides; this tool never deletes data it was not asked to
neutralize).

Usage:
  python3 pack_layout_tables.py [--table PATH] [--out-dir DIR] [--print-diff]
"""
import argparse,difflib,json,sys
from pathlib import Path

HERE=Path(__file__).resolve().parent
DEFAULT_TABLE=HERE.parent/'blade-prep'/'per-version-table.json'

# Ordered longest-first so compound names neutralize before their substrings.
# Roles: MEASURED-TITLE = the locally measured UE5 image (codename family),
# BLIND-TEST-TITLE = the round-5 blind-test target, NEGATIVE-CONTROL-TITLE =
# the 0/244-markers negative control. Tokens carry role, never a name.
GAME_NAMES=(
    ('Silent Hill: Townfall','MEASURED-TITLE'),
    ('Silent Hill','MEASURED-TITLE'),
    ('Townfall-PS5-Shipping.self','MEASURED-TITLE-PS5-Shipping.self'),
    ('Townfall','MEASURED-TITLE'),
    ('townfall','MEASURED-TITLE'),
    ('Stellar Blade','BLIND-TEST-TITLE'),
    ('stellar blade','BLIND-TEST-TITLE'),
    ('Astro Bot','NEGATIVE-CONTROL-TITLE'),
    ('astro bot','NEGATIVE-CONTROL-TITLE'),
)
NEEDLES=tuple(n for n,_ in GAME_NAMES)


def neutralize_str(s):
    """Return (new_string, [(needle, token, count), ...]) — only actual hits."""
    hits=[]
    for needle,token in GAME_NAMES:
        c=s.count(needle)
        if c:
            s=s.replace(needle,token);hits.append((needle,token,c))
    return s,hits


def neutralize_doc(obj):
    """Deep-copy obj with every string (keys included) neutralized.
    Returns (new_obj, [{'path':json_path,'field':name|None,'hits':[...]}])."""
    log=[]
    def walk(o,path):
        if isinstance(o,dict):
            out={}
            for k,v in o.items():
                nk,kh=neutralize_str(k)
                if kh:log.append({'path':path,'field':k,'hits':kh})
                walk_v=walk(v,path+'.'+k if nk==k else path+'.%r'%nk)
                out[nk]=walk_v
            return out
        if isinstance(o,list):
            return [walk(v,path+'[%d]'%i) for i,v in enumerate(o)]
        if isinstance(o,str):
            ns,h=neutralize_str(o)
            if h:log.append({'path':path,'field':None,'hits':h})
            return ns
        return o
    return walk(obj,''),log


def scan_hits(text):
    """Case-insensitive scan of raw text for any known needle."""
    low=text.lower()
    return [n for n in NEEDLES if n.lower() in low]


def sha256_file(path):
    import hashlib
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for chunk in iter(lambda:f.read(1<<20),b''):
            h.update(chunk)
    return h.hexdigest()


def pack_table(table_path,out_dir,print_diff=False):
    table_path=Path(table_path);out_dir=Path(out_dir)
    raw=table_path.read_text()
    doc=json.loads(raw)
    new_doc,repl=neutralize_doc(doc)
    out_dir.mkdir(parents=True,exist_ok=True)
    stem=table_path.stem
    ship_p=out_dir/(stem+'.shippable.json')
    cells_p=out_dir/(stem+'.shippable.cells.json')
    diff_p=out_dir/(stem+'.provenance-diff.txt')
    receipt_p=out_dir/'PACK-LAYOUT-TABLES-RECEIPT.json'
    ship_p.write_text(json.dumps(new_doc,indent=2)+'\n')
    cells={'schema':1,'kind':'per-version-layout-cells-shippable',
           'table_id':new_doc.get('table_id'),
           'source_table':{'path':str(table_path),'sha256':sha256_file(table_path)},
           'names_neutralized':True,
           'provenance':'game names neutralized by harvest/pack_layout_tables.py (agent HH 2026-10-10); see PACK-LAYOUT-TABLES-RECEIPT.json',
           'cell_count':len(new_doc.get('entries',[])),
           'cells':new_doc.get('entries',[])}
    cells_p.write_text(json.dumps(cells,indent=2)+'\n')
    diff=list(difflib.unified_diff(
        json.dumps(doc,indent=2,sort_keys=True).splitlines(),
        json.dumps(new_doc,indent=2,sort_keys=True).splitlines(),
        fromfile='AUTHORING/'+table_path.name,tofile='SHIPPABLE/'+ship_p.name,
        lineterm='',n=1))
    diff_text='\n'.join(diff)+'\n'
    diff_p.write_text(diff_text)
    # fail-closed gate: re-read the WRITTEN outputs and scan them
    survivors={}
    for p in (ship_p,cells_p):
        found=scan_hits(p.read_text())
        if found:survivors[str(p)]=found
    gate_ok=not survivors
    counts={}
    for r in repl:
        for needle,token,c in r['hits']:
            counts[needle]=counts.get(needle,0)+c
    receipt={'schema':1,'tool':'harvest/pack_layout_tables.py',
             'agent':'HH','date':'2026-10-10',
             'status':'PACK_LAYOUT_TABLES_PASS' if gate_ok else 'PACK_LAYOUT_TABLES_FAILED_NAMES_SURVIVED',
             'input':{'path':str(table_path),'sha256':sha256_file(table_path),
                      'bytes':table_path.stat().st_size,'modified':False,
                      'note':'authoring original is read-only input; never rewritten'},
             'games_law':('Rule 10: shippable layout tables carry no game names. '
                          'Neutralized role tokens: MEASURED-TITLE (locally measured UE5 image), '
                          'BLIND-TEST-TITLE (round-5 blind-test target), NEGATIVE-CONTROL-TITLE '
                          '(0/244-markers negative control).'),
             'replacements':{'fields_touched':len(repl),'occurrences':counts,'detail':repl},
             'outputs':{'shippable':{'path':str(ship_p),'sha256':sha256_file(ship_p),'bytes':ship_p.stat().st_size},
                        'cells':{'path':str(cells_p),'sha256':sha256_file(cells_p),'bytes':cells_p.stat().st_size,
                                 'cell_count':cells['cell_count']},
                        'diff':{'path':str(diff_p),'sha256':sha256_file(diff_p),'lines':len(diff)}},
             'gates':[{'gate':'zero-known-game-names-in-outputs','ok':gate_ok,
                       'detail':{'scanned':[str(ship_p),str(cells_p)],'survivors':survivors,
                                 'needles':list(NEEDLES)}}],
             'retained_review':[
                 {'item':'measurement sha256 values in entries[].measured_on are RETAINED',
                  'why':'they are measurement-instance provenance (receipt-pinned), not game names; '
                        'this tool neutralizes names only — the ship reviewer decides policy on hashes'},
                 {'item':'unknown-name limitation','why':'the scan covers the known project game-name '
                        'list only; an unknown title in future tables needs a new NEEDLES entry'}],
             'console_contact':False,'native_execution':False}
    receipt_p.write_text(json.dumps(receipt,indent=2)+'\n')
    if print_diff:
        print(diff_text)
    return receipt


def main():
    ap=argparse.ArgumentParser(description='Rule-10 provenance whitelist for shippable layout tables')
    ap.add_argument('--table',default=str(DEFAULT_TABLE),help='authoring table JSON (read-only)')
    ap.add_argument('--out-dir',default=None,help='output dir (default: <table dir>/shippable)')
    ap.add_argument('--print-diff',action='store_true',help='print the provenance diff to stdout')
    args=ap.parse_args()
    out_dir=Path(args.out_dir) if args.out_dir else Path(args.table).parent/'shippable'
    receipt=pack_table(args.table,out_dir,args.print_diff)
    print(json.dumps({'status':receipt['status'],'input':receipt['input']['path'],
                      'outputs':{k:v['path'] for k,v in receipt['outputs'].items()},
                      'occurrences':receipt['replacements']['occurrences'],
                      'gate':receipt['gates'][0]['ok']}))
    return 0 if receipt['gates'][0]['ok'] else 1


if __name__=='__main__':
    sys.exit(main())
