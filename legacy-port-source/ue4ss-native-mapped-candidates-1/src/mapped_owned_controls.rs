use std::{vec,vec::Vec,string::String,format,println};
// Test-only owned loader. Materializes known compiler fixtures, never executes
// their bytes. No process discovery, game data, service registration or syscalls.
extern crate std;
use crate::{mapped_image::{MappedImage,MappedSegment,MappedFormat,MappedExtent},mapped_api::{ps_resolve_mapped_core_v1,PsMappedCoreResult},ps_scan_abi::{PsMappedImageViewV1,PsMappedSegment,PsMappedExtent},MemoryTrait};
use object::{Object,ObjectSection,ObjectSymbol};
use std::{fs,collections::BTreeMap};
fn u16le(b:&[u8],n:usize)->usize{u16::from_le_bytes(b[n..n+2].try_into().unwrap()) as usize}
fn u32le(b:&[u8],n:usize)->usize{u32::from_le_bytes(b[n..n+4].try_into().unwrap()) as usize}
fn u64le(b:&[u8],n:usize)->usize{usize::try_from(u64::from_le_bytes(b[n..n+8].try_into().unwrap())).unwrap()}
struct Fixture{bytes:Vec<u8>,spans:Vec<(usize,usize,u32)>,text:usize,frame:Option<(usize,usize)>,header:Option<(usize,usize)>,exceptions:Option<(usize,usize)>,format:u32,symbols:BTreeMap<String,usize>}
impl Fixture{
 fn load(path:&str)->Self{
  let b=fs::read(path).unwrap();let file=object::File::parse(&*b).unwrap();let mut spans=vec![];
  let (mut bytes,text,frame,header,exceptions,format)=if b.starts_with(b"\x7fELF"){
   assert_eq!(b[4],2);assert_eq!(b[5],1);let phoff=u64le(&b,32);let phsize=u16le(&b,54);let phcount=u16le(&b,56);
   let mut loads=vec![];
   for i in 0..phcount{let o=phoff+i*phsize;if u32le(&b,o)==1{let flags=u32le(&b,o+4);let off=u64le(&b,o+8);let va=u64le(&b,o+16);let fs=u64le(&b,o+32);let ms=u64le(&b,o+40);assert!(fs<=ms);assert!(off.checked_add(fs).unwrap()<=b.len());let perm=if flags&4!=0{1}else{0}|if flags&2!=0{2}else{0}|if flags&1!=0{4}else{0};loads.push((va,ms,off,fs,perm));}}
   assert_eq!(loads.iter().map(|x|x.0).min(),Some(0));let n=loads.iter().map(|x|x.0.checked_add(x.1).unwrap()).max().unwrap();assert!(n<1048576);
   let mut data=vec![0;n];for (va,ms,off,fs,p) in loads{data[va..va+fs].copy_from_slice(&b[off..off+fs]);spans.push((va,ms,p));}
   let sect=|name|file.section_by_name(name).map(|s|(s.address() as usize,s.size() as usize));
   (data,sect(".text").unwrap().0,sect(".eh_frame"),sect(".eh_frame_hdr"),None,1)
  }else{
   let pe=u32le(&b,60);assert_eq!(&b[pe..pe+4],b"PE\0\0");let count=u16le(&b,pe+6);let opt=pe+24;assert_eq!(u16le(&b,opt),0x20b);
   let n=u32le(&b,opt+56);let hs=u32le(&b,opt+60);assert!(n<1048576);let mut data=vec![0;n];data[..hs].copy_from_slice(&b[..hs]);spans.push((0,hs,1));
   let ss=opt+u16le(&b,pe+20);let mut text=None;
   for i in 0..count{let o=ss+i*40;let va=u32le(&b,o+12);let vs=u32le(&b,o+8);let rs=u32le(&b,o+16);let ro=u32le(&b,o+20);let c=u32le(&b,o+36);let size=vs.max(rs);assert!(va+size<=data.len());assert!(ro+rs<=b.len());data[va..va+rs].copy_from_slice(&b[ro..ro+rs]);let p=if c&0x40000000!=0{1}else{0}|if c&0x80000000!=0{2}else{0}|if c&0x20000000!=0{4}else{0};if p&4!=0{text=Some(va);}spans.push((va,size,p));}
   let ex=opt+112+3*8;let r=u32le(&b,ex);let z=u32le(&b,ex+4);
   (data,text.unwrap(),None,None,Some((r,z)),2)
  };
  // Fixture PIC encodings require no relocations. Byte storage is zero-initialized
  // including genuine BSS; this caller owns materialization responsibility.
  bytes.shrink_to_fit();let base=file.relative_address_base();let symbols=file.symbols().filter_map(|s|Some((s.name().ok()?.to_owned(),usize::try_from(s.address().checked_sub(base)?).ok()?))).collect();
  Self{bytes,spans,text,frame,header,exceptions,format,symbols}
 }
 fn segments(&self)->Vec<PsMappedSegment>{self.spans.iter().map(|&(o,n,p)|PsMappedSegment{bytes:self.bytes[o..o+n].as_ptr(),size:n,runtime_address:self.addr(o),permissions:p,reserved:0}).collect()}
 fn addr(&self,rva:usize)->u64{self.bytes.as_ptr() as usize as u64+rva as u64}
 fn extent(&self,e:Option<(usize,usize)>)->PsMappedExtent{match e{Some((r,n))=>PsMappedExtent{address:self.addr(r),size:n},None=>PsMappedExtent{address:0,size:0}}}
 fn view(&self,seg:&[PsMappedSegment])->PsMappedImageViewV1{PsMappedImageViewV1{abi_version:1,format:self.format,segments:seg.as_ptr(),segment_count:seg.len(),text_base:self.addr(self.text),image_base:if self.format==2{self.addr(0)}else{0},eh_frame:self.extent(self.frame),eh_frame_hdr:self.extent(self.header),pe_exception_directory:self.extent(self.exceptions)}}
 fn safe<'a>(&'a self)->MappedImage<'a>{let s=self.spans.iter().map(|&(o,n,p)|MappedSegment{bytes:&self.bytes[o..o+n],runtime_address:self.addr(o),permissions:p}).collect();let e=|v:Option<(usize,usize)>|v.map(|(r,n)|MappedExtent{address:self.addr(r),size:n});let format=if self.format==1{MappedFormat::Elf64{text_base:self.addr(self.text),eh_frame:e(self.frame),eh_frame_hdr:e(self.header)}}else{MappedFormat::Pe64{text_base:self.addr(self.text),image_base:self.addr(0),exception_directory:e(self.exceptions)}};MappedImage::new(format,s).unwrap()}
}
static CALLS:std::sync::atomic::AtomicUsize=std::sync::atomic::AtomicUsize::new(0);
fn call(view:*const PsMappedImageViewV1,id:u32)->(i32,PsMappedCoreResult){CALLS.fetch_add(1,std::sync::atomic::Ordering::Relaxed);let mut out=PsMappedCoreResult{address:123,major:999,minor:999,reserved:999};let status=unsafe{ps_resolve_mapped_core_v1(view,id,&mut out)};if status!=0{assert_eq!((out.address,out.major,out.minor,out.reserved),(0,0,0,0));}else{assert_eq!(out.reserved,0);}(status,out)}
#[test]
fn caller_owned_mapped_elf_pe(){
 let f=Fixture::load("owned-fixtures/mapped.elf");let mut seg=f.segments();let mut view=f.view(&seg);let mut rows=vec![];
 for id in 1..=7{let (status,r)=call(&view,id);println!("ELF id={id} status={status} address={:#x} version={}.{}",r.address,r.major,r.minor);rows.push(format!("{{\"id\":{id},\"status\":{status},\"actual_address\":{},\"major\":{},\"minor\":{}}}",r.address,r.major,r.minor));match id{1=>assert_eq!((status,r.major,r.minor),(0,5,7)),4=>assert_eq!(status,3),_=>{assert_eq!(status,0);let label=match id{2=>"owned_object_slot",3=>"owned_fname_ctor",5=>"owned_allocator_slot",6=>"owned_construct",7=>"owned_tick",_=>unreachable!()};assert_eq!(r.address,f.addr(f.symbols[label]));assert!(seg.iter().any(|s|s.runtime_address<=r.address&&r.address<s.runtime_address+s.size as u64));}}}
 let image=f.safe();assert_eq!(image.image().function_records().unwrap().len(),4);let tick=f.addr(f.symbols["owned_tick"]);assert_eq!(image.image().get_root_function(tick+2).unwrap().unwrap().range.start,tick);let first=seg[0].runtime_address;assert!(image.image().memory.range(first..seg[1].runtime_address+1).is_err());assert_eq!(image.permissions(tick,1),Some(5));
 // Absent metadata is not a fabricated root. Independent pattern core remains.
 view.eh_frame=PsMappedExtent{address:0,size:0};view.eh_frame_hdr=PsMappedExtent{address:0,size:0};assert_eq!(call(&view,7).0,3);assert_eq!(call(&view,3).0,3);assert_eq!(call(&view,2).0,0);
 // Real malformed in-span metadata differs from unsupported descriptor extents.
 view=f.view(&seg);view.eh_frame.size=1;assert_eq!(call(&view,7).0,5);
 view=f.view(&seg);view.eh_frame_hdr.size=1;assert_eq!(call(&view,7).0,5);
 view=f.view(&seg);view.eh_frame.address=f.addr(f.bytes.len());assert_eq!(call(&view,7).0,2);
 view=f.view(&seg);view.eh_frame.address=u64::MAX;assert_eq!(call(&view,7).0,2);
 view=f.view(&seg);view.eh_frame.size=0;assert_eq!(call(&view,7).0,2);
 view=f.view(&seg);view.abi_version=2;assert_eq!(call(&view,1).0,2);view=f.view(&seg);view.format=3;assert_eq!(call(&view,1).0,2);
 view=f.view(&seg);view.image_base=first;assert_eq!(call(&view,1).0,2);
 view=f.view(&seg);view.pe_exception_directory=PsMappedExtent{address:first,size:1};assert_eq!(call(&view,1).0,2);
 view=f.view(&seg);view.text_base=first;assert_eq!(call(&view,1).0,2);
 view=f.view(&seg);view.segment_count=0;assert_eq!(call(&view,1).0,2);view.segment_count=usize::MAX;assert_eq!(call(&view,1).0,2);
 view=f.view(&seg);view.segments=std::ptr::null();assert_eq!(call(&view,1).0,2);view.segments=(seg.as_ptr() as *const u8).wrapping_add(1).cast();assert_eq!(call(&view,1).0,2);
 for mode in 0..6{let mut bad=f.segments();match mode{0=>bad[0].reserved=1,1=>bad[0].permissions=8,2=>bad[0].permissions=2,3=>bad[0].runtime_address+=1,4=>bad[0].size=0,5=>bad[0].bytes=std::ptr::null(),_=>unreachable!()};assert_eq!(call(&f.view(&bad),1).0,2);}
 seg.push(PsMappedSegment{bytes:seg[0].bytes,size:1,runtime_address:seg[0].runtime_address,permissions:1,reserved:0});assert_eq!(call(&f.view(&seg),1).0,2);seg.pop();
 view=f.view(&seg);assert_eq!(call(&view,0).0,1);assert_eq!(call(&view,8).0,1);assert_eq!(call(std::ptr::null(),1).0,1);assert_eq!(call((&view as *const PsMappedImageViewV1 as *const u8).wrapping_add(1).cast(),1).0,1);assert_eq!(unsafe{ps_resolve_mapped_core_v1(&view,1,std::ptr::null_mut())},1);
 let mut aligned_output=[0u64;3];assert_eq!(unsafe{ps_resolve_mapped_core_v1(&view,1,(aligned_output.as_mut_ptr() as *mut u8).add(1).cast())},1);
 let a=Fixture::load("owned-fixtures/ambiguous.elf");let aseg=a.segments();assert_eq!(call(&a.view(&aseg),2).0,4);
 // Genuine PE strings + real .pdata: recompute RVA at actual owned image base.
 let p=Fixture::load("owned-fixtures/pe-strings.exe");let ps=p.segments();let pv=p.view(&ps);let pi=p.safe();assert_eq!(pi.image().function_records().unwrap().len(),7);
 let mut pe_rows=vec![];for id in 1..=7{let (status,r)=call(&pv,id);println!("PE id={id} status={status} address={:#x}",r.address);pe_rows.push(format!("{{\"id\":{id},\"status\":{status},\"actual_address\":{}}}",r.address));if id==5{assert_eq!(status,0);assert_eq!(r.address,p.addr(0x3000));}else if id==6{assert_eq!(status,0);assert_eq!(r.address,p.addr(0x1080));}else{assert_eq!(status,3);}}
 fs::write("owned-mapped-controls.json",format!("{{\"scope\":\"compiler-owned materialized ELF/PE bytes only; no execution or game input\",\"elf_fde_count\":4,\"pe_pdata_count\":7,\"elf_results\":[{}],\"pe_results\":[{}],\"c_abi_calls\":{},\"invalid_output_refusals\":2}}\n",rows.join(","),pe_rows.join(","),CALLS.load(std::sync::atomic::Ordering::Relaxed))).unwrap();
 println!("owned mapped fixtures and boundary controls passed");
}

fn split_at(f:&Fixture, spans:&[PsMappedSegment], cut:usize)->Vec<PsMappedSegment>{
 let address=f.addr(cut);let mut result=vec![];let mut changed=false;
 for s in spans{
  let end=s.runtime_address+s.size as u64;
  if s.runtime_address<address&&address<end{
   let left=(address-s.runtime_address)as usize;
   result.push(PsMappedSegment{bytes:s.bytes,size:left,runtime_address:s.runtime_address,permissions:s.permissions,reserved:0});
   result.push(PsMappedSegment{bytes:unsafe{s.bytes.add(left)},size:s.size-left,runtime_address:address,permissions:s.permissions^2,reserved:0});changed=true;
  }else{result.push(PsMappedSegment{bytes:s.bytes,size:s.size,runtime_address:s.runtime_address,permissions:s.permissions,reserved:s.reserved});}
 }
 assert!(changed);result
}
fn safe_split<'a>(f:&'a Fixture, spans:&[PsMappedSegment])->MappedImage<'a>{
 let segments=spans.iter().map(|s|{let start=(s.runtime_address-f.addr(0))as usize;MappedSegment{bytes:&f.bytes[start..start+s.size],runtime_address:s.runtime_address,permissions:s.permissions}}).collect();
 let e=|v:Option<(usize,usize)>|v.map(|(r,n)|MappedExtent{address:f.addr(r),size:n});
 let format=if f.format==1{MappedFormat::Elf64{text_base:f.addr(f.text),eh_frame:e(f.frame),eh_frame_hdr:e(f.header)}}else{MappedFormat::Pe64{text_base:f.addr(f.text),image_base:f.addr(0),exception_directory:e(f.exceptions)}};
 MappedImage::new(format,segments).unwrap()
}
fn parity(f:&Fixture,split:&[PsMappedSegment]){
 let baseline=f.segments();let base=f.safe();let fragmented=safe_split(f,split);
 assert_eq!(fragmented.image().function_records().unwrap(),base.image().function_records().unwrap());
 for id in 1..=7{
  let (a,av)=call(&f.view(&baseline),id);let (b,bv)=call(&f.view(split),id);
  assert_eq!((a,av.address,av.major,av.minor,av.reserved),(b,bv.address,bv.major,bv.minor,bv.reserved));
 }
}
#[test]
fn fragmented_metadata_parity(){
 let elf=Fixture::load("owned-fixtures/mapped.elf");let raw=elf.segments();
 let (frame,size)=elf.frame.unwrap();let cut=frame+size/2;
 let split=split_at(&elf,&raw,cut);parity(&elf,&split);
 let (header,header_size)=elf.header.unwrap();assert!(header_size>5);
 let both=split_at(&elf,&split,header+5);parity(&elf,&both);
 let mut gap=split_at(&elf,&raw,cut);
 let right=gap.iter_mut().find(|s|s.runtime_address==elf.addr(cut)).unwrap();
 right.bytes=unsafe{right.bytes.add(1)};right.runtime_address+=1;right.size-=1;
 assert_eq!(call(&elf.view(&gap),7).0,2);
 let pe=Fixture::load("owned-fixtures/pe-strings.exe");let (table,n)=pe.exceptions.unwrap();assert!(n>6);
 let split=split_at(&pe,&pe.segments(),table+6);parity(&pe,&split);
 let original=pe.safe();let unwind=original.image().function_records().unwrap()[0].unwind;
 let unwind_offset=(unwind-pe.addr(0))as usize;
 let both=split_at(&pe,&split,unwind_offset+2);parity(&pe,&both);
 println!("fragmented ELF frame/header and PE table/unwind preserve exact roots+all7statuses; gap refused");
}

#[test]
fn fragmented_header_only_elf_metadata(){
 let f=Fixture::load("owned-fixtures/mapped.elf");let (frame,size)=f.frame.unwrap();
 let raw=f.segments();let split=split_at(&f,&raw,frame+size/2);
 let construct=|spans:&[PsMappedSegment]|{
  let segments=spans.iter().map(|s|{let start=(s.runtime_address-f.addr(0))as usize;MappedSegment{bytes:&f.bytes[start..start+s.size],runtime_address:s.runtime_address,permissions:s.permissions}}).collect();
  let (header,n)=f.header.unwrap();
  MappedImage::new(MappedFormat::Elf64{text_base:f.addr(f.text),eh_frame:None,eh_frame_hdr:Some(MappedExtent{address:f.addr(header),size:n})},segments).unwrap()
 };
 let baseline=construct(&raw);let fragmented=construct(&split);
 assert_eq!(fragmented.image().function_records().unwrap(),baseline.image().function_records().unwrap());
 let mut av=f.view(&raw);av.eh_frame=PsMappedExtent{address:0,size:0};
 let mut bv=f.view(&split);bv.eh_frame=PsMappedExtent{address:0,size:0};
 for id in 1..=7{let(a,x)=call(&av,id);let(b,y)=call(&bv,id);assert_eq!((a,x.address,x.major,x.minor),(b,y.address,y.major,y.minor));}
 println!("ELF header-only fallback preserves exact roots/all7statuses across actual supplied permission boundary");
}
