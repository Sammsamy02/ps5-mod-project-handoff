use core::{
    
    fmt::{Debug, Display},
    str::FromStr,
};

use alloc::{vec::Vec, format, string::ToString};

use itertools::Itertools;
use scanner::Pattern;

use crate::resolvers::ensure_one;
use crate::util;
use crate::{
    MemoryTrait,
    resolvers::{
        AsyncContext, ResolveError,
    },
};

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct EngineVersion {
    pub major: u16,
    pub minor: u16,
}
impl Display for EngineVersion {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{}.{}", self.major, self.minor)
    }
}
impl Debug for EngineVersion {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "EngineVersion({}.{})", self.major, self.minor)
    }
}
impl FromStr for EngineVersion {
    type Err = ResolveError;
    fn from_str(s: &str) -> core::result::Result<Self, Self::Err> {
        let (major, minor) = s
            .split_once('.')
            .ok_or(ResolveError::new_msg("expected <major>.<minor e.g.: 5.4"))?;
        let major = major
            .parse::<u16>()
            .map_err(|e| ResolveError::new_msg(e.to_string()))?;
        let minor = minor
            .parse::<u16>()
            .map_err(|e| ResolveError::new_msg(e.to_string()))?;
        Ok(Self { major, minor })
    }
}

#[rustfmt::skip]
const VERSION_PATTERNS: &[&str] = &[
    "C7 47 20 | 04 00 ?? 00 66 89 6F 24",
    "C7 4? 20 | 04 00 ?? ?? 66 4? 89 ?? 24",
    "C7 ?? 24 20 | 04 00 ?? ?? 48 8D 45 F0",
    "C7 05 ?? ?? ?? ?? | 04 00 ?? 00 66 89 ?? ?? ?? ?? ?? C7 05",
    "C7 05 ?? ?? ?? ?? | 04 00 ?? 00 66 89 ?? ?? ?? ?? ?? 89",
    "41 C7 ?? | 04 00 ?? 00 ?? ?? 00 00 00 66 41 89",
    "41 C7 ?? | 04 00 ?? 00 66 41 89 ?? 04",
    "41 C7 04 24 | 04 00 ?? 00 66 ?? 89 ?? 24",
    "41 C7 04 24 | 04 00 ?? 00 B9 ?? 00 00 00",
    "41 C7 44 24 20 | 04 00 ?? 00 66 ?? 89 ?? 24",
    "41 C7 ?? 20 | 04 00 ?? 00 41 89 ?? 28",
    "41 C7 ?? | 04 00 ?? 00 66 41 C7 4? 04",
    "C7 05 ?? ?? ?? ?? | 04 00 ?? 00 89 3D ?? ?? ?? ?? 85 FF",
    "C7 05 ?? ?? ?? ?? | 04 00 ?? 00 89 05 ?? ?? ?? ?? E8",
    "C7 05 ?? ?? ?? ?? | 04 00 ?? 00 66 89 ?? ?? ?? ?? ??",
    "C7 46 20 | 04 00 ?? 00 66 44 89 76 24 44 89 76 28 48 39 C7",
    "C7 03 | 04 00 ?? 00 66 44 89 63 04 C7 43 08 C1 5C 08 80 E8",
    "C7 47 20 | 04 00 ?? 00 66 89 6F 24 C7 47 28 ?? ?? ?? ?? 49",
    "C7 03 | 04 00 ?? 00 66 89 6B 04 89 7B 08 48 83 C3 10",
    "41 C7 06 | 05 00 ?? ?? 48 8B 5C 24 ?? 49 8D 76 ?? 33 ED 41 89 46",
    "C7 06 | 05 00 ?? ?? 48 8B 5C 24 20 4C 8D 76 10 33 ED",
    "11 76 30 c7 46 20 | 04 00 ?? 00",
    "0F 57 C0 0F 11 43 10 C7 03 | 05 ?? ?? ?? 66 C7 43 04 ?? ??", // <- last one is patch
    "48 89 2? 48 89 6? 08 C7 0? | 05 00 ?? ?? 66",
    "49 89 2? 49 89 6? 08 C7 0? | 05 00 ?? ?? 66",
    "C7 46 20 | 05 00 ?? ?? 66 89 ?? 24",
    "C7 43 20 | 05 00 ?? ?? 48 3B F0",
    "C7 46 20 | 05 00 ?? ?? 48 8D 44 24 20",
    "C7 4? 20 | 05 00 ?? ?? 66 44 89 ?? 24",
    "C7 ?? 24 20 | 05 00 ?? ?? 48 8D 45 F0",
    "C7 06 | 05 00 ?? 00 66 C7 46 04",
    "0F B6 D8 C1 E3 1F E8 ?? ?? ?? ?? 0B C3 C7 06 | 05 00 ?? 00",
    "0F B6 D8 C1 E3 1F E8 ?? ?? ?? ?? 0B C3 C7 06 | 04 00 ?? 00",
    "0F B6 D8 C1 E3 1F E8 ?? ?? ?? ?? 33 ED C7 06 | 05 00 ?? 00",
    "89 2E 89 6E 08 48 8D 4E 0C 89 29 41 C7 07 | 05 00 ?? 00",
];

#[rustfmt::skip]
const MOV_RM32_IMM32: &[&str] = &[
    "C7 000000??",              // [reg]
    "C7 0000011?",              // [reg]
    "C7 04 ??",                 // [reg+reg*n]
    "C7 05 ?? ?? ?? ??",        // [rip+disp32]
    "C7 010000?? ??",           // [reg+disp8]
    "C7 01000101 ??",           // [rbp+disp8]
    "C7 0100011? ??",           // [reg+disp8]
    "C7 44 ?? ??",              // [reg+reg*n+disp8]
    "C7 100000?? ?? ?? ?? ??",  // [reg+disp32]
    "C7 10000101 ?? ?? ?? ??",  // [rbp+disp32]
    "C7 1000011? ?? ?? ?? ??",  // [reg+disp32]
    "C7 84 ?? ?? ?? ?? ??",     // [reg+reg*n+disp32]
];

fn plausible(ver: &EngineVersion) -> bool {
    match ver.major {
        4 => (0..=27).contains(&ver.minor),
        5 => (0..=20).contains(&ver.minor),
        _ => false,
    }
}

fn scan_version_stores(ctx: &crate::resolvers::AsyncContext<'_>) -> Vec<EngineVersion> {
    use crate::disassemble::disassemble_single;
    use iced_x86::{Code, Instruction, OpKind};

    // the `66` is the operand size prefix of the `Patch` store that must follow
    let patterns = MOV_RM32_IMM32
        .iter()
        .flat_map(|s| {
            [
                format!("{s} 0? 00 ?? 00 66"),
                format!("41 {s} 0? 00 ?? 00 66"),
            ]
        })
        .collect_vec();

    let res: Vec<_> = patterns.iter().map(|p| ctx.scan(Pattern::new(p).unwrap())).collect();

    let img = ctx.image();
    res.iter()
        .flatten()
        .filter_map(|&addr| {
            let store = disassemble_single(img, addr).ok()??;
            if store.code() != Code::Mov_rm32_imm32 || store.op0_kind() != OpKind::Memory {
                return None;
            }
            let (base, index, disp) = (
                store.memory_base(),
                store.memory_index(),
                store.memory_displacement64(),
            );
            let writes = |inst: &Instruction, field: u64| {
                inst.op0_kind() == OpKind::Memory
                    && inst.memory_base() == base
                    && inst.memory_index() == index
                    && inst.memory_displacement64() == disp.wrapping_add(field)
            };

            // FEngineVersionBase is 4 byte aligned
            if disp % 4 != 0 {
                return None;
            }

            let mut ip = addr + store.len() as u64;
            let patch = disassemble_single(img, ip).ok()??;
            if !writes(&patch, 4)
                || match patch.code() {
                    Code::Mov_rm16_r16 => false,
                    Code::Mov_rm16_imm16 => patch.immediate16() > 30,
                    _ => true,
                }
            {
                return None;
            }

            // `Changelist` follows shortly after, but not always adjacently.
            // Without it every `mov dword ptr [X], 5` in the binary looks like a 5.0 candidate.
            ip += patch.len() as u64;
            if !(0..4).any(|_| match disassemble_single(img, ip).ok().flatten() {
                Some(inst) if writes(&inst, 8) => true,
                Some(inst) => {
                    ip += inst.len() as u64;
                    false
                }
                None => false,
            }) {
                return None;
            }

            let imm = store.immediate32();
            Some(EngineVersion {
                major: imm as u16,
                minor: (imm >> 16) as u16,
            })
        })
        .filter(|ver| plausible(ver) && ver.minor != 0)
        .collect()
}

fn scan_version_pattern_matches(
    ctx: &crate::resolvers::AsyncContext<'_>,
) -> Vec<EngineVersion> {
    let res: Vec<_> = VERSION_PATTERNS.iter().map(|p| ctx.scan(Pattern::new(p).unwrap())).collect();

    let mem = &ctx.image().memory;
    res.iter()
        .flatten()
        .filter_map(|a| {
            Some(EngineVersion {
                major: mem.u16_le(*a).ok()?,
                minor: mem.u16_le(a + 2).ok()?,
            })
        })
        // TODO 4.0 can false positive so ignore it. need to harden if this is to work on 4.0 games
        .filter(|ver| plausible(ver) && !(ver.major == 4 && ver.minor == 0))
        .collect()
}

fn scan_engine_version_string(
    ctx: &crate::resolvers::AsyncContext<'_>,
) -> Vec<EngineVersion> {
    // wide "<digit>.<digit(s)>." at the start of a string, i.e. preceded by the NUL terminating whatever came before it
    let patterns = [
        "00 00 | 0011???? 00 2E 00 0011???? 00 2E 00",
        "00 00 | 0011???? 00 2E 00 0011???? 00 0011???? 00 2E 00",
    ];
    let res: Vec<_> = patterns.iter().map(|p| ctx.scan(Pattern::new(p).unwrap())).collect();

    let mem = &ctx.image().memory;
    res.iter()
        .flatten()
        .filter_map(|&a| {
            let s = mem.read_wstring(a).ok()?;
            let (major, rest) = s.split_once('.')?;
            let (minor, rest) = rest.split_once('.')?;
            let (patch, build_version) = rest.split_once('-')?;
            patch.parse::<u16>().ok()?;
            // "<branch>-CL-<changelist>" or "<changelist>+<branch>"
            if !build_version.contains('+') && !build_version.contains("-CL-") {
                return None;
            }
            Some(EngineVersion {
                major: major.parse().ok()?,
                minor: minor.parse().ok()?,
            })
        })
        .filter(plausible)
        .collect()
}

fn scan_branch_name(ctx: &crate::resolvers::AsyncContext<'_>) -> Vec<EngineVersion> {
    const BRANCHES: &[&str] = &["++UE4+Release-", "++UE5+Release-", "++depot+UE4-Releases+"];
    let res: Vec<_> = BRANCHES.iter().map(|b| (b.len(), ctx.scan(util::utf16_pattern(b)))).collect();

    let mem = &ctx.image().memory;
    res.iter()
        .flat_map(|(prefix, addresses)| addresses.iter().map(move |a| (*prefix, *a)))
        .filter_map(|(prefix, a)| {
            let s = mem.read_wstring(a).ok()?;
            let version = s.get(prefix..)?;
            // trailing junk is expected, e.g. "++UE4+Release-4.25Plus"
            let (major, rest) = split_number(version)?;
            let (minor, _) = split_number(rest.strip_prefix('.')?)?;
            Some(EngineVersion { major, minor })
        })
        .filter(plausible)
        .collect()
}

fn split_number(s: &str) -> Option<(u16, &str)> {
    let end = s.find(|c: char| !c.is_ascii_digit()).unwrap_or(s.len());
    Some((s[..end].parse().ok()?, &s[end..]))
}

pub fn resolve_engine_version(ctx: &AsyncContext<'_>) -> Result<EngineVersion, ResolveError> {
    let version_string = scan_engine_version_string(ctx);
    let stores = scan_version_stores(ctx);
    let patterns = scan_version_pattern_matches(ctx);
    let branch_name = scan_branch_name(ctx);

    let patterns = ensure_one(patterns);
    for candidates in [version_string, stores] {
        if let Ok(version) = ensure_one(candidates) {
            return Ok(version);
        }
    }
    if patterns.is_ok() {
        return patterns;
    }
    ensure_one(branch_name).or(patterns)
}
