// Additive static SysV ELF cleanup candidate. Serialized bytes only; this does
// not supply GMalloc identity, a live singleton, a mapping, or missing roots.
use alloc::{vec,vec::Vec};
use iced_x86::{Code,Decoder,DecoderOptions,FlowControl,Instruction,InstructionInfoFactory,Mnemonic,OpAccess,OpKind,Register};
use crate::{OwnedImage,MemoryTrait,ImageKind,resolvers::{AsyncContext,ResolveError,ensure_one},required_util as util};
#[derive(Clone,Copy,Debug,Eq,PartialEq)]enum Origin{Unknown,IncomingPointer,SlotAddress(u64),Receiver(u64),Vtable(u64)}
#[derive(Clone,Debug,Eq,PartialEq)]pub struct CandidateProof{pub slot:u64,pub direct_callee:u64,pub receiver_call:u64,pub vtable_offset:u64}
fn index(r:Register)->Option<usize>{let r=r.full_register();r.is_gpr64().then(||r as usize-Register::RAX as usize)}
fn get(regs:&[Origin;16],r:Register)->Origin{index(r).map(|i|regs[i]).unwrap_or(Origin::Unknown)}
fn set(regs:&mut[Origin;16],r:Register,value:Origin){if let Some(i)=index(r){regs[i]=value;}}
fn decode(image:&OwnedImage<'_>,ip:u64,code:Option<&[core::ops::Range<u64>]>)->Result<Instruction,()>{let mut d=Decoder::with_ip(64,image.memory.range_from(ip)?,ip,DecoderOptions::NONE);let i=d.decode();if i.is_invalid()||code.is_some_and(|ranges|i.next_ip()<=ip||!ranges.iter().any(|r|r.start<=ip&&i.next_ip()<=r.end)){Err(())}else{Ok(i)}}
fn input_null_test(i:&Instruction)->bool{i.code()==Code::Test_rm64_r64&&i.op0_register()==Register::RDI&&i.op1_register()==Register::RDI}
// An actual encoded null branch must return. This never invents an FDE/root.
fn null_return(image:&OwnedImage<'_>,mut ip:u64,end:u64,code:Option<&[core::ops::Range<u64>]> )->Result<bool,()>{for _ in 0..8{if ip>=end{return Ok(false);}let i=decode(image,ip,code)?;if i.next_ip()>end{return Ok(false);}if i.flow_control()==FlowControl::Return{return Ok(true);}if i.flow_control()!=FlowControl::Next||i.is_ip_rel_memory_operand(){return Ok(false);}if !matches!(i.mnemonic(),Mnemonic::Pop|Mnemonic::Add|Mnemonic::Nop){return Ok(false);}ip=i.next_ip();}Ok(false)}
pub fn prove_direct_sysv_free(image:&OwnedImage<'_>,entry:u64)->Result<Vec<CandidateProof>,()>{prove_direct_sysv_free_inner(image,entry,false,None)}
fn prove_direct_sysv_free_inner(image:&OwnedImage<'_>,entry:u64,refuse_indirect_unknown:bool,code:Option<&[core::ops::Range<u64>]>)->Result<Vec<CandidateProof>,()>{
 if image.kind!=ImageKind::Elf{return Ok(vec![]);}
 let bound=entry.checked_add(1000).ok_or(())?;let first=decode(image,entry,code)?;if !input_null_test(&first){return Ok(vec![]);}let branch=decode(image,first.next_ip(),code)?;
 if branch.mnemonic()!=Mnemonic::Je||branch.flow_control()!=FlowControl::ConditionalBranch||!null_return(image,branch.near_branch_target(),bound,code)?{return Ok(vec![]);}
 let mut regs=[Origin::Unknown;16];set(&mut regs,Register::RDI,Origin::IncomingPointer);let mut queue=vec![(branch.next_ip(),regs,None::<u64>)];let mut seen=Vec::new();let mut proofs=Vec::new();let mut factory=InstructionInfoFactory::new();
 while let Some((ip,mut regs,mut tested_slot))=queue.pop(){
  if ip<entry||ip>=bound{return Err(());}if seen.contains(&(ip,regs,tested_slot)){continue;}if seen.len()>=1024{return Err(());}seen.push((ip,regs,tested_slot));let i=decode(image,ip,code)?;if i.next_ip()>bound{return Err(());}
  let destination=i.op0_register();let mut value=None;
  if i.code()==Code::Test_rm64_r64&&i.op0_register()==i.op1_register(){if let Origin::Receiver(slot)=get(&regs,i.op0_register()){tested_slot=Some(slot);}}
  if i.code()==Code::Cmp_rm64_imm8&&i.op0_kind()==OpKind::Memory&&i.memory_base()==Register::RIP&&i.immediate8()==0{tested_slot=Some(i.ip_rel_memory_address());}
  if i.code()==Code::Lea_r64_m&&i.op0_kind()==OpKind::Register&&i.memory_base()==Register::RIP&&i.memory_index()==Register::None{value=Some(Origin::SlotAddress(i.ip_rel_memory_address()));}
  if matches!(i.code(),Code::Mov_r64_rm64|Code::Mov_rm64_r64)&&i.op0_kind()==OpKind::Register {
   value=Some(if i.op1_kind()==OpKind::Register{get(&regs,i.op1_register())}else if i.op1_kind()==OpKind::Memory&&i.memory_index()==Register::None{
    if i.memory_base()==Register::RIP{Origin::Receiver(i.ip_rel_memory_address())}else if i.memory_displacement64()==0{match get(&regs,i.memory_base()){Origin::SlotAddress(s)=>Origin::Receiver(s),Origin::Receiver(s)=>Origin::Vtable(s),_=>Origin::Unknown}}else{Origin::Unknown}
   }else{Origin::Unknown});
  }
  let mut proven_virtual=false;
  if matches!(i.flow_control(),FlowControl::IndirectCall|FlowControl::IndirectBranch)&&i.op0_kind()==OpKind::Memory&&i.memory_index()==Register::None {
   if let Origin::Vtable(slot)=get(&regs,i.memory_base()) {
    let offset=i.memory_displacement64();if tested_slot==Some(slot)&&get(&regs,Register::RDI)==Origin::Receiver(slot)&&get(&regs,Register::RSI)==Origin::IncomingPointer&&offset<=256&&offset%8==0 {
     proofs.push(CandidateProof{slot,direct_callee:entry,receiver_call:i.ip(),vtable_offset:offset});proven_virtual=true;
    }
   }
   if !proven_virtual{return Err(());}
  }
  // The mapped strategy must not turn a qualifying unknown register-indirect
  // path into an empty proof: it can conceal a competing singleton candidate.
  if refuse_indirect_unknown&&matches!(i.flow_control(),FlowControl::IndirectCall|FlowControl::IndirectBranch)&&!proven_virtual{return Err(());}
  for used in factory.info(&i).used_registers(){if matches!(used.access(),OpAccess::Write|OpAccess::CondWrite|OpAccess::ReadWrite|OpAccess::ReadCondWrite){set(&mut regs,used.register(),Origin::Unknown);}}
  if let Some(value)=value{set(&mut regs,destination,value);}
  match i.flow_control(){
   FlowControl::Return=>{},
   FlowControl::ConditionalBranch=>{queue.push((i.near_branch_target(),regs,tested_slot));queue.push((i.next_ip(),regs,tested_slot));},
   FlowControl::UnconditionalBranch=>queue.push((i.near_branch_target(),regs,tested_slot)),
   FlowControl::IndirectBranch=>{},
   FlowControl::Call|FlowControl::IndirectCall=>{for r in [Register::RAX,Register::RCX,Register::RDX,Register::RSI,Register::RDI,Register::R8,Register::R9,Register::R10,Register::R11]{set(&mut regs,r,Origin::Unknown);}queue.push((i.next_ip(),regs,tested_slot));},
   FlowControl::Next=>queue.push((i.next_ip(),regs,tested_slot)),
   _=>return Err(()),
  }
 }
 proofs.sort_by_key(|p|(p.slot,p.direct_callee,p.receiver_call,p.vtable_offset));proofs.dedup();Ok(proofs)
}
pub fn prove_deletefile_candidates(ctx:&AsyncContext<'_>)->Result<Vec<CandidateProof>,ResolveError>{prove_deletefile_candidates_inner(ctx,false,None)}
pub(crate) fn prove_mapped_deletefile_candidates(ctx:&AsyncContext<'_>,code:&[core::ops::Range<u64>])->Result<Vec<CandidateProof>,ResolveError>{prove_deletefile_candidates_inner(ctx,true,Some(code))}
fn prove_deletefile_candidates_inner(ctx:&AsyncContext<'_>,refuse_incomplete:bool,code:Option<&[core::ops::Range<u64>]>)->Result<Vec<CandidateProof>,ResolveError>{
 if ctx.image().kind!=ImageKind::Elf{return Err(ResolveError::new_msg("SysV ELF candidate only"));}
 let strings=ctx.scan(util::utf16_pattern("DeleteFile %s\0"));let refs=util::scan_xrefs(ctx,&strings);let mut proofs=Vec::new();
 for xref in refs {
  if code.is_some_and(|ranges|!ranges.iter().any(|r|r.contains(&xref))){continue;}
  let first=decode(ctx.image(),xref,code)?;if first.code()!=Code::Lea_r64_m||first.memory_base()!=Register::RIP||!strings.contains(&first.ip_rel_memory_address()){continue;}
  let bytes=ctx.image().memory.range_from(xref)?;let mut d=Decoder::with_ip(64,&bytes[..bytes.len().min(256)],xref,DecoderOptions::NONE);let mut instructions=Vec::new();for instruction in d.iter(){if instruction.is_invalid()||code.is_some_and(|ranges|instruction.next_ip()<=instruction.ip()||!ranges.iter().any(|r|r.start<=instruction.ip()&&instruction.next_ip()<=r.end)){break;}let stop=matches!(instruction.flow_control(),FlowControl::Return|FlowControl::UnconditionalBranch|FlowControl::IndirectBranch);instructions.push(instruction);if stop{break;}}
  for window in instructions.windows(4){let [load,test,skip,call]=window else{unreachable!()};
   if load.code()!=Code::Mov_r64_rm64||load.op0_register()!=Register::RDI||load.op1_kind()!=OpKind::Memory||!matches!(load.memory_base(),Register::RBP|Register::RSP)||load.memory_index()!=Register::None||!input_null_test(test)||skip.mnemonic()!=Mnemonic::Je||call.code()!=Code::Call_rel32_64||skip.near_branch_target()!=call.next_ip(){continue;}
   // Only an encoded direct call entry is considered. Missing roots stay absent.
   match prove_direct_sysv_free_inner(ctx.image(),call.near_branch_target(),refuse_incomplete,code){
    Ok(found)=>proofs.extend(found),
    Err(())if refuse_incomplete=>return Err(ResolveError::new_msg("qualifying mapped SysV free path incomplete")),
    Err(())=>{},
   }
  }
 }
 Ok(proofs)
}
pub fn resolve_deletefile_candidate(ctx:&AsyncContext<'_>)->Result<u64,ResolveError>{ensure_one(prove_deletefile_candidates(ctx)?.into_iter().map(|p|p.slot))}
