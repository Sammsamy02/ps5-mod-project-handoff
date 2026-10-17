import struct, json, os

EB = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
f = open(EB, "rb")
eh = f.read(64)
assert eh[:4] == b"\x7fELF", "not ELF"
(ei, etype, machine) = eh[4], struct.unpack_from("<H", eh, 16)[0], struct.unpack_from("<H", eh, 18)[0]
phoff = struct.unpack_from("<Q", eh, 0x20)[0]
phentsize = struct.unpack_from("<H", eh, 0x36)[0]
phnum = struct.unpack_from("<H", eh, 0x38)[0]
print(f"e_type=0x{etype:x} machine={machine} phoff=0x{phoff:x} phentsize={phentsize} phnum={phnum} size={os.path.getsize(EB)}")

loads = []
f.seek(phoff)
phdrs = f.read(phentsize * phnum)
for i in range(phnum):
    p_type, p_flags, p_off, p_va, p_pa, p_fsz, p_msz, p_align = struct.unpack_from("<IIQQQQQQ", phdrs, i * phentsize)
    if p_type == 1:  # PT_LOAD
        delta = p_off - p_va
        loads.append(dict(idx=i, flags=p_flags, off=p_off, va=p_va, filesz=p_fsz, memsz=p_msz,
                          delta=delta, delta_hex=hex(delta),
                          va_end=p_va + p_msz, file_end=p_off + p_fsz))
        print(f"LOAD idx={i} flags={p_flags:#x} off=0x{p_off:x} va=0x{p_va:x} filesz=0x{p_fsz:x} memsz=0x{p_msz:x} "
              f"delta(off-va)=0x{delta:x} va_range=[0x{p_va:x},0x{p_va+p_msz:x}) file=[0x{p_off:x},0x{p_off+p_fsz:x})")

# classify pinned addresses from the RE worker's outputs
PINNED = {
  "fname_ctor": 0x23d590, "find_or_store": 0x23d600, "pool_core": 0x242470, "pool_init": 0x23a5d0,
  "current_block_getter": 0x23d570, "reserve": 0x246090, "fmemory_malloc": 0x4b7a20,
  "bucket_find_insert": 0x23c340, "store_name": 0x23c0b0, "tostring_getter": 0x243240,
  "pool_base": 0x9365ec0, "retracted_wrong_pool_base": 0x9369ec0, "init_flag_byte": 0x9365ea0,
  "gmalloc_slot": 0x9350950, "ctor_of_allocator_obj": 0x93089b8, "allocator_global": 0x930add8,
  "size_class_table": 0x9327000, "tls_index": 0x92dc288,
  "allocator_vt_final": 0x7ec26e0, "allocator_vt_first": 0x7ec2e28, "wrapper_vt": 0x7ec4030,
  "str_None": 0x7b96c17, "str_ByteProperty": 0x7abcd83, "str_IntProperty": 0x7bfb4f8, "str_FloatProperty": 0x7c184db,
  "memset_plt": 0x7871e40, "operator_new_plt": 0x7871e70, "pthread_getspecific": 0x7872530,
  "allocator_ctor_fn": 0x4afd40, "creator": 0xbeba0, "getter": 0x40c080, "ctor_write_site": 0xb660,
  "malloc_impl": 0xb48e0, "binned_slow_path": 0x4b1460, "mutex_init": 0x78728e0,
}
def classify(va):
    for L in loads:
        if L["va"] <= va < L["va_end"]:
            return L
    return None

out = []
for name, va in sorted(PINNED.items(), key=lambda kv: kv[1]):
    L = classify(va)
    if L is None:
        out.append((name, hex(va), "NOT-IN-ANY-LOAD-mem-range", None))
        print(f"{name:28s} 0x{va:x}  NOT in any PT_LOAD mem range")
    else:
        correct_off = va + L["delta"]
        out.append((name, hex(va), f"LOAD{loads.index(L)}", hex(correct_off)))
        print(f"{name:28s} 0x{va:x}  LOAD{loads.index(L)}  delta=0x{L['delta']:x}  correct_file_off=0x{correct_off:x}"
              f"{'  (FILE-BACKED)' if va < L['va']+L['filesz'] else '  (BSS: memsz-only, no file bytes)'}")

# verify the +0x4000 trap class: which loads carry delta +0x4000
trap = [loads.index(L) for L in loads if L["delta"] == 0x4000]
print("\nLOADs with file_off == vaddr + 0x4000:", trap)
print("LOADs with other deltas:", {loads.index(L): hex(L['delta']) for L in loads if L['delta'] != 0x4000})

# verify corrected string vaddrs by reading file bytes at corrected offsets
def read_at_va(va, n):
    L = classify(va)
    if L is None or va >= L["va"] + L["filesz"]:
        return None
    off = va + L["delta"]
    f.seek(off)
    return f.read(n)

for nm, va in [("str_None", 0x7b96c17), ("str_ByteProperty", 0x7abcd83), ("str_IntProperty", 0x7bfb4f8), ("str_FloatProperty", 0x7c184db)]:
    b = read_at_va(va, 16)
    print(nm, hex(va), "bytes@corrected_off:", b)

# Also verify code bytes at pinned code addrs (LOAD0 family expected)
for nm, va, exp in [("current_block_getter", 0x23d570, "8b054e891209"),
                    ("fname_ctor", 0x23d590, "55 48 89 e5".replace(" ","")),
                    ("pool_init+0x89", 0x23a609, "bf00000200")]:
    b = read_at_va(va, len(exp)//2)
    print(nm, hex(va), "file bytes:", None if b is None else b.hex(), "expected:", exp, "MATCH" if b and b.hex()==exp else "MISMATCH" if b else "N/A")

json.dump({"loads": loads, "pinned": out}, open("/tmp/wrfp_phdr_result.json","w"), indent=1, default=str)
