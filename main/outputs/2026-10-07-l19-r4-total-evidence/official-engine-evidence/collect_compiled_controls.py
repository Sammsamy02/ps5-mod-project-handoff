#!/usr/bin/env python3
"""Parse saved PS5 object bytes for literal actual-source compile controls."""
import hashlib,json,struct
from pathlib import Path
OUT=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def symbols(path):
    data=path.read_bytes();assert data[:6]==b'\x7fELF\x02\x01'
    assert struct.unpack_from('<H',data,18)[0]==62
    shoff=struct.unpack_from('<Q',data,40)[0];entry,num=struct.unpack_from('<HH',data,58)
    sections=[struct.unpack_from('<IIQQQQIIQQ',data,shoff+i*entry) for i in range(num)]
    found={}
    for s in sections:
        if s[1]!=2:continue
        names=sections[s[6]];strings=data[names[4]:names[4]+names[5]]
        for f in range(s[4],s[4]+s[5],s[9]):
            n,info,other,index,value,size=struct.unpack_from('<IBBHQQ',data,f)
            name=strings[n:strings.index(0,n)].decode()
            if index==0 or index>=len(sections):continue
            section=sections[index];offset=section[4]+value-section[3]
            found[name]={'name':name,'file_byte_offset':offset,'size':size,'bytes':data[offset:offset+size].hex(),'section_index':index,'symbol_info':info}
    return found
extract=json.loads((OUT/'EXTRACTION.json').read_text())
commands=json.loads((OUT/'COMPILE-COMMANDS-FINAL.json').read_text())
rows=[]
for cmd in commands:
    assert cmd['exit']==0 and not cmd['executed'] and not cmd['linked']
    obj=OUT/(cmd['name']+'.o');syms=symbols(obj)
    array=next(v for k,v in syms.items() if k.endswith('OfficialLayoutFacts'))
    assert array['size']==len(extract['array_labels'])*4
    values=dict(zip(extract['array_labels'],struct.unpack('<'+str(len(extract['array_labels']))+'I',bytes.fromhex(array['bytes']))))
    body=syms['OfficialFieldSizeBody']
    rows.append({'name':cmd['name'],'object_path':str(obj),'object_sha256':sha(obj),'machine':'EM_X86_64','values':values,
                 'array_symbol':array,'direct_size_query_body_symbol':body,'argv':cmd['argv'],'LLVM_CONFIG':cmd['LLVM_CONFIG'],
                 'compile_source_sha256':extract['output_cpp_sha256']})
result={'scope':'Saved x86_64 PS5 compiler object data from literal extracted licensed source declarations. Controls prove projection layout under specified flags, not independently the exact game source/config identity.',
        'extraction_path':str(OUT/'EXTRACTION.json'),'extraction_sha256':sha(OUT/'EXTRACTION.json'),
        'source_path':str(OUT/'literal_layout_control.cpp'),'source_sha256':sha(OUT/'literal_layout_control.cpp'),
        'commands_path':str(OUT/'COMPILE-COMMANDS-FINAL.json'),'commands_sha256':sha(OUT/'COMPILE-COMMANDS-FINAL.json'),
        'controls':rows}
(OUT/'COMPILED-CONTROLS.json').write_text(json.dumps(result,indent=2)+'\n')
for r in rows:
    v=r['values'];print(json.dumps({'name':r['name'],'FField':v['sizeof_FField'],'FFieldClass':v['sizeof_FFieldClass'],'FName':v['sizeof_FName'],'FField_Flags':v['FField_FlagsPrivate'],'FFieldClass_Id':v['FFieldClass_Id']}))
