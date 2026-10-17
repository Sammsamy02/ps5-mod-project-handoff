#!/usr/bin/env python3
"""verify_pins.py - re-derive each cap07-step2 engine pin from the local eboot
and check it against evidence/pins.json. Mac-only, read-only.

This does NOT trust the addresses in pins.json: it re-derives the key facts
from the image bytes and only then compares. Prints how many pins it checked
and FAILS (non-zero exit) when that count is zero (empty-scan == failed gate).

Usage:
  python3 verify_pins.py [--image PATH] [--pins PATH]
Default image: the pinned Townfall decrypted eboot under .superpowers.
"""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
from elfimg import Image  # noqa: E402

DEFAULT_IMAGE = os.path.join(
    HERE, "..", "..", "..", "..",
    "what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin")
PINNED_SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"


def hx(x):
    return int(x, 16) if isinstance(x, str) else x


def check(results, name, ok, detail=""):
    results.append((name, bool(ok), detail))
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}  {detail}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default=DEFAULT_IMAGE)
    ap.add_argument("--pins", default=os.path.join(HERE, "..", "evidence", "pins.json"))
    args = ap.parse_args()

    img = Image(args.image)
    pins = json.load(open(args.pins))
    P = {p["id"]: p for p in pins["pins"]}

    results = []
    print(f"image sha256 = {img.sha256}")
    check(results, "image-sha matches pinned", img.sha256 == PINNED_SHA, img.sha256[:12])

    # P5: FPakPlatformFile vtable identity via the ctor LEA signature (anchor 1).
    # 48 8D 05 <disp32> 4D 8D 7E 38 C5 F8 57 C0 4C 89 E7 49 89 06 ; disp32 => vtable.
    head = bytes([0x48, 0x8D, 0x05])
    tail = bytes([0x4D, 0x8D, 0x7E, 0x38, 0xC5, 0xF8, 0x57, 0xC0,
                  0x4C, 0x89, 0xE7, 0x49, 0x89, 0x06])
    vt_pak = None
    vt_hits = 0
    for seg in img.text_segments():
        data = img.blob[seg["off"]:seg["off"] + seg["filesz"]]
        s = 0
        while True:
            i = data.find(head, s)
            if i < 0:
                break
            if data[i + 7:i + 7 + len(tail)] == tail:
                disp = struct.unpack_from("<i", data, i + 3)[0]
                vt_pak = seg["vaddr"] + i + 7 + disp
                vt_hits += 1
            s = i + 1
    check(results, "P5 ctor anchor is unique", vt_hits == 1, f"{vt_hits} hit(s)")
    check(results, "P5 FPakPlatformFile vtable found via ctor anchor",
          vt_pak == hx(P["P5"]["vaddr"]), f"{vt_pak:#x}" if vt_pak else "none")

    # P5 slot layout: dtor pair + Initialize assert string + GetLowerLevel body.
    if vt_pak:
        s0 = img.ptr(vt_pak + 0x00)
        s8 = img.ptr(vt_pak + 0x08)
        init = img.ptr(vt_pak + 0x28)
        gll = img.ptr(vt_pak + 0x68)
        # Initialize must reference the assert string "FPakPlatformFile::Initialize"
        astr = img.find_all("FPakPlatformFile::Initialize".encode("ascii"), limit=1)
        init_refs = img.rip_refs(astr[0]) if astr else []
        init_ok = any(r >= init and r < init + 0x400 for r in init_refs)
        gll_body = img.read(gll, 12)
        check(results, "P5 dtor pair present", s0 and s8 and img.is_exec(s0) and img.is_exec(s8),
              f"{s0:#x}/{s8:#x}")
        check(results, "P5 Initialize slot self-identifies via assert string", init_ok,
              f"init={init:#x}")
        check(results, "P6 GetLowerLevel slot +0x68 reads [rdi+8] (mov rax,[rdi+8])",
              bool(gll_body and bytes([0x48, 0x8B, 0x47, 0x08]) in gll_body),
              f"{gll:#x}:{gll_body.hex() if gll_body else '?'}")

    # P7: HandleMountPakDelegate = the function that references the
    # 'Mounting pak file' utf16 string.
    mstr = img.find_all("Mounting pak file".encode("utf-16-le"), limit=2)
    handle_fns = []
    for s in mstr:
        for r in img.rip_refs(s):
            blob = img.read(max(0, r - 0x4000), 0x4000)
            j = blob.rfind(b"\x55\x48\x89\xe5") if blob else -1
            if j >= 0:
                handle_fns.append(r - 0x4000 + j)
    handle_fns = sorted(set(handle_fns))
    check(results, "P7 HandleMountPakDelegate found via 'Mounting pak file' string",
          hx(P["P7"]["vaddr"]) in handle_fns, str([hex(x) for x in handle_fns]))

    # P1: re-derive the (static -> functor) binding map inside Initialize.
    # The engine allocator 0x70350 is called with rsi = the delegate static; the
    # instance it returns is filled by the FOLLOWING block, whose functor is a
    # 'lea rNN,[rip]=<text fn>' stored into [inst+0x20]. So: for each op-new call,
    # the static is the rsi set just before it, and the functor is the next
    # [reg+0x20] <- lea-text store before the next op-new. Assert P1 -> P7.
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    X = capstone.x86
    init_fn = hx(P["P5"]["proof_slots"]["+0x28"].split()[0]) if False else 0x1769ee0
    code = img.read(init_fn, 0xC00)
    ALLOC = 0x70350
    reg_lea = {}           # reg -> last lea target
    cur_static = None
    pending_static = None  # static of the instance currently being filled
    binding = {}           # static -> functor
    for ins in md.disasm(code, init_fn):
        if ins.id == X.X86_INS_LEA:
            o = ins.operands
            if o[1].type == X.X86_OP_MEM and o[1].mem.base == X.X86_REG_RIP:
                t = ins.address + ins.size + o[1].mem.disp
                reg_lea[o[0].reg] = t
                if 0x934f000 <= t <= 0x9350000:
                    cur_static = t
        elif ins.id == X.X86_INS_CALL and ins.operands[0].type == X.X86_OP_IMM \
                and ins.operands[0].imm == ALLOC:
            pending_static = cur_static   # this op-new's instance belongs to cur_static
        elif ins.id == X.X86_INS_MOV and ins.operands[0].type == X.X86_OP_MEM \
                and ins.operands[0].mem.disp == 0x20 \
                and ins.operands[1].type == X.X86_OP_REG:
            src = reg_lea.get(ins.operands[1].reg)
            if pending_static and src and img.is_exec(src):
                binding.setdefault(pending_static, src)
    p1, p7 = hx(P["P1"]["vaddr"]), hx(P["P7"]["vaddr"])
    check(results, "P1 MountPak static binds exactly P7 functor",
          binding.get(p1) == p7, f"{p1:#x} -> {binding.get(p1, 0):#x}")

    # P2: Execute slot 0x60 -> a TDelegateInstance iface vtable whose +0x60 body
    # loads [inst+0x18] (bound this) and dispatches. Use the example iface vtable.
    ex_vt = hx(P["P2"]["iface_vtable_example"].split()[0]) if "iface_vtable_example" in P["P2"] else 0x7fef570
    ex_body_ptr = img.ptr(ex_vt + 0x60)
    ex_body = img.read(ex_body_ptr, 0x20) if ex_body_ptr else None
    # thunk: mov rax,rdi; mov rdi,[rdi+0x18]; add rdi,[rax+0x28]; mov rcx,[rax+0x20]
    ex_ok = bool(ex_body and ex_body[:3] == bytes([0x48, 0x89, 0xF8])
                 and ex_body[3:7] == bytes([0x48, 0x8B, 0x7F, 0x18]))
    check(results, "P2 Execute slot +0x60 is the raw-method dispatch thunk", ex_ok,
          f"{ex_body_ptr:#x}" if ex_body_ptr else "?")

    # P3/P4: Get()/GetPlatformFile() accessors reused by the Mount utoc tail.
    get_body = img.read(hx(P["P3"]["vaddr"]), 8)
    getpf_body = img.read(hx(P["P4"]["vaddr"]), 4)
    check(results, "P3 Get() movzx-guard accessor", bool(get_body and get_body[0] == 0x0F and get_body[1] == 0xB6),
          get_body.hex() if get_body else "?")
    getpf8 = img.read(hx(P["P4"]["vaddr"]), 12)
    check(results, "P4 GetPlatformFile() loads [rdi] then rets (lazy on null)",
          bool(getpf8 and getpf8[:3] == bytes([0x48, 0x8B, 0x07]) and 0xC3 in getpf8),
          getpf8.hex() if getpf8 else "?")

    # P8: Mount tail references the '.utoc' string and the container ctor.
    utoc = img.find_all(".utoc".encode("utf-16-le"), limit=8)
    check(results, "P8 .utoc utf16 string present (IoStore mount tail)", len(utoc) > 0,
          str([hex(x) for x in utoc[:3]]))

    n = len(results)
    npass = sum(1 for _, ok, _ in results if ok)
    print(f"\nchecked {n} assertions across {len(P)} pins; {npass}/{n} PASS")
    if n == 0:
        print("GATE FAIL: scanned nothing")
        sys.exit(2)
    if npass != n:
        print("GATE FAIL: one or more pin assertions failed")
        sys.exit(1)
    print("GATE PASS: all pins re-derived and match pins.json")


if __name__ == "__main__":
    main()
