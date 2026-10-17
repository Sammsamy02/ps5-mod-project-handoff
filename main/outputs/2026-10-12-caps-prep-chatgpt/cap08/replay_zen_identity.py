#!/usr/bin/env python3
"""Read-only replay of source-pinned selected mod Zen metadata; writes JSON only."""
import argparse
import hashlib
import importlib.util
import json
import mmap
from pathlib import Path
import struct
from zen_identity import decode_package, need

def source(path):
    need(path.is_file(),'source unavailable')
    h=hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return {'path':str(path.resolve()),'bytes':path.stat().st_size,'sha256':h.hexdigest()}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('closure','parser','toc','ucas','retoc-source','cue4-source','out'):
        p.add_argument('--'+key,required=True,type=Path)
    a=p.parse_args(); home=Path(__file__).resolve().parent; dest=a.out.resolve()
    inputs=[a.closure,a.parser,a.toc,a.ucas]
    need(dest.is_relative_to(home) and dest not in {x.resolve() for x in inputs},'output must remain in cap08 and preserve inputs')
    need(a.ucas.stat().st_size<=512*1024*1024 and a.toc.stat().st_size<=64*1024*1024,'source exceeds replay cap')
    closure=json.loads(a.closure.read_text()); pins={k:source(getattr(a,k)) for k in ('closure','parser','toc','ucas')}
    for key,pin in [('parser','parser_sha256'),('toc','source_mod_utoc_sha256'),('ucas','source_mod_ucas_sha256')]:
        need(pins[key]['sha256']==closure[pin],'source pin mismatch: '+key)
    subjects=closure['closure_packages'];need(0<len(subjects)<=8 and len(set(subjects))==len(subjects),'invalid/zero/duplicate subject package ids')
    spec=importlib.util.spec_from_file_location('pinned_toc_reader',a.parser); parser=importlib.util.module_from_spec(spec);spec.loader.exec_module(parser)
    toc=parser.parse_toc(a.toc.read_bytes(),strict_admission=False);parser.validate_lookup(toc)
    parser.validate_single_partition(toc,a.ucas.stat().st_size,permit_tail_padding=True)
    # Hash-pinned public format sources; no source contents are copied to outputs.
    public={}
    for name,path in {'zen_summary_export':a.retoc_source/'retoc/src/zen.rs','name_batch':a.retoc_source/'retoc/src/name_map.rs','container_version':a.retoc_source/'retoc/src/container_header.rs','object_index':a.cue4_source/'CUE4Parse/UE4/IO/Objects/FPackageObjectIndex.cs','export_map':a.cue4_source/'CUE4Parse/UE4/IO/Objects/FExportMapEntry.cs','retoc_license':a.retoc_source/'LICENSE','cue4parse_license':a.cue4_source/'LICENSE'}.items():
        public[name]=source(path)
    rows={cid.hex():i for i,cid in enumerate(toc.ids)}; results=[];total=0
    with a.ucas.open('rb') as f, mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as mapped:
        def chunk(row):
            nonlocal total
            _,size=toc.offset_length(row); total+=size
            need(0<size<=4*1024*1024 and total<=8*1024*1024,'bounded replay chunk/job size exceeded')
            b=parser.extract_chunk_bytes(toc,row,mapped)
            need(parser.blake3_20(b)==toc.meta(row)[:20],'chunk IoHash mismatch')
            return b
        headers=[i for i,c in enumerate(toc.ids) if c[11]&63==6]
        need(len(headers)==1,'exactly one container-header chunk required')
        header_raw=chunk(headers[0]);need(len(header_raw)>=8,'truncated container header')
        version=struct.unpack_from('<I',header_raw,4)[0];need(version==5,'Zen profile requires actual container header v5')
        store=parser.parse_container_header(header_raw)
        for pidhex in subjects:
            pid=int(pidhex,16); cid=pid.to_bytes(8,'little').hex()+'00000001';need(cid in rows,'subject ExportBundleData chunk unavailable')
            row=rows[cid]; data=chunk(row); decoded=decode_package(data,version)
            need(pid in store.packages,'subject missing container store')
            imports=store.packages[pid].imported_packages
            for ref in decoded['import_map']+[e['class_ref'] for e in decoded['exports']]:
                if ref['kind']=='PackageImport':
                    index=ref['imported_package_index'];need(index<len(imports),'imported package reference outside store array')
                    ref['referenced_package_id']=f'{imports[index]:016x}'
            results.append({'package_id':pidhex,'chunk_id':cid,'toc_row':row,'chunk_iohash_20':toc.meta(row)[:20].hex(),
                            'chunk_iohash_verified':True,'container_store_imported_packages':[f'{i:016x}' for i in imports],'decoded':decoded})
    need(source(a.ucas)['sha256']==pins['ucas']['sha256'],'source changed during replay')
    output={'schema':1,'label':'Mac-only','result':'SOURCE_BOUND_EXPORT_METADATA','items_checked':len(results),'sources':pins,'format_sources':public,
            'reader_sha256':source(Path(__file__).parent/'zen_identity.py')['sha256'],'reserved_header_bytes':'Relaxed only by existing parser strict_admission=False; lookup and geometry validated.',
            'packages':results,'class_identity_resolved':False,'compatible':False,'native_shader_proven':False,
            'limits':'Only selected mod export metadata decoded from original hash-pinned bytes. No base export/class bytes supplied, no script-object/class-name mapping, no shader evidence or import compatibility promotion. No original payload copied into outputs.'}
    a.out.write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps({'result':output['result'],'items_checked':len(results),'exports_checked':sum(r['decoded']['exports_checked'] for r in results),'class_identity_resolved':False}))

if __name__=='__main__':main()
