#!/usr/bin/env python3
"""Audit pinned public source bytes only. No kernel calls, clients, target binaries or extraction."""
import argparse,hashlib,io,json,os,zipfile
from pathlib import Path
HOME=Path(__file__).resolve().parent
ARCHIVE_SHA='b4fcf169a12aa184c812f4243c5f0cca40f1ad932d9a14530bbc667aef47ddf6'
PREFIX='kstuff-lite-1.11/'
MEMBERS={PREFIX+k:v for k,v in {
 'ps5-kstuff/uelf/kekcall.c':'dd43efbb499964483cba543558fff084e96b082d74d7c18e49b41641d2ed421b',
 'ps5-kstuff/uelf/kekcall.h':'41e7dc87c4c53cbe40331122e58651759326114313872f8d3d6f6752e4efad6a',
 'ps5-kstuff/uelf/main.c':'7aff330b1d55433e7ff121cde090e11e375843263e773b4672cc31017bcf5692',
 'ps5-kstuff/uelf/utils.h':'8ba30e5181aa4009b1ba97fcfc188d5b58ebd133a82f33ebb903be2060981b26',
 'ps5-kstuff/uelf/syscall_fixes.c':'6d2dad6d72d06130eb3234a76a8fd90939e689663be239e38cd65f280cb4c73b',
 'prosper0gdb/r0gdb.h':'09dab6be010d66068c7b92d81ecefd063fe61ef81108ac6a459a9eafec96eefa',
 '.gitmodules':'0bb1e445f625bd81b457fe96e804c2953ab2844f04c468ad67a0544697c5fb93'}.items()}
ARCHIVE_CAP=16*1024*1024;MEMBER_CAP=64*1024;OUTPUT_CAP=128*1024
LICENSE_NAMES={'license','license.txt','license.md','copying','copying.txt','unlicense','copyright','notice'}
def need(ok,why):
 if not ok:raise ValueError(why)
def sha(b):return hashlib.sha256(b).hexdigest()
def inspect_zip(blob,archive_pin,member_pins):
 need(0<len(blob)<=ARCHIVE_CAP,'archive size bound');need(sha(blob)==archive_pin,'archive SHA mismatch')
 need(type(member_pins) is dict and 0<len(member_pins)<=32,'nonzero bounded source pin set required')
 with zipfile.ZipFile(io.BytesIO(blob)) as z:
  infos=z.infolist();names=[x.filename for x in infos]
  need(len(names)==len(set(names)),'duplicate archive member name');need(0<len(infos)<=4096,'archive entry count bound')
  refs=[]
  for name,pin in member_pins.items():
   need(name in names,'missing pinned member');info=z.getinfo(name);need(not info.is_dir() and 0<info.file_size<=MEMBER_CAP,'source member size bound')
   b=z.read(name);need(len(b)==info.file_size and sha(b)==pin,'member SHA mismatch')
   try:b.decode('utf-8')
   except UnicodeDecodeError:raise ValueError('pinned member is not UTF8 source text')
   refs.append({'member':name,'bytes':len(b),'sha256':pin,'text_only':True})
  license_candidates=[n for n in names if n.rsplit('/',1)[-1].lower() in LICENSE_NAMES]
 return {'label':'SOURCE_BYTES_BOUND_ONLY','archive_sha256':sha(blob),'archive_bytes':len(blob),'archive_members':len(infos),'files_checked':len(refs),'members':refs,'license_candidates':license_candidates,'license_status':'PRESENT_UNREVIEWED' if license_candidates else 'NOT_FOUND_IN_ARCHIVE','redistribution_permission_established':False,'kernel_provider_admitted':False,'limits':'Hash-bound source snapshot only. No source↔binary, current resident provider, service semantics, target behavior or redistribution permission admission.'}
def stable_read(path,cap,pin):
 p=Path(path).resolve();ident=lambda s:[s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns]
 before=p.stat();need(p.is_file() and 0<before.st_size<=cap,'input size bound')
 with p.open('rb') as f:
  need(ident(os.fstat(f.fileno()))==ident(before),'input replaced before read');b=f.read(cap+1);end=os.fstat(f.fileno())
 after=p.stat();need(len(b)==before.st_size<=cap and ident(before)==ident(end)==ident(after),'input changed during read');need(sha(b)==pin,'input SHA mismatch')
 return b,{'path':str(p),'bytes':len(b),'sha256':sha(b),'identity':ident(after)}
def audit(path):
 b,ref=stable_read(path,ARCHIVE_CAP,ARCHIVE_SHA);out=inspect_zip(b,ARCHIVE_SHA,MEMBERS)
 again,after=stable_read(path,ARCHIVE_CAP,ARCHIVE_SHA);need(again==b and after==ref,'source archive changed after audit')
 out.update({'schema':1,'result':'BLOCKED_NO_ADMITTED_KERNEL_PROVIDER','sources':[ref],'source_unchanged':True,'source_before_after_sha256':[ref['sha256'],after['sha256']],
 'kernel_capability_1_complete':False,'kernel_capability_3_complete':False,'client_staged':False,'kernel_operations':0,'target_binary_reads':0,
 'contract_review':{
  'dispatcher':{'member':PREFIX+'ps5-kstuff/uelf/kekcall.c','lines':[20,108],'conclusion':'No generic client kernel-VA copy/read/write operation in this pinned dispatcher.'},
  'internal_helpers':{'member':PREFIX+'ps5-kstuff/uelf/utils.h','lines':[7,29],'conclusion':'Kernel-copy helpers depend on privileged internal context; declarations are not a callable userspace service.'},
  'debug_syscall_route':{'member':PREFIX+'ps5-kstuff/uelf/main.c','lines':[85,91],'conclusion':'Routes through syscall fixup; no bounded debugging request structure, read/write target admission or partial-I/O contract established.'},
  'legacy_debug_declaration':{'member':PREFIX+'prosper0gdb/r0gdb.h','lines':[74,75],'conclusion':'Opaque pointer prototype alone cannot define a client operation ABI or restoration semantics.'}},
 'missing_prerequisites':['Independent binding between these source members and exact resident provider binary/configuration','Existing callable debugging provider with documented bounded owned-target read/write request/response and partial-I/O behavior','Target ownership/lifetime/revocation and independently known expected bytes','Readback/restoration verification, recovery evidence and3 separate restart passes'],
 'scope_limit':'Source/contract audit only; no fake callback API, mmap substitute, target syscall shim, added privileged gate, kernel-resident code or kernel-security changes.'})
 return out
def write_output(result,path):
 p=Path(path).resolve();need(p.is_relative_to(HOME) and not p.exists(),'new output inside cap01-03 required')
 need(p not in {Path(r['path']).resolve() for r in result.get('sources',[])},'input clobber refused')
 raw=json.dumps(result,indent=2)+'\n';need(len(raw.encode())<=OUTPUT_CAP,'output size bound')
 with p.open('x') as f:f.write(raw)
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--archive',type=Path,required=True);a.add_argument('--out',type=Path,required=True);args=a.parse_args()
 out=audit(args.archive);write_output(out,args.out);print(json.dumps({k:out[k] for k in ['result','files_checked','license_status','kernel_capability_1_complete','kernel_capability_3_complete']}))
if __name__=='__main__':main()
