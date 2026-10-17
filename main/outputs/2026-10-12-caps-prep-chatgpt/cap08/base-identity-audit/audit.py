#!/usr/bin/env python3
"""Small metadata audit only; no UCAS acquisition/read or compatibility claims."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import sys

CAP=8*1024*1024

def need(ok,why):
    if not ok:raise ValueError(why)

def sha_file(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()

def small(p):
    need(p.is_file() and p.stat().st_size<=CAP,'missing/over-cap metadata input')
    b=p.read_bytes();return b,{'path':str(p.resolve()),'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b)}

def load(name,p):
    spec=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(spec);sys.modules[name]=m;spec.loader.exec_module(m);return m

def import_matches(identities):
    exports={}
    for p in identities['packages']:
        for e in p['decoded']['exports']:
            key=(p['package_id'],e['public_export_hash']);need(key not in exports,'ambiguous package/public export hash');exports[key]=e['name']
    rows=[]
    for p in identities['packages']:
        for slot,ref in enumerate(p['decoded']['import_map']):
            if ref['kind']!='PackageImport':continue
            target=(ref['referenced_package_id'],ref['referenced_public_export_hash'])
            row={'source_package':p['package_id'],'source_import_map_slot':slot,'target_package':target[0],'target_public_export_hash':target[1],'runtime_compatible':False}
            if target in exports:row.update(resolution='mod_export_hash_match',target_export_name=exports[target])
            else:row['resolution']='base_export_payload_required'
            rows.append(row)
    need(len(rows)>0,'zero package-import edges checked');return rows

class RangedToc:
    def __init__(self,path,pin,parser):
        self.path=path;self.parser=parser;self.pin=pin;self.reads=[];self.bytes_read=0
        need(sha_file(path)==pin,'TOC source SHA mismatch')
        self.f=path.open('rb');h=self.read(0,144);self.h=parser.parse_toc_header(h)
        x=self.h;self.n=x['toc_entry_count'];self.bs=x['compression_block_size']
        need(x['version']==8 and x['header_size']==144 and x['block_entry_size']==12 and self.n>0 and self.bs>0,'unsupported TOC metadata profile')
        need(x['container_flags']==8 and x['compression_method_name_count']==0 and x['compression_method_name_length']==32 and x['partition_count']==1 and x['partition_size']==parser.U64_MAX,'unsupported TOC flags/geometry')
        need(x['key_guid']==bytes(16),'encrypted key guid unsupported')
        raw=self.read(144,12*self.n);self.ids=tuple(raw[i*12:(i+1)*12] for i in range(self.n));need(len(set(self.ids))==self.n,'duplicate chunk ids')
        self.rows={c.hex():i for i,c in enumerate(self.ids)}
        self.seed_start=144+22*self.n;self.overflow_start=self.seed_start+4*x['perfect_hash_seeds_count']
        self.block_start=self.overflow_start+4*x['chunks_without_perfect_hash_count']
        self.meta_start=self.block_start+12*x['block_entry_count']+x['directory_index_size']
        need(self.meta_start+24*self.n==path.stat().st_size,'TOC EOF/region geometry mismatch')
    def read(self,at,n):
        need(at>=0 and n>=0 and self.bytes_read+n<=CAP,'random-read cap exceeded')
        self.f.seek(at);b=self.f.read(n);need(len(b)==n,'short TOC metadata read');self.bytes_read+=n
        self.reads.append({'offset':at,'bytes':n,'sha256':hashlib.sha256(b).hexdigest()});return b
    def geometry(self,cid):
        need(cid in self.rows,'requested chunk absent');row=self.rows[cid];c=bytes.fromhex(cid);x=self.h
        seeds=x['perfect_hash_seeds_count']
        if seeds:
            bucket=self.parser.hash_with_seed(c,0)%seeds;seed=struct.unpack('<i',self.read(self.seed_start+4*bucket,4))[0]
            need(seed!=0,'zero seed source lookup')
            slot=-seed-1 if seed<0 else self.parser.hash_with_seed(c,seed)%self.n
            if slot>=self.n:
                overflow=x['chunks_without_perfect_hash_count'];raw=self.read(self.overflow_start,overflow*4);candidates=struct.unpack('<%dI'%overflow,raw)
                need(row in candidates,'requested row absent from lookup overflow')
            else:need(slot==row and self.ids[slot]==c,'source lookup does not resolve requested row')
        pair=self.read(144+12*self.n+10*row,10);logical=int.from_bytes(pair[:5],'big');length=int.from_bytes(pair[5:],'big');need(length>0,'zero chunk length')
        first,last=logical//self.bs,(logical+length-1)//self.bs;need(last<x['block_entry_count'],'chunk outside block table')
        blocks=[]
        for i in range(first,last+1):
            at=self.block_start+12*i;b=self.read(at,12);physical=int.from_bytes(b[:5],'little');stored=int.from_bytes(b[5:8],'little');decoded=int.from_bytes(b[8:11],'little');method=b[11]
            begin=logical%self.bs if i==first else 0;end=(logical+length-1)%self.bs+1 if i==last else self.bs
            need(method==0 and stored==decoded and 0<=begin<end<=decoded<=self.bs,'unsupported stored-block slice')
            blocks.append({'descriptor_offset_in_toc':at,'physical_block_offset':physical,'stored_bytes':stored,'decoded_bytes':decoded,'slice_begin':begin,'slice_end':end})
        meta=self.read(self.meta_start+24*row,24);need(meta[20] in (0,2) and meta[21:]==bytes(3),'unsupported chunk metadata')
        return {'chunk_id':cid,'toc_row':row,'chunk_type':c[11]&63,'logical_offset':logical,'logical_length':length,'expected_iohash_20':meta[:20].hex(),'stored_blocks':blocks,'owner_toc_sha256':self.pin,'owner_container_id_u64':f'{int.from_bytes(x["container_id"],"little"):016x}','ucas_name':self.path.with_suffix('.ucas').name,'payload_present_verified':False}
    def close(self):
        self.f.close();need(sha_file(self.path)==self.pin,'TOC changed during ranged audit')
        return {'path':str(self.path.resolve()),'sha256':self.pin,'bytes':self.path.stat().st_size,'random_read_bytes':self.bytes_read,'range_reads':self.reads,'before_after_sha256_match':True}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--repo',type=Path,default=Path.cwd());ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();root=a.repo.resolve();home=Path(__file__).resolve().parent
    need(a.out.resolve().is_relative_to(home),'output must stay in base-identity-audit')
    def p(s):return root/s
    sources={}
    def read_json(key,path):
        b,ref=small(path);sources[key]=ref;return json.loads(b)
    ident=read_json('mod_identity',p('outputs/2026-10-12-caps-prep-chatgpt/cap08/leon-zen-identities.json'))
    script=read_json('script_resolution',p('outputs/2026-10-12-caps-prep-chatgpt/cap08/script-objects/leon-script-class-resolution.json'))
    selection=read_json('owner_selection',p('outputs/2026-10-12-caps-prep-chatgpt/cap07/evidence/leon-container-selection.json'))
    member=read_json('store_membership',p('outputs/2026-10-12-caps-prep-chatgpt/cap07/evidence/leon-secondary-import-membership.json'))
    closure=read_json('mod_closure',p('outputs/2026-10-12-caps-prep-chatgpt/cap07/evidence/leon-secondary-mod-import-closure.json'))
    parserpath=Path(script['sources']['parser']['path']);pb,pref=small(parserpath);sources['parser']=pref;need(pref['sha256']==ident['sources']['parser']['sha256']==closure['parser_sha256'],'parser source pin differs');parser=load('base_identity_existing_parser',parserpath)
    edges=import_matches(ident);targets=set(closure['closure_packages'])|set(closure['external_imports']);owned={pid:[] for pid in targets};readers=[]
    for base in selection['bases']:
        t=RangedToc(Path(base['path']),base['sha256'],parser);readers.append(t)
        for pid in targets:
            cid=int(pid,16).to_bytes(8,'little').hex()+'00000001'
            if cid in t.rows:owned[pid].append(t.geometry(cid))
    for pid,owners in owned.items():need(len(owners)<=1,'ambiguous captured owner: '+pid)
    # Store dependency list is independently IoHash-bound to the chunk0 TOC row.
    storepath=Path(member['store_path']);storebytes,storeref=small(storepath);sources['base_store']=storeref;need(storeref['sha256']==member['store_sha256'],'base store source mismatch')
    store=parser.parse_container_header(storebytes);storecid=store.container_id.to_bytes(8,'little').hex()+'00000006';storer=[t for t in readers if storecid in t.rows];need(len(storer)==1,'base store TOC association ambiguous')
    storegeo=storer[0].geometry(storecid);need(parser.blake3_20(storebytes).hex()==storegeo['expected_iohash_20'],'base store IoHash mismatch')
    base_store_rows=[]
    for pid in targets:
        if int(pid,16) in store.packages:
            e=store.packages[int(pid,16)];base_store_rows.append({'package_id':pid,'imported_packages':[f'{x:016x}' for x in e.imported_packages],'shader_map_hashes':[x.hex() for x in e.shader_map_hashes],'store_container_id':f'{store.container_id:016x}'})
    downstream=[]
    # This is a source-backed package-store dependency layer, not a complete
    # export/property closure. Do not recursively infer object identities.
    for row in base_store_rows:
        for target in row['imported_packages']:
            cid=int(target,16).to_bytes(8,'little').hex()+'00000001'; owners=[]
            for t in readers:
                if cid in t.rows:owners.append(t.geometry(cid))
            need(len(owners)<=1,'ambiguous downstream captured owner')
            entry=store.packages.get(int(target,16))
            downstream.append({'source_package':row['package_id'],'target_package':target,'captured_export_owners':owners,
                               'known_chunk0_store_membership':entry is not None,
                               'known_store_imports':[f'{i:016x}' for i in entry.imported_packages] if entry else None,
                               'export_public_hash_identity_resolved':False,'closure_complete':False})
    missing_store=[]
    for t in readers:
        owner_pid=[pid for pid,owners in owned.items() if owners and owners[0]['owner_toc_sha256']==t.pin]
        if owner_pid and int.from_bytes(t.h['container_id'],'little')!=store.container_id:
            cid=t.h['container_id'].hex()+'00000006';need(cid in t.rows,'owned container lacks ContainerHeader chunk')
            missing_store.append({'needed_for_packages':owner_pid,'purpose':'complete owner package-store entries/import arrays','geometry':t.geometry(cid)})
    # Reuse the actually bound small global table to resolve all ScriptImport slots.
    ucas=Path(script['sources']['ucas']['path']);raw,ref=small(ucas);sources['global_metadata_ucas']=ref;need(ref==script['sources']['ucas'],'global metadata identity mismatch')
    meta=script['scriptobjects'];payload=raw[meta['logical_offset']:meta['logical_offset']+meta['logical_length']]
    need(hashlib.sha256(payload).hexdigest()==meta['decoded_sha256'] and parser.blake3_20(payload).hex()==meta['iohash_20'],'global metadata chunk mismatch')
    decoderpath=Path(script['sources']['decoder']['path']);db,dr=small(decoderpath);sources['decoder']=dr;need(dr==script['sources']['decoder'],'decoder source changed');decoder=load('base_audit_existing_script_decoder',decoderpath);table=decoder.read_script_objects(payload)
    script_imports=[]
    for pkg in ident['packages']:
        for i,r in enumerate(pkg['decoded']['import_map']):
            if r['kind']=='ScriptImport':
                index=int(r['raw'],16);need(index in table.objects,'unresolved captured script import')
                script_imports.append({'source_package':pkg['package_id'],'import_map_slot':i,'script_raw':r['raw'],'captured_outer_chain_path':table.object_path(index),'runtime_compatible':False})
    # Existing partial base-header slices are deliberately quarantined, not decoded.
    quarantined=[]
    for number in (1,2):
        recpath=p(f'.superpowers/native-observations/unreal-external-dependency-headers-{number}/receipt.json');d=read_json('header_receipt_'+str(number),recpath)
        for e in d['entries']:
            c=e.get('candidate',{});pid=c.get('package_id_u64_le_hex')
            if pid not in targets or 'header_path' not in e:continue
            hp=Path(e['header_path']);hb,hr=small(hp);sources['quarantined_header_'+pid]=hr
            quarantined.append({'package_id':pid,'header_source':hr,'declared_header_bytes':e['declared_header_bytes'],'header_matches_saved_hash':hr['sha256']==e['sha256'],'atomic_source_identity_proven':d.get('atomic_source_identity_proven',False),'range_final_control_completion_observed':d.get('range_final_control_completion_observed',False),'admitted_for_identity':False,'reason':'partial header has no whole-chunk IoHash and source atomic/control completion not proven'})
    req=[]
    for pid in sorted(targets):
        need_for=[e for e in edges if e['target_package']==pid]
        req.append({'package_id':pid,'captured_owners':owned[pid],'expected_public_hashes_from_verified_mod_imports':[e['target_public_export_hash'] for e in need_for],
                    'required': 'provenance-complete base ExportBundleData chunk, validate IoHash, decode export/class/import metadata' if owned[pid] else 'complete base owner catalogue before treating captured absence as global absence',
                    'base_export_identity_resolved':False})
    toc_sources=[t.close() for t in readers];need(sum(t['random_read_bytes'] for t in toc_sources)<=CAP,'aggregate random-read budget exceeded')
    output={'schema':1,'label':'Mac-only','result':'BASE_IDENTITY_INPUT_GAPS_EXACT','sources':sources,'toc_ranged_sources':toc_sources,'mod_package_import_edges':edges,'script_import_metadata':script_imports,'captured_base_store_rows':base_store_rows,'downstream_store_dependency_layer':downstream,'import_closure_complete':False,'base_store_geometry':storegeo,'missing_owner_stores':missing_store,'required_base_export_inputs':req,'quarantined_partial_headers':quarantined,'base_identity_resolved':False,'compatible':False,'native_shader_proven':False,'limits':['No UCAS read except already-bound3.4MB ScriptObjects metadata; no mod or base asset payload copied.','All large-TOC operations are streaming SHA<=1MiB buffers and <=8MiB aggregate recorded random metadata reads; no whole TOC parsing/materialization.','Package import slot/hash equality resolves metadata reference, not material-parent/property role or runtime compatibility.','Saved header candidates with unproven atomic/range provenance are not promoted or decoded. Captured owner absence is not global absence.']}
    for ref in sources.values():
        need(sha_file(Path(ref['path']))==ref['sha256'],'small source changed during audit')
    output['small_inputs_unchanged']=True
    a.out.write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps({'mod_hash_matches':sum(e['resolution']=='mod_export_hash_match' for e in edges),'external_export_gaps':sum(e['resolution']=='base_export_payload_required' for e in edges),'script_import_slots_resolved':len(script_imports),'base_store_rows':len(base_store_rows),'random_toc_bytes':sum(t['random_read_bytes'] for t in toc_sources),'base_identity_resolved':False}))

if __name__=='__main__':main()
