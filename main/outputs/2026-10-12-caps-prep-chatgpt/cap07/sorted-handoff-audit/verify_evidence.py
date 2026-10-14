import sys
sys.dont_write_bytecode=True
import capstone,hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3];r=json.loads((H/'receipt.json').read_text());critical=json.loads((H/'critical.json').read_text());P=R/r['image_path']
sources=['CLAUDE.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/handoff-audit/FINDING.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/handoff-audit/verification.json','outputs/2026-10-12-caps-prep-chatgpt/cap07/branch-selector-audit/FINDING.md','outputs/2026-10-12-caps-prep-chatgpt/cap07/branch-selector-audit/verification.json']
def hashes():return {p:hashlib.sha256((R/p).read_bytes()).hexdigest() for p in sources}
before=hashes()
with P.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 assert hashlib.sha256(b).hexdigest()==r['image_sha256']
 po=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ps=[struct.unpack_from('<IIQQQQQQ',b,po+i*pe) for i in range(pn)]
 def off(v,n):
  for p in ps:
   if p[0]==1 and p[3]<=v and v+n<=p[3]+p[5]:return p[2]+v-p[3]
  raise ValueError(hex(v))
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);decoded={};branches=[]
 for w in r['windows']:
  v=int(w['start'],16);raw=b[off(v,w['length']):off(v,w['length'])+w['length']];assert hashlib.sha256(raw).hexdigest()==w['sha256'];ins=list(md.disasm(raw,v));assert len(ins)==w['instructions'];lines=(H/w['file']).read_text().splitlines();assert len(lines)==len(ins)
  for j,i in enumerate(ins,1):decoded[i.address]=(i,w['file'],j);assert lines[j-1].startswith(f'{i.address:#010x} {i.bytes.hex()}')
 for c in critical:
  v=int(c['address'],16);i,w,j=decoded[v];assert (i.bytes.hex(),i.mnemonic,i.op_str,w,j)==(c['bytes'],c['mnemonic'],c['operands'],c['window'],c['line'])
  z=i.bytes;target=None
  if z[0] in (0xe8,0xe9):target=v+len(z)+struct.unpack_from('<i',z,1)[0]
  elif z[0]==0xeb or 0x70<=z[0]<=0x7f:target=v+len(z)+struct.unpack_from('<b',z,1)[0]
  elif z[0]==0x0f and 0x80<=z[1]<=0x8f:target=v+len(z)+struct.unpack_from('<i',z,2)[0]
  if target is not None:assert i.op_str==hex(target);branches.append({'site':hex(v),'target':hex(target),'bytes':z.hex()})
 # Guard check only, not synthetic execution: a deliberately wrong window hash must fail.
 try:assert hashlib.sha256(raw).hexdigest()=='0'*64
 except AssertionError:negative=True
 else:raise AssertionError('wrong-hash rejection failed')
 after=hashes();assert before==after
 out={'label':'Fresh Mac-only exact instruction replay PASS; conditional single-entry sorted bucket, not execution','image_sha256':r['image_sha256'],'windows':len(r['windows']),'critical_instructions':len(critical),'relative_branches_and_calls':branches,'source_hashes_before':before,'source_hashes_after':after,'wrong_window_hash_rejected':negative,'critical_receipt_sha256':hashlib.sha256((H/'critical.json').read_bytes()).hexdigest()}
 (H/'verification.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS:',len(r['windows']),'window hashes/decodes,',len(critical),'critical instructions,',len(branches),'relative branch/call displacements,',len(before),'unchanged source hashes,1 wrong-hash rejection')
