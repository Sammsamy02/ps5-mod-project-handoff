use alloc::{vec,vec::Vec};
use iced_x86::FlowControl;
use crate::{OwnedImage as Image,disassemble::{Control,disassemble},resolvers::ResolveError};
type Result<T> = core::result::Result<T,ResolveError>;
pub use crate::fname_util::root_functions;
pub use crate::util::{utf16,utf16_pattern};
pub use crate::xrefs::{scan_xrefs,scan_xcalls};
pub fn utf8_pattern(s:&str)->scanner::Pattern{scanner::Pattern::from_bytes(s.as_bytes().to_vec()).unwrap()}
#[derive(Debug,Clone,Copy)]pub struct Call{pub index:usize,pub ip:u64,pub callee:u64}
    pub fn find_calls(img: &Image<'_>, f: u64) -> Result<Vec<Call>> {
        let mut calls = vec![];

        disassemble(img, f, |inst| {
            let cur = inst.ip();
            if Some(f) != img.get_root_function(cur)?.map(|f| f.range.start) {
                return Ok(Control::Break);
            }

            match inst.flow_control() {
                FlowControl::Call
                | FlowControl::ConditionalBranch
                | FlowControl::UnconditionalBranch => {
                    let call = inst.near_branch_target();
                    if Some(f) != img.get_root_function(call)?.map(|f| f.range.start) {
                        calls.push(Call {
                            index: 0,
                            ip: inst.ip(),
                            callee: call,
                        });
                    }
                }
                _ => {}
            }

            Ok(Control::Continue)
        })?;

        calls.sort_by_key(|c| c.ip);
        for (i, call) in calls.iter_mut().enumerate() {
            call.index = i;
        }
        Ok(calls)
    }

