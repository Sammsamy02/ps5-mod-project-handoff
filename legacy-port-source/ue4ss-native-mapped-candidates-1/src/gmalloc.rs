use core::fmt::Debug;
use alloc::{vec,vec::Vec,collections::BTreeSet as HashSet};
use itertools::Itertools;
use scanner::Pattern;
use iced_x86::{Code,OpKind,Register};
use crate::{OwnedImage as Image,MemoryTrait,disassemble::{Control,disassemble},resolvers::{AsyncContext,ResolveError,try_ensure_one},required_util as util};
type Result<T> = core::result::Result<T,ResolveError>;
macro_rules! join {($($v:expr),* $(,)?)=>{($($v),*)};}
macro_rules! try_join {($a:expr,$b:expr $(,)?)=>{{let a=$a;let b=$b;Ok::<_,ResolveError>((a?,b?))}};}
#[derive(Debug,PartialEq)]pub struct GMalloc(pub u64);
#[derive(Debug,PartialEq)]pub struct GMallocPatterns(pub u64);
#[derive(Debug,PartialEq)]pub struct GMallocString(pub u64);
pub fn resolve_gmalloc(ctx:&AsyncContext<'_>)->Result<GMalloc> {
    //eprintln!("GMalloc Scan Start!");
    let (patterns, strings) = join!(
        resolve_gmalloc_patterns(ctx),
        resolve_gmalloc_string(ctx),
    );
    Ok(GMalloc(patterns.map(|r| r.0).or(strings.map(|r| r.0))?))
}
pub fn resolve_gmalloc_patterns(ctx:&AsyncContext<'_>)->Result<GMallocPatterns> {
    let patterns = [
        "48 ?? ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b", // Purgatory
        "eb 03 ?? 8b ?? 48 8b ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b", // Purg_notX
        "e8 ?? ?? ?? ?? 48 8b ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b", // Purg_withX
        "48 85 C9 74 2E 53 48 83 EC 20 48 8B D9 48 8B ?? | ?? ?? ?? ?? 48 85 C9",          // A
        "75 ?? E8 ?? ?? ?? ?? 48 8b 0d | ?? ?? ?? ?? 48 8b ?? 48 ?? ?? ff 50 ?? 48 83 c4 ?? ?? c3", // bnew1
        "48 85 C9 74 ?? 4C 8B 05 | ?? ?? ?? ?? 4D 85 C0 0F 84", // altshort
        "48 ?? ?? ?? ?? ?? ?? e8 ?? ?? ?? ?? 48 8b 0d | ?? ?? ?? ?? 48 8b 01 ff 50 ?? 84 c0 75 ?? b9 38 00 00 00", // gcreatemallocshort
        "84 C0 75 ?? B9 38 00 00 00 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 85 c0 74 ?? 48 8b 0d | ?? ?? ?? ?? 48 8d 05 ?? ?? ?? ?? 48 89", // gcreatemallocmiddle
        "ff 15 ?? ?? ?? ?? 48 8b 5c 24 ?? 48 89 3d | ?? ?? ?? ?? 48 8b 7c 24 20 48 83 c4 28 c3", // gcreatemallocend
        "48 89 ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 48 39 ?? 74 ?? 48 8b 1d", // clang1
        "48 89 ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 48 39 ?? 75 ?? 48 83 c4", // clang2
        "48 89 5C 24 08 57 48 83 EC 20 48 8B F9 ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 85 C9 75 ?? E8 ?? ?? ?? FF 48 8B 0D | ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 ?? ?? ?? ?? 48",
        "48 89 5C 24 08 57 48 83 EC ?? 48 83 3D ?? ?? ?? ?? 00 8B DA 48 8B F9 75 07 E8 ?? ?? ?? FF EB 07 33 C9 E8 ?? ?? ?? FF 48 8B 0D | ?? ?? ?? ?? 44 8B C3 48 8B D7 48 8B 01 FF 50 10 80 3D ?? ?? ?? ?? 00 48 8B D8 75 ?? 48 8B 05 ?? ?? ?? ?? 48 85 C0 75 05 E8 ?? ?? ?? FF ?? 44 24 ?? 01",
        "48 89 5C 24 08 57 48 83 EC 20 48 8B F9 8B DA 48 8B 0D | ?? ?? ?? ?? 48 85 C9 75 2E 65 48 8B 04 25 58 00 00 00 44 8B 05 ?? ?? ?? ?? BA 18 00 00 00 4E 8B 04 C0 42 8B 04 02 39 05 ?? ?? ?? ?? 7E 09 EB 1E 48 8B 0D ?? ?? ?? ?? 48 8B 01 44 8B C3 48 8B D7 48 8B 5C 24 30 48 83 C4 20 5F 48 FF 60 10 48 8D 0D",
    ];

    let res = ctx.scan_many(&patterns.iter().map(|p| Pattern::new(p).unwrap()).collect::<Vec<_>>());

    Ok(GMallocPatterns(try_ensure_one(res.iter().flatten().map(
        |a| -> Result<_> { Ok(ctx.image().memory.rip4(*a)?) },
    ))?))
}
pub fn resolve_gmalloc_string_pe(ctx:&AsyncContext<'_>)->Result<GMallocString> {
    
    use iced_x86::FlowControl;
    

    let strings = ctx.scan(util::utf16_pattern("DeleteFile %s\0"));
    let refs = util::scan_xrefs(ctx, &strings);

    let fns = util::root_functions(ctx, &refs)?;

    fn find_global(
        img: &Image<'_>,
        f: u64,
        depth: usize,
        searched: &mut HashSet<u64>,
    ) -> Result<Option<u64>> {
        searched.insert(f);

        //println!("searching {f:x?}");

        let mut mov_rcx = None;
        let mut possible_gmalloc = vec![];
        let mut calls = vec![];

        disassemble(img, f, |inst| {
            let cur = inst.ip();
            if !(f..f.checked_add(1000).ok_or(())?).contains(&cur)
                && Some(f) != img.get_root_function(cur)?.map(|f| f.range.start)
            {
                //println!("bailing at {:x}", inst.ip());
                return Ok(Control::Break);
            }

            if inst.code() == Code::Cmp_rm64_imm8
                && inst.memory_base() == Register::RIP
                && inst.op0_kind() == OpKind::Memory
                && inst.op1_kind() == OpKind::Immediate8to64
                && inst.immediate8() == 0
            {
                possible_gmalloc.push(inst.ip_rel_memory_address());
            }

            if inst.code() == Code::Test_rm64_r64
                && inst.op0_register() == Register::RCX
                && inst.op1_register() == Register::RCX
                && let Some(mov_rcx) = mov_rcx
            {
                possible_gmalloc.push(mov_rcx);
            }

            if inst.code() == Code::Mov_r64_rm64
                && inst.memory_base() == Register::RIP
                && inst.op0_register() == Register::RCX
            {
                /*
                println!(
                    "{depth} {:x} {:x} {:x?}",
                    inst.ip(),
                    inst.ip_rel_memory_address(),
                    inst
                );
                */
                mov_rcx = Some(inst.ip_rel_memory_address());
            } else {
                mov_rcx = None;
            }

            match inst.flow_control() {
                FlowControl::Call
                | FlowControl::ConditionalBranch
                | FlowControl::UnconditionalBranch => {
                    let call = inst.near_branch_target();
                    //println!("{:x} {:x}", inst.ip(), call);
                    if Some(f) != img.get_root_function(call)?.map(|f| f.range.start) {
                        calls.push(call);
                    }
                }
                _ => {}
            }

            Ok(Control::Continue)
        })?;

        if let [gmalloc] = possible_gmalloc.as_slice() {
            Ok(Some(*gmalloc))
        } else {
            if depth > 0 {
                for call in calls.iter().rev() {
                    if !searched.contains(call) {
                        let res = find_global(img, *call, depth - 1, searched)?;
                        if res.is_some() {
                            return Ok(res);
                        }
                    }
                }
            }
            Ok(None)
        }
    }

    let fns = fns
        .into_iter()
        .map(|f| find_global(ctx.image(), f, 3, &mut Default::default()))
        .flatten_ok();

    Ok(GMallocString(try_ensure_one(fns)?))
}
pub fn resolve_gmalloc_string_elf(ctx:&AsyncContext<'_>)->Result<GMallocString> {
    
    use core::ops::Range;

    //eprintln!("GMalloc String Scan");
    let string_xref_used_by = |pattern: &'static str| {
        let strings = ctx.scan(util::utf8_pattern(pattern));
        //eprintln!("Found /proc/meminfo @ {:?} ", strings);
        let refs = util::scan_xrefs(ctx, &strings);
        //eprintln!("Found {} refs", refs.len());

        let fns = util::root_functions(ctx, &refs)?;
        //eprintln!("Found related functions @ {:?}", fns);

        Result::<Vec<_>>::Ok(util::scan_xcalls(ctx, &fns))
    };

    let find_string_pattern1 = || { string_xref_used_by("/proc/meminfo\0") };

    let find_string_pattern2 = || {
        let fns2 = string_xref_used_by("Refusing to run with the root privileges.\n\0")?;
        //eprintln!("Found {} xcall fns2 @ {:?}", fns2.len(), fns2);
        let fns2 = fns2.iter().map(|&x| x.checked_add(24).map(|end| x..end).ok_or_else(||ResolveError::new_msg("serialized xcall window overflows"))).collect::<Result<Vec<_>>>()?;
        // another possible address for FMemory::GCreateMalloc
        Result::<Vec<Range<_>>>::Ok(fns2)
    };

    let (fns, fns2) = try_join!(find_string_pattern1(), find_string_pattern2())?;

    let fns = fns
        .into_iter()
        .filter(|x| fns2.iter().any(|y| y.contains(x)))
        .map(|f| -> Result<Option<u64>> {
            let mut possible_gmalloc = vec![];
            // eprintln!("disassemble @ {}", f);
            disassemble(ctx.image(), f, |inst| {
                let cur = inst.ip();
                if !(f..f.checked_add(20).ok_or(())?).contains(&cur) {
                    return Ok(Control::Break);
                }

                // find mov rdi
                if inst.code() == Code::Mov_r64_rm64
                    && inst.memory_base() == Register::RIP
                    && inst.op0_kind() == OpKind::Register
                    && inst.op1_kind() == OpKind::Memory
                {
                    // eprintln!("Found one possible gmlaaoc @ {:#08X}", inst.ip_rel_memory_address() as usize);
                    possible_gmalloc.push(inst.ip_rel_memory_address());
                }
                Ok(Control::Continue)
            })?;
            Ok(
                (possible_gmalloc.len() == 2 && possible_gmalloc[0] == possible_gmalloc[1])
                    .then(|| possible_gmalloc[0]),
            )
        })
        .flatten_ok();

    Ok(GMallocString(try_ensure_one(fns)?))
}
pub fn resolve_gmalloc_string(ctx:&AsyncContext<'_>)->Result<GMallocString>{match ctx.image().kind{crate::ImageKind::Elf=>resolve_gmalloc_string_elf(ctx),crate::ImageKind::PE=>resolve_gmalloc_string_pe(ctx)}}
