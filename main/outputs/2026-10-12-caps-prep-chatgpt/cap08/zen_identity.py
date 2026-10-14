#!/usr/bin/env python3
"""Bounded UE5/v5 Zen identity metadata reader; no asset/property decoder.

Independent Python implementation of public retoc/CUE4Parse format facts.
Provenance/licensing is recorded by the replay tool and ZEN-IDENTITY.md.
Only unversioned SoftPackageReferencesOffset (=5) packages are supported.
"""
import hashlib
import struct

def need(ok, why):
    if not ok: raise ValueError(why)

def object_ref(raw):
    kind=raw>>62; value=raw&((1<<62)-1)
    if raw==(1<<64)-1: label='Null'
    else:
        need(kind<3,'invalid non-null package object type')
        label=('Export','ScriptImport','PackageImport')[kind]
    out={'raw':f'{raw:016x}','kind':label,'value':value,'class_identity_resolved':False}
    if label=='Export':out['export_index']=raw&0xffffffff
    if label=='PackageImport':out.update(imported_package_index=value>>32,imported_public_export_hash_index=raw&0xffffffff)
    return out

def decode_package(data,container_header_version):
    need(container_header_version==5,'unsupported container-header version')
    need(len(data)>=64,'package too short for name-count field')
    need(len(data)<=4*1024*1024,'package size outside bounded profile')
    has_version,header,name_index,name_number,flags,cooked=struct.unpack_from('<6I',data)
    need(has_version==0,'versioned package metadata unsupported')
    need(header>=64,'header ends before name-count field')
    need(header<=len(data),'invalid Zen header size')
    iph,imp,exp,bundle,deph,depe,importnames,cellimp,cellexp=struct.unpack_from('<9i',data,24)
    order=[60,iph,imp,exp,cellimp,cellexp,bundle,deph,depe,importnames,header]
    need(order==sorted(order),'nonmonotonic Zen metadata region offsets')
    need((imp-iph)%8==0 and (exp-imp)%8==0 and (cellimp-exp)%72==0,'misaligned import/export map region')
    count=(cellimp-exp)//72
    need(0<count<=65536,'zero/excess export count')
    pos=60; names_count=struct.unpack_from('<I',data,pos)[0];pos+=4
    need(0<names_count<=65536 and pos+12<=iph,'invalid/empty name batch')
    string_bytes,hash_algo=struct.unpack_from('<IQ',data,pos);pos+=12
    need(hash_algo==0xc1640000,'unsupported name batch hash version')
    need(pos+10*names_count<=iph,'truncated name batch hashes/lengths')
    hashes=data[pos:pos+8*names_count];pos+=8*names_count
    lengths=[int.from_bytes(data[pos+2*i:pos+2*i+2],'big') for i in range(names_count)];pos+=2*names_count
    names=[]; strings_start=pos
    for word in lengths:
        units=word&0x7fff; width=2 if word&0x8000 else 1; n=units*width
        need(pos+n<=iph,'name string exceeds metadata bounds')
        try:names.append(data[pos:pos+n].decode('utf-16-le' if width==2 else 'utf-8'))
        except UnicodeError as e:raise ValueError('unsupported invalid name encoding') from e
        pos+=n
    need(pos-strings_start==string_bytes,'name batch byte count mismatch')
    def mapped(index,number):
        need(index>>30==0,'non-package mapped name unsupported')
        index&=(1<<30)-1;need(index<len(names),'mapped name index outside name batch')
        return names[index]+('_'+str(number-1) if number else '')
    exports=[]
    for i in range(count):
        at=exp+72*i
        serial,size,idx,num,outer,cls,sup,template,pub,objflags,filter_flags,padding=struct.unpack_from('<QQIIQQQQQIB3s',data,at)
        need(filter_flags<=2 and padding==bytes(3),'unknown export filter/padding')
        refs={k:object_ref(v) for k,v in [('outer',outer),('class',cls),('super',sup),('template',template)]}
        for ref in refs.values():
            if ref['kind']=='Export':need(ref['export_index']<count,'local export reference out of bounds')
        exports.append({'export_index':i,'entry_offset':at,'name':mapped(idx,num),'public_export_hash':f'{pub:016x}',
                        'public_export':pub not in (0,(1<<64)-1),'class_ref':refs['class'],'outer_ref':refs['outer'],
                        'super_ref':refs['super'],'template_ref':refs['template'],'object_flags':objflags,
                        'filter_flags':filter_flags,'cooked_serial_offset_metadata':serial,'cooked_serial_size_metadata':size})
    for row in exports:
        ref=row['class_ref']
        if ref['kind']=='Export':ref['referenced_local_export_name']=exports[ref['export_index']]['name']
        # A local export's name is not itself verified engine class identity.
    hashes_import=[f'{v:016x}' for v in struct.unpack_from('<%dQ'%((imp-iph)//8),data,iph)]
    imports=[object_ref(v) for v in struct.unpack_from('<%dQ'%((exp-imp)//8),data,imp)]
    for ref in [r['class_ref'] for r in exports]+imports:
        if ref['kind']=='PackageImport':
            n=ref['imported_public_export_hash_index'];need(n<len(hashes_import),'import public-hash index out of bounds')
            ref['referenced_public_export_hash']=hashes_import[n]
    return {'label':'Mac-only','profile':'UE5 unversioned Zen / container header v5','package_name':mapped(name_index,name_number),
            'chunk_sha256':hashlib.sha256(data).hexdigest(),'header_size':header,'cooked_header_size_metadata':cooked,
            'package_flags':flags,'names_checked':len(names),'exports_checked':len(exports),'exports':exports,'import_map':imports,
            'imported_public_export_hashes':hashes_import,'name_hashes_sha256':hashlib.sha256(hashes).hexdigest(),
            'unparsed_between_names_and_import_hashes':{'bytes':iph-pos,'sha256':hashlib.sha256(data[pos:iph]).hexdigest()},
            'limits':'Export identity bytes decoded; script/package-import class names unresolved. Property data, original cooked serial geometry, base export equivalence, native shader usability and compatibility are not established.'}
