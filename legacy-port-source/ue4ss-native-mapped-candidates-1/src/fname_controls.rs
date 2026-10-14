use super::*;
fn pe()->Vec<u8>{
 let mut b=vec![0u8;5120];
 fn w16(b:&mut[u8],o:usize,v:u16){b[o..o+2].copy_from_slice(&v.to_le_bytes())}
 fn w32(b:&mut[u8],o:usize,v:u32){b[o..o+4].copy_from_slice(&v.to_le_bytes())}
 b[0..2].copy_from_slice(b"MZ");w32(&mut b,0x3c,128);b[128..132].copy_from_slice(b"PE\0\0");w16(&mut b,132,0x8664);w16(&mut b,134,2);w16(&mut b,148,240);w16(&mut b,150,0x22);
 let o=152;w16(&mut b,o,0x20b);b[o+24..o+32].copy_from_slice(&0x100000u64.to_le_bytes());w32(&mut b,o+32,4096);w32(&mut b,o+36,512);w32(&mut b,o+56,0x4000);w32(&mut b,o+60,512);w32(&mut b,o+108,16);
 for (n,name,rva,size,offset,flags) in [(0,b".text\0\0\0",0x1000,4096,512,0x60000020),(1,b".rdata\0\0",0x3000,512,4608,0x40000040)]{
  let s=392+n*40;b[s..s+8].copy_from_slice(name);w32(&mut b,s+8,size);w32(&mut b,s+12,rva);w32(&mut b,s+16,size);w32(&mut b,s+20,offset);w32(&mut b,s+36,flags);
 }
 b
}
fn put_pattern(b:&mut[u8],offset:usize,pattern:&str,target:Option<u64>){
 let mut bytes=vec![];let mut operand=None;
 for token in pattern.split_whitespace(){if token=="|"{operand=Some(bytes.len());}else{bytes.push(if token=="??"{0}else{u8::from_str_radix(token,16).unwrap()});}}
 if let Some(target)=target{let n=operand.unwrap();let address=0x101000+(offset-512) as u64+n as u64;bytes[n..n+4].copy_from_slice(&i32::try_from(target as i128-(address+4)as i128).unwrap().to_le_bytes());}
 b[offset..offset+bytes.len()].copy_from_slice(&bytes);
}
fn call(b:&[u8],ctor:bool)->(i32,u64){let mut out=99;let status=unsafe{if ctor{ps_fname_ctor_wchar_file_image(b.as_ptr(),b.len(),&mut out)}else{ps_fname_to_string_file_image(b.as_ptr(),b.len(),&mut out)}};(status,out)}
const CTOR1:&str="48 89 5C 24 08 57 48 83 EC 30 48 8B D9 41 8B F8 33 C9 4C 8B DA 44 8B D1 4C 8B CA 48 85 D2 74 ?? 0F B7 02 66 85 C0";
const CTOR2:&str="48 89 5C 24 08 57 48 83 EC 30 48 8B D9 48 89 54 24 20 33 C9 41 8B F8 4C 8B D2 44 8B C9 48 85 D2 74 ?? 0F B7 02 66 85 C0";
const INLINE:&str="EB 07 48 8D 15 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 41 B8 01 00 00 00 E8 | ?? ?? ?? ??";
const STRING:&str="56 57 48 83 EC 28 48 89 D6 48 89 CF 83 79 ?? 00 74";
const VOID:[&str;3]=["E8 | ?? ?? ?? ?? ?? 01 00 00 00 ?? 39 ?? 48 0F 8E","E8 | ?? ?? ?? ?? BD 01 00 00 00 41 39 6E ?? 0F 8E","E8 | ?? ?? ?? ?? 48 8B 4C 24 ?? 8B FD 48 85 C9"];
const FSTRING:&str="48 8b 48 ?? 48 89 4c 24 ?? 48 8d 4c 24 ?? e8 | ?? ?? ?? ?? 83 7c 24 ?? 00 48 8d";
#[test]fn elf_real_cfi_success(){let b=include_bytes!("../fixtures/elf-success.elf");assert_eq!(call(b,true),(0,4142));assert_eq!(call(b,false),(0,4164));let image=OwnedImage::parse(b).unwrap();assert_eq!(image.get_root_function(4100).unwrap().unwrap().range,4096..4142);}
#[test]fn elf_ambiguities(){assert_eq!(call(include_bytes!("../fixtures/elf-ctor-ambiguous.elf"),true),(4,0));assert_eq!(call(include_bytes!("../fixtures/elf-string-ambiguous.elf"),false),(4,0));}
#[test]fn elf_unavailable_metadata_is_not_a_root(){let mut b=include_bytes!("../fixtures/elf-success.elf").to_vec();let o=b.windows(10).position(|w|w==b".eh_frame\0").unwrap();b[o]=b'x';let o=b.windows(14).position(|w|w==b".eh_frame_hdr\0").unwrap();b[o]=b'x';let phoff=u64::from_le_bytes(b[32..40].try_into().unwrap())as usize;let count=u16::from_le_bytes(b[56..58].try_into().unwrap())as usize;for i in 0..count{let p=phoff+i*56;if u32::from_le_bytes(b[p..p+4].try_into().unwrap())==object::elf::PT_GNU_EH_FRAME{b[p..p+4].copy_from_slice(&0u32.to_le_bytes());}}assert!(OwnedImage::parse(&b).unwrap().get_root_function(4100).is_err());assert_eq!(call(&b,true),(5,0));assert_eq!(call(&b,false),(0,4164));}
#[test]fn pe_both_constructor_prologues(){for p in [CTOR1,CTOR2]{let mut b=pe();put_pattern(&mut b,512+100,p,None);assert_eq!(call(&b,true),(0,0x101064));assert!(OwnedImage::parse(&b).unwrap().get_root_function(0x101064).is_err());}}
#[test]fn pe_constructor_inline_and_priority(){let mut b=pe();put_pattern(&mut b,512+100,INLINE,Some(0x101700));assert_eq!(call(&b,true),(0,0x101700));put_pattern(&mut b,512+200,CTOR1,None);assert_eq!(call(&b,true),(0,0x1010c8));put_pattern(&mut b,512+300,CTOR2,None);assert_eq!(call(&b,true),(4,0));}
#[test]fn pe_string_direct_and_ambiguity(){let mut b=pe();put_pattern(&mut b,512+100,STRING,None);assert_eq!(call(&b,false),(0,0x101064));put_pattern(&mut b,512+200,STRING,None);assert_eq!(call(&b,false),(4,0));}
#[test]fn pe_void_patterns_and_fstring_priority(){for p in VOID{let mut b=pe();put_pattern(&mut b,512+100,p,Some(0x101700));assert_eq!(call(&b,false),(0,0x101700));put_pattern(&mut b,512+200,FSTRING,Some(0x101800));assert_eq!(call(&b,false),(0,0x101800));put_pattern(&mut b,512+300,STRING,None);assert_eq!(call(&b,false),(0,0x10112c));}}
#[test]fn checked_c_abi_and_no_match(){let mut out=123u64;assert_eq!(unsafe{ps_fname_ctor_wchar_file_image(core::ptr::null(),1,&mut out)},1);assert_eq!(out,123);assert_eq!(call(&[1,2,3],true),(2,0));assert_eq!(call(&pe(),true),(3,0));assert_eq!(call(&pe(),false),(3,0));}
#[test]fn patterns_cannot_cross_file_section_tail(){let mut b=pe();put_pattern(&mut b,512+4096-5,"48 89 5C 24 08",None);put_pattern(&mut b,4608,"57 48 83 EC 30",None);assert_eq!(call(&b,true),(3,0));}
fn lea_string(b:&mut[u8],offset:usize,string:u64){b[offset..offset+3].copy_from_slice(&[0x48,0x8d,0x15]);let next=0x101000+(offset-512)as u64+7;b[offset+3..offset+7].copy_from_slice(&i32::try_from(string as i128-next as i128).unwrap().to_le_bytes());}
fn module_string(b:&mut[u8]){let s=util::utf16("MovementComponent0\0");b[4608..4608+s.len()].copy_from_slice(&s);}
#[test]fn pe_ctor_string_direct_and_tailcall(){for tail in [false,true]{let mut b=pe();module_string(&mut b);let start=512+100;if tail{put_pattern(&mut b,start,"41 b8 01 00 00 00",None);}let lea=start+if tail{6}else{0};lea_string(&mut b,lea,0x103000);put_pattern(&mut b,lea+7,if tail{"48 8d 0d ?? ?? ?? ?? e9 | ?? ?? ?? ??"}else{"48 8d 0d ?? ?? ?? ?? e8 | ?? ?? ?? ??"},Some(0x101700));assert_eq!(call(&b,true),(0,0x101700));}}
#[test]fn pe_first_call_retains_upstream_decoder_field(){let mut b=pe();module_string(&mut b);lea_string(&mut b,512+100,0x103000);put_pattern(&mut b,512+107,"4c 8d 05 ?? ?? ?? ?? 41 b1 01 e8 | ?? ?? ?? ??",Some(0x101700));put_pattern(&mut b,512+0x700,"e8 | ?? ?? ?? ??",Some(0x101800));/* Preserve the genuine source decoder field. This fixture verifies the actual iced-x86 result for the CALL rather than substituting another field. */assert_eq!(call(&b,true),(0,0x101800));b[512+0x700]=0xc3;assert_eq!(call(&b,true),(5,0));}
#[test]fn pe_string_root_metadata_unavailable_refuses(){let mut b=pe();let s=util::utf16("  DrivingBone: %s\nDrivenParamet");b[4608..4608+s.len()].copy_from_slice(&s);lea_string(&mut b,512+20,0x103000);assert_eq!(call(&b,false),(5,0));}
#[test]fn pe_string_last_call_uses_real_compiler_root(){
 let mut b=include_bytes!("../fixtures/fname-pe.exe").to_vec();
 // Retain real compiler .pdata/.xdata bytes and exact function ranges; patch
 // caller-owned serialized code/data only. This file is never mapped/called.
 let file=object::File::parse(&*b).unwrap();let text=file.section_by_name(".text").unwrap();let (text_off,_)=text.file_range().unwrap();let text_addr=text.address();let data=file.section_by_name(".data").unwrap();let (data_off,_)=data.file_range().unwrap();let string_addr=data.address();
 let roots=OwnedImage::parse(&b).unwrap().get_root_functions().unwrap();let first=&roots[0];let second=&roots[1];let first_offset=text_off as usize+(first.start-text_addr)as usize;
 b[first_offset..first_offset+39].fill(0x90);
 b[first_offset]=0xe8;b[first_offset+1..first_offset+5].copy_from_slice(&i32::try_from(second.start as i128-(first.start+5)as i128).unwrap().to_le_bytes());
 b[first_offset+10]=0xe8;b[first_offset+11..first_offset+15].copy_from_slice(&i32::try_from(first.start as i128-(first.start+15)as i128).unwrap().to_le_bytes());
 let reference=first_offset+20;b[reference..reference+3].copy_from_slice(&[0x48,0x8d,0x15]);b[reference+3..reference+7].copy_from_slice(&i32::try_from(string_addr as i128-(first.start+27)as i128).unwrap().to_le_bytes());
 let s=util::utf16("  DrivingBone: %s\nDrivenParamet");let off=data_off as usize;b[off..off+s.len()].copy_from_slice(&s);
 assert_eq!(OwnedImage::parse(&b).unwrap().get_root_function(first.start+20).unwrap().unwrap().range,*first);
 assert_eq!(call(&b,false),(0,first.start));
}
