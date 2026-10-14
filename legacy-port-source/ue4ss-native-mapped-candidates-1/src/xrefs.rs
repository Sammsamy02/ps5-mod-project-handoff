use alloc::{format,vec::Vec};
#[cfg(test)] use alloc::vec;
use scanner::Pattern;
use crate::resolvers::AsyncContext;
// Synchronous port of the pinned upstream unreal::util rules. Batch only the
// traversal: target order, indirect-pointer expansion, encodings and duplicates
// are preserved. These labels describe serialized bytes, never native pointers.
fn targets_with_indirection(ctx:&AsyncContext<'_>,addresses:&[u64])->Vec<u64> {
 let patterns:Vec<_>=addresses.iter().map(|a|Pattern::from_bytes(a.to_le_bytes().to_vec()).unwrap()).collect();
 addresses.iter().copied().chain(ctx.scan_many(&patterns).into_iter().flatten()).collect()
}
pub fn scan_xrefs(ctx:&AsyncContext<'_>,addresses:&[u64])->Vec<u64> {
 let mut patterns=Vec::new();
 for address in targets_with_indirection(ctx,addresses) {
  for lea in ["48 8d 05","48 8d 0d","48 8d 15","48 8d 1d","48 8d 25","48 8d 2d","48 8d 35","48 8d 3d","4c 8d 05","4c 8d 0d","4c 8d 15","4c 8d 1d","4c 8d 25","4c 8d 2d","4c 8d 35","4c 8d 3d"] {
   patterns.push(Pattern::new(format!("{lea} X0x{address:X}")).unwrap());
  }
  if u32::try_from(address).is_ok() {
   for op in ["b8","b9","ba","bb","bc","bd","be","bf"] {patterns.push(Pattern::new(format!("{op} 0x{address:X}")).unwrap());}
  }
 }
 ctx.scan_many(&patterns).into_iter().flatten().collect()
}
pub fn scan_xcalls(ctx:&AsyncContext<'_>,addresses:&[u64])->Vec<u64> {
 let mut patterns=Vec::new();
 for address in targets_with_indirection(ctx,addresses) {
  for op in ["e8","e9"] {patterns.push(Pattern::new(format!("{op} X0x{address:X}")).unwrap());}
 }
 ctx.scan_many(&patterns).into_iter().flatten().collect()
}

#[cfg(test)] mod tests {
 use super::*;
 use crate::{OwnedImage,OwnedSection};
 #[test] fn genuine_xref_and_indirect_encodings() {
  let base=0x1000u64;let target=0x7000u64;let pointer=0x8000u64;
  let mut code=vec![0x90;256];
  let mut expected=vec![];
  let prefixes=[0x48,0x4c];let mut at=0usize;
  for rex in prefixes {for modrm in [0x05,0x0d,0x15,0x1d,0x25,0x2d,0x35,0x3d] {
   code[at..at+3].copy_from_slice(&[rex,0x8d,modrm]);
   code[at+3..at+7].copy_from_slice(&((target as i64-(base+at as u64+7) as i64) as i32).to_le_bytes());
   expected.push(base+at as u64);at+=8;
  }}
  for opcode in 0xb8..=0xbf {code[at]=opcode;code[at+1..at+5].copy_from_slice(&(target as u32).to_le_bytes());expected.push(base+at as u64);at+=8;}
  code[at..at+3].copy_from_slice(&[0x48,0x8d,0x05]);code[at+3..at+7].copy_from_slice(&((pointer as i64-(base+at as u64+7) as i64) as i32).to_le_bytes());expected.push(base+at as u64);
  let ptr=target.to_le_bytes();let image=OwnedImage::with_metadata(&[],vec![OwnedSection{address:base,data:&code},OwnedSection{address:pointer,data:&ptr}]);
  let ctx=AsyncContext{exe:&image};let mut got=scan_xrefs(&ctx,&[target]);got.sort_unstable();expected.sort_unstable();assert_eq!(got,expected);
 }
 fn linear_refs(ctx:&AsyncContext<'_>,addresses:&[u64])->Vec<u64> {
  let indirect:Vec<_>=addresses.iter().flat_map(|a|ctx.scan(Pattern::from_bytes(a.to_le_bytes().to_vec()).unwrap())).collect();let mut out=vec![];
  for a in addresses.iter().copied().chain(indirect) {
   for rex in [0x48,0x4c] {for reg in 0..8 {out.extend(ctx.scan(Pattern::new(format!("{rex:02x} 8d {:02x} X0x{a:X}",5+8*reg)).unwrap()));}}
   if a<=u32::MAX as u64 {for op in 0xb8..=0xbf {out.extend(ctx.scan(Pattern::new(format!("{op:02x} 0x{a:X}")).unwrap()));}}
  }out
 }
 #[test] fn batched_matches_original_sequential_search() {
  for case in 0..32u64 {
   let base=if case%2==0 {0x1000} else {0x100001000};let target=base+0x4000+case*8;
   let mut seed=case+1;let mut code=vec![0u8;4096];for b in &mut code {seed=seed.wrapping_mul(6364136223846793005).wrapping_add(1);*b=(seed>>32) as u8;}
   for slot in 0..16usize {let at=slot*32;let addr=target+((slot%3)*16) as u64;code[at..at+3].copy_from_slice(&[if slot%2==0 {0x48}else{0x4c},0x8d,5+8*((slot%8)as u8)]);code[at+3..at+7].copy_from_slice(&((addr as i64-(base+at as u64+7)as i64)as i32).to_le_bytes());}
   let ptr=target.to_le_bytes();let image=OwnedImage::with_metadata(&[],vec![OwnedSection{address:base,data:&code},OwnedSection{address:base+0x8000,data:&ptr}]);let ctx=AsyncContext{exe:&image};let targets=[target+32,target,target+16,target];
   assert_eq!(scan_xrefs(&ctx,&targets),linear_refs(&ctx,&targets));
  }
 }
 #[test] fn calls_and_jumps_preserve_target_order() {
  let base=0x1000u64;let targets=[0x7000u64,0x6000];let mut code=vec![0x90;64];
  for (at,target,opcode) in [(0,targets[0],0xe8),(16,targets[1],0xe9)] {code[at]=opcode;code[at+1..at+5].copy_from_slice(&((target as i64-(base+at as u64+5) as i64) as i32).to_le_bytes());}
  let image=OwnedImage::with_metadata(&[],vec![OwnedSection{address:base,data:&code}]);let ctx=AsyncContext{exe:&image};assert_eq!(scan_xcalls(&ctx,&targets),vec![base,base+16]);assert!(scan_xrefs(&ctx,&[]).is_empty());
 }
 #[test] #[ignore] fn owned_buffer_batch_timing() {
  use std::time::Instant;
  let base=0x100000000u64;let mut seed=19u64;let mut code=vec![0u8;8*1024*1024];
  for b in &mut code {seed=seed.wrapping_mul(6364136223846793005).wrapping_add(1);*b=(seed>>32)as u8;}
  let targets:Vec<u64>=(0..16).map(|i|base+0x1800000+i*32).collect();
  for (i,target) in targets.iter().enumerate() {let at=256+i*16;code[at..at+3].copy_from_slice(&[0x48,0x8d,0x05]);code[at+3..at+7].copy_from_slice(&((*target as i64-(base+at as u64+7)as i64)as i32).to_le_bytes());}
  let image=OwnedImage::with_metadata(&[],vec![OwnedSection{address:base,data:&code}]);let ctx=AsyncContext{exe:&image};
  let before=Instant::now();let expected=linear_refs(&ctx,&targets);let sequential=before.elapsed();
  let before=Instant::now();let actual=scan_xrefs(&ctx,&targets);let batched=before.elapsed();assert_eq!(actual,expected);assert_eq!(actual.len(),16);
  std::println!("owned_bytes={} targets={} sequential_ms={} batched_ms={} identical_results=true",code.len(),targets.len(),sequential.as_millis(),batched.as_millis());
 }

}
