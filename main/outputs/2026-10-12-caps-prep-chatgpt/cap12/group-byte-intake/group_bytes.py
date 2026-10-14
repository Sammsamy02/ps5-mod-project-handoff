#!/usr/bin/env python3
"""Verify a provided whole logical raw09 buffer; metadata only, no UCAS or codec reader."""
import argparse,hashlib,importlib.util,json,os
from pathlib import Path
HOME=Path(__file__).resolve().parent
MANIFEST=HOME.parent/'manifest-v1/manifest_v1.py'
MANIFEST_SHA='6851610fc3ce27457af4271ac8f9a595868bab483288cdfc3541b3c8fc1a067e'
CAP=8*1024*1024

def need(ok,why):
 if not ok:raise ValueError(why)
def load_manifest():
 # Bootstrap only: the verified module supplies all chunk/source admission logic.
 p=MANIFEST.resolve();cap=128*1024
 def ident(s):return (s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns)
 before=p.stat();need(p.is_file() and 0<before.st_size<=cap,'reader size bound')
 with p.open('rb') as f:
  need(ident(os.fstat(f.fileno()))==ident(before),'reader replaced before read');b=f.read(cap+1);ended=os.fstat(f.fileno())
 after=p.stat();need(len(b)==before.st_size<=cap and ident(before)==ident(ended)==ident(after),'reader changed')
 digest=hashlib.sha256(b).hexdigest();need(digest==MANIFEST_SHA,'reader SHA mismatch')
 ref={'path':str(p),'sha256':digest,'bytes':len(b),'identity':list(ident(after)),'read_cap':cap}
 spec=importlib.util.spec_from_file_location('group_bytes_pinned_manifest',str(p));m=importlib.util.module_from_spec(spec);exec(compile(b,str(p),'exec'),m.__dict__)
 return m,ref

def verify_group(manifest_payload,manifest_receipt,manifest_id,group_payload,group_receipt,group_id,exact_owner,*,selected_shader_indices=None):
 need(isinstance(group_id,str) and len(group_id)==24 and all(c in '0123456789abcdef' for c in group_id) and group_id.endswith('09'),'exact raw09 group ID required')
 need(isinstance(exact_owner,dict) and set(exact_owner)=={'toc_sha256','container_id','toc_row'},'explicit exact actual owner required')
 for field,width in [('toc_sha256',64),('container_id',16)]:
  v=exact_owner[field];need(isinstance(v,str) and len(v)==width and all(c in '0123456789abcdef' for c in v),'exact owner hex required')
 need(type(exact_owner['toc_row']) is int and exact_owner['toc_row']>=0,'exact owner row required')
 selections=[] if selected_shader_indices is None else selected_shader_indices
 need(isinstance(selections,list) and len(selections)<=16 and all(type(x) is int and x>=0 for x in selections) and len(set(selections))==len(selections),'member selection bound/duplicate/type')
 m,readerref=load_manifest();manifest=m.verify_and_parse(manifest_payload,manifest_receipt,manifest_id)
 matches=[g for g in manifest['metadata']['groups'] if g['raw_chunk_id_12']==group_id];need(len(matches)==1,'unique actual manifest group required');group=matches[0]
 need(set(selections).issubset(group['shader_indices']),'selected shader outside actual group')
 data,binding=m.verify_logical_chunk(group_payload,group_receipt,group_id,expected_type=9,expected_owner=exact_owner)
 need(len(data)==group['compressed_size'],'group buffer length differs from manifest compressed_size')
 need(binding['logical_length']+next(s['bytes'] for s in manifest['sources'] if Path(s['path'])==Path(manifest_payload).resolve())<=CAP,'combined payload bound')
 spans=group['spans'];need(bool(spans),'nonempty member spans required')
 views=[]
 if not group['compressed']:
  need(len(data)==group['uncompressed_size'],'stored group uncompressed length mismatch')
  for span in spans:
   start,end=span['offset'],span['end'];need(0<=start<end<=len(data),'invalid actual member extent')
   if not selections or span['shader_index'] in selections:
    views.append(dict(span,content_sha256=hashlib.sha256(memoryview(data)[start:end]).hexdigest(),fingerprint_kind='view content SHA256; not stored shader identity'))
 refs=[readerref]+manifest['sources']+binding['sources'];m.check_sources(refs)
 members=[manifest['metadata']['shaders'][i] for i in group['shader_indices']]
 out={'schema':1,'label':'WHOLE_LOGICAL_GROUP_BUFFER_VERIFIED_ONLY','whole_buffer_verified':True,'manifest':{k:manifest[k] for k in ['chunk_id','toc_owner','payload_sha256','iohash_20','format_provenance','reserved_header_relaxation','intake_provenance_limit']},'group':binding,'group_index':group['group_index'],'raw_chunk_id_12':group_id,'compressed_size':group['compressed_size'],'uncompressed_size':group['uncompressed_size'],'group_codec_status':'CODEC_REQUIRED' if group['compressed'] else 'STORED_SOURCE_PROFILE','unassigned_prefix_bytes':group['unclaimed_prefix_bytes'],'writer_order_candidate':group['unclaimed_prefix_bytes']==0 and group['shader_indices']==[s['shader_index'] for s in spans],'members':members,'selected_shader_indices':selections,'byte_views':views,'sources':refs,'sources_unchanged':True,'stored_shader_hash_verified':False,'identity_status':'ID_DERIVATION_UNCHECKED','target_format_proven':False,'compatible':False,'ucas_read':False,'payload_copies_written':False,'limits':'Whole logical buffer integrity is verified against pinned full TOC and receipt. Member ranges follow source-specified sorted offsets. Their SHA256 values fingerprint views only; no stored shader hash, group ID derivation, code/instruction/target or compatibility proof. Unequal-size group bytes remain opaque; no codec assumed.'}
 need(len(json.dumps(out).encode())<=CAP,'metadata output bound');return out

def write_output(result,path):
 p=Path(path).resolve();need(p.is_relative_to(HOME) and not p.exists(),'new output inside group-byte-intake required')
 need(p not in {Path(r['path']).resolve() for r in result['sources']},'input clobber refused')
 b=json.dumps(result,indent=2)+'\n';need(len(b.encode())<=CAP,'metadata output bound')
 with p.open('x') as f:f.write(b)
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ['manifest-payload','manifest-receipt','manifest-id','group-payload','group-receipt','group-id','owner-toc-sha','owner-container']:ap.add_argument('--'+name,required=True)
 ap.add_argument('--owner-row',type=int,required=True);ap.add_argument('--shader-index',type=int,action='append',default=[]);ap.add_argument('--out',type=Path)
 a=ap.parse_args();owner={'toc_sha256':a.owner_toc_sha,'container_id':a.owner_container,'toc_row':a.owner_row}
 result=verify_group(a.manifest_payload,a.manifest_receipt,a.manifest_id,a.group_payload,a.group_receipt,a.group_id,owner,selected_shader_indices=a.shader_index)
 if a.out:write_output(result,a.out)
 print(json.dumps({'label':result['label'],'group_codec_status':result['group_codec_status'],'byte_views':len(result['byte_views']),'identity_status':result['identity_status']}))
if __name__=='__main__':main()
