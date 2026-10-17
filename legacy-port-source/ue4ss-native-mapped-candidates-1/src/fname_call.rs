//! Conservative ELF SysV FName constructor call-site candidate inference.
//! A shared encoded target is static evidence, never a callable/validated pointer.
use alloc::{vec::Vec,collections::{BTreeSet,BTreeMap}};
use iced_x86::{Decoder,DecoderOptions,InstructionInfoFactory,FlowControl,Mnemonic,OpKind,Register,OpAccess};
use object::read::elf::{FileHeader,ProgramHeader};
use crate::{MemoryTrait,ImageKind,resolvers::AsyncContext,util,fname_util};
#[derive(Debug,PartialEq,Eq)] pub enum Error {NoCandidate,Ambiguous,Malformed,Incomplete}
#[derive(Debug)] pub struct Candidate {pub address:u64,pub calls_by_anchor:[Vec<u64>;5]}
const NAMES:[&str;5]=["Engine","Renderer","AnimGraphRuntime","Landscape","RenderCore"];
fn writes(info:&mut InstructionInfoFactory,i:&iced_x86::Instruction,r:Register)->bool {
 info.info(i).used_registers().iter().any(|v|v.register().full_register()==r && matches!(v.access(),OpAccess::Write|OpAccess::CondWrite|OpAccess::ReadWrite|OpAccess::ReadCondWrite))
}
fn executable_ranges(bytes:&[u8])->Result<Vec<core::ops::Range<u64>>,Error>{
 let h=object::elf::FileHeader64::<object::LittleEndian>::parse(bytes).map_err(|_|Error::Malformed)?;let e=object::LittleEndian;
 let mut ranges=Vec::new();
 for p in h.program_headers(e,bytes).map_err(|_|Error::Malformed)?{
  if p.p_type(e)!=object::elf::PT_LOAD||p.p_flags(e)&object::elf::PF_X==0{continue;}
  let end=p.p_vaddr(e).checked_add(p.p_filesz(e)).ok_or(Error::Malformed)?;
  if p.p_offset(e).checked_add(p.p_filesz(e)).is_none_or(|n|n>bytes.len() as u64){return Err(Error::Malformed);}
  ranges.push(p.p_vaddr(e)..end);
 }
 Ok(ranges)
}
/// First direct call after a genuine RSI string reference, within64bytes.
/// All three arguments must be established along this one sequential path;
/// an intervening call/branch/clobber invalidates the evidence.
fn call_after(ctx:&AsyncContext<'_>,reference:u64,strings:&BTreeSet<u64>,code:&[core::ops::Range<u64>])->Option<(u64,u64)>{
 if !code.iter().any(|r|r.contains(&reference)){return None;}
 let bytes=ctx.image().memory.range_from(reference).ok()?;let mut d=Decoder::with_ip(64,&bytes[..bytes.len().min(64)],reference,DecoderOptions::NONE);
 let first=d.decode();
 if first.is_invalid()||first.mnemonic()!=Mnemonic::Lea||first.op0_register()!=Register::RSI||!first.is_ip_rel_memory_operand(){return None;}
 let offsets=d.get_constant_offsets(&first);if offsets.displacement_size()!=4{return None;}
 let raw=bytes.get(offsets.displacement_offset()..offsets.displacement_offset()+4)?;
 let string=first.next_ip().checked_add_signed(i32::from_le_bytes(raw.try_into().ok()?) as i64)?;
 if !strings.contains(&string){return None;}
 let mut stack_arg=false;let mut enum_arg=false;let mut info=InstructionInfoFactory::new();
 while d.can_decode(){
  let i=d.decode();if i.is_invalid(){return None;}
  if i.flow_control()==FlowControl::Call {
   if i.op0_kind()!=OpKind::NearBranch64||!stack_arg||!enum_arg{return None;}
   let at=usize::try_from(i.next_ip().checked_sub(reference)?.checked_sub(4)?).ok()?;
   let raw=bytes.get(at..at.checked_add(4)?)?;
   let target=i.next_ip().checked_add_signed(i32::from_le_bytes(raw.try_into().ok()?) as i64)?;return code.iter().any(|r|r.contains(&target)).then_some((i.ip(),target));
  }
  if i.flow_control()!=FlowControl::Next{return None;}
  if writes(&mut info,&i,Register::RSI){return None;}
  if writes(&mut info,&i,Register::RDI){stack_arg=i.mnemonic()==Mnemonic::Lea&&i.op0_register()==Register::RDI&&matches!(i.memory_base(),Register::RSP|Register::RBP)&&i.memory_index()==Register::None;}
  if writes(&mut info,&i,Register::RDX){enum_arg=i.mnemonic()==Mnemonic::Mov&&i.op0_register()==Register::EDX&&i.op1_kind()==OpKind::Immediate32&&i.immediate32()==1;}
 }None
}
pub fn resolve(ctx:&AsyncContext<'_>,bytes:&[u8])->Result<Candidate,Error>{
 if ctx.image().kind!=ImageKind::Elf{return Err(Error::NoCandidate);}
 let code=executable_ranges(bytes)?;resolve_with_ranges(ctx,&code)
}
/// Existing bounded SysV call analysis using explicit executable range facts.
pub(crate) fn resolve_with_ranges(ctx:&AsyncContext<'_>,code:&[core::ops::Range<u64>])->Result<Candidate,Error>{
 if ctx.image().kind!=ImageKind::Elf{return Err(Error::NoCandidate);}
 let mut groups:Vec<BTreeMap<u64,Vec<u64>>>=Vec::new();
 for name in NAMES {
  let pattern=alloc::format!("\0{name}\0");
  let strings:BTreeSet<_>=ctx.scan(util::utf16_pattern(&pattern)).into_iter().filter_map(|a|a.checked_add(2)).collect();
  if strings.is_empty(){return Err(Error::NoCandidate);}if strings.len()>64{return Err(Error::Incomplete);}
  let refs=fname_util::scan_xrefs(ctx,&strings.iter().copied().collect::<Vec<_>>());if refs.len()>16384{return Err(Error::Incomplete);}
  let mut targets:BTreeMap<u64,Vec<u64>>=BTreeMap::new();
  for reference in refs {if let Some((call,target))=call_after(ctx,reference,&strings,&code){let calls=targets.entry(target).or_default();if !calls.contains(&call){calls.push(call);}}}
  if targets.is_empty(){return Err(Error::NoCandidate);}groups.push(targets);
 }
 let mut common:BTreeSet<_>=groups[0].keys().copied().collect();
 for g in &groups[1..]{common.retain(|a|g.contains_key(a));}
 if common.is_empty(){return Err(Error::NoCandidate);}if common.len()!=1{return Err(Error::Ambiguous);}
 let address=*common.first().unwrap();let calls_by_anchor=core::array::from_fn(|i|groups[i].remove(&address).unwrap());
 Ok(Candidate{address,calls_by_anchor})
}
