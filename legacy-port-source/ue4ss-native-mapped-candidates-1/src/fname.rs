use core::fmt::Debug;
use alloc::{format,vec::Vec,collections::BTreeSet as HashSet};
use scanner::Pattern;
use crate::{MemoryTrait,resolvers::{AsyncContext,ResolveError,ensure_one,try_ensure_one,Context},fname_util as util};
type Result<T> = core::result::Result<T,ResolveError>;
fn join_all<T>(values:impl IntoIterator<Item=T>)->Vec<T>{values.into_iter().collect()}
macro_rules! join {($($value:expr),* $(,)?)=>{($($value),*)};}
#[derive(Debug,PartialEq)]pub struct FNameCtorWchar(pub u64);
#[derive(Debug,PartialEq)]pub struct FNameToString(pub u64);
#[derive(Debug,PartialEq)]pub struct FNameToStringVoid(pub u64);
#[derive(Debug,PartialEq)]pub struct FNameToStringFString(pub u64);

pub fn resolve_ctor_elf(ctx:&AsyncContext<'_>)->Result<FNameCtorWchar> {
    use crate::resolvers::ResolveError;

    let strings = [
        "\0Engine\0",
        "\0Renderer\0",
        "\0AnimGraphRuntime\0",
        "\0Landscape\0",
        "\0RenderCore\0",
    ];

    // find the strings
    let strings = join_all(strings.iter().map(|s| ctx.scan(util::utf16_pattern(s))));
    let strings: Vec<Vec<_>> = strings
        .into_iter()
        .map(|pats| pats.into_iter().map(|addr| addr + 2).collect())
        .collect();
    //eprintln!("Find each pattern @ {:?}", strings);
    // find refs to them
    let refs: Vec<_> = join_all(strings.iter().map(|addr| util::scan_xrefs(ctx, addr)));
    //eprintln!("Find pattern refs @ {:?}", refs);
    let fns: Vec<_> = refs
        .into_iter()
        .flat_map(|addr| util::root_functions(ctx, &addr).ok())
        .collect();
    //eprintln!("Find pattern fns @ {:?}", fns);
    //strings.into_iter().map(|addr| async move { util::root_functions(ctx, &util::scan_xrefs(ctx, &addr) ) } ).collect();

    // find fns of these refs
    let fns = fns
        .into_iter()
        .reduce(|x, y| {
            let x: HashSet<u64> = HashSet::from_iter(x);
            let y: HashSet<u64> = HashSet::from_iter(y);
            x.intersection(&y).cloned().collect::<Vec<_>>()
        })
        .ok_or(ResolveError::new_msg("function-root groups unavailable"))?;

    // output fns
    //eprintln!("Found all fns at {:?}", fns);
    let fnLoadPreInitModules = ensure_one(fns)?;
    let pattern = Pattern::new("ba 01 00 00 00 e8 | ?? ?? ?? ??").unwrap();
    // found fLoadPreInitModules, try find target
    /*
        03f30310 53              PUSH       RBX
        03f30311 48  83  ec       SUB        RSP ,0x30
                 30
        03f30315 e8  c6  25       CALL       FUN_06c928e0                                     undefined FUN_06c928e0()
                 d6  02
        03f3031a 48  89  c3       MOV        RBX ,RAX
        03f3031d 48  8d  7c       LEA        RDI => local_10 ,[RSP  + 0x28 ]
                 24  28
        03f30322 be  38  8a       MOV        ESI ,u_Engine_00868a38                           = u"Engine"
                 86  00
        03f30327 ba  01  00       MOV        EDX ,0x1 <--- pat
                 00  00
        03f3032c e8  af  71       CALL       FName::FName     <- call                                void FName(undefined8 * this, us
                 dc  02
    */
    let mem = ctx
        .image()
        .memory
        .get_section_containing(fnLoadPreInitModules)?;
    let index = fnLoadPreInitModules - mem.address();
    let mut result = None;
    for i in 0..48 {
        if mem.matches(&pattern, usize::try_from(index.checked_add(i).ok_or(())?).map_err(|_|())?) {
            result = ctx
                .image()
                .memory
                .rip4(fnLoadPreInitModules.checked_add(i).and_then(|a|a.checked_add(pattern.custom_offset as u64)).ok_or(())?)
                .ok();
        }
    }
    // how to scan code from X?
    let result = result.ok_or(ResolveError::new_msg("cannot find address"))?;
    /*
    Post check
    if util::root_functions(ctx, &[result]).unwrap()[0] == result {
        eprintln!("ok!!");
    }
    */
    Ok(FNameCtorWchar(result))
}

pub fn resolve_ctor_pe(ctx:&AsyncContext<'_>)->Result<FNameCtorWchar> {
    use iced_x86::{Code, Decoder, DecoderOptions};

    // FName(wchar_t const*, EFindName) prologue
    let ctor = (|| {
        join_all([
            "48 89 5C 24 08 57 48 83 EC 30 48 8B D9 41 8B F8 33 C9 4C 8B DA 44 8B D1 4C 8B CA 48 85 D2 74 ?? 0F B7 02 66 85 C0",
            "48 89 5C 24 08 57 48 83 EC 30 48 8B D9 48 89 54 24 20 33 C9 41 8B F8 4C 8B D2 44 8B C9 48 85 D2 74 ?? 0F B7 02 66 85 C0",
        ].map(|p| ctx.scan(Pattern::new(p).unwrap())))
        
        .into_iter()
        .flatten()
        .collect::<Vec<_>>()
    })();
    let strings = (|| {
        let strings = ["TGPUSkinVertexFactoryUnlimited\0", "MovementComponent0\0"];
        join_all(strings.iter().map(|s| ctx.scan(util::utf16_pattern(s))))
    })();
    let patterns = (|| {
        ctx.scan(Pattern::new("EB 07 48 8D 15 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 41 B8 01 00 00 00 E8 | ?? ?? ?? ??").unwrap())
    })();
    let (ctor, patterns, strings) = join!(ctor, patterns, strings);

    // prefer the direct prologue match when present
    if !ctor.is_empty() {
        return Ok(FNameCtorWchar(try_ensure_one(ctor.into_iter().map(Ok))?));
    }

    // sometimes the call gets inlined so use patterns if any match
    if !patterns.is_empty() {
        return Ok(FNameCtorWchar(try_ensure_one(
            patterns
                .iter()
                .map(|a| -> Result<_> { Ok(ctx.image().memory.rip4(*a)?) }),
        )?));
    }

    #[derive(Clone, Copy)]
    enum Tag {
        Direct,
        FirstCall,
    }

    let refs = join_all(strings.iter().flatten().flat_map(|s| {
        [
            (
                Tag::FirstCall,
                format!("48 8d 15 X0x{s:x} 4c 8d 05 ?? ?? ?? ?? 41 b1 01 e8 | ?? ?? ?? ??"),
            ),
            (
                Tag::Direct,
                format!("48 8d 15 X0x{s:x} 48 8d 0d ?? ?? ?? ?? e8 | ?? ?? ?? ??"),
            ),
            (
                Tag::Direct,
                format!(
                    "41 b8 01 00 00 00 48 8d 15 X0x{s:x} 48 8d 0d ?? ?? ?? ?? e9 | ?? ?? ?? ??"
                ),
            ),
        ]
        .into_iter()
        .map(|(t, p)| ctx.scan_tagged2(t, Pattern::new(p).unwrap()))
    }))
    ;

    Ok(FNameCtorWchar(try_ensure_one(refs.iter().flatten().map(
        |(tag, address)| {
            let f = ctx.image().memory.rip4(*address)?;
            match tag {
                Tag::Direct => Ok(f),
                Tag::FirstCall => {
                    let bytes = ctx.image().memory.range(f..f.checked_add(0x200).ok_or(())?)?;
                    let mut decoder = Decoder::with_ip(64, bytes, f, DecoderOptions::NONE);

                    decoder
                        .iter()
                        .find_map(|i| {
                            (i.code() == Code::Call_rel32_64).then_some(i.memory_displacement64())
                        })
                        .context("did not find CALL instruction")
                }
            }
        },
    ))?))
}

pub fn resolve_to_string_elf(ctx:&AsyncContext<'_>)->Result<FNameToString> {
    let strings = ctx.scan(util::utf16_pattern("SkySphereMesh\0"));
    let str_addr = ensure_one(strings)?;
    let pattern = Pattern::new(format!(
        "e8 | ?? ?? ?? ?? 49 8b 5f 10 48 8d 7c 24 30 be 0x{str_addr:08x}"
    ))
    .map_err(|_|ResolveError::new_msg("ELF FName string address cannot be encoded in the genuine imm32 pattern"))?;
    let refs = ctx.scan(pattern);
    Ok(FNameToString(try_ensure_one(
        refs.into_iter().map(|a| Ok(ctx.image().memory.rip4(a)?)),
    )?))
}

pub fn resolve_to_string_pe(ctx:&AsyncContext<'_>)->Result<FNameToString> {
    use iced_x86::{Code, Decoder, DecoderOptions};

    let patterns = (|| {
        let patterns = ["56 57 48 83 EC 28 48 89 D6 48 89 CF 83 79 ?? 00 74"];

        join_all(patterns.iter().map(|p| ctx.scan(Pattern::new(p).unwrap())))
            
            .into_iter()
            .flatten()
            .collect::<Vec<_>>()
    })();

    let string = (|| {
        // Locates either variant by searching for a string ref and finding the first function
        // call directly above it. Which variant depends on how much inlining has occured
        let s = Pattern::from_bytes(
            "  DrivingBone: %s\nDrivenParamet"
                .encode_utf16()
                .flat_map(u16::to_le_bytes)
                .collect(),
        )
        .unwrap();
        let strings = ctx.scan(s);

        let refs = join_all(
            strings
                .iter()
                .map(|s| ctx.scan(Pattern::new(format!("48 8d 15 X0x{s:x}")).unwrap())),
        )
        ;

        let fn_gather_debug_data = ensure_one(
            refs.into_iter()
                .flatten()
                .map(|r| -> Result<_> {
                    Ok(ctx.image().get_root_function(r)?.map(|f| f.range.start..r))
                })
                .collect::<Result<Vec<_>>>()? // TODO avoid this collect?
                .into_iter()
                .flatten(),
        )?;

        let bytes = ctx.image().memory.range(fn_gather_debug_data.clone())?;

        let mut decoder = Decoder::with_ip(
            64,
            bytes,
            fn_gather_debug_data.start as u64,
            DecoderOptions::NONE,
        );

        let addr = decoder
            .iter()
            .filter_map(|i| (i.code() == Code::Call_rel32_64).then_some(i.memory_displacement64()))
            .last()
            .context("did not find CALL instruction")?;

        let res: Result<u64> = Ok(addr);

        res
    })();

    let any = join!(
        resolve_to_string_fstring(ctx),
        resolve_to_string_void(ctx),
        string,
        patterns,
    );

    // use pattern if found
    if !any.3.is_empty() {
        return Ok(FNameToString(ensure_one(any.3)?));
    }

    Ok(FNameToString(any.0.map(|r| r.0).or(any.1.map(|r| r.0)).or(any.2)?))
}

pub fn resolve_to_string_void(ctx:&AsyncContext<'_>)->Result<FNameToStringVoid> {
    let patterns = [
        "E8 | ?? ?? ?? ?? ?? 01 00 00 00 ?? 39 ?? 48 0F 8E",
        "E8 | ?? ?? ?? ?? BD 01 00 00 00 41 39 6E ?? 0F 8E",
        "E8 | ?? ?? ?? ?? 48 8B 4C 24 ?? 8B FD 48 85 C9",
    ];

    let res = join_all(patterns.iter().map(|p| ctx.scan(Pattern::new(p).unwrap())));

    Ok(FNameToStringVoid(try_ensure_one(
        res.iter()
            .flatten()
            .map(|a| -> Result<_> { Ok(ctx.image().memory.rip4(*a)?) }),
    )?))
}

pub fn resolve_to_string_fstring(ctx:&AsyncContext<'_>)->Result<FNameToStringFString> {
    let patterns =
        ["48 8b 48 ?? 48 89 4c 24 ?? 48 8d 4c 24 ?? e8 | ?? ?? ?? ?? 83 7c 24 ?? 00 48 8d"];

    let res = join_all(patterns.iter().map(|p| ctx.scan(Pattern::new(p).unwrap())));

    Ok(FNameToStringFString(try_ensure_one(
        res.iter()
            .flatten()
            .map(|a| -> Result<_> { Ok(ctx.image().memory.rip4(*a)?) }),
    )?))
}
