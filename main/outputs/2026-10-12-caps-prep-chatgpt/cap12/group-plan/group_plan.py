#!/usr/bin/env python3
"""Discovery-only shader group requirements from verified type8 metadata. No UCAS reader."""
import argparse,hashlib,importlib.util,json,os,re
from pathlib import Path
HOME=Path(__file__).resolve().parent
MANIFEST=HOME.parent/'manifest-v1/manifest_v1.py'
MANIFEST_SHA='6851610fc3ce27457af4271ac8f9a595868bab483288cdfc3541b3c8fc1a067e'
CAP=8*1024*1024

def need(ok,why):
 if not ok:raise ValueError(why)
def sha(b):return hashlib.sha256(b).hexdigest()
def stable_read(path,cap,pin=None):
 p=Path(path).resolve()
 def ident(s):return (s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns)
 before=p.stat();need(p.is_file() and 0<before.st_size<=cap,'input size bound')
 with p.open('rb') as f:
  need(ident(os.fstat(f.fileno()))==ident(before),'input changed before read');b=f.read(cap+1);ended=os.fstat(f.fileno())
 after=p.stat();need(len(b)==before.st_size<=cap and ident(before)==ident(ended)==ident(after),'input drift')
 digest=sha(b);need(pin is None or pin==digest,'input SHA mismatch')
 return b,{'path':str(p),'sha256':digest,'bytes':len(b),'identity':list(ident(after)),'read_cap':cap}
def check_sources(refs):
 for r in refs:
  _,current=stable_read(r['path'],r['read_cap'],r['sha256']);need(current==r,'input drift after planning')
def load_manifest():
 b,ref=stable_read(MANIFEST,128*1024,MANIFEST_SHA)
 spec=importlib.util.spec_from_file_location('group_plan_pinned_manifest',str(MANIFEST));mod=importlib.util.module_from_spec(spec)
 exec(compile(b,str(MANIFEST),'exec'),mod.__dict__)
 return mod,ref
def hexid(s,n,label):
 need(isinstance(s,str) and re.fullmatch('[0-9a-f]{'+str(n*2)+'}',s) is not None,label+' exact lowercase hex required');return s
def integer(v,label):need(type(v) is int and v>=0,label+' nonnegative integer required');return v

def plan(payload,receipt,chunk_id,catalogue,catalogue_sha,maps,groups,owners):
 need(isinstance(maps,list) and isinstance(groups,list) and 0<len(maps)+len(groups)<=16,'selection bound/nonempty')
 for s in maps:hexid(s,20,'map hash')
 for s in groups:hexid(s,12,'group ID');need(s.endswith('09'),'raw09 group required')
 need(isinstance(owners,dict) and len(owners)<=16,'owner selection bound')
 for cid,o in owners.items():
  hexid(cid,12,'owner group');need(cid.endswith('09'),'raw09 group required');need(isinstance(o,dict) and set(o)=={'toc_sha256','container_id','toc_row'},'exact owner selection required')
  hexid(o['toc_sha256'],32,'owner TOC SHA');hexid(o['container_id'],8,'owner container');integer(o['toc_row'],'owner row')
 hexid(catalogue_sha,32,'catalogue SHA')
 catbytes,catref=stable_read(catalogue,CAP,catalogue_sha);cat=json.loads(catbytes)
 need(cat.get('schema')==1 and isinstance(cat.get('inputs'),list) and len(cat['inputs'])<=64,'catalogue schema/input bound')
 mod,modref=load_manifest();verified=mod.verify_and_parse(payload,receipt,chunk_id);md=verified['metadata']
 selected=set()
 for h in maps:
  matches=[m for m in md['maps'] if m['stored_map_hash_20']==h];need(len(matches)==1,'unknown/ambiguous map selection')
  selected.update(md['shaders'][i]['group_index'] for i in matches[0]['shader_indices'])
 for cid in groups:
  matches=[g for g in md['groups'] if g['raw_chunk_id_12']==cid];need(len(matches)==1,'unknown/ambiguous group selection');selected.add(matches[0]['group_index'])
 need(0<len(selected)<=65536,'resolved group bound')
 need(set(owners).issubset({md['groups'][i]['raw_chunk_id_12'] for i in selected}),'owner selection outside selected groups')
 total=sum(md['groups'][i]['compressed_size'] for i in selected)
 need(total<=CAP and all(0<md['groups'][i]['compressed_size']<=4*1024*1024 for i in selected),'selected group size bound')
 # Index bounded catalogue once, preserving all exact owner alternatives.
 selected_ids={md['groups'][i]['raw_chunk_id_12'] for i in selected}
 by_id={};rowcount=0
 for inp in cat['inputs']:
  need(isinstance(inp,dict) and isinstance(inp.get('shader_related_rows'),list),'catalogue rows schema')
  rowcount+=len(inp['shader_related_rows']);need(rowcount<=65536,'catalogue row bound')
  for row in inp['shader_related_rows']:
   need(isinstance(row,dict),'catalogue row schema');cid=row.get('chunk_id')
   if cid not in selected_ids:continue
   need(inp.get('toc_version')==8 and row.get('chunk_type_raw')==9 and row.get('chunk_type')==9,'exact v8 raw09 owner required')
   tocsha=hexid(inp['source']['sha256'],32,'catalogue TOC SHA');container=hexid(inp['container_id_bytes_hex'],8,'catalogue container');index=integer(row['toc_row'],'catalogue row')
   by_id.setdefault(cid,[]).append(({'toc_sha256':tocsha,'container_id':container,'toc_row':index},inp,row))
 result=[]
 for i in sorted(selected):
  group=md['groups'][i];cid=hexid(group['raw_chunk_id_12'],12,'manifest group');need(cid.endswith('09'),'raw09 group required')
  candidates=by_id.get(cid,[])
  keys=[json.dumps(c[0],sort_keys=True) for c in candidates];need(len(set(keys))==len(keys),'duplicate owner row ambiguity')
  if cid in owners:
   candidates=[c for c in candidates if c[0]==owners[cid]];need(len(candidates)==1,'owner selection does not uniquely match')
  ordered=[s['shader_index'] for s in group['spans']]
  writer=group['unclaimed_prefix_bytes']==0 and ordered==group['shader_indices'] and all(s['bytes']>0 for s in group['spans'])
  item={'group_index':i,'raw_chunk_id_12':cid,'compressed_size':group['compressed_size'],'uncompressed_size':group['uncompressed_size'],'members':[md['shaders'][x] for x in group['shader_indices']],'spans':group['spans'],'writer_layout_candidate':writer,'writer_profile_admitted':False,'identity_status':'ID_DERIVATION_UNCHECKED','shader_code_hashes_verified':False,'group_codec_status':'CODEC_REQUIRED' if group['compressed'] else 'STORED_SIZE_PROFILE_ONLY','status':'INPUT_REQUIRED' if not candidates else 'OWNER_SELECTION_REQUIRED' if len(candidates)>1 else 'DISCOVERY_REQUIREMENT_ONLY','owner_candidates':[c[0] for c in candidates]}
  if len(candidates)==1:
   owner,inp,row=candidates[0];need(integer(row['logical_length'],'logical length')==group['compressed_size'],'owner logical length differs from manifest compressed size')
   integer(row['logical_offset'],'logical offset');hexid(row['metadata_content_hash_20_bytes'],20,'TOC IoHash')
   need(type(row.get('metadata_flags')) is int and row['metadata_flags'] in (0,2),'unsupported TOC metadata flags')
   blocks=row.get('blocks');need(isinstance(blocks,list) and 0<len(blocks)<=4096,'block count bound')
   coverage=0
   for b in blocks:
    for k in ['block_index','physical_block_offset','stored_bytes','decoded_bytes','compression_method_index','logical_slice_begin_in_decoded_block','logical_slice_end_in_decoded_block']:integer(b[k],k)
    start,end=b['logical_slice_begin_in_decoded_block'],b['logical_slice_end_in_decoded_block'];need(0<=start<end<=b['decoded_bytes'] and b['stored_bytes']>0,'invalid block span');coverage+=end-start
   need(coverage==row['logical_length'],'block coverage mismatch')
   item.update({'owner':owner,'toc_source':inp['source'],'logical_offset':row['logical_offset'],'logical_length':row['logical_length'],'expected_iohash_20':row['metadata_content_hash_20_bytes'],'metadata_flags':row['metadata_flags'],'physical_blocks':blocks,'transport_status':'STORED_TRANSPORT_DISCOVERY_ONLY' if all(b['compression_method_index']==0 and b['stored_bytes']==b['decoded_bytes'] for b in blocks) else 'TRANSPORT_CODEC_REQUIRED'})
  result.append(item)
 refs=[catref,modref]+verified['sources'];check_sources(refs)
 out={'schema':1,'label':'DISCOVERY_ONLY_GROUP_REQUIREMENTS','selected_maps':maps,'selected_raw_groups':groups,'selected_group_logical_bytes':total,'manifest':{'chunk_id':chunk_id,'payload_sha256':verified['payload_sha256'],'iohash_20':verified['iohash_20'],'toc_row':verified['toc_row'],'format_provenance':verified['format_provenance'],'reserved_header_relaxation':verified['reserved_header_relaxation'],'intake_provenance_limit':verified['intake_provenance_limit']},'groups':result,'sources':refs,'sources_unchanged':True,'group_payload_read':False,'compatible':False,'full_toc_group_admission':False,'limits':'Catalogue geometry is hash-bound discovery metadata only. Future full original TOC lookup/format checks and whole logical chunk BLAKE3/SHA required. Group ID derivation unchecked; no format string, group decompression, member code hashes, object+216 relation or compatibility proof.'}
 need(len(json.dumps(out).encode())<=CAP,'output bound');return out

def write_output(result,path):
 out=Path(path).resolve();need(out.is_relative_to(HOME) and not out.exists(),'new output inside group-plan required')
 need(out not in {Path(r['path']).resolve() for r in result['sources']},'input clobber refused')
 data=json.dumps(result,indent=2)+'\n';need(len(data.encode())<=CAP,'output bound')
 with out.open('x') as f:f.write(data)
def main():
 a=argparse.ArgumentParser(description=__doc__)
 for flag in ['payload','receipt','chunk-id','catalogue','catalogue-sha']:a.add_argument('--'+flag,required=True)
 a.add_argument('--map',action='append',default=[]);a.add_argument('--group',action='append',default=[]);a.add_argument('--owner',action='append',default=[],help='rawID:tocSHA:containerID:row');a.add_argument('--out',type=Path)
 args=a.parse_args();owners={}
 need(len(args.owner)<=16,'owner selection bound')
 for value in args.owner:
  parts=value.split(':');need(len(parts)==4 and parts[0] not in owners,'unique exact owner argument');owners[parts[0]]={'toc_sha256':parts[1],'container_id':parts[2],'toc_row':int(parts[3])}
 result=plan(args.payload,args.receipt,args.chunk_id,args.catalogue,args.catalogue_sha,args.map,args.group,owners)
 if args.out:write_output(result,args.out)
 print(json.dumps({'label':result['label'],'groups':len(result['groups']),'statuses':[g['status'] for g in result['groups']],'identity_status':'ID_DERIVATION_UNCHECKED'}))
if __name__=='__main__':main()
