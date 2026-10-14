//! FC-1 calls_by_anchor materialization (worker-fname-fc1-prep).
//!
//! READ-ONLY over the frozen capture: one std::fs::read, no writes, no
//! network, no engine execution. Bounded: single linear scans, fixed caps
//! (MAX_STRINGS_PER_ANCHOR=64, MAX_XREF_RElevant windows<=64 starts per call
//! site, MAX_CALL_SITES=4096), constant memory ~ capture size.
//!
//! Calls the survey resolver UNMODIFIED:
//!   .superpowers/native-dependency-spike/ue4ss-native-tick-relocation-resolver-1/
//!     crate/src/fname_call.rs::resolve()  (pub fn, line 51)
//! The survey FFI ps_fname_ctor_call_file_image (lib.rs:299-304) builds
//! OwnedImage::parse(bytes) + resolvers::AsyncContext{exe:&image} internally,
//! then writes ONLY Candidate.address -- calls_by_anchor is dropped at the FFI
//! boundary. AsyncContext's field is pub(crate) (lib.rs:21) so an external
//! crate cannot construct it; this binary instead reinterprets a same-layout
//! single-reference wrapper with a compile-time size assert (both are one
//! thin &OwnedImage). No resolver byte is changed; raw provenance preserved.
//!
//! Byte/annotation layer (own logic, clearly labeled in output):
//!  - PT_LOAD p_vaddr->file-offset map to dump exact bytes at 0x23d590 and at
//!    each materialized call site (read-only);
//!  - window reconstruction that RE-APPLIES the resolver's own call_after
//!    constraints (fname_call.rs:27-49: first instr LEA RSI,[rip+disp32] to a
//!    scanned anchor string; branch-free Next flow; RSI never rewritten; RDI
//!    output = LEA RDI,[RSP|RBP] no index; EDX=1; terminal E8 rel32) to pin
//!    the window start/anchor string of each call site for the FC-1 spec.
use std::os::unix::io::AsRawFd;
use patternsleuth_native_engine_version::{fname_call, OwnedImage};
use patternsleuth_native_engine_version::resolvers::AsyncContext;
use iced_x86::{Decoder, DecoderOptions, FlowControl, InstructionInfoFactory, Mnemonic, OpAccess, OpKind, Register};
use std::io::Write as _;

const NAMES: [&str; 5] = ["Engine", "Renderer", "AnimGraphRuntime", "Landscape", "RenderCore"]; // must match fname_call.rs NAMES order
const TARGET_EXPECT: u64 = 0x23d590; // frozen survey run-release-1: fname_ctor_call_candidate status=0 linked=0x23d590
const LOAD0_RUNTIME_BASE: u64 = 0x400000; // proven segment correspondence (ZCODE-FORK-HANDOFF PROVEN SEGMENT FACTS)
const MAX_CALL_SITES: usize = 4096;

fn hex(b: &[u8]) -> String { b.iter().map(|x| format!("{:02x}", x)).collect() }

#[derive(Clone, Copy)]
struct Load { vaddr: u64, offset: u64, filesz: u64, flags: u32 }

fn loads(bytes: &[u8]) -> Vec<Load> {
    let mut v = Vec::new();
    if bytes.len() < 64 || &bytes[0..4] != b"\x7fELF" { return v; }
    let phoff = u64::from_le_bytes(bytes[0x20..0x28].try_into().unwrap());
    let phentsize = u16::from_le_bytes(bytes[0x36..0x38].try_into().unwrap()) as u64;
    let phnum = u16::from_le_bytes(bytes[0x38..0x3a].try_into().unwrap()) as u64;
    if phnum > 256 { return v; }
    for i in 0..phnum {
        let s = phoff as usize + i as usize * phentsize as usize;
        let Some(p) = bytes.get(s..s + phentsize as usize) else { continue };
        if p.len() < 56 { continue }
        if u32::from_le_bytes(p[0..4].try_into().unwrap()) != 1 { continue } // PT_LOAD only
        v.push(Load {
            offset: u64::from_le_bytes(p[8..16].try_into().unwrap()),
            vaddr: u64::from_le_bytes(p[16..24].try_into().unwrap()),
            filesz: u64::from_le_bytes(p[32..40].try_into().unwrap()),
            flags: u32::from_le_bytes(p[4..8].try_into().unwrap()),
        });
    }
    v
}

fn va2file(ls: &[Load], bytes: &[u8], va: u64) -> Option<(u64, usize)> {
    for l in ls {
        if va >= l.vaddr && va < l.vaddr.checked_add(l.filesz)? {
            let off = l.offset.checked_add(va - l.vaddr)?;
            if off > bytes.len() as u64 { return None }
            let avail = (l.filesz - (va - l.vaddr)) as usize;
            return Some((off, avail.min(bytes.len() - off as usize)));
        }
    }
    None
}

fn slice_at<'a>(ls: &[Load], bytes: &'a [u8], va: u64, len: usize) -> Option<&'a [u8]> {
    let (off, avail) = va2file(ls, bytes, va)?;
    if avail < len { return None }
    Some(&bytes[off as usize..off as usize + len])
}

fn file2va(ls: &[Load], off: u64, plen: u64) -> Option<u64> {
    for l in ls {
        if off >= l.offset && off + plen <= l.offset.checked_add(l.filesz)? {
            return Some(l.vaddr + (off - l.offset));
        }
    }
    None
}

fn utf16le_null_pad(name: &str) -> Vec<u8> { // = util::utf16_pattern("\0NAME\0") semantics
    let mut v = vec![0u8, 0];
    for c in name.encode_utf16() { v.extend_from_slice(&c.to_le_bytes()); }
    v.extend_from_slice(&[0u8, 0]);
    v
}

fn find_all(hay: &[u8], pat: &[u8]) -> Vec<usize> {
    let mut out = Vec::new();
    if pat.is_empty() || hay.len() < pat.len() { return out }
    let (p0, p1) = (pat[0], pat[1]);
    let mut i = 0usize;
    let last = hay.len() - pat.len();
    while i <= last {
        if hay[i] == p0 && hay[i + 1] == p1 && &hay[i..i + pat.len()] == pat { out.push(i); }
        i += 1;
    }
    out
}

// Mirror of fname_call.rs:10-12 writes()
fn writes(info: &mut InstructionInfoFactory, i: &iced_x86::Instruction, r: Register) -> bool {
    info.info(i).used_registers().iter().any(|v| v.register().full_register() == r
        && matches!(v.access(), OpAccess::Write | OpAccess::CondWrite | OpAccess::ReadWrite | OpAccess::ReadCondWrite))
}

struct Win { start: u64, anchor: &'static str, string_va: u64, instrs: Vec<String> }

// Re-applies the resolver's call_after chain (fname_call.rs:27-49) starting at
// each candidate window start w in [call-63, call]; a start validates only if
// the decode reaches exactly the materialized CALL under the resolver's rules.
fn windows_at(ls: &[Load], bytes: &[u8], call: u64, strings: &[(u64, &'static str)]) -> Vec<Win> {
    let mut out = Vec::new();
    let lo = call.saturating_sub(63);
    for w in (lo..=call).rev() {
        let Some((off, avail)) = va2file(ls, bytes, w) else { continue };
        let need = (call + 5 - w) as usize;
        if avail < need { continue }
        let sl = &bytes[off as usize..off as usize + need];
        let mut d = Decoder::with_ip(64, sl, w, DecoderOptions::NONE);
        let first = d.decode();
        if first.is_invalid() || first.mnemonic() != Mnemonic::Lea || first.op0_register() != Register::RSI || !first.is_ip_rel_memory_operand() { continue }
        let offs = d.get_constant_offsets(&first);
        if offs.displacement_size() != 4 { continue }
        let Some(raw) = sl.get(offs.displacement_offset()..offs.displacement_offset() + 4) else { continue };
        let Some(sva) = first.next_ip().checked_add_signed(i32::from_le_bytes(raw.try_into().unwrap()) as i64) else { continue };
        let Some((_, anchor)) = strings.iter().find(|(a, _)| *a == sva) else { continue };
        let mut stack_arg = false; let mut enum_arg = false;
        let mut info = InstructionInfoFactory::new();
        let mut instrs = Vec::new();
        let mut ok = false;
        while d.can_decode() {
            let i = d.decode();
            if i.is_invalid() { break }
            if i.flow_control() == FlowControl::Call {
                if i.op0_kind() == OpKind::NearBranch64 && stack_arg && enum_arg && i.next_ip() == call + 5 { ok = true; }
                break;
            }
            if i.flow_control() != FlowControl::Next { break }
            if writes(&mut info, &i, Register::RSI) { break }
            if writes(&mut info, &i, Register::RDI) { stack_arg = i.mnemonic() == Mnemonic::Lea && i.op0_register() == Register::RDI && matches!(i.memory_base(), Register::RSP | Register::RBP) && i.memory_index() == Register::None; }
            if writes(&mut info, &i, Register::RDX) { enum_arg = i.mnemonic() == Mnemonic::Mov && i.op0_register() == Register::EDX && i.op1_kind() == OpKind::Immediate32 && i.immediate32() == 1; }
            instrs.push(format!("0x{:x}: {:?}", i.ip(), i.mnemonic()));
        }
        if ok { out.push(Win { start: w, anchor, string_va: sva, instrs }); }
    }
    out
}

fn main() {
    let mut out = std::io::stdout().lock();
    let mut w = |s: &str| { let _ = out.write_all(s.as_bytes()); let _ = out.flush(); };
    let mark = |s: &str| { let _ = std::io::stderr().write_all(format!("fc1-progress: {s}\n").as_bytes()); };
    let Some(path) = std::env::args().nth(1) else { eprintln!("usage: fc1_calls_by_anchor <frozen-capture>"); std::process::exit(64) };
    // READ-ONLY mmap of the frozen capture: MAP_PRIVATE PROT_READ file-backed
    // clean pages (evictable without swap). Never written; the mapping is
    // intentionally alive for the whole process (leaked by design; process
    // exit tears it down). This keeps dirty anonymous memory at KBs instead
    // of a 194MB fs::read copy -- the prior fs::read variant wedged the
    // process in-kernel on allocation while host swap was ~95% full.
    let file = match std::fs::File::open(&path) { Ok(f) => f, Err(e) => { eprintln!("open failed: {path}: {e}"); std::process::exit(65) } };
    let len = file.metadata().map(|m| m.len() as usize).unwrap_or(0);
    if len == 0 { eprintln!("empty capture"); std::process::exit(65) }
    let ptr = unsafe { libc::mmap(std::ptr::null_mut(), len, libc::PROT_READ, libc::MAP_PRIVATE, file.as_raw_fd(), 0) };
    if ptr == libc::MAP_FAILED { eprintln!("mmap failed"); std::process::exit(65) }
    let bytes: &[u8] = unsafe { core::slice::from_raw_parts(ptr as *const u8, len) };
    std::mem::forget(file); // keep fd alive for the mapping's lifetime
    mark(&format!("capture mapped read-only ({} bytes)", bytes.len()));
    mark("deferring annotation until after resolve (survey-identical call order)");

    // ---- UNMODIFIED survey resolver ----
    mark("parsing owned image");
    let image = match OwnedImage::parse(&bytes) { Ok(i) => i, Err(()) => { w("{\"error\":\"OwnedImage::parse failed (survey FFI status 2 Malformed equivalent)\"}"); return } };
    const _: () = assert!(core::mem::size_of::<AsyncContext<'static>>() == core::mem::size_of::<&OwnedImage<'static>>());
    let ctx: &AsyncContext = unsafe { core::mem::transmute::<&OwnedImage, &AsyncContext>(&image) };
    mark("calling fname_call::resolve (unmodified survey resolver)");
    let resolved = fname_call::resolve(ctx, &bytes);
    mark("resolve returned");
    let (address, cba) = match resolved {
        Ok(c) => (c.address, c.calls_by_anchor),
        Err(e) => {
            let st = match e { fname_call::Error::Malformed => 2, fname_call::Error::NoCandidate => 3, fname_call::Error::Ambiguous => 4, fname_call::Error::Incomplete => 5 };
            w(&format!("{{\"error\":\"resolver did not emit calls_by_anchor\",\"fname_call_error\":\"{e:?}\",\"ffi_status_equivalent\":{st}}}"));
            return;
        }
    };
    // ---- byte materialization + window annotation (own logic; AFTER resolver) ----
    let ls = loads(&bytes);
    mark("pt_load map built");

    // Anchor string candidates: whole-file UTF-16LE search, mapped to PT_LOAD vaddrs.
    let mut strings: Vec<(u64, &'static str)> = Vec::new();
    let mut string_counts = Vec::new();
    for name in NAMES {
        let pat = utf16le_null_pad(name);
        let hits = find_all(&bytes, &pat);
        let mut n = 0usize;
        for o in hits {
            if let Some(va) = file2va(&ls, o as u64, pat.len() as u64) { strings.push((va + 2, name)); n += 1; } // +2 = content start, matches resolver's a.checked_add(2)
        }
        string_counts.push(n);
        mark(&format!("anchor '{name}' string candidates: {n}"));
    }

    mark("annotation inputs materialized");


    let total: usize = cba.iter().map(|v| v.len()).sum();
    if total > MAX_CALL_SITES { w("{\"error\":\"call-site count over MAX_CALL_SITES cap\"}"); return }

    // ---- byte materialization + window annotation (own logic) ----
    let target_bytes64 = slice_at(&ls, &bytes, address, 64).map(hex).unwrap_or_else(|| "OUT-OF-FILE-BACKED-RANGE".into());
    let mut target_decode = String::new();
    if let Some(tb) = slice_at(&ls, &bytes, address, 64) {
        let mut d = Decoder::with_ip(64, tb, address, DecoderOptions::NONE);
        for _ in 0..16 {
            let i = d.decode();
            if i.is_invalid() { break }
            target_decode.push_str(&format!("0x{:x}: {:?}\\n", i.ip(), i.mnemonic()));
        }
    }

    w("{");
    w(&format!("\"kind\":\"fc1_calls_by_anchor_materialization\",\"resolver\":\"ue4ss-native-tick-relocation-resolver-1/crate/src/fname_call.rs resolve() UNMODIFIED\",\"resolver_fname_call_sha256\":\"f481e6ddf86515dfc739ad0610c1b2518035df0340756086666a9b0b79e429a8\",\"capture\":\"{}\",", esc(&path)));
    w(&format!("\"anchors\":{:?},\"anchor_string_candidates\":{:?},", NAMES, string_counts));
    w(&format!("\"address\":\"0x{address:x}\",\"address_matches_frozen_survey\":{},\"runtime_address\":\"0x{:x}\",\"runtime_base_note\":\"mapped_start(LOAD0)=0x400000 per ZCODE-FORK-HANDOFF PROVEN SEGMENT FACTS\",", address == TARGET_EXPECT, address.wrapping_add(LOAD0_RUNTIME_BASE)));
    w("\"calls_by_anchor\":[");
    for (i, v) in cba.iter().enumerate() {
        if i > 0 { w(",") }
        w(&format!("[{}]", v.iter().map(|a| format!("\"0x{a:x}\"")).collect::<Vec<_>>().join(",")));
    }
    w("],");
    mark("target bytes materialized");
    w(&format!("\"target\":{{\"bytes64_hex\":\"{target_bytes64}\",\"decode\":\"{}\"}},", esc(&target_decode)));
    w("\"call_sites\":[");
    let mut first_site = true;
    let mut all_rel32_ok = true;
    for (ai, v) in cba.iter().enumerate() {
        for &call in v {
            if !first_site { w(",") }
            first_site = false;
            let cb = slice_at(&ls, &bytes, call, 5).map(|b| b.to_vec());
            let (bytes_hex, rel32_target) = match &cb {
                Some(b) if b.len() == 5 && b[0] == 0xE8 => {
                    let rel = i32::from_le_bytes(b[1..5].try_into().unwrap());
                    (hex(b), Some(call.wrapping_add(5).wrapping_add_signed(rel as i64)))
                }
                Some(b) => (hex(b), None),
                None => ("OUT-OF-FILE-BACKED-RANGE".into(), None),
            };
            if rel32_target != Some(address) { all_rel32_ok = false; }
            let wins = windows_at(&ls, &bytes, call, &strings);
            w(&format!("{{\"anchor\":\"{}\",\"call_vaddr\":\"0x{call:x}\",\"call_runtime\":\"0x{:x}\",\"call_bytes\":\"{}\",\"rel32_target\":\"{}\",\"rel32_matches_candidate\":{},\"windows\":[", NAMES[ai], call.wrapping_add(LOAD0_RUNTIME_BASE), bytes_hex,
                rel32_target.map(|t| format!("0x{t:x}")).unwrap_or_else(|| "UNDECODABLE".into()), rel32_target == Some(address)));
            for (wi, win) in wins.iter().enumerate() {
                if wi > 0 { w(",") }
                w(&format!("{{\"start_vaddr\":\"0x{:x}\",\"start_runtime\":\"0x{:x}\",\"anchor\":\"{}\",\"anchor_string_vaddr\":\"0x{:x}\",\"anchor_string_runtime\":\"0x{:x}\",\"instrs\":\"{}\"}}",
                    win.start, win.start.wrapping_add(LOAD0_RUNTIME_BASE), win.anchor, win.string_va, win.string_va.wrapping_add(LOAD0_RUNTIME_BASE), esc(&win.instrs.join("\\n"))));
            }
            w("]}");
            mark(&format!("call site {call:#x} annotated"));
        }
    }
    w("],");
    mark("all call sites annotated");
    w(&format!("\"revalidation\":{{\"all_call_bytes_E8\":{},\"all_rel32_equal_candidate\":{}}}", cb_ok(cba_flat(&cba), &ls, &bytes, address), all_rel32_ok));
    w("}\n");
}

fn cba_flat(cba: &[Vec<u64>; 5]) -> Vec<u64> { cba.iter().flat_map(|v| v.iter().copied()).collect() }
fn cb_ok(sites: Vec<u64>, ls: &[Load], bytes: &[u8], address: u64) -> bool {
    sites.iter().all(|&c| matches!(slice_at(ls, bytes, c, 5), Some(b) if b.len() == 5 && b[0] == 0xE8 && c.wrapping_add(5).wrapping_add_signed(i32::from_le_bytes(b[1..5].try_into().unwrap()) as i64) == address))
}
fn esc(s: &str) -> String { s.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n") }
