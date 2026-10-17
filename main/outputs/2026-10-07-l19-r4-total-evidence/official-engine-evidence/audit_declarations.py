#!/usr/bin/env python3
"""Audit complete source class boundaries and top-level data, not only projection."""
import hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];OUT=Path(__file__).resolve().parent
ENGINE=ROOT/'outputs/2026-10-07-l19-r4-official-engine-source';DEP=ENGINE/'dependencies'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def mask(raw):
    pattern=r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern,lambda m:re.sub(r'[^\n]',' ',m.group()),raw)
def audit(path,name):
    raw=path.read_text();clean=mask(raw)
    match=re.search(r'\b(?:class|struct) '+name+r'\s*\{',clean)
    assert match is not None
    opening=clean.index('{',match.start());depth=1;end=opening+1
    while depth:
        if clean[end]=='{':depth+=1
        elif clean[end]=='}':depth-=1
        end+=1
    body=clean[opening+1:end-1];depth=1;buffer='';statements=[]
    for c in body:
        if depth==1:buffer+=c
        if c=='{':depth+=1
        elif c=='}':
            depth-=1
            if depth==1:buffer=''
        elif c==';' and depth==1:
            statement=re.sub(r'\s+',' ',re.sub(r'^\s*#.*$', '',buffer,flags=re.M)).strip()
            statement=re.sub(r'^(?:(?:public|private|protected):\s*)+', '',statement)
            statements.append(statement);buffer=''
    data=[]
    for s in statements:
        if not s or s==';':continue
        if s.startswith(('using ','typedef ','friend ','static ','constexpr ')):continue
        if '(' in s and '(*ConstructFn)' not in s:continue
        data.append(s)
    return {'name':name,'source_path':str(path),'source_sha256':sha(path),'start_byte':len(raw[:match.start()].encode()),
            'end_byte_exclusive':len(raw[:end+1].encode()),'start_line':raw.count('\n',0,match.start())+1,'end_line':raw.count('\n',0,end)+1,
            'top_level_semicolon_statements':statements,'candidate_data_statements':data,
            'contains_virtual_keyword':bool(re.search(r'\bvirtual\b',body)),
            'contains_layout_attribute':bool(re.search(r'\balignas\b|__attribute__|__declspec',body)),
            'contains_pragma_pack':bool(re.search(r'#\s*pragma\s+pack',body)),
            'head_has_no_base_clause':':' not in clean[match.start():opening]}
rows=[audit(ENGINE/'Field.h','FFieldClass'),audit(DEP/'ThreadSafeCounter.h','FThreadSafeCounter'),audit(DEP/'NameTypes.h','FName'),audit(DEP/'NameTypes.h','FNameEntryId')]
expected={
 'FFieldClass':['FName Name;','uint64 Id;','uint64 CastFlags;','EClassFlags ClassFlags;','FFieldClass* SuperClass;','FField* DefaultObject;','FField* (*ConstructFn)(const FFieldVariant&, const FName&, EObjectFlags);','FThreadSafeCounter UnqiueNameIndexCounter;'],
 'FThreadSafeCounter':['volatile int32 Counter;'],
 'FName':['FNameEntryId ComparisonIndex;','uint32 Number;','FNameEntryId DisplayIndex;'],
 'FNameEntryId':['uint32 Value;']}
for row in rows:
    assert row['candidate_data_statements']==expected[row['name']],(row['name'],row['candidate_data_statements'])
    assert not row['contains_virtual_keyword'] and not row['contains_layout_attribute'] and not row['contains_pragma_pack'] and row['head_has_no_base_clause']
result={'scope':'Complete pinned source class audit. All actual source top-level data declarations retained in projection, conditional FName data retained. This does not prove the game source revision is byte-identical.',
        'classes':rows,'expected_data_statements':expected,
        'whole_Field_h_contains_no_pack_pragma':not bool(re.search(r'#\s*pragma\s+pack',mask((ENGINE/'Field.h').read_text()))),
        'full_platform_preset_header_environment_reproduced':False}
(OUT/'COMPLETE-DECLARATION-AUDIT.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'classes_checked':len(rows),'data_counts':{r['name']:len(r['candidate_data_statements']) for r in rows},'attributes_bases_virtuals_or_pack_found':False}))
