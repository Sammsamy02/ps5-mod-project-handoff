#!/usr/bin/env python3
"""Resolve raw mod ScriptImport class refs against captured script metadata.

Uses existing retained bounded decoder, not guessed class hashes/names.
The one-chunk non-indexed global TOC has its own explicit format profile;
no generic overlay admission rule is relaxed.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys

def need(ok,reason):
    if not ok:raise ValueError(reason)

def source(path,cap):
    need(path.is_file() and path.stat().st_size<=cap,'missing/over-cap local source')
    b=path.read_bytes()
    return {'path':str(path.resolve()),'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b)},b

def resolve_classes(identities,table):
    rows=[];seen=set();record_indexes={index:i for i,index in enumerate(table.objects)}
    for pkg in identities['packages']:
        for export in pkg['decoded']['exports']:
            raw=int(export['class_ref']['raw'],16);key=(pkg['package_id'],export['name'])
            need(key not in seen,'duplicate subject export');seen.add(key)
            r={'package_id':pkg['package_id'],'export_name':export['name'],'class_raw':f'{raw:016x}',
               'class_metadata_identity_resolved':False,'compatible':False,'native_shader_proven':False}
            if export['class_ref']['kind']!='ScriptImport' or raw>>62!=1:r['reason']='class_ref_not_script_import'
            elif raw not in table.objects:r['reason']='script_index_absent_from_captured_table'
            else:
                obj=table.objects[raw];path=table.object_path(raw)
                # Older retained decoder treats all type3 values as a path end.
                # Require exact canonical null and complete bounded chain here.
                current=raw;chain=[]
                while current!=(1<<64)-1:
                    need(current in table.objects,'outer missing or noncanonical null')
                    need(current not in chain and len(chain)<128,'cyclic/over-deep outer chain');chain.append(current)
                    current=table.objects[current].outer_index
                r.update(class_metadata_identity_resolved=True,class_object_name=obj.name,class_object_outer_chain_path=path,
                         object_record_index=record_indexes[raw],object_record_offset=table.entries_offset+32*record_indexes[raw],
                         outer_raw=f'{obj.outer_index:016x}',cdo_class_raw=f'{obj.cdo_class_index:016x}',
                         outer_chain_raw=[f'{x:016x}' for x in chain])
            rows.append(r)
    need(0<len(rows)<=64,'zero/over-cap subjects')
    return rows

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);sys.modules[name]=m;spec.loader.exec_module(m);return m

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for key in ('identity','profile','capture-receipt','manifest','toc','ucas','parser','decoder','decoder-readme','out'):
        ap.add_argument('--'+key,required=True,type=Path)
    a=ap.parse_args();dest=a.out.resolve();home=Path(__file__).resolve().parent
    inputs=[getattr(a,k) for k in ('identity','profile','capture_receipt','manifest','toc','ucas','parser','decoder','decoder_readme')]
    need(dest.is_relative_to(home) and dest not in {p.resolve() for p in inputs},'output must stay in script-objects and preserve inputs')
    pins={};data={}
    for key in ('identity','profile','capture_receipt','manifest','toc','ucas','parser','decoder','decoder_readme'):
        pins[key],data[key]=source(getattr(a,key),8*1024*1024)
    profile=json.loads(data['profile']);cap=json.loads(data['capture_receipt']);ident=json.loads(data['identity'])
    need(pins['ucas']['sha256']==profile['global_ucas_sha256']==cap['sha256'] and pins['ucas']['bytes']==cap['bytes'],'ScriptObjects capture source mismatch')
    need(pins['toc']['sha256']==profile['global_utoc_sha256'],'global TOC pin mismatch')
    need(profile['global_ucas']==str(a.ucas) and profile['global_utoc']==str(a.toc),'profile source path mismatch')
    need(ident['label']=='Mac-only' and ident['items_checked']>0,'invalid/empty source identities')
    need(pins['parser']['sha256']==ident['sources']['parser']['sha256'],'identity/ScriptObjects parser pin differs')
    # Preserve identity/capture association as local observation, not installation proof.
    parser=load('script_capture_toc_parser',a.parser);decoder=load('retained_script_capture_decoder',a.decoder)
    b=data['toc'];info=parser.parse_toc_header(b)
    need((info['version'],info['header_size'],info['toc_entry_count'],info['block_entry_size'],info['compression_method_name_count'],info['compression_method_name_length'],info['partition_count'],info['partition_size'],info['container_flags'],info['directory_index_size'],info['perfect_hash_seeds_count'],info['chunks_without_perfect_hash_count'])==(8,144,1,12,0,32,1,parser.U64_MAX,0,0,1,0),'unsupported non-indexed ScriptObjects TOC profile')
    need(info['key_guid']==bytes(16) and not any(b[17:20]+b[81:84]+b[100:144]),'unsupported encryption/reserved header bytes')
    nblocks=info['block_entry_count'];need(0<nblocks<=4096 and len(b)==144+12+10+4+nblocks*12+24,'TOC geometry/EOF mismatch')
    cid=b[144:156];need(cid==bytes.fromhex('000000000000000000000005'),'not exact ScriptObjects chunk id')
    toc=parser.Toc(header=b[:144],version=8,ids=(cid,),offset_lengths=b[156:166],seeds=struct.unpack_from('<i',b,166),overflow=(),block_data=b[170:170+nblocks*12],directory_index=b'',metas=b[-24:],container_id=info['container_id'],block_size=info['compression_block_size'],partition_count=1,partition_size=parser.U64_MAX,container_flags=0,key_guid=info['key_guid'])
    parser.validate_lookup(toc);parser.validate_single_partition(toc,len(data['ucas']),permit_tail_padding=True)
    logical,length=toc.offset_length(0);need([logical,logical+length]==profile['scriptobjects_decoded_range'],'declared source chunk/profile range mismatch')
    payload=parser.extract_chunk_bytes(toc,0,data['ucas']);need(parser.blake3_20(payload)==toc.meta(0)[:20],'ScriptObjects IoHash mismatch')
    need(toc.meta(0)[20] in (0,2) and toc.meta(0)[21:]==bytes(3),'unsupported ScriptObjects metadata flags')
    table=decoder.read_script_objects(payload);need(len(table.names)>0 and len(table.objects)>0,'zero decoded global items')
    rows=resolve_classes(ident,table)
    output={'schema':1,'label':'Mac-only','result':'CAPTURED_SCRIPT_CLASS_METADATA','sources':pins,
            'scriptobjects':{'chunk_id':cid.hex(),'logical_offset':logical,'logical_length':length,'block_count':nblocks,'iohash_20':toc.meta(0)[:20].hex(),'iohash_verified':True,'decoded_sha256':hashlib.sha256(payload).hexdigest(),'names_checked':len(table.names),'objects_checked':len(table.objects),'trailing_source_bytes':len(data['ucas'])-length},
            'classes':rows,'items_checked':len(rows),'resolved_count':sum(r['class_metadata_identity_resolved'] for r in rows),'compatible':False,'native_shader_proven':False,
            'limits':['Names resolved only by exact raw ScriptImport index and complete outer chain in captured global metadata.','Retained decoder cites retoc d034ade1ae8117d4786eaf6b0418d4cf48474d7f; script_objects.rs is not present locally, so that original-source attribution was not independently reverified in this run.','Global TOC/capture association is hash-pinned by saved profile and capture receipt; manifest is context only, not proof its complete installed files match today.','Dedicated non-indexed ScriptObjects v8 profile, exact reserved zeroes, lookup, physical bounds and IoHash checked; generic overlay admission unchanged.','No base package export payload comparison, runtime class availability, visual compatibility or native shader proof. No payload/source copied.']}
    for key in pins:
        after,_=source(getattr(a,key),8*1024*1024)
        need(after==pins[key],'input changed during replay: '+key)
    output['input_files_unchanged']=True
    a.out.write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps({'items_checked':len(rows),'resolved_count':output['resolved_count'],'names_checked':len(table.names),'objects_checked':len(table.objects),'classes':[r.get('class_object_outer_chain_path') for r in rows]}))

if __name__=='__main__':main()
