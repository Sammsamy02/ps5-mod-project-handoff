#!/usr/bin/env python3
"""Read-only UE5/v8 TOC shader catalogue; never opens UCAS or decodes code."""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path

TYPE_NAMES={8:'ShaderCodeLibrary',9:'ShaderCode'}

def shader_rows(toc):
    if not toc.ids or toc.block_size<=0:
        raise ValueError('zero chunks or invalid block size')
    result=[]
    for row,cid in enumerate(toc.ids):
        # Retoc get_chunk_type masks version bits; preserve raw byte as evidence.
        typ=cid[11]&0x3f
        if typ not in TYPE_NAMES: continue
        logical,length=toc.offset_length(row)
        if length<=0: raise ValueError('zero shader-related chunk length')
        first,last=logical//toc.block_size,(logical+length-1)//toc.block_size
        if not 0<=first<=last<toc.block_count:
            raise ValueError('shader chunk outside block descriptors')
        blocks=[]
        for i in range(first,last+1):
            physical,stored,decoded,method=toc.block(i)
            begin=logical%toc.block_size if i==first else 0
            end=(logical+length-1)%toc.block_size+1 if i==last else toc.block_size
            if not (0<=begin<end<=decoded<=toc.block_size) or stored<=0:
                raise ValueError('shader logical slice exceeds decoded block')
            if method!=0 or stored!=decoded:
                raise ValueError('unsupported compressed shader geometry')
            blocks.append({'block_index':i,'physical_block_offset':physical,
                           'stored_bytes':stored,'decoded_bytes':decoded,'compression_method_index':method,
                           'logical_slice_begin_in_decoded_block':begin,'logical_slice_end_in_decoded_block':end})
        meta=toc.meta(row)
        result.append({'toc_row':row,'chunk_id':cid.hex(),'chunk_type_raw':cid[11],
                       'chunk_type':typ,'type_name':TYPE_NAMES[typ],
                       'logical_offset':logical,'logical_length':length,
                       'metadata_content_hash_20_bytes':meta[:20].hex(),'metadata_flags':meta[20],
                       'blocks':blocks,'payload_observed':False})
    return result

def source(p):
    return {'path':str(p.resolve()),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--parser',required=True,type=Path)
    p.add_argument('--enum-source',required=True,type=Path)
    p.add_argument('--out',required=True,type=Path)
    p.add_argument('tocs',nargs='+',type=Path); a=p.parse_args()
    dest=a.out.resolve(); home=Path(__file__).resolve().parent
    if not dest.is_relative_to(home) or dest in {x.resolve() for x in a.tocs+[a.parser,a.enum_source]}:
        raise ValueError('output must remain in cap12 and not overwrite sources')
    enum=a.enum_source.read_text()
    if 'ShaderCodeLibrary = 8' not in enum or 'ShaderCode = 9' not in enum:
        raise ValueError('UE5 type enum evidence missing')
    spec=importlib.util.spec_from_file_location('retained_toc_parser',a.parser)
    mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    items=[]; total=Counter()
    for path in a.tocs:
        raw=path.read_bytes(); toc=mod.parse_toc(raw,strict_admission=False)
        if toc.partition_count!=1 or toc.partition_size!=mod.U64_MAX:
            raise ValueError('unsupported partition geometry')
        rows=shader_rows(toc); counts=Counter(cid[11]&0x3f for cid in toc.ids)
        total.update(r['type_name'] for r in rows)
        items.append({'source':source(path),'toc_version':toc.version,'container_id_bytes_hex':toc.container_id.hex(),
                      'chunk_count':len(toc.ids),'chunk_type_counts':dict(sorted(counts.items())),
                      'compression_block_size':toc.block_size,'partition_count':toc.partition_count,
                      'shader_related_rows':rows})
    if not items or sum(total.values())==0:
        raise ValueError('zero files or zero shader rows checked')
    result={'schema':1,'label':'Mac-only','parser_source':source(a.parser),'enum_source':source(a.enum_source),
            'files_checked':len(items),'shader_related_counts':dict(total),'inputs':items,
            'key_semantics':{'chunk_id':'Exact TOC lookup key; no library name inferred.',
                             'metadata_content_hash':'Opaque per-chunk metadata content hash; not labeled shader-map/resource identity.',
                             'global_cache_resource_hash_to_library':'UNRESOLVED; requires manifest semantics not present in TOCs.',
                             'shader_map_hash_to_code_chunk':'UNRESOLVED; do not equate prefixes.'},
            'limits':'Logical chunk offsets and physical compression-block descriptor offsets are distinct. UCAS payload availability, physical EOF, payload hash, library names, shader boundaries and instruction contents were not validated. No retrieval plan or code offsets generated.'}
    a.out.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'files_checked':len(items),'shader_related_counts':dict(total)}))

if __name__=='__main__': main()
