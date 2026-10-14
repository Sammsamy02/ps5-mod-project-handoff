#!/usr/bin/env python3
"""Source-specified retoc v1 type8 metadata reader. No type9/UCAS reader.

MIT format provenance: ../manifest-format-audit/public-source-receipt.json
retoc revision d034ade1ae8117d4786eaf6b0418d4cf48474d7f.
"""
import argparse,hashlib,importlib.util,json,os
from pathlib import Path
import struct

PAYLOAD_CAP=4*1024*1024
OUTPUT_CAP=8*1024*1024
COUNT_CAP=65536
REFERENCE_CAP=131072
PARSER_SHA='a72636fbdec542bb6ce9521d6ca5709b61b09739938544671212ec93b0c958e6'
def need(ok,why):
 if not ok:raise ValueError(why)
def sha(b):return hashlib.sha256(b).hexdigest()
def parse(data):
 need(isinstance(data,bytes) and 4<=len(data)<=PAYLOAD_CAP,'manifest size bound')
 pos=0;total=0
 def scalar(fmt):
  nonlocal pos
  n=struct.calcsize('<'+fmt);need(pos+n<=len(data),'truncated scalar');v=struct.unpack_from('<'+fmt,data,pos);pos+=n;return v[0] if len(v)==1 else v
 need(scalar('I')==1,'unsupported manifest version')
 def vector(fmt):
  nonlocal pos,total
  count=scalar('I');size=struct.calcsize('<'+fmt);total+=count
  need(count<=COUNT_CAP and total<=REFERENCE_CAP,'vector count bound')
  need(count*size<=len(data)-pos,'truncated vector')
  return [scalar(fmt) for _ in range(count)]
 maps,hashes,ids,map_entries,codes,groups,indices=[vector(fmt) for fmt in ['20s','20s','12s','II','Q','IIII','I']]
 need(pos==len(data),'unsupported trailing bytes')
 need(len(maps)==len(map_entries)>0 and len(hashes)==len(codes)>0 and len(ids)==len(groups)>0,'parallel/empty vector count')
 need(len(set(maps))==len(maps) and len(set(hashes))==len(hashes) and len(set(ids))==len(ids),'ambiguous duplicate identities')
 need(all(i<len(codes) for i in indices),'shader index out of range')
 refs=0
 def selection(offset,count):
  nonlocal refs
  refs+=count;need(refs<=REFERENCE_CAP,'referenced index bound')
  need(count>0 and offset<=len(indices) and count<=len(indices)-offset,'index-array range')
  return indices[offset:offset+count]
 shaders=[{'shader_index':i,'stored_shader_hash_20':hashes[i].hex(),'packed_raw':f'{c:016x}','frequency_raw':c&15,'group_index':(c>>4)&((1<<30)-1),'offset_in_uncompressed_group':c>>34} for i,c in enumerate(codes)]
 for e in shaders:need(e['group_index']<len(groups),'packed group index out of range')
 mapout=[]
 for i,(offset,count) in enumerate(map_entries):
  selected=selection(offset,count);mapout.append({'map_index':i,'stored_map_hash_20':maps[i].hex(),'index_array_offset':offset,'shader_indices':selected,'shader_hashes':[hashes[x].hex() for x in selected]})
 groupout=[];claimed=set()
 for i,(offset,count,uncompressed,compressed) in enumerate(groups):
  selected=selection(offset,count)
  need(uncompressed>0 and compressed>0,'invalid group sizes')
  need(len(set(selected))==len(selected),'duplicate group membership')
  need(not claimed.intersection(selected),'shader belongs to multiple groups');claimed.update(selected)
  for x in selected:
   need(shaders[x]['group_index']==i,'group membership disagrees with packed entry')
   need(shaders[x]['offset_in_uncompressed_group']<uncompressed,'offset outside group size')
  ordered=sorted(selected,key=lambda x:shaders[x]['offset_in_uncompressed_group']);starts=[shaders[x]['offset_in_uncompressed_group'] for x in ordered]
  need(len(set(starts))==len(starts),'duplicate group offsets')
  spans=[{'shader_index':x,'offset':start,'end':end,'bytes':end-start} for x,start,end in zip(ordered,starts,starts[1:]+[uncompressed])]
  groupout.append({'group_index':i,'raw_chunk_id_12':ids[i].hex(),'index_array_offset':offset,'shader_indices':selected,'uncompressed_size':uncompressed,'compressed_size':compressed,'compressed':compressed!=uncompressed,'compression_codec':'unknown' if compressed!=uncompressed else 'stored','unclaimed_prefix_bytes':starts[0],'spans':spans})
 need(len(claimed)==len(codes),'shader missing from group membership')
 out={'label':'SOURCE_SPECIFIED_DECODER_ONLY','format_version':1,'maps':mapout,'shaders':shaders,'groups':groupout,'index_array_count':len(indices),'vector_elements_checked':total,'references_checked':refs,'shader_payload_hashes_verified':False,'type9_read':False,'object_216_relationship_proven':False,'target_format_proven':False,'compatible':False,'limits':'Stored hashes and group extents are manifest metadata. No group bytes, compression, code hash equality, object field mapping or instruction boundaries verified.'}
 need(len(json.dumps(out).encode())<=OUTPUT_CAP,'metadata output bound');return out

def read_bound(p,cap,pin=None,*,snapshots):
 p=Path(p).resolve()
 def identity(st):return (st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns)
 need(p.is_file(),'missing/over-cap input');before=p.stat();need(0<before.st_size<=cap,'missing/over-cap input')
 with p.open('rb') as f:
  opened=os.fstat(f.fileno());need(identity(opened)==identity(before),'input replaced before open')
  b=f.read(cap+1);ended=os.fstat(f.fileno())
 after=p.stat();need(len(b)<=cap and identity(before)==identity(ended)==identity(after) and len(b)==before.st_size,'input changed while read')
 digest=sha(b);need(pin is None or digest==pin,'input SHA mismatch')
 previous=snapshots.get(str(p));current={'path':str(p),'bytes':len(b),'sha256':digest,'read_cap':cap,'identity':list(identity(after))}
 need(previous is None or previous==current,'input changed after parsing');snapshots[str(p)]=current;return b

def check_sources(sources):
 snapshots={r['path']:r for r in sources}
 for ref in sources:read_bound(ref['path'],ref['read_cap'],ref['sha256'],snapshots=snapshots)

def verify_logical_chunk(payload_path,receipt_path,chunk_id,*,expected_type,expected_owner=None):
 snapshots={}
 def read(p,cap,pin=None):return read_bound(p,cap,pin,snapshots=snapshots)
 need(expected_type in (8,9),'unsupported logical chunk type')
 need(isinstance(chunk_id,str) and len(chunk_id)==24 and all(c in '0123456789abcdef' for c in chunk_id) and chunk_id[-2:]==f'{expected_type:02x}','exact raw chunk ID/type required')
 need(expected_type!=9 or expected_owner is not None,'explicit actual owner required for raw09')
 need(Path(payload_path).suffix=='.bin','explicit chosen .bin required')
 receipt=json.loads(read(receipt_path,2*1024*1024));need(receipt.get('schema')==1 and receipt.get('result')=='LOCAL_PAYLOAD_VERIFIED','unsupported intake receipt')
 rows=receipt.get('chunks');need(isinstance(rows,list) and 0<len(rows)<=16 and receipt.get('items_checked')==len(rows),'receipt count')
 need(len({r['chunk_id'] for r in rows})==len(rows),'duplicate receipt chunk IDs')
 chosen=[r for r in rows if r['chunk_id']==chunk_id];need(len(chosen)==1,'unique chosen receipt row required');r=chosen[0]
 need(type(r['chunk_type']) is int and r['chunk_type']==expected_type and r['iohash_verified'] is True,'verified expected-type receipt row required')
 parserref=receipt['sources']['parser'];need(parserref['sha256']==PARSER_SHA,'unsupported original overlay parser pin')
 parser_bytes=read(parserref['path'],512*1024,PARSER_SHA)
 spec=importlib.util.spec_from_file_location('manifest_v1_pinned_overlay',parserref['path']);mod=importlib.util.module_from_spec(spec)
 exec(compile(parser_bytes,mod.__file__,'exec'),mod.__dict__)
 tocref=receipt['sources']['toc'];toc=mod.parse_toc(read(tocref['path'],32*1024*1024,tocref['sha256']),strict_admission=False);mod.validate_lookup(toc)
 index=r['toc_row'];need(type(index) is int and 0<=index<len(toc.ids),'TOC row out of range')
 need(toc.ids[index].hex()==chunk_id and toc.ids[index][11]==expected_type,'TOC ID/type mismatch')
 actual_owner={'toc_sha256':tocref['sha256'],'container_id':toc.container_id.hex(),'toc_row':index}
 if expected_owner is not None:
  need(isinstance(expected_owner,dict) and set(expected_owner)==set(actual_owner),'exact owner triple required')
  need(type(expected_owner['toc_row']) is int and expected_owner==actual_owner,'actual TOC owner mismatch')
 offset,length=toc.offset_length(index);meta=toc.meta(index)
 need(len(meta)==24 and meta[20] in (0,2) and meta[:20].hex()==r['iohash_20'],'TOC/receipt IoHash mismatch')
 data=read(payload_path,PAYLOAD_CAP,r['sha256']);need(len(data)==r['bytes']==length,'TOC/receipt/payload length mismatch')
 need(mod.blake3_20(data)==meta[:20],'actual whole chunk BLAKE3 IoHash mismatch')
 check_sources(list(snapshots.values()))
 out={'schema':1,'label':'Mac-only; source-specified decoder only','chunk_id':chunk_id,'toc_row':index,'toc_logical_offset':offset,'payload_sha256':sha(data),'iohash_20':meta[:20].hex(),'iohash_verified':True,'sources':list(snapshots.values()),'sources_unchanged':True,'format_provenance':{'repository':'trumank/retoc','revision':'d034ade1ae8117d4786eaf6b0418d4cf48474d7f','license':'MIT','source_url':'https://github.com/trumank/retoc/blob/d034ade1ae8117d4786eaf6b0418d4cf48474d7f/retoc/src/shader_library.rs','source_sha256':'6bbdf0886b2a0aa4a5bc602a3392f014e362b0b63ea7d473918c792091c53b43'},'toc_owner':actual_owner,'logical_length':length,'chunk_type':expected_type,'source_scope':r.get('source_scope'),'source_physical_base':r.get('source_physical_base'),'ucas_provenance_declaration':receipt['sources'].get('ucas'),'intake_provenance_limit':receipt.get('limits'),'reserved_header_relaxation':'Existing parser strict_admission=False relaxes reserved header bytes; EOF/profile and full lookup validation remain enabled. UCAS paths in receipt are never opened.'}
 need(len(json.dumps(out).encode())<=OUTPUT_CAP,'metadata output bound');return data,out

def verify_and_parse(payload_path,receipt_path,chunk_id):
 data,out=verify_logical_chunk(payload_path,receipt_path,chunk_id,expected_type=8)
 out['metadata']=parse(data)
 check_sources(out['sources'])
 need(len(json.dumps(out).encode())<=OUTPUT_CAP,'metadata output bound');return out

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--payload',type=Path,required=True);ap.add_argument('--receipt',type=Path,required=True);ap.add_argument('--chunk-id',required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();home=Path(__file__).resolve().parent;out=a.out.resolve()
 need(out.is_relative_to(home) and not out.exists(),'new output file inside manifest-v1 required')
 result=verify_and_parse(a.payload,a.receipt,a.chunk_id);need(out not in {Path(r['path']).resolve() for r in result['sources']},'output would clobber input')
 encoded=json.dumps(result)+'\n';need(len(encoded.encode())<=OUTPUT_CAP,'metadata output bound')
 with out.open('x') as f:f.write(encoded)
 print(json.dumps({'label':result['label'],'maps':len(result['metadata']['maps']),'shaders':len(result['metadata']['shaders']),'groups':len(result['metadata']['groups']),'target_format_proven':False}))
if __name__=='__main__':main()
