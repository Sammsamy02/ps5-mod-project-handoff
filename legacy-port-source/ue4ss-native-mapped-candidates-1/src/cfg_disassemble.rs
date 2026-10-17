use alloc::{vec::Vec,collections::BTreeSet};
use iced_x86::{Decoder,DecoderOptions,FlowControl,Instruction};
use crate::{OwnedImage as Image,MemoryTrait};
    pub enum Control {
        Continue,
        Break,
        Exit,
        Follow,
    }

    pub fn disassemble<'mem, 'img: 'mem, F>(
        exe: &'img Image<'mem>,
        address: u64,
        mut visitor: F,
    ) -> Result<(), ()>
    where
        F: FnMut(&Instruction) -> Result<Control, ()>,
    {
        struct Ctx<'mem, 'img: 'mem> {
            exe: &'img Image<'mem>,
            queue: Vec<u64>,
            visited: BTreeSet<u64>,
            address: u64,
            block: &'mem [u8],
            decoder: Decoder<'mem>,
            instruction: Instruction,
        }

        let block = exe.memory.range_from(address)?;
        let mut ctx = Ctx {
            exe,
            queue: Default::default(),
            visited: Default::default(),
            address,
            block,
            decoder: Decoder::with_ip(64, block, address, DecoderOptions::NONE),
            instruction: Default::default(),
        };

        impl Ctx<'_, '_> {
            fn start(&mut self, address: u64) -> Result<(), ()> {
                //println!("starting at {address:x}");
                self.address = address;
                self.block = self.exe.memory.range_from(self.address)?;
                self.decoder = Decoder::with_ip(64, self.block, self.address, DecoderOptions::NONE);
                Ok(())
            }
            /// Returns true if pop was successful
            fn pop(&mut self) -> Result<bool, ()> {
                Ok(if let Some(next) = self.queue.pop() {
                    self.start(next)?;
                    true
                } else {
                    false
                })
            }
        }

        while ctx.decoder.can_decode() {
            ctx.decoder.decode_out(&mut ctx.instruction);

            let addr = ctx.instruction.ip();

            if ctx.visited.contains(&addr) {
                if ctx.pop()? {
                    continue;
                } else {
                    break;
                }
            } else {
                ctx.visited.insert(addr);
                match visitor(&ctx.instruction)? {
                    Control::Continue => {}
                    Control::Break => {
                        if ctx.pop()? {
                            continue;
                        } else {
                            break;
                        }
                    }
                    Control::Exit => {
                        break;
                    }
                    Control::Follow => {
                        // Follow the instruction's target immediately (depth-first)
                        let target = match ctx.instruction.flow_control() {
                            FlowControl::Call
                            | FlowControl::UnconditionalBranch
                            | FlowControl::ConditionalBranch => {
                                ctx.instruction.near_branch_target()
                            }
                            _ => {
                                continue;
                            }
                        };

                        ctx.queue.push(ctx.instruction.next_ip());

                        ctx.start(target)?;
                        continue;
                    }
                }
            }

            /*
            if !matches!(ctx.instruction.flow_control(), FlowControl::Next) {
                //println!();
            }
            ctx.print();
            if !matches!(ctx.instruction.flow_control(), FlowControl::Next) {
                //println!();
            }
            */

            match ctx.instruction.flow_control() {
                FlowControl::Next => {}
                FlowControl::UnconditionalBranch => {
                    // TODO figure out how to handle tail calls
                    ctx.start(ctx.instruction.near_branch_target())?;
                }
                //FlowControl::IndirectBranch => todo!(),
                FlowControl::ConditionalBranch => {
                    ctx.queue.push(ctx.instruction.near_branch_target());
                }
                FlowControl::Return if !ctx.pop()? => {
                    break;
                }
                //FlowControl::Call => todo!(),
                //FlowControl::IndirectCall => todo!(),
                //FlowControl::Interrupt => todo!(),
                //FlowControl::XbeginXabortXend => todo!(),
                //FlowControl::Exception => todo!(),
                _ => {}
            }
        }
        Ok(())
    }

#[cfg(test)]mod cfg_owned_controls {
 use super::*;
 use crate::{OwnedImage,OwnedMemory,OwnedSection,ImageKind,function_ranges::MetadataError};
 fn image(bytes:&[u8])->OwnedImage<'_>{OwnedImage{serialized_bytes:&[],memory:OwnedMemory{sections:alloc::vec![OwnedSection{address:0x1000,data:bytes}]},kind:ImageKind::Elf,function_ranges:Err(MetadataError::Unavailable)}}
 #[test]fn conditional_branches_and_returns_follow_real_decoder_edges(){
  let bytes=[0x75,3,0x90,0xc3,0x90,0x90,0xc3];let img=image(&bytes);let mut seen=Vec::new();disassemble(&img,0x1000,|i|{seen.push(i.ip());Ok(Control::Continue)}).unwrap();assert_eq!(seen,[0x1000,0x1002,0x1003,0x1005,0x1006]);
 }
 #[test]fn follow_visits_call_target_then_actual_fallthrough(){
  let bytes=[0xe8,1,0,0,0,0xc3,0x90,0xc3];let img=image(&bytes);let mut seen=Vec::new();disassemble(&img,0x1000,|i|{seen.push(i.ip());Ok(if i.ip()==0x1000{Control::Follow}else{Control::Continue})}).unwrap();assert_eq!(seen,[0x1000,0x1006,0x1007,0x1005]);
 }
 #[test]fn loop_dedup_and_owned_span_bounds_stop_traversal(){
  let bytes=[0xeb,0xfe];let img=image(&bytes);let mut count=0;disassemble(&img,0x1000,|_|{count+=1;Ok(Control::Continue)}).unwrap();assert_eq!(count,1);
  let bytes=[0xe9,0x7f,0,0,0];assert!(disassemble(&image(&bytes),0x1000,|_|Ok(Control::Continue)).is_err());
 }
}
