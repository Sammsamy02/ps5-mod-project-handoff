use core::fmt::Debug;
use alloc::vec::Vec;
use scanner::Pattern;
use crate::{Addressable,MemoryTrait,resolvers::{AsyncContext,ResolveError,try_ensure_one}};
type Result<T> = core::result::Result<T,ResolveError>;
#[derive(Debug,PartialEq)]
pub struct GUObjectArray(pub u64);
pub fn resolve_guobject_array_patterns(ctx:&AsyncContext<'_>)->Result<GUObjectArray> {
    let patterns = [
        "8B 05 ?? ?? ?? ?? 3B 05 ?? ?? ?? ?? 75 ?? 48 8D 15 ?? ?? ?? ?? 48 8D 0D | ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 05",
        "74 ?? 48 8D 0D | ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01 E8 ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01",
        "75 ?? 48 ?? ?? 48 8D 0D | ?? ?? ?? ?? E8 ?? ?? ?? ?? 45 33 C9 4C 89 74 24",
        "45 84 c0 48 c7 41 10 00 00 00 00 b8 ff ff ff ff 4c 8d 1d | ?? ?? ?? ?? 89 41 08 4c 8b d1 4c 89 19 0f 45 05 ?? ?? ?? ?? ff c0 89 41 08 3b 05",
        "81 ce 00 00 00 02 83 e0 fb 89 47 08 48 8d 0d | ?? ?? ?? ?? 48 89 fa 45 31 c0 e8 ?? ?? ?? ??",
        "E8 ?? ?? ?? ?? 8B 05 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 03 0D | ?? ?? ?? ?? 29 C8 87 05",
    ];
    // mov imm32 pattern for linux
    let patterns1 = [
        /*
        06f97b32 41  39  ee       CMP        R14D ,EBP
        06f97b35 0f  8e  7e       JLE        LAB_06f97cb9
                 01  00  00
        06f97b3b bf  d8  c5       MOV        param_1 ,GUObjectArray
                 ac  0b
        06f97b40 48  8b  74       MOV        param_2 ,qword ptr [RSP  + local_68 ]
                 24  10
        06f97b45 e8  16  d2       CALL       FUN_06f64d60                                     undefined FUN_06f64d60()
                 fc  ff
        06f97b4a e9  6a  01       JMP        LAB_06f97cb9
                 00  00
         */
        "41 39 ee 0f 8e ?? ?? ?? ?? bf | ?? ?? ?? ?? 48 8b 74 24 10 e8 ?? ?? ?? ?? e9",
        /*
        06fa15c8 8b  6f  3c       MOV        EBP ,dword ptr [RDI  + 0x3c ]
        06fa15cb 4c  89  f7       MOV        RDI ,R14
        06fa15ce 31  f6           XOR        ESI ,ESI
        06fa15d0 e8  1b  09       CALL       FUN_06fa1ef0                                     undefined FUN_06fa1ef0()
                 00  00
        06fa15d5 41  39  ef       CMP        R15D ,EBP
        06fa15d8 7e  0d           JLE        LAB_06fa15e7
        06fa15da bf  d8  c5       MOV        EDI ,GUObjectArray
                 ac  0b
        06fa15df 48  89  de       MOV        RSI ,RBX
        06fa15e2 e8  79  37       CALL       FUN_06f64d60                                     undefined FUN_06f64d60()
                 fc  ff
         */
        "8b 6f ?? 4c 89 f7 31 f6 e8 ?? ?? ?? ?? 41 39 ef 7e 0d bf | ?? ?? ?? ?? 48 89 de e8",
    ];
    // `ObjObjects.Num() - ObjFirstGCIndex - <available count>` inlined into a stat update.
    // order and spacing depends on version so capture all three and match against known spacings
    let num_minus_available =
        Pattern::new("8B 05 [ ?? ?? ?? ?? ] 2B 05 [ ?? ?? ?? ?? ] 2B 05 [ ?? ?? ?? ?? ]").unwrap();

    let res0: Vec<_> = patterns.iter().map(|p| ctx.scan(Pattern::new(p).unwrap())).collect();
    let res1: Vec<_> = patterns1.iter().map(|p| ctx.scan(Pattern::new(p).unwrap())).collect();
    let res2 = ctx.scan_tagged((), num_minus_available);

    let res1 = res1
        .iter()
        .flatten()
        .map(|a| -> Result<_> { Ok(ctx.image().memory.u32_le(*a)? as u64) });

    let res2 = res2
        .2
        .iter()
        .map(|a| -> Result<_> {
            let Some(caps) = ctx.image().memory.captures(&res2.1, *a)? else {
                return Ok(None);
            };
            let mut operands = [caps[0].rip()?, caps[1].rip()?, caps[2].rip()?];
            operands.sort_unstable();
            Ok(base_from_operands(operands))
        })
        .collect::<Result<Vec<_>>>()?
        .into_iter()
        .flatten()
        .map(Ok);

    /// Given the three ascending operand addresses, return the `FUObjectArray` base they belong to
    fn base_from_operands(operands: [u64; 3]) -> Option<u64> {
        [[0x00, 0x24, 0x60], [0x00, 0x24, 0x68], [0x08, 0x20, 0x68]]
            .iter()
            .find_map(|layout| {
                let base = operands[0].checked_sub(layout[0])?;
                layout
                    .iter()
                    .zip(operands)
                    .all(|(offset, operand)| base.checked_add(*offset) == Some(operand))
                    .then_some(base)
            })
    }

    Ok(GUObjectArray(try_ensure_one(
        res0.iter()
            .flatten()
            .map(|a| -> Result<_> { Ok(ctx.image().memory.rip4(*a)?) })
            .chain(res1)
            .chain(res2),
    )?))
}

// Pattern lookup has priority. Only its exact empty-result error on ELF64 can
// enter this static serialized-file candidate inference. Preserve all original
// ambiguity/operand errors, and never feed a PE image to the ELF-only parser.
// CFG feature bits are bounded decoded evidence, not complete register dataflow
// proof, native singleton validation or inferred native function boundaries.
pub fn resolve_guobject_array(ctx:&AsyncContext<'_>)->Result<GUObjectArray>{
 match resolve_guobject_array_patterns(ctx){
  Err(original) if original.message=="expected at least one value" && ctx.image().kind==crate::ImageKind::Elf =>{
   match crate::object_cfg::resolve_owned_file(ctx.image().serialized_file_bytes()){
    Ok(candidate)=>Ok(GUObjectArray(candidate.address)),
    Err(crate::object_cfg::Error::NoCandidate)=>Err(original),
    Err(crate::object_cfg::Error::Ambiguous)=>Err(ResolveError::new_msg("found ambiguous static CFG object-array candidates")),
    // A valid section-only image can lack the PT_LOAD service required
    // by this optional matcher. Preserve the genuine original no-match.
    Err(crate::object_cfg::Error::Malformed)=>Err(original),
    Err(crate::object_cfg::Error::Incomplete)=>Err(ResolveError::new_msg("static CFG evidence is incomplete")),
   }
  },
  result=>result,
 }
}
