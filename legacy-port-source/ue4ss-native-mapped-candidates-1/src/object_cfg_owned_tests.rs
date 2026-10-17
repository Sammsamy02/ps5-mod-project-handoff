use super::*;
use alloc::{vec,vec::Vec};
fn put64(b:&mut[u8],off:usize,n:u64){b[off..off+8].copy_from_slice(&n.to_le_bytes());}
fn put32(b:&mut[u8],off:usize,n:u32){b[off..off+4].copy_from_slice(&n.to_le_bytes());}
fn put16(b:&mut[u8],off:usize,n:u16){b[off..off+2].copy_from_slice(&n.to_le_bytes());}
fn relative(code:&mut Vec<u8>,base:u64,target:u64,prefix:&[u8]){code.extend_from_slice(prefix);let next=base+code.len() as u64+4;code.extend_from_slice(&i32::try_from(target as i128-next as i128).unwrap().to_le_bytes());}
fn text(b:&mut[u8],off:usize,s:&str){b[off..off+s.len()].copy_from_slice(s.as_bytes());b[off+s.len()]=0;}
fn ph(b:&mut[u8],i:usize,flags:u32,off:u64,va:u64,size:u64,mem:u64){let at=64+i*56;put32(b,at,1);put32(b,at+4,flags);put64(b,at+8,off);put64(b,at+16,va);put64(b,at+32,size);put64(b,at+40,mem);put64(b,at+48,1);}
fn caller(base:u64,global:u64,callee:u64,clobber:bool)->Vec<u8>{let mut c=vec![];relative(&mut c,base,global,&[0x48,0x8d,0x3d]);if clobber{c.extend_from_slice(&[0x31,0xff]);}relative(&mut c,base,callee,&[0xe8]);c.push(0xc3);c}
fn fixture(global:u64,ambiguous:bool,wrong_stride:bool,clobber:bool,missing_free:bool,incomplete:bool)->Vec<u8>{
 let mut b=vec![0u8;0x2000];b[..7].copy_from_slice(b"\x7fELF\x02\x01\x01");put16(&mut b,16,3);put16(&mut b,18,62);put32(&mut b,20,1);put64(&mut b,32,64);put16(&mut b,52,64);put16(&mut b,54,56);put16(&mut b,56,3);
 ph(&mut b,0,5,0x200,0x410000,0x700,0x700);ph(&mut b,1,4,0x1000,0x710000,0x800,0x800);ph(&mut b,2,6,0x1800,0xb20000,0,0x10000);
 b[0x200..0x900].fill(0xcc);text(&mut b,0x1000,object_cfg::ALLOC_DIAGNOSTIC);if !missing_free{text(&mut b,0x1200,object_cfg::FREE_DIAGNOSTIC);}
 // Owned machine code uses varied labels, a separate writable zero-fill span,
 // explicit called entries, typed object-index/stride instructions and diagnostics.
 let allocation=0x410100;let free=0x410300;let mut a=vec![0x8b,0x46,0x0c,0xc1,0xe8,0x10,0x48,0x8d,0x04,0x40,0xc1,0xe0,if wrong_stride{2}else{3},0x89,0x46,0x0c];relative(&mut a,allocation,0x710000,&[0x48,0x8d,0x15]);a.push(0xc3);
 let mut f=vec![0x8b,0x46,0x0c,0xc1,0xe8,0x10,0x48,0x8d,0x04,0x40,0xc1,0xe0,0x03,0xc7,0x46,0x0c,0xff,0xff,0xff,0xff];relative(&mut f,free,0x710200,&[0x48,0x8d,0x15]);if incomplete{f.extend_from_slice(&[0xff,0xe0]);}else{f.push(0xc3);}
 b[0x300..0x300+a.len()].copy_from_slice(&a);b[0x500..0x500+f.len()].copy_from_slice(&f);
 let call_a=caller(0x410020,global,allocation,clobber);b[0x220..0x220+call_a.len()].copy_from_slice(&call_a);let call_f=caller(0x410420,global,free,false);b[0x620..0x620+call_f.len()].copy_from_slice(&call_f);
 if ambiguous{let a2=caller(0x410500,global+0x80,allocation,false);b[0x700..0x700+a2.len()].copy_from_slice(&a2);let f2=caller(0x410580,global+0x80,free,false);b[0x780..0x780+f2.len()].copy_from_slice(&f2);}
 b
}
#[test]fn independently_anchored_owned_elf_candidates_vary_with_encoded_operands(){for address in [0xb20180,0xb20558,0xb2a008]{let c=object_cfg::resolve_owned_file(&fixture(address,false,false,false,false,false)).unwrap();assert_eq!(c.address,address);assert_ne!(c.allocation_entry,c.free_entry);assert_eq!(c.evidence_flags&63,63);}}
#[test]fn competing_complete_owned_candidates_refuse(){assert_eq!(object_cfg::resolve_owned_file(&fixture(0xb20180,true,false,false,false,false)).err(),Some(object_cfg::Error::Ambiguous));}
#[test]fn wrong_item_stride_refuses(){assert!(object_cfg::resolve_owned_file(&fixture(0xb20180,false,true,false,false,false)).is_err());}
#[test]fn overwritten_first_argument_refuses(){assert!(object_cfg::resolve_owned_file(&fixture(0xb20180,false,false,true,false,false)).is_err());}
#[test]fn missing_independent_free_anchor_refuses(){assert_eq!(object_cfg::resolve_owned_file(&fixture(0xb20180,false,false,false,true,false)).err(),Some(object_cfg::Error::NoCandidate));}
#[test]fn incomplete_indirect_branch_refuses(){assert_eq!(object_cfg::resolve_owned_file(&fixture(0xb20180,false,false,false,false,true)).err(),Some(object_cfg::Error::Incomplete));}
#[test]fn invalid_program_extent_refuses(){let mut b=fixture(0xb20180,false,false,false,false,false);put64(&mut b,64+32,u64::MAX);assert_eq!(object_cfg::resolve_owned_file(&b).err(),Some(object_cfg::Error::Malformed));}
#[test]fn nonwritable_argument_refuses(){assert!(object_cfg::resolve_owned_file(&fixture(0x710000,false,false,false,false,false)).is_err());}

#[test]fn unrelated_raw_anchor_underflow_before_positive_evidence_is_not_a_candidate(){
 for address in [0xb20208,0xb266a0]{let mut b=fixture(address,false,false,false,false,false);
  // Raw encoded RIP LEA at the beginning of RX: next IP minus 2^31
  // underflows. It cannot address either literal; no path executes this span.
  b[0x200..0x207].copy_from_slice(&[0x48,0x8d,0x15,0,0,0,0x80]);
  assert_eq!(object_cfg::resolve_owned_file(&b).unwrap().address,address);
 }
}
#[test]fn relevant_incomplete_competitor_still_refuses_complete_positive_pair(){
 let mut b=fixture(0xb24480,false,false,false,false,false);
 // Independent encoded target with a qualifying writable RDI caller, before
 // the anchors. Its indirect jump is unknowable despite the other complete pair.
 b[0x280..0x282].copy_from_slice(&[0xff,0xe0]);
 let c=caller(0x410680,0xb24500,0x410080,false);
 b[0x880..0x880+c.len()].copy_from_slice(&c);
 let image=object_cfg::FileImage::parse(&b).unwrap();
 assert!(image.writable_rdi_argument(0x410687).unwrap().is_some());
 assert_eq!(object_cfg::resolve_owned_file(&b).err(),Some(object_cfg::Error::Incomplete));
}
#[test]fn bounded_cfg_node_budget_still_refuses(){
 let b=fixture(0xb24480,false,false,false,false,false);
 let image=object_cfg::FileImage::parse(&b).unwrap();
 assert!(matches!(image.bounded_cfg(0x410100,&[],object_cfg::CfgLimits{radius:65536,max_nodes:1}),Err(object_cfg::Error::Incomplete)));
}

// Integration-only policy controls; the matcher above remains frozen unchanged.
fn original_immediate(b:&mut[u8],offset:usize,address:u32){let mut p=vec![0x41,0x39,0xee,0x0f,0x8e,0,0,0,0,0xbf];p.extend_from_slice(&address.to_le_bytes());p.extend_from_slice(&[0x48,0x8b,0x74,0x24,0x10,0xe8,0,0,0,0,0xe9]);b[offset..offset+p.len()].copy_from_slice(&p);}
fn integrated_gu(b:&[u8])->(i32,u64){let mut out=123;let status=unsafe{ps_guobject_array_file_image(b.as_ptr(),b.len(),&mut out)};(status,out)}
#[test]fn original_exact_empty_elf_result_enters_static_candidate_fallback(){let b=fixture(0xb20558,false,false,false,false,false);let image=OwnedImage::parse(&b).unwrap();let ctx=resolvers::AsyncContext{exe:&image};assert_eq!(guobject_array::resolve_guobject_array_patterns(&ctx).unwrap_err().message,"expected at least one value");assert_eq!(integrated_gu(&b),(0,0xb20558));}
#[test]fn original_success_and_ambiguity_never_fall_back(){let mut b=fixture(0xb20558,false,false,false,false,false);original_immediate(&mut b,0x830,0x11111111);assert_eq!(integrated_gu(&b),(0,0x11111111));original_immediate(&mut b,0x860,0x22222222);assert_eq!(integrated_gu(&b),(4,0));}
#[test]fn original_operand_error_never_falls_back(){let mut b=fixture(0xb20558,false,false,false,false,false);let mut p=vec![0x74,0,0x48,0x8d,0x0d];p.extend_from_slice(&(-0x500000i32).to_le_bytes());p.extend_from_slice(&[0xc6,0x05,0,0,0,0,0x01,0xe8,0,0,0,0,0xc6,0x05,0,0,0,0,0x01]);b[0x830..0x830+p.len()].copy_from_slice(&p);let image=OwnedImage::parse(&b).unwrap();let ctx=resolvers::AsyncContext{exe:&image};let original=guobject_array::resolve_guobject_array_patterns(&ctx).unwrap_err();assert_eq!(original.message,"serialized section operand is out of bounds or overflows");assert_eq!(guobject_array::resolve_guobject_array(&ctx).unwrap_err().message,original.message);assert_eq!(integrated_gu(&b),(5,0));}
#[test]fn pe_original_no_match_is_preserved(){assert_eq!(integrated_gu(include_bytes!("../fixtures/fname-pe.exe")),(3,0));}
#[test]fn static_fallback_ambiguity_and_incompleteness_stay_errors(){assert_eq!(integrated_gu(&fixture(0xb20558,true,false,false,false,false)),(4,0));assert_eq!(integrated_gu(&fixture(0xb20558,false,false,false,false,true)),(5,0));}
#[test]fn explicit_static_candidate_c_abi_layout_and_refusals(){assert_eq!(core::mem::size_of::<PsObjectArrayStaticCandidate>(),48);assert_eq!(core::mem::offset_of!(PsObjectArrayStaticCandidate,evidence_flags),40);let mut out=PsObjectArrayStaticCandidate{address:123,..Default::default()};assert_eq!(unsafe{ps_guobject_array_cfg_file_image(core::ptr::null(),1,&mut out)},1);assert_eq!(out.address,123);let b=fixture(0xb20558,false,false,false,false,false);assert_eq!(unsafe{ps_guobject_array_cfg_file_image(b.as_ptr(),b.len(),&mut out)},0);assert_eq!(out.address,0xb20558);assert_eq!(out.evidence_flags&63,63);let bad=[1u8,2,3];assert_eq!(unsafe{ps_guobject_array_cfg_file_image(bad.as_ptr(),bad.len(),&mut out)},2);assert_eq!(out.address,0);assert_eq!(out.evidence_flags,0);}
