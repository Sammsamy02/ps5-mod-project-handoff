#!/usr/bin/env python3
"""Read-only whole logical decoded-chunk metadata intake; no UCAS/range reader."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

CAP=8*1024*1024
KEYS=('owner_toc_sha256','owner_container_id_u64','toc_row','chunk_id')
def need(ok,why):
    if not ok:raise ValueError(why)
def sha(data):return hashlib.sha256(data).hexdigest()
def load(name,p):
    s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);sys.modules[name]=m;s.loader.exec_module(m);return m
class Inputs:
    def __init__(self):self.total=0;self.records={}
    def read(self,ref,bin_only=False):
        p=Path(ref['path']).resolve()
        need(p.is_file(),'missing local input')
        need(not bin_only or p.suffix.lower()=='.bin','whole decoded chunk must be an explicit .bin input')
        n=p.stat().st_size;need(0<n<=CAP and self.total+n<=CAP,'8MiB aggregate input bound exceeded')
        b=p.read_bytes();self.total+=len(b)
        need(len(b)==n and sha(b)==ref['sha256'],'source SHA mismatch')
        self.records[str(p)]={'path':str(p),'bytes':len(b),'sha256':sha(b)};return b
    def unchanged(self):
        for r in self.records.values():
            p=Path(r['path']);need(p.stat().st_size==r['bytes'] and sha(p.read_bytes())==r['sha256'],'source changed during intake')
def geometries(audit):
    rows=[]
    for r in audit['required_base_export_inputs']:rows.extend(r['captured_owners'])
    for r in audit.get('downstream_store_dependency_layer',[]):rows.extend(r['captured_export_owners'])
    for r in audit['missing_owner_stores']:rows.append(r['geometry'])
    if 'base_store_geometry' in audit:rows.append(audit['base_store_geometry'])
    unique={}
    for g in rows:
        key=tuple(g[k] for k in KEYS)
        need(key not in unique or unique[key]==g,'ambiguous audited geometry')
        unique[key]=g
    return unique

def run(job,job_bytes=None):
    need(job.get('schema')==1,'unsupported job schema')
    need(isinstance(job.get('targets'),list) and 0<len(job['targets'])<=32,'empty/excess targets')
    inputs=Inputs();inputs.total=len(json.dumps(job).encode()) if job_bytes is None else job_bytes
    need(inputs.total<=CAP,'8MiB job bound exceeded')
    audit=json.loads(inputs.read(job['audit']));rows=geometries(audit)
    parserref=audit['sources']['parser'];inputs.read(parserref);parser=load('base_intake_overlay_parser',Path(parserref['path']))
    inputs.read(job['zen_reader']);zen=load('base_intake_zen_reader',Path(job['zen_reader']['path']))
    modref=audit['sources']['mod_identity'];mod=json.loads(inputs.read(modref))
    admitted=[];seen=set()
    for target in job['targets']:
        selector=target['selector'];need(set(selector)==set(KEYS),'exact selector fields required')
        key=tuple(selector[k] for k in KEYS);need(key in rows,'no exact audited chunk owner/row/ID')
        need(key not in seen,'duplicate target');seen.add(key);g=rows[key]
        cid=bytes.fromhex(g['chunk_id']);need(len(cid)==12 and cid[11]&63==g['chunk_type'] and g['chunk_type'] in (1,6),'unsupported chunk ID/type')
        identity=f'{int.from_bytes(cid[:8],"little"):016x}'
        need(g['chunk_type']!=6 or identity==g['owner_container_id_u64'],'store chunk/container identity mismatch')
        b=inputs.read(target['source'],True)
        need(len(b)==g['logical_length'],'whole logical length mismatch (partial input refused)')
        need(parser.blake3_20(b).hex()==g['expected_iohash_20'],'whole chunk IoHash mismatch')
        admitted.append((g,b,identity))
    stores={};storeout=[]
    requested_packages={r[2] for r in admitted if r[0]['chunk_type']==1}
    requested_packages.update(r['package_id'] for r in audit['required_base_export_inputs'])
    requested_packages.update(r['target_package'] for r in audit.get('downstream_store_dependency_layer',[]))
    for g,b,identity in admitted:
        if g['chunk_type']!=6:continue
        h=parser.parse_container_header(b)
        need(f'{h.container_id:016x}'==identity,'parsed store owner mismatch')
        key=(g['owner_toc_sha256'],identity);need(key not in stores,'ambiguous owner store');stores[key]=h
        storeout.append({'geometry':g,'container_header_version':parser.CH_VERSION,'total_packages_checked':len(h.packages),'projection':'audited required/downstream package IDs only','packages':[{'package_id':f'{p:016x}','imported_packages':[f'{i:016x}' for i in e.imported_packages],'shader_map_hashes':[x.hex() for x in e.shader_map_hashes]} for p,e in sorted(h.packages.items()) if f'{p:016x}' in requested_packages]})
    exports=[]
    requirements={r['package_id']:r for r in audit['required_base_export_inputs']}
    for g,b,pid in admitted:
        if g['chunk_type']!=1:continue
        row={'geometry':g,'package_id':pid,'compatible':False};exports.append(row)
        store=stores.get((g['owner_toc_sha256'],g['owner_container_id_u64']))
        if store is None:row['result']='OWNER_STORE_REQUIRED';continue
        need(int(pid,16) in store.packages,'export package absent from verified owner store')
        decoded=zen.decode_package(b,parser.CH_VERSION);imports=store.packages[int(pid,16)].imported_packages
        for ref in decoded['import_map']+[e['class_ref'] for e in decoded['exports']]:
            if ref['kind']=='PackageImport':
                index=ref['imported_package_index'];need(index<len(imports),'package-import index exceeds verified store')
                ref['referenced_package_id']=f'{imports[index]:016x}'
        hashes={}
        for e in decoded['exports']:
            h=e['public_export_hash']
            if e['public_export']:need(h not in hashes,'ambiguous public export hash');hashes[h]=e
        required=requirements.get(pid,{}).get('expected_public_hashes_from_verified_mod_imports',[])
        matches=[{'public_export_hash':h,'present':h in hashes,'base_export_name':hashes[h]['name'] if h in hashes else None} for h in required]
        projection=[]
        for p in mod['packages']:
            if p['package_id']!=pid:continue
            for e in p['decoded']['exports']:
                base=hashes.get(e['public_export_hash']);projection.append({'mod_export_name':e['name'],'public_export_hash':e['public_export_hash'],'base_export_name':base['name'] if base else None,'result':'name_and_public_hash_match' if base and base['name']==e['name'] else 'absent_or_different'})
        row.update(result='IDENTITY_METADATA_ONLY',decoded=decoded,required_public_hashes=matches,mod_export_projection=projection,verified_store_imported_packages=[f'{i:016x}' for i in imports])
    inputs.unchanged()
    return {'schema':1,'label':'Mac-only whole-chunk metadata replay','items_checked':len(admitted),'source_files_checked':len(inputs.records),'result':'LOCAL_CHUNK_METADATA_ONLY','audit_sha256':job['audit']['sha256'],'sources':list(inputs.records.values()),'aggregate_input_bytes':inputs.total,'sources_unchanged':True,'stores':storeout,'exports':exports,'compatible':False,'material_parent_proven':False,'native_shader_proven':False,'import_closure_complete':False,'limits':['Only supplied complete decoded logical chunks were admitted by SHA256, exact audited owner/row/ID, length and whole BLAKE3-20. No UCAS or slice reader.','Store metadata and package/public-hash/name matches are identities only. Script/import class refs remain unresolved by this tool. No property-role, base equivalence, compatibility or runtime proof.','ContainerHeader support retains the pinned parser v5 restrictions. Zen reader additionally bounds each export chunk to4MiB. Audit completeness and capture provenance remain those of the pinned audit.']}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--job',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
    home=Path(__file__).resolve().parent;out=a.out.resolve();need(out.is_relative_to(home),'output must stay in base-payload-intake')
    need(a.job.stat().st_size<=CAP,'job over8MiB');raw=a.job.read_bytes();job=json.loads(raw)
    protected={a.job.resolve(),Path(job['audit']['path']).resolve(),Path(job['zen_reader']['path']).resolve()}
    protected.update(Path(t['source']['path']).resolve() for t in job['targets'])
    need(out not in protected,'output would overwrite source')
    result=run(job,len(raw));protected.update(Path(r['path']).resolve() for r in result['sources']);need(out not in protected,'output would overwrite pinned input')
    need(a.job.read_bytes()==raw,'job changed during intake');result['job_sha256']=sha(raw)
    text=json.dumps(result,indent=2)+'\n';need(len(text.encode())<=CAP,'metadata output over8MiB');out.write_text(text);print(json.dumps({'items_checked':result['items_checked'],'source_files_checked':result['source_files_checked'],'exports':len(result['exports']),'stores':len(result['stores']),'compatible':False}))
if __name__=='__main__':main()
