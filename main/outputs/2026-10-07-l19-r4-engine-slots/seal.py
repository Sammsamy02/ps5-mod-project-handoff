"""Seal selected native semantic evidence, without modifying exploratory captures."""
import hashlib
import json
import struct
from pathlib import Path
import investigate as m

O=m.OUT; R=m.ROOT
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def put(name,x):
    p=O/name; assert not p.exists(),p
    p.write_text(json.dumps(x,indent=2)+'\n')
    return p

instructions={}
def take(name,lo,hi):
    rows=json.loads((O/name).read_text())
    for row in rows:
        a=int(row['elf_image_vaddr'],16)
        if lo<=a<hi:
            normalized={k:v for k,v in row.items() if k!='utf16_candidates'}
            if hex(a) in instructions: assert instructions[hex(a)]==normalized
            instructions[hex(a)]=normalized

# Every boundary below is an already decoded instruction boundary. Full exploratory
# captures may include adjacent functions; only selected ranges become evidence.
ranges=[
 ('0x2fda7a0-0xe0.json',0x2fda7a0,0x2fda86c),
 ('0x2fda880-0x30.json',0x2fda880,0x2fda892),
 ('0x2fdac20-0x340.json',0x2fdac20,0x2fdaca0),
 ('0x2fc8fd0-0x150.json',0x2fc8fd0,0x2fc8fdb),
 ('0x2fda8a0-0x100.json',0x2fda8a0,0x2fda95b),
 ('0x2fdaa40-0x140.json',0x2fdaa40,0x2fdaafb),
 ('0x2fd8ef0-0x40.json',0x2fd8ef0,0x2fd8f1d),
 ('0x3001a10-0x30.json',0x3001a10,0x3001a3d),
 ('0x2fdb2b0-0xa0.json',0x2fdb2b0,0x2fdb344),
 ('0x2fdb350-0x430.json',0x2fdb350,0x2fdb6b0),
 ('0x2fdc310-0x30.json',0x2fdc310,0x2fdc331),
 ('0x499610-0x800.json',0x499610,0x4997b8),
 ('0x494470-0xb00.json',0x494470,0x4944dc),
 ('0x494470-0xb00.json',0x494ad4,0x494b15),
 ('0x494470-0xb00.json',0x494bc0,0x494bfa),
 ('0x496380-0x180.json',0x496380,0x496438),
 ('0x495dc0-0x180.json',0x495dc0,0x495e72),
 ('0x402040-0x120.json',0x402040,0x4020c9),
 ('0x402160-0x110.json',0x402160,0x4021d8),
 ('0x418d60-0x280.json',0x418d60,0x418e00),
 ('0x3e1e20-0x210.json',0x3e1e20,0x3e1fbd),
 ('0x384d460-0x900.json',0x384d460,0x384d560),
 ('0x384dd60-0x2730.json',0x384df80,0x384dfbd),
 ('0x384dd60-0x2730.json',0x384e2df,0x384e313),
 ('0x384dd60-0x2730.json',0x384ec50,0x384ec7b),
 ('0x384dd60-0x2730.json',0x384f01b,0x384f070),
 ('0x385c160-0x260.json',0x385c1d9,0x385c1fc),
 ('0x384ca20-0xa40.json',0x384ca20,0x384ca52),
 ('0x384ca20-0xa40.json',0x384cb50,0x384cb86),
 ('0x384ca20-0xa40.json',0x384d2be,0x384d300),
 ('0x384ae10-0x1610.json',0x384ae10,0x384ae70),
 ('0x384ae10-0x1610.json',0x384b882,0x384b8bc),
 ('0x384ae10-0x1610.json',0x384bab0,0x384bae0),
 ('0x384ae10-0x1610.json',0x384c1d0,0x384c210),
]
for args in ranges: take(*args)
tick_path=R/'outputs/2026-10-07-l19-worker-engine-slots/ENGINE-TICK-FACTS.json'
assert sha(tick_path)=='132121833e16efadfffb9e34dd5d9bae44d325f7a0fd12f1eea0743aaa93ad3e'
tick=json.loads(tick_path.read_text())
for a,row in tick['instruction_evidence'].items():
    instructions[a]=row

data=[]
def raw(v,n,role,**extra):
    o=m.fmap(v,n)
    row=dict(elf_image_vaddr=hex(v),file_byte_offset=hex(o),bytes=m.B[o:o+n].hex(),role=role,**extra)
    data.append(row); return row
def string(v,encoding,expected=None):
    o=m.fmap(v); end=o; step=2 if encoding=='UTF16LE' else 1
    while m.B[end:end+step]!=b'\0'*step: end+=step
    b=m.B[o:end+step]; text=b[:-step].decode('utf-16le' if step==2 else 'ascii')
    if expected is not None: assert text==expected,(hex(v),text,expected)
    raw(v,len(b),'native named string',encoding=encoding,text=text)
    return text
for row in tick['data_evidence']:
    if row.get('text') in ('Engine','GameEngine'): string(int(row['elf_image_vaddr'],16),'UTF16LE',row['text'])
string(0x783a91a,'UTF16LE','GameModeBase')
string(0x76c22e0,'UTF16LE','GameStateBase')
string(0x793768c,'UTF16LE','Loading Map %s')
for a in (0x385c1d9,0x385c1e7,0x384cb69,0x384c1ee,0x384f03a):
    row=instructions[hex(a)]; string(int(row['rip_targets'][0],16),'UTF16LE')

relocations={}
def reloc(v,addend=None):
    rows=m.rela_row(v); assert len(rows)==1 and rows[0]['r_info']=='0x8',(hex(v),rows)
    row=rows[0]
    if addend is not None: assert int(row['r_addend'],16)==addend
    relocations[hex(v)]=row
    return int(row['r_addend'],16)
for ptr,slot,fn in [(0x82315c0,0x4f8,0x384d460),(0x8477708,0x4f8,0x384d460),
                    (0x848a0b0,0x7b8,0x2fdb2b0),(0x848a0b0,0x548,0x2fdb350),
                    (0x848a0b0,0x6e8,0x2fdc310),(0x82315c0,0x4e8,0x384ae10),
                    (0x82315c0,0x4f0,0x384ca20)]:reloc(ptr+slot,fn)
for v in (0x82315c0,0x8477708,0x848a0b0):reloc(v)
raw(0x848a060,0x40,'GameModeBase native class construction parameters')
raw(0x849ae90,0x40,'GameStateBase native class construction parameters')
reloc(0x848a060,0x2fc8fd0);reloc(0x848a088,0x8489de0)
reloc(0x849ae90,0x2fdaa40);reloc(0x849aeb8,0x849ad80)
properties=[
 ('GameModeBase','GameStateClass',0x8489960,0x2c0,0x8489df0,0x7bffb23,0x11),
 ('GameModeBase','GameState',0x8489be0,0x308,0x8489e38,0x7c0e04a,0x52),
 ('GameModeBase','SpectatorClass',0x8489ac8,0x2e8,0x8489e18,0x7b52b49,0x11),
 ('GameStateBase','GameModeClass',0x849ab10,0x2a8,0x849ad80,0x7b7e25d,0x11),
 ('GameStateBase','AuthorityGameMode',0x849ab58,0x2b0,0x849ad88,0x7c1c7c9,0x52),
 ('GameStateBase','SpectatorClass',0x849ab98,0x2b8,0x849ad90,0x7b52b49,0x11),
]
property_evidence=[]
for owner,name,desc,off,array_entry,nv,kind in properties:
    raw(desc,0x48,'native property descriptor',owner=owner,name=name)
    assert struct.unpack_from('<H',m.B,m.fmap(desc+0x32,2))[0]==off
    assert m.B[m.fmap(desc+0x18)]==kind
    reloc(desc,nv);reloc(array_entry,desc);string(nv,'ASCII',name)
    property_evidence.append(dict(owner=owner,name=name,descriptor=hex(desc),array_entry=hex(array_entry),offset=hex(off),kind=hex(kind)))
j=json.loads((O/'PROPERTY-KIND-DISPATCH.json').read_text())
raw(int(j['elf_image_vaddr'],16),len(bytes.fromhex(j['bytes'])),'native property kind jump table')
source=R/'outputs/2026-10-07-l19-local-profile-r3/units/UnrealInitializer.cpp'
callsite_source=dict(path=str(source.relative_to(R)),sha256=sha(source),lines=[2084,2087,2090,2219,2222,2225])

facts=[
 dict(key='UEngine.LoadMap.vtable_byte_offset',value='0x4f8',width_bytes=8,status='validated-static',confidence='high',live_validation=False,
      evidence_keys=['0x384d460','0x384d4b8','0x385c1d9','0x385c1e0','0x385c1e7','0x384df8c','0x384ec71','0x384cb69','0x384d2f2','0x384c1ee','0x384b8b0'],
      inference='Prior byte-backed named Engine and GameEngine constructors establish unadjusted root vptrs 82315c0 and 8477708. Unique actual DT_RELA entries at each root+4f8 select 384d460. Named native UEngine::TickWorldTravel and UEngine::Browse call this exact Engine slot. TickWorldTravel preserves Engine RDI->R13 and context RSI->R14, and invokes it with same Engine, context, URL, pending-game and error-string arguments. The callee preserves this argument shape, logs Loading Map %s from URL+28, and reads/resets the exact scalar registered as Engine.DelayTrimMemoryDuringMapLoadMode whose native documentation explicitly identifies LoadMap phases. Map package error paths independently corroborate. Method identity is an inference from named native callers, parameters, dedicated LoadMap configuration and body semantics; no Windows slot or current runtime vptr assumed.'),
 dict(key='AGameModeBase.InitGameState.vtable_byte_offset',value='0x7b8',width_bytes=8,status='validated-static',confidence='high',live_validation=False,
      evidence_keys=['0x2fda7f7','0x2fda7e9','0x2fda82a','0x2fda88a','0x2fdac85','0x2fdac97','0x2fdb6a9','0x2fdc324','0x2fdb2e3','0x2fdb324','0x3e1fa4','0x3e1fa9'],
      inference='Named native GameModeBase registration supplies wrapper2fda880, which invokes constructor2fdac20 on the unadjusted allocated object. That constructor installs root848a0b0; unique actual DT_RELA root+7b8 selects2fdb2b0. Native lifecycle caller2fdb350 obtains GameStateClass, creates/stores GameState at308, links its AuthorityGameMode2b0 to self, then invokes same-self slot7b8; separate lifecycle caller2fdc310 invokes the same slot. The selected callee assigns GameState.GameModeClass2a8 from self native Class10 and GameState.SpectatorClass2b8 from self.SpectatorClass2e8, then notifies the same state via virtuals. Actual named class/property descriptors, registration ownership, kind dispatcher and shared property initializer independently prove these names and offsets. This uniquely identifies the state initialization virtual by native lifecycle semantics. InitGameState is not a literal method-name string; this semantic inference is explicit, and notification method names are not guessed.'),
]
sources={str((O/name).relative_to(R)):sha(O/name) for name in sorted(set(x[0] for x in ranges))}
sources[str(tick_path.relative_to(R))]=sha(tick_path)
sources[str(source.relative_to(R))]=sha(source)
receipt=dict(format='townfall-native-engine-slot-facts-v1',scope='offline native evidence; no source/runtime/artifact changes',
 image=dict(path=str(m.IMAGE.relative_to(R)),sha256=m.IMAGE_SHA,size_bytes=len(m.B)),
 address_model='ELF virtual addresses; every window mapped through actual unique PT_LOAD; relocations from actual PT_DYNAMIC DT_RELA',
 facts=facts,unresolved=[],instruction_evidence=dict(sorted(instructions.items(),key=lambda x:int(x[0],16))),
 data_evidence=data,relocations=list(relocations.values()),property_semantics=property_evidence,
 native_class_parameters={'GameModeBase':'0x848a060','GameStateBase':'0x849ae90'},
 native_call_sites={'LoadMap':['0x384b8b0','0x384bad4','0x384d2f2'],'InitGameState':['0x2fdb6a9','0x2fdc324']},
 source_callsite_reference=callsite_source,sources=sources,
 limits=['No live execution or console observation.','No proof either slot is dormant on the original menu/spawn path; native world-travel and state-creation callsites are recorded, and required runtime checks remain intact.',
 'Method semantic identities are explicit inferences from native named lifecycle/field evidence, not symbol-table or Windows-source identities.',
 'Diagnostic tables do not establish vtable extents; only selected actual DT_RELA records support slot claims.',
 'Exploratory captures, malformed mid-instruction probes and broad pattern hits are preserved but excluded from selected facts.'])
receipt['revision']=2
receipt['supersedes_diagnostic_receipt']=dict(path='ENGINE-SLOT-FACTS.json',sha256=sha(O/'ENGINE-SLOT-FACTS.json'),reason='Replaced two mid-instruction exploratory getter captures with actual function starts; constructor passed via R15 stack argument, not RCX. Selected slot values unchanged.')
put('ENGINE-SLOT-FACTS-v2.json',receipt)
print(json.dumps({'facts':len(facts),'instruction_windows':len(instructions),'data_windows':len(data),'relocations':len(relocations),'sha256':sha(O/'ENGINE-SLOT-FACTS-v2.json')}))
