#!/usr/bin/env python3
"""test_sidecar_emit.py — [agent HH 2026-10-10] Mac-only test for the l26
packaging driver's PRX symbols sidecar emission (PHASEA-PLANS #5 BUILD GAP).

Never touches the console and never runs the driver's --execute mode: it
fabricates a minimal package-receipt shape (synthetic bare ELF + fabricated
-r map + fake faked-signed PRX twins), calls emit_symbols_sidecar() directly
via import, and round-trips the emitted sidecar through the frozen consumer
contract phasea-tools/symbolize_crash.py::load_prx_symbols_sidecar
(accept on the real pairing + refuse on every tamper axis).

Writes SIDECAR-EMIT-TEST-RECEIPT.json beside this script.
"""
import importlib.util,json,struct,sys,shutil
from pathlib import Path

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]                       # .../what
DRIVER_P=ROOT/'outputs/2026-10-08-l26-runtime/l26_packaging_driver.py'
SYMZ_P=ROOT/'outputs/2026-10-08-l25-prep/phasea-tools/symbolize_crash.py'
FIX=HERE/'fixture-package'

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

# ------------------------------------------------ synthetic bare ELF ------
def build_bare_elf():
    """Minimal ET_EXEC ELF exercising exactly what the emit path consumes:
    .text @0x1000 (0x60 B), .dynsym (2 defined syms at the DT_INIT/DT_FINI
    anchors 0x15a0/0x1e00), .dynstr, .dynamic, .note.gnu.build-id, .shstrtab.
    Same entry layouts (Elf64_Shdr '<IIQQQQIIQQ', Elf64_Sym '<IBBHQQ') the
    frozen consumer parser reads."""
    # build .shstrtab incrementally
    shstr=bytearray(b'\0');name_off={}
    for n in ('.text','.dynstr','.dynsym','.dynamic','.note.gnu.build-id','.shstrtab'):
        name_off[n]=len(shstr);shstr+=n.encode()+b'\0'
    text=b'\x90'*(0x60)
    dynstr=b'mod_start_fake\0mod_stop_fake\0'
    def sym(nameo,value,size):
        return struct.pack('<IBBHQQ',nameo,0x12,0,1,value,size)
    dynsym=sym(0,0x15a0,0x40)+sym(15,0x1e00,0x30)
    dynamic=struct.pack('<qQ',12,0x15a0)+struct.pack('<qQ',13,0x1e00)+struct.pack('<qQ',0,0)
    note=struct.pack('<III',4,8,3)+b'GNU\0'+bytes.fromhex('aabbcc1122334455')
    off=64;parts={}
    def place(data,align=8):
        nonlocal off
        off=(off+align-1)&~(align-1);o=off;off+=len(data);return o
    text_o=place(text,16);dynstr_o=place(dynstr,1)
    dynsym_o=place(dynsym,8);dynamic_o=place(dynamic,8)
    note_o=place(note,4);shstr_o=place(bytes(shstr),1)
    shoff=(off+15)&~15
    def sh(name,typ,flags,addr,o,size,link=0,info=0,align=1,entsize=0):
        return struct.pack('<IIQQQQIIQQ',name_off.get(name,0),typ,flags,addr,o,size,link,info,align,entsize)
    shdrs=(sh('',0,0,0,0,0)
           +sh('.text',1,6,0x1000,text_o,len(text),align=16)
           +sh('.dynstr',3,0,0,dynstr_o,len(dynstr))
           +sh('.dynsym',11,0,0,dynsym_o,len(dynsym),link=2,info=1,align=8,entsize=24)
           +sh('.dynamic',6,3,0,dynamic_o,len(dynamic),align=8,entsize=16)
           +sh('.note.gnu.build-id',7,0,0,note_o,len(note),align=4)
           +sh('.shstrtab',3,0,0,shstr_o,len(shstr)))
    ehdr=struct.pack('<16sHHIQQQIHHHHHH',b'\x7fELF\x02\x01\x01'+b'\0'*9,
                     2,62,1,0x15a0,0,shoff,0,64,0,0,64,7,6)
    out=bytearray(ehdr)+bytearray(shoff-len(ehdr))
    for o,d in ((text_o,text),(dynstr_o,dynstr),(dynsym_o,dynsym),
                (dynamic_o,dynamic),(note_o,note),(shstr_o,bytes(shstr))):
        out[o:o+len(d)]=d
    out+=shdrs
    return bytes(out),shoff

def build_map():
    """Fabricated `-r` partial-link map (ALL VMA=0, real column windows:
    [0:16) vma, [16:33) lma, [33:42) size, [42:48) align, name at 48+)."""
    def row(vma,size,align,name):
        return '%16x%17x%9x%6x %s'%(vma,0,size,align,name)
    return '\n'.join((
        'Archive member included because of file (symbol)',
        row(0,0x20,16,'.text.func_one'),
        row(0,0x20,16,'.text.func_two'),
        row(0,0x40,16,'.rodata.str'),       # class absent from PRX -> skipped
        row(0,0x10,16,'.text.tail'),
        ''))

def sha(p):
    import hashlib
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    results=[];ok_all=True
    def check(name,ok,detail=None):
        nonlocal ok_all
        ok_all=ok_all and bool(ok)
        results.append({'check':name,'ok':bool(ok),**({'detail':detail} if detail else {})})
        print(('PASS ' if ok else 'FAIL ')+name)
    if FIX.exists():shutil.rmtree(FIX)
    pkg=FIX/'pkg';build=FIX/'build'
    for d in (pkg/'pass-1'/'bare-original',pkg/'pass-2'/'bare-original',
              build/'pass1',build/'pass2'):
        d.mkdir(parents=True)
    prx_name='modbridge_supplier_composition.prx'
    packaged=b'FAKE-FILTERED-SELF-FIXTURE\x00'+bytes(range(256))*4
    (pkg/'pass-1'/prx_name).write_bytes(packaged)      # faked-signed twin 1
    (pkg/'pass-2'/prx_name).write_bytes(packaged)      # twin 2 (P9-identical)
    bare,_=build_bare_elf()
    (pkg/'pass-1'/'bare-original'/prx_name).write_bytes(bare)
    (pkg/'pass-2'/'bare-original'/prx_name).write_bytes(bare)
    mmap=build_map()
    (build/'pass1'/'runtime.map').write_text(mmap)
    (build/'pass2'/'runtime.map').write_text(mmap)
    for n in (1,2):
        (pkg/('pass-%d'%n)/'BUILD-RECEIPT.json').write_text(json.dumps(
            {'status':'MAC_PACKAGE_CHECKS_PASS','fixture':True}))
    variant={'key':'T','build':build,'pkg':pkg,'artifact':'runtime-T.o',
             'note':'fixture','unsafe':False}
    drv=load('l26_packaging_driver',DRIVER_P)
    gates=[];steps=[]
    def record(name,ok,detail):
        gates.append({'gate':name,'ok':bool(ok) if ok is not None else None,'detail':detail})
    info=drv.emit_symbols_sidecar(variant,record,steps,prx_name=prx_name)
    sidecar_p=pkg/(prx_name+'.symbols.json')
    check('sidecar-written-beside-prx',sidecar_p.exists(),{'path':str(sidecar_p)})
    check('gate-P12-recorded-ok',any(g['gate']=='P12-symbols-sidecar-valid' and g['ok'] for g in gates))
    check('gate-P13-recorded-ok',any(g['gate']=='P13-symbols-sidecar-prx-pairing' and g['ok'] for g in gates))
    check('emit-step-recorded',any(s.get('step')=='emit-prx-symbols-sidecar' for s in steps))
    check('prx-sha-is-packaged-twin',info['prx_sha256']==sha(pkg/'pass-1'/prx_name)==sha(pkg/'pass-2'/prx_name))
    check('build-id-extracted',info['build_id']=='aabbcc1122334455',{'build_id':info['build_id']})
    sd=json.loads(sidecar_p.read_text())
    check('required-keys-present',all(k in sd for k in drv.SIDECAR_REQUIRED_KEYS))
    check('symbols-exact-are-dynsym-anchors',
          [s['name'] for s in sd['symbols']]==['mod_start_fake (=module_start/DT_INIT)',
                                              'mod_stop_fake (=module_stop/DT_FINI)']
          and all(s['tier']=='DYNSYM-EXACT' for s in sd['symbols']))
    mr=sd['map_reconstructed']
    check('map-tier-honest-reconstructed',mr['tier']=='MAP-RECONSTRUCTED')
    exp=[('<section> .text.func_one',0x1000,0x20),('<section> .text.func_two',0x1020,0x20),
         ('<section> .text.tail',0x1040,0x10)]
    got=[(r['name'],int(r['addr'],16),int(r['size'],16)) for r in mr['symbols']]
    check('map-reconstruction-rows',got==exp,{'got':got,'expected':exp})
    check('reconstructed-rows-not-in-symbols',not any(s in sd['symbols'] for s in mr['symbols']))
    check('game-anchors-block-present',sd['sources']['game_side_anchors']['path'].endswith('game-symbols-townfall.json'))
    # ---- round-trip through the frozen consumer loader ----
    sc=load('symbolize_crash',SYMZ_P)
    syms,ci=sc.load_prx_symbols_sidecar(sidecar_p,pkg/'pass-1'/prx_name)
    check('consumer-accept-pairing-verified',ci['pairing']=='verified' and len(syms)==2,
          {'pairing':ci.get('pairing'),'symbols':[(s['name'],hex(s['addr'])) for s in syms]})
    check('consumer-addresses-int',all(isinstance(s['addr'],int) for s in syms))
    tam=HERE/'tampered.symbols.json'
    bad=json.loads(sidecar_p.read_text());bad['prx_sha256']='0'*64
    tam.write_text(json.dumps(bad))
    try:
        sc.load_prx_symbols_sidecar(tam,pkg/'pass-1'/prx_name);check('consumer-refuse-wrong-sidecar-sha',False)
    except sc.PairingRefused:check('consumer-refuse-wrong-sidecar-sha',True)
    tamprx=HERE/'tampered.prx';tamprx.write_bytes(packaged+b'X')
    try:
        sc.load_prx_symbols_sidecar(sidecar_p,tamprx);check('consumer-refuse-tampered-prx',False)
    except sc.PairingRefused:check('consumer-refuse-tampered-prx',True)
    nokey=HERE/'nokey.symbols.json'
    nk=json.loads(sidecar_p.read_text());nk.pop('prx_sha256')
    nokey.write_text(json.dumps(nk))
    try:
        sc.load_prx_symbols_sidecar(nokey,pkg/'pass-1'/prx_name);check('consumer-refuse-missing-pairing-key',False)
    except sc.PairingRefused:check('consumer-refuse-missing-pairing-key',True)
    corrupt=HERE/'corrupt.symbols.json';corrupt.write_text('{"schema": oops')
    okv,detv=drv.validate_symbols_sidecar(corrupt,[pkg/'pass-1'/prx_name])
    check('driver-validate-rejects-invalid-json',not okv and 'JSON' in detv['error'],{'detail':detv})
    okm,detm=drv.validate_symbols_sidecar(tam,[pkg/'pass-1'/prx_name])
    check('driver-validate-rejects-mismatch',not okm and 'MISMATCH' in detm['error'])
    okn,detn=drv.validate_symbols_sidecar(HERE/'nonexistent.symbols.json',[])
    check('driver-validate-rejects-absent',not okn and 'not present' in detn['error'])
    for p in (tam,tamprx,nokey,corrupt):
        p.unlink()
    receipt={'schema':1,'tool':'test_sidecar_emit.py','agent':'HH','date':'2026-10-10',
             'status':'SIDECAR_EMIT_TEST_PASS' if ok_all else 'SIDECAR_EMIT_TEST_FAIL',
             'checks':results,
             'fixture':{'package_dir':str(pkg),'build_dir':str(build),
                        'packaged_prx_sha256':sha(pkg/'pass-1'/prx_name),
                        'bare_prx_sha256':sha(pkg/'pass-1'/'bare-original'/prx_name),
                        'map_sha256':sha(build/'pass1'/'runtime.map')},
             'emitted_sidecar':info,
             'gates_recorded':gates,
             'consumer_contract':str(SYMZ_P),
             'console_contact':False,'native_execution':False,'driver_execute_mode':False}
    (HERE/'SIDECAR-EMIT-TEST-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'status':receipt['status'],'checks':len(results),'failed':[r['check'] for r in results if not r['ok']]}))
    return 0 if ok_all else 1

if __name__=='__main__':
    sys.exit(main())
