//! Generic owned-file, diagnostic-anchored CFG/caller-argument evidence matcher.
//! No function extents are inferred and no image label is read as live memory.
use alloc::{vec,vec::Vec,collections::{BTreeMap,BTreeSet}};
use iced_x86::{Decoder,DecoderOptions,Instruction,InstructionInfoFactory,FlowControl,Mnemonic,OpKind,Register,OpAccess};
use object::read::elf::{FileHeader,ProgramHeader};
use crate::PsObjectArrayStaticCandidate;
#[derive(Debug,Clone,Copy,PartialEq,Eq)]pub enum Error{Malformed,NoCandidate,Ambiguous,Incomplete}
pub(crate) const ALLOC_DIAGNOSTIC:&str="Unable to add more objects to disregard for GC pool (Max: %d)";
pub(crate) const FREE_DIAGNOSTIC:&str="Removing object (0x%016llx) at index %d but the index points to a different object (0x%016llx)!";
const RADIUS:u64=65536;
const NODE_LIMIT:usize=8192;
const ROOT_LIMIT:usize=256;
const XREF_LIMIT:usize=64;
#[derive(Clone)]struct Span<'a>{va:u64,mem:u64,flags:u32,data:&'a [u8]}
pub struct FileImage<'a>{spans:Vec<Span<'a>>}
impl<'a> FileImage<'a>{
 pub fn parse(bytes:&'a [u8])->Result<Self,Error>{
  let h=object::elf::FileHeader64::<object::LittleEndian>::parse(bytes).map_err(|_|Error::Malformed)?;let e=object::LittleEndian;
  if !h.is_class_64()||!h.is_little_endian()||h.e_machine(e)!=object::elf::EM_X86_64||h.e_version(e)!=1||h.e_ehsize(e)!=64{return Err(Error::Malformed);}
  let ph=h.program_headers(e,bytes).map_err(|_|Error::Malformed)?;if ph.len()>128{return Err(Error::Malformed);}
  let mut spans=vec![];
  for p in ph {if p.p_type(e)!=object::elf::PT_LOAD{continue;}let start=p.p_offset(e);let size=p.p_filesz(e);let mem=p.p_memsz(e);let va=p.p_vaddr(e);let end=start.checked_add(size).ok_or(Error::Malformed)?;va.checked_add(mem).ok_or(Error::Malformed)?;
   if size>mem||end>bytes.len() as u64{return Err(Error::Malformed);}
   let range=usize::try_from(start).map_err(|_|Error::Malformed)?..usize::try_from(end).map_err(|_|Error::Malformed)?;let data=bytes.get(range).ok_or(Error::Malformed)?;
   if mem!=0{spans.push(Span{va,mem,flags:p.p_flags(e),data});}
  }
  // Aliased load labels would make executable/global classification ambiguous.
  for i in 0..spans.len(){for j in 0..i{if spans[i].va<spans[j].va+spans[j].mem&&spans[j].va<spans[i].va+spans[i].mem{return Err(Error::Malformed);}}}
  if spans.iter().all(|s|s.flags&1==0||s.data.is_empty()){return Err(Error::Malformed);}
  Ok(Self{spans})
 }
 /// Construct only from validated actual-pointer borrowed mapped segments.
 pub(crate) fn from_mapped(segments:&[crate::mapped_image::MappedSegment<'a>])->Result<Self,Error>{
  let mut spans=vec![];
  for segment in segments {
   if segment.bytes.is_empty()||segment.runtime_address!=segment.bytes.as_ptr()as usize as u64||segment.permissions&1==0||segment.permissions&!7!=0{return Err(Error::Malformed);}
   segment.runtime_address.checked_add(segment.bytes.len()as u64).ok_or(Error::Malformed)?;
   let flags=((segment.permissions&1)<<2)|(segment.permissions&2)|((segment.permissions&4)>>2);
   spans.push(Span{va:segment.runtime_address,mem:segment.bytes.len()as u64,flags,data:segment.bytes});
  }
  for i in 0..spans.len(){for j in 0..i{if spans[i].va<spans[j].va+spans[j].mem&&spans[j].va<spans[i].va+spans[i].mem{return Err(Error::Malformed);}}}
  if spans.iter().all(|s|s.flags&1==0){return Err(Error::Malformed);}
  Ok(Self{spans})
 }
 fn executable(&self,ip:u64)->Option<&Span<'a>>{self.spans.iter().find(|s|s.flags&1!=0&&s.va<=ip&&ip<s.va+s.data.len() as u64)}
 fn writable(&self,ip:u64)->bool{self.spans.iter().any(|s|s.flags&2!=0&&s.va<=ip&&ip<s.va+s.mem)}
}
fn decode(span:&Span<'_>,ip:u64)->Result<Instruction,Error>{
 let at=usize::try_from(ip.checked_sub(span.va).ok_or(Error::Incomplete)?).map_err(|_|Error::Incomplete)?;
 let bytes=span.data.get(at..).ok_or(Error::Incomplete)?;let i=Decoder::with_ip(64,bytes,ip,DecoderOptions::NONE).decode();
 if i.is_invalid()||i.next_ip()<=ip||i.next_ip()>span.va+span.data.len() as u64{return Err(Error::Incomplete);}Ok(i)
}
fn rip_displacement(span:&Span<'_>,i:&Instruction)->Result<i32,Error>{
 let offset=usize::try_from(i.ip()-span.va).map_err(|_|Error::Incomplete)?;let bytes=&span.data[offset..];let mut decoder=Decoder::with_ip(64,bytes,i.ip(),DecoderOptions::NONE);let decoded=decoder.decode();if decoded!=*i{return Err(Error::Incomplete);}let constants=decoder.get_constant_offsets(i);
 if constants.displacement_size()!=4{return Err(Error::Incomplete);}let start=constants.displacement_offset();let b=bytes.get(start..start+4).ok_or(Error::Incomplete)?;let displacement=i32::from_le_bytes(b.try_into().map_err(|_|Error::Incomplete)?);
 Ok(displacement)
}
fn rip_target(span:&Span<'_>,i:&Instruction)->Result<u64,Error>{
 i.next_ip().checked_add_signed(rip_displacement(span,i)? as i64).ok_or(Error::Incomplete)
}
fn string_targets(image:&FileImage<'_>,text:&str)->Result<BTreeSet<u64>,Error>{
 let mut output=BTreeSet::new();let mut ascii=text.as_bytes().to_vec();ascii.push(0);let mut wide:Vec<u8>=text.encode_utf16().flat_map(u16::to_le_bytes).collect();wide.extend_from_slice(&[0,0]);
 for s in image.spans.iter().filter(|s|s.flags&5!=0){for pattern in [&ascii,&wide]{if pattern.len()>s.data.len(){continue;}for (offset,b) in s.data.windows(pattern.len()).enumerate(){if b[0]==pattern[0]&&b==pattern.as_slice(){output.insert(s.va+offset as u64);if output.len()>XREF_LIMIT{return Err(Error::Incomplete);}}}}}Ok(output)
}
fn anchors(image:&FileImage<'_>)->Result<Vec<(usize,u64)>,Error>{
 let targets=[string_targets(image,ALLOC_DIAGNOSTIC)?,string_targets(image,FREE_DIAGNOSTIC)?];if targets.iter().any(|s|s.is_empty()){return Err(Error::NoCandidate);}let mut anchors=vec![];
 for s in image.spans.iter().filter(|s|s.flags&1!=0){for offset in 0..s.data.len().saturating_sub(6){let b=&s.data[offset..];if !(0x40..=0x4f).contains(&b[0])||b[1]!=0x8d||b[2]&0xc7!=5{continue;}let ip=s.va+offset as u64;let Ok(i)=decode(s,ip)else{continue;};if i.mnemonic()!=Mnemonic::Lea||!i.is_ip_rel_memory_operand(){continue;}// Raw scan encodings are candidate xrefs only. A nonformable address
// cannot name a literal; preserve structural decode errors and all path errors.
let Some(target)=i.next_ip().checked_add_signed(rip_displacement(s,&i)? as i64)else{continue;};for(role,addresses)in targets.iter().enumerate(){if addresses.contains(&target){anchors.push((role,ip));if anchors.len()>XREF_LIMIT{return Err(Error::Incomplete);}}}}}
 if !anchors.iter().any(|(r,_)|*r==0)||!anchors.iter().any(|(r,_)|*r==1){return Err(Error::NoCandidate);}Ok(anchors)
}
fn call_target(s:&Span<'_>,offset:usize)->Option<(u64,u64)>{
 if s.data.get(offset)!=Some(&0xe8){return None;}let raw=s.data.get(offset+1..offset+5)?;let ip=s.va+offset as u64;let i=decode(s,ip).ok()?;if i.mnemonic()!=Mnemonic::Call||i.op0_kind()!=OpKind::NearBranch64{return None;}
 let target=i.next_ip().checked_add_signed(i32::from_le_bytes(raw.try_into().ok()?) as i64)?;Some((ip,target))
}
/// Generic bounded control-flow evidence. Instructions are reachable decoded
/// nodes, never a guessed function extent. Calls are treated as opaque returning
/// edges; callee bodies and indirect branch targets are not guessed.
#[derive(Clone,Copy)]pub struct CfgLimits{pub radius:u64,pub max_nodes:usize}
impl Default for CfgLimits{fn default()->Self{Self{radius:RADIUS,max_nodes:NODE_LIMIT}}}
pub struct CfgEvidence{pub entry:u64,pub reached_targets:BTreeSet<u64>,pub instructions:Vec<Instruction>,pub direct_call_targets:BTreeSet<u64>}
#[derive(Clone,Copy)]pub struct EncodedCall{pub caller:u64,pub target:u64}
#[derive(Clone,Copy)]pub struct EncodedGlobalArgument{pub definition:u64,pub address:u64}
impl FileImage<'_>{
 // Host inspection methods are absent from the native module.
 #[cfg(not(feature="native-runtime"))]
 pub fn diagnostic_anchors(&self)->Result<Vec<(usize,u64)>,Error>{anchors(self)}
 #[cfg(not(feature="native-runtime"))]
 pub fn first_nonformable_rip_anchor_encoding(&self)->Option<(u64,u64)>{
  for span in self.spans.iter().filter(|s|s.flags&1!=0){for offset in 0..span.data.len().saturating_sub(6){let b=&span.data[offset..];if !(0x40..=0x4f).contains(&b[0])||b[1]!=0x8d||b[2]&0xc7!=5{continue;}let Ok(i)=decode(span,span.va+offset as u64)else{continue;};if i.mnemonic()==Mnemonic::Lea&&i.is_ip_rel_memory_operand()&&rip_target(span,&i).is_err(){return Some((i.ip(),i.ip_rel_memory_address()));}}}None
 }
 /// Necessary encoded first-argument prerequisite only; no caller instruction
 /// boundary, function identity or runtime global validity is inferred.
 pub fn writable_rdi_argument(&self,call:u64)->Result<Option<EncodedGlobalArgument>,Error>{
  let span=self.executable(call).ok_or(Error::Incomplete)?;
  Ok(argument(self,span,call)?.map(|(definition,address)|EncodedGlobalArgument{definition,address}))
 }
 pub fn encoded_call_targets_near(&self,anchors:&[u64],radius:u64,max_targets:usize)->Result<BTreeSet<u64>,Error>{
  if radius==0||radius>16*1024*1024||max_targets==0||max_targets>4096{return Err(Error::Incomplete);}
  let mut targets=BTreeSet::new();
  for s in self.spans.iter().filter(|s|s.flags&1!=0){for offset in 0..s.data.len().saturating_sub(4){if let Some((_,target))=call_target(s,offset){if self.executable(target).is_some()&&anchors.iter().any(|a|target<=*a&&*a-target<=radius){targets.insert(target);if targets.len()>max_targets{return Err(Error::Incomplete);}}}}}Ok(targets)
 }
 pub fn incoming_encoded_calls(&self,targets:&BTreeSet<u64>,max_calls:usize)->Result<Vec<EncodedCall>,Error>{
  if max_calls==0||max_calls>65536{return Err(Error::Incomplete);}let mut calls=vec![];
  for s in self.spans.iter().filter(|s|s.flags&1!=0){for offset in 0..s.data.len().saturating_sub(4){if let Some((caller,target))=call_target(s,offset){if targets.contains(&target){calls.push(EncodedCall{caller,target});if calls.len()>max_calls{return Err(Error::Incomplete);}}}}}Ok(calls)
 }
 pub fn bounded_cfg(&self,entry:u64,targets:&[u64],limits:CfgLimits)->Result<CfgEvidence,Error>{
  if limits.radius==0||limits.radius>16*1024*1024||limits.max_nodes==0||limits.max_nodes>65536{return Err(Error::Incomplete);}
  let s=self.executable(entry).ok_or(Error::Incomplete)?;let low=entry.saturating_sub(limits.radius).max(s.va);let high=entry.checked_add(limits.radius).ok_or(Error::Incomplete)?.min(s.va+s.data.len() as u64);let mut todo=vec![entry];let mut seen=BTreeSet::new();let mut output=CfgEvidence{entry,reached_targets:BTreeSet::new(),instructions:vec![],direct_call_targets:BTreeSet::new()};
  while let Some(ip)=todo.pop(){if ip<low||ip>=high{return Err(Error::Incomplete);}if !seen.insert(ip){continue;}if seen.len()>limits.max_nodes{return Err(Error::Incomplete);}let i=decode(s,ip)?;if targets.contains(&ip){output.reached_targets.insert(ip);}if i.mnemonic()==Mnemonic::Call&&i.op0_kind()==OpKind::NearBranch64{output.direct_call_targets.insert(i.near_branch_target());}
   match i.flow_control(){FlowControl::ConditionalBranch=>{todo.push(i.near_branch_target());todo.push(i.next_ip());},FlowControl::UnconditionalBranch=>{if i.op0_kind()!=OpKind::NearBranch64{return Err(Error::Incomplete);}todo.push(i.near_branch_target());},FlowControl::Next|FlowControl::Call|FlowControl::IndirectCall=>todo.push(i.next_ip()),FlowControl::Return|FlowControl::Exception=>{},_=>return Err(Error::Incomplete)}
   output.instructions.push(i);
  }Ok(output)
 }
}
#[derive(Clone)]struct Cfg{entry:u64,roles:BTreeSet<usize>,features:u32}
fn cfg(image:&FileImage<'_>,root:u64,anchors:&[(usize,u64)])->Result<Cfg,Error>{
 let targets:Vec<u64>=anchors.iter().map(|(_,at)|*at).collect();let decoded=image.bounded_cfg(root,&targets,CfgLimits::default())?;let mut roles=BTreeSet::new();for(role,anchor)in anchors{if decoded.reached_targets.contains(anchor){roles.insert(*role);}}
 let mut features=0;
 for i in decoded.instructions{
  if matches!(i.mnemonic(),Mnemonic::Mov|Mnemonic::Movsxd)&&i.op_count()>1&&i.op1_kind()==OpKind::Memory&&i.memory_displacement64()==12{features|=1;}
  if i.mnemonic()==Mnemonic::Shr&&i.op_count()>1&&i.op1_kind()==OpKind::Immediate8&&i.immediate8()==16{features|=2;}
  if i.mnemonic()==Mnemonic::Shl&&i.op_count()>1&&i.op1_kind()==OpKind::Immediate8&&i.immediate8()==3{features|=4;}
  if i.mnemonic()==Mnemonic::Lea&&i.memory_index_scale()==2&&i.memory_base()==i.memory_index()&&i.memory_base()!=Register::None{features|=8;}
  if i.mnemonic()==Mnemonic::Mov&&i.op_count()>1&&i.op0_kind()==OpKind::Memory&&i.memory_displacement64()==12&&i.op1_kind()==OpKind::Immediate32&&i.immediate32()==u32::MAX{features|=16;}
  if i.mnemonic()==Mnemonic::Mov&&i.op_count()>1&&i.op0_kind()==OpKind::Memory&&i.memory_displacement64()==12&&i.op1_kind()==OpKind::Register{features|=32;}

 }
 Ok(Cfg{entry:root,roles,features})
}
fn is_rdi(r:Register)->bool{matches!(r,Register::RDI|Register::EDI|Register::DI|Register::DIL)}
fn argument(image:&FileImage<'_>,s:&Span<'_>,call:u64)->Result<Option<(u64,u64)>,Error>{
 let begin=call.saturating_sub(128).max(s.va);let mut answers=BTreeSet::new();
 for start in begin..call{let mut ip=start;let mut value=None;let mut info=InstructionInfoFactory::new();while ip<call{let Ok(i)=decode(s,ip)else{break;};if i.next_ip()>call{break;}
  if i.mnemonic()==Mnemonic::Lea&&i.op0_kind()==OpKind::Register&&i.op0_register()==Register::RDI&&i.is_ip_rel_memory_operand(){let address=rip_target(s,&i)?;value=image.writable(address).then_some((ip,address));}
  else if !matches!(i.flow_control(),FlowControl::Next)||info.info(&i).used_registers().iter().any(|r|is_rdi(r.register())&&matches!(r.access(),OpAccess::Write|OpAccess::CondWrite|OpAccess::ReadWrite|OpAccess::ReadCondWrite)){value=None;}
  ip=i.next_ip();
 }if ip==call{if let Some(answer)=value{answers.insert(answer);}}}
 if answers.len()>1{return Err(Error::Ambiguous);}Ok(answers.first().copied())
}
/// Returns one operand-derived static file-image label only. It never reads the
/// candidate's file data or zero-fill tail, and supplies no runtime base/contents.
pub fn resolve_owned_file(bytes:&[u8])->Result<PsObjectArrayStaticCandidate,Error>{
 let image=FileImage::parse(bytes)?;resolve_image(&image)
}
/// Identical evidence matcher over supplied image spans; result scope is a
/// candidate only, never singleton identity, register dataflow or callable ABI.
pub(crate) fn resolve_image(image:&FileImage<'_>)->Result<PsObjectArrayStaticCandidate,Error>{
 let anchors=anchors(image)?;let anchor_addresses:Vec<u64>=anchors.iter().map(|(_,at)|*at).collect();let roots=image.encoded_call_targets_near(&anchor_addresses,RADIUS,ROOT_LIMIT)?;
 let mut matching=BTreeMap::new();let mut incomplete=false;
 for root in roots{match cfg(&image,root,&anchors){Ok(c) if !c.roles.is_empty()=>{matching.insert(root,c);},Ok(_)=>{},Err(_)=>{incomplete=true;}}}
 // An incomplete root may conceal another diagnostic path. Never promote a
 // unique-looking intersection while any searched call-target CFG is incomplete.
 let mut globals:BTreeMap<u64,Vec<(usize,u64,u64,u32)>>=BTreeMap::new();
 let matching_targets: BTreeSet<u64>=matching.keys().copied().collect();
 for encoded in image.incoming_encoded_calls(&matching_targets,4096)?{let call=encoded.caller;let target=encoded.target;let s=image.executable(call).ok_or(Error::Incomplete)?;let c=matching.get(&target).ok_or(Error::Incomplete)?;let Some((_,address))=argument(&image,s,call)?else{continue;};
  for role in &c.roles{let mask=if *role==0{46}else{31};if c.features&mask==mask{globals.entry(address).or_default().push((*role,c.entry,call,c.features));}}
 }
 let mut candidates=vec![];
 for(address,evidence)in globals{let allocation=evidence.iter().find(|x|x.0==0);let free=evidence.iter().find(|x|x.0==1);if let(Some(a),Some(f))=(allocation,free){if a.1==f.1{return Err(Error::Ambiguous);}candidates.push(PsObjectArrayStaticCandidate{address,allocation_entry:a.1,free_entry:f.1,allocation_call:a.2,free_call:f.2,evidence_flags:a.3|f.3,reserved:0});}}
 match candidates.len(){0=>Err(if incomplete{Error::Incomplete}else{Error::NoCandidate}),1=>if incomplete{Err(Error::Incomplete)}else{Ok(candidates.remove(0))},_=>Err(Error::Ambiguous)}
}
