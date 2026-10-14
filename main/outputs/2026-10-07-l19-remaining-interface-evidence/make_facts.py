from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
import hashlib, json, struct

w = Path(__file__).resolve().parents[2]
o = Path(__file__).resolve().parent
p = w / '.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
b = p.read_bytes()
sha = hashlib.sha256(b).hexdigest()
assert sha == '3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
assert len(b) == 194241424
phoff = struct.unpack_from('<Q', b, 32)[0]
phsize, phnum = struct.unpack_from('<HH', b, 54)
segments = [struct.unpack_from('<IIQQQQQQ', b, phoff + i * phsize) for i in range(phnum)]
def vf(address):
    for ty, flags, off, va, pa, fs, ms, align in segments:
        if ty == 1 and va <= address < va + fs:
            return off + address - va
    raise ValueError(hex(address))

md = Cs(CS_ARCH_X86, CS_MODE_64)
groups = {
    'Native-interface-predicate.asm': [(0x324550, 0x3245fd)],
    'Native-interface-lookup.asm': [(0x3254d8, 0x32550f), (0x325859, 0x3258e4)],
    'Native-UClass-serialize.asm': [(0x323b00, 0x323b21), (0x323dd8, 0x323e0d),
                                  (0x323e84, 0x323e9f), (0x323ef2, 0x323f5f)],
    'Native-interface-array-elements.asm': [(0x332618, 0x33263b), (0x3326db, 0x33270e),
                                         (0x33278a, 0x3327a8), (0x3327c0, 0x3327e9),
                                         (0x3327f6, 0x332810), (0x332830, 0x33284b)],
    'UInterface-descriptor.asm': [(0x39fa3b, 0x39fa45), (0x39fa67, 0x39fa8f),
                                (0x39faa4, 0x39fabb)],
}
evidence = {}
group_keys = {}
for artifact, spans in groups.items():
    keys = []
    for low, high in spans:
        consumed = 0
        for ins in md.disasm(b[vf(low):vf(high)], low):
            assert ins.address + ins.size <= high
            f = vf(ins.address)
            assert b[f:f + ins.size] == bytes(ins.bytes)
            key = hex(ins.address)
            evidence[key] = {'elf_image_vaddr': key, 'file_byte_offset': hex(f),
                             'instruction_bytes': bytes(ins.bytes).hex(),
                             'instruction': f'{ins.mnemonic} {ins.op_str}',
                             'disassembly_artifact': artifact}
            keys.append(key)
            consumed += ins.size
        assert consumed == high - low, (artifact, hex(low), consumed, high-low)
    group_keys[artifact] = keys

# Direct native named Class anchor, not a host template or a field-extent guess.
for address in [0x32650a, 0x32651d, 0x326531, 0x326599]:
    f = vf(address)
    ins = next(md.disasm(b[f:f+15], address, count=1))
    evidence[hex(address)] = {'elf_image_vaddr': hex(address), 'file_byte_offset': hex(f),
                             'instruction_bytes': bytes(ins.bytes).hex(),
                             'instruction': f'{ins.mnemonic} {ins.op_str}',
                             'disassembly_artifact': 'frozen UCLASS-FACTS.json named descriptor'}
names = {}
for key, address, text in [('UClass_name', 0x76ae74e, 'Class'), ('UInterface_name', 0x76098c4, 'Interface')]:
    raw = (text + '\0').encode('utf-16le')
    f = vf(address)
    assert b[f:f+len(raw)] == raw
    names[key] = {'elf_image_vaddr': hex(address), 'file_byte_offset': hex(f),
                  'bytes': raw.hex(), 'decoded_utf16le': text + '\0'}

dyn = {}
for ty, flags, off, va, pa, fs, ms, align in segments:
    if ty == 2:
        for f in range(off, off + fs, 16):
            tag, value = struct.unpack_from('<QQ', b, f)
            if tag == 0:
                break
            dyn[tag] = value
assert dyn[9] == 24
matches = []
for f in range(vf(dyn[7]), vf(dyn[7]) + dyn[8], 24):
    target, info, addend = struct.unpack_from('<QQq', b, f)
    if target == 0x7ed7a28 + 0xc8:
        matches.append((f, target, info, addend))
assert matches == [(0x93735d8, 0x7ed7af0, 8, 0x323b00)]
f, target, info, addend = matches[0]
names['UClass_Serialize_rela'] = {'file_byte_offset': hex(f), 'bytes': b[f:f+24].hex(),
                                 'r_offset': hex(target), 'r_info': hex(info), 'r_addend': hex(addend),
                                 'interpretation': 'Named native Class vptr+0xC8 uniquely resolves to323B00, which passes self+1D8 into exact array serializer332600.'}

target_width = json.loads((o / 'TARGET-WIDTH.json').read_text())
assert target_width['exit'] == 0
sources = [Path(__file__).resolve(), o / 'target_interface_width.cpp', o / 'compile_target_width.py',
           o / 'TARGET-WIDTH.json', o / 'target-interface-width.o', o / 'target-interface-width.d',
           w / 'outputs/2026-10-07-l19-unresolved-evidence/UCLASS-FACTS.json',
           w / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/Class.hpp',
           w / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/UnrealType.hpp',
           w / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UClass.hpp',
           w / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_SrcWrapper_UClass.hpp',
           w / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealFlags.hpp',
           w / '.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/src/NativeLayoutWidths.cpp']
sources += [o / artifact for artifact in groups]
fact = {'key': 'UClass.Interfaces', 'value': '0x1d8', 'width_bytes': 16,
        'status': 'validated-static', 'confidence': 'high', 'live_validation': False,
        'evidence_keys': list(evidence), 'data_evidence_keys': list(names),
        'inference': 'Native interface predicate324550 rejects the named base Interface class and traverses each native superclass+40. It reads array data atself1D8 (qword), count atself1E0 (dword), scans16B entries and tests their class0 ancestry against the requested interface. Named native Class vptr+0xC8 relocates to323B00, which passes self1D8 to332600. That exact array serializer reads data0,count8,capacityC, reallocates16B/8-aligned elements and advances16. Native function lookup3254C0 independently repeats array1D8/count1E0/stride16, recursively searching entryclass0 and superclass40. Actual selected-target public getter and legacy binding provider both yield header width16; no adapter or guessed offset is needed.'}
d = {'format': 1, 'scope': 'LOCAL ONLY saved authenticated image + selected public sources; evidence candidate only; immutable R2 untouched',
     'image': {'path': str(p), 'bytes': len(b), 'sha256': sha},
     'address_model': 'ELF image vaddr via actual PT_LOAD; e.g.324550 maps file328550; runtime address adds module base400000',
     'facts': [fact], 'unresolved': [], 'evidence': evidence, 'data_evidence': names,
     'array_representation': {'header_size_bytes': 16, 'data_offset': 0, 'data_width_bytes': 8,
                              'count_offset': 8, 'count_width_bytes': 4, 'capacity_offset': 12, 'capacity_width_bytes': 4,
                              'element_stride_bytes': 16, 'element_class_offset': 0, 'element_class_width_bytes': 8,
                              'native_initialized_auxiliary_fields': [{'offset': 8, 'width_bytes': 4}, {'offset': 12, 'width_bytes': 1}]},
     'source_comparison': 'Selected FImplementedInterface declares Class,PointerOffset,bImplementedByK2 at0/8/C with stride16. Native field roles beyond Class are not required to publish Interfaces; typed comparison is separately compiled, not used to guess native offset.',
     'accessor_width_implications': {'actual_legacy_binding_provider': 16, 'actual_public_GetInterfaces_value_width': 16,
                                     'additional_adapter_needed': False},
     'sources': [{'path': str(x), 'bytes': x.stat().st_size, 'sha256': hashlib.sha256(x.read_bytes()).hexdigest()} for x in sources],
     'validation': {'instruction_windows_checked': len(evidence), 'all_instruction_bytes_exact': True,
                    'actual_DT_RELA_bounds_checked': True, 'unique_named_UClass_vtable_record_exact': True,
                    'exact_named_Interface_checked': True, 'target_width_compile_exit': 0,
                    'R2_source_mutations': 0, 'console_network_account_actions': 0, 'runtime_tests': 0}}
q = o / 'INTERFACE-FACTS.json'
q.write_text(json.dumps(d, indent=2) + '\n')
print(q)
print(hashlib.sha256(q.read_bytes()).hexdigest())
print(len(evidence), 'exact instruction windows; Interfaces1D8/16B; legacy/public widths16')
