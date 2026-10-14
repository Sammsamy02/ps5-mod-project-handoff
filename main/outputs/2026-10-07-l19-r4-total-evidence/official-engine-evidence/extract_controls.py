#!/usr/bin/env python3
"""Extract literal layout-bearing declarations from immutable supplied Epic source."""
import hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
OUT=Path(__file__).resolve().parent
ENGINE=ROOT/'outputs/2026-10-07-l19-r4-official-engine-source'
DEP=ENGINE/'dependencies'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
receipts=[ENGINE/'SOURCE-RECEIPT.json',DEP/'SOURCE-RECEIPT.json',DEP/'MACRO-SOURCE-RECEIPT.json']
for p in receipts:
    for s in json.loads(p.read_text())['files']:
        q=Path(s['local_path']);assert sha(q)==s['sha256']
        blob=hashlib.sha1(b'blob '+str(len(q.read_bytes())).encode()+b'\0'+q.read_bytes()).hexdigest()
        assert blob==s['blob_sha1']
segments=[]
def between(name,file,start,end,include_end=False):
    raw=file.read_text();lo=raw.index(start);hi=raw.index(end,lo)+(len(end) if include_end else 0)
    text=raw[lo:hi]
    segments.append({'name':name,'source_path':str(file),'source_sha256':sha(file),
                     'start_byte':len(raw[:lo].encode()),'end_byte_exclusive':len(raw[:hi].encode()),
                     'start_line':raw.count('\n',0,lo)+1,'end_line':raw.count('\n',0,hi)+1,
                     'exact_text':text,'text_sha256':hashlib.sha256(text.encode()).hexdigest()})
    return text
def enum(name):
    file=DEP/'ObjectMacros.h';raw=file.read_text();lo=raw.index('enum '+name+'\n{');hi=raw.index('\n};',lo)+3
    return between(name,file,raw[lo:lo+len('enum '+name+'\n{')],raw[hi-3:hi],True)
# Source-defined primitive aliases; no PC compiler type facts are used.
pointer=between('pointer_type_selector',DEP/'GenericPlatform.h','template<typename T32BITS','/**\n* Generic types')
generic=between('generic_platform_types',DEP/'GenericPlatform.h','struct FGenericPlatformTypes','\n};',True)
entry=between('FNameEntryId_data',DEP/'NameTypes.h','\tuint32 Value;','\n\n\tCORE_API static FNameEntryId')
fname=between('FName_data_and_conditions',DEP/'NameTypes.h','\tFNameEntryId\tComparisonIndex;\n#if !UE_FNAME_OUTLINE_NUMBER','\n\n\tfriend const TCHAR* DebugFName')
counter=between('FThreadSafeCounter_data',DEP/'ThreadSafeCounter.h','\tvolatile int32 Counter;','\n};')
variant=between('FFieldVariant_union',ENGINE/'Field.h','\tunion FFieldObjectUnion','\n\n\tstatic constexpr uintptr_t UObjectMask')
fieldclass=between('FFieldClass_data',ENGINE/'Field.h','\t/** Name of this field class */','\n\t/** Creates a default object instance')
field_first=between('FField_first_member_and_first_virtual',ENGINE/'Field.h','\t/** Pointer to the class object representing the type of this FField */','\n\tinline static constexpr uint64 StaticClassCastFlagsPrivate()')
field_other=between('FField_remaining_unconditional_data',ENGINE/'Field.h','\t/** Owner of this field */','\n\t// Constructors.')
field_metadata=between('FField_conditional_metadata_member',ENGINE/'Field.h','#if WITH_METADATA\nprivate:\n\t/** Editor-only meta data map */','\npublic:')+'\n#endif\n'
field_dtor=between('FField_virtual_destructor',ENGINE/'Field.h','\tCOREUOBJECT_API virtual ~FField();','\n\n\t// Begin UObject interface')
noncopy=between('UE_NONCOPYABLE_definition',DEP/'CoreMiscDefines.h','#define UE_NONCOPYABLE(TypeName)','\n\n')
assert 'virtual' not in noncopy and 'pragma' not in noncopy and noncopy.count('= delete;')==4
parts=[pointer,generic,noncopy+'\n','\nusing uint32=FGenericPlatformTypes::uint32;\nusing uint64=FGenericPlatformTypes::uint64;\nusing int32=FGenericPlatformTypes::int32;\nusing SIZE_T=FGenericPlatformTypes::SIZE_T;\n',enum('EClassFlags'),enum('EObjectFlags'),
       '\nclass FField; class FFieldClass; class UObject; class FString; template<class K,class V> class TMap;\n',
       'struct FNameEntryId {\npublic:\n'+entry+'\n};\n',
       'class FName {\npublic:\n'+fname+'\n};\n',
       'class FThreadSafeCounter {\npublic:\n'+counter+'\n};\n',
       'class FFieldVariant {\npublic:\n'+variant+'\n};\n',
       'class FFieldClass {\npublic:\nUE_NONCOPYABLE(FFieldClass);\n'+fieldclass+'\n};\n',
       'class FField {\npublic:\nUE_NONCOPYABLE(FField);\n'+field_first+'\n'+field_other+'\n'+field_dtor+'\n'+field_metadata+'\n};\n']
# The source API macro decorates methods only. Publicizing private members permits
# offsetof controls and does not change data ordering, bases, virtuals or packing.
prelude='''// Generated literal source-layout projection; see EXTRACTION.json for exact byte spans.
#ifndef WITH_CASE_PRESERVING_NAME
#define WITH_CASE_PRESERVING_NAME 0
#endif
#ifndef UE_FNAME_OUTLINE_NUMBER
#define UE_FNAME_OUTLINE_NUMBER 0
#endif
#ifndef WITH_METADATA
#define WITH_METADATA 0
#endif
#define COREUOBJECT_API
#define UE_DEPRECATED(...)
#if PACK_CONTROL == 4
#pragma pack(push,4)
#elif PACK_CONTROL == 1
#pragma pack(push,1)
#endif
'''
layout='\n'.join(parts)
labels=['sizeof_FField','alignof_FField','sizeof_FFieldClass','alignof_FFieldClass','sizeof_FName','alignof_FName','sizeof_FNameEntryId','sizeof_FThreadSafeCounter','sizeof_FFieldVariant',
        'FField_ClassPrivate','FField_Owner','FField_Next','FField_NamePrivate','FField_FlagsPrivate','FFieldClass_Name','FFieldClass_Id','FFieldClass_CastFlags','FFieldClass_ClassFlags','FFieldClass_SuperClass','FFieldClass_DefaultObject','FFieldClass_ConstructFn','FFieldClass_Counter','sizeof_SIZE_T','sizeof_EClassFlags','sizeof_EObjectFlags']
expr=['sizeof(FField)','alignof(FField)','sizeof(FFieldClass)','alignof(FFieldClass)','sizeof(FName)','alignof(FName)','sizeof(FNameEntryId)','sizeof(FThreadSafeCounter)','sizeof(FFieldVariant)',
      '__builtin_offsetof(FField,ClassPrivate)','__builtin_offsetof(FField,Owner)','__builtin_offsetof(FField,Next)','__builtin_offsetof(FField,NamePrivate)','__builtin_offsetof(FField,FlagsPrivate)',
      '__builtin_offsetof(FFieldClass,Name)','__builtin_offsetof(FFieldClass,Id)','__builtin_offsetof(FFieldClass,CastFlags)','__builtin_offsetof(FFieldClass,ClassFlags)','__builtin_offsetof(FFieldClass,SuperClass)','__builtin_offsetof(FFieldClass,DefaultObject)','__builtin_offsetof(FFieldClass,ConstructFn)','__builtin_offsetof(FFieldClass,UnqiueNameIndexCounter)','sizeof(SIZE_T)','sizeof(EClassFlags)','sizeof(EObjectFlags)']
tail='\nextern "C" __attribute__((used)) const uint32 OfficialLayoutFacts[]={\n'+',\n'.join(expr)+'\n};\n'
tail+='extern "C" __attribute__((used)) SIZE_T OfficialFieldSizeBody(const FField* field){return field->FField::GetFieldSize();}\n'
tail+='\n#if PACK_CONTROL == 4 || PACK_CONTROL == 1\n#pragma pack(pop)\n#endif\n'
(OUT/'literal_layout_control.cpp').write_text(prelude+layout+tail)
classes=['FNameEntryId','FName','FThreadSafeCounter','FFieldVariant','FFieldClass','FField']
metadata={'scope':'Literal actual-source data declarations and first size-query body, with methods not affecting layout omitted. Projection is not a full engine translation unit.',
          'official_source_receipts':[{'path':str(p),'sha256':sha(p)} for p in receipts],
          'segments':segments,'classes':classes,'array_labels':labels,
          'transformations':['Non-layout method declarations/bodies, friends, static members and typedefs omitted except FField first virtual GetFieldSize body and virtual destructor declaration.',
                             'Private member blocks publicized for offsetof access; data text/order/types unchanged.',
                             'Actual source-defined generic primitive aliases compiled with saved PS5 compiler; native stores independently constrain the relevant scalar widths.',
                             'Actual UE_NONCOPYABLE macro copied from pinned CoreMiscDefines and expanded: four deleted special-member methods, no data/virtual/packing effect.',
                             'CoreUObject API export macro empty for local object compile; no layout effect expected.',
                             'PACK_CONTROL deliberately varies a single packing scope enclosing all declarations, as an adversarial compile control; source Field.h has no pragma pack between FFieldClass and FField.'],
          'not_proved_by_compile':['Exact game build source commit identity','Full licensed platform header environment','WITH_EDITORONLY_DATA target value','Native pointer/page validity'],
          'output_cpp_sha256':sha(OUT/'literal_layout_control.cpp')}
(OUT/'EXTRACTION.json').write_text(json.dumps(metadata,indent=2)+'\n')
print(json.dumps({'verified_official_sources':sum(len(json.loads(p.read_text())['files']) for p in receipts),'literal_segments':len(segments),'layout_value_labels':len(labels),'output':str(OUT/'literal_layout_control.cpp')}))
