"""Finite source-derived three-task queue case. Saved windows only; no binary reads/execution."""
import sys
sys.dont_write_bytecode=True
import hashlib,json,re,struct
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3]
PINS={'outputs/2026-10-12-caps-prep-chatgpt/cap07/sorted-multi-entry-audit/receipt.json': 'cc6feaf30faacc5719d0a1fbff37f372fc1788a696f4798c1eb233589fc33dfb', 'outputs/2026-10-12-caps-prep-chatgpt/cap07/sorted-multi-entry-audit/window-1dc40.txt': 'ae1a14b68e54d40d647442022b14e61fd01ad5dd80cd4f33a9b243cda8fdc283', 'outputs/2026-10-12-caps-prep-chatgpt/cap07/sorted-multi-entry-audit/window-1e980.txt': 'b08e6908db9ebf6e71e29e58682e1183c656abb9b6efab2906df58b743656a04', 'outputs/2026-10-12-caps-prep-chatgpt/cap07/transfer-contract-audit/receipt.json': '50528825e3eb57e0c1cbba31335c975baa64160c92702f6704d6d41b92beef1d'}

def bound(path):
 p=R/path;assert p.stat().st_size<=8*1024*1024;b=p.read_bytes();assert hashlib.sha256(b).hexdigest()==PINS[path],path;return b
before={p:hashlib.sha256(bound(p)).hexdigest() for p in PINS}
saved=json.loads(bound('outputs/2026-10-12-caps-prep-chatgpt/cap07/sorted-multi-entry-audit/receipt.json'));provider=json.loads(bound('outputs/2026-10-12-caps-prep-chatgpt/cap07/transfer-contract-audit/receipt.json'))
ins={};windows=[]
for name in ['window-1dc40.txt','window-1e980.txt']:
 path='outputs/2026-10-12-caps-prep-chatgpt/cap07/sorted-multi-entry-audit/'+name;lines=bound(path).decode().splitlines();raw=bytearray();start=None;cursor=None
 for number,line in enumerate(lines,1):
  match=re.fullmatch(r'(0x[0-9a-f]+)\s+([0-9a-f]+)\s+(.*)',line);assert match,line
  v=int(match[1],16);b=bytes.fromhex(match[2]);start=v if start is None else start;cursor=v if cursor is None else cursor;assert v==cursor;cursor+=len(b);raw.extend(b);ins[v]={'address':hex(v),'bytes':b.hex(),'instruction':match[3],'source':path,'line':number}
 row=[w for w in saved['windows'] if w['file']==name];assert len(row)==1;row=row[0];assert start==int(row['start'],16) and len(raw)==row['length'] and hashlib.sha256(raw).hexdigest()==row['sha256'];windows.append(row)
critical=[]
def expect(v,hexbytes):
 assert ins[v]['bytes']==hexbytes,(hex(v),ins[v]);critical.append(ins[v]);return ins[v]
checks={0x1dd0f:'4180be0801000001',0x1dd1d:'410fb68424a4000000',0x1dd32:'8b44d608',0x1dd38:'0f8e39010000',0x1dd48:'4189c1',0x1dd4b:'4189c0',0x1dd4e:'4531ff',0x1dd60:'4183f901',0x1dd64:'4589c1',0x1dd67:'0f860d010000',0x1dd6d:'41d1e8',0x1dd73:'478d1438',0x1dd77:'4e8b1cd1',0x1dd85:'493b7368',0x1dd89:'0f92c3',0x1dd8c:'410f44dd',0x1dd92:'75cc',0x1de82:'413b450c',0x1de86:'0f8504feffff',0x1dc90:'8d5001',0x1dc96:'4429f8',0x1dc99:'41895508',0x1dc9d:'4863d0',0x1dca0:'488d34d9',0x1dca4:'488d7cd908',0x1dca9:'48c1e203',0x1dcad:'e8de414507',0x1dcb6:'4c8924d8',0x1dcc2:'498b4d18',0x1dccd:'480f44d0',0x1dcd1:'4c8922',0x1dcd4:'4d896518',0x1dcd8:'41c74520ffffffff',0x1e99c:'0f84b4000000',0x1ea5e:'7e6a',0x1eacc:'488b83f0000000',0x1eadf:'0f85a4000000',0x1eae5:'833da0d52b0900',0x1eaf4:'0f858f000000',0x1eafa:'418b7608',0x1eb06:'498b16',0x1eb09:'4189f1',0x1eb0c:'89f7',0x1eb0e:'4531c0',0x1eb20:'4183f901',0x1eb24:'4189f9',0x1eb27:'764a',0x1eb29:'d1ef',0x1eb2b:'468d1407',0x1eb33:'49394368',0x1eb37:'7417',0x1eb39:'410f92c3',0x1eb40:'74de',0x1eb54:'4c3b9bf8000000',0x1eb62:'74bc',0x1eb76:'7e11',0x1eb7f:'48394668',0x1eb83:'0f841cfeffff',0x1e9a8:'488b04f2',0x1e9ae:'0f8499000000',0x1ea4d:'45894620',0x1e9b4:'418b4e08',0x1e9b8:'41f7d0',0x1e9bb:'4101c8',0x1e9be:'741e',0x1e9c0:'488d3cf2',0x1e9c7:'4989c7',0x1e9ca:'48c1e203',0x1e9ce:'488d7708',0x1e9d2:'e8b9344507',0x1e9db:'4c89f8',0x1e9de:'ffc9',0x1e9e0:'41894e08',0x1e9fd:'0f84c9010000',0x1ea10:'4889ca',0x1ea13:'488b4950',0x1ea1f:'75ef',0x1ea2a:'488b4850',0x1ea2e:'48894a50',0x1ea32:'48c7405000000000',0x1ea3e:'0f85a5010000',0x1ea44:'49895618',0x1ebe9:'41c74620ffffffff',0x1ebf1:'488b4868',0x1ec00:'48898bf0000000'}
for v,b in checks.items():expect(v,b)
# Independently compute actual relative branch/call targets from retained bytes.
edges=[]
for v in [0x1dd38,0x1dd67,0x1dd92,0x1de86,0x1dcad,0x1e99c,0x1ea5e,0x1eb27,0x1eb37,0x1eb40,0x1eb62,0x1eb83,0x1e9ae,0x1e9be,0x1e9d2,0x1e9fd,0x1ea1f,0x1ea3e]:
 b=bytes.fromhex(ins[v]['bytes']);disp=struct.unpack('<i',b[-4:])[0] if len(b)>=5 else struct.unpack('<b',b[-1:])[0];target=v+len(b)+disp;edges.append({'site':hex(v),'target':hex(target),'bytes':b.hex()});assert hex(target) in ins[v]['instruction']
assert provider['capture']['sha256']=='6951ddd7df5daabfbc9ac495ea0eee73e7de121cddb30a1e979fad005a7c6b04' and provider['provider_name_versions_match']
path=[p for p in provider['count_paths'] if p['input_rdx']==16];assert len(path)==1 and path[0]['computed_target']=='0x429d'
callee=[w for w in provider['windows'] if w['vaddr']=='0x429d'];assert len(callee)==1 and callee[0]['bytes']=='c5fa6f06c5fa7f075dc3'
# Explicit finite unsigned-register traces for this case, not target instruction execution.
T,A,B=0x1000,0x2000,0x3000;resources={T:0x110,A:0x220,B:0x330};array=[A,B];r15=0;r8=r9=2;insert=[]
for expected_index in [1,0]:
 r8>>=1;mid=r8+r15;assert mid==expected_index;less=resources[T]<resources[array[mid]];assert less
 old_r9=r9;r9=r8;insert.append({'probe_index':mid,'task':'B' if mid==1 else 'A','resource_less':True,'branch_1dd92_taken':True,'cmp_old_r9':old_r9,'r9_after_mov':r9,'branch_1dd67_taken':old_r9<=1})
 if old_r9<=1:break
assert r15==0 and len(insert)==2
# The actual audited 16B path loads a snapshot before writing, preserving overlapping source.
buffer=bytearray(struct.pack('<QQQ',A,B,0));snapshot=bytes(buffer[:16]);buffer[8:24]=snapshot;buffer[:8]=struct.pack('<Q',T);assert list(struct.unpack('<QQQ',buffer))==[T,A,B]
links={A:B,B:T,T:0};head,tail=A,T;post_insert={'active_array':['T','A','B'],'count':3,'head':'A','tail':'T','links':{'A':'B','B':'T','T':None},'cached_index':-1}
# Actual affinity lower-bound search over count3: first A (>U), then T (=U, offset>=last).
array=[T,A,B];r8=0;edi=r9=3;select=[]
for expected_index in [1,0]:
 edi>>=1;mid=edi+r8;assert mid==expected_index;task=array[mid]
 if task==T:assert resources[task]==resources[T];offset_before=False;branch='equal resource; T.offset>=last-offset; 1eb62->1eb20'
 else:assert resources[task]>resources[T];branch='resource greater; 1eb40->1eb20'
 old_r9=r9;r9=edi;select.append({'probe_index':mid,'task':'A' if task==A else 'T','branch':branch,'cmp_old_r9':old_r9,'r9_after_mov':r9,'branch_1eb27_taken':old_r9<=1})
 if old_r9<=1:break
assert r8==0 and array[r8]==T
count=3;index=0;remaining=count-index-1;assert remaining*8==16
snapshot=bytes(buffer[8:24]);buffer[:16]=snapshot;assert list(struct.unpack('<QQ',buffer[:16]))==[A,B]
# Pop saved task in R15; list walk A->B->T identifies predecessor B, not head fallback.
walk=[];previous=head;current=links[head]
while current!=T:
 walk.append({'previous':hex(previous),'current':hex(current)});previous=current;current=links[current]
assert previous==B;links[previous]=links[T];links[T]=0;tail=previous;assert head==A and tail==B and links=={A:B,B:0,T:0}
after={p:hashlib.sha256(bound(p)).hexdigest() for p in PINS};assert before==after
out={'schema':1,'label':'Specific static three-task source-derived case; no runtime execution','new_binary_bytes_read':0,'saved_game_image_sha256':saved['image_sha256'],'sources':before,'sources_unchanged':True,'caller_windows':windows,'critical_instructions':critical,'computed_edges':edges,'captured_provider_path':{'capture':provider['capture'],'count_path':path[0],'implementation_window':callee[0],'actual_live_binding_proven':False},'predicates':['Existing active sorted array[A,B], count2, capacity>=3; priorities0; resourcesT<A<B unsigned and all nonnull; A.next=B,B.next=null,headA,tailB; T sole fresh pending node with next=null.','Queue mode1, no dirty/config/history mutation or concurrent/cancellation/lookahead alternatives; insertion avoids growth.','Selection last-resource=T.resource, last-priority0,last-offset<=T.offset; threshold<=0, configuration0, higher buckets empty; cached index invalid after insert.','Valid memory and captured provider count16 path actually bound; entry destination+count does not wrap/overlap protected frame; vector instruction support and required return/register conventions assumed.','No intervening mutation between peek and pop; selected T noncancelled; no runtime/quiescence or managed/native ABI assertion.'],'insertion_trace':insert,'insert_call_registers':{'rsi':'P','rdi':'P+8','rdx':16},'post_insert':post_insert,'selection_trace':select,'selection_source':'Sorted array index0 from affinity lower-bound; linked head remains A and is not selected.','pop_call_registers':{'rsi':'P+8','rdi':'P','rdx':16,'saved_selected_task':'R15=T'},'list_walk':walk,'post_pop':{'active_array':['A','B'],'count':2,'head':'A','tail':'B','links':{'A':'B','B':None,'T':None},'returned_task':'T','last_resource':'T.resource','cached_index':-1,'inactive_array_slot2':'Not cleared/claimed; outside active count.'},'quick_check':'Finite traces independently match unsigned resource ordering; actual 16B load-before-store snapshot preserves[A,B] through overlapping right then left transfer; source hashes unchanged. This is an arithmetic consistency check, not execution/emulation.','limits':'Specific conditional three-task branch only. No arbitrary scheduling/order/growth/concurrency, plaintext transfer API/ABI, live provider identity, target run or shipping values proven.'}
p=H/'receipt.json'
if p.exists():assert json.loads(p.read_text())==out,'replay drift'
p.write_text(json.dumps(out,indent=2)+'\n');print('PASS new three-task exact index0/16-byte insert+pop;2 insertion probes,2 affinity probes;',len(critical),'pinned instructions;',len(edges),'relative edges;0 binary bytes;source hashes unchanged')
