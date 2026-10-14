#!/usr/bin/env python3
"""Read-only receipt verifier + native/target disagreement controls. No runtime engine tests."""
from pathlib import Path
import json,struct,hashlib,capstone,copy
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=Path(__file__).resolve().parent
E=json.loads((OUT/'NATIVE-EVIDENCE.json').read_text());C=json.loads((OUT/'TARGET-CENSUS.json').read_text());P=json.loads((OUT/'FRAME-PROFILE-CANDIDATE.json').read_text());D=json.loads((OUT/'TARGET-DEPENDENCIES.json').read_text())
for p in C['inputs']+[C['output'],C['command']]:
 q=Path(p['path']);bb=q.read_bytes();assert len(bb)==p['bytes'] and hashlib.sha256(bb).hexdigest()==p['sha256'],p['path']
bb=Path(C['output']['path']).read_bytes();assert bb[:5]==b'\x7fELF\x02' and struct.unpack_from('<H',bb,18)[0]==62
so=struct.unpack_from('<Q',bb,40)[0];se,sn=struct.unpack_from('<HH',bb,58);sh=[struct.unpack_from('<IIQQQQIIQQ',bb,so+i*se) for i in range(sn)];vals=None
def sz(d,o):return d[o:d.index(0,o)].decode()
for ss in sh:
 if ss[1]!=2:continue
 ns=sh[ss[6]];names=bb[ns[4]:ns[4]+ns[5]]
 for off in range(ss[4],ss[4]+ss[5],ss[9]):
  no,info,other,idx,value,size=struct.unpack_from('<IBBHQQ',bb,off)
  if sz(names,no)=='FrameFacts':
   assert size==len(C['facts'])*8;vals=struct.unpack_from('<'+'Q'*len(C['facts']),bb,sh[idx][4]+value)
assert vals and all(v==int(x['value'],16) for v,x in zip(vals,C['facts']))
for p in D['dependencies']:
 b=Path(p['path']).read_bytes();assert len(b)==p['bytes'] and hashlib.sha256(b).hexdigest()==p['sha256'],p['path']
b=Path(E['image']['path']).read_bytes();assert len(b)==E['image']['bytes'] and hashlib.sha256(b).hexdigest()==E['image']['sha256']
for k,x in E['instructions'].items():
 off=int(x['file_offset'],16);bb=bytes.fromhex(x['bytes']);assert b[off:off+len(bb)]==bb,k
for x in E['relocations']:
 off=int(x['file_offset'],16);assert b[off:off+24]==bytes.fromhex(x['bytes']);a,i,add=struct.unpack_from('<QQq',b,off);assert (hex(a),hex(i),hex(add))==(x['target'],x['info'],x['addend'])
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
def disp(a):
 x=E['instructions'][hex(a)];i=next(md.disasm(bytes.fromhex(x['bytes']),a));return next(op.mem.disp for op in i.operands if op.type==capstone.x86.X86_OP_MEM)
# Every map uses an independently decoded native operand; ProcessEvent stack member references subtract its exact framebase58.
M={'Node':(0x459a24,0x58),'Object':(0x459a28,0x58),'Code':(0x4577a4,0),'Locals':(0x4577da,0),'MostRecentProperty':(0x459dd5,0),'MostRecentPropertyAddress':(0x4577e5,0),'MostRecentPropertyContainer':(0x4577e9,0),'PreviousFrame':(0x457c71,0),'OutParms':(0x4577fe,0),'PropertyChainForCompiledIn':(0x459a6c,0x58),'CurrentNativeFunction':(0x326b1e,0),'PreviousTrackingFrame':(0x459d54,0),'bAbortingExecution':(0x459d73,0)}
def correlate(c,p):
 f={x['key']:int(x['value'],16) for x in c['facts']};pf={x['key']:x for x in p['facts']}
 for name,(a,base) in M.items():
  native=disp(a)-base;assert native==f['FFrame_51_AndAbove.'+name];assert native==int(pf['FFrame.'+name]['value'],16)
 assert f['FNativeFuncPtr.size']==8 and f['FNativeFuncPtr.is_member']==0 and f['UnrealScriptFunction.same_FNativeFuncPtr']==1
 assert f['UnrealScriptFunction.size']==8 and f['UFunction.GetFunc419.footprint']==8 and f['UObject.member_fn.size']==16
 x=E['instructions']['0x4577c3'];i=next(md.disasm(bytes.fromhex(x['bytes']),int(x['image_vaddr'],16)));mem=i.operands[0];assert mem.size==8 and mem.mem.scale==int(pf['GNatives.entry_width']['value'],16)
 assert pf['GNatives.table_image_vaddr']['value']==E['instructions']['0x4577ab']['rip_targets'][0]
 assert not any(x['live_validation'] for x in p['facts'])
correlate(C,P)
controls=[]
for key,bad,label in [('FFrame_51_AndAbove.Code','0x18','reject stale gap-l2a Code18'),('FFrame_51_AndAbove.CurrentNativeFunction','0x88','reject legacy Frame current88'),('FNativeFuncPtr.size','0x10','reject member-pointer width assumption')]:
 c=copy.deepcopy(C)
 next(x for x in c['facts'] if x['key']==key)['value']=bad
 try:correlate(c,P)
 except AssertionError:controls.append({'name':label,'expected':'REFUSE','actual':'REFUSE'})
 else:raise AssertionError(label)
p=copy.deepcopy(P);next(x for x in p['facts'] if x['key']=='GNatives.entry_width')['value']='0x10'
try:correlate(C,p)
except AssertionError:controls.append({'name':'reject GNatives stride16','expected':'REFUSE','actual':'REFUSE'})
else:raise AssertionError('stride')
print(json.dumps({'pass':True,'actual_compiler_dependency_pins':D['count'],'native_instruction_windows':len(E['instructions']),'native_relocation_records':len(E['relocations']),'matched_frame_members':len(M),'controls':controls,'native_runtime_tests':0}))
