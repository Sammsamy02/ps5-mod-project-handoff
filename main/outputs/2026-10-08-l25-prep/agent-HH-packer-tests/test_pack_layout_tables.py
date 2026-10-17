#!/usr/bin/env python3
"""test_pack_layout_tables.py — [agent HH 2026-10-10] Mac-only test for
harvest/pack_layout_tables.py (Rule-10 provenance whitelist).

Fabricates an authoring table with game names in generated_by / laws /
nested entries, runs pack_table() directly via import, and proves:
names neutralized everywhere (values AND keys), non-name data byte-equal,
authoring original untouched, diff carries the replacements, outputs scan
clean, receipts + shippable cells written. Also checks the scanner itself
(case-insensitive detection) and the documented unknown-name limitation.

Writes PACK-LAYOUT-WHITELIST-TEST-RECEIPT.json beside this script.
"""
import importlib.util,json,sys
from pathlib import Path

HERE=Path(__file__).resolve().parent
TOOL_P=HERE.parents[0]/'harvest'/'pack_layout_tables.py'
FIX=HERE/'fixture-table'

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

TABLE={
 'schema':1,'table_id':'ue-layout-table-test-v1',
 'generated_by':'agent-X 2026-10-08 (per-version layout tables, Stellar Blade blind-test prep)',
 'laws':{'rule10':'keyed by ENGINE VERSION only; no game names',
         'note':'labels measured on the Townfall image; Townfall live base 0x400000'},
 'marker_classifier':{'positive_control':'Townfall: 54 present; C+Python agree',
                      'negative_control':'Astro Bot reconstructed ELF: 0/244 -> REFUSED'},
 'entries':[
   {'version':'5.6','status':'measured','confidence':1.0,
    'measured_on':'Townfall decrypted eboot, sha256 '+'a'*64+', 194241424 B',
    'keep_int':123,'keep_list':[1,2,3],
    'shipping_path':'PT_SCE PATH: .../Townfall-PS5-Shipping.self'},
   {'version':'4.27','status':'seed','confidence':0.0,
    'measured_on':'never measured; seed only'}],
 'version_axis_note':'30 versions',
 'alignment_with_gi':'aligned with GI audit 2026-10-10'}

EXPECTED=json.loads(json.dumps(TABLE))     # deep copy
EXPECTED['generated_by']=EXPECTED['generated_by'].replace('Stellar Blade','BLIND-TEST-TITLE')
EXPECTED['laws']['note']=EXPECTED['laws']['note'].replace('Townfall','MEASURED-TITLE').replace('Townfall','MEASURED-TITLE')
EXPECTED['marker_classifier']['positive_control']=EXPECTED['marker_classifier']['positive_control'].replace('Townfall','MEASURED-TITLE')
EXPECTED['marker_classifier']['negative_control']=EXPECTED['marker_classifier']['negative_control'].replace('Astro Bot','NEGATIVE-CONTROL-TITLE')
EXPECTED['entries'][0]['measured_on']=EXPECTED['entries'][0]['measured_on'].replace('Townfall','MEASURED-TITLE')
EXPECTED['entries'][0]['shipping_path']=EXPECTED['entries'][0]['shipping_path'].replace('Townfall-PS5-Shipping.self','MEASURED-TITLE-PS5-Shipping.self')

def main():
    results=[];ok_all=True
    def check(name,ok,detail=None):
        nonlocal ok_all
        ok_all=ok_all and bool(ok)
        results.append({'check':name,'ok':bool(ok),**({'detail':detail} if detail else {})})
        print(('PASS ' if ok else 'FAIL ')+name)
    tool=load('pack_layout_tables',TOOL_P)
    # scanner unit checks
    check('scan-detects-case-insensitive',tool.scan_hits('townfall ASTRO BOT stellar blade')==
          ['Townfall','townfall','Stellar Blade','stellar blade','Astro Bot','astro bot'],
          {'found':tool.scan_hits('townfall ASTRO BOT stellar blade')})
    check('scan-clean-on-clean-text',tool.scan_hits('MEASURED-TITLE and BLIND-TEST-TITLE tokens are fine')==[])
    # neutralize_str unit checks (longest-first: compound name goes whole)
    s,h=tool.neutralize_str('Silent Hill: Townfall and Townfall-PS5-Shipping.self')
    check('compound-names-neutralize-whole',s=='MEASURED-TITLE and MEASURED-TITLE-PS5-Shipping.self',{'hits':h})
    FIX.mkdir(parents=True,exist_ok=True)
    tbl=FIX/'per-version-table.json'
    tbl.write_text(json.dumps(TABLE,indent=2)+'\n')
    import hashlib
    orig_sha=hashlib.sha256(tbl.read_bytes()).hexdigest()
    receipt=tool.pack_table(tbl,FIX/'shippable')
    check('receipt-status-pass',receipt['status']=='PACK_LAYOUT_TABLES_PASS',{'status':receipt['status']})
    check('authoring-original-untouched',hashlib.sha256(tbl.read_bytes()).hexdigest()==orig_sha)
    ship=json.loads((FIX/'shippable'/'per-version-table.shippable.json').read_text())
    check('shippable-exactly-expected',ship==EXPECTED)
    check('blind-test-name-gone',b'Stellar Blade' not in (FIX/'shippable'/'per-version-table.shippable.json').read_bytes())
    check('all-known-names-gone-from-outputs',
          not tool.scan_hits((FIX/'shippable'/'per-version-table.shippable.json').read_text())
          and not tool.scan_hits((FIX/'shippable'/'per-version-table.shippable.cells.json').read_text()))
    check('non-name-data-preserved',ship['entries'][0]['keep_int']==123 and ship['entries'][0]['keep_list']==[1,2,3]
          and ship['version_axis_note']=='30 versions' and ship['schema']==1)
    check('measurement-sha-retained','a'*64 in ship['entries'][0]['measured_on'])
    cells=json.loads((FIX/'shippable'/'per-version-table.shippable.cells.json').read_text())
    check('cells-file-shape',cells['kind']=='per-version-layout-cells-shippable' and cells['cell_count']==2
          and len(cells['cells'])==2 and cells['names_neutralized'] is True)
    diff=(FIX/'shippable'/'per-version-table.provenance-diff.txt').read_text()
    check('diff-shows-before-after',all(m in diff for m in
          ('Stellar Blade','BLIND-TEST-TITLE','Townfall','MEASURED-TITLE',
           'Astro Bot','NEGATIVE-CONTROL-TITLE')))
    check('receipt-replacement-counts',receipt['replacements']['occurrences'].get('Townfall')==4
          and receipt['replacements']['occurrences'].get('Stellar Blade')==1
          and receipt['replacements']['occurrences'].get('Astro Bot')==1
          and receipt['replacements']['occurrences'].get('Townfall-PS5-Shipping.self')==1,
          {'occurrences':receipt['replacements']['occurrences']})
    check('receipt-fields-touched',receipt['replacements']['fields_touched']==6,
          {'fields_touched':receipt['replacements']['fields_touched']})
    check('receipt-gate-pass',receipt['gates'][0]['ok'] is True)
    check('retained-review-documented',len(receipt['retained_review'])>=2)
    # unknown-name limitation is documented behavior, not silent success
    unknown=dict(TABLE);unknown['generated_by']='agent-X (Unknown Game Adventures prep)'
    utbl=FIX/'unknown-table.json';utbl.write_text(json.dumps(unknown,indent=2)+'\n')
    ureceipt=tool.pack_table(utbl,FIX/'shippable-unknown')
    ucheck=(ureceipt['status']=='PACK_LAYOUT_TABLES_PASS'
            and 'Unknown Game Adventures' in json.loads((FIX/'shippable-unknown'/'unknown-table.shippable.json').read_text())['generated_by']
            and any('unknown-name limitation' in (r.get('item','')+r.get('why','')) for r in ureceipt['retained_review']))
    check('unknown-name-limitation-documented',ucheck)
    (HERE/'PACK-LAYOUT-WHITELIST-TEST-RECEIPT.json').write_text(json.dumps(
        {'schema':1,'tool':'test_pack_layout_tables.py','agent':'HH','date':'2026-10-10',
         'status':'PACK_LAYOUT_WHITELIST_TEST_PASS' if ok_all else 'PACK_LAYOUT_WHITELIST_TEST_FAIL',
         'checks':results,'fixture_dir':str(FIX),'console_contact':False,'native_execution':False},indent=2)+'\n')
    print(json.dumps({'status':'PASS' if ok_all else 'FAIL','checks':len(results),
                      'failed':[r['check'] for r in results if not r['ok']]}))
    return 0 if ok_all else 1

if __name__=='__main__':
    sys.exit(main())
