"""Independent offline verifier: parses ELF mappings and actual DT_RELA afresh."""
import hashlib,json,struct
from pathlib import Path
import capstone

O=Path(__file__).resolve().parent;R=O.parents[1]
P=O/'ENGINE-SLOT-FACTS-v2.json';F=json.loads(P.read_text())
B=(R/F['image']['path']).read_bytes()
def sha(b):return hashlib.sha256(b).hexdigest()
assert sha(B)==F['image']['sha256']=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
assert len(B)==F['image']['size_bytes']
assert B[:6]==b'\x7fELF\x02\x01'
phoff=struct.unpack_from('<Q',B,0x20)[0];phsize,phnum=struct.unpack_from('<HH',B,0x36)
PH=[struct.unpack_from('<IIQQQQQQ',B,phoff+i*phsize) for i in range(phnum)]
LOAD=[p for p in PH if p[0]==1]
def fileoff(v,n):
    a=[p for p in LOAD if p[3]<=v and v+n<=p[3]+p[5]]
    assert len(a)==1,(hex(v),n)
    return a[0][2]+v-a[0][3]
def memory(v,n):return B[fileoff(v,n):fileoff(v,n)+n]
CS=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);CS.detail=True
decoded={};direct_calls={};rip_refs={}
for key,row in F['instruction_evidence'].items():
    v=int(row['elf_image_vaddr'],16);raw=bytes.fromhex(row['instruction_bytes'])
    assert key==hex(v)
    assert fileoff(v,len(raw))==int(row['file_byte_offset'],16)
    assert memory(v,len(raw))==raw
    ins=list(CS.disasm(raw,v));assert len(ins)==1 and ins[0].size==len(raw),(key,row)
    i=ins[0];assert i.mnemonic+' '+i.op_str==row['instruction'],key
    decoded[v]=i
    rr=[i.address+i.size+x.mem.disp for x in i.operands if x.type==capstone.x86.X86_OP_MEM and x.mem.base==capstone.x86.X86_REG_RIP]
    if 'rip_targets' in row:assert [hex(x) for x in rr]==row['rip_targets']
    if rr:rip_refs[hex(v)]=[hex(x) for x in rr]
    if i.mnemonic=='call' and i.operands[0].type==capstone.x86.X86_OP_IMM:direct_calls[hex(v)]=hex(i.operands[0].imm)

for row in F['data_evidence']:
    v=int(row['elf_image_vaddr'],16);raw=bytes.fromhex(row['bytes'])
    assert fileoff(v,len(raw))==int(row['file_byte_offset'],16)
    assert memory(v,len(raw))==raw
    if row.get('encoding'):
        step=2 if row['encoding']=='UTF16LE' else 1
        assert raw[-step:]==b'\0'*step
        assert raw[:-step].decode('utf-16le' if step==2 else 'ascii')==row['text']
        assert all(raw[i:i+step]!=b'\0'*step for i in range(0,len(raw)-step,step))

dyn=next(p for p in PH if p[0]==2);dt={}
for o in range(dyn[2],dyn[2]+dyn[5],16):
    k,v=struct.unpack_from('<QQ',B,o)
    if not k:break
    dt[k]=v
assert dt[9]==24 and dt[8]%24==0
base=fileoff(dt[7],dt[8]);wanted={int(r['r_offset'],16):r for r in F['relocations']}
assert len(wanted)==len(F['relocations'])
found={v:[] for v in wanted}
for o in range(base,base+dt[8],24):
    target,info,add=struct.unpack_from('<QQq',B,o)
    if target in found:found[target].append((o,info,add))
for v,row in wanted.items():
    entries=found[v];assert len(entries)==1,(hex(v),entries)
    o,info,add=entries[0]
    assert info==8==int(row['r_info'],16)
    assert add==int(row['r_addend'],16)
    assert o==int(row['relocation_file_offset'],16)
    assert B[o:o+24].hex()==row['entry_bytes']
    fileoff(v,8)
def rel(v):return found[v][0][2]
def ins(v,expected):assert decoded[v].mnemonic+' '+decoded[v].op_str==expected,(hex(v),decoded[v].op_str,expected)
def call(v,target):assert direct_calls[hex(v)]==hex(target)
def rip(v,target):assert hex(target) in rip_refs[hex(v)]

# Explicit semantic assertions bind receipt prose to the decoded target instructions.
for ptr in (0x82315c0,0x8477708):assert rel(ptr+0x4f8)==0x384d460
assert rel(0x848a0b0+0x7b8)==0x2fdb2b0
assert rel(0x848a0b0+0x548)==0x2fdb350 and rel(0x848a0b0+0x6e8)==0x2fdc310
assert rel(0x82315c0+0x4e8)==0x384ae10 and rel(0x82315c0+0x4f0)==0x384ca20
rip(0x2fda7f7,0x783a91a);rip(0x2fda7e9,0x2fda880);ins(0x2fda82a,'push r15');call(0x2fda88a,0x2fdac20)
rip(0x2fdac85,0x848a0b0);ins(0x2fdac97,'mov qword ptr [rbx], rax')
call(0x2fc8fd4,0x2fda8a0);rip(0x2fda8f7,0x783a91a);rip(0x2fdaa97,0x76c22e0)
rip(0x2fd8f08,0x848a060);call(0x2fd8f0f,0x499610)
rip(0x3001a28,0x849ae90);call(0x3001a2f,0x499610)
ins(0x49967d,'call qword ptr [r13]');ins(0x499704,'bextr eax, dword ptr [r13 + 0x38], eax')
ins(0x499711,'add rcx, qword ptr [r13 + 0x28]')
ins(0x4944a6,'mov r15, qword ptr [rax - 8]');ins(0x4944aa,'movzx eax, byte ptr [r15 + 0x18]')
call(0x49641f,0x402040);call(0x495e57,0x402160);call(0x4020b8,0x418d60);call(0x4021af,0x418d60);call(0x418ded,0x3e1e20)
ins(0x3e1fa4,'movzx eax, word ptr [r13 + 0x32]');ins(0x3e1fa9,'mov dword ptr [rbx + 0x44], eax')
assert rel(0x848a060)==0x2fc8fd0 and rel(0x849ae90)==0x2fdaa40
property_counts={}
for owner,params in [('GameModeBase',0x848a060),('GameStateBase',0x849ae90)]:
    array=rel(params+0x28)
    count=(struct.unpack('<I',memory(params+0x38,4))[0]>>15)&0x7ff
    assert count>0;property_counts[owner]=count
    for p in [x for x in F['property_semantics'] if x['owner']==owner]:
        desc=int(p['descriptor'],16);entry=int(p['array_entry'],16)
        assert array<=entry<array+count*8 and (entry-array)%8==0
        assert rel(entry)==desc
        assert memory(desc+0x18,1)[0]==int(p['kind'],16)
        assert struct.unpack('<H',memory(desc+0x32,2))[0]==int(p['offset'],16)
        nv=rel(desc);text=bytearray();j=0
        while memory(nv+j,1)!=b'\0':text.extend(memory(nv+j,1));j+=1
        assert text.decode('ascii')==p['name']
for kind,target in [(0x11,0x494ad4),(0x12,0x494bc0)]:
    displacement=struct.unpack('<i',memory(0x7c3564c+(kind-1)*4,4))[0]
    assert 0x7c3564c+displacement==target
ins(0x2fdb624,'mov qword ptr [rbx + 0x308], r15')
ins(0x2fdb650,'mov qword ptr [r15 + 0x2b0], rbx')
ins(0x2fdb6a3,'mov rdi, rbx');ins(0x2fdb6a6,'mov rax, qword ptr [rbx]');ins(0x2fdb6a9,'call qword ptr [rax + 0x7b8]')
ins(0x2fdc324,'call qword ptr [rax + 0x7b8]')
ins(0x2fdb2e3,'mov qword ptr [r12 + 0x2a8], r14');ins(0x2fdb324,'mov qword ptr [r14 + 0x2b8], rdi')
ins(0x384ca3b,'mov r14, rsi');ins(0x384ca3e,'mov r13, rdi')
ins(0x384d2e1,'mov rax, qword ptr [r13]');ins(0x384d2e9,'mov rdi, r13');ins(0x384d2ec,'mov rsi, r14');ins(0x384d2ef,'mov rdx, r12')
ins(0x384d2f2,'call qword ptr [rax + 0x4f8]');ins(0x384b8b0,'call qword ptr [rax + 0x4f8]');ins(0x384bad4,'call qword ptr [rax + 0x4f8]')
rip(0x385c1e0,0x9553978)
for a in (0x384df8c,0x384dfa6,0x384e2ea,0x384ec55,0x384ec63,0x384ec71):rip(a,0x9553978)
for p,digest in F['sources'].items():assert sha((R/p).read_bytes())==digest,p
facts={f['key']:int(f['value'],16) for f in F['facts']}
assert facts=={'UEngine.LoadMap.vtable_byte_offset':0x4f8,'AGameModeBase.InitGameState.vtable_byte_offset':0x7b8}
assert all(f['width_bytes']==8 and f['live_validation'] is False for f in F['facts'])
out=dict(pass_=True,image_sha256=sha(B),fact_receipt_sha256=sha(P.read_bytes()),
 instruction_windows=len(decoded),data_windows=len(F['data_evidence']),unique_actual_DT_RELA_records=len(found),
 single_instruction_decodes=True,actual_PT_LOAD_mapping=True,source_pins=len(F['sources']),
 semantic_topology_assertions=True,native_property_array_counts=property_counts,
 direct_calls=direct_calls,rip_targets=rip_refs,findings=[])
out['pass']=out.pop('pass_')
q=O/'VALIDATION.json';assert not q.exists();q.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({k:out[k] for k in ['pass','instruction_windows','data_windows','unique_actual_DT_RELA_records','source_pins','native_property_array_counts','findings']}))
