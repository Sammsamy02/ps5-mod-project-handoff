"""Verify actual ELF NID/provider/version rows against the exact import contract."""
import base64,hashlib,struct

ALPHABET='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-'
SALT=bytes.fromhex('518D64A635DED8C1E6B039B1C3E55230')

def nid(name):
    digest=hashlib.sha1(name.encode()+SALT).digest()[:8][::-1]
    return base64.b64encode(digest).decode()[:11].replace('/','-')

def decode_id(text):
    value=0
    for char in text:value=value*64+ALPHABET.index(char)
    return value

def verify(blob,contract,require_getter=False):
    if blob[:4]!=b'\x7fELF':raise ValueError('bare ELF required')
    offset=struct.unpack_from('<Q',blob,40)[0];count,index=struct.unpack_from('<HH',blob,60)
    sections=[struct.unpack_from('<II4QII2Q',blob,offset+i*64) for i in range(count)]
    s=sections[index];strings=blob[s[4]:s[4]+s[5]]
    named={strings[s[0]:].split(b'\0')[0].decode():s for s in sections}
    s=named['.dynstr'];dynstr=blob[s[4]:s[4]+s[5]]
    def string(pos):return dynstr[pos:].split(b'\0')[0].decode()
    modules={};libraries={};current_module=None;s=named['.dynamic']
    for pos in range(s[4],s[4]+s[5],16):
        tag,value=struct.unpack_from('<QQ',blob,pos)
        if tag==0x61000045:
            key=value>>48;record={'name':string(value&0xffffffff),'version':(value>>32)&0xffff,'value_offset':pos+8}
            if key in modules:raise ValueError('duplicate module identity')
            modules[key]=record;current_module=key
        elif tag==0x61000049:
            key=value>>48;record={'name':string(value&0xffffffff),'version':(value>>32)&0xffff,'module':current_module,'value_offset':pos+8}
            if key in libraries:raise ValueError('duplicate library identity')
            libraries[key]=record
    rows=contract['contract']['imports'];expected={nid(r['ImportSymbol']['Name']):r for r in rows}
    if len(expected)!=len(rows):raise ValueError('ambiguous contract NID')
    result=[];seen=set();s=named['.dynsym']
    for pos in range(s[4],s[4]+s[5],24):
        name,info,other,section,value,size=struct.unpack_from('<IBBHQQ',blob,pos)
        if section or info>>4 not in (1,2):continue
        text=string(name);parts=text.split('#')
        if len(parts)!=3 or parts[0] not in expected:raise ValueError('unknown/malformed NID: '+text)
        row=expected[parts[0]];declaration=row['ImportSymbol'];lid,mid=map(decode_id,parts[1:])
        module=modules.get(mid);library=libraries.get(lid)
        if not module or not library or library['module']!=mid:raise ValueError('provider ID/ownership mismatch: '+text)
        if (module['name']!=declaration['ModuleName'] or module['version']!=declaration['ModuleVersion']
            or library['name']!=declaration['LibraryName'] or library['version']!=declaration['LibraryVersion']):
            raise ValueError('provider name/version mismatch: '+declaration['Name'])
        if declaration['Name'] in seen:raise ValueError('duplicate import: '+declaration['Name'])
        seen.add(declaration['Name'])
        if declaration['Name']=='scePthreadGetthreadid' and (info&15)!=2:raise ValueError('getter import must be FUNC')
        result.append({'name':declaration['Name'],'published':text,'module':module,'library':library,'type':info&15})
    if seen!={r['ImportSymbol']['Name'] for r in rows}:raise ValueError('actual/contract import set mismatch')
    if require_getter and 'scePthreadGetthreadid' not in seen:raise ValueError('getter absent')
    return {'status':'PASS_MAC_ONLY_EXACT_ELF_IDENTITIES','count':len(result),'imports':result,'native_resolution_proven':False}
